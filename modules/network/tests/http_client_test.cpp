// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <chrono>
#include <future>
#include <string>
#include <utility>

#include "../src/http_client.h"
#include "goldendict/network/network_runtime.h"

namespace goldendict::network {
namespace {

class HttpFixture final : public QObject {
   public:
    HttpFixture() {
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            while (server_.hasPendingConnections()) {
                QTcpSocket* socket = server_.nextPendingConnection();
                connect(
                    socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                        QByteArray request =
                            socket->property("request").toByteArray();
                        request += socket->readAll();
                        if (!request.contains("\r\n\r\n")) {
                            socket->setProperty("request", request);
                            return;
                        }
                        const QByteArray target = request.split(' ').value(1);
                        QByteArray response;
                        if (target == "/ok") {
                            response =
                                "HTTP/1.1 200 OK\r\nContent-Type: text/plain; "
                                "charset=utf-8\r\nContent-Length: 5\r\n"
                                "Connection: close\r\n\r\nhello";
                        } else if (target == "/cache") {
                            ++cache_requests_;
                            response =
                                "HTTP/1.1 200 OK\r\nContent-Type: "
                                "text/plain\r\n"
                                "Cache-Control: public, max-age=3600\r\n"
                                "Content-Length: 6\r\nConnection: close\r\n\r\n"
                                "cached";
                        } else if (target == "/redirect") {
                            response =
                                "HTTP/1.1 302 Found\r\nLocation: /ok\r\n"
                                "Content-Length: 0\r\nConnection: "
                                "close\r\n\r\n";
                        } else if (target == "/loop") {
                            response =
                                "HTTP/1.1 302 Found\r\nLocation: /loop\r\n"
                                "Content-Length: 0\r\nConnection: "
                                "close\r\n\r\n";
                        } else if (target == "/large") {
                            response =
                                "HTTP/1.1 200 OK\r\nContent-Length: 8\r\n"
                                "Connection: close\r\n\r\n12345678";
                        } else if (target == "/slow") {
                            return;
                        } else if (target == "/authenticated" ||
                                   target == "/authenticated-redirect") {
                            if (request.contains("\r\nAuthorization: Basic "
                                                 "dXNlcjpwYXNz\r\n")) {
                                if (target == "/authenticated-redirect") {
                                    response =
                                        "HTTP/1.1 302 Found\r\nLocation: " +
                                        QByteArray::fromStdString(
                                            cross_origin_target_) +
                                        "\r\nContent-Length: 0\r\n"
                                        "Connection: close\r\n\r\n";
                                } else {
                                    response =
                                        "HTTP/1.1 200 OK\r\nContent-Length: "
                                        "2\r\n"
                                        "Connection: close\r\n\r\nok";
                                }
                            } else {
                                response =
                                    "HTTP/1.1 401 Unauthorized\r\n"
                                    "WWW-Authenticate: Basic "
                                    "realm=\"fixture\"\r\n"
                                    "Content-Length: 0\r\nConnection: "
                                    "close\r\n\r\n";
                            }
                        } else if (target == "/credential-leak") {
                            if (request.contains("\r\nAuthorization:")) {
                                response =
                                    "HTTP/1.1 400 Bad Request\r\n"
                                    "Content-Length: 0\r\nConnection: "
                                    "close\r\n\r\n";
                            } else {
                                response =
                                    "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n"
                                    "Connection: close\r\n\r\nsafe";
                            }
                        } else {
                            response =
                                "HTTP/1.1 404 Not Found\r\nContent-Length: "
                                "0\r\n"
                                "Connection: close\r\n\r\n";
                        }
                        socket->write(response);
                        socket->disconnectFromHost();
                    });
            }
        });
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            qFatal("Could not start local HTTP fixture");
        }
    }

    std::string Url(std::string_view path) const {
        return "http://127.0.0.1:" +
               std::to_string(static_cast<unsigned int>(server_.serverPort())) +
               std::string(path);
    }

    void SetCrossOriginTarget(std::string target) {
        cross_origin_target_ = std::move(target);
    }

    int cache_requests() const { return cache_requests_; }

   private:
    QTcpServer server_;
    std::string cross_origin_target_;
    int cache_requests_ = 0;
};

