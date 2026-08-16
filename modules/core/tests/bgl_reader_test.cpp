// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/bgl/bgl_reader.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include "support/bgl_fixture.h"

namespace goldendict::core::formats::bgl {
class BglReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsMetadataEntriesAliasesAndResources();
    void TraversesFullTextArticlesWithStableOwnership();
    void DecodesDeclaredLegacyCharset();
    void RejectsCorruption();
};

void BglReaderTest::ReadsMetadataEntriesAliasesAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteBglFixture(
        std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(reader.metadata().name, "Fixture BGL");
    QCOMPARE(reader.metadata().source_language, "en");
    QCOMPARE(reader.metadata().target_language, "de");
    QCOMPARE(reader.LookupExact("EXAMPLE").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(reader.FindHeadwordsForSynonym("alias", 20U),
             std::vector<std::string>{"example"});
    QVERIFY(reader.FindHeadwordsForSynonym("example", 20U).empty());
    QCOMPARE(reader.SuggestPrefix("exa").front(), "example");
    QVERIFY(reader.LookupPrefix("exa", 0U).empty());
    QVERIFY(reader.SuggestPrefix("exa", 0U).empty());
    QVERIFY(reader.LookupExact("example").front().data.find(
                "<b>definition</b>") != std::string::npos);
    QVERIFY(reader.Resource("pixel.png") != nullptr);
    QCOMPARE(*reader.Resource("pixel.png"), "png-data");
}

void BglReaderTest::TraversesFullTextArticlesWithStableOwnership() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteBglFullTextFixture(
        std::filesystem::path(directory.path().toStdString())));

    QCOMPARE(reader.article_count(), std::size_t{5});
    QCOMPARE(reader.headword_count(), std::size_t{6});
    const auto articles = reader.ReadFullTextArticles();
    QCOMPARE(articles.size(), std::size_t{3});
    QCOMPARE(articles[0].headword, "first-owner");
    QCOMPARE(articles[0].record_ordinal, std::size_t{0});
    QCOMPARE(articles[0].article_ordinal, std::size_t{0});
    QCOMPARE(articles[1].headword, "third-owner");
    QCOMPARE(articles[1].record_ordinal, std::size_t{3});
    QCOMPARE(articles[1].article_ordinal, std::size_t{2});
    QCOMPARE(articles[2].headword, "fourth-owner");
    QCOMPARE(articles[2].record_ordinal, std::size_t{4});
    QCOMPARE(articles[2].article_ordinal, std::size_t{3});
    QCOMPARE(reader.LookupExact("fourth-alias").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("empty-article").size(), std::size_t{1});
    QCOMPARE(reader.source_snapshot().size(), std::size_t{1});
}

void BglReaderTest::DecodesDeclaredLegacyCharset() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteWindows1251BglFixture(
        std::filesystem::path(directory.path().toStdString())));

    const auto article = reader.LookupExact(u8"пример");
    QCOMPARE(article.size(), std::size_t{1});
    QCOMPARE(article.front().headword, u8"пример");
    QCOMPARE(article.front().data, u8"тест");
}

void BglReaderTest::RejectsCorruption() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = root / "bad.bgl";
    std::ofstream(path, std::ios::binary) << "not bgl";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(path), Error);

    const auto corrupt_gzip = root / "corrupt.bgl";
    std::ofstream(corrupt_gzip, std::ios::binary)
        .write("\x12\x34\0\1\0\6not-gzip", 14);
    QVERIFY_EXCEPTION_THROWN(Reader::Open(corrupt_gzip), Error);

    std::string truncated;
    truncated.push_back('\x01');
    truncated.push_back('\x0a');
    truncated.push_back('x');
    QVERIFY_EXCEPTION_THROWN(
        Reader::Open(test::WriteBglStream(root, truncated, "truncated.bgl")),
        Error);
}
}  // namespace goldendict::core::formats::bgl

using goldendict::core::formats::bgl::BglReaderTest;
QTEST_APPLESS_MAIN(BglReaderTest)
#include "bgl_reader_test.moc"
