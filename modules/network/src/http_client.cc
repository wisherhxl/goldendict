// SPDX-License-Identifier: GPL-3.0-or-later

#include "http_client.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace goldendict::network {
namespace {

constexpr auto kCancellationPollInterval = std::chrono::milliseconds(10);

QUrl ValidateUrl(const QString& value) {
    QUrl url(value, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != "http" && scheme != "https") ||
        url.host().isEmpty() || !url.userInfo().isEmpty()) {
        throw HttpError(HttpErrorCode::kInvalidUrl,
                        "HTTP URL must use http or https without user info");
    }
    url.setFragment({});
    return url;
}

std::chrono::milliseconds Remaining(const QElapsedTimer& elapsed,
                                    std::chrono::milliseconds timeout) {
    const auto spent = std::chrono::milliseconds(elapsed.elapsed());
    return spent >= timeout ? std::chrono::milliseconds::zero()
                            : timeout - spent;
}

bool HasSameOrigin(const QUrl& first, const QUrl& second) {
    return first.scheme().compare(second.scheme(), Qt::CaseInsensitive) == 0 &&
           first.host().compare(second.host(), Qt::CaseInsensitive) == 0 &&
           first.port(first.scheme() == "https" ? 443 : 80) ==
               second.port(second.scheme() == "https" ? 443 : 80);
}

}  // namespace

