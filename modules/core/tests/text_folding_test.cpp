// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "../src/foundation/text_folding.h"

namespace goldendict::core::foundation {
namespace {

bool IsHyphen(char32_t code_point) {
    return code_point == U'-';
}

}  // namespace

class TextFoldingTest : public QObject {
    Q_OBJECT

   private slots:
    void FoldsLookupEquivalentText_data();
    void FoldsLookupEquivalentText();
    void UsesInjectedSeparatorPolicy();
    void PreservesFrozenLegacyPrefixRankingFolds();
    void NormalizesExactLookupText();
    void RejectsMalformedUtf8();
};

void TextFoldingTest::FoldsLookupEquivalentText_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("empty") << QString() << QString();
    QTest::newRow("case-diacritic-punctuation")
        << QString::fromUtf8(" Café-au LAIT! ") << QStringLiteral("cafeaulait");
    QTest::newRow("prefix-cafeteria")
        << QStringLiteral("cafeteria") << QStringLiteral("cafeteria");
    QTest::newRow("prefix-cafe-noir")
        << QString::fromUtf8("café noir") << QStringLiteral("cafenoir");
    QTest::newRow("full-case-fold")
        << QString::fromUtf8("Straße") << QStringLiteral("strasse");
    QTest::newRow("compatibility-characters")
        << QString::fromUtf8("Ｆｕｌｌ　Ｗｉｄｔｈ")
        << QStringLiteral("fullwidth");
    QTest::newRow("non-latin")
        << QString::fromUtf8("СЛОВО") << QString::fromUtf8("слово");
    QTest::newRow("cjk") << QString::fromUtf8("词典")
                         << QString::fromUtf8("词典");
}

void TextFoldingTest::NormalizesExactLookupText() {
    QCOMPARE(QString::fromStdString(NormalizeForExactLookup(
                 QString::fromUtf8("CAFÉ-au lait").toStdString(), false)),
             QString::fromUtf8("café-au lait"));
    QCOMPARE(QString::fromStdString(NormalizeForExactLookup(
                 QString::fromUtf8("CAFE\u0301-au lait").toStdString(), true)),
             QStringLiteral("cafe-au lait"));
    QVERIFY(NormalizeForExactLookup("cafe-au lait", false) !=
            NormalizeForExactLookup("cafe au lait", false));
    QVERIFY_EXCEPTION_THROWN(
        NormalizeForExactLookup(std::string("\xc3\x28", 2), true),
        TextFoldingError);
}

void TextFoldingTest::FoldsLookupEquivalentText() {
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(QString::fromStdString(FoldForLookup(input.toStdString())),
             expected);
}

void TextFoldingTest::UsesInjectedSeparatorPolicy() {
    QCOMPARE(FoldForLookupWithSeparatorPolicy("A-B C!", IsHyphen), "ab c!");
    QVERIFY_EXCEPTION_THROWN(
        FoldForLookupWithSeparatorPolicy("word", nullptr), TextFoldingError);
}

void TextFoldingTest::PreservesFrozenLegacyPrefixRankingFolds() {
    const auto legacy_diacritic = FoldForLegacyPrefixRanking("r\xc3\xb8me");
    QCOMPARE(legacy_diacritic.without_diacritics, "rome");

    const auto post_unicode_5_2_upper =
        FoldForLegacyPrefixRanking("\xe1\xb2\x90");
    const auto post_unicode_5_2_lower =
        FoldForLegacyPrefixRanking("\xe1\x83\x90");
    QCOMPARE(post_unicode_5_2_upper.simple_case, "\xe1\xb2\x90");
    QCOMPARE(post_unicode_5_2_lower.simple_case, "\xe1\x83\x90");
    QVERIFY(post_unicode_5_2_upper.simple_case !=
            post_unicode_5_2_lower.simple_case);

    QVERIFY_EXCEPTION_THROWN(
        FoldForLegacyPrefixRanking(std::string("\xc3\x28", 2)),
        TextFoldingError);
}

void TextFoldingTest::RejectsMalformedUtf8() {
    QVERIFY_EXCEPTION_THROWN(FoldForLookup(std::string("\xc3\x28", 2)),
                             TextFoldingError);
}

}  // namespace goldendict::core::foundation

using goldendict::core::foundation::TextFoldingTest;

QTEST_APPLESS_MAIN(TextFoldingTest)

#include "text_folding_test.moc"
