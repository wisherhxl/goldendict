// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#if defined(Q_OS_LINUX)
#include <QDataStream>
#include <QLocalSocket>
#include <QProcess>
#include <QTemporaryDir>
#endif

#include "command_line_lookup.h"
#include "goldendict/core/dictionary_service.h"
#if defined(Q_OS_LINUX)
#include "linux_display_platform.h"
#include "linux_single_instance_lookup.h"
#endif

namespace {

class CommandLineLookupTest : public QObject {
    Q_OBJECT

   private slots:
    void AcceptsPlainOperand();
    void NormalizesLegacyUris_data();
    void NormalizesLegacyUris();
    void RejectsAmbiguousOrUnsupportedInput_data();
    void RejectsAmbiguousOrUnsupportedInput();
    void EnforcesBoundsAndOneShotConsumption();
#if defined(Q_OS_LINUX)
    void ConfiguresLinuxDisplayPlatform_data();
    void ConfiguresLinuxDisplayPlatform();
    void ForwardsOnlyAfterPublication();
    void RejectsInvalidFrames();
#endif
};

void CommandLineLookupTest::AcceptsPlainOperand() {
    auto request = goldendict::app::ParseInitialLookup(
        {QStringLiteral("goldendict"), QStringLiteral("  two words  ")});
    QVERIFY(request.has_value());
    QCOMPARE(request->TakeWord(), QStringLiteral("two words"));

    auto unicode = goldendict::app::ParseInitialLookup(
        {QStringLiteral("goldendict"), QString::fromUtf8("词典")});
    QVERIFY(unicode.has_value());
    QCOMPARE(unicode->TakeWord(), QString::fromUtf8("词典"));
}

void CommandLineLookupTest::NormalizesLegacyUris_data() {
    QTest::addColumn<QString>("operand");
    QTest::addColumn<QString>("expected");

    QTest::newRow("goldendict")
        << QStringLiteral("goldendict://word") << QStringLiteral("word");
    QTest::newRow("goldendict-extra-slash")
        << QStringLiteral("goldendict:///two%20words/")
        << QStringLiteral("two words");
    QTest::newRow("dict") << QStringLiteral("dict://caf%C3%A9")
                          << QString::fromUtf8("caf\xC3\xA9");
    QTest::newRow("dict-extra-slash")
        << QStringLiteral("dict:///word") << QStringLiteral("word");
    QTest::newRow("single-slash-payload")
        << QStringLiteral("dict:////") << QStringLiteral("/");
    QTest::newRow("encoded-slash")
        << QStringLiteral("dict://%2F") << QStringLiteral("/");
}

void CommandLineLookupTest::NormalizesLegacyUris() {
    QFETCH(QString, operand);
    QFETCH(QString, expected);
    auto request = goldendict::app::ParseInitialLookup(
        {QStringLiteral("goldendict"), operand});
    QVERIFY(request.has_value());
    QCOMPARE(request->TakeWord(), expected);
}

void CommandLineLookupTest::RejectsAmbiguousOrUnsupportedInput_data() {
    QTest::addColumn<QStringList>("arguments");

    QTest::newRow("no-operand") << QStringList{QStringLiteral("goldendict")};
    QTest::newRow("empty") << QStringList{QStringLiteral("goldendict"),
                                          QString{}};
    QTest::newRow("whitespace")
        << QStringList{QStringLiteral("goldendict"), QStringLiteral("  ")};
    QTest::newRow("option")
        << QStringList{QStringLiteral("goldendict"), QStringLiteral("--help")};
    QTest::newRow("option-and-operand")
        << QStringList{QStringLiteral("goldendict"), QStringLiteral("--smoke"),
                       QStringLiteral("word")};
    QTest::newRow("multiple-operands")
        << QStringList{QStringLiteral("goldendict"), QStringLiteral("one"),
                       QStringLiteral("two")};
    QTest::newRow("unsupported-scheme") << QStringList{
        QStringLiteral("goldendict"), QStringLiteral("https://example.test")};
    QTest::newRow("case-sensitive-scheme") << QStringList{
        QStringLiteral("goldendict"), QStringLiteral("Dict://word")};
    QTest::newRow("empty-uri")
        << QStringList{QStringLiteral("goldendict"), QStringLiteral("dict://")};
    QTest::newRow("malformed-percent-short") << QStringList{
        QStringLiteral("goldendict"), QStringLiteral("dict://word%")};
    QTest::newRow("malformed-percent-digit") << QStringList{
        QStringLiteral("goldendict"), QStringLiteral("dict://word%XZ")};
    QTest::newRow("invalid-utf8") << QStringList{QStringLiteral("goldendict"),
                                                 QStringLiteral("dict://%FF")};
    QTest::newRow("encoded-nul") << QStringList{
        QStringLiteral("goldendict"), QStringLiteral("dict://word%00tail")};
}

void CommandLineLookupTest::RejectsAmbiguousOrUnsupportedInput() {
    QFETCH(QStringList, arguments);
    QVERIFY(!goldendict::app::ParseInitialLookup(arguments).has_value());
}

void CommandLineLookupTest::EnforcesBoundsAndOneShotConsumption() {
    const qsizetype maximum =
        static_cast<qsizetype>(goldendict::core::kMaximumLookupTextBytes);
    auto bounded = goldendict::app::ParseInitialLookup(
        {QStringLiteral("goldendict"), QString(maximum, QLatin1Char('x'))});
    QVERIFY(bounded.has_value());
    QCOMPARE(bounded->TakeWord().size(), maximum);
    QVERIFY(bounded->TakeWord().isEmpty());

    QVERIFY(!goldendict::app::ParseInitialLookup(
                 {QStringLiteral("goldendict"),
                  QString(maximum + 1, QLatin1Char('x'))})
                 .has_value());
    QVERIFY(
        !goldendict::app::ParseInitialLookup(
             {QStringLiteral("goldendict"),
              QStringLiteral("dict://") + QString(maximum, QLatin1Char('x'))})
             .has_value());
}

#if defined(Q_OS_LINUX)
void CommandLineLookupTest::ConfiguresLinuxDisplayPlatform_data() {
    QTest::addColumn<bool>("session_present");
    QTest::addColumn<QByteArray>("session");
    QTest::addColumn<bool>("platform_present");
    QTest::addColumn<QByteArray>("platform");
    QTest::addColumn<bool>("expected_present");
    QTest::addColumn<QByteArray>("expected");

    QTest::newRow("wayland") << true << QByteArray("wayland") << false
                             << QByteArray() << true << QByteArray("xcb");
    QTest::newRow("mixed-case-wayland")
        << true << QByteArray("WayLand") << false << QByteArray() << true
        << QByteArray("xcb");
    QTest::newRow("wayland-replaces-platform")
        << true << QByteArray("wayland") << true << QByteArray("offscreen")
        << true << QByteArray("xcb");
    QTest::newRow("x11-preserves-platform")
        << true << QByteArray("x11") << true << QByteArray("offscreen") << true
        << QByteArray("offscreen");
    QTest::newRow("empty-preserves-platform")
        << true << QByteArray() << true << QByteArray("minimal") << true
        << QByteArray("minimal");
    QTest::newRow("absent-preserves-absence")
        << false << QByteArray() << false << QByteArray() << false
        << QByteArray();
    QTest::newRow("unknown-preserves-absence")
        << true << QByteArray("mir") << false << QByteArray() << false
        << QByteArray();
}

void CommandLineLookupTest::ConfiguresLinuxDisplayPlatform() {
    QFETCH(bool, session_present);
    QFETCH(QByteArray, session);
    QFETCH(bool, platform_present);
    QFETCH(QByteArray, platform);
    QFETCH(bool, expected_present);
    QFETCH(QByteArray, expected);

    const bool saved_session_present =
        qEnvironmentVariableIsSet("XDG_SESSION_TYPE");
    const QByteArray saved_session = qgetenv("XDG_SESSION_TYPE");
    const bool saved_platform_present =
        qEnvironmentVariableIsSet("QT_QPA_PLATFORM");
    const QByteArray saved_platform = qgetenv("QT_QPA_PLATFORM");

    session_present ? qputenv("XDG_SESSION_TYPE", session)
                    : qunsetenv("XDG_SESSION_TYPE");
    platform_present ? qputenv("QT_QPA_PLATFORM", platform)
                     : qunsetenv("QT_QPA_PLATFORM");
    goldendict::app::ConfigureLinuxDisplayPlatform();
    const bool actual_present = qEnvironmentVariableIsSet("QT_QPA_PLATFORM");
    const QByteArray actual = qgetenv("QT_QPA_PLATFORM");

    saved_session_present ? qputenv("XDG_SESSION_TYPE", saved_session)
                          : qunsetenv("XDG_SESSION_TYPE");
    saved_platform_present ? qputenv("QT_QPA_PLATFORM", saved_platform)
                           : qunsetenv("QT_QPA_PLATFORM");

    QCOMPARE(actual_present, expected_present);
    QCOMPARE(actual, expected);
}

void CommandLineLookupTest::ForwardsOnlyAfterPublication() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString endpoint = directory.filePath(QStringLiteral("instance"));
    goldendict::app::LinuxSingleInstanceLookup primary(endpoint);
    QCOMPARE(primary.Start(std::nullopt),
             goldendict::app::SingleInstanceStartResult::kPrimary);

