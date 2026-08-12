// SPDX-License-Identifier: GPL-3.0-or-later

#include "website_source.h"

#include <QByteArray>
#include <QStringDecoder>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <utility>
#include <vector>

namespace goldendict::network {
namespace {

constexpr std::string_view kWordMarker = "%GDWORD%";
constexpr std::size_t kMaximumWordBytes = 4096U;

QString DecodeUtf8(std::string_view bytes, HttpErrorCode error_code,
                   const char* message) {
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded =
        decoder(QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size())));
    if (decoder.hasError()) {
        throw HttpError(error_code, message);
    }
    return decoded;
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](char ch) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    });
    return value;
}

std::optional<std::string> HeaderCharset(const std::string& content_type) {
    const std::string lower = LowerAscii(content_type);
    const std::size_t marker = lower.find("charset=");
    if (marker == std::string::npos) {
        return std::nullopt;
    }
    std::size_t first = marker + 8U;
    while (first < lower.size() &&
           std::isspace(static_cast<unsigned char>(lower[first]))) {
        ++first;
    }
    const char quote =
        first < lower.size() && (lower[first] == '\'' || lower[first] == '"')
            ? lower[first++]
            : '\0';
    std::size_t last = first;
    while (last < lower.size() &&
           ((quote != '\0' && lower[last] != quote) ||
            (quote == '\0' && lower[last] != ';' &&
             !std::isspace(static_cast<unsigned char>(lower[last]))))) {
        ++last;
    }
    return lower.substr(first, last - first);
}

QString DecodeWindows1252(const std::vector<unsigned char>& bytes) {
    constexpr std::array<char32_t, 32> kSpecial = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};
    QString decoded;
    decoded.reserve(static_cast<qsizetype>(bytes.size()));
    for (unsigned char byte : bytes) {
        const char32_t value =
            byte >= 0x80U && byte <= 0x9FU ? kSpecial[byte - 0x80U] : byte;
        decoded.append(QString::fromUcs4(&value, 1));
    }
    return decoded;
}

QString DecodeHtml(const HttpResponse& response) {
    const std::string charset =
        HeaderCharset(response.content_type).value_or(std::string("utf-8"));
    if (charset == "utf-8" || charset == "utf8") {
        return DecodeUtf8(std::string_view(reinterpret_cast<const char*>(
                                               response.body.data()),
                                           response.body.size()),
                          HttpErrorCode::kInvalidResponse,
                          "Website response is not valid UTF-8");
    }
    if (charset == "iso-8859-1" || charset == "latin1" ||
        charset == "latin-1") {
        return QString::fromLatin1(
            reinterpret_cast<const char*>(response.body.data()),
            static_cast<qsizetype>(response.body.size()));
    }
    if (charset == "windows-1252" || charset == "cp1252") {
        return DecodeWindows1252(response.body);
    }
    throw HttpError(HttpErrorCode::kInvalidResponse,
                    "Website response uses an unsupported charset");
}

bool IsNameCharacter(QChar ch) {
    return ch.isLetterOrNumber() || ch == '-' || ch == '_' || ch == ':';
}

