// SPDX-License-Identifier: GPL-3.0-or-later

#include "dict_server_source.h"

#include <QElapsedTimer>
#include <QStringDecoder>
#include <QTcpSocket>

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>

namespace goldendict::network {
namespace {

constexpr std::size_t kMaximumQueryBytes = 4096U;
constexpr std::size_t kMaximumResults = 60U;
constexpr std::size_t kMaximumLineBytes = 64U * 1024U;
constexpr std::size_t kMaximumProxyHeaderBytes = 64U * 1024U;
constexpr int kWaitSliceMilliseconds = 50;

[[noreturn]] void InvalidResponse(const std::string& message) {
    throw DictServerError(DictServerErrorCode::kInvalidResponse,
                          "Invalid DICT response: " + message);
}

void CheckCancellation(const std::function<bool()>& is_cancelled) {
    if (is_cancelled && is_cancelled()) {
        throw DictServerError(DictServerErrorCode::kCancelled,
                              "DICT request was cancelled");
    }
}

QString DecodeUtf8(std::string_view value, const char* name,
                   DictServerErrorCode error_code) {
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded =
        decoder(QByteArray(value.data(), static_cast<qsizetype>(value.size())));
    if (decoder.hasError()) {
        throw DictServerError(
            error_code, std::string("DICT ") + name + " must be valid UTF-8");
    }
    return decoded;
}

void ValidateAtom(std::string_view value, const char* name) {
    if (value.empty() || value.size() > 128U ||
        !std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isalnum(ch) != 0 || ch == '*' || ch == '-' ||
                   ch == '_' || ch == '.' || ch == '!';
        })) {
        throw DictServerError(DictServerErrorCode::kInvalidConfiguration,
                              std::string("DICT ") + name + " is invalid");
    }
}

QByteArray QuoteQuery(std::string_view query) {
    if (query.empty() || query.size() > kMaximumQueryBytes) {
        throw DictServerError(DictServerErrorCode::kInvalidRequest,
                              "DICT query must be non-empty and bounded");
    }
    const QByteArray utf8 =
        DecodeUtf8(query, "query", DictServerErrorCode::kInvalidRequest)
            .toUtf8();
    if (utf8.contains('\r') || utf8.contains('\n') || utf8.contains('\0')) {
        throw DictServerError(DictServerErrorCode::kInvalidRequest,
                              "DICT query contains a forbidden control byte");
    }
    QByteArray quoted;
    quoted.reserve(utf8.size() + 2);
    quoted += '"';
    for (const char ch : utf8) {
        if (ch == '"' || ch == '\\') {
            quoted += '\\';
        }
        quoted += ch;
    }
    quoted += '"';
    return quoted;
}

class DictConnection final {
   public:
    DictConnection(const DictServerOptions& options,
                   const std::function<bool()>& is_cancelled)
        : options_(options), is_cancelled_(is_cancelled) {
        timer_.start();
        const bool proxied = options.proxy.has_value();
        socket_.connectToHost(QString::fromStdString(
                                  proxied ? options.proxy->host : options.host),
                              proxied ? options.proxy->port : options.port);
        while (socket_.state() != QAbstractSocket::ConnectedState) {
            CheckCancellation(is_cancelled_);
            const int wait = WaitInterval();
            if (!socket_.waitForConnected(wait) &&
                socket_.state() != QAbstractSocket::ConnectingState) {
                throw DictServerError(
                    proxied ? DictServerErrorCode::kProxyTransport
                            : DictServerErrorCode::kTransport,
                    proxied ? "DICT proxy connection failed"
                            : "DICT server connection failed");
            }
        }
        if (proxied) {
            EstablishProxyTunnel();
        }
        ExpectStatus(220);
        Write("CLIENT GoldenDict\r\n");
        ExpectStatus(250);
        Write("OPTION MIME\r\n");
        ExpectStatus(250);
    }

    ~DictConnection() {
        if (socket_.state() == QAbstractSocket::ConnectedState) {
            socket_.write("QUIT\r\n");
            socket_.disconnectFromHost();
        }
    }

    void Write(const QByteArray& command) {
        CheckCancellation(is_cancelled_);
        if (socket_.write(command) != command.size()) {
            throw DictServerError(DictServerErrorCode::kTransport,
                                  "DICT command write failed");
        }
        while (socket_.bytesToWrite() != 0) {
            CheckCancellation(is_cancelled_);
            if (!socket_.waitForBytesWritten(WaitInterval()) &&
                socket_.error() != QAbstractSocket::UnknownSocketError) {
                throw DictServerError(DictServerErrorCode::kTransport,
                                      "DICT command write failed");
            }
        }
    }

