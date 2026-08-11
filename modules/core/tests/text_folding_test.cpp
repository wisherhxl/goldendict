// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "../src/foundation/text_folding.h"

namespace goldendict::core::foundation {

class TextFoldingTest : public QObject {
    Q_OBJECT

   private slots:
    void FoldsLookupEquivalentText_data();
    void FoldsLookupEquivalentText();
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

void TextFoldingTest::FoldsLookupEquivalentText() {
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(QString::fromStdString(FoldForLookup(input.toStdString())),
             expected);
}

void TextFoldingTest::RejectsMalformedUtf8() {
    QVERIFY_EXCEPTION_THROWN(FoldForLookup(std::string("\xc3\x28", 2)),
                             TextFoldingError);
}

}  // namespace goldendict::core::foundation

using goldendict::core::foundation::TextFoldingTest;

QTEST_APPLESS_MAIN(TextFoldingTest)

#include "text_folding_test.moc"