    QProcess activation;
    activation.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--forward-helper"), endpoint});
    QVERIFY(activation.waitForStarted());
    QTRY_COMPARE_WITH_TIMEOUT(activation.state(), QProcess::NotRunning, 12000);
    QCOMPARE(activation.exitCode(), 0);

    QProcess secondary;
    secondary.start(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("--forward-helper"), endpoint,
                     QStringLiteral("dict://two%20words")});
    QVERIFY(secondary.waitForStarted());
    QTRY_COMPARE_WITH_TIMEOUT(secondary.state(), QProcess::NotRunning, 12000);
    QCOMPARE(secondary.exitCode(), 0);

    QProcess second;
    second.start(QCoreApplication::applicationFilePath(),
                 {QStringLiteral("--forward-helper"), endpoint,
                  QStringLiteral("second")});
    QVERIFY(second.waitForStarted());
    QTRY_COMPARE_WITH_TIMEOUT(second.state(), QProcess::NotRunning, 12000);
    QCOMPARE(second.exitCode(), 0);

    QStringList received;
    primary.PublishConsumer(
        [&received](goldendict::app::SingleInstanceMessage message) {
            if (message.Kind() ==
                goldendict::app::SingleInstanceMessageKind::kActivation) {
                received.push_back(QStringLiteral("activation"));
            } else {
                received.push_back(QStringLiteral("lookup:") +
                                   message.LookupText());
            }
        });
    QCOMPARE(received, QStringList({QStringLiteral("activation"),
                                    QStringLiteral("lookup:two words"),
                                    QStringLiteral("lookup:second")}));

    QProcess no_op;
    no_op.start(QCoreApplication::applicationFilePath(),
                {QStringLiteral("--forward-helper"), endpoint,
                 QStringLiteral("--invalid")});
    QVERIFY(no_op.waitForFinished());
    QCOMPARE(no_op.exitCode(), 0);
    QCOMPARE(received.size(), 3);
}