QString RewriteTag(const QString& tag, const QUrl& base) {
    QString rewritten = tag;

    struct Replacement {
        qsizetype first;
        qsizetype length;
        QString value;
    };

    std::vector<Replacement> replacements;
    qsizetype position = 1;
    while (position < tag.size()) {
        while (position < tag.size() && !IsNameCharacter(tag[position])) {
            ++position;
        }
        const qsizetype name_first = position;
        while (position < tag.size() && IsNameCharacter(tag[position])) {
            ++position;
        }
        const QString name =
            tag.mid(name_first, position - name_first).toLower();
        if (name != "href" && name != "src") {
            continue;
        }
        while (position < tag.size() && tag[position].isSpace()) {
            ++position;
        }
        if (position >= tag.size() || tag[position] != '=') {
            continue;
        }
        ++position;
        while (position < tag.size() && tag[position].isSpace()) {
            ++position;
        }
        const QChar quote = position < tag.size() && (tag[position] == '\'' ||
                                                      tag[position] == '"')
                                ? tag[position++]
                                : QChar();
        const qsizetype value_first = position;
        while (position < tag.size() &&
               ((!quote.isNull() && tag[position] != quote) ||
                (quote.isNull() && !tag[position].isSpace() &&
                 tag[position] != '>'))) {
            ++position;
        }
        const QString value = tag.mid(value_first, position - value_first);
        const QUrl reference(value, QUrl::StrictMode);
        if (!value.isEmpty() && !value.startsWith('#') && reference.isValid() &&
            reference.scheme().isEmpty()) {
            replacements.push_back(
                {value_first, position - value_first,
                 base.resolved(reference).toString(QUrl::FullyEncoded)});
        }
    }
    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
        rewritten.replace(it->first, it->length, it->value);
    }
    return rewritten;
}

QString RewriteRelativeLinks(QString html, const std::string& final_url) {
    const QUrl base(QString::fromStdString(final_url), QUrl::StrictMode);
    qsizetype position = 0;
    while ((position = html.indexOf('<', position)) >= 0) {
        qsizetype end = position + 1;
        QChar quote;
        for (; end < html.size(); ++end) {
            if (quote.isNull() && (html[end] == '\'' || html[end] == '"')) {
                quote = html[end];
            } else if (!quote.isNull() && html[end] == quote) {
                quote = QChar();
            } else if (quote.isNull() && html[end] == '>') {
                break;
            }
        }
        if (end == html.size()) {
            break;
        }
        const QString tag = html.mid(position, end - position + 1);
        const QString rewritten = RewriteTag(tag, base);
        html.replace(position, tag.size(), rewritten);
        position += rewritten.size();
    }
    return html;
}

}  // namespace

WebsiteSource::WebsiteSource(std::string url_template, HttpRequest transport)
    : url_template_(std::move(url_template)), transport_(std::move(transport)) {
    if (!transport_.url.empty()) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        "Website transport template URL must be empty");
    }
    if (url_template_.find(kWordMarker) == std::string::npos) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        "Website URL template must contain %GDWORD%");
    }
    static_cast<void>(RequestUrl("validation"));
}

WebsitePage WebsiteSource::Fetch(
    std::string_view word, const std::function<bool()>& is_cancelled) const {
    HttpRequest request = transport_;
    request.url = RequestUrl(word);
    HttpResponse response = FetchHttp(request, is_cancelled);
    QString html =
        RewriteRelativeLinks(DecodeHtml(response), response.final_url);
    return {std::move(response.final_url), html.toStdString()};
}

std::string WebsiteSource::RequestUrl(std::string_view word) const {
    if (word.empty() || word.size() > kMaximumWordBytes) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        "Website word must be non-empty and bounded");
    }
    const QString decoded = DecodeUtf8(word, HttpErrorCode::kInvalidRequest,
                                       "Website word must be valid UTF-8");
    const std::string encoded = QUrl::toPercentEncoding(decoded).toStdString();
    std::string url = url_template_;
    for (std::size_t marker = url.find(kWordMarker);
         marker != std::string::npos; marker = url.find(kWordMarker, marker)) {
        url.replace(marker, kWordMarker.size(), encoded);
        marker += encoded.size();
    }
    const QString decoded_url =
        DecodeUtf8(url, HttpErrorCode::kInvalidUrl,
                   "Website URL template must be valid UTF-8");
    const QUrl validated(decoded_url, QUrl::StrictMode);
    const QString scheme = validated.scheme().toLower();
    if (!validated.isValid() || (scheme != "http" && scheme != "https") ||
        validated.host().isEmpty() || !validated.userInfo().isEmpty()) {
        throw HttpError(HttpErrorCode::kInvalidUrl,
                        "Website URL template produced an invalid HTTP URL");
    }
    return validated.toString(QUrl::FullyEncoded).toStdString();
}

}  // namespace goldendict::network
