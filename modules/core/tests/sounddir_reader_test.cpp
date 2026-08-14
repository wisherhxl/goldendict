// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/sounddir/sounddir_reader.h"
#include <QtTest>
#include "support/sounddir_fixture.h"

namespace goldendict::core::formats::sounddir {
class SoundDirReaderTest final : public QObject {
    Q_OBJECT
   private slots:
    void RecursivelyIndexesAndLoadsAudioFiles();
    void RejectsEmptyConfiguredDirectories();
};

void SoundDirReaderTest::RecursivelyIndexesAndLoadsAudioFiles() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto temporary_root =
        std::filesystem::path(temporary.path().toStdString());
    const auto root =
        test::WriteSoundDirectoryFixture(temporary_root / "sounds");
    test::WriteFile(temporary_root / "outside.wav", "RIFFoutside");
    std::error_code symlink_error;
    std::filesystem::create_symlink(temporary_root / "outside.wav",
                                    root / "linked.wav", symlink_error);
    const auto reader = Reader::Open(root, "Fixture sounds");
    QCOMPARE(reader.metadata().name, std::string("Fixture sounds"));
    QCOMPARE(reader.LookupExact("EXAMPLE").front().headword,
             std::string("example"));
    QCOMPARE(reader.LookupPrefix("sec").front().headword,
             std::string("second"));
    QCOMPARE(reader.SuggestPrefix("s").front(), std::string("second"));
    QCOMPARE(reader.Resource("nested/second.ogg"),
             std::string("OggSfixture-ogg"));
    QVERIFY(reader.Resource("../secret.wav").empty());
    test::WriteFile(root / "added.wav", "RIFFadded-after-open");
    const auto page = reader.EnumerateHeadwords(0U, 10U, 1024U);
    QCOMPARE(page.first,
             (std::vector<std::string>{".hidden", "Apple", "apple", "duplicate",
                                       "example", "second"}));
    QVERIFY(page.second);
}

void SoundDirReaderTest::RejectsEmptyConfiguredDirectories() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    QVERIFY_EXCEPTION_THROWN(Reader::Open(root, {}), Error);
}
}  // namespace goldendict::core::formats::sounddir

using goldendict::core::formats::sounddir::SoundDirReaderTest;
QTEST_MAIN(SoundDirReaderTest)
#include "sounddir_reader_test.moc"
