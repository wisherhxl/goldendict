// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/aard/aard_reader.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>
#include "support/aard_fixture.h"

namespace goldendict::core::formats::aard {
class AardReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsMetadataAliasesArticlesAndRedirects();
    void ReadsBzip2Payloads();
    void ReadsLegacyRawArticles();
    void StopsTraversalOnCheckpointOrVisitorFailure();
    void RejectsCorruption();
};

void AardReaderTest::ReadsMetadataAliasesArticlesAndRedirects() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteAardFixture(
        std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(reader.metadata().name, "Fixture Aard");
    QCOMPARE(reader.metadata().source_language, "en");
    QCOMPARE(reader.metadata().target_language, "de");
    QCOMPARE(reader.metadata().article_count, std::size_t{2});
    QCOMPARE(reader.source_snapshot().size(), std::size_t{1});

    struct ArticleSnapshot {
        std::size_t record_ordinal;
        std::string headword;
        std::size_t article_ordinal;
        std::string data;
    };

    static_assert(
        std::is_same_v<decltype(FullTextArticle::headword), std::string_view>);
    static_assert(
        std::is_same_v<decltype(FullTextArticle::data), std::string_view>);
    std::vector<ArticleSnapshot> full_text;
    std::size_t checkpoints = 0U;
    reader.VisitFullTextArticles(
        [&full_text](const FullTextArticle& article) {
            full_text.push_back(
                {article.record_ordinal, std::string(article.headword),
                 article.article_ordinal, std::string(article.data)});
        },
        [&checkpoints]() { ++checkpoints; });
    QCOMPARE(checkpoints, std::size_t{3});
    QCOMPARE(full_text.size(), std::size_t{2});
    QCOMPARE(full_text[0].record_ordinal, std::size_t{0});
    QCOMPARE(full_text[0].headword, "example");
    QCOMPARE(full_text[0].article_ordinal, std::size_t{0});
    QCOMPARE(full_text[0].data,
             std::string("<b>definition</b><a href=\"bword://alias\">"
                         "alias</a>"));
    QCOMPARE(full_text[1].record_ordinal, std::size_t{2});
    QCOMPARE(full_text[1].headword, "redirect");
    QCOMPARE(full_text[1].article_ordinal, std::size_t{1});
    QCOMPARE(full_text[1].data,
             std::string("<a href=\"bword://example\">example</a>"));
    QCOMPARE(reader.LookupExact("EXAMPLE").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(reader.SuggestPrefix("exa").front(), "example");
    QVERIFY(reader.LookupPrefix("exa", 0U).empty());
    QVERIFY(reader.LookupExact("example").front().data.find(
                "<b>definition</b>") != std::string::npos);
    QVERIFY(reader.LookupExact("example").front().data.find("bword://alias") !=
            std::string::npos);
    QVERIFY(
        reader.LookupExact("redirect").front().data.find("bword://example") !=
        std::string::npos);
}

void AardReaderTest::StopsTraversalOnCheckpointOrVisitorFailure() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteAardFixture(
        std::filesystem::path(directory.path().toStdString())));

    std::size_t checkpoints = 0U;
    std::vector<std::size_t> emitted;
    try {
        reader.VisitFullTextArticles(
            [&emitted](const FullTextArticle& article) {
                emitted.push_back(article.record_ordinal);
            },
            [&checkpoints]() {
                if (++checkpoints == 2U)
                    throw std::runtime_error("checkpoint failure");
            });
        QFAIL("checkpoint exception was not propagated");
    } catch (const std::runtime_error& error) {
        QCOMPARE(std::string_view(error.what()),
                 std::string_view("checkpoint failure"));
    }
    QCOMPARE(checkpoints, std::size_t{2});
    QCOMPARE(emitted, (std::vector<std::size_t>{0U}));

    checkpoints = 0U;
    emitted.clear();
    try {
        reader.VisitFullTextArticles(
            [&emitted](const FullTextArticle& article) {
                emitted.push_back(article.record_ordinal);
                throw std::runtime_error("visitor failure");
            },
            [&checkpoints]() { ++checkpoints; });
        QFAIL("visitor exception was not propagated");
    } catch (const std::runtime_error& error) {
        QCOMPARE(std::string_view(error.what()),
                 std::string_view("visitor failure"));
    }
    QCOMPARE(checkpoints, std::size_t{1});
    QCOMPARE(emitted, (std::vector<std::size_t>{0U}));
}

void AardReaderTest::ReadsBzip2Payloads() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteAardFixture(
        std::filesystem::path(directory.path().toStdString()), "bzip.aar", true,
        true));
    QCOMPARE(reader.LookupExact("example").size(), std::size_t{1});
    QVERIFY(reader.LookupExact("example").front().data.find("definition") !=
            std::string::npos);
}

void AardReaderTest::ReadsLegacyRawArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteAardFixture(
        std::filesystem::path(directory.path().toStdString()), "raw.aar", false,
        false, true));
    QCOMPARE(reader.LookupExact("example").size(), std::size_t{1});
}

void AardReaderTest::RejectsCorruption() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path =
        std::filesystem::path(directory.path().toStdString()) / "bad.aar";
    std::ofstream(path, std::ios::binary) << "not aard";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(path), Error);
}
}  // namespace goldendict::core::formats::aard

using goldendict::core::formats::aard::AardReaderTest;
QTEST_APPLESS_MAIN(AardReaderTest)
#include "aard_reader_test.moc"
