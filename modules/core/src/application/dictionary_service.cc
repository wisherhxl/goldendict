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
#include "../formats/stardict/stardict_dictionary.h"
#include "../formats/stardict/stardict_discovery.h"
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

std::string StableId(const std::filesystem::path& path) {
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
    output << "stardict-" << std::hex << std::setfill('0') << std::setw(16)
           << hash;
    return output.str();
}

DictionaryIdentity PublicIdentity(const dictionary::Identity& identity) {
    DictionaryIdentity result;
    result.id = identity.id;
    result.name = identity.name;
    result.source = identity.source;
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

bool HasInvalidFilter(const std::vector<std::string>& filters) {
    return std::any_of(filters.begin(), filters.end(), [](const auto& filter) {
        return filter.empty() || filter.size() > kMaximumLookupFilterBytes ||
               filter.find('\0') != std::string::npos ||
               !foundation::IsValidUtf8(filter);
    });
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

class ServiceState final {
   public:
    explicit ServiceState(const CoreConfiguration& configuration) {
        std::vector<std::filesystem::path> roots;
        roots.reserve(configuration.dictionary_paths.size());
        for (const auto& root : configuration.dictionary_paths) {
            roots.push_back(std::filesystem::u8path(root));
        }
        const auto discovery = formats::stardict::Discover(roots);
        for (const auto& issue : discovery.issues) {
            startup_errors_.push_back(
                {LookupErrorCode::kDictionaryUnavailable,
                 {},
                 issue.path.string() + ": " + issue.message});
        }
        for (const auto& info_path : discovery.info_files) {
            const std::string id = StableId(info_path);
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
        if (query.match_mode != MatchMode::kExact) {
            response.errors.push_back(
                {LookupErrorCode::kInvalidQuery,
                 {},
                 "Only exact lookup is currently supported"});
            return response;
        }
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
                    backend->LookupExact(query.text, backend_options);
                for (const auto& article : articles) {
                    const auto document =
                        article::Assemble(identity, {article});
                    DictionaryEntry entry;
                    entry.dictionary = PublicIdentity(identity);
                    entry.language = {identity.source_language,
                                      identity.target_language};
                    entry.match = {query.text, article.headword,
                                   MatchMode::kExact, 1.0};
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
