// SPDX-License-Identifier: GPL-3.0-or-later

#include "forvo_source.h"

#include <QByteArray>
#include <QStringDecoder>
#include <QUrl>
#include <QXmlStreamReader>

#include <algorithm>
#include <utility>

namespace goldendict::network {
namespace {

constexpr std::size_t kMaximumWordBytes = 320U;
constexpr std::size_t kMaximumApiKeyBytes = 256U;
constexpr std::size_t kMaximumForvoResults = 50U;

QString DecodeUtf8(std::string_view value, const char* name) {
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded =
        decoder(QByteArray(value.data(), static_cast<qsizetype>(value.size())));
    if (decoder.hasError()) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        std::string("Forvo ") + name + " must be valid UTF-8");
    }
    return decoded;
}

void ValidateRemoteUrl(std::string_view value, const char* name,
                       HttpErrorCode error_code) {
    const QUrl url(DecodeUtf8(value, name), QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != "http" && scheme != "https") ||
        url.host().isEmpty() || !url.userInfo().isEmpty()) {
        throw HttpError(error_code,
                        std::string("Forvo ") + name + " URL is invalid");
    }
}

int ParseVoteCount(const QString& text) {
    bool valid = false;
    const int value = text.toInt(&valid);
    if (!valid || value < 0) {
        throw HttpError(HttpErrorCode::kInvalidResponse,
                        "Forvo response contains an invalid vote count");
    }
    return value;
}

std::vector<ForvoPronunciation> ParsePronunciations(
    const HttpResponse& response, std::size_t maximum_results) {
    const QByteArray bytes(reinterpret_cast<const char*>(response.body.data()),
                           static_cast<qsizetype>(response.body.size()));
    QXmlStreamReader xml(bytes);
    std::vector<ForvoPronunciation> results;
    while (!xml.atEnd() && results.size() < maximum_results) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != u"item") {
            continue;
        }
        ForvoPronunciation pronunciation;
        int total_votes = 0;
        while (xml.readNextStartElement()) {
            const QString name = xml.name().toString();
            const QString text = xml.readElementText();
            if (name == "pathmp3") {
                pronunciation.audio_url = text.toStdString();
            } else if (name == "username") {
                pronunciation.username = text.toStdString();
            } else if (name == "country") {
                pronunciation.country = text.toStdString();
            } else if (name == "sex") {
                pronunciation.sex = text.toLower().toStdString();
            } else if (name == "num_votes") {
                total_votes = ParseVoteCount(text);
            } else if (name == "num_positive_votes") {
                pronunciation.positive_votes = ParseVoteCount(text);
            }
        }
        if (pronunciation.audio_url.empty() ||
            pronunciation.positive_votes > total_votes) {
            throw HttpError(HttpErrorCode::kInvalidResponse,
                            "Forvo response contains an incomplete item");
        }
        ValidateRemoteUrl(pronunciation.audio_url, "audio",
                          HttpErrorCode::kInvalidResponse);
        pronunciation.negative_votes =
            total_votes - pronunciation.positive_votes;
        results.push_back(std::move(pronunciation));
    }
    if (xml.hasError()) {
        throw HttpError(HttpErrorCode::kInvalidResponse,
                        "Forvo response contains malformed XML");
    }
    return results;
}

}  // namespace

ForvoSource::ForvoSource(std::string api_base_url, std::string api_key,
                         std::string language_code, HttpRequest transport)
    : api_base_url_(std::move(api_base_url)),
      api_key_(std::move(api_key)),
      language_code_(std::move(language_code)),
      transport_(std::move(transport)) {
    if (!transport_.url.empty() || api_key_.empty() ||
        api_key_.size() > kMaximumApiKeyBytes || language_code_.size() < 2U ||
        language_code_.size() > 16U ||
        !std::all_of(language_code_.begin(), language_code_.end(), [](char ch) {
            return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                   ch == '-';
        })) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        "Forvo source configuration is invalid");
    }
    ValidateRemoteUrl(api_base_url_, "API base", HttpErrorCode::kInvalidUrl);
}

std::vector<ForvoPronunciation> ForvoSource::Lookup(
    std::string_view word, std::size_t maximum_results,
    const std::function<bool()>& is_cancelled) const {
    if (word.empty() || word.size() > kMaximumWordBytes ||
        maximum_results == 0U || maximum_results > kMaximumForvoResults) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        "Forvo lookup word or result limit is invalid");
    }
    return ParsePronunciations(Fetch(RequestUrl(word), is_cancelled),
                               maximum_results);
}

ForvoAudio ForvoSource::FetchAudio(
    std::string_view audio_url,
    const std::function<bool()>& is_cancelled) const {
    ValidateRemoteUrl(audio_url, "audio", HttpErrorCode::kInvalidUrl);
    HttpResponse response = Fetch(std::string(audio_url), is_cancelled);
    if (!QString::fromStdString(response.content_type)
             .toLower()
             .startsWith("audio/")) {
        throw HttpError(HttpErrorCode::kInvalidResponse,
                        "Forvo audio response has an invalid content type");
    }
    return {std::move(response.content_type), std::move(response.body)};
}

std::string ForvoSource::RequestUrl(std::string_view word) const {
    QUrl base(DecodeUtf8(api_base_url_, "API base"), QUrl::StrictMode);
    QString path = base.path();
    if (!path.endsWith('/')) {
        path += '/';
    }
    path += "key/";
    path += QString::fromLatin1(
        QUrl::toPercentEncoding(DecodeUtf8(api_key_, "API key")));
    path += "/action/word-pronunciations/format/xml/word/";
    path +=
        QString::fromLatin1(QUrl::toPercentEncoding(DecodeUtf8(word, "word")));
    path += "/language/";
    path += QString::fromLatin1(
        QUrl::toPercentEncoding(DecodeUtf8(language_code_, "language code")));
    path += "/order/rate-desc";
    base.setPath(path, QUrl::StrictMode);
    base.setQuery(QString{});
    base.setFragment(QString{});
    return base.toString(QUrl::FullyEncoded).toStdString();
}

HttpResponse ForvoSource::Fetch(
    const std::string& url, const std::function<bool()>& is_cancelled) const {
    HttpRequest request = transport_;
    request.url = url;
    return FetchHttp(request, is_cancelled);
}

}  // namespace goldendict::network