void CommandLineLookupTest::RejectsInvalidFrames() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString endpoint = directory.filePath(QStringLiteral("instance"));
    goldendict::app::LinuxSingleInstanceLookup primary(endpoint);
    QCOMPARE(primary.Start(std::nullopt),
             goldendict::app::SingleInstanceStartResult::kPrimary);

    const auto reject_frame = [&endpoint](quint32 magic, quint16 version,
                                          const QByteArray& payload) {
        QLocalSocket socket;
        socket.connectToServer(endpoint);
        if (!socket.waitForConnected()) {
            return false;
        }
        QByteArray frame;
        QDataStream stream(&frame, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        stream << magic << version << static_cast<quint32>(payload.size());
        stream.writeRawData(payload.constData(), payload.size());
        if (socket.write(frame) != frame.size() ||
            !socket.waitForBytesWritten()) {
            return false;
        }
        for (int attempt = 0;
             attempt < 1000 && socket.state() != QLocalSocket::UnconnectedState;
             ++attempt) {
            QCoreApplication::processEvents();
        }
        return socket.state() == QLocalSocket::UnconnectedState &&
               socket.readAll().isEmpty();
    };

    QVERIFY(reject_frame(0x47444C49U, 99U, QByteArray("word")));
    QVERIFY(reject_frame(0x47444149U, 1U, QByteArray("word")));
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
#if defined(Q_OS_LINUX)
    const QStringList arguments = app.arguments();
    if (arguments.size() >= 3 &&
        arguments[1] == QStringLiteral("--forward-helper")) {
        auto lookup = arguments.size() == 4
                          ? goldendict::app::ParseInitialLookup(
                                {arguments[0], arguments[3]})
                          : std::nullopt;
        std::optional<goldendict::app::SingleInstanceMessage> message;
        if (lookup) {
            message = goldendict::app::SingleInstanceMessage::Lookup(
                lookup->TakeWord());
        } else if (arguments.size() == 3) {
            message = goldendict::app::SingleInstanceMessage::Activation();
        }
        goldendict::app::LinuxSingleInstanceLookup secondary(arguments[2]);
        const auto result = secondary.Start(message);
        return result == goldendict::app::SingleInstanceStartResult::
                               kForwarded ||
                       result == goldendict::app::SingleInstanceStartResult::
                                     kSecondaryNoOp
                   ? 0
                   : 1;
    }
#endif
    CommandLineLookupTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "command_line_lookup_test.moc"
