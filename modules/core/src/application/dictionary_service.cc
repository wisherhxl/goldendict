// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/dictionary_service.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <future>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "../article/article_assembler.h"
#include "../dictionary/dictionary_backend.h"
#include "../dictionary/full_text_index_lifecycle.h"
#include "../dictionary/full_text_index_work_executor.h"
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
#include "exact_article_target_resolver.h"
#include "full_text_index_lifecycle_inspection.h"
#include "goldendict/core/application.h"
#include "input_phrase.h"

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

constexpr std::uint32_t kRegexMatchLimit = 10000U;
constexpr std::uint32_t kRegexDepthLimit = 100U;
constexpr std::uint32_t kRegexHeapLimitKiB = 256U;
constexpr std::size_t kMaximumDictionaryDescriptionBytes = 1024U * 1024U;
constexpr std::size_t kMaximumExactCollisionCandidates = 4096U;

std::uint64_t RotateLeft(std::uint64_t value, unsigned bits) noexcept {
    return (value << bits) | (value >> (64U - bits));
}

std::uint64_t SipHash(std::string_view input, std::uint64_t key0,
                      std::uint64_t key1) noexcept {
    std::uint64_t v0 = 0x736f6d6570736575ULL ^ key0;
    std::uint64_t v1 = 0x646f72616e646f6dULL ^ key1;
    std::uint64_t v2 = 0x6c7967656e657261ULL ^ key0;
    std::uint64_t v3 = 0x7465646279746573ULL ^ key1;
    const auto round = [&]() {
        v0 += v1;
        v1 = RotateLeft(v1, 13U);
        v1 ^= v0;
        v0 = RotateLeft(v0, 32U);
        v2 += v3;
        v3 = RotateLeft(v3, 16U);
        v3 ^= v2;
        v0 += v3;
        v3 = RotateLeft(v3, 21U);
        v3 ^= v0;
        v2 += v1;
        v1 = RotateLeft(v1, 17U);
        v1 ^= v2;
        v2 = RotateLeft(v2, 32U);
    };
    std::uint64_t block = 0U;
    std::size_t offset = 0U;
    while (input.size() - offset >= 8U) {
        std::memcpy(&block, input.data() + offset, sizeof(block));
        v3 ^= block;
        round();
        round();
        v0 ^= block;
        offset += 8U;
    }
    block = static_cast<std::uint64_t>(input.size()) << 56U;
    for (std::size_t index = 0U; offset + index < input.size(); ++index) {
        block |= static_cast<std::uint64_t>(
                     static_cast<unsigned char>(input[offset + index]))
                 << (index * 8U);
    }
    v3 ^= block;
    round();
    round();
    v0 ^= block;
    v2 ^= 0xffU;
    round();
    round();
    round();
    round();
    return v0 ^ v1 ^ v2 ^ v3;
}

constexpr std::uint64_t kCursorKey0 = 0x476f6c64656e4469ULL;
constexpr std::uint64_t kCursorKey1 = 0x637448656164776fULL;

std::string Hex64(std::uint64_t value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

std::optional<std::uint64_t> ParseHex64(std::string_view value) {
    if (value.size() != 16U)
        return std::nullopt;
    std::uint64_t result = 0U;
    const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), result, 16);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size()
               ? std::optional<std::uint64_t>(result)
               : std::nullopt;
}

std::pair<std::uint64_t, std::uint64_t> NewSnapshotId() {
    std::random_device random;
    const auto next = [&]() {
        return (static_cast<std::uint64_t>(random()) << 32U) ^ random();
    };
    return {next(), next()};
}

std::string PlainMetadata(std::string_view input) {
    std::string output;
    output.reserve(std::min(input.size(), kMaximumDictionaryDescriptionBytes));
    bool inside_tag = false;
    for (std::size_t index = 0U;
         index < input.size() &&
         output.size() < kMaximumDictionaryDescriptionBytes;
         ++index) {
        const char character = input[index];
        if (character == '<') {
            const auto closing = input.find('>', index + 1U);
            if (closing != std::string_view::npos) {
                std::string tag(input.substr(index + 1U, closing - index - 1U));
                std::transform(tag.begin(), tag.end(), tag.begin(),
                               [](unsigned char byte) {
                                   return static_cast<char>(std::tolower(byte));
                               });
                if (tag == "script" || tag == "style") {
                    const std::string end_tag = "</" + tag;
                    const auto end = std::search(
                        input.begin() +
                            static_cast<std::ptrdiff_t>(closing + 1U),
                        input.end(), end_tag.begin(), end_tag.end(),
                        [](unsigned char left, unsigned char right) {
                            return std::tolower(left) == std::tolower(right);
                        });
                    if (end != input.end()) {
                        index = static_cast<std::size_t>(end - input.begin()) +
                                end_tag.size() - 1U;
                        inside_tag = true;
                        continue;
                    }
                }
                if ((tag == "br" || tag == "br/" || tag == "/p" ||
                     tag == "/div" || tag == "/li") &&
                    !output.empty() && output.back() != '\n')
                    output.push_back('\n');
            }
            inside_tag = true;
            continue;
        }
        if (inside_tag) {
            if (character == '>')
                inside_tag = false;
            continue;
        }
        if (character == '&') {
            const auto semicolon = input.find(';', index + 1U);
            if (semicolon != std::string_view::npos &&
                semicolon - index <= 6U) {
                const auto entity = input.substr(index, semicolon - index + 1U);
                if (entity == "&amp;")
                    output.push_back('&');
                else if (entity == "&lt;")
                    output.push_back('<');
                else if (entity == "&gt;")
                    output.push_back('>');
                else if (entity == "&quot;")
                    output.push_back('"');
                else if (entity == "&#39;")
                    output.push_back('\'');
                else
                    output.append(entity);
                index = semicolon;
                continue;
            }
        }
        const auto byte = static_cast<unsigned char>(character);
        if (character == '\r') {
            if (index + 1U < input.size() && input[index + 1U] == '\n')
                continue;
            output.push_back('\n');
        } else if (character == '\n' || character == '\t' || byte >= 0x20U) {
            output.push_back(character);
        }
    }
    while (!output.empty() && (output.back() == ' ' || output.back() == '\n' ||
                               output.back() == '\t'))
        output.pop_back();
    if (output.size() > kMaximumDictionaryDescriptionBytes)
        output.resize(kMaximumDictionaryDescriptionBytes);
    return output;
}

