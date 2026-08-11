// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "../src/foundation/utf8.h"

namespace goldendict::core::foundation {

class Utf8Test : public QObject {
    Q_OBJECT

   private slots:
    void AcceptsWellFormedText();
    void RejectsMalformedText_data();
    void RejectsMalformedText();
};

void Utf8Test::AcceptsWellFormedText() {
    QVERIFY(IsValidUtf8(""));
    QVERIFY(IsValidUtf8("plain ASCII"));
    QVERIFY(IsValidUtf8("caf\xc3\xa9"));
    QVERIFY(IsValidUtf8("\xe8\xaf\x8d\xe5\x85\xb8"));
    QVERIFY(IsValidUtf8("\xf0\x9f\xa7\xad"));
}

void Utf8Test::RejectsMalformedText_data() {
    QTest::addColumn<QByteArray>("text");

    QTest::newRow("stray-continuation") << QByteArray("\x80", 1);
    QTest::newRow("truncated-two-byte") << QByteArray("\xc3", 1);
    QTest::newRow("invalid-continuation") << QByteArray("\xc3\x28", 2);
    QTest::newRow("overlong-two-byte") << QByteArray("\xc0\x80", 2);
    QTest::newRow("overlong-three-byte") << QByteArray("\xe0\x80\x80", 3);
    QTest::newRow("surrogate") << QByteArray("\xed\xa0\x80", 3);
    QTest::newRow("above-unicode-range") << QByteArray("\xf4\x90\x80\x80", 4);
    QTest::newRow("five-byte-prefix") << QByteArray("\xf8\x88\x80\x80\x80", 5);
}

void Utf8Test::RejectsMalformedText() {
    QFETCH(QByteArray, text);

    QVERIFY(!IsValidUtf8(std::string_view(
        text.constData(), static_cast<std::size_t>(text.size()))));
}

}  // namespace goldendict::core::foundation

using goldendict::core::foundation::Utf8Test;

QTEST_APPLESS_MAIN(Utf8Test)

#include "utf8_test.moc"
