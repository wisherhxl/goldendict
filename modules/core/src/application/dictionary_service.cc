// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/dictionary_service.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

#include "../article/article_assembler.h"
#include "../dictionary/dictionary_backend.h"
#include "../formats/aard/aard_dictionary.h"
#include "../formats/aard/aard_discovery.h"
#include "../formats/bgl/bgl_dictionary.h"
#include "../formats/bgl/bgl_discovery.h"
#include "../formats/dictd/dictd_dictionary.h"
#include "../formats/dictd/dictd_discovery.h"
#include "../formats/dsl/dsl_dictionary.h"
#include "../formats/dsl/dsl_discovery.h"
#include "../formats/epwing/epwing_dictionary.h"
#include "../formats/epwing/epwing_discovery.h"
#include "../formats/gls/gls_dictionary.h"
#include "../formats/gls/gls_discovery.h"
#include "../formats/lsa/lsa_dictionary.h"
#include "../formats/lsa/lsa_discovery.h"
#include "../formats/mdict/mdict_dictionary.h"
#include "../formats/mdict/mdict_discovery.h"
#include "../formats/sdict/sdict_dictionary.h"
#include "../formats/sdict/sdict_discovery.h"
#include "../formats/slob/slob_dictionary.h"
#include "../formats/slob/slob_discovery.h"
#include "../formats/sounddir/sounddir_dictionary.h"
#include "../formats/stardict/stardict_dictionary.h"
#include "../formats/stardict/stardict_discovery.h"
#include "../formats/xdxf/xdxf_dictionary.h"
#include "../formats/xdxf/xdxf_discovery.h"
#include "../formats/zim/zim_dictionary.h"
#include "../formats/zim/zim_discovery.h"
#include "../formats/zipsounds/zipsounds_dictionary.h"
#include "../formats/zipsounds/zipsounds_discovery.h"
#include "../foundation/text_folding.h"
#include "../foundation/utf8.h"
#include "goldendict/core/application.h"

namespace goldendict::core {
namespace {

class CancellationAdapter final : public dictionary::CancellationSignal {
   public:
    explicit CancellationAdapter(const CancellationToken* token)
        : token_(token) {}

    bool IsCancellationRequested() const noexcept override {
        return token_ != nullptr && token_->IsCancellationRequested();
    }