std::string RedactedProvenance(std::string source) {
    const auto scheme = source.find("://");
    if (scheme == std::string::npos)
        return source;
    const auto authority = scheme + 3U;
    const auto path = source.find('/', authority);
    const auto at = source.find('@', authority);
    if (at != std::string::npos && (path == std::string::npos || at < path))
        source.erase(authority, at - authority + 1U);
    const auto private_part = source.find_first_of("?#", authority);
    if (private_part != std::string::npos)
        source.erase(private_part);
    return source;
}

std::optional<std::string> WildcardPrefix(std::string_view pattern) {
    std::string prefix;
    bool escaped = false;
    for (const char character : pattern) {
        if (escaped) {
            if (character == '*' || character == '?' || character == '[' ||
                character == ']' || character == '\\') {
                prefix.push_back(character);
            } else {
                prefix.push_back('\\');
                prefix.push_back(character);
            }
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '*' || character == '?' || character == '[') {
            break;
        }
        prefix.push_back(character);
    }
    if (escaped) {
        prefix.push_back('\\');
    }
    return prefix.empty() ? std::nullopt
                          : std::optional<std::string>(std::move(prefix));
}

std::optional<std::string> RegexPrefix(std::string_view pattern) {
    std::string prefix;
    std::size_t index = !pattern.empty() && pattern.front() == '^' ? 1U : 0U;
    while (index < pattern.size()) {
        const char character = pattern[index++];
        if (character == '\\') {
            if (index >= pattern.size()) {
                break;
            }
            const char escaped = pattern[index++];
            if (escaped == '.' || escaped == '^' || escaped == '$' ||
                escaped == '*' || escaped == '+' || escaped == '?' ||
                escaped == '(' || escaped == ')' || escaped == '[' ||
                escaped == ']' || escaped == '{' || escaped == '}' ||
                escaped == '|' || escaped == '\\') {
                prefix.push_back(escaped);
                continue;
            }
            break;
        }
        if (character == '.' || character == '^' || character == '$' ||
            character == '*' || character == '+' || character == '?' ||
            character == '(' || character == '[' || character == '{' ||
            character == '|') {
            break;
        }
        prefix.push_back(character);
    }
    return prefix.empty() ? std::nullopt
                          : std::optional<std::string>(std::move(prefix));
}

std::string WildcardToRegex(std::string_view wildcard) {
    std::string regex;
    bool escaped = false;
    for (std::size_t index = 0U; index < wildcard.size(); ++index) {
        const char character = wildcard[index];
        if (escaped) {
            if (character == '*' || character == '?' || character == '[' ||
                character == ']' || character == '\\') {
                regex.push_back('\\');
                regex.push_back(character);
            } else {
                regex.append("\\\\");
                regex.push_back(character);
            }
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
        } else if (character == '*') {
            regex.append(".*");
        } else if (character == '?') {
            regex.push_back('.');
        } else if (character == '[') {
            const auto closing = wildcard.find(']', index + 1U);
            if (closing == std::string_view::npos) {
                regex.append("\\[");
            } else {
                regex.push_back('[');
                ++index;
                if (index < closing && wildcard[index] == '!') {
                    regex.push_back('^');
                    ++index;
                }
                for (; index < closing; ++index) {
                    regex.push_back(wildcard[index]);
                }
                regex.push_back(']');
            }
        } else {
            if (character == '.' || character == '^' || character == '$' ||
                character == '+' || character == '(' || character == ')' ||
                character == '{' || character == '}' || character == '|') {
                regex.push_back('\\');
            }
            regex.push_back(character);
        }
    }
    if (escaped) {
        regex.append("\\\\");
    }
    return regex;
}