    QByteArray ReadLine() {
        while (!proxy_buffer_.contains('\n') && !socket_.canReadLine()) {
            CheckCancellation(is_cancelled_);
            if (socket_.bytesAvailable() >
                static_cast<qint64>(kMaximumLineBytes)) {
                throw DictServerError(DictServerErrorCode::kResponseTooLarge,
                                      "DICT response line is too large");
            }
            if (!socket_.waitForReadyRead(WaitInterval()) &&
                socket_.state() != QAbstractSocket::ConnectedState) {
                throw DictServerError(DictServerErrorCode::kTransport,
                                      "DICT server closed the connection");
            }
        }
        QByteArray line = std::move(proxy_buffer_);
        proxy_buffer_.clear();
        if (!line.contains('\n')) {
            line += socket_.readLine();
        } else {
            const qsizetype end = line.indexOf('\n') + 1;
            proxy_buffer_ = line.mid(end);
            line.truncate(end);
        }
        received_bytes_ += static_cast<std::size_t>(line.size());
        if (received_bytes_ > options_.maximum_response_bytes ||
            line.size() > static_cast<qsizetype>(kMaximumLineBytes)) {
            throw DictServerError(DictServerErrorCode::kResponseTooLarge,
                                  "DICT response is too large");
        }
        while (line.endsWith('\n') || line.endsWith('\r')) {
            line.chop(1);
        }
        return line;
    }

