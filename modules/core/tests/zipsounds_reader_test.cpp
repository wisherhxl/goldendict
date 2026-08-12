// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/zipsounds/zipsounds_reader.h"
#include <QtTest>
#include "support/zipsounds_fixture.h"

namespace goldendict::core::formats::zipsounds {
class ZipSoundsReaderTest final : public QObject {
    Q_OBJECT
   private slots:
    void ReadsStoredAndDeflatedAudioMembers();
    void RejectsCorruptMemberChecksums();
};

void ZipSoundsReaderTest::ReadsStoredAndDeflatedAudioMembers() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = test::WriteZipSoundsFixture(
        std::filesystem::path(temporary.path().toStdString()));
    const auto reader = Reader::Open(path);
    QCOMPARE(reader.metadata().name, std::string("fixture"));
    QCOMPARE(reader.LookupExact("EXAMPLE").front().headword,
             std::string("example"));
    QCOMPARE(reader.SuggestPrefix("nested/s").front(),
             std::string("nested/second"));
    QCOMPARE(reader.Resource("example.wav"), std::string("RIFFfixture-wave"));
    QCOMPARE(reader.Resource("nested/second.ogg"),
             std::string("OggSfixture-ogg"));
    QVERIFY(reader.Resource("ignored.txt").empty());
}

void ZipSoundsReaderTest::RejectsCorruptMemberChecksums() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = test::WriteZipSoundsFixture(
        std::filesystem::path(temporary.path().toStdString()));
    QFile file(QString::fromStdString(path.string()));
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.seek(41));
    QCOMPARE(file.write("X", 1), 1);
    file.close();
    const auto reader = Reader::Open(path);
    QVERIFY_EXCEPTION_THROWN(reader.Resource("example.wav"), Error);
}
}  // namespace goldendict::core::formats::zipsounds

using goldendict::core::formats::zipsounds::ZipSoundsReaderTest;
QTEST_MAIN(ZipSoundsReaderTest)
#include "zipsounds_reader_test.moc"
