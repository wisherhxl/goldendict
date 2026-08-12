// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include <string>

#include "../src/forvo_source.h"

namespace goldendict::network {
namespace {

class ForvoFixture final : public QObject {
   public:
    ForvoFixture() {
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
                        QByteArray content_type = "application/xml";
                        QByteArray body;
                        if (target == "/audio.mp3") {
                            content_type = "audio/mpeg";
                            body = "mp3data";
                        } else if (target.contains("/word/A%26B/") &&
                                   target.contains("/key/test-key/") &&
                                   target.contains("/language/en/")) {
                            body =
                                "<items><item><pathmp3>" +
                                QByteArray::fromStdString(AudioUrl()) +
                                "</pathmp3><username>speaker</username>"
                                "<country>United States</country><sex>F</sex>"
                                "<num_votes>5</num_votes>"
                                "<num_positive_votes>4</num_positive_votes>"
                                "</item></items>";
                        } else if (target.contains("/word/broken/")) {
                            body =
                                "<items><item><pathmp3>file:///tmp/x</pathmp3>"
                                "<num_votes>1</num_votes>"
                                "<num_positive_votes>2</num_positive_votes>"
                                "</item></items>";
                        } else {
                            body = "<items/>";
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
            qFatal("Could not start local Forvo fixture");
        }
    }

    std::string BaseUrl() const {
        return "http://127.0.0.1:" +
               std::to_string(static_cast<unsigned int>(server_.serverPort()));
    }

    std::string AudioUrl() const { return BaseUrl() + "/audio.mp3"; }

   private:
    QTcpServer server_;
};

template <typename Callback>
void VerifyError(HttpErrorCode expected, Callback&& callback) {
    try {
        callback();
        QFAIL("Expected Forvo error");
    } catch (const HttpError& error) {
        QCOMPARE(error.code(), expected);
    }
}

}  // namespace

class ForvoSourceTest : public QObject {
    Q_OBJECT

   private slots:
    void FetchesPronunciationsAndAudio();
    void RejectsInvalidConfigurationQueriesAndResponses();
};

void ForvoSourceTest::FetchesPronunciationsAndAudio() {
    ForvoFixture fixture;
    const ForvoSource source(fixture.BaseUrl(), "test-key", "en");

    const std::vector<ForvoPronunciation> results = source.Lookup("A&B", 5U);
    QCOMPARE(results.size(), 1U);
    QCOMPARE(results.front().audio_url, fixture.AudioUrl());
    QCOMPARE(results.front().username, std::string("speaker"));
    QCOMPARE(results.front().country, std::string("United States"));
    QCOMPARE(results.front().sex, std::string("f"));
    QCOMPARE(results.front().positive_votes, 4);
    QCOMPARE(results.front().negative_votes, 1);

    const ForvoAudio audio = source.FetchAudio(results.front().audio_url);
    QCOMPARE(audio.content_type, std::string("audio/mpeg"));
    QCOMPARE(std::string(audio.bytes.begin(), audio.bytes.end()),
             std::string("mp3data"));
}

void ForvoSourceTest::RejectsInvalidConfigurationQueriesAndResponses() {
    VerifyError(HttpErrorCode::kInvalidUrl, []() {
        static_cast<void>(ForvoSource("file:///tmp/api", "key", "en"));
    });
    VerifyError(HttpErrorCode::kInvalidRequest, []() {
        static_cast<void>(ForvoSource("https://example.test", "", "en"));
    });

    ForvoFixture fixture;
    const ForvoSource source(fixture.BaseUrl(), "test-key", "en");
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Lookup("", 1U)); });
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Lookup("word", 51U)); });
    VerifyError(HttpErrorCode::kInvalidResponse,
                [&]() { static_cast<void>(source.Lookup("broken", 1U)); });
    VerifyError(HttpErrorCode::kInvalidUrl, [&]() {
        static_cast<void>(source.FetchAudio("file:///tmp/audio.mp3"));
    });
}

}  // namespace goldendict::network

using goldendict::network::ForvoSourceTest;

QTEST_GUILESS_MAIN(ForvoSourceTest)

#include "forvo_source_test.moc"
