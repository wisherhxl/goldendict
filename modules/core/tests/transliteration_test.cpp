// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "../src/dictionary/transliteration.h"

namespace goldendict::core::dictionary {

class TransliterationTest : public QObject {
    Q_OBJECT

   private slots:
    void TransliterateRussianCases_data();
    void TransliterateRussianCases();
    void TransliterateGermanCases_data();
    void TransliterateGermanCases();
    void ReturnsNoAlternateForUnchangedText();
    void RejectsInvalidInputAndOutputOverflow();
};

void TransliterationTest::TransliterateRussianCases_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("longest-match")
        << QStringLiteral("shchuka") << QString::fromUtf8("щука");
    QTest::newRow("mixed-case")
        << QStringLiteral("YoZh") << QString::fromUtf8("ЁЖ");
    QTest::newRow("hard-soft-and-e")
        << QStringLiteral("ob\"'ekt") << QString::fromUtf8("объэкт");
    QTest::newRow("soft-sign")
        << QStringLiteral("sem'") << QString::fromUtf8("семь");
    QTest::newRow("apostrophe-e")
        << QStringLiteral("'eto") << QString::fromUtf8("это");
    QTest::newRow("unicode-passthrough")
        << QString::fromUtf8("猫 sh") << QString::fromUtf8("猫 ш");
}

void TransliterationTest::TransliterateRussianCases() {
    QFETCH(QString, input);
    QFETCH(QString, expected);
    const auto actual = TransliterateRussian(input.toStdString());
    QVERIFY(actual.has_value());
    QCOMPARE(QString::fromStdString(*actual), expected);
}

void TransliterationTest::TransliterateGermanCases_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("lowercase")
        << QStringLiteral("fuer groesser") << QString::fromUtf8("für größer");
    QTest::newRow("uppercase") << QStringLiteral("UEBER OEFFNUNG")
                               << QString::fromUtf8("ÜBER ÖFFNUNG");
    QTest::newRow("title-case") << QStringLiteral("Aehre Oel Uebermass")
                                << QString::fromUtf8("Ähre Öl Übermaß");
    QTest::newRow("mixed-case-is-unmatched")
        << QStringLiteral("aE oE uE sS") << QStringLiteral("aE oE uE sS");
    QTest::newRow("unicode-passthrough")
        << QString::fromUtf8("猫 und Oel") << QString::fromUtf8("猫 und Öl");
}

void TransliterationTest::TransliterateGermanCases() {
    QFETCH(QString, input);
    QFETCH(QString, expected);
    const auto actual = TransliterateGerman(input.toStdString());
    if (input == expected) {
        QVERIFY(!actual.has_value());
    } else {
        QVERIFY(actual.has_value());
        QCOMPARE(QString::fromStdString(*actual), expected);
    }
}

void TransliterationTest::ReturnsNoAlternateForUnchangedText() {
    QVERIFY(!TransliterateRussian("").has_value());
    QVERIFY(!TransliterateRussian("猫").has_value());
    QVERIFY(!TransliterateGerman("").has_value());
    QVERIFY(!TransliterateGerman("already umlauted: ä").has_value());
}

void TransliterationTest::RejectsInvalidInputAndOutputOverflow() {
    QVERIFY_EXCEPTION_THROWN(TransliterateRussian(std::string("bad\0input", 9)),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateRussian(std::string("\xc3\x28", 2)),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateRussian(std::string(
                                 kMaximumTransliterationInputBytes + 1U, 'a')),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateRussian("shch", 1U),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateGerman(std::string("bad\0input", 9)),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateGerman(std::string("\xc3\x28", 2)),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateGerman(std::string(
                                 kMaximumTransliterationInputBytes + 1U, 'a')),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateGerman("ue", 1U),
                             TransliterationError);
}

}  // namespace goldendict::core::dictionary

using goldendict::core::dictionary::TransliterationTest;

QTEST_APPLESS_MAIN(TransliterationTest)

#include "transliteration_test.moc"
