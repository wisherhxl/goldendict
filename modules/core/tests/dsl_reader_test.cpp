// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/dsl/dsl_reader.h"
#include "support/dsl_fixture.h"

namespace goldendict::core::formats::dsl {

class DslReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsMetadataExpansionsMarkupAndRankedMatches();
    void ReadsCompressedAndUtf16AndInvokesCheckpoints();
    void RejectsMalformedOrCorruptInput();
};

void DslReaderTest::ReadsMetadataExpansionsMarkupAndRankedMatches() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteDslFixture(
        std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(reader.metadata().name, "Fixture DSL");
    QCOMPARE(reader.metadata().source_language, "en");
    QCOMPARE(reader.metadata().target_language, "de");
    QCOMPARE(reader.LookupExact("caf").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("cafe").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("caf shop").size(), std::size_t{1});
    const auto article = reader.LookupExact("cafe").front().data;
    QVERIFY(article.find("<b>drink</b>") != std::string::npos);
    QVERIFY(article.find("bword://coffee") != std::string::npos);
    QVERIFY(article.find("images/cup.png") != std::string::npos);
    QVERIFY(article.find("<gd-optional>optional</gd-optional>") !=
            std::string::npos);
    QCOMPARE(reader.SuggestPrefix("caf").front(), "Caf");
    const auto full_text = reader.ReadFullTextArticles();
    QCOMPARE(full_text.size(), std::size_t{2});
    QCOMPARE(full_text[0].record_ordinal, std::size_t{0});
    QCOMPARE(full_text[0].headword, std::string("Caf"));
    QCOMPARE(full_text[0].article_ordinal, std::size_t{0});
    QCOMPARE(full_text[1].record_ordinal, std::size_t{3});
    QCOMPARE(full_text[1].headword, std::string("cafeteria"));
    QCOMPARE(full_text[1].article_ordinal, std::size_t{1});
    QCOMPARE(reader.source_snapshot().size(), std::size_t{1});
    QCOMPARE(reader.source_snapshot().front().path,
             reader.dictionary_path().generic_string());
}

void DslReaderTest::ReadsCompressedAndUtf16AndInvokesCheckpoints() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Reader compressed = Reader::Open(
        test::CompressDslFixture(test::WriteDslFixture(root / "compressed")));
    int checkpoints = 0;
    QVERIFY(
        !compressed.LookupPrefix("caf", 2U, [&checkpoints]() { ++checkpoints; })
             .empty());
    QVERIFY(checkpoints > 0);
    const Reader utf16 = Reader::Open(test::WriteUtf16LeDslFixture(root));
    QCOMPARE(utf16.metadata().name, "UTF16 DSL");
    QCOMPARE(utf16.LookupExact("example").front().headword, "example");
    QCOMPARE(utf16.ReadFullTextArticles().size(), std::size_t{1});
    const Reader compressed_utf16 = Reader::Open(test::CompressDslFixture(
        test::WriteUtf16LeDslFixture(root / "utf16-compressed")));
    QCOMPARE(compressed_utf16.LookupExact("example").size(), std::size_t{1});
}

void DslReaderTest::RejectsMalformedOrCorruptInput() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto no_article = root / "empty.dsl";
    std::ofstream(no_article) << "#NAME \"Empty\"\n";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(no_article), Error);
    const auto corrupt = root / "corrupt.dsl.dz";
    std::ofstream(corrupt, std::ios::binary) << "not gzip";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(corrupt), Error);
}

}  // namespace goldendict::core::formats::dsl

using goldendict::core::formats::dsl::DslReaderTest;
QTEST_APPLESS_MAIN(DslReaderTest)
#include "dsl_reader_test.moc"
