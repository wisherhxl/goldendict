// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

#include <string>

#include "../src/mediawiki_source.h"

namespace goldendict::network {
namespace {

class MediaWikiFixture final : public QObject {
   public:
    MediaWikiFixture() {
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
                    const QUrlQuery query(QUrl::fromEncoded(target));
                    QByteArray body;
                    if (query.queryItemValue("action") == "query" &&
                        query.queryItemValue("list") == "allpages" &&
                        query.queryItemValue("apfrom") ==
                            QString::fromUtf8("A&B")) {
                        body =
                            R"({"query":{"allpages":[{"title":"Alpha"},{"title":"Alpha"},{"title":"A&B"}]}})";
                    } else if (query.queryItemValue("action") == "parse" &&
                               query.queryItemValue("page") == "Alpha") {
                        body =
                            R"({"parse":{"title":"Alpha","text":"<p>Article</p>"}})";
                    } else if (query.queryItemValue("page") == "Broken") {
                        body = R"({"parse":{"title":"Broken"}})";
                    } else {
                        body = R"({"error":{"code":"badrequest"}})";
                    }
                    const QByteArray response =
                        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                        "Content-Length: " +
                        QByteArray::number(body.size()) +
                        "\r\nConnection: close\r\n\r\n" + body;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            qFatal("Could not start local MediaWiki fixture");
        }
    }

    std::string BaseUrl() const {
        return "http://127.0.0.1:" +
               std::to_string(static_cast<unsigned int>(server_.serverPort())) +
               "/wiki";
    }

   private:
    QTcpServer server_;
};

template <typename Callback>
void VerifyError(HttpErrorCode expected, Callback&& callback) {
    try {
        callback();
        QFAIL("Expected MediaWiki error");
    } catch (const HttpError& error) {
        QCOMPARE(error.code(), expected);
    }
}

}  // namespace

class MediaWikiSourceTest : public QObject {
    Q_OBJECT

   private slots:
    void FetchesSuggestionsAndArticles();
    void RejectsInvalidConfigurationQueriesAndResponses();
};

void MediaWikiSourceTest::FetchesSuggestionsAndArticles() {
    MediaWikiFixture fixture;
    const MediaWikiSource source(fixture.BaseUrl());

    const std::vector<std::string> suggestions = source.Suggest("A&B", 2U);
    QCOMPARE(suggestions,
             std::vector<std::string>({std::string("Alpha"), "A&B"}));

    const MediaWikiArticle article = source.FetchArticle("Alpha");
    QCOMPARE(article.title, std::string("Alpha"));
    QCOMPARE(article.html, std::string("<p>Article</p>"));
}

void MediaWikiSourceTest::RejectsInvalidConfigurationQueriesAndResponses() {
    VerifyError(HttpErrorCode::kInvalidUrl, []() {
        static_cast<void>(MediaWikiSource("file:///tmp/wiki"));
    });
    HttpRequest nonempty_transport;
    nonempty_transport.url = "http://example.test";
    VerifyError(HttpErrorCode::kInvalidRequest, [&]() {
        static_cast<void>(
            MediaWikiSource("https://example.test", nonempty_transport));
    });

    MediaWikiFixture fixture;
    const MediaWikiSource source(fixture.BaseUrl());
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Suggest("", 1U)); });
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Suggest("A", 51U)); });
    const std::string invalid_utf8(1U, static_cast<char>(0xFF));
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Suggest(invalid_utf8, 1U)); });
    VerifyError(HttpErrorCode::kInvalidResponse,
                [&]() { static_cast<void>(source.FetchArticle("Broken")); });
}

}  // namespace goldendict::network

using goldendict::network::MediaWikiSourceTest;

QTEST_GUILESS_MAIN(MediaWikiSourceTest)

#include "mediawiki_source_test.moc"
