// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QSemaphore>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <string>
#include <utility>

#include "../src/dict_server_source.h"

namespace goldendict::network {
namespace {

class DictServerThread final : public QThread {
   public:
    void WaitUntilReady() { ready_.acquire(); }

    unsigned short Port() const { return port_; }

   protected:
    void run() override {
        QTcpServer server;
        connect(&server, &QTcpServer::newConnection, &server, [&server]() {
            while (server.hasPendingConnections()) {
                QTcpSocket* socket = server.nextPendingConnection();
                socket->write("220 local fixture ready <id@test>\r\n");
                connect(socket, &QTcpSocket::readyRead, socket,
                        [socket]() { HandleCommands(socket); });
            }
        });
        if (server.listen(QHostAddress::LocalHost, 0)) {
            port_ = static_cast<unsigned short>(server.serverPort());
        }
        ready_.release();
        if (port_ != 0U) {
            exec();
        }
    }

   private:
    static void HandleCommands(QTcpSocket* socket) {
        QByteArray buffered = socket->property("commands").toByteArray();
        buffered += socket->readAll();
        while (true) {
            const qsizetype end = buffered.indexOf("\r\n");
            if (end < 0) {
                break;
            }
            const QByteArray command = buffered.left(end);
            buffered.remove(0, end + 2);
            QByteArray response;
            if (command == "CLIENT GoldenDict") {
                response = "250 client accepted\r\n";
            } else if (command == "OPTION MIME") {
                response = "250 MIME enabled\r\n";
            } else if (command == "MATCH * prefix \"A\\\"B\"") {
                response =
                    "152 3 matches found\r\n"
                    "db \"Alpha\"\r\n"
                    "db \"Alpha\"\r\n"
                    "db \"A\\\"B\"\r\n"
                    ".\r\n"
                    "250 ok\r\n";
            } else if (command == "DEFINE * \"Alpha\"") {
                response =
                    "150 1 definitions retrieved\r\n"
                    "151 \"Alpha\" db \"Fixture Dictionary\"\r\n"
                    "Content-Type: text/plain; charset=utf-8\r\n"
                    "\r\n"
                    "First line\r\n"
                    "..dot-stuffed\r\n"
                    ".\r\n"
                    "250 ok\r\n";
            } else if (command == "MATCH * prefix \"none\"") {
                response = "552 no match\r\n";
            } else if (command == "MATCH * prefix \"slow\"") {
                continue;
            } else if (command == "DEFINE * \"broken\"") {
                response =
                    "150 1 definitions retrieved\r\n"
                    "151 \"broken\" db \"Fixture\"\r\n"
                    "not-a-header\r\n";
            } else if (command == "QUIT") {
                response = "221 bye\r\n";
            } else {
                response = "500 unknown command\r\n";
            }
            socket->write(response);
        }
        socket->setProperty("commands", buffered);
    }

    QSemaphore ready_;
    unsigned short port_ = 0U;
};

class DictServerFixture final {
   public:
    DictServerFixture() {
        thread_.start();
        thread_.WaitUntilReady();
        if (thread_.Port() == 0U) {
            qFatal("Could not start local DICT fixture");
        }
    }

    ~DictServerFixture() {
        thread_.quit();
        thread_.wait();
    }

    unsigned short Port() const { return thread_.Port(); }

   private:
    DictServerThread thread_;
};

template <typename Callback>
void VerifyError(DictServerErrorCode expected, Callback&& callback) {
    try {
        callback();
        QFAIL("Expected DICT server error");
    } catch (const DictServerError& error) {
        QCOMPARE(error.code(), expected);
    }
}

DictServerOptions FixtureOptions(const DictServerFixture& fixture) {
    DictServerOptions options;
    options.host = "127.0.0.1";
    options.port = fixture.Port();
    options.timeout = std::chrono::seconds(2);
    return options;
}

}  // namespace

class DictServerSourceTest : public QObject {
    Q_OBJECT

   private slots:
    void FetchesSuggestionsAndDefinitions();
    void HandlesNoMatchesAndRejectsInvalidInputs();
    void RejectsMalformedResponsesAndCancellation();
};

void DictServerSourceTest::FetchesSuggestionsAndDefinitions() {
    DictServerFixture fixture;
    const DictServerSource source(FixtureOptions(fixture));

    QCOMPARE(source.Suggest("A\"B", 2U),
             std::vector<std::string>({"Alpha", "A\"B"}));

    const std::vector<DictServerArticle> articles = source.Define("Alpha", 1U);
    QCOMPARE(articles.size(), 1U);
    QCOMPARE(articles.front().headword, std::string("Alpha"));
    QCOMPARE(articles.front().database, std::string("db"));
    QCOMPARE(articles.front().database_name, std::string("Fixture Dictionary"));
    QCOMPARE(articles.front().content_type,
             std::string("text/plain; charset=utf-8"));
    QCOMPARE(articles.front().body, std::string("First line\n.dot-stuffed"));
}

void DictServerSourceTest::HandlesNoMatchesAndRejectsInvalidInputs() {
    DictServerFixture fixture;
    const DictServerSource source(FixtureOptions(fixture));
    QVERIFY(source.Suggest("none", 1U).empty());

    DictServerOptions invalid;
    invalid.host = "";
    VerifyError(DictServerErrorCode::kInvalidConfiguration,
                [&]() { static_cast<void>(DictServerSource(invalid)); });
    VerifyError(DictServerErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Suggest("", 1U)); });
    VerifyError(DictServerErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Define("word", 61U)); });
    VerifyError(DictServerErrorCode::kInvalidRequest, [&]() {
        static_cast<void>(source.Suggest("line\rbreak", 1U));
    });
}

void DictServerSourceTest::RejectsMalformedResponsesAndCancellation() {
    DictServerFixture fixture;
    const DictServerSource source(FixtureOptions(fixture));
    VerifyError(DictServerErrorCode::kInvalidResponse,
                [&]() { static_cast<void>(source.Define("broken", 1U)); });
    VerifyError(DictServerErrorCode::kCancelled, [&]() {
        static_cast<void>(source.Suggest("Alpha", 1U, []() { return true; }));
    });

    DictServerOptions short_deadline = FixtureOptions(fixture);
    short_deadline.timeout = std::chrono::milliseconds(100);
    const DictServerSource impatient(std::move(short_deadline));
    VerifyError(DictServerErrorCode::kDeadlineExceeded,
                [&]() { static_cast<void>(impatient.Suggest("slow", 1U)); });

    DictServerOptions tiny_response = FixtureOptions(fixture);
    tiny_response.maximum_response_bytes = 10U;
    const DictServerSource bounded(std::move(tiny_response));
    VerifyError(DictServerErrorCode::kResponseTooLarge,
                [&]() { static_cast<void>(bounded.Suggest("Alpha", 1U)); });
}

}  // namespace goldendict::network

using goldendict::network::DictServerSourceTest;

QTEST_GUILESS_MAIN(DictServerSourceTest)

#include "dict_server_source_test.moc"