class ProxyFixture final : public QObject {
   public:
    ProxyFixture() {
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            while (server_.hasPendingConnections()) {
                QTcpSocket* socket = server_.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                    QByteArray request =
                        socket->property("request").toByteArray();
                    request += socket->readAll();
                    if (!request.contains("\r\n\r\n")) {
                        socket->setProperty("request", request);
                        return;
                    }
                    QByteArray response;
                    if (!request.contains("\r\nProxy-Authorization: Basic "
                                          "cHJveHk6c2VjcmV0\r\n")) {
                        response =
                            "HTTP/1.1 407 Proxy Authentication Required\r\n"
                            "Proxy-Authenticate: Basic realm=\"proxy\"\r\n"
                            "Content-Length: 0\r\nConnection: close\r\n\r\n";
                    } else if (request.startsWith(
                                   "GET http://example.test/proxied ")) {
                        response =
                            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                            "Content-Length: 7\r\nConnection: close\r\n\r\n"
                            "proxied";
                    } else {
                        response =
                            "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n"
                            "Connection: close\r\n\r\n";
                    }
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            qFatal("Could not start local proxy fixture");
        }
    }

    HttpRequest::Proxy Configuration() const {
        return {"127.0.0.1", static_cast<unsigned short>(server_.serverPort()),
                HttpRequest::Credentials{"proxy", "secret"}};
    }

   private:
    QTcpServer server_;
};

template <typename Callback>
void VerifyError(HttpErrorCode expected, Callback&& callback) {
    try {
        callback();
        QFAIL("Expected HTTP error");
    } catch (const HttpError& error) {
        QCOMPARE(error.code(), expected);
    }
}

HttpResponse FetchWhileProcessingEvents(
    const std::shared_ptr<NetworkRuntime>& runtime,
    const HttpRequest& request) {
    auto future = std::async(std::launch::async, [runtime, request]() {
        return runtime->Fetch(request);
    });
    while (future.wait_for(std::chrono::milliseconds(1)) !=
           std::future_status::ready) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return future.get();
}

}  // namespace

class HttpClientTest : public QObject {
    Q_OBJECT

   private slots:
    void FetchesResponseAndFollowsRelativeRedirect();
    void RejectsInvalidUrlsRedirectLoopsAndHttpErrors();
    void EnforcesResponseAndTimeBudgets();
    void ObservesCancellation();
    void SupportsScopedOriginAndProxyAuthentication();
    void PreparesAndCleansOnlyOwnedCacheStorage();
    void OwnsExactIsolatedPersistentCache();
    void DisablesAndClearsOnlyOwnedCache();
    void CancelsAndJoinsBeforeShutdownCleanup();
    void ActivatesPreparedPolicyTransactionally();
};

void HttpClientTest::FetchesResponseAndFollowsRelativeRedirect() {
    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/redirect");

    const HttpResponse response = FetchHttp(request);

    QCOMPARE(response.status_code, 200);
    QCOMPARE(response.content_type, std::string("text/plain; charset=utf-8"));
    QCOMPARE(std::string(response.body.begin(), response.body.end()),
             std::string("hello"));
    QVERIFY(response.final_url.size() >= 3U);
    QCOMPARE(response.final_url.substr(response.final_url.size() - 3U),
             std::string("/ok"));
}

void HttpClientTest::RejectsInvalidUrlsRedirectLoopsAndHttpErrors() {
    HttpRequest invalid_url;
    invalid_url.url = "file:///etc/passwd";
    VerifyError(HttpErrorCode::kInvalidUrl, [&]() { FetchHttp(invalid_url); });
    invalid_url.url = "http://user:secret@example.test/";
    VerifyError(HttpErrorCode::kInvalidUrl, [&]() { FetchHttp(invalid_url); });
    HttpRequest invalid_request;
    invalid_request.url = "http://example.test/";
    invalid_request.maximum_response_bytes = 0U;
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { FetchHttp(invalid_request); });

    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/loop");
    request.maximum_redirects = 1U;
    VerifyError(HttpErrorCode::kTooManyRedirects,
                [&]() { FetchHttp(request); });

    request.url = fixture.Url("/missing");
    VerifyError(HttpErrorCode::kHttpStatus, [&]() { FetchHttp(request); });
}

void HttpClientTest::EnforcesResponseAndTimeBudgets() {
    HttpFixture fixture;
    HttpRequest large;
    large.url = fixture.Url("/large");
    large.maximum_response_bytes = 4U;
    VerifyError(HttpErrorCode::kResponseTooLarge, [&]() { FetchHttp(large); });

    HttpRequest slow;
    slow.url = fixture.Url("/slow");
    slow.timeout = std::chrono::milliseconds(30);
    VerifyError(HttpErrorCode::kDeadlineExceeded, [&]() { FetchHttp(slow); });
}

