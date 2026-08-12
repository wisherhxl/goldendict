// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include <string>

#include "../src/website_source.h"

namespace goldendict::network {
namespace {

class WebsiteFixture final : public QObject {
   public:
    WebsiteFixture() {
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
                    QByteArray content_type = "text/html; charset=utf-8";
                    QByteArray body;
                    if (target == "/lookup?q=A%26B") {
                        body =
                            "<a href='../next'>Next</a>"
                            "<img src=\"/image.png\">"
                            "<a href='https://outside.test/x'>Outside</a>"
                            "<a href='#section'>Section</a>";
                    } else if (target == "/lookup?q=currency") {
                        content_type = "text/html; charset=windows-1252";
                        body = QByteArray("<p>") + QByteArray(1, char(0x80)) +
                               " price</p>";
                    } else if (target == "/lookup?q=unknown") {
                        content_type = "text/html; charset=x-unknown";
                        body = "text";
                    } else {
                        body = "<p>fallback</p>";
                    }
                    const QByteArray response =
                        "HTTP/1.1 200 OK\r\nContent-Type: " + content_type +
                        "\r\nContent-Length: " +
                        QByteArray::number(body.size()) +
                        "\r\nConnection: close\r\n\r\n" + body;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            qFatal("Could not start local website fixture");
        }
    }

    std::string Template() const {
        return "http://127.0.0.1:" +
               std::to_string(static_cast<unsigned int>(server_.serverPort())) +
               "/lookup?q=%GDWORD%";
    }

   private:
    QTcpServer server_;
};

template <typename Callback>
void VerifyError(HttpErrorCode expected, Callback&& callback) {
    try {
        callback();
        QFAIL("Expected website error");
    } catch (const HttpError& error) {
        QCOMPARE(error.code(), expected);
    }
}

}  // namespace

class WebsiteSourceTest : public QObject {
    Q_OBJECT

   private slots:
    void EncodesQueriesAndRewritesRelativeLinks();
    void DecodesDeclaredLegacyCharset();
    void RejectsInvalidTemplatesQueriesAndCharsets();
};

void WebsiteSourceTest::EncodesQueriesAndRewritesRelativeLinks() {
    WebsiteFixture fixture;
    const WebsiteSource source(fixture.Template());
    const WebsitePage page = source.Fetch("A&B");

    QVERIFY(page.html.find("href='http://127.0.0.1:") != std::string::npos);
    QVERIFY(page.html.find("/next'") != std::string::npos);
    QVERIFY(page.html.find("src=\"http://127.0.0.1:") != std::string::npos);
    QVERIFY(page.html.find("/image.png\"") != std::string::npos);
    QVERIFY(page.html.find("href='https://outside.test/x'") !=
            std::string::npos);
    QVERIFY(page.html.find("href='#section'") != std::string::npos);
}

void WebsiteSourceTest::DecodesDeclaredLegacyCharset() {
    WebsiteFixture fixture;
    const WebsiteSource source(fixture.Template());
    const WebsitePage page = source.Fetch("currency");
    QCOMPARE(page.html, std::string("<p>€ price</p>"));
}

void WebsiteSourceTest::RejectsInvalidTemplatesQueriesAndCharsets() {
    VerifyError(HttpErrorCode::kInvalidRequest, []() {
        static_cast<void>(WebsiteSource("https://example.test/no-marker"));
    });
    VerifyError(HttpErrorCode::kInvalidUrl, []() {
        static_cast<void>(WebsiteSource("file:///tmp/%GDWORD%"));
    });

    WebsiteFixture fixture;
    const WebsiteSource source(fixture.Template());
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Fetch("")); });
    const std::string invalid_utf8(1U, static_cast<char>(0xFF));
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Fetch(invalid_utf8)); });
    VerifyError(HttpErrorCode::kInvalidResponse,
                [&]() { static_cast<void>(source.Fetch("unknown")); });
}

}  // namespace goldendict::network

using goldendict::network::WebsiteSourceTest;

QTEST_GUILESS_MAIN(WebsiteSourceTest)

#include "website_source_test.moc"
