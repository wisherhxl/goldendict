// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/lsa/lsa_reader.h"
#include <QtTest>
#include "support/lsa_fixture.h"

namespace goldendict::core::formats::lsa {
class LsaReaderTest final : public QObject {
    Q_OBJECT
   private slots:
    void ReadsIndexAndDecodesBoundedWavResources();
    void RejectsCorruptSampleRanges();
};

void LsaReaderTest::ReadsIndexAndDecodesBoundedWavResources() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = test::WriteLsaFixture(
        std::filesystem::path(temporary.path().toStdString()));
    const auto reader = Reader::Open(path);
    QCOMPARE(reader.metadata().name, std::string("fixture"));
    QCOMPARE(reader.LookupExact("EXAMPLE").front().headword,
             std::string("example"));
    QCOMPARE(reader.SuggestPrefix("s").front(), std::string("second"));
    const auto wav = reader.Resource("example.wav");
    QCOMPARE(wav.substr(0U, 4U), std::string("RIFF"));
    QCOMPARE(wav.substr(8U, 4U), std::string("WAVE"));
    QCOMPARE(wav.size(), 44U + 16U * 2U);
    QVERIFY(reader.Resource("missing.wav").empty());
}

void LsaReaderTest::RejectsCorruptSampleRanges() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = test::WriteLsaFixture(
        std::filesystem::path(temporary.path().toStdString()));
    QFile file(QString::fromStdString(path.string()));
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.seek(70));
    const char invalid[] = {static_cast<char>(0xff), static_cast<char>(0xff),
                            static_cast<char>(0xff), static_cast<char>(0x7f)};
    QCOMPARE(file.write(invalid, 4), 4);
    file.close();
    QVERIFY_EXCEPTION_THROWN(Reader::Open(path), Error);
}
}  // namespace goldendict::core::formats::lsa

using goldendict::core::formats::lsa::LsaReaderTest;
QTEST_MAIN(LsaReaderTest)
#include "lsa_reader_test.moc"
