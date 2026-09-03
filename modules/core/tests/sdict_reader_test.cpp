// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/sdict/sdict_reader.h"
#include "support/sdict_fixture.h"

namespace goldendict::core::formats::sdict {

class SdictReaderTest : public QObject {
    Q_OBJECT

   private slots:
    void ReadsMetadataRankedMatchesAndMarkupLinks();
    void ReadsZlibAndBzip2Fields();
    void EnumeratesDistinctFullTextArticlesWithStableProvenance();
    void InvokesScanCheckpoints();
    void RejectsCorruptAndUnsupportedCompression();
    void RejectsInvalidSignatureAndTruncatedArticle();
};

void SdictReaderTest::ReadsMetadataRankedMatchesAndMarkupLinks() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteSdictFixture(
        root,
        {{"cafeteria", "long"}, {"Caf\xc3\xa9", "<t>exact</t> <r>target</r>"}});

    const Reader reader = Reader::Open(path);
    const auto exact = reader.LookupExact("CAFE");
    const auto prefix = reader.LookupPrefix("CAFE", 2U);
    const auto suggestions = reader.SuggestPrefix("CAFE", 2U);

    QCOMPARE(reader.metadata().name, "Fixture SDict");
    QCOMPARE(reader.metadata().source_language, "eng");
    QCOMPARE(reader.metadata().target_language, "deu");
    QCOMPARE(exact.size(), std::size_t{1});
    QVERIFY(exact.front().data.find("<span>exact</span>") != std::string::npos);
    QVERIFY(exact.front().data.find("href=\"bword://target\"") !=
            std::string::npos);
    QCOMPARE(prefix.size(), std::size_t{2});
    QCOMPARE(prefix.front().headword, "Caf\xc3\xa9");
    QCOMPARE(suggestions.front(), "Caf\xc3\xa9");
}

void SdictReaderTest::ReadsZlibAndBzip2Fields() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    for (const std::uint8_t compression : {std::uint8_t{1}, std::uint8_t{2}}) {
        const auto path = test::WriteSdictFixture(
            root / std::to_string(static_cast<unsigned int>(compression)),
            {{"example", "compressed definition"}}, compression);
        const Reader reader = Reader::Open(path);
        QCOMPARE(reader.LookupExact("example").front().data,
                 "compressed definition");
    }
}

void SdictReaderTest::EnumeratesDistinctFullTextArticlesWithStableProvenance() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteSdictFixture(
        root, {{"canonical", "<b>shared definition</b>"},
               {"alias", "ignored alias data", 0U},
               {"second", "second definition"}});

    const Reader reader = Reader::Open(path);
    const auto articles = reader.ReadFullTextArticles();

    QCOMPARE(articles.size(), std::size_t{2});
    QCOMPARE(articles[0].record_ordinal, std::size_t{0});
    QCOMPARE(articles[0].headword, std::string("canonical"));
    QCOMPARE(articles[0].data, std::string("<b>shared definition</b>"));
    QCOMPARE(articles[1].record_ordinal, std::size_t{2});
    QCOMPARE(articles[1].headword, std::string("second"));
    QVERIFY(articles[0].article_offset != articles[1].article_offset);
    QCOMPARE(reader.source_snapshot().size(), std::size_t{1});
    QCOMPARE(reader.source_snapshot().front().path,
             std::filesystem::weakly_canonical(path).generic_string());
}

void SdictReaderTest::InvokesScanCheckpoints() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Reader reader = Reader::Open(
        test::WriteSdictFixture(root, {{"example", "definition"}}));
    std::size_t checkpoints = 0;

    const auto result =
        reader.LookupPrefix("exa", 1U, [&checkpoints]() { ++checkpoints; });

    QCOMPARE(result.size(), std::size_t{1});
    QCOMPARE(checkpoints, std::size_t{1});
}

void SdictReaderTest::RejectsCorruptAndUnsupportedCompression() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto corrupt = test::WriteSdictFixture(
        root / "corrupt", {{"example", "compressed definition"}}, 1U);
    {
        std::fstream file(corrupt,
                          std::ios::binary | std::ios::in | std::ios::out);
        file.seekg(-1, std::ios::end);
        char byte = 0;
        file.read(&byte, 1);
        byte ^= 0x5a;
        file.seekp(-1, std::ios::end);
        file.write(&byte, 1);
    }
    const Reader reader = Reader::Open(corrupt);
    QVERIFY_EXCEPTION_THROWN(reader.LookupExact("example"), Error);

    const auto unsupported = test::WriteSdictFixture(
        root / "unsupported", {{"example", "definition"}});
    {
        std::fstream file(unsupported,
                          std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(10, std::ios::beg);
        file.put(static_cast<char>(3));
    }
    try {
        static_cast<void>(Reader::Open(unsupported));
        QFAIL("Unsupported SDict compression should fail");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kUnsupportedFeature);
    }
}

void SdictReaderTest::RejectsInvalidSignatureAndTruncatedArticle() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path =
        test::WriteSdictFixture(root, {{"example", "definition"}});
    std::ofstream(path, std::ios::binary | std::ios::trunc) << "invalid";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(path), Error);

    const auto truncated = test::WriteSdictFixture(root / "truncated",
                                                   {{"example", "definition"}});
    std::filesystem::resize_file(truncated,
                                 std::filesystem::file_size(truncated) - 1U);
    QVERIFY_EXCEPTION_THROWN(Reader::Open(truncated), Error);
}

}  // namespace goldendict::core::formats::sdict

using goldendict::core::formats::sdict::SdictReaderTest;
QTEST_APPLESS_MAIN(SdictReaderTest)
#include "sdict_reader_test.moc"
