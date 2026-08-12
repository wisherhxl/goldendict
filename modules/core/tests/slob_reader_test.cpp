// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/slob/slob_reader.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include "support/slob_fixture.h"

namespace goldendict::core::formats::slob {
class SlobReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsMetadataAliasesArticlesAndResources();
    void ReadsCompressedItems();
    void RejectsCorruption();
};

void SlobReaderTest::ReadsMetadataAliasesArticlesAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteSlobFixture(
        std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(reader.metadata().name, "Fixture SLOB");
    QCOMPARE(reader.metadata().source_language, "en");
    QCOMPARE(reader.metadata().target_language, "de");
    QCOMPARE(reader.LookupExact("EXAMPLE").front().data,
             "<b>definition</b><img src=\"pixel.png\">");
    QCOMPARE(reader.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(reader.SuggestPrefix("exa").front(), "example");
    QCOMPARE(*reader.Resource("pixel.png"), "png-data");
}

void SlobReaderTest::ReadsCompressedItems() {
    for (const auto compression : {"zlib", "bz2"}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const Reader reader = Reader::Open(test::WriteSlobFixture(
            std::filesystem::path(directory.path().toStdString()),
            compression));
        QCOMPARE(reader.LookupExact("example").size(), std::size_t{1});
    }
}

void SlobReaderTest::RejectsCorruption() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path =
        std::filesystem::path(directory.path().toStdString()) / "broken.slob";
    std::ofstream(path, std::ios::binary) << "not slob";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(path), Error);
}
}  // namespace goldendict::core::formats::slob

using goldendict::core::formats::slob::SlobReaderTest;
QTEST_APPLESS_MAIN(SlobReaderTest)
#include "slob_reader_test.moc"
