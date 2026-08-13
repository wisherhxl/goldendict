// SPDX-License-Identifier: GPL-3.0-or-later

#include "runtime_composition.h"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mediawiki_source.h"
#include "website_source.h"

namespace goldendict::network {
namespace {

using goldendict::core::RuntimeDictionaryArticle;
using goldendict::core::RuntimeDictionaryIdentity;
using goldendict::core::RuntimeDictionaryResource;
using goldendict::core::RuntimeDictionarySource;
using goldendict::core::RuntimeRequestOptions;
using goldendict::core::RuntimeSourceError;
using goldendict::core::RuntimeSourceErrorCode;

void CheckRequest(const RuntimeRequestOptions& options) {
    if (options.cancellation != nullptr &&
        options.cancellation->IsCancellationRequested()) {
        throw RuntimeSourceError(RuntimeSourceErrorCode::kCancelled,
                                 "Network source request was cancelled");
    }
    if (std::chrono::steady_clock::now() >= options.deadline) {
        throw RuntimeSourceError(
            RuntimeSourceErrorCode::kDeadlineExceeded,
            "Network source request deadline was exceeded");
    }
}

std::function<bool()> StopRequested(const RuntimeRequestOptions& options) {
    return [&options]() {
        return (options.cancellation != nullptr &&
                options.cancellation->IsCancellationRequested()) ||
               std::chrono::steady_clock::now() >= options.deadline;
    };
}

[[noreturn]] void TranslateHttpError(const HttpError& error,
                                     const RuntimeRequestOptions& options) {
    RuntimeSourceErrorCode code = RuntimeSourceErrorCode::kUnavailable;
    if (std::chrono::steady_clock::now() >= options.deadline) {
        throw RuntimeSourceError(
            RuntimeSourceErrorCode::kDeadlineExceeded,
            "Network source request deadline was exceeded");
    }
    switch (error.code()) {
        case HttpErrorCode::kCancelled:
            code = RuntimeSourceErrorCode::kCancelled;
            break;
        case HttpErrorCode::kDeadlineExceeded:
            code = RuntimeSourceErrorCode::kDeadlineExceeded;
            break;
        case HttpErrorCode::kInvalidRequest:
        case HttpErrorCode::kInvalidUrl:
        case HttpErrorCode::kInvalidResponse:
            code = RuntimeSourceErrorCode::kInvalidData;
            break;
        case HttpErrorCode::kResponseTooLarge:
        case HttpErrorCode::kRedirectRejected:
        case HttpErrorCode::kTooManyRedirects:
        case HttpErrorCode::kHttpStatus:
        case HttpErrorCode::kTransport:
            break;
    }
    throw RuntimeSourceError(code, error.what());
}

class MediaWikiRuntimeSource final : public RuntimeDictionarySource {
   public:
    explicit MediaWikiRuntimeSource(
        const goldendict::core::MediaWikiSourceConfiguration& configuration)
        : source_(configuration.base_url) {
        identity_.id = configuration.id;
        identity_.name = configuration.name;
        identity_.source = configuration.base_url;
    }

    const RuntimeDictionaryIdentity& identity() const noexcept override {
        return identity_;
    }

    std::vector<RuntimeDictionaryArticle> LookupExact(
        std::string_view headword,
        const RuntimeRequestOptions& options) const override {
        CheckRequest(options);
        if (options.result_limit == 0U) {
            return {};
        }
        try {
            const auto article =
                source_.FetchArticle(headword, StopRequested(options));
            CheckRequest(options);
            return {{article.title, "html", article.html}};
        } catch (const HttpError& error) {
            TranslateHttpError(error, options);
        }
    }

    std::vector<RuntimeDictionaryArticle> LookupPrefix(
        std::string_view prefix,
        const RuntimeRequestOptions& options) const override {
        return LookupExact(prefix, options);
    }

    std::vector<std::string> SuggestPrefix(
        std::string_view prefix,
        const RuntimeRequestOptions& options) const override {
        CheckRequest(options);
        if (options.result_limit == 0U) {
            return {};
        }
        try {
            auto suggestions = source_.Suggest(prefix, options.result_limit,
                                               StopRequested(options));
            CheckRequest(options);
            return suggestions;
        } catch (const HttpError& error) {
            TranslateHttpError(error, options);
        }
    }

    std::optional<RuntimeDictionaryResource> GetResource(
        std::string_view, const RuntimeRequestOptions& options) const override {
        CheckRequest(options);
        return std::nullopt;
    }

   private:
    RuntimeDictionaryIdentity identity_;
    MediaWikiSource source_;
};

class WebsiteRuntimeSource final : public RuntimeDictionarySource {
   public:
    explicit WebsiteRuntimeSource(
        const goldendict::core::WebsiteSourceConfiguration& configuration)
        : source_(configuration.url_template) {
        identity_.id = configuration.id;
        identity_.name = configuration.name;
        identity_.source = configuration.url_template;
    }

    const RuntimeDictionaryIdentity& identity() const noexcept override {
        return identity_;
    }

    std::vector<RuntimeDictionaryArticle> LookupExact(
        std::string_view headword,
        const RuntimeRequestOptions& options) const override {
        CheckRequest(options);
        if (options.result_limit == 0U) {
            return {};
        }
        try {
            const auto page = source_.Fetch(headword, StopRequested(options));
            CheckRequest(options);
            return {{std::string(headword), "html", page.html}};
        } catch (const HttpError& error) {
            TranslateHttpError(error, options);
        }
    }

    std::vector<RuntimeDictionaryArticle> LookupPrefix(
        std::string_view prefix,
        const RuntimeRequestOptions& options) const override {
        return LookupExact(prefix, options);
    }

    std::vector<std::string> SuggestPrefix(
        std::string_view, const RuntimeRequestOptions& options) const override {
        CheckRequest(options);
        return {};
    }

    std::optional<RuntimeDictionaryResource> GetResource(
        std::string_view, const RuntimeRequestOptions& options) const override {
        CheckRequest(options);
        return std::nullopt;
    }

   private:
    RuntimeDictionaryIdentity identity_;
    WebsiteSource source_;
};

}  // namespace

std::vector<std::unique_ptr<RuntimeDictionarySource>>
ComposeConfiguredRuntimeSources(
    const goldendict::core::CoreConfiguration& configuration) {
    goldendict::core::ValidateConfiguration(configuration);
    std::vector<std::unique_ptr<RuntimeDictionarySource>> result;
    for (const auto& source : configuration.mediawiki_sources) {
        if (source.enabled) {
            result.push_back(std::make_unique<MediaWikiRuntimeSource>(source));
        }
    }
    for (const auto& source : configuration.website_sources) {
        if (source.enabled) {
            result.push_back(std::make_unique<WebsiteRuntimeSource>(source));
        }
    }
    return result;
}

}  // namespace goldendict::network
