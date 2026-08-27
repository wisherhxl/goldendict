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
    void TransliterateGreekCases_data();
    void TransliterateGreekCases();
    void BelarusianMappingCountsAndDuplicates();
    void TransliterateBelarusianCases_data();
    void TransliterateBelarusianCases();
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

void TransliterationTest::TransliterateGreekCases_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("modern-longest-match")
        << QStringLiteral("psuxh") << QString::fromUtf8("ψυχη");
    QTest::newRow("modern-diacritics")
        << QStringLiteral("'a\"'i") << QString::fromUtf8("άΐ");
    QTest::newRow("classical-beta-code")
        << QStringLiteral("*)/A a(/ w=|") << QString::fromUtf8("Ἄ ἅ ῷ");
    QTest::newRow("unofficial-order")
        << QStringLiteral("A/)| u/(") << QString::fromUtf8("ᾌ ὕ");
    QTest::newRow("tonos-and-oxia")
        << QString::fromUtf8("ά ά") << QString::fromUtf8("ά ά");
    QTest::newRow("pinned-uppercase-rho")
        << QStringLiteral("R") << QString::fromUtf8("Τ");
    QTest::newRow("unicode-passthrough")
        << QString::fromUtf8("猫 ks") << QString::fromUtf8("猫 ξ");
}

void TransliterationTest::TransliterateGreekCases() {
    QFETCH(QString, input);
    QFETCH(QString, expected);
    const auto actual = TransliterateGreek(input.toStdString());
    QVERIFY(actual.has_value());
    QCOMPARE(QString::fromStdString(*actual), expected);
}

void TransliterationTest::BelarusianMappingCountsAndDuplicates() {
    const auto classic = BelarusianLatinClassicMappingCounts();
    QCOMPARE(classic.declarations, 334U);
    QCOMPARE(classic.effective, 333U);
    const auto school = BelarusianLatinSchoolMappingCounts();
    QCOMPARE(school.declarations, 535U);
    QCOMPARE(school.effective, 534U);
    const auto smoothing = BelarusianSchoolClassicMappingCounts();
    QCOMPARE(smoothing.declarations, 446U);
    QCOMPARE(smoothing.effective, 436U);

    QCOMPARE(*TransliterateBelarusianLatinClassic("cia"), std::string("ця"));
    QCOMPARE(*TransliterateBelarusianLatinSchool("cia"), std::string("ця"));
    QCOMPARE(*TransliterateBelarusianSchoolClassic("зье"), std::string("з'е"));
}

void TransliterationTest::TransliterateBelarusianCases_data() {
    QTest::addColumn<int>("variant");
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("classic-latin-to-cyrillic-longest-uppercase")
        << 0 << QString::fromUtf8("BIEŁARUŚ") << QString::fromUtf8("беларусь");
    QTest::newRow("classic-cyrillic-to-latin-uppercase")
        << 0 << QString::fromUtf8("БЕЛАРУСЬ") << QString::fromUtf8("biełaruś");
    QTest::newRow("classic-pinned-typo")
        << 0 << QStringLiteral("LIA") << QString::fromUtf8("ліa");
    QTest::newRow("classic-ascii-apostrophe")
        << 0 << QStringLiteral("BJA") << QString::fromUtf8("б'я");
    QTest::newRow("classic-modifier-apostrophe")
        << 0 << QString::fromUtf8("БʼЯ") << QStringLiteral("bja");
    QTest::newRow("classic-curly-apostrophe")
        << 0 << QString::fromUtf8("Б’Я") << QStringLiteral("bja");
    QTest::newRow("school-latin-longest-uppercase")
        << 1 << QString::fromUtf8("ŚCIA") << QString::fromUtf8("сця");
    QTest::newRow("school-cyrillic-to-latin")
        << 1 << QString::fromUtf8("СЦЯ") << QStringLiteral("scia");
    QTest::newRow("smoothing-school-to-classic")
        << 2 << QString::fromUtf8("СЦЯ") << QString::fromUtf8("сьця");
    QTest::newRow("smoothing-classic-to-school")
        << 2 << QString::fromUtf8("СЬЦЯ") << QString::fromUtf8("сця");
    QTest::newRow("smoothing-apostrophe-to-soft-sign")
        << 2 << QString::fromUtf8("З’Я") << QString::fromUtf8("зья");
    QTest::newRow("unicode-passthrough")
        << 2 << QString::fromUtf8("猫 СЦЯ") << QString::fromUtf8("猫 сьця");
}

void TransliterationTest::TransliterateBelarusianCases() {
    QFETCH(int, variant);
    QFETCH(QString, input);
    QFETCH(QString, expected);
    std::optional<std::string> actual;
    if (variant == 0) {
        actual = TransliterateBelarusianLatinClassic(input.toStdString());
    } else if (variant == 1) {
        actual = TransliterateBelarusianLatinSchool(input.toStdString());
    } else {
        actual = TransliterateBelarusianSchoolClassic(input.toStdString());
    }
    QVERIFY(actual.has_value());
    QCOMPARE(QString::fromStdString(*actual), expected);
}

void TransliterationTest::ReturnsNoAlternateForUnchangedText() {
    QVERIFY(!TransliterateRussian("").has_value());
    QVERIFY(!TransliterateRussian("猫").has_value());
    QVERIFY(!TransliterateGerman("").has_value());
    QVERIFY(!TransliterateGerman("already umlauted: ä").has_value());
    QVERIFY(!TransliterateGreek("").has_value());
    QVERIFY(!TransliterateGreek("猫").has_value());
    QVERIFY(!TransliterateBelarusianLatinClassic("").has_value());
    QVERIFY(!TransliterateBelarusianLatinSchool("猫").has_value());
    QVERIFY(!TransliterateBelarusianSchoolClassic("猫").has_value());
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
    QVERIFY_EXCEPTION_THROWN(TransliterateGreek(std::string("bad\0input", 9)),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateGreek(std::string("\xc3\x28", 2)),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateGreek(std::string(
                                 kMaximumTransliterationInputBytes + 1U, 'a')),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateGreek("a", 1U), TransliterationError);
    QVERIFY_EXCEPTION_THROWN(
        TransliterateBelarusianLatinClassic(std::string("bad\0input", 9)),
        TransliterationError);
    QVERIFY_EXCEPTION_THROWN(
        TransliterateBelarusianLatinSchool(std::string("\xc3\x28", 2)),
        TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateBelarusianSchoolClassic(std::string(
                                 kMaximumTransliterationInputBytes + 1U, 'a')),
                             TransliterationError);
    QVERIFY_EXCEPTION_THROWN(TransliterateBelarusianLatinClassic("a", 1U),
                             TransliterationError);
}

}  // namespace goldendict::core::dictionary

using goldendict::core::dictionary::TransliterationTest;

QTEST_APPLESS_MAIN(TransliterationTest)

#include "transliteration_test.moc"