class CompiledHeadwordPattern final {
   public:
    CompiledHeadwordPattern(std::string pattern, bool match_case) {
        int error_code = 0;
        PCRE2_SIZE error_offset = 0U;
        const std::uint32_t options =
            PCRE2_UTF | PCRE2_UCP | (match_case ? 0U : PCRE2_CASELESS);
        code_.reset(pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()),
                                  pattern.size(), options, &error_code,
                                  &error_offset, nullptr));
        if (code_ == nullptr) {
            PCRE2_UCHAR message[128] = {};
            pcre2_get_error_message(error_code, message, sizeof(message));
            error_ = "Invalid pattern at byte " +
                     std::to_string(static_cast<std::size_t>(error_offset)) +
                     ": " + reinterpret_cast<const char*>(message);
            return;
        }
        context_.reset(pcre2_match_context_create(nullptr));
        if (context_ == nullptr ||
            pcre2_set_match_limit(context_.get(), kRegexMatchLimit) != 0 ||
            pcre2_set_depth_limit(context_.get(), kRegexDepthLimit) != 0 ||
            pcre2_set_heap_limit(context_.get(), kRegexHeapLimitKiB) != 0) {
            error_ = "Could not initialize bounded regular expression matching";
        }
    }

    const std::string& error() const noexcept { return error_; }

    bool Matches(std::string_view subject, std::string* error) const {
        if (subject.size() > kMaximumLookupTextBytes) {
            *error = "Headword exceeds the regular-expression subject bound";
            return false;
        }
        using MatchData =
            std::unique_ptr<pcre2_match_data, decltype(&pcre2_match_data_free)>;
        MatchData data(
            pcre2_match_data_create_from_pattern(code_.get(), nullptr),
            &pcre2_match_data_free);
        if (data == nullptr) {
            *error = "Could not allocate bounded regular expression state";
            return false;
        }
        const int result = pcre2_match(
            code_.get(), reinterpret_cast<PCRE2_SPTR>(subject.data()),
            subject.size(), 0U, 0U, data.get(), context_.get());
        if (result >= 0) {
            return true;
        }
        if (result == PCRE2_ERROR_NOMATCH) {
            return false;
        }
        *error = "Regular expression exceeded its resource limits";
        return false;
    }

   private:
    using Code = std::unique_ptr<pcre2_code, decltype(&pcre2_code_free)>;
    using Context = std::unique_ptr<pcre2_match_context,
                                    decltype(&pcre2_match_context_free)>;
    Code code_{nullptr, &pcre2_code_free};
    Context context_{nullptr, &pcre2_match_context_free};
    std::string error_;
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