    int ReadStatus() {
        const QByteArray line = ReadLine();
        if (line.size() < 3 ||
            !std::isdigit(static_cast<unsigned char>(line[0])) ||
            !std::isdigit(static_cast<unsigned char>(line[1])) ||
            !std::isdigit(static_cast<unsigned char>(line[2])) ||
            (line.size() > 3 && line[3] != ' ' && line[3] != '-')) {
            InvalidResponse("malformed status line");
        }
        return (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
    }

    void ExpectStatus(int expected) {
        if (ReadStatus() != expected) {
            InvalidResponse("unexpected status code");
        }
    }

   private:
    void EstablishProxyTunnel() {
        const QByteArray authority =
            QByteArray::fromStdString(options_.host) + ":" +
            QByteArray::number(static_cast<unsigned int>(options_.port));
        QByteArray request = "CONNECT " + authority +
                             " HTTP/1.1\r\nHost: " + authority +
                             "\r\nProxy-Connection: Keep-Alive\r\n";
        if (options_.proxy->credentials.has_value()) {
            const auto& credentials = *options_.proxy->credentials;
            const QByteArray token =
                (QByteArray::fromStdString(credentials.username) + ":" +
                 QByteArray::fromStdString(credentials.password))
                    .toBase64();
            request += "Proxy-Authorization: Basic " + token + "\r\n";
        }
        request += "\r\n";
        if (request.size() > static_cast<qsizetype>(kMaximumProxyHeaderBytes) ||
            socket_.write(request) != request.size()) {
            throw DictServerError(DictServerErrorCode::kProxyTransport,
                                  "DICT proxy request failed");
        }
        while (socket_.bytesToWrite() != 0) {
            CheckCancellation(is_cancelled_);
            if (!socket_.waitForBytesWritten(WaitInterval()) &&
                socket_.error() != QAbstractSocket::UnknownSocketError) {
                throw DictServerError(DictServerErrorCode::kProxyTransport,
                                      "DICT proxy request failed");
            }
        }

        QByteArray response;
        while (!response.contains("\r\n\r\n")) {
            CheckCancellation(is_cancelled_);
            if (socket_.bytesAvailable() == 0 &&
                !socket_.waitForReadyRead(WaitInterval()) &&
                socket_.state() != QAbstractSocket::ConnectedState) {
                throw DictServerError(DictServerErrorCode::kProxyTransport,
                                      "DICT proxy closed the connection");
            }
            response += socket_.readAll();
            if (response.size() >
                static_cast<qsizetype>(kMaximumProxyHeaderBytes)) {
                throw DictServerError(DictServerErrorCode::kProxyTransport,
                                      "DICT proxy response is invalid");
            }
        }
        const qsizetype header_end = response.indexOf("\r\n\r\n") + 4;
        const QByteArray header = response.left(header_end);
        proxy_buffer_ = response.mid(header_end);
        const QByteArray status_line = header.left(header.indexOf("\r\n"));
        const QList<QByteArray> parts = status_line.split(' ');
        bool valid = false;
        const int status = parts.size() >= 2 ? parts[1].toInt(&valid) : 0;
        if (!status_line.startsWith("HTTP/1.") || !valid) {
            throw DictServerError(DictServerErrorCode::kProxyTransport,
                                  "DICT proxy response is invalid");
        }
        if (status == 407) {
            throw DictServerError(
                DictServerErrorCode::kProxyAuthenticationRequired,
                "DICT proxy authentication is required");
        }
        if (status < 200 || status >= 300) {
            throw DictServerError(DictServerErrorCode::kProxyTransport,
                                  "DICT proxy rejected the connection");
        }
    }

    int WaitInterval() const {
        const qint64 remaining = options_.timeout.count() - timer_.elapsed();
        if (remaining <= 0) {
            throw DictServerError(DictServerErrorCode::kDeadlineExceeded,
                                  "DICT request deadline exceeded");
        }
        return static_cast<int>(std::min<qint64>(
            remaining, static_cast<qint64>(kWaitSliceMilliseconds)));
    }

    const DictServerOptions& options_;
    const std::function<bool()>& is_cancelled_;
    QTcpSocket socket_;
    QElapsedTimer timer_;
    QByteArray proxy_buffer_;
    std::size_t received_bytes_ = 0U;
};

std::size_t ParseCount(const QByteArray& line, int expected_status) {
    if (!line.startsWith(QByteArray::number(expected_status) + " ")) {
        InvalidResponse("unexpected result status");
    }
    const QByteArray remainder = line.mid(4);
    const qsizetype separator = remainder.indexOf(' ');
    const QByteArray count_text =
        separator < 0 ? remainder : remainder.left(separator);
    bool valid = false;
    const qulonglong count = count_text.toULongLong(&valid);
    if (!valid || count > kMaximumResults) {
        InvalidResponse("invalid or excessive result count");
    }
    return static_cast<std::size_t>(count);
}

std::vector<QByteArray> ParseTokens(const QByteArray& line) {
    std::vector<QByteArray> tokens;
    qsizetype position = 0;
    while (position < line.size()) {
        while (position < line.size() && line[position] == ' ') {
            ++position;
        }
        if (position == line.size()) {
            break;
        }
        QByteArray token;
        const bool quoted = line[position] == '"';
        if (quoted) {
            ++position;
        }
        bool closed = !quoted;
        while (position < line.size()) {
            const char ch = line[position++];
            if (quoted && ch == '\\') {
                if (position == line.size()) {
                    InvalidResponse("dangling token escape");
                }
                token += line[position++];
            } else if (quoted && ch == '"') {
                closed = true;
                break;
            } else if (!quoted && ch == ' ') {
                break;
            } else {
                token += ch;
            }
        }
        if (!closed ||
            (quoted && position < line.size() && line[position] != ' ')) {
            InvalidResponse("malformed quoted token");
        }
        tokens.push_back(std::move(token));
    }
    return tokens;
}

std::string StrictResponseText(const QByteArray& value, const char* name) {
    return DecodeUtf8(std::string_view(value.constData(), value.size()), name,
                      DictServerErrorCode::kInvalidResponse)
        .toStdString();
}

}  // namespace

