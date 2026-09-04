// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/dsl/dsl_headword_parser.h"
#include "../src/formats/dsl/dsl_reader.h"
#include "support/dsl_fixture.h"

namespace goldendict::core::formats::dsl {

class DslReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsMetadataExpansionsMarkupAndRankedMatches();
    void PreservesLegacyHeadwordExpansionSemantics();
    void ExposesExactLegacyExpansionRecordsAndBounds();
    void RendersLegacyDisplayTildesAndEscapes();
    void ReadsCompressedAndUtf16AndInvokesCheckpoints();
    void SupportsApprovedLargeDictionaryBoundary();
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

void DslReaderTest::PreservesLegacyHeadwordExpansionSemantics() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    std::string text =
        "#NAME \"Legacy headwords\"\n"
        "#SOURCE_CODE_PAGE \"UTF-8\"\n"
        "Alpha{ignored}(x)\n"
        "~ alias\n"
        "\tfirst\n"
        "Case\n"
        "^~\n"
        "\tsecond\n"
        "dup(x)(x)\n"
        "\tthird\n"
        "Esc\\{aped\\}(x)\n"
        "\\~ literal\n"
        "\tfourth\n"
        "cap(1)(2)(3)(4)(5)(6)\n"
        "~(a)(b)(c)(d)(e)(f)\n"
        "\tfifth\n";
    std::string within_legacy_limit;
    for (std::size_t index = 0U; index < 497U; ++index) {
        within_legacy_limit += "\xc3\xa9";
    }
    text += within_legacy_limit + "(x)\n\tsixth\n";
    std::string beyond_legacy_limit;
    for (std::size_t index = 0U; index < 501U; ++index) {
        beyond_legacy_limit += "\xc3\xa9";
    }
    text += beyond_legacy_limit + "(x)\n\tseventh\n";

    const Reader reader = Reader::Open(test::WriteDslTextFixture(root, text));

    QCOMPARE(reader.article_count(), std::size_t{7});
    QCOMPARE(reader.headword_count(), std::size_t{79});
    QCOMPARE(reader.LookupExact("Alpha").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("Alphax").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("Alpha alias").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("case").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("dupx").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("Esc{aped}").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("~ literal").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact(within_legacy_limit).size(), std::size_t{1});
    QCOMPARE(reader.LookupExact(within_legacy_limit + "x").size(),
             std::size_t{1});
    QCOMPARE(reader.LookupExact(beyond_legacy_limit + "(x)").size(),
             std::size_t{1});
}

