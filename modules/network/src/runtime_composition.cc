// SPDX-License-Identifier: GPL-3.0-or-later

#include "runtime_composition.h"

#include <QByteArray>
#include <QStringDecoder>

#include <chrono>
#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dict_server_source.h"
#include "forvo_source.h"
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

std::string HexEncode(std::string_view value) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2U);
    for (const unsigned char byte : value) {
        result.push_back(kHex[byte >> 4U]);
        result.push_back(kHex[byte & 0x0fU]);
    }
    return result;
}

bool IsUtf8(std::string_view value) {
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded =
        decoder(QByteArray(value.data(), static_cast<qsizetype>(value.size())));
    static_cast<void>(decoded);
    return !decoder.hasError();
}

std::optional<std::string> HexDecode(std::string_view value) {
    if (value.size() % 2U != 0U) {
        return std::nullopt;
    }
    const auto digit = [](char character) -> int {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }
        return -1;
    };
    std::string result;
    result.reserve(value.size() / 2U);
    for (std::size_t index = 0; index < value.size(); index += 2U) {
        const int high = digit(value[index]);
        const int low = digit(value[index + 1U]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        result.push_back(static_cast<char>((high << 4U) | low));
    }
    return result;
}

std::string ForvoChildId(std::string_view source_id,
                         std::string_view language) {
    return "forvo:" + std::to_string(source_id.size()) + ":" +
           std::string(source_id) + ":" + HexEncode(language);
}

std::string EscapeHtml(std::string_view value) {
    std::string result;
    for (const char character : value) {
        switch (character) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\'':
                result += "&#39;";
                break;
            default:
                result.push_back(character);
                break;
        }
    }
    return result;
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

[[noreturn]] void TranslateDictError(const DictServerError& error,
                                     const RuntimeRequestOptions& options) {
    if (std::chrono::steady_clock::now() >= options.deadline) {
        throw RuntimeSourceError(
            RuntimeSourceErrorCode::kDeadlineExceeded,
            "Network source request deadline was exceeded");
    }
    RuntimeSourceErrorCode code = RuntimeSourceErrorCode::kUnavailable;
    switch (error.code()) {
        case DictServerErrorCode::kCancelled:
            code = RuntimeSourceErrorCode::kCancelled;
            break;
        case DictServerErrorCode::kDeadlineExceeded:
            code = RuntimeSourceErrorCode::kDeadlineExceeded;
            break;
        case DictServerErrorCode::kInvalidRequest:
        case DictServerErrorCode::kInvalidConfiguration:
        case DictServerErrorCode::kInvalidResponse:
            code = RuntimeSourceErrorCode::kInvalidData;
            break;
        case DictServerErrorCode::kResponseTooLarge:
        case DictServerErrorCode::kTransport:
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

class ForvoRuntimeSource final : public RuntimeDictionarySource {
   public:
    ForvoRuntimeSource(
        const goldendict::core::ForvoSourceConfiguration& configuration,
        std::string credential, std::string language)
        : source_(configuration.api_base_url, std::move(credential), language) {
        identity_.id = ForvoChildId(configuration.id, language);
        identity_.name = configuration.name + " (" + language + ")";
        identity_.source = configuration.api_base_url;
        identity_.description =
            "Configured Forvo source ID: " + configuration.id;
        identity_.source_language = std::move(language);
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
            const auto pronunciations = source_.Lookup(
                headword, std::min<std::size_t>(options.result_limit, 50U),
                StopRequested(options));
            CheckRequest(options);
            std::string html;
            for (const auto& pronunciation : pronunciations) {
                html += "<p>";
                if (!pronunciation.username.empty()) {
                    html += EscapeHtml(pronunciation.username) + " ";
                }
                if (!pronunciation.country.empty()) {
                    html += "(" + EscapeHtml(pronunciation.country) + ") ";
                }
                html += "<audio controls><source src=\"audio/" +
                        HexEncode(pronunciation.audio_url) + "\"></audio></p>";
            }
            if (html.empty()) {
                return {};
            }
            return {{std::string(headword), "text/html", std::move(html)}};
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
        std::string_view resource_id,
        const RuntimeRequestOptions& options) const override {
        CheckRequest(options);
        constexpr std::string_view kPrefix = "audio/";
        if (resource_id.substr(0U, kPrefix.size()) != kPrefix) {
            return std::nullopt;
        }
        const auto url = HexDecode(resource_id.substr(kPrefix.size()));
        if (!url.has_value()) {
            return std::nullopt;
        }
        try {
            auto audio = source_.FetchAudio(*url, StopRequested(options));
            CheckRequest(options);
            std::vector<std::byte> data;
            data.reserve(audio.bytes.size());
            for (const unsigned char byte : audio.bytes) {
                data.push_back(static_cast<std::byte>(byte));
            }
            return RuntimeDictionaryResource{std::string(resource_id),
                                             std::move(audio.content_type),
                                             std::move(data)};
        } catch (const HttpError& error) {
            TranslateHttpError(error, options);
        }
    }

   private:
    RuntimeDictionaryIdentity identity_;
    ForvoSource source_;
};

class DictRuntimeSource final : public RuntimeDictionarySource {
   public:
    explicit DictRuntimeSource(
        const goldendict::core::DictServerSourceConfiguration& configuration)
        : source_({configuration.host, configuration.port,
                   configuration.database, configuration.strategy}) {
        identity_.id = configuration.id;
        identity_.name = configuration.name;
        identity_.source =
            configuration.host + ":" +
            std::to_string(static_cast<unsigned int>(configuration.port));
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
            const auto articles = source_.Define(
                headword, std::min<std::size_t>(options.result_limit, 60U),
                StopRequested(options));
            CheckRequest(options);
            std::vector<RuntimeDictionaryArticle> result;
            result.reserve(articles.size());
            for (const auto& article : articles) {
                const bool html =
                    article.content_type.substr(0U, 9U) == "text/html";
                result.push_back({article.headword, html ? "text/html" : "text",
                                  article.body});
            }
            return result;
        } catch (const DictServerError& error) {
            TranslateDictError(error, options);
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
            auto result = source_.Suggest(
                prefix, std::min<std::size_t>(options.result_limit, 60U),
                StopRequested(options));
            CheckRequest(options);
            return result;
        } catch (const DictServerError& error) {
            TranslateDictError(error, options);
        }
    }

    std::optional<RuntimeDictionaryResource> GetResource(
        std::string_view, const RuntimeRequestOptions& options) const override {
        CheckRequest(options);
        return std::nullopt;
    }

   private:
    RuntimeDictionaryIdentity identity_;
    DictServerSource source_;
};

}  // namespace

