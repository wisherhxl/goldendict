// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "../src/foundation/text_encoding.h"

namespace goldendict::core::foundation {

class TextEncodingTest : public QObject {
    Q_OBJECT

   private slots:
    void DecodesRepresentativeLegacyEncodings_data();
    void DecodesRepresentativeLegacyEncodings();
    void RoundTripsRepresentativeLegacyEncodings_data();
    void RoundTripsRepresentativeLegacyEncodings();
    void RejectsMalformedOrUnrepresentableText();
    void EnforcesOutputAndEncodingNameBounds();
};

void TextEncodingTest::DecodesRepresentativeLegacyEncodings_data() {
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QString>("encoding");
    QTest::addColumn<QString>("expected");

    QTest::newRow("latin-1")
        << QByteArray("caf\xe9", 4) << QStringLiteral("ISO-8859-1")
        << QString::fromUtf8("café");
    QTest::newRow("utf-16le")
        << QByteArray::fromHex("575b7851") << QStringLiteral("UTF-16LE")
        << QString::fromUtf8("字典");
    QTest::newRow("gb18030")
        << QByteArray::fromHex("d7d6b5e4") << QStringLiteral("GB18030")
        << QString::fromUtf8("字典");
    QTest::newRow("euc-jp")
        << QByteArray::fromHex("bcadbdf1") << QStringLiteral("EUC-JP")
        << QString::fromUtf8("辞書");
    QTest::newRow("state-only") << QByteArray::fromHex("1b2842")
                                << QStringLiteral("ISO-2022-JP") << QString();
}

void TextEncodingTest::DecodesRepresentativeLegacyEncodings() {
    QFETCH(QByteArray, bytes);
    QFETCH(QString, encoding);
    QFETCH(QString, expected);

    QCOMPARE(QString::fromStdString(
                 DecodeToUtf8(std::string_view(bytes.constData(), bytes.size()),
                              encoding.toStdString(), 64U)),
             expected);
}

void TextEncodingTest::RoundTripsRepresentativeLegacyEncodings_data() {
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("encoding");

    QTest::newRow("latin-1")
        << QString::fromUtf8("café") << QStringLiteral("ISO-8859-1");
    QTest::newRow("gb18030")
        << QString::fromUtf8("字典") << QStringLiteral("GB18030");
    QTest::newRow("euc-jp")
        << QString::fromUtf8("辞書") << QStringLiteral("EUC-JP");
}

void TextEncodingTest::RoundTripsRepresentativeLegacyEncodings() {
    QFETCH(QString, text);
    QFETCH(QString, encoding);

    const std::string encoded =
        EncodeFromUtf8(text.toStdString(), encoding.toStdString(), 64U);
    QCOMPARE(QString::fromStdString(
                 DecodeToUtf8(encoded, encoding.toStdString(), 64U)),
             text);
}

void TextEncodingTest::RejectsMalformedOrUnrepresentableText() {
    QVERIFY_EXCEPTION_THROWN(DecodeToUtf8(std::string("\x81", 1), "UTF-8", 64U),
                             TextEncodingError);
    QVERIFY_EXCEPTION_THROWN(
        EncodeFromUtf8(std::string("\xc3\x28", 2), "ISO-8859-1", 64U),
        TextEncodingError);
    QVERIFY_EXCEPTION_THROWN(
        EncodeFromUtf8("dictionary \xe8\xaf\x8d\xe5\x85\xb8", "ISO-8859-1",
                       64U),
        TextEncodingError);
    QVERIFY_EXCEPTION_THROWN(
        DecodeToUtf8(std::string("\0", 1), "UTF-16LE", 64U), TextEncodingError);
    QVERIFY_EXCEPTION_THROWN(DecodeToUtf8("text", "not-an-encoding", 64U),
                             TextEncodingError);
}

void TextEncodingTest::EnforcesOutputAndEncodingNameBounds() {
    QVERIFY_EXCEPTION_THROWN(DecodeToUtf8("caf\xe9", "ISO-8859-1", 3U),
                             TextEncodingError);
    QVERIFY_EXCEPTION_THROWN(EncodeFromUtf8("café", "ISO-8859-1", 3U),
                             TextEncodingError);
    QVERIFY_EXCEPTION_THROWN(DecodeToUtf8("", "", 0U), TextEncodingError);
    QVERIFY_EXCEPTION_THROWN(DecodeToUtf8("", std::string(129U, 'x'), 0U),
                             TextEncodingError);
    QCOMPARE(EncodeFromUtf8("caf\xc3\xa9", "ISO-8859-1", 4U),
             std::string("caf\xe9", 4));
}

}  // namespace goldendict::core::foundation

using goldendict::core::foundation::TextEncodingTest;

QTEST_APPLESS_MAIN(TextEncodingTest)

#include "text_encoding_test.moc"