HttpError::HttpError(HttpErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

HttpResponse FetchHttp(const HttpRequest& request,
                       const std::function<bool()>& is_cancelled) {
    if (QCoreApplication::instance() == nullptr) {
        throw HttpError(HttpErrorCode::kTransport,
                        "HTTP fetch requires a core application event loop");
    }
    if (request.timeout <= std::chrono::milliseconds::zero() ||
        request.maximum_response_bytes == 0U) {
        throw HttpError(HttpErrorCode::kInvalidRequest,
                        "HTTP timeout and response limit must be positive");
    }

    QUrl url = ValidateUrl(QString::fromStdString(request.url));
    const QUrl credential_origin = url;
    QNetworkAccessManager manager;
    if (request.proxy.has_value()) {
        if (request.proxy->host.empty() || request.proxy->port == 0U) {
            throw HttpError(HttpErrorCode::kInvalidRequest,
                            "HTTP proxy host and port must be valid");
        }
        QNetworkProxy proxy(QNetworkProxy::HttpProxy,
                            QString::fromStdString(request.proxy->host),
                            request.proxy->port);
        manager.setProxy(proxy);
    }
    QObject::connect(
        &manager, &QNetworkAccessManager::authenticationRequired, &manager,
        [&](QNetworkReply* challenged_reply, QAuthenticator* authenticator) {
            if (!request.credentials.has_value() ||
                !HasSameOrigin(credential_origin,
                               challenged_reply->request().url())) {
                return;
            }
            authenticator->setUser(
                QString::fromStdString(request.credentials->username));
            authenticator->setPassword(
                QString::fromStdString(request.credentials->password));
        });
    QObject::connect(
        &manager, &QNetworkAccessManager::proxyAuthenticationRequired, &manager,
        [&](const QNetworkProxy&, QAuthenticator* authenticator) {
            if (!request.proxy.has_value() ||
                !request.proxy->credentials.has_value()) {
                return;
            }
            authenticator->setUser(
                QString::fromStdString(request.proxy->credentials->username));
            authenticator->setPassword(
                QString::fromStdString(request.proxy->credentials->password));
        });
    QElapsedTimer elapsed;
    elapsed.start();

    for (std::size_t redirects = 0;; ++redirects) {
        if (is_cancelled && is_cancelled()) {
            throw HttpError(HttpErrorCode::kCancelled,
                            "HTTP request was cancelled");
        }
        const auto remaining = Remaining(elapsed, request.timeout);
        if (remaining <= std::chrono::milliseconds::zero()) {
            throw HttpError(HttpErrorCode::kDeadlineExceeded,
                            "HTTP request deadline was exceeded");
        }

        QNetworkRequest network_request(url);
        network_request.setAttribute(
            QNetworkRequest::RedirectPolicyAttribute,
            QVariant::fromValue(
                static_cast<int>(QNetworkRequest::ManualRedirectPolicy)));
        network_request.setHeader(QNetworkRequest::UserAgentHeader,
                                  "GoldenDict/1.6");
        QNetworkReply* reply = manager.get(network_request);
        QEventLoop loop;
        QTimer deadline_timer;
        deadline_timer.setSingleShot(true);
        QTimer cancellation_timer;
        cancellation_timer.setInterval(
            static_cast<int>(kCancellationPollInterval.count()));
        std::vector<unsigned char> body;
        bool too_large = false;
        bool deadline_exceeded = false;
        bool cancelled = false;

        QObject::connect(reply, &QIODevice::readyRead, &loop, [&]() {
            const QByteArray chunk = reply->readAll();
            if (chunk.size() < 0 ||
                static_cast<std::size_t>(chunk.size()) >
                    request.maximum_response_bytes - body.size()) {
                too_large = true;
                reply->abort();
                return;
            }
            const auto* first =
                reinterpret_cast<const unsigned char*>(chunk.constData());
            body.insert(body.end(), first, first + chunk.size());
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop,
                         &QEventLoop::quit);
        QObject::connect(&deadline_timer, &QTimer::timeout, &loop, [&]() {
            deadline_exceeded = true;
            reply->abort();
        });
        QObject::connect(&cancellation_timer, &QTimer::timeout, &loop, [&]() {
            if (is_cancelled && is_cancelled()) {
                cancelled = true;
                reply->abort();
            }
        });
        deadline_timer.start(static_cast<int>(std::min<std::int64_t>(
            remaining.count(), std::numeric_limits<int>::max())));
        if (is_cancelled) {
            cancellation_timer.start();
        }
        loop.exec();
        deadline_timer.stop();
        cancellation_timer.stop();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QUrl redirect =
            reply->attribute(QNetworkRequest::RedirectionTargetAttribute)
                .toUrl();
        const QString content_type =
            reply->header(QNetworkRequest::ContentTypeHeader).toString();
        const QNetworkReply::NetworkError transport_error = reply->error();
        const QString transport_message = reply->errorString();
        reply->deleteLater();

        if (cancelled) {
            throw HttpError(HttpErrorCode::kCancelled,
                            "HTTP request was cancelled");
        }
        if (deadline_exceeded) {
            throw HttpError(HttpErrorCode::kDeadlineExceeded,
                            "HTTP request deadline was exceeded");
        }
        if (too_large) {
            throw HttpError(HttpErrorCode::kResponseTooLarge,
                            "HTTP response exceeds the size limit");
        }
        if (!redirect.isEmpty() && status >= 300 && status < 400) {
            if (redirects >= request.maximum_redirects) {
                throw HttpError(HttpErrorCode::kTooManyRedirects,
                                "HTTP redirect limit was exceeded");
            }
            const QUrl next = ValidateUrl(url.resolved(redirect).toString());
            if (url.scheme().compare("https", Qt::CaseInsensitive) == 0 &&
                next.scheme().compare("http", Qt::CaseInsensitive) == 0) {
                throw HttpError(HttpErrorCode::kRedirectRejected,
                                "HTTPS to HTTP redirect was rejected");
            }
            url = next;
            continue;
        }
        if (status < 200 || status >= 300) {
            if (status == 0) {
                throw HttpError(HttpErrorCode::kTransport,
                                transport_message.toStdString());
            }
            throw HttpError(
                HttpErrorCode::kHttpStatus,
                "HTTP server returned status " + std::to_string(status));
        }
        if (transport_error != QNetworkReply::NoError) {
            throw HttpError(HttpErrorCode::kTransport,
                            transport_message.toStdString());
        }

        return {status, url.toString(QUrl::FullyEncoded).toStdString(),
                content_type.toStdString(), std::move(body)};
    }
}

}  // namespace goldendict::network