RuntimeCompositionResult ComposeConfiguredRuntimeSources(
    const goldendict::core::CoreConfiguration& configuration,
    const ForvoCredentialMap& forvo_credentials) {
    goldendict::core::ValidateConfiguration(configuration);
    std::set<std::string> forvo_ids;
    for (const auto& source : configuration.forvo_sources) {
        forvo_ids.insert(source.id);
    }
    for (const auto& [source_id, credential] : forvo_credentials) {
        if (forvo_ids.count(source_id) == 0U || credential.empty() ||
            credential.size() > 256U || !IsUtf8(credential)) {
            throw std::invalid_argument(
                "Forvo credential injection is invalid");
        }
    }

    std::set<std::string> runtime_ids;
    const auto add_id = [&runtime_ids](const std::string& id) {
        if (!runtime_ids.insert(id).second) {
            throw std::invalid_argument("Runtime source identity collision");
        }
    };
    for (const auto& source : configuration.mediawiki_sources) {
        if (source.enabled) {
            add_id(source.id);
        }
    }
    for (const auto& source : configuration.website_sources) {
        if (source.enabled) {
            add_id(source.id);
        }
    }
    for (const auto& source : configuration.forvo_sources) {
        if (source.enabled && forvo_credentials.count(source.id) != 0U) {
            for (const auto& language : source.language_codes) {
                add_id(ForvoChildId(source.id, language));
            }
        }
    }
    for (const auto& source : configuration.dict_server_sources) {
        if (source.enabled) {
            add_id(source.id);
        }
    }

    RuntimeCompositionResult result;
    for (const auto& source : configuration.mediawiki_sources) {
        if (source.enabled) {
            result.sources.push_back(
                std::make_unique<MediaWikiRuntimeSource>(source));
        }
    }
    for (const auto& source : configuration.website_sources) {
        if (source.enabled) {
            result.sources.push_back(
                std::make_unique<WebsiteRuntimeSource>(source));
        }
    }
    for (const auto& source : configuration.forvo_sources) {
        if (!source.enabled) {
            continue;
        }
        const auto credential = forvo_credentials.find(source.id);
        if (credential == forvo_credentials.end()) {
            result.diagnostics.push_back(
                {RuntimeCompositionDiagnosticCode::kMissingForvoCredential,
                 source.id});
            continue;
        }
        for (const auto& language : source.language_codes) {
            result.sources.push_back(std::make_unique<ForvoRuntimeSource>(
                source, credential->second, language));
        }
    }
    for (const auto& source : configuration.dict_server_sources) {
        if (source.enabled) {
            result.sources.push_back(
                std::make_unique<DictRuntimeSource>(source));
        }
    }
    return result;
}

}  // namespace goldendict::network