DictServerError::DictServerError(DictServerErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

DictServerSource::DictServerSource(DictServerOptions options)
    : options_(std::move(options)) {
    if (options_.host.empty() || options_.host.size() > 253U ||
        options_.host.find_first_of("\r\n") != std::string::npos ||
        options_.host.find('\0') != std::string::npos || options_.port == 0U ||
        options_.timeout.count() <= 0 ||
        options_.maximum_response_bytes == 0U ||
        options_.maximum_response_bytes > 64U * 1024U * 1024U) {
        throw DictServerError(DictServerErrorCode::kInvalidConfiguration,
                              "DICT server configuration is invalid");
    }
    if (options_.proxy.has_value() &&
        (options_.proxy->host.empty() || options_.proxy->host.size() > 253U ||
         options_.proxy->host.find_first_of("\r\n\0") != std::string::npos ||
         options_.proxy->port == 0U)) {
        throw DictServerError(DictServerErrorCode::kInvalidConfiguration,
                              "DICT proxy configuration is invalid");
    }
    static_cast<void>(DecodeUtf8(options_.host, "host",
                                 DictServerErrorCode::kInvalidConfiguration));
    ValidateAtom(options_.database, "database");
    ValidateAtom(options_.strategy, "strategy");
}

std::vector<std::string> DictServerSource::Suggest(
    std::string_view prefix, std::size_t maximum_results,
    const std::function<bool()>& is_cancelled) const {
    if (maximum_results == 0U || maximum_results > kMaximumResults) {
        throw DictServerError(DictServerErrorCode::kInvalidRequest,
                              "DICT result limit must be between 1 and 60");
    }
    DictConnection connection(options_, is_cancelled);
    connection.Write("MATCH " + QByteArray::fromStdString(options_.database) +
                     " " + QByteArray::fromStdString(options_.strategy) + " " +
                     QuoteQuery(prefix) + "\r\n");
    QByteArray status_line = connection.ReadLine();
    if (status_line.startsWith("552 ") || status_line == "552") {
        return {};
    }
    const std::size_t count = ParseCount(status_line, 152);
    std::vector<std::string> results;
    std::unordered_set<std::string> seen;
    for (std::size_t index = 0; index < count; ++index) {
        const std::vector<QByteArray> tokens =
            ParseTokens(connection.ReadLine());
        if (tokens.size() != 2U) {
            InvalidResponse("malformed match record");
        }
        std::string word = StrictResponseText(tokens[1], "match");
        if (!word.empty() && seen.insert(word).second &&
            results.size() < maximum_results) {
            results.push_back(std::move(word));
        }
    }
    if (connection.ReadLine() != ".") {
        InvalidResponse("missing match terminator");
    }
    connection.ExpectStatus(250);
    return results;
}

std::vector<DictServerArticle> DictServerSource::Define(
    std::string_view word, std::size_t maximum_articles,
    const std::function<bool()>& is_cancelled) const {
    if (maximum_articles == 0U || maximum_articles > kMaximumResults) {
        throw DictServerError(DictServerErrorCode::kInvalidRequest,
                              "DICT article limit must be between 1 and 60");
    }
    DictConnection connection(options_, is_cancelled);
    connection.Write("DEFINE " + QByteArray::fromStdString(options_.database) +
                     " " + QuoteQuery(word) + "\r\n");
    QByteArray status_line = connection.ReadLine();
    if (status_line.startsWith("552 ") || status_line == "552") {
        return {};
    }
    const std::size_t count = ParseCount(status_line, 150);
    std::vector<DictServerArticle> articles;
    for (std::size_t index = 0; index < count; ++index) {
        const QByteArray definition_line = connection.ReadLine();
        if (!definition_line.startsWith("151 ")) {
            InvalidResponse("missing definition header");
        }
        const std::vector<QByteArray> tokens =
            ParseTokens(definition_line.mid(4));
        if (tokens.size() != 3U) {
            InvalidResponse("malformed definition header");
        }

        std::string content_type = "text/plain";
        for (;;) {
            const QByteArray header = connection.ReadLine();
            if (header.isEmpty()) {
                break;
            }
            const qsizetype separator = header.indexOf(':');
            if (separator <= 0) {
                InvalidResponse("malformed MIME header");
            }
            if (header.left(separator).trimmed().compare(
                    "Content-Type", Qt::CaseInsensitive) == 0) {
                content_type = StrictResponseText(
                    header.mid(separator + 1).trimmed(), "content type");
            }
        }

        std::string body;
        bool first_line = true;
        for (;;) {
            QByteArray line = connection.ReadLine();
            if (line == ".") {
                break;
            }
            if (line.startsWith("..")) {
                line.remove(0, 1);
            }
            if (!first_line) {
                body += '\n';
            }
            body += StrictResponseText(line, "definition");
            first_line = false;
        }
        if (articles.size() < maximum_articles) {
            articles.push_back({StrictResponseText(tokens[0], "headword"),
                                StrictResponseText(tokens[1], "database"),
                                StrictResponseText(tokens[2], "database name"),
                                std::move(content_type), std::move(body)});
        }
    }
    connection.ExpectStatus(250);
    return articles;
}

}  // namespace goldendict::network