DictionaryIdentity PublicIdentity(const dictionary::Identity& identity,
                                  bool supports_full_text_search) {
    DictionaryIdentity result;
    result.id = identity.id;
    result.name = identity.name;
    result.source = RedactedProvenance(identity.source);
    result.description = PlainMetadata(identity.description);
    result.source_language = identity.source_language;
    result.target_language = identity.target_language;
    result.article_count = identity.article_count;
    result.headword_count = identity.headword_count;
    result.supports_headword_enumeration =
        identity.supports_headword_enumeration;
    result.supports_full_text_search = supports_full_text_search;
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
            return LookupErrorCode::kInternal;
        case dictionary::ErrorCode::kUnsupported:
            return LookupErrorCode::kUnsupported;
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
    return false;
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
    if (query.filter_mode != HeadwordFilterMode::kPrefix &&
        query.text.size() > kMaximumHeadwordPatternBytes) {
        return "Headword pattern exceeds the 256-byte input bound";
    }
    if (query.filter_mode != HeadwordFilterMode::kPrefix &&
        query.filter_mode != HeadwordFilterMode::kWildcard &&
        query.filter_mode != HeadwordFilterMode::kRegularExpression) {
        return "Headword filter mode is invalid";
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

std::optional<std::string> ValidateQuery(const FullTextQuery& query) {
    if (query.text.empty() || query.result_limit == 0U ||
        query.result_limit > kMaximumFullTextResults ||
        query.timeout <= std::chrono::milliseconds::zero()) {
        return "Full-text text, result limit, and timeout must be valid";
    }
    if (query.maximum_articles_per_dictionary.has_value() &&
        (*query.maximum_articles_per_dictionary == 0U ||
         *query.maximum_articles_per_dictionary > 100000U)) {
        return "Full-text maximum articles per dictionary must be between 1 "
               "and 100000";
    }
    if (query.text.size() > kMaximumFullTextQueryBytes ||
        query.text.find('\0') != std::string::npos ||
        !foundation::IsValidUtf8(query.text)) {
        return "Full-text query exceeds the UTF-8 input bounds";
    }
    if (query.dictionary_ids.size() > kMaximumLookupDictionaryFilters ||
        HasInvalidFilter(query.dictionary_ids)) {
        return "Full-text dictionary filters exceed the UTF-8 input bounds";
    }
    if (query.maximum_word_distance.has_value() &&
        *query.maximum_word_distance > kMaximumFullTextWordDistance) {
        return "Full-text word distance exceeds the supported bound";
    }
    if ((query.mode == FullTextQueryMode::kWildcard ||
         query.mode == FullTextQueryMode::kRegularExpression) &&
        (query.ignore_word_order || query.maximum_word_distance.has_value())) {
        return "Word order and distance require a word-based search mode";
    }
    if (query.mode == FullTextQueryMode::kRegularExpression) {
        const CompiledHeadwordPattern pattern(query.text, query.match_case);
        if (!pattern.error().empty())
            return pattern.error();
    }
    return std::nullopt;
}

class ServiceState final {
   public:
    explicit ServiceState(
        const CoreConfiguration& configuration,
        std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources)
        : preferences_(configuration.preferences) {
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdidx");
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::stardict::Dictionary>(
                        formats::stardict::Dictionary::Open(
                            id, info_path, index_path, full_text_index_path)));
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::dictd::Dictionary>(
                        formats::dictd::Dictionary::Open(
                            id, index_path, full_text_index_path)));
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::sdict::Dictionary>(
                        formats::sdict::Dictionary::Open(
                            id, dictionary_path, full_text_index_path)));
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::xdxf::Dictionary>(
                        formats::xdxf::Dictionary::Open(id, dictionary_path,
                                                        full_text_index_path)));
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::gls::Dictionary>(
                        formats::gls::Dictionary::Open(id, dictionary_path,
                                                       full_text_index_path)));
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::dsl::Dictionary>(
                        formats::dsl::Dictionary::Open(
                            id, dictionary_path,
                            configuration.preferences.interface_language,
                            full_text_index_path)));
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::epwing::Dictionary>(
                        formats::epwing::Dictionary::Open(
                            id, catalog_path, full_text_index_path)));
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
        std::vector<formats::aard::Dictionary*> registered_aard;
        registered_aard.reserve(aard_discovery.dictionary_files.size());
        for (const auto& issue : aard_discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& dictionary_path : aard_discovery.dictionary_files) {
            const std::string id = StableId("aard", dictionary_path);
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                auto aard = std::make_unique<formats::aard::Dictionary>(
                    formats::aard::Dictionary::Open(id, dictionary_path,
                                                    full_text_index_path));
                if (!full_text_index_coordinator_.RegisterDictionary(
                        {id, "AARD", aard->identity().article_count},
                        aard->full_text_work_port(),
                        aard->full_text_snapshot_holder())) {
                    startup_errors_.push_back(
                        {LookupErrorCode::kInternal, id,
                         "Could not register AARD full-text lifecycle"});
                } else {
                    registered_aard.push_back(aard.get());
                }
                dictionaries_.push_back(std::move(aard));
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::slob::Dictionary>(
                        formats::slob::Dictionary::Open(id, dictionary_path,
                                                        full_text_index_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        for (const auto& files : zim_discovery.dictionaries) {
            const std::string id = StableId("zim", files.primary);
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::zim::Dictionary>(
                        formats::zim::Dictionary::Open(id, files,
                                                       full_text_index_path)));
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::bgl::Dictionary>(
                        formats::bgl::Dictionary::Open(id, dictionary_path,
                                                       full_text_index_path)));
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
            std::optional<std::filesystem::path> full_text_index_path;
            if (!configuration.index_directory.empty()) {
                full_text_index_path =
                    std::filesystem::u8path(configuration.index_directory) /
                    (id + ".gdfts");
            }
            try {
                dictionaries_.push_back(
                    std::make_unique<formats::mdict::Dictionary>(
                        formats::mdict::Dictionary::Open(
                            id, files, full_text_index_path)));
            } catch (const dictionary::Error& error) {
                startup_errors_.push_back(
                    {TranslateErrorCode(error.code()), id, error.what()});
            }
        }
        std::sort(dictionaries_.begin(), dictionaries_.end(),
                  [](const auto& left, const auto& right) {
                      return left->identity().id < right->identity().id;
                  });
        std::unordered_set<std::string> identities;
        for (const auto& dictionary : dictionaries_) {
            identities.insert(dictionary->identity().id);
        }
        for (auto& source : runtime_sources) {
            if (source == nullptr || source->identity().id.empty() ||
                !identities.insert(source->identity().id).second) {
                throw std::runtime_error(
                    "Runtime dictionary sources must have unique non-empty "
                    "IDs");
            }
            runtime_source_ids_.insert(source->identity().id);
            dictionaries_.push_back(std::move(source));
        }
        for (const auto& group : configuration.dictionary_groups) {
            std::vector<const dictionary::Backend*> resolved;
            resolved.reserve(group.dictionary_ids.size());
            std::unordered_set<std::string> added;
            for (const auto& id : group.dictionary_ids) {
                const auto found =
                    std::find_if(dictionaries_.begin(), dictionaries_.end(),
                                 [&id](const auto& backend) {
                                     return backend->identity().id == id;
                                 });
                if (found != dictionaries_.end() && added.insert(id).second) {
                    resolved.push_back(found->get());
                }
            }
            groups_.emplace(group.id, std::move(resolved));
        }
        if (!full_text_index_coordinator_.ApplyPolicyToRegisteredEntries(
                dictionary::ProjectFullTextIndexPolicy(preferences_))) {
            throw std::runtime_error(
                "Could not apply persisted full-text index policy");
        }
        for (const auto* aard : registered_aard) {
            const auto lifecycle =
                full_text_index_coordinator_.Snapshot(aard->identity().id);
            if (!lifecycle.has_value())
                continue;
            const auto evidence =
                aard->StartupArtifactEvidence(lifecycle->identity());
            if (evidence.has_value())
                full_text_index_coordinator_.ReconcileStartupArtifact(
                    *evidence);
        }
        full_text_index_executor_.emplace(full_text_index_coordinator_);
    }

    std::vector<DictionaryIdentity> GetCatalog() const {
        std::vector<DictionaryIdentity> catalog;
        catalog.reserve(dictionaries_.size());
        for (const auto& dictionary : dictionaries_) {
            catalog.push_back(
                PublicIdentity(dictionary->identity(),
                               dynamic_cast<const dictionary::FullTextBackend*>(
                                   dictionary.get()) != nullptr));
        }
        return catalog;
    }

    FullTextResponse SearchFullText(
        const FullTextQuery& query,
        const CancellationToken* cancellation) const {
        FullTextResponse response;
        if (const auto invalid = ValidateQuery(query); invalid.has_value()) {
            response.errors.push_back(
                {FullTextErrorCode::kInvalidQuery, {}, *invalid});
            return response;
        }
        if (cancellation != nullptr &&
            cancellation->IsCancellationRequested()) {
            response.errors.push_back({FullTextErrorCode::kCancelled,
                                       {},
                                       "Full-text search cancelled"});
            return response;
        }
        if (query.dictionary_filter_active && query.dictionary_ids.empty()) {
            return response;
        }
        std::unordered_set<std::string> requested(query.dictionary_ids.begin(),
                                                  query.dictionary_ids.end());
        std::unordered_set<std::string> found;
        const auto deadline = std::chrono::steady_clock::now() + query.timeout;
        for (const auto& backend : dictionaries_) {
            const auto& id = backend->identity().id;
            if (query.dictionary_filter_active && requested.count(id) == 0U) {
                continue;
            }
            found.insert(id);
            const auto* full_text =
                dynamic_cast<const dictionary::FullTextBackend*>(backend.get());
            if (full_text == nullptr) {
                response.errors.push_back({FullTextErrorCode::kUnsupported, id,
                                           "Full-text indexing is not "
                                           "available for this dictionary"});
                continue;
            }
            if (response.results.size() == query.result_limit)
                continue;
            const auto now = std::chrono::steady_clock::now();
            if (cancellation != nullptr &&
                cancellation->IsCancellationRequested()) {
                response.errors.push_back({FullTextErrorCode::kCancelled, id,
                                           "Full-text search cancelled"});
                break;
            }
            if (now >= deadline) {
                response.errors.push_back(
                    {FullTextErrorCode::kDeadlineExceeded, id,
                     "Full-text operation deadline exceeded"});
                break;
            }
            auto backend_query = query;
            const auto remaining = query.result_limit - response.results.size();
            const auto dictionary_limit =
                query.maximum_articles_per_dictionary.has_value()
                    ? std::min(*query.maximum_articles_per_dictionary,
                               remaining)
                    : remaining;
            backend_query.result_limit = dictionary_limit;
            backend_query.timeout =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                      now);
            if (backend_query.timeout <= std::chrono::milliseconds::zero())
                backend_query.timeout = std::chrono::milliseconds(1);
            auto backend_response =
                full_text->SearchFullText(backend_query, cancellation);
            for (auto& error : backend_response.errors) {
                if (error.dictionary_id.empty())
                    error.dictionary_id = id;
                response.errors.push_back(std::move(error));
            }
            if (backend_response.results.size() > dictionary_limit)
                backend_response.results.resize(dictionary_limit);
            for (auto& result : backend_response.results) {
                result.dictionary.supports_full_text_search = true;
                response.results.push_back(std::move(result));
            }
        }
        if (query.dictionary_filter_active)
            for (const auto& id : query.dictionary_ids) {
                if (found.count(id) == 0U) {
                    response.errors.push_back(
                        {FullTextErrorCode::kDictionaryUnavailable, id,
                         "Full-text dictionary is unavailable"});
                }
            }
        response.partial = !response.errors.empty();
        return response;
    }

    HeadwordEnumerationPage EnumerateHeadwords(
        const HeadwordEnumerationQuery& query,
        const CancellationToken* cancellation) const {
        HeadwordEnumerationPage response;
        response.dictionary_id = query.dictionary_id;
        const auto fail = [&](HeadwordEnumerationErrorCode code,
                              std::string message) {
            response.headwords.clear();
            response.next_cursor.clear();
            response.complete = false;
            response.error = HeadwordEnumerationError{code, query.dictionary_id,
                                                      std::move(message)};
            return response;
        };
        if (query.dictionary_id.empty() ||
            query.dictionary_id.size() > kMaximumLookupFilterBytes ||
            query.dictionary_id.find('\0') != std::string::npos ||
            !foundation::IsValidUtf8(query.dictionary_id) ||
            query.page_size == 0U ||
            query.page_size > kMaximumHeadwordEnumerationPageSize ||
            query.cursor.size() > kMaximumHeadwordEnumerationCursorBytes) {
            return fail(HeadwordEnumerationErrorCode::kInvalidRequest,
                        "Headword enumeration request exceeds its bounds");
        }
        const auto backend =
            std::find_if(dictionaries_.begin(), dictionaries_.end(),
                         [&query](const auto& item) {
                             return item->identity().id == query.dictionary_id;
                         });
        if (backend == dictionaries_.end()) {
            return fail(HeadwordEnumerationErrorCode::kDictionaryUnavailable,
                        "Requested dictionary is unavailable");
        }
        if (!(*backend)->identity().supports_headword_enumeration) {
            return fail(HeadwordEnumerationErrorCode::kUnsupported,
                        "Dictionary does not support headword enumeration");
        }
        std::size_t offset = 0U;
        if (!query.cursor.empty()) {
            std::array<std::string_view, 7> fields;
            std::size_t begin = 0U;
            for (std::size_t field = 0U; field < fields.size(); ++field) {
                const auto end = query.cursor.find(':', begin);
                if (field + 1U == fields.size()) {
                    if (end != std::string::npos) {
                        return fail(
                            HeadwordEnumerationErrorCode::kMalformedCursor,
                            "Headword enumeration cursor is malformed");
                    }
                    fields[field] =
                        std::string_view(query.cursor).substr(begin);
                } else {
                    if (end == std::string::npos) {
                        return fail(
                            HeadwordEnumerationErrorCode::kMalformedCursor,
                            "Headword enumeration cursor is malformed");
                    }
                    fields[field] = std::string_view(query.cursor)
                                        .substr(begin, end - begin);
                    begin = end + 1U;
                }
            }
            const auto snapshot0 = ParseHex64(fields[1]);
            const auto snapshot1 = ParseHex64(fields[2]);
            const auto dictionary_hash = ParseHex64(fields[3]);
            const auto cursor_offset = ParseHex64(fields[4]);
            const auto reserved = ParseHex64(fields[5]);
            const auto tag = ParseHex64(fields[6]);
            const auto payload_end = query.cursor.rfind(':');
            if (fields[0] != "gdhe1" || !snapshot0 || !snapshot1 ||
                !dictionary_hash || !cursor_offset || !reserved || !tag ||
                *reserved != 0U || payload_end == std::string::npos ||
                SipHash(std::string_view(query.cursor).substr(0U, payload_end),
                        kCursorKey0, kCursorKey1) != *tag) {
                return fail(HeadwordEnumerationErrorCode::kMalformedCursor,
                            "Headword enumeration cursor is malformed");
            }
            if (*snapshot0 != snapshot_id_.first ||
                *snapshot1 != snapshot_id_.second ||
                *dictionary_hash !=
                    SipHash(query.dictionary_id, kCursorKey1, kCursorKey0)) {
                return fail(HeadwordEnumerationErrorCode::kStaleCursor,
                            "Headword enumeration cursor is stale");
            }
            if (*cursor_offset > std::numeric_limits<std::size_t>::max()) {
                return fail(HeadwordEnumerationErrorCode::kStaleCursor,
                            "Headword enumeration cursor is stale");
            }
            offset = static_cast<std::size_t>(*cursor_offset);
        }
        const CancellationAdapter signal(cancellation);
        dictionary::RequestOptions options;
        options.result_limit = query.page_size;
        options.cancellation = &signal;
        if (query.timeout <= std::chrono::milliseconds::zero()) {
            options.deadline = std::chrono::steady_clock::time_point::min();
        } else {
            const auto now = std::chrono::steady_clock::now();
            const auto remaining =
                std::chrono::steady_clock::time_point::max() - now;
            options.deadline =
                query.timeout >= remaining
                    ? std::chrono::steady_clock::time_point::max()
                    : now + query.timeout;
        }
        try {
            auto page = (*backend)->EnumerateHeadwords(offset, options);
            response.headwords = std::move(page.headwords);
            response.complete = page.complete;
            if (!response.complete) {
                const auto next = offset + response.headwords.size();
                std::string payload = "gdhe1:" + Hex64(snapshot_id_.first) +
                                      ":" + Hex64(snapshot_id_.second) + ":" +
                                      Hex64(SipHash(query.dictionary_id,
                                                    kCursorKey1, kCursorKey0)) +
                                      ":" + Hex64(next) + ":" + Hex64(0U);
                response.next_cursor =
                    payload + ":" +
                    Hex64(SipHash(payload, kCursorKey0, kCursorKey1));
            }
            return response;
        } catch (const std::out_of_range&) {
            return fail(HeadwordEnumerationErrorCode::kStaleCursor,
                        "Headword enumeration cursor is stale");
        } catch (const dictionary::Error& error) {
            switch (error.code()) {
                case dictionary::ErrorCode::kCancelled:
                    return fail(HeadwordEnumerationErrorCode::kCancelled,
                                error.what());
                case dictionary::ErrorCode::kDeadlineExceeded:
                    return fail(HeadwordEnumerationErrorCode::kDeadlineExceeded,
                                error.what());
                case dictionary::ErrorCode::kUnavailable:
                    return fail(
                        HeadwordEnumerationErrorCode::kDictionaryUnavailable,
                        error.what());
                case dictionary::ErrorCode::kUnsupported:
                    return fail(HeadwordEnumerationErrorCode::kUnsupported,
                                error.what());
                case dictionary::ErrorCode::kInvalidData:
                    return fail(HeadwordEnumerationErrorCode::kInternal,
                                error.what());
            }
        } catch (const std::exception& error) {
            return fail(HeadwordEnumerationErrorCode::kInternal, error.what());
        }
        return fail(HeadwordEnumerationErrorCode::kInternal,
                    "Headword enumeration failed");
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
        if (!application::IsInputPhraseAccepted(query.text, preferences_)) {
            response.errors.push_back(
                {LookupErrorCode::kInvalidQuery,
                 {},
                 "Input phrase exceeds the configured symbol limit"});
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
        if (query.dictionary_filter_active && query.dictionary_ids.empty()) {
            return response;
        }
        const std::string folded_query = foundation::FoldForLookup(query.text);
        const std::string exact_query = foundation::NormalizeForExactLookup(
            query.text, preferences_.ignore_diacritics);
        response.errors = startup_errors_;
        const std::unordered_set<std::string> requested(
            query.dictionary_ids.begin(), query.dictionary_ids.end());
        std::unordered_set<std::string> found;
        const CancellationAdapter signal(cancellation);
        const auto options = MakeOptions(query, &signal);
        const auto backends = BackendsForGroup(query.group_id);
        std::vector<std::string> lookup_terms{query.text};
        std::unordered_set<std::string> folded_lookup_terms{folded_query};
        std::unordered_set<std::string> exact_lookup_terms{exact_query};
        if (query.match_mode == MatchMode::kExact &&
            preferences_.synonym_search_enabled) {
            for (const auto* backend : backends) {
                if (lookup_terms.size() >= options.result_limit)
                    break;
                const auto& identity = backend->identity();
                if ((!requested.empty() &&
                     requested.count(identity.id) == 0U) ||
                    (!query.languages.empty() &&
                     std::find(query.languages.begin(), query.languages.end(),
                               identity.source_language) ==
                         query.languages.end() &&
                     std::find(query.languages.begin(), query.languages.end(),
                               identity.target_language) ==
                         query.languages.end())) {
                    continue;
                }
                const auto* synonym_backend =
                    dynamic_cast<const dictionary::SynonymBackend*>(backend);
                if (synonym_backend == nullptr)
                    continue;
                try {
                    for (auto headword :
                         synonym_backend->FindHeadwordsForSynonym(query.text,
                                                                  options)) {
                        dictionary::CheckRequest(options);
                        if (folded_lookup_terms
                                .insert(foundation::FoldForLookup(headword))
                                .second) {
                            exact_lookup_terms.insert(
                                foundation::NormalizeForExactLookup(
                                    headword, preferences_.ignore_diacritics));
                            lookup_terms.push_back(std::move(headword));
                            if (lookup_terms.size() >= options.result_limit)
                                break;
                        }
                    }
                } catch (const dictionary::Error& error) {
                    response.errors.push_back({TranslateErrorCode(error.code()),
                                               identity.id, error.what()});
                    if (error.code() == dictionary::ErrorCode::kCancelled ||
                        error.code() ==
                            dictionary::ErrorCode::kDeadlineExceeded) {
                        return response;
                    }
                } catch (const std::exception& error) {
                    response.errors.push_back({LookupErrorCode::kInternal,
                                               identity.id, error.what()});
                }
            }
        }
        for (const auto* backend : backends) {
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
            const bool runtime_source =
                runtime_source_ids_.count(identity.id) != 0U;
            if (query.match_mode == MatchMode::kExact &&
                preferences_.ignore_diacritics && runtime_source &&
                !identity.supports_diacritic_insensitive_lookup) {
                response.errors.push_back(
                    {LookupErrorCode::kUnsupported, identity.id,
                     "Dictionary source does not support "
                     "diacritic-insensitive exact lookup"});
                continue;
            }
            backend_options.ignore_diacritics =
                query.match_mode == MatchMode::kExact &&
                preferences_.ignore_diacritics;
            backend_options.result_limit =
                query.match_mode == MatchMode::kExact && !runtime_source
                    ? kMaximumExactCollisionCandidates
                    : options.result_limit - response.entries.size();
            try {
                std::vector<dictionary::Article> articles;
                for (const auto& lookup_term : lookup_terms) {
                    auto term_articles =
                        query.match_mode == MatchMode::kExact
                            ? backend->LookupExact(lookup_term, backend_options)
                            : backend->LookupPrefix(lookup_term,
                                                    backend_options);
                    articles.insert(
                        articles.end(),
                        std::make_move_iterator(term_articles.begin()),
                        std::make_move_iterator(term_articles.end()));
                    if (articles.size() >= backend_options.result_limit)
                        break;
                }
                std::unordered_set<std::string> seen_articles;
                for (const auto& article : articles) {
                    dictionary::CheckRequest(backend_options);
                    if (query.match_mode == MatchMode::kExact &&
                        exact_lookup_terms.count(
                            foundation::NormalizeForExactLookup(
                                article.headword,
                                preferences_.ignore_diacritics)) == 0U) {
                        continue;
                    }
                    if (response.entries.size() >= options.result_limit)
                        break;
                    const auto document =
                        article::Assemble(identity, {article});
                    const std::string article_key =
                        document.plain_text + "\n" + document.sanitized_html;
                    if (!seen_articles.insert(article_key).second)
                        continue;
                    DictionaryEntry entry;
                    entry.dictionary = PublicIdentity(
                        identity,
                        dynamic_cast<const dictionary::FullTextBackend*>(
                            backend) != nullptr);
                    entry.language = {identity.source_language,
                                      identity.target_language};
                    const std::string folded_headword =
                        foundation::FoldForLookup(article.headword);
                    const bool exact =
                        folded_lookup_terms.count(folded_headword) != 0U;
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
                if (query.match_mode == MatchMode::kExact && !runtime_source &&
                    articles.size() == kMaximumExactCollisionCandidates) {
                    response.errors.push_back(
                        {LookupErrorCode::kInternal, identity.id,
                         "Exact lookup collision scan reached its bounded "
                         "candidate limit"});
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
        if (!application::IsInputPhraseAccepted(query.text, preferences_)) {
            response.errors.push_back(
                {LookupErrorCode::kInvalidQuery,
                 {},
                 "Input phrase exceeds the configured symbol limit"});
            return response;
        }
        std::string seed = query.text;
        std::optional<CompiledHeadwordPattern> pattern;
        if (query.filter_mode != HeadwordFilterMode::kPrefix) {
            const auto extracted =
                query.filter_mode == HeadwordFilterMode::kWildcard
                    ? WildcardPrefix(query.text)
                    : RegexPrefix(query.text);
            if (!extracted.has_value()) {
                response.errors.push_back(
                    {LookupErrorCode::kInvalidQuery,
                     {},
                     "Wildcard and regular-expression filters require a "
                     "leading literal prefix"});
                return response;
            }
            seed = *extracted;
            pattern.emplace(query.filter_mode == HeadwordFilterMode::kWildcard
                                ? WildcardToRegex(query.text)
                                : query.text,
                            query.match_case);
            if (!pattern->error().empty()) {
                response.errors.push_back(
                    {LookupErrorCode::kInvalidQuery, {}, pattern->error()});
                return response;
            }
        }
        const std::string folded_query = foundation::FoldForLookup(seed);
        if (query.dictionary_filter_active && query.dictionary_ids.empty()) {
            return response;
        }
        response.errors = startup_errors_;
        const std::unordered_set<std::string> requested(
            query.dictionary_ids.begin(), query.dictionary_ids.end());
        std::unordered_set<std::string> found;
        const CancellationAdapter signal(cancellation);
        const auto options = MakeOptions(query, &signal);
        const auto backends = BackendsForGroup(query.group_id);
        for (const auto* backend : backends) {
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
                    backend->SuggestPrefix(seed, backend_options);
                for (const auto& headword : headwords) {
                    if (pattern.has_value()) {
                        dictionary::CheckRequest(backend_options);
                        std::string pattern_error;
                        const bool matches =
                            pattern->Matches(headword, &pattern_error);
                        dictionary::CheckRequest(backend_options);
                        if (!matches) {
                            if (!pattern_error.empty()) {
                                response.suggestions.clear();
                                response.errors.push_back(
                                    {LookupErrorCode::kInvalidQuery,
                                     {},
                                     std::move(pattern_error)});
                                return response;
                            }
                            continue;
                        }
                    }
                    const std::string folded_headword =
                        foundation::FoldForLookup(headword);
                    const bool exact = folded_headword == folded_query;
                    HeadwordSuggestion suggestion;
                    suggestion.dictionary = PublicIdentity(
                        identity,
                        dynamic_cast<const dictionary::FullTextBackend*>(
                            backend) != nullptr);
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

    ResolvedExactArticleTarget ResolveExactArticleTarget(
        const ExactArticleTarget& target) const {
        const auto backend =
            std::find_if(dictionaries_.begin(), dictionaries_.end(),
                         [&target](const auto& item) {
                             return item->identity().id == target.dictionary_id;
                         });
        if (backend == dictionaries_.end()) {
            return {
                ExactArticleTargetError::kDictionaryUnavailable, {}, {}, {}};
        }
        const auto* full_text =
            dynamic_cast<const dictionary::FullTextBackend*>(backend->get());
        if (full_text == nullptr || !full_text->IsFullTextIndexAvailable()) {
            return {
                ExactArticleTargetError::kDictionaryUnavailable, {}, {}, {}};
        }
        const auto resolved =
            full_text->ResolveFullTextDocument(target.document_id);
        if (!resolved.has_value()) {
            return {ExactArticleTargetError::kDocumentNotFound, {}, {}, {}};
        }
        return {ExactArticleTargetError::kNone, resolved->dictionary,
                resolved->document_id, resolved->headword};
    }

    std::optional<dictionary::FullTextIndexLifecycleSnapshot>
    FullTextIndexLifecycleSnapshot(const std::string& dictionary_id) const {
        return full_text_index_coordinator_.Snapshot(dictionary_id);
    }

   private:
    std::vector<const dictionary::Backend*> BackendsForGroup(
        std::uint32_t group_id) const {
        const auto group = groups_.find(group_id);
        if (group_id != 0U && group != groups_.end()) {
            return group->second;
        }
        std::vector<const dictionary::Backend*> all;
        all.reserve(dictionaries_.size());
        for (const auto& backend : dictionaries_) {
            all.push_back(backend.get());
        }
        return all;
    }

    ApplicationPreferences preferences_;
    dictionary::FullTextIndexLifecycleCoordinator full_text_index_coordinator_;
    std::unordered_set<std::string> runtime_source_ids_;
    std::vector<std::unique_ptr<dictionary::Backend>> dictionaries_;
    std::unordered_map<std::uint32_t, std::vector<const dictionary::Backend*>>
        groups_;
    std::vector<LookupError> startup_errors_;
    const std::pair<std::uint64_t, std::uint64_t> snapshot_id_ =
        NewSnapshotId();
    std::optional<dictionary::FullTextIndexWorkExecutor>
        full_text_index_executor_;
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
    DictionaryServiceImpl(
        const CoreConfiguration& configuration,
        std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources)
        : state_(std::make_shared<ServiceState>(configuration,
                                                std::move(runtime_sources))) {}

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

    HeadwordEnumerationPage EnumerateHeadwords(
        const HeadwordEnumerationQuery& query,
        const CancellationToken* cancellation) const override {
        return state_->EnumerateHeadwords(query, cancellation);
    }

    FullTextResponse SearchFullText(
        const FullTextQuery& query,
        const CancellationToken* cancellation) const override {
        return state_->SearchFullText(query, cancellation);
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

    ResolvedExactArticleTarget ResolveExactArticleTarget(
        const ExactArticleTarget& target) const {
        return state_->ResolveExactArticleTarget(target);
    }

    std::optional<dictionary::FullTextIndexLifecycleSnapshot>
    FullTextIndexLifecycleSnapshot(const std::string& dictionary_id) const {
        return state_->FullTextIndexLifecycleSnapshot(dictionary_id);
    }

   private:
    std::shared_ptr<const ServiceState> state_;
};

}  // namespace

namespace application {

std::optional<dictionary::FullTextIndexLifecycleSnapshot>
FullTextIndexLifecycleSnapshot(const DictionaryService& service,
                               const std::string& dictionary_id) {
    const auto* implementation =
        dynamic_cast<const DictionaryServiceImpl*>(&service);
    if (implementation == nullptr)
        return std::nullopt;
    return implementation->FullTextIndexLifecycleSnapshot(dictionary_id);
}

ResolvedExactArticleTarget ResolveExactArticleTarget(
    const DictionaryService& service, const ExactArticleTarget& target) {
    const auto* implementation =
        dynamic_cast<const DictionaryServiceImpl*>(&service);
    if (implementation == nullptr) {
        return {ExactArticleTargetError::kDictionaryUnavailable, {}, {}, {}};
    }
    return implementation->ResolveExactArticleTarget(target);
}

}  // namespace application

CancellationToken::~CancellationToken() = default;

LookupRequest::~LookupRequest() = default;

DictionaryService::~DictionaryService() = default;

FullTextResponse DictionaryService::SearchFullText(
    const FullTextQuery& query, const CancellationToken* cancellation) const {
    static_cast<void>(query);
    FullTextResponse response;
    response.errors.push_back(
        {cancellation != nullptr && cancellation->IsCancellationRequested()
             ? FullTextErrorCode::kCancelled
             : FullTextErrorCode::kUnsupported,
         {},
         "Full-text search is unsupported"});
    return response;
}

std::unique_ptr<DictionaryService> CreateDictionaryService(
    const CoreConfiguration& configuration) {
    return CreateDictionaryService(configuration, {});
}

std::unique_ptr<DictionaryService> CreateDictionaryService(
    const CoreConfiguration& configuration,
    std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources) {
    return std::make_unique<DictionaryServiceImpl>(configuration,
                                                   std::move(runtime_sources));
}

}  // namespace goldendict::core
