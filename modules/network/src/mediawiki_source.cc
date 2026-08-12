// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediawiki_source.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringDecoder>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace goldendict::network {
namespace {

constexpr std::size_t kMaximumQueryBytes = 4096U;
constexpr std::size_t kMediaWikiMaximumSuggestions = 50U;

[[noreturn]] void InvalidResponse(const std::string& message) {
    throw HttpError(HttpErrorCode::kInvalidResponse,
                    "Invalid MediaWiki response: " + message);
}

QJsonObject ParseResponse(const HttpResponse& response) {
    QJsonParseError error;
    const QByteArray bytes(reinterpret_cast<const char*>(response.body.data()),
                           static_cast<qsizetype>(response.body.size()));
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        InvalidResponse("malformed JSON");
    }
    const QJsonObject root = document.object();
    if (root.contains("error")) {
        InvalidResponse("API error");
    }
    return root;
}

void ValidateQuery(std::string_view value, const char* name) {
    if (value.empty() || value.size() > kMaximumQueryBytes) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        std::string("MediaWiki ") + name +
                            " must be non-empty and bounded");
    }
}

QString DecodeQuery(std::string_view value) {
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded =
        decoder(QByteArray(value.data(), static_cast<qsizetype>(value.size())));
    if (decoder.hasError()) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        "MediaWiki query must be valid UTF-8");
    }
    return decoded;
}

}  // namespace

MediaWikiSource::MediaWikiSource(std::string base_url, HttpRequest transport)
    : base_url_(std::move(base_url)), transport_(std::move(transport)) {
    if (!transport_.url.empty()) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        "MediaWiki transport template URL must be empty");
    }
    static_cast<void>(ApiUrl({}));
}

std::vector<std::string> MediaWikiSource::Suggest(
    std::string_view prefix, std::size_t maximum_results,
    const std::function<bool()>& is_cancelled) const {
    ValidateQuery(prefix, "prefix");
    if (maximum_results == 0U ||
        maximum_results > kMediaWikiMaximumSuggestions) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        "MediaWiki suggestion limit must be between 1 and 50");
    }

    const QJsonObject root = ParseResponse(
        Fetch(ApiUrl({{"action", "query"},
                      {"list", "allpages"},
                      {"apfrom", std::string(prefix)},
                      {"aplimit", std::to_string(maximum_results)},
                      {"format", "json"},
                      {"formatversion", "2"}}),
              is_cancelled));
    const QJsonValue pages_value =
        root.value("query").toObject().value("allpages");
    if (!pages_value.isArray()) {
        InvalidResponse("missing allpages array");
    }

    std::vector<std::string> suggestions;
    std::unordered_set<std::string> seen;
    for (const QJsonValue& value : pages_value.toArray()) {
        const QJsonValue title_value = value.toObject().value("title");
        if (!title_value.isString()) {
            InvalidResponse("suggestion without a title");
        }
        std::string title = title_value.toString().toStdString();
        if (!title.empty() && seen.insert(title).second) {
            suggestions.push_back(std::move(title));
            if (suggestions.size() == maximum_results) {
                break;
            }
        }
    }
    return suggestions;
}

MediaWikiArticle MediaWikiSource::FetchArticle(
    std::string_view title, const std::function<bool()>& is_cancelled) const {
    ValidateQuery(title, "title");
    const QJsonObject root =
        ParseResponse(Fetch(ApiUrl({{"action", "parse"},
                                    {"page", std::string(title)},
                                    {"prop", "text"},
                                    {"format", "json"},
                                    {"formatversion", "2"}}),
                            is_cancelled));
    const QJsonObject parse = root.value("parse").toObject();
    const QJsonValue response_title = parse.value("title");
    const QJsonValue html = parse.value("text");
    if (!response_title.isString() || !html.isString()) {
        InvalidResponse("missing parsed title or text");
    }
    return {response_title.toString().toStdString(),
            html.toString().toStdString()};
}

std::string MediaWikiSource::ApiUrl(
    const std::vector<std::pair<std::string, std::string>>& query) const {
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded_base = decoder(QByteArray::fromStdString(base_url_));
    if (decoder.hasError()) {
        throw HttpError(HttpErrorCode::kInvalidUrl,
                        "MediaWiki base URL must be valid UTF-8");
    }
    QUrl url(decoded_base, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != "http" && scheme != "https") ||
        url.host().isEmpty() || !url.userInfo().isEmpty() ||
        !url.query().isEmpty() || url.hasFragment()) {
        throw HttpError(HttpErrorCode::kInvalidUrl,
                        "MediaWiki base URL must be an HTTP origin/path");
    }
    QString path = url.path();
    if (!path.endsWith("/api.php")) {
        if (!path.endsWith('/')) {
            path += '/';
        }
        path += "api.php";
    }
    url.setPath(path);
    QUrlQuery url_query;
    for (const auto& [name, value] : query) {
        url_query.addQueryItem(QString::fromStdString(name),
                               DecodeQuery(value));
    }
    url.setQuery(url_query);
    return url.toString(QUrl::FullyEncoded).toStdString();
}

HttpResponse MediaWikiSource::Fetch(
    const std::string& url, const std::function<bool()>& is_cancelled) const {
    HttpRequest request = transport_;
    request.url = url;
    return FetchHttp(request, is_cancelled);
}

}  // namespace goldendict::network
