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
    void ProjectsRetainedTextualReferencesForFullText();
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
        QCOMPARE(reader.ReadFullTextArticles().size(), std::size_t{1});
    }
}

void SlobReaderTest::ProjectsRetainedTextualReferencesForFullText() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteSlobFullTextFixture(
        std::filesystem::path(directory.path().toStdString())));
    const auto articles = reader.ReadFullTextArticles();
    QCOMPARE(articles.size(), std::size_t{12});
    QCOMPARE(articles[0].headword, std::string("first-owner"));
    QCOMPARE(articles[0].first_record_ordinal, std::size_t{0});
    QCOMPARE(articles[0].article_ordinal, std::size_t{0});
    QCOMPARE(articles[0].item_index, std::uint32_t{0});
    QCOMPARE(articles[0].bin_index, std::uint16_t{0});
    QCOMPARE(articles[1].headword, std::string("plain-owner"));
    QCOMPARE(articles[1].first_record_ordinal, std::size_t{2});
    QCOMPARE(articles[1].article_ordinal, std::size_t{1});
    QCOMPARE(articles[1].item_index, std::uint32_t{0});
    QCOMPARE(articles[1].bin_index, std::uint16_t{1});
    QCOMPARE(articles[2].headword, std::string("same-bin-other-item"));
    QCOMPARE(articles[2].first_record_ordinal, std::size_t{3});
    QCOMPARE(articles[2].item_index, std::uint32_t{1});
    QCOMPARE(articles[2].bin_index, std::uint16_t{0});
    QCOMPARE(articles.back().headword, std::string("owner-10"));
    QCOMPARE(articles.back().first_record_ordinal, std::size_t{13});
    QCOMPARE(articles.back().article_ordinal, std::size_t{11});
    QCOMPARE(articles.back().item_index, std::uint32_t{10});
    QCOMPARE(articles.back().bin_index, std::uint16_t{0});
    QCOMPARE(reader.source_snapshot().size(), std::size_t{1});
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