void DslReaderTest::ExposesExactLegacyExpansionRecordsAndBounds() {
    const auto case_expansion = headword::Parse({"Case", "^~"});
    QCOMPARE(case_expansion.primary, std::string("Case"));
    QCOMPARE(case_expansion.records.size(), std::size_t{2});
    QCOMPARE(case_expansion.records[0], std::string("Case"));
    QCOMPARE(case_expansion.records[1], std::string("case"));

    const std::string deseret_small_long_i = "\xf0\x90\x90\xa8";
    const auto supplementary_case =
        headword::Parse({deseret_small_long_i, "^~"});
    QCOMPARE(supplementary_case.records.size(), std::size_t{2});
    QCOMPARE(supplementary_case.records[0], deseret_small_long_i);
    QCOMPARE(supplementary_case.records[1], deseret_small_long_i);

    const auto rolling_tilde = headword::Parse({"z", "a", "~x"});
    QCOMPARE(rolling_tilde.primary, std::string("z"));
    QCOMPARE(rolling_tilde.records.size(), std::size_t{3});
    QCOMPARE(rolling_tilde.records[0], std::string("a"));
    QCOMPARE(rolling_tilde.records[1], std::string("ax"));
    QCOMPARE(rolling_tilde.records[2], std::string("z"));

    const auto empty_first = headword::Parse({"(x)"});
    QCOMPARE(empty_first.primary, std::string("x"));
    QCOMPARE(empty_first.records.size(), std::size_t{2});
    QCOMPARE(empty_first.records[0], std::string());
    QCOMPARE(empty_first.records[1], std::string("x"));

    const auto duplicate_expansion = headword::Parse({"dup(x)(x)"});
    QCOMPARE(duplicate_expansion.records.size(), std::size_t{4});
    QCOMPARE(duplicate_expansion.records[0], std::string("dup"));
    QCOMPARE(duplicate_expansion.records[1], std::string("dupx"));
    QCOMPARE(duplicate_expansion.records[2], std::string("dupx"));
    QCOMPARE(duplicate_expansion.records[3], std::string("dupxx"));

    const auto per_line_cap = headword::Parse(
        {"cap(1)(2)(3)(4)(5)(6)", "~(a)(b)(c)(d)(e)(f)"});
    QCOMPARE(per_line_cap.records.size(), std::size_t{64});

    std::string adversarial = "bounded";
    for (std::size_t index = 0U; index < 120U; ++index) {
        adversarial += "(x)";
    }
    const auto bounded_expansion = headword::Parse({adversarial});
    QCOMPARE(bounded_expansion.records.size(), std::size_t{32});

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Reader reader = Reader::Open(test::WriteDslTextFixture(
        root,
        "z\n"
        "a\n"
        "~x\n"
        "\trolling ownership\n"));
    const auto full_text = reader.ReadFullTextArticles();
    QCOMPARE(full_text.size(), std::size_t{1});
    QCOMPARE(full_text.front().headword, std::string("z"));
    QCOMPARE(full_text.front().record_ordinal, std::size_t{0});
    QCOMPARE(full_text.front().article_ordinal, std::size_t{0});

    const std::string long_primary(16U * 1024U + 1U, 'z');
    const Reader filtered = Reader::Open(test::WriteDslTextFixture(
        root / "filtered",
        long_primary + "\nshort\n\tfiltered ownership\n"));
    QCOMPARE(filtered.LookupExact("short").size(), std::size_t{1});
    const auto filtered_full_text = filtered.ReadFullTextArticles();
    QCOMPARE(filtered_full_text.size(), std::size_t{1});
    QCOMPARE(filtered_full_text.front().headword, long_primary);
    QCOMPARE(filtered_full_text.front().record_ordinal, std::size_t{0});

    const Reader empty_filtered = Reader::Open(test::WriteDslTextFixture(
        root / "empty-filtered", "(x)\n\tempty ownership\n"));
    QCOMPARE(empty_filtered.headword_count(), std::size_t{2});
    QCOMPARE(empty_filtered.LookupExact("").size(), std::size_t{0});
    QCOMPARE(empty_filtered.LookupExact("x").size(), std::size_t{1});
    const auto [enumerated, complete] =
        empty_filtered.EnumerateHeadwords(0U, 10U, 1024U);
    QVERIFY(complete);
    QCOMPARE(enumerated.size(), std::size_t{1});
    QCOMPARE(enumerated.front(), std::string("x"));
    const auto empty_filtered_full_text =
        empty_filtered.ReadFullTextArticles();
    QCOMPARE(empty_filtered_full_text.size(), std::size_t{1});
    QCOMPARE(empty_filtered_full_text.front().headword, std::string("x"));
    QCOMPARE(empty_filtered_full_text.front().record_ordinal, std::size_t{0});
}

void DslReaderTest::RendersLegacyDisplayTildesAndEscapes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Reader reader = Reader::Open(test::WriteDslTextFixture(
        root,
        "#NAME \"Display tilde\"\n"
        "Al{ph}a(x)\n"
        "\t~ \\~ \\[b]literal\\[/b] [[i]] A\\ B "
        "<<tar\\>>get>> {{hidden\\}}still}} "
        "[c attr\\]x]color[/c] [s]pic\\[/s].png[/s]\n"));

    const auto results = reader.LookupExact("Ala");
    QCOMPARE(results.size(), std::size_t{1});
    QVERIFY(results.front().data.find("Alpha ~ [b]literal[/b]") !=
            std::string::npos);
    QVERIFY(results.front().data.find("[i]") != std::string::npos);
    QVERIFY(results.front().data.find(std::string("A\xc2\xa0" "B")) !=
            std::string::npos);
    QVERIFY(results.front().data.find(
                "<a href=\"bword://tar&gt;&gt;get\">tar&gt;&gt;get</a>") !=
            std::string::npos);
    QVERIFY(results.front().data.find("hidden") == std::string::npos);
    QVERIFY(results.front().data.find("still") == std::string::npos);
    QVERIFY(results.front().data.find("<span>color</span>") !=
            std::string::npos);
    QVERIFY(results.front().data.find("<img src=\"pic[/s].png\">") !=
            std::string::npos);
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

void DslReaderTest::SupportsApprovedLargeDictionaryBoundary() {
    constexpr std::size_t kApprovedBritannicaDecodedBytes = 595963560U;
    QVERIFY(kMaximumDictionaryBytes >= kApprovedBritannicaDecodedBytes);
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
