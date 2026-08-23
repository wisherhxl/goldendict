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
void CommandLineLookupTest::ForwardsOnlyAfterPublication() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString endpoint = directory.filePath(QStringLiteral("instance"));
    goldendict::app::LinuxSingleInstanceLookup primary(endpoint);
    QCOMPARE(primary.Start({}),
             goldendict::app::SingleInstanceStartResult::kPrimary);

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
        [&received](QString lookup) { received.push_back(std::move(lookup)); });
    QCOMPARE(received, QStringList({QStringLiteral("two words"),
                                    QStringLiteral("second")}));

    QProcess no_op;
    no_op.start(QCoreApplication::applicationFilePath(),
                {QStringLiteral("--forward-helper"), endpoint});
    QVERIFY(no_op.waitForFinished());
    QCOMPARE(no_op.exitCode(), 0);
    QCOMPARE(received.size(), 2);
}

void CommandLineLookupTest::RejectsInvalidFrames() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString endpoint = directory.filePath(QStringLiteral("instance"));
    goldendict::app::LinuxSingleInstanceLookup primary(endpoint);
    QCOMPARE(primary.Start({}),
             goldendict::app::SingleInstanceStartResult::kPrimary);

    QLocalSocket socket;
    socket.connectToServer(endpoint);
    QVERIFY(socket.waitForConnected());
    QByteArray frame;
    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint32{0x47444C49U} << quint16{99U} << quint32{4U};
    stream.writeRawData("word", 4);
    QCOMPARE(socket.write(frame), frame.size());
    QVERIFY(socket.waitForBytesWritten());
    QTRY_COMPARE(socket.state(), QLocalSocket::UnconnectedState);
    QVERIFY(socket.readAll().isEmpty());
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
        goldendict::app::LinuxSingleInstanceLookup secondary(arguments[2]);
        const auto result =
            secondary.Start(lookup ? lookup->TakeWord() : QString{});
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