   private:
    const CancellationToken* token_;
};

std::string StableId(std::string_view format,
                     const std::filesystem::path& path) {
    std::error_code filesystem_error;
    const auto canonical =
        std::filesystem::weakly_canonical(path, filesystem_error);
    const std::string source =
        (filesystem_error ? path.lexically_normal() : canonical)
            .generic_string();
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char character : source) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << format << '-' << std::hex << std::setfill('0') << std::setw(16)
           << hash;
    return output.str();
}

DictionaryIdentity PublicIdentity(const dictionary::Identity& identity) {
    DictionaryIdentity result;
    result.id = identity.id;
    result.name = identity.name;
    result.source = identity.source;
    result.description = identity.description;
    result.article_count = identity.article_count;
    result.headword_count = identity.headword_count;
    return result;
}

LookupErrorCode TranslateErrorCode(dictionary::ErrorCode code) {
    switch (code) {
        case dictionary::ErrorCode::kUnavailable:
            return LookupErrorCode::kDictionaryUnavailable;
        case dictionary::ErrorCode::kCancelled:
            return LookupErrorCode::kCancelled;
        case dictionary::ErrorCode::kDeadlineExceeded:
            return LookupErrorCode::kDeadlineExceeded;
        case dictionary::ErrorCode::kInvalidData:
        case dictionary::ErrorCode::kUnsupported:
            return LookupErrorCode::kInternal;
    }
    return LookupErrorCode::kInternal;
}

dictionary::RequestOptions MakeOptions(
    const LookupQuery& query, const dictionary::CancellationSignal* signal) {
    dictionary::RequestOptions options;
    options.result_limit = std::min(query.result_limit, kMaximumLookupResults);
    options.cancellation = signal;
    if (query.timeout <= std::chrono::milliseconds::zero()) {
        options.deadline = std::chrono::steady_clock::time_point::min();
    } else {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining =
            std::chrono::steady_clock::time_point::max() - now;
        options.deadline = query.timeout >= remaining
                               ? std::chrono::steady_clock::time_point::max()
                               : now + query.timeout;
    }
    return options;
}

dictionary::RequestOptions MakeOptions(
    const SuggestionQuery& query,
    const dictionary::CancellationSignal* signal) {
    dictionary::RequestOptions options;
    options.result_limit = std::min(query.result_limit, kMaximumLookupResults);
    options.cancellation = signal;
    if (query.timeout <= std::chrono::milliseconds::zero()) {
        options.deadline = std::chrono::steady_clock::time_point::min();
    } else {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining =
            std::chrono::steady_clock::time_point::max() - now;
        options.deadline = query.timeout >= remaining
                               ? std::chrono::steady_clock::time_point::max()
                               : now + query.timeout;
    }
    return options;
}

bool HasInvalidFilter(const std::vector<std::string>& filters) {
    return std::any_of(filters.begin(), filters.end(), [](const auto& filter) {
        return filter.empty() || filter.size() > kMaximumLookupFilterBytes ||
               filter.find('\0') != std::string::npos ||
               !foundation::IsValidUtf8(filter);
    });
}

std::size_t Utf8CodePointCount(std::string_view text) noexcept {
    return static_cast<std::size_t>(
        std::count_if(text.begin(), text.end(), [](char byte) {
            return (static_cast<unsigned char>(byte) & 0xc0U) != 0x80U;
        }));
}

double PrefixScore(std::string_view folded_query,
                   std::string_view folded_headword) noexcept {
    const auto query_length = Utf8CodePointCount(folded_query);
    const auto headword_length = Utf8CodePointCount(folded_headword);
    if (query_length == 0U || headword_length == 0U) {
        return 0.0;
    }
    return static_cast<double>(query_length) /
           static_cast<double>(headword_length);
}

bool SuggestionLess(const HeadwordSuggestion& left,
                    const HeadwordSuggestion& right) noexcept {
    const bool left_exact = left.match.mode == MatchMode::kExact;
    const bool right_exact = right.match.mode == MatchMode::kExact;
    if (left_exact != right_exact) {
        return left_exact;
    }
    const auto left_length = Utf8CodePointCount(left.match.normalized_headword);
    const auto right_length =
        Utf8CodePointCount(right.match.normalized_headword);
    if (left_length != right_length) {
        return left_length < right_length;
    }
    if (left.match.normalized_headword != right.match.normalized_headword) {
        return left.match.normalized_headword < right.match.normalized_headword;
    }
    if (left.headword != right.headword) {
        return left.headword < right.headword;
    }
    return left.dictionary.id < right.dictionary.id;
}

std::optional<std::string> ValidateQuery(const LookupQuery& query) {
    if (query.text.empty() || query.result_limit == 0U) {
        return "Lookup text and result limit must be non-empty";
    }
    if (query.text.size() > kMaximumLookupTextBytes ||
        query.text.find('\0') != std::string::npos ||
        !foundation::IsValidUtf8(query.text)) {
        return "Lookup text exceeds the UTF-8 input bounds";
    }
    if (query.dictionary_ids.size() > kMaximumLookupDictionaryFilters ||
        HasInvalidFilter(query.dictionary_ids)) {
        return "Dictionary filters exceed the UTF-8 input bounds";
    }
    if (query.languages.size() > kMaximumLookupLanguageFilters ||
        HasInvalidFilter(query.languages)) {
        return "Language filters exceed the UTF-8 input bounds";
    }
    return std::nullopt;
}

std::optional<std::string> ValidateQuery(const SuggestionQuery& query) {
    if (query.text.empty() || query.result_limit == 0U) {
        return "Suggestion text and result limit must be non-empty";
    }
    if (query.text.size() > kMaximumLookupTextBytes ||
        query.text.find('\0') != std::string::npos ||
        !foundation::IsValidUtf8(query.text)) {
        return "Suggestion text exceeds the UTF-8 input bounds";
    }
    if (query.dictionary_ids.size() > kMaximumLookupDictionaryFilters ||
        HasInvalidFilter(query.dictionary_ids)) {
        return "Dictionary filters exceed the UTF-8 input bounds";
    }
    if (query.languages.size() > kMaximumLookupLanguageFilters ||
        HasInvalidFilter(query.languages)) {
        return "Language filters exceed the UTF-8 input bounds";
    }
    return std::nullopt;
}

class ServiceState final {
   public:
    explicit ServiceState(const CoreConfiguration& configuration) {
        std::vector<std::filesystem::path> roots;
        roots.reserve(configuration.dictionary_paths.size());
        for (const auto& root : configuration.dictionary_paths) {
            roots.push_back(std::filesystem::u8path(root));
        }
        for (const auto& sound_directory : configuration.sound_directories) {
            const auto path = std::filesystem::u8path(sound_directory.path);
            const std::string id = StableId("sounddir", path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::sounddir::Dictionary>(
                        formats::sounddir::Dictionary::Open(
                            id, path, sound_directory.name)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto discovery = formats::stardict::Discover(roots);
        for (const auto& issue : discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& info_path : discovery.info_files) {
            const std::string id = StableId("stardict", info_path);
            std::optional<std::filesystem::path> index_path;
            if (!configuration.index_directory.empty()) {
                index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdidx");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::stardict::Dictionary>(
                        formats::stardict::Dictionary::Open(id, info_path,
                                                            index_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto dictd_discovery = formats::dictd::Discover(roots);
        for (const auto& issue : dictd_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& index_path : dictd_discovery.index_files) {
            const std::string id = StableId("dictd", index_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::dictd::Dictionary>(
                        formats::dictd::Dictionary::Open(id, index_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto sdict_discovery = formats::sdict::Discover(roots);
        for (const auto& issue : sdict_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path : sdict_discovery.dictionary_files) {
            const std::string id = StableId("sdict", dictionary_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::sdict::Dictionary>(
                        formats::sdict::Dictionary::Open(id, dictionary_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto xdxf_discovery = formats::xdxf::Discover(roots);
        for (const auto& issue : xdxf_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path : xdxf_discovery.dictionary_files) {
            const std::string id = StableId("xdxf", dictionary_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::xdxf::Dictionary>(
                        formats::xdxf::Dictionary::Open(id, dictionary_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto gls_discovery = formats::gls::Discover(roots);
        for (const auto& issue : gls_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path : gls_discovery.dictionary_files) {
            const std::string id = StableId("gls", dictionary_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::gls::Dictionary>(
                        formats::gls::Dictionary::Open(id, dictionary_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto dsl_discovery = formats::dsl::Discover(roots);
        for (const auto& issue : dsl_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path : dsl_discovery.dictionary_files) {
            const std::string id = StableId("dsl", dictionary_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::dsl::Dictionary>(
                        formats::dsl::Dictionary::Open(id, dictionary_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto epwing_discovery = formats::epwing::Discover(roots);
        for (const auto& issue : epwing_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& catalog_path : epwing_discovery.catalog_files) {
            const std::string id = StableId("epwing", catalog_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::epwing::Dictionary>(
                        formats::epwing::Dictionary::Open(id, catalog_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto lsa_discovery = formats::lsa::Discover(roots);
        for (const auto& issue : lsa_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path : lsa_discovery.dictionary_files) {
            const std::string id = StableId("lsa", dictionary_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::lsa::Dictionary>(
                        formats::lsa::Dictionary::Open(id, dictionary_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto zipsounds_discovery = formats::zipsounds::Discover(roots);
        for (const auto& issue : zipsounds_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path :
             zipsounds_discovery.dictionary_files) {
            const std::string id = StableId("zipsounds", dictionary_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::zipsounds::Dictionary>(
                        formats::zipsounds::Dictionary::Open(id,
                                                             dictionary_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto aard_discovery = formats::aard::Discover(roots);
        for (const auto& issue : aard_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path : aard_discovery.dictionary_files) {
            const std::string id = StableId("aard", dictionary_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::aard::Dictionary>(
                        formats::aard::Dictionary::Open(id, dictionary_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto zim_discovery = formats::zim::Discover(roots);
        for (const auto& issue : zim_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        const auto slob_discovery = formats::slob::Discover(roots);
        for (const auto& issue : slob_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path : slob_discovery.dictionary_files) {
            const std::string id = StableId("slob", dictionary_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::slob::Dictionary>(
                        formats::slob::Dictionary::Open(id, dictionary_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        for (const auto& files : zim_discovery.dictionaries) {
            const std::string id = StableId("zim", files.primary);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::zim::Dictionary>(
                        formats::zim::Dictionary::Open(id, files)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto bgl_discovery = formats::bgl::Discover(roots);
        for (const auto& issue : bgl_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path : bgl_discovery.dictionary_files) {
            const std::string id = StableId("bgl", dictionary_path);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::bgl::Dictionary>(
                        formats::bgl::Dictionary::Open(id, dictionary_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        const auto mdict_discovery = formats::mdict::Discover(roots);
        for (const auto& issue : mdict_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& files : mdict_discovery.dictionaries) {
            const std::string id = StableId("mdict", files.mdx);
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::mdict::Dictionary>(
                        formats::mdict::Dictionary::Open(id, files)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        std::sort(dictionaries_.begin(), dictionaries_.end(),
                  [](const auto& left, const auto& right) {
                      return left->identity().id < right->identity().id;
                  });
    }

    std::vector<DictionaryIdentity> GetCatalog() const {
        std::vector<DictionaryIdentity> catalog;
        catalog.reserve(dictionaries_.size());
        for (const auto& dictionary : dictionaries_) {
            catalog.push_back(PublicIdentity(dictionary->identity()));
        }
        return catalog;
    }

    LookupResponse Lookup(const LookupQuery& query,
                          const CancellationToken* cancellation) const {
        LookupResponse response;
        if (const auto validation_error = ValidateQuery(query);
            validation_error.has_value()) {
            response.errors.push_back(
                {LookupErrorCode::kInvalidQuery, {}, *validation_error});
            return response;
        }
        if (query.match_mode != MatchMode::kExact &&
            query.match_mode != MatchMode::kPrefix) {
            response.errors.push_back(
                {LookupErrorCode::kInvalidQuery,
                 {},
                 "Only exact and prefix lookup are currently supported"});
            return response;
        }
        const std::string folded_query = foundation::FoldForLookup(query.text);
        response.errors = startup_errors_;
        const std::unordered_set<std::string> requested(
            query.dictionary_ids.begin(), query.dictionary_ids.end());
        std::unordered_set<std::string> found;
        const CancellationAdapter signal(cancellation);
        const auto options = MakeOptions(query, &signal);
        for (const auto& backend : dictionaries_) {
            const auto& identity = backend->identity();
            if (!requested.empty() && requested.count(identity.id) == 0U) {
                continue;
            }
            found.insert(identity.id);
            if (!query.languages.empty() &&
                std::find(query.languages.begin(), query.languages.end(),
                          identity.source_language) == query.languages.end() &&
                std::find(query.languages.begin(), query.languages.end(),
                          identity.target_language) == query.languages.end()) {
                continue;
            }
            if (response.entries.size() >= options.result_limit) {
                break;
            }
            auto backend_options = options;
            backend_options.result_limit =
                options.result_limit - response.entries.size();
            try {
                const auto articles =
                    query.match_mode == MatchMode::kExact
                        ? backend->LookupExact(query.text, backend_options)
                        : backend->LookupPrefix(query.text, backend_options);
                for (const auto& article : articles) {
                    const auto document =
                        article::Assemble(identity, {article});
                    DictionaryEntry entry;
                    entry.dictionary = PublicIdentity(identity);
                    entry.language = {identity.source_language,
                                      identity.target_language};
                    const std::string folded_headword =
                        foundation::FoldForLookup(article.headword);
                    const bool exact = folded_headword == folded_query;
                    entry.match = {
                        query.text, folded_headword,
                        exact ? MatchMode::kExact : MatchMode::kPrefix,
                        exact ? 1.0
                              : PrefixScore(folded_query, folded_headword)};
                    entry.article.plain_text = document.plain_text;
                    entry.article.sanitized_html = document.sanitized_html;
                    for (const auto& resource : document.resources) {
                        entry.resources.push_back(
                            {resource.dictionary_id, resource.resource_id,
                             dictionary::MediaTypeForResourceId(
                                 resource.resource_id)});
                    }
                    response.entries.push_back(std::move(entry));
                }
            } catch (const dictionary::Error& error) {
                response.errors.push_back({TranslateErrorCode(error.code()),
                                           identity.id, error.what()});
            } catch (const std::exception& error) {
                response.errors.push_back(
                    {LookupErrorCode::kInternal, identity.id, error.what()});
            }
        }
        for (const auto& id : requested) {
            if (found.count(id) == 0U) {
                response.errors.push_back(
                    {LookupErrorCode::kDictionaryUnavailable, id,
                     "Requested dictionary is unavailable"});
            }
        }
        response.partial =
            !response.entries.empty() && !response.errors.empty();
        return response;
    }

    SuggestionResponse Suggest(const SuggestionQuery& query,
                               const CancellationToken* cancellation) const {
        SuggestionResponse response;
        if (const auto validation_error = ValidateQuery(query);
            validation_error.has_value()) {
            response.errors.push_back(
                {LookupErrorCode::kInvalidQuery, {}, *validation_error});
            return response;
        }
        const std::string folded_query = foundation::FoldForLookup(query.text);
        response.errors = startup_errors_;
        const std::unordered_set<std::string> requested(
            query.dictionary_ids.begin(), query.dictionary_ids.end());
        std::unordered_set<std::string> found;
        const CancellationAdapter signal(cancellation);
        const auto options = MakeOptions(query, &signal);
        for (const auto& backend : dictionaries_) {
            const auto& identity = backend->identity();
            if (!requested.empty() && requested.count(identity.id) == 0U) {
                continue;
            }
            found.insert(identity.id);
            if (!query.languages.empty() &&
                std::find(query.languages.begin(), query.languages.end(),
                          identity.source_language) == query.languages.end() &&
                std::find(query.languages.begin(), query.languages.end(),
                          identity.target_language) == query.languages.end()) {
                continue;
            }
            auto backend_options = options;
            backend_options.result_limit = options.result_limit;
            try {
                const auto headwords =
                    backend->SuggestPrefix(query.text, backend_options);
                for (const auto& headword : headwords) {
                    const std::string folded_headword =
                        foundation::FoldForLookup(headword);
                    const bool exact = folded_headword == folded_query;
                    HeadwordSuggestion suggestion;
                    suggestion.dictionary = PublicIdentity(identity);
                    suggestion.language = {identity.source_language,
                                           identity.target_language};
                    suggestion.match = {
                        query.text, folded_headword,
                        exact ? MatchMode::kExact : MatchMode::kPrefix,
                        exact ? 1.0
                              : PrefixScore(folded_query, folded_headword)};
                    suggestion.headword = headword;
                    response.suggestions.push_back(std::move(suggestion));
                }
            } catch (const dictionary::Error& error) {
                response.errors.push_back({TranslateErrorCode(error.code()),
                                           identity.id, error.what()});
            } catch (const std::exception& error) {
                response.errors.push_back(
                    {LookupErrorCode::kInternal, identity.id, error.what()});
            }
        }
        for (const auto& id : requested) {
            if (found.count(id) == 0U) {
                response.errors.push_back(
                    {LookupErrorCode::kDictionaryUnavailable, id,
                     "Requested dictionary is unavailable"});
            }
        }
        std::stable_sort(response.suggestions.begin(),
                         response.suggestions.end(), SuggestionLess);
        if (response.suggestions.size() > options.result_limit) {
            response.suggestions.resize(options.result_limit);
        }
        response.partial =
            !response.suggestions.empty() && !response.errors.empty();
        return response;
    }

    std::vector<std::byte> GetResource(
        const ResourceReference& resource,
        const CancellationToken* cancellation) const {
        const auto iterator = std::find_if(
            dictionaries_.begin(), dictionaries_.end(),
            [&resource](const auto& backend) {
                return backend->identity().id == resource.dictionary_id;
            });
        if (iterator == dictionaries_.end()) {
            return {};
        }
        const CancellationAdapter signal(cancellation);
        dictionary::RequestOptions options;
        options.cancellation = &signal;
        try {
            const auto loaded =
                (*iterator)->GetResource(resource.resource_id, options);
            return loaded.has_value() ? loaded->data : std::vector<std::byte>{};
        } catch (const dictionary::Error&) {
            return {};
        }
    }

   private:
    std::vector<std::unique_ptr<dictionary::Backend>> dictionaries_;
    std::vector<LookupError> startup_errors_;
};

class AsyncCancellationToken final : public CancellationToken {
   public:
    void Cancel() noexcept { cancelled_.store(true); }

    bool IsCancellationRequested() const noexcept override {
        return cancelled_.load();
    }

   private:
    std::atomic<bool> cancelled_{false};
};

struct AsyncRequestState {
    AsyncCancellationToken cancellation;
    std::atomic<bool> finished{false};
};

class LookupRequestImpl final : public LookupRequest {
   public:
    LookupRequestImpl(std::shared_ptr<const ServiceState> state,
                      LookupQuery query)
        : request_state_(std::make_shared<AsyncRequestState>()),
          future_(std::async(
              std::launch::async,
              [request_state = request_state_, state = std::move(state),
               query = std::move(query)]() {
                  try {
                      auto response =
                          state->Lookup(query, &request_state->cancellation);
                      request_state->finished.store(true);
                      return response;
                  } catch (...) {
                      request_state->finished.store(true);
                      throw;
                  }
              })) {}

    ~LookupRequestImpl() override {
        Cancel();
        if (future_.valid()) {
            future_.wait();
        }
    }

    void Cancel() noexcept override { request_state_->cancellation.Cancel(); }

    bool IsFinished() const noexcept override {
        return request_state_->finished.load();
    }

    LookupResponse Await() override {
        if (!future_.valid()) {
            throw std::logic_error("Lookup result was already consumed");
        }
        return future_.get();
    }

   private:
    std::shared_ptr<AsyncRequestState> request_state_;
    std::future<LookupResponse> future_;
};

class DictionaryServiceImpl final : public DictionaryService {
   public:
    explicit DictionaryServiceImpl(const CoreConfiguration& configuration)
        : state_(std::make_shared<ServiceState>(configuration)) {}

    std::vector<DictionaryIdentity> GetCatalog() const override {
        return state_->GetCatalog();
    }

    LookupResponse Lookup(
        const LookupQuery& query,
        const CancellationToken* cancellation) const override {
        return state_->Lookup(query, cancellation);
    }

    SuggestionResponse Suggest(
        const SuggestionQuery& query,
        const CancellationToken* cancellation) const override {
        return state_->Suggest(query, cancellation);
    }

    std::unique_ptr<LookupRequest> StartLookup(
        LookupQuery query) const override {
        return std::make_unique<LookupRequestImpl>(state_, std::move(query));
    }

    std::vector<std::byte> GetResource(
        const ResourceReference& resource,
        const CancellationToken* cancellation) const override {
        return state_->GetResource(resource, cancellation);
    }

   private:
    std::shared_ptr<const ServiceState> state_;
};

}  // namespace

CancellationToken::~CancellationToken() = default;

LookupRequest::~LookupRequest() = default;

DictionaryService::~DictionaryService() = default;

std::unique_ptr<DictionaryService> CreateDictionaryService(
    const CoreConfiguration& configuration) {
    return std::make_unique<DictionaryServiceImpl>(configuration);
}

}  // namespace goldendict::core
