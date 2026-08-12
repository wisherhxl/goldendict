// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include <chrono>
#include <string>

#include "../src/http_client.h"

namespace goldendict::network {
namespace {

class HttpFixture final : public QObject {
   public:
    HttpFixture() {
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
                    const QByteArray target = request.split(' ').value(1);
                    QByteArray response;
                    if (target == "/ok") {
                        response =
                            "HTTP/1.1 200 OK\r\nContent-Type: text/plain; "
                            "charset=utf-8\r\nContent-Length: 5\r\n"
                            "Connection: close\r\n\r\nhello";
                    } else if (target == "/redirect") {
                        response =
                            "HTTP/1.1 302 Found\r\nLocation: /ok\r\n"
                            "Content-Length: 0\r\nConnection: close\r\n\r\n";
                    } else if (target == "/loop") {
                        response =
                            "HTTP/1.1 302 Found\r\nLocation: /loop\r\n"
                            "Content-Length: 0\r\nConnection: close\r\n\r\n";
                    } else if (target == "/large") {
                        response =
                            "HTTP/1.1 200 OK\r\nContent-Length: 8\r\n"
                            "Connection: close\r\n\r\n12345678";
                    } else if (target == "/slow") {
                        return;
                    } else {
                        response =
                            "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
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

}  // namespace

class HttpClientTest : public QObject {
    Q_OBJECT

   private slots:
    void FetchesResponseAndFollowsRelativeRedirect();
    void RejectsInvalidUrlsRedirectLoopsAndHttpErrors();
    void EnforcesResponseAndTimeBudgets();
    void ObservesCancellation();
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
    VerifyError(HttpErrorCode::kInvalidUrl,
                []() { FetchHttp({"file:///etc/passwd"}); });
    VerifyError(HttpErrorCode::kInvalidUrl,
                []() { FetchHttp({"http://user:secret@example.test/"}); });
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

}  // namespace goldendict::network

using goldendict::network::HttpClientTest;

QTEST_GUILESS_MAIN(HttpClientTest)

#include "http_client_test.moc"
