// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "command_line_lookup.h"
#include "goldendict/core/dictionary_service.h"

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

}  // namespace

QTEST_APPLESS_MAIN(CommandLineLookupTest)

#include "command_line_lookup_test.moc"