void HttpClientTest::ObservesCancellation() {
    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/slow");
    int polls = 0;
    VerifyError(HttpErrorCode::kCancelled,
                [&]() { FetchHttp(request, [&]() { return ++polls > 1; }); });
}

void HttpClientTest::SupportsScopedOriginAndProxyAuthentication() {
    HttpFixture fixture;
    HttpRequest origin_request;
    origin_request.url = fixture.Url("/authenticated");
    origin_request.credentials = HttpRequest::Credentials{"user", "pass"};
    const HttpResponse origin_response = FetchHttp(origin_request);
    QCOMPARE(
        std::string(origin_response.body.begin(), origin_response.body.end()),
        std::string("ok"));

    HttpFixture cross_origin;
    fixture.SetCrossOriginTarget(cross_origin.Url("/credential-leak"));
    origin_request.url = fixture.Url("/authenticated-redirect");
    const HttpResponse redirected_response = FetchHttp(origin_request);
    QCOMPARE(std::string(redirected_response.body.begin(),
                         redirected_response.body.end()),
             std::string("safe"));

    ProxyFixture proxy;
    HttpRequest proxy_request;
    proxy_request.url = "http://example.test/proxied";
    proxy_request.proxy = proxy.Configuration();
    const HttpResponse proxy_response = FetchHttp(proxy_request);
    QCOMPARE(
        std::string(proxy_response.body.begin(), proxy_response.body.end()),
        std::string("proxied"));

    HttpRequest invalid_proxy = proxy_request;
    invalid_proxy.proxy->port = 0U;
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { FetchHttp(invalid_proxy); });

    HttpRequest redacted_proxy;
    redacted_proxy.url = "http://example.test/";
    redacted_proxy.timeout = std::chrono::milliseconds(100);
    redacted_proxy.proxy = HttpRequest::Proxy{
        "secret-proxy.invalid", 3128U,
        HttpRequest::Credentials{"secret-user", "secret-password"}};
    try {
        static_cast<void>(FetchHttp(redacted_proxy));
        QFAIL("Expected proxy transport error");
    } catch (const HttpError& error) {
        QCOMPARE(error.code(), HttpErrorCode::kTransport);
        const QByteArray message(error.what());
        QVERIFY(!message.contains("secret-proxy"));
        QVERIFY(!message.contains("secret-user"));
        QVERIFY(!message.contains("secret-password"));
    }
}

void HttpClientTest::PreparesAndCleansOnlyOwnedCacheStorage() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QFile sibling(QDir(root.path()).filePath("webengine-sentinel"));
    QVERIFY(sibling.open(QIODevice::WriteOnly));
    QCOMPARE(sibling.write("browser"), qint64{7});
    sibling.close();

    const auto prepared =
        NetworkRuntime::Prepare({1U, false}, root.path().toStdString());
    QVERIFY(prepared.cache_available);
    QCOMPARE(prepared.cache_directory,
             QDir(root.path()).filePath("qt-network-http").toStdString());
    QVERIFY(QDir(QString::fromStdString(prepared.cache_directory)).exists());
    QVERIFY(
        !QFile::exists(QDir(QString::fromStdString(prepared.cache_directory))
                           .filePath(".goldendict-write-test")));

    auto disabled = NetworkRuntime::Create(
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString()));
    QVERIFY(
        !QDir(QString::fromStdString(disabled->cache_directory())).exists());
    QVERIFY(sibling.exists());
    disabled.reset();

    auto idempotent = NetworkRuntime::Create(
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString()));
    QVERIFY(
        !QDir(QString::fromStdString(idempotent->cache_directory())).exists());
    QVERIFY(sibling.exists());
}

void HttpClientTest::OwnsExactIsolatedPersistentCache() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto preparation =
        NetworkRuntime::Prepare({7U, false}, root.path().toStdString());
    QVERIFY(preparation.cache_available);
    QCOMPARE(preparation.cache_directory,
             QDir(root.path()).filePath("qt-network-http").toStdString());
    auto runtime = NetworkRuntime::Create(std::move(preparation));
    QCOMPARE(runtime->maximum_cache_bytes(), 7LL * 1024LL * 1024LL);

    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/cache");
    request.runtime = runtime;
    const auto first = FetchWhileProcessingEvents(runtime, request);
    const auto second = FetchWhileProcessingEvents(runtime, request);
    QCOMPARE(std::string(first.body.begin(), first.body.end()),
             std::string("cached"));
    QCOMPARE(std::string(second.body.begin(), second.body.end()),
             std::string("cached"));
    QCOMPARE(fixture.cache_requests(), 1);
    runtime.reset();

    auto restarted = NetworkRuntime::Create(
        NetworkRuntime::Prepare({7U, false}, root.path().toStdString()));
    request.runtime = restarted;
    const auto response = FetchWhileProcessingEvents(restarted, request);
    QCOMPARE(std::string(response.body.begin(), response.body.end()),
             std::string("cached"));
    QCOMPARE(fixture.cache_requests(), 1);
}

void HttpClientTest::DisablesAndClearsOnlyOwnedCache() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QFile sibling(QDir(root.path()).filePath("webengine-sentinel"));
    QVERIFY(sibling.open(QIODevice::WriteOnly));
    QCOMPARE(sibling.write("browser"), qint64{7});
    sibling.close();

    auto disabled = NetworkRuntime::Create(
        NetworkRuntime::Prepare({0U, true}, root.path().toStdString()));
    QCOMPARE(disabled->maximum_cache_bytes(), 0);
    QVERIFY(
        !QDir(QString::fromStdString(disabled->cache_directory())).exists());
    disabled.reset();
    QVERIFY(sibling.exists());

    auto clearing = NetworkRuntime::Create(
        NetworkRuntime::Prepare({1U, true}, root.path().toStdString()));
    const QString owned = QString::fromStdString(clearing->cache_directory());
    QVERIFY(QDir(owned).exists());
    clearing.reset();
    QVERIFY(!QDir(owned).exists());
    QVERIFY(sibling.exists());

    const auto failed =
        NetworkRuntime::Prepare({1U, false}, sibling.fileName().toStdString());
    QVERIFY(!failed.cache_available);
    QCOMPARE(failed.diagnostic,
             std::string("Qt Network cache setup failed; HTTP traffic will "
                         "remain uncached"));
    const auto missing_root = NetworkRuntime::Prepare({1U, false}, {});
    QVERIFY(!missing_root.cache_available);
    QVERIFY(missing_root.cache_directory.empty());
}

void HttpClientTest::CancelsAndJoinsBeforeShutdownCleanup() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({10240U, true}, root.path().toStdString()));
    QCOMPARE(runtime->maximum_cache_bytes(), 10240LL * 1024LL * 1024LL);
    const QString owned = QString::fromStdString(runtime->cache_directory());

    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/slow");
    request.timeout = std::chrono::seconds(5);
    auto fetch = std::async(std::launch::async, [runtime, request]() {
        try {
            static_cast<void>(runtime->Fetch(request));
            return false;
        } catch (const HttpError& error) {
            return error.code() == HttpErrorCode::kCancelled;
        }
    });
    QTest::qWait(20);
    runtime->Shutdown();
    QVERIFY(fetch.get());
    QVERIFY(!QDir(owned).exists());

    HttpRequest rejected;
    rejected.url = fixture.Url("/ok");
    VerifyError(HttpErrorCode::kCancelled,
                [&]() { static_cast<void>(runtime->Fetch(rejected)); });
}

void HttpClientTest::ActivatesPreparedPolicyTransactionally() {
    QTemporaryDir root;
    QTemporaryDir other_root;
    QVERIFY(root.isValid());
    QVERIFY(other_root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));

    auto wrong_owner =
        NetworkRuntime::Prepare({1U, false}, other_root.path().toStdString());
    QVERIFY(!runtime->Activate(std::move(wrong_owner)));
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);

    auto reduced =
        NetworkRuntime::Prepare({1U, false}, root.path().toStdString());
    QVERIFY(runtime->Activate(std::move(reduced)));
    QCOMPARE(runtime->maximum_cache_bytes(), 1LL * 1024LL * 1024LL);

    const QString owned = QString::fromStdString(runtime->cache_directory());
    QVERIFY(QDir(owned).exists());
    auto disabled =
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString());
    QVERIFY(runtime->Activate(std::move(disabled)));
    QCOMPARE(runtime->maximum_cache_bytes(), 0);
    QVERIFY(!QDir(owned).exists());
}

}  // namespace goldendict::network

using goldendict::network::HttpClientTest;

QTEST_GUILESS_MAIN(HttpClientTest)

#include "http_client_test.moc"
