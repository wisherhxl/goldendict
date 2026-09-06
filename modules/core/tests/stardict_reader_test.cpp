// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "../src/formats/stardict/stardict_reader.h"
#include "support/stardict_fixture.h"

namespace goldendict::core::formats::stardict {

namespace internal {
std::string ConvertLegacyDescriptionText(std::string_view source);
}

namespace {

struct LegacyHtmlEntityExpectation {
    std::string_view name;
    std::uint32_t code_point;
};

constexpr LegacyHtmlEntityExpectation kLegacyHtmlEntityExpectations[] = {
#include "../src/formats/stardict/legacy_html_entities.inc"
};

void AppendExpectedUtf8(std::uint32_t code_point, std::string* output) {
    if (code_point <= 0x7fU) {
        output->push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7ffU) {
        output->push_back(static_cast<char>(0xc0U | (code_point >> 6U)));
        output->push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    } else if (code_point <= 0xffffU) {
        output->push_back(static_cast<char>(0xe0U | (code_point >> 12U)));
        output->push_back(
            static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
        output->push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    } else {
        output->push_back(static_cast<char>(0xf0U | (code_point >> 18U)));
        output->push_back(
            static_cast<char>(0x80U | ((code_point >> 12U) & 0x3fU)));
        output->push_back(
            static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
        output->push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    }
}

class StardictReaderTest : public QObject {
    Q_OBJECT

   private slots:
    void ReadsMetadataAndExactArticles();
    void MatchesFoldEquivalentHeadwords();
    void RanksFoldedPrefixMatches();
    void SuggestsDistinctRankedHeadwords();
    void ReadsSynonymsAndPreservesPrimaryHeadwordsInGeneratedIndex();
    void InfersLegacyLanguagesAndIgnoresIfoLanguageFields();
    void PreservesLegacyDescriptionMetadata();
    void ConvertsPreformattedDescriptionBoundaries();
    void PreservesLegacyPlainDescriptionWhitespace();
    void DecodesAllLegacyDescriptionEntities();
    void PreservesLegacyDescriptionAssembly();
    void ProcessesMalformedDescriptionInLinearTime();
    void AcceptsLegacyNumericPrefixes();
    void PreservesDeclaredCountsAndFiltersLegacySynonyms();
    void PreservesFrozenHtmlEscapedHeadwordBehavior();
    void AcceptsLegacyVersionAndAdvisoryIndexSize();
    void AcceptsMissingLegacyBookName();
    void RequiresVersionOnSecondInfoLine();
    void AcceptsTruncatedIndexAndSynonymTails();
    void RejectsSynonymTargetOutsideParsedPrimaryRecords();
    void RejectsUnsupportedIndexWidthAndDictionaryType();
    void ReadsCompressedIndexVariants();
    void ReadsLegacyDictionaryNameVariants();
    void ReadsSynonymNameAndCompressionVariants();
    void PrefersCanonicalPlainIndexAndSynonymFiles();
    void ContinuesWhenDeclaredSynonymFileIsMissing();
    void IgnoresUndeclaredSynonymFile();
    void RejectsCorruptCompressedIndexAndSynonymFiles();
    void RebuildsGeneratedIndexWhenCompressedSynonymChanges();
    void InvokesLookupCheckpoints();
    void ReturnsNoArticleForMissingHeadword();
    void RejectsInvalidInfoSignature();
    void RejectsInvalidNumericMetadata();
    void RejectsInvalidUtf8Headword();
    void RejectsArticleOutsideDictionaryData();
    void ReportsMissingCompanionFile();
    void ReadsCompressedDictionary();
    void PrefersUncompressedDictionary();
    void RejectsCorruptCompressedDictionary();
    void RejectsTruncatedCompressedDictionary();
    void RejectsOversizedCompressedDictionary();
    void RebuildsGeneratedIndexWhenCompressedSourceChanges();
    void CreatesAndReusesGeneratedIndex();
    void RebuildsPreviousGeneratedIndexFormat();
    void RebuildsStaleGeneratedIndex();
    void RebuildsCorruptGeneratedIndex();
    void RejectsDirectoryAsGeneratedIndexTarget();
};

std::filesystem::path TemporaryPath(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
}

void StardictReaderTest::ReadsMetadataAndExactArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory),
        {{"example", "A test definition."},
         {"\xE8\xAF\x8D\xE5\x85\xB8", "A UTF-8 headword."},
         {"example", "A second article."}});

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.metadata().book_name, "Generated Test Dictionary en-en");
    QCOMPARE(reader.metadata().word_count, std::uint64_t{3});
    const auto articles = reader.LookupExact("example");
    QCOMPARE(articles.size(), std::size_t{2});
    QCOMPARE(articles[0].data, "A test definition.");
    QCOMPARE(articles[1].data, "A second article.");
    QCOMPARE(reader.LookupExact("\xE8\xAF\x8D\xE5\x85\xB8").front().data,
             "A UTF-8 headword.");
}

void StardictReaderTest::MatchesFoldEquivalentHeadwords() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory), {{"Caf\xc3\xa9-au-lait", "accented"},
                                   {"Stra\xc3\x9f"
                                    "e",
                                    "case-folded"},
                                   {"\xe8\xaf\x8d\xe5\x85\xb8", "cjk"}});

    const Reader reader = Reader::Open(info_path);

    const auto accented = reader.LookupExact("CAFE AU LAIT");
    QCOMPARE(accented.size(), std::size_t{1});
    QCOMPARE(accented.front().headword, "Caf\xc3\xa9-au-lait");
    const auto case_folded = reader.LookupExact("STRASSE");
    QCOMPARE(case_folded.size(), std::size_t{1});
    QCOMPARE(case_folded.front().data, "case-folded");
    const auto cjk = reader.LookupExact("\xe8\xaf\x8d\xe5\x85\xb8");
    QCOMPARE(cjk.size(), std::size_t{1});
    QCOMPARE(cjk.front().data, "cjk");
    QVERIFY(reader.LookupExact("!!!").empty());
}

void StardictReaderTest::
    ReadsSynonymsAndPreservesPrimaryHeadwordsInGeneratedIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root, {{"primary", "definition"}, {"other", "other definition"}});
    test::WriteStardictSynonyms(info_path, {{"alias", 0U}});
    const auto generated_index = root / "fixture.gdindex";

    const Reader created = Reader::Open(info_path, generated_index);
    QCOMPARE(created.index_state(), IndexState::kCreated);
    QCOMPARE(created.LookupExact("alias").front().data, "definition");
    QCOMPARE(created.FindHeadwordsForSynonym("alias", 20U),
             std::vector<std::string>{"primary"});
    QCOMPARE(created.SuggestPrefix("ali").front(), "alias");

    const Reader reused = Reader::Open(info_path, generated_index);
    QCOMPARE(reused.index_state(), IndexState::kReused);
    QCOMPARE(reused.FindHeadwordsForSynonym("alias", 20U),
             std::vector<std::string>{"primary"});
}

void StardictReaderTest::InfersLegacyLanguagesAndIgnoresIfoLanguageFields() {
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path =
            test::WriteStardictFixture(root, {{"entry", "article"}});
        test::AppendStardictInfoField(info_path, "bookname",
                                      "Dictionary Without Language Codes");
        test::AppendStardictInfoField(info_path, "lang_from", "zz");
        test::AppendStardictInfoField(info_path, "lang_to", "yy");

        const Reader reader = Reader::Open(info_path);

        QVERIFY(reader.metadata().source_language.empty());
        QVERIFY(reader.metadata().target_language.empty());
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path =
            test::WriteStardictFixture(root, {{"entry", "article"}});
        test::AppendStardictInfoField(info_path, "bookname",
                                      "Unknown zz-yy Dictionary");

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.metadata().source_language, "zz");
        QCOMPARE(reader.metadata().target_language, "yy");
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path =
            test::WriteStardictFixture(root, {{"entry", "article"}});
        test::AppendStardictInfoField(info_path, "bookname",
                                      "Unknown zzz-yyy Dictionary");

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.metadata().source_language, "zz");
        QCOMPARE(reader.metadata().target_language, "yy");
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path =
            test::WriteStardictFixture(root, {{"entry", "article"}});
        const auto renamed_base = root / "lexicon-eng-fre";
        std::filesystem::rename(info_path, renamed_base.string() + ".ifo");
        std::filesystem::rename(root / "fixture.idx",
                                renamed_base.string() + ".idx");
        std::filesystem::rename(root / "fixture.dict",
                                renamed_base.string() + ".dict");

        const Reader reader = Reader::Open(renamed_base.string() + ".ifo");

        QCOMPARE(reader.metadata().source_language, "en");
        QCOMPARE(reader.metadata().target_language, "fr");
    }
    for (const auto& [filename, source_language, target_language] :
         std::vector<std::tuple<std::string, std::string, std::string>>{
             {"dict-e\xe2\x84\xaa-fre", "ek", "fr"},
             {"dict-\xc5\xbfv-eng", "sv", "en"}}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path =
            test::WriteStardictFixture(root, {{"entry", "article"}});
        const auto renamed_base = root / std::filesystem::u8path(filename);
        auto renamed_info = renamed_base;
        renamed_info += ".ifo";
        auto renamed_index = renamed_base;
        renamed_index += ".idx";
        auto renamed_dictionary = renamed_base;
        renamed_dictionary += ".dict";
        std::filesystem::rename(info_path, renamed_info);
        std::filesystem::rename(root / "fixture.idx", renamed_index);
        std::filesystem::rename(root / "fixture.dict", renamed_dictionary);

        const Reader reader = Reader::Open(renamed_info);

        QCOMPARE(reader.metadata().source_language, source_language);
        QCOMPARE(reader.metadata().target_language, target_language);
    }
}

void StardictReaderTest::PreservesDeclaredCountsAndFiltersLegacySynonyms() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"first", "one"}, {"second", "two"}});
    test::WriteStardictSynonyms(
        info_path, {{"alias", 0U}, {"/discard$", 0U}, {"discard/$", 1U}});
    const auto generated_index = root / "fixture.gdidx";

    const Reader created = Reader::Open(info_path, generated_index);

    QCOMPARE(created.article_count(), std::size_t{2});
    QCOMPARE(created.headword_count(), std::size_t{5});
    QCOMPARE(created.metadata().synonym_count, std::uint64_t{3});
    QCOMPARE(created.LookupExact("alias").front().data, "one");
    QVERIFY(created.LookupExact("/discard$").empty());
    QVERIFY(created.LookupExact("discard/$").empty());
    const Reader reused = Reader::Open(info_path, generated_index);
    QCOMPARE(reused.index_state(), IndexState::kReused);
    QCOMPARE(reused.article_count(), std::size_t{2});
    QCOMPARE(reused.headword_count(), std::size_t{5});
    QCOMPARE(reused.ReadPrimaryArticles().size(), std::size_t{2});
}

void StardictReaderTest::PreservesFrozenHtmlEscapedHeadwordBehavior() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root, {{"encoded&#65;", "legacy empty headword"}});
    const auto generated_index = root / "fixture.gdidx";

    const Reader created = Reader::Open(info_path, generated_index);

    QVERIFY(created.LookupExact("encodedA").empty());
    const auto [headwords, complete] =
        created.EnumerateHeadwords(0U, 10U, 1024U);
    QCOMPARE(headwords, std::vector<std::string>{""});
    QVERIFY(complete);
    const Reader reused = Reader::Open(info_path, generated_index);
    QCOMPARE(reused.index_state(), IndexState::kReused);
    QCOMPARE(reused.EnumerateHeadwords(0U, 10U, 1024U).first,
             std::vector<std::string>{""});
}

void StardictReaderTest::AcceptsLegacyVersionAndAdvisoryIndexSize() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"entry", "article"}});
    auto info = test::ReadBinaryFile(info_path);
    const auto version = info.find("version=2.4.2");
    QVERIFY(version != std::string::npos);
    info.replace(version, std::string("version=2.4.2").size(),
                 "version=legacy-compatible");
    info += "idxfilesize=1\n";
    info += "wordcount=0\n";
    test::WriteBinaryFile(info_path, info);

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.LookupExact("entry").front().data, "article");
    QCOMPARE(reader.metadata().index_file_size, std::uint64_t{1});
    QCOMPARE(reader.article_count(), std::size_t{0});
    QCOMPARE(reader.headword_count(), std::size_t{0});
    QCOMPARE(reader.ReadPrimaryArticles().size(), std::size_t{1});
}

void StardictReaderTest::PreservesLegacyDescriptionMetadata() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"entry", "article"}});
    test::AppendStardictInfoField(info_path, "copyright", "First<BR>Second");
    test::AppendStardictInfoField(
        info_path, "description",
        "  <p>One\tTwo</p>After<div>Three &copy; &euro; &Alpha; &nbsp; "
        "&#128; <b>bold</b></div><div>Nested<div>Inner</div>Tail</div>"
        "<script>hidden</script>  ");

    const Reader reader = Reader::Open(info_path);

    const std::string expected_description =
        "Copyright: First\nSecond\n\nAuthor: Fixture Author\n\n"
        "One\nTwo\nAfter\nThree \xc2\xa9 \xe2\x82\xac \xce\x91   "
        "\xe2\x82\xac bold\nNested\nInnerTail";
    QCOMPARE(reader.metadata().description, expected_description);

    QTemporaryDir semicolonless_directory;
    QVERIFY(semicolonless_directory.isValid());
    const auto semicolonless_info = test::WriteStardictFixture(
        TemporaryPath(semicolonless_directory), {{"entry", "article"}});
    test::AppendStardictInfoField(semicolonless_info, "description",
                                  "Trailing &copy");

    const Reader semicolonless_reader = Reader::Open(semicolonless_info);

    QCOMPARE(semicolonless_reader.metadata().description,
             "Author: Fixture Author\n\nTrailing \xc2\xa9");

    for (const auto& [source, expected] :
         std::vector<std::pair<std::string, std::string>>{
             {"&nbsp;A", " A"},
             {"A&nbsp;", "A "},
             {"A &nbsp; B", "A   B"},
             {"A\xc2\xa0 B&amp;C", "A  B&C"},
             {"A&emsp; B", "A B"},
             {"A &ensp;B", "A B"},
             {"A&thinsp;  B", "A B"},
             {"A&#9; B", "A B"},
             {"A\xc2\xa0"
              "B&amp;C",
              "A B&C"},
             {"A\xe2\x80\x83"
              "B&amp;C",
              "A B&C"},
             {"A\xe3\x80\x80"
              "B&amp;C",
              "A B&C"},
             {"<pre>A&emsp;B&#9;C</pre>",
              "A\xe2\x80\x83"
              "B\tC"},
             {"<pre>A\xe2\x80\x83"
              "B</pre>",
              "A\xe2\x80\x83"
              "B"},
             {"A\xe2\x80\xa8"
              "B&amp;C",
              "A B&C"},
             {"A\xe2\x80\xa9"
              "B&amp;C",
              "A\nB&C"},
             {"A&#x2028;B", "A B"},
             {"A&#x2029;B", "A\nB"},
             {"<pre>A&#x2028;B&#x2029;C</pre>", "A\nB\nC"},
             {"A<script>x</script>B", "AB"},
             {"A<script/>B", "AB"},
             {"<script>A<script/>B</script>C", "C"},
             {"<style>A<style/>B</style>C", "C"},
             {"<p>A&nbsp;</p>B", "A \nB"},
             {"A<div>B</div>C", "A\nBC"},
             {"A<hr>", "A\n"},
             {"<hr>A", "\n\nA"},
             {"A<hr>B", "A\n\nB"},
             {"\xc2\xa0 A &amp; B \xc2\xa0", "A & B"}}) {
        QTemporaryDir boundary_directory;
        QVERIFY(boundary_directory.isValid());
        const auto boundary_info = test::WriteStardictFixture(
            TemporaryPath(boundary_directory), {{"entry", "article"}});
        test::AppendStardictInfoField(boundary_info, "description", source);

        const Reader boundary_reader = Reader::Open(boundary_info);

        QCOMPARE(boundary_reader.metadata().description,
                 std::string{"Author: Fixture Author\n\n"} + expected);
    }
}

void StardictReaderTest::ConvertsPreformattedDescriptionBoundaries() {
    for (const auto& [source, expected] :
         std::vector<std::pair<std::string, std::string>>{
             {"<pre>\nA\n</pre>", "A"},
             {"<pre>\n\nA\n\n</pre>", "\nA\n"},
             {"X<pre>\nA\n</pre>Y", "X\nA\nY"},
             {"<pre>&#10;A&#10;</pre>", "\nA"}}) {
        QCOMPARE(internal::ConvertLegacyDescriptionText(source), expected);
    }
}

void StardictReaderTest::PreservesLegacyPlainDescriptionWhitespace() {
    for (const auto& value : std::vector<std::string>{
             "  A  ", "\xc2\xa0 A \xc2\xa0", "\xe3\x80\x80 A \xe3\x80\x80"}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto info_path = test::WriteStardictFixture(
            TemporaryPath(directory), {{"entry", "article"}});
        test::AppendStardictInfoField(info_path, "description", value);

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.metadata().description,
                 std::string{"Author: Fixture Author\n\n"} + value);
    }
}

void StardictReaderTest::DecodesAllLegacyDescriptionEntities() {
    std::string source;
    std::string expected;
    for (const auto& entity : kLegacyHtmlEntityExpectations) {
        source += "A&";
        source += entity.name;
        source += ";B|";
        expected += 'A';
        if (entity.code_point == 0x00a0U || entity.code_point == 0x2002U ||
            entity.code_point == 0x2003U || entity.code_point == 0x2009U) {
            expected += ' ';
        } else {
            AppendExpectedUtf8(entity.code_point, &expected);
        }
        expected += "B|";
    }

    source += "A&#9;B|A&#10;B|A&#11;B|A&#12;B|A&#13;B|";
    expected += "A B|A B|A B|A B|A B|";
    source += "A&#128;B|A&#129;B|A&#xD800;B|A&#xDFFF;B|A&#x110000;B";
    expected +=
        "A\xe2\x82\xac"
        "B|A\xc2\x81"
        "B|A?B|A?B|A??B";
    source += "|A&#+65;B|A&#x+41;B|A&#X+41;B|A&#-65;B";
    expected += "|AAB|AAB|AAB|A&#-65;B";

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"entry", "article"}});
    test::AppendStardictInfoField(info_path, "description", source);

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.metadata().description,
             std::string{"Author: Fixture Author\n\n"} + expected);
}

void StardictReaderTest::PreservesLegacyDescriptionAssembly() {
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto info_path = test::WriteStardictFixture(
            TemporaryPath(directory), {{"entry", "article"}});
        auto info = test::ReadBinaryFile(info_path);
        const auto description = info.find("description=Fixture description\n");
        QVERIFY(description != std::string::npos);
        info.erase(description,
                   std::string("description=Fixture description\n").size());
        test::WriteBinaryFile(info_path, info);

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.metadata().description, "Author: Fixture Author\n\n");
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto info_path = test::WriteStardictFixture(
            TemporaryPath(directory), {{"entry", "article"}});
        auto info = test::ReadBinaryFile(info_path);
        const auto author = info.find("author=Fixture Author\n");
        QVERIFY(author != std::string::npos);
        info.erase(author, std::string("author=Fixture Author\n").size());
        const auto description = info.find("description=Fixture description\n");
        QVERIFY(description != std::string::npos);
        info.erase(description,
                   std::string("description=Fixture description\n").size());
        test::WriteBinaryFile(info_path, info);

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.metadata().description, "NONE");
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto info_path = test::WriteStardictFixture(
            TemporaryPath(directory), {{"entry", "article"}});
        auto info = test::ReadBinaryFile(info_path);
        const auto author = info.find("author=Fixture Author\n");
        QVERIFY(author != std::string::npos);
        info.erase(author, std::string("author=Fixture Author\n").size());
        test::WriteBinaryFile(info_path, info);
        test::AppendStardictInfoField(info_path, "description",
                                      "<script>x</script>");

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.metadata().description, "NONE");
    }
}

void StardictReaderTest::ProcessesMalformedDescriptionInLinearTime() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"entry", "article"}});
    const std::string malformed(900U * 1024U, '<');
    test::AppendStardictInfoField(info_path, "description", malformed);

    QElapsedTimer timer;
    timer.start();
    const Reader reader = Reader::Open(info_path);

    QVERIFY2(timer.elapsed() < 5000,
             "Malformed bounded metadata must be processed linearly");
    QCOMPARE(reader.metadata().description,
             std::string{"Author: Fixture Author\n\n"} + malformed);
}

void StardictReaderTest::AcceptsLegacyNumericPrefixes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"entry", "article"}});
    test::AppendStardictInfoField(info_path, "wordcount", " \t+7records");
    test::AppendStardictInfoField(info_path, "synwordcount", "+0ignored");
    test::AppendStardictInfoField(info_path, "idxfilesize", " 1byte");
    test::AppendStardictInfoField(info_path, "idxoffsetbits", " +32bits");

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.article_count(), std::size_t{7});
    QCOMPARE(reader.headword_count(), std::size_t{7});
    QCOMPARE(reader.metadata().synonym_count, std::uint64_t{0});
    QCOMPARE(reader.metadata().index_file_size, std::uint64_t{1});
    QCOMPARE(reader.ReadPrimaryArticles().size(), std::size_t{1});

    QTemporaryDir wrapped_directory;
    QVERIFY(wrapped_directory.isValid());
    const auto wrapped_info = test::WriteStardictFixture(
        TemporaryPath(wrapped_directory), {{"entry", "article"}});
    test::WriteStardictSynonymData(wrapped_info, {{"alias", 0U}});
    test::AppendStardictInfoField(wrapped_info, "synwordcount", "-1records");

    const Reader wrapped_reader = Reader::Open(wrapped_info);

    QCOMPARE(wrapped_reader.article_count(), std::size_t{1});
    QCOMPARE(wrapped_reader.metadata().synonym_count,
             std::uint64_t{std::numeric_limits<std::uint32_t>::max()});
    QCOMPARE(wrapped_reader.headword_count(), std::size_t{0});
    QCOMPARE(wrapped_reader.LookupExact("alias").size(), std::size_t{1});
}

void StardictReaderTest::RequiresVersionOnSecondInfoLine() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "invalid.ifo";
    test::WriteBinaryFile(path,
                          "StarDict's dict ifo file\nbookname=Invalid\n"
                          "version=2.4.2\n");

    try {
        static_cast<void>(Reader::Open(path));
        QFAIL("Reader::Open should require version on the second line");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidInfo);
    }
}

void StardictReaderTest::AcceptsMissingLegacyBookName() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    test::WriteBinaryFile(
        info_path,
        "StarDict's dict ifo file\nversion=2.4.2\nwordcount=1\n"
        "idxfilesize=16\nsametypesequence=m\n");

    const Reader reader = Reader::Open(info_path);

    QVERIFY(reader.metadata().book_name.empty());
    QCOMPARE(reader.LookupExact("example").front().data, "article");
}

void StardictReaderTest::AcceptsTruncatedIndexAndSynonymTails() {
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path =
            test::WriteStardictFixture(root, {{"entry", "article"}});
        test::WriteBinaryFile(root / "fixture.idx",
                              std::string("entry\0\0\0", 8U));

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.article_count(), std::size_t{1});
        QCOMPARE(reader.headword_count(), std::size_t{1});
        QVERIFY(reader.ReadPrimaryArticles().empty());
        QVERIFY(reader.LookupExact("entry").empty());
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path =
            test::WriteStardictFixture(root, {{"entry", "article"}});
        test::AppendStardictInfoField(info_path, "synwordcount", "1");
        test::WriteBinaryFile(root / "fixture.syn",
                              std::string("alias\0\0\0", 8U));

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.article_count(), std::size_t{1});
        QCOMPARE(reader.headword_count(), std::size_t{2});
        QVERIFY(reader.LookupExact("alias").empty());
    }
}

void StardictReaderTest::RejectsSynonymTargetOutsideParsedPrimaryRecords() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"entry", "article"}});
    test::AppendStardictInfoField(info_path, "wordcount", "2");
    test::WriteStardictSynonyms(info_path, {{"alias", 1U}});

    try {
        static_cast<void>(Reader::Open(info_path));
        QFAIL("Reader::Open should reject a synonym outside parsed primaries");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidIndex);
    }
}

void StardictReaderTest::RejectsUnsupportedIndexWidthAndDictionaryType() {
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto info_path = test::WriteStardictFixture(
            TemporaryPath(directory), {{"entry", "article"}});
        test::AppendStardictInfoField(info_path, "idxoffsetbits", "64");
        try {
            static_cast<void>(Reader::Open(info_path));
            QFAIL("Reader::Open should reject 64-bit index offsets");
        } catch (const Error& error) {
            QCOMPARE(error.code(), ErrorCode::kUnsupportedFeature);
        }
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto info_path = test::WriteStardictFixture(
            TemporaryPath(directory), {{"entry", "article"}});
        test::AppendStardictInfoField(info_path, "idxoffsetbits", "33");
        try {
            static_cast<void>(Reader::Open(info_path));
            QFAIL("Reader::Open should reject malformed index offsets");
        } catch (const Error& error) {
            QCOMPARE(error.code(), ErrorCode::kInvalidInfo);
        }
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto info_path = test::WriteStardictFixture(
            TemporaryPath(directory), {{"entry", "article"}});
        test::AppendStardictInfoField(info_path, "dicttype", "tree");
        try {
            static_cast<void>(Reader::Open(info_path));
            QFAIL("Reader::Open should reject special dictionary types");
        } catch (const Error& error) {
            QCOMPARE(error.code(), ErrorCode::kUnsupportedFeature);
        }
    }
}

void StardictReaderTest::ReadsCompressedIndexVariants() {
    for (const std::string suffix :
         {".idx.gz", ".idx.dz", ".IDX.GZ", ".IDX.DZ"}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path = test::WriteStardictFixture(
            root, {{"example", "compressed index definition"}});
        const auto source_index = root / "fixture.idx";
        const auto compressed_index = root / ("fixture" + suffix);
        test::CompressStardictCompanion(source_index, compressed_index);
        QVERIFY(std::filesystem::remove(source_index));

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.LookupExact("example").front().data,
                 "compressed index definition");
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "uppercase index"}});
    std::filesystem::rename(root / "fixture.idx", root / "fixture.IDX");

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.LookupExact("example").front().data, "uppercase index");
}

void StardictReaderTest::ReadsLegacyDictionaryNameVariants() {
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path = test::WriteStardictFixture(
            root, {{"example", "uppercase dictionary"}});
        std::filesystem::rename(root / "fixture.dict", root / "fixture.DICT");

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.LookupExact("example").front().data,
                 "uppercase dictionary");
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path = test::WriteStardictFixture(
            root, {{"example", "mixed-case compressed dictionary"}});
        const auto source_dictionary = root / "fixture.dict";
        test::CompressStardictCompanion(source_dictionary,
                                        root / "fixture.dict.DZ");
        QVERIFY(std::filesystem::remove(source_dictionary));

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.LookupExact("example").front().data,
                 "mixed-case compressed dictionary");
    }
}

void StardictReaderTest::ReadsSynonymNameAndCompressionVariants() {
    for (const std::string suffix :
         {".syn.gz", ".syn.dz", ".SYN.GZ", ".SYN.DZ"}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path = test::WriteStardictFixture(
            root, {{"primary", "compressed synonym definition"}});
        test::WriteStardictSynonyms(info_path, {{"alias", 0U}});
        const auto source_synonym = root / "fixture.syn";
        const auto compressed_synonym = root / ("fixture" + suffix);
        test::CompressStardictCompanion(source_synonym, compressed_synonym);
        QVERIFY(std::filesystem::remove(source_synonym));

        const Reader reader = Reader::Open(info_path);

        QCOMPARE(reader.LookupExact("alias").front().data,
                 "compressed synonym definition");
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"primary", "uppercase synonym"}});
    test::WriteStardictSynonyms(info_path, {{"alias", 0U}});
    std::filesystem::rename(root / "fixture.syn", root / "fixture.SYN");

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.LookupExact("alias").front().data, "uppercase synonym");
}

void StardictReaderTest::PrefersCanonicalPlainIndexAndSynonymFiles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"primary", "definition"}});
    test::WriteStardictSynonyms(info_path, {{"alias", 0U}});
    test::WriteBinaryFile(root / "fixture.idx.gz", "corrupt index shadow");
    test::WriteBinaryFile(root / "fixture.syn.gz", "corrupt synonym shadow");

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.LookupExact("alias").front().data, "definition");
}

void StardictReaderTest::ContinuesWhenDeclaredSynonymFileIsMissing() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory), {{"primary", "definition"}});
    test::AppendStardictInfoField(info_path, "synwordcount", "1");

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.LookupExact("primary").front().data, "definition");
    QVERIFY(reader.LookupExact("alias").empty());
}

void StardictReaderTest::IgnoresUndeclaredSynonymFile() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"primary", "definition"}});
    const auto synonym_path =
        test::WriteStardictSynonymData(info_path, {{"alias", 0U}});
    const auto generated_index = root / "fixture.gdidx";

    const Reader created = Reader::Open(info_path, generated_index);
    QCOMPARE(created.index_state(), IndexState::kCreated);
    QVERIFY(created.LookupExact("alias").empty());
    test::WriteBinaryFile(synonym_path, "changed but still ignored");

    const Reader reused = Reader::Open(info_path, generated_index);
    QCOMPARE(reused.index_state(), IndexState::kReused);
    QVERIFY(reused.LookupExact("alias").empty());
}

void StardictReaderTest::RejectsCorruptCompressedIndexAndSynonymFiles() {
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path =
            test::WriteStardictFixture(root, {{"primary", "definition"}});
        QVERIFY(std::filesystem::remove(root / "fixture.idx"));
        test::WriteBinaryFile(root / "fixture.idx.gz", "not gzip data");

        try {
            static_cast<void>(Reader::Open(info_path));
            QFAIL("Reader::Open should reject a corrupt compressed index");
        } catch (const Error& error) {
            QCOMPARE(error.code(), ErrorCode::kInvalidIndex);
        }
    }
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto info_path =
            test::WriteStardictFixture(root, {{"primary", "definition"}});
        test::AppendStardictInfoField(info_path, "synwordcount", "1");
        test::WriteBinaryFile(root / "fixture.syn.gz", "not gzip data");

        try {
            static_cast<void>(Reader::Open(info_path));
            QFAIL("Reader::Open should reject a corrupt compressed synonym");
        } catch (const Error& error) {
            QCOMPARE(error.code(), ErrorCode::kInvalidIndex);
        }
    }
}

void StardictReaderTest::RebuildsGeneratedIndexWhenCompressedSynonymChanges() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"primary", "definition"}});
    test::WriteStardictSynonyms(info_path, {{"first alias", 0U}});
    const auto source_synonym = root / "fixture.syn";
    const auto compressed_synonym = root / "fixture.syn.gz";
    test::CompressStardictCompanion(source_synonym, compressed_synonym);
    QVERIFY(std::filesystem::remove(source_synonym));
    const auto generated_index = root / "fixture.gdidx";
    const Reader created = Reader::Open(info_path, generated_index);
    QCOMPARE(created.index_state(), IndexState::kCreated);
    QCOMPARE(created.LookupExact("first alias").front().data, "definition");

    test::WriteStardictSynonymData(info_path, {{"second alias", 0U}});
    test::CompressStardictCompanion(source_synonym, compressed_synonym);
    QVERIFY(std::filesystem::remove(source_synonym));
    const auto modified = std::filesystem::last_write_time(compressed_synonym);
    std::filesystem::last_write_time(compressed_synonym,
                                     modified + std::chrono::seconds(2));

    const Reader rebuilt = Reader::Open(info_path, generated_index);
    QCOMPARE(rebuilt.index_state(), IndexState::kRebuiltStale);
    QVERIFY(rebuilt.LookupExact("first alias").empty());
    QCOMPARE(rebuilt.LookupExact("second alias").front().data, "definition");
}

void StardictReaderTest::RanksFoldedPrefixMatches() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory), {{"cafeteria", "long"},
                                   {"caf\xc3\xa9 noir", "medium"},
                                   {"Caf\xc3\xa9", "exact"},
                                   {"caffeine", "different-prefix"}});

    const Reader reader = Reader::Open(info_path);
    const auto articles = reader.LookupPrefix("CAFE", 3U);

    QCOMPARE(articles.size(), std::size_t{3});
    QCOMPARE(articles[0].data, "exact");
    QCOMPARE(articles[1].data, "medium");
    QCOMPARE(articles[2].data, "long");
}

void StardictReaderTest::SuggestsDistinctRankedHeadwords() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory), {{"cafeteria", "long"},
                                   {"caf\xc3\xa9 noir", "medium"},
                                   {"Caf\xc3\xa9", "first"},
                                   {"Caf\xc3\xa9", "second"}});

    const Reader reader = Reader::Open(info_path);
    const auto suggestions = reader.SuggestPrefix("CAFE", 3U);

    QCOMPARE(suggestions.size(), std::size_t{3});
    QCOMPARE(suggestions[0], "Caf\xc3\xa9");
    QCOMPARE(suggestions[1], "caf\xc3\xa9 noir");
    QCOMPARE(suggestions[2], "cafeteria");
}

void StardictReaderTest::InvokesLookupCheckpoints() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", "article"}});
    const Reader reader = Reader::Open(info_path);
    std::size_t checkpoints = 0;

    const auto articles =
        reader.LookupPrefix("exa", 1U, [&checkpoints]() { ++checkpoints; });

    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(checkpoints, std::size_t{1});
}

void StardictReaderTest::ReturnsNoArticleForMissingHeadword() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"present", "article"}});

    const Reader reader = Reader::Open(info_path);

    QVERIFY(reader.LookupExact("missing").empty());
}

void StardictReaderTest::RejectsInvalidInfoSignature() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "invalid.ifo";
    test::WriteBinaryFile(path, "Not a StarDict file\n");

    try {
        static_cast<void>(Reader::Open(path));
        QFAIL("Reader::Open should reject an invalid signature");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidInfo);
    }
}

void StardictReaderTest::RejectsInvalidNumericMetadata() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "invalid.ifo";
    test::WriteBinaryFile(
        path,
        "StarDict's dict ifo file\nversion=2.4.2\nbookname=Invalid\n"
        "wordcount=one\nidxfilesize=0\nsametypesequence=m\n");

    try {
        static_cast<void>(Reader::Open(path));
        QFAIL("Reader::Open should reject invalid numeric metadata");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidInfo);
    }
}

void StardictReaderTest::RejectsInvalidUtf8Headword() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory), {{std::string("\xc3\x28", 2), "invalid"}});

    try {
        static_cast<void>(Reader::Open(info_path));
        QFAIL("Reader::Open should reject an invalid UTF-8 headword");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidIndex);
    }
}

void StardictReaderTest::RejectsArticleOutsideDictionaryData() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(root, {{"example", "x"}});
    std::string index("example\0", 8);
    test::AppendBigEndian32(100, &index);
    test::AppendBigEndian32(5, &index);
    test::WriteBinaryFile(root / "fixture.idx", index);
    const std::string info =
        "StarDict's dict ifo file\nversion=2.4.2\n"
        "bookname=Generated Test Dictionary\nwordcount=1\nidxfilesize=" +
        std::to_string(index.size()) + "\nsametypesequence=m\n";
    test::WriteBinaryFile(info_path, info);

    try {
        static_cast<void>(Reader::Open(info_path));
        QFAIL("Reader::Open should reject an out-of-range article");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
    }
}

void StardictReaderTest::ReportsMissingCompanionFile() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(root, {{"example", "x"}});
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));

    try {
        static_cast<void>(Reader::Open(info_path));
        QFAIL("Reader::Open should report a missing dictionary data file");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kMissingFile);
    }
}

void StardictReaderTest::ReadsCompressedDictionary() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root, {{"example", "Compressed definition."}});
    const auto compressed_path = test::CompressStardictDictionary(info_path);
    QVERIFY(std::filesystem::is_regular_file(compressed_path));
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.LookupExact("example").front().data,
             "Compressed definition.");
}

void StardictReaderTest::PrefersUncompressedDictionary() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "plain"}});
    const auto compressed_path = test::CompressStardictDictionary(info_path);
    test::WriteBinaryFile(compressed_path, "corrupt compressed shadow");

    const Reader reader = Reader::Open(info_path);

    QCOMPARE(reader.LookupExact("example").front().data, "plain");
}

void StardictReaderTest::RejectsCorruptCompressedDictionary() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto compressed_path = test::CompressStardictDictionary(info_path);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    test::WriteBinaryFile(compressed_path, "not gzip data");

    try {
        static_cast<void>(Reader::Open(info_path));
        QFAIL("Reader::Open should reject corrupt compressed data");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
    }
}

void StardictReaderTest::RejectsTruncatedCompressedDictionary() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root, {{"example", "compressed article data"}});
    const auto compressed_path = test::CompressStardictDictionary(info_path);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    const auto compressed_size = std::filesystem::file_size(compressed_path);
    QVERIFY(compressed_size > 8U);
    std::filesystem::resize_file(compressed_path, compressed_size - 4U);

    try {
        static_cast<void>(Reader::Open(info_path));
        QFAIL("Reader::Open should reject a truncated compressed stream");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
    }
}

void StardictReaderTest::RejectsOversizedCompressedDictionary() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto compressed_path = test::CompressStardictDictionary(info_path);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    std::filesystem::resize_file(
        compressed_path,
        static_cast<std::uintmax_t>(2U) * 1024U * 1024U * 1024U + 1U);

    try {
        static_cast<void>(Reader::Open(info_path));
        QFAIL("Reader::Open should reject oversized compressed data");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
    }
}

void StardictReaderTest::RebuildsGeneratedIndexWhenCompressedSourceChanges() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "first"}});
    const auto compressed_path = test::CompressStardictDictionary(info_path);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    const auto generated_index = root / "fixture.gdidx";
    const Reader created = Reader::Open(info_path, generated_index);
    QCOMPARE(created.index_state(), IndexState::kCreated);

    test::WriteBinaryFile(root / "fixture.dict", "later");
    test::CompressStardictDictionary(info_path);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    const auto modified = std::filesystem::last_write_time(compressed_path);
    std::filesystem::last_write_time(compressed_path,
                                     modified + std::chrono::seconds(2));

    const Reader rebuilt = Reader::Open(info_path, generated_index);
    QCOMPARE(rebuilt.index_state(), IndexState::kRebuiltStale);
    QCOMPARE(rebuilt.LookupExact("example").front().data, "later");
}

void StardictReaderTest::CreatesAndReusesGeneratedIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "A test definition."}});
    const auto generated_index = root / "indexes" / "fixture.gdidx";

    const Reader created = Reader::Open(info_path, generated_index);
    QCOMPARE(created.index_state(), IndexState::kCreated);
    QVERIFY(std::filesystem::is_regular_file(generated_index));
    std::size_t generated_file_count = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator(generated_index.parent_path())) {
        static_cast<void>(entry);
        ++generated_file_count;
    }
    QCOMPARE(generated_file_count, std::size_t{1});

    const Reader reused = Reader::Open(info_path, generated_index);
    QCOMPARE(reused.index_state(), IndexState::kReused);
    QCOMPARE(reused.LookupExact("example").front().data, "A test definition.");
}

void StardictReaderTest::RebuildsStaleGeneratedIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto generated_index = root / "fixture.gdidx";
    const Reader created = Reader::Open(info_path, generated_index);
    QCOMPARE(created.index_state(), IndexState::kCreated);

    const auto source_index = root / "fixture.idx";
    const auto modified = std::filesystem::last_write_time(source_index);
    std::filesystem::last_write_time(source_index,
                                     modified + std::chrono::seconds(2));

    const Reader rebuilt = Reader::Open(info_path, generated_index);
    QCOMPARE(rebuilt.index_state(), IndexState::kRebuiltStale);
    QCOMPARE(rebuilt.LookupExact("example").front().data, "article");
}

void StardictReaderTest::RebuildsPreviousGeneratedIndexFormat() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto generated_index = root / "fixture.gdidx";
    const Reader source = Reader::Open(info_path);
    dictionary::StoreGeneratedIndex(generated_index, "stardict-records-v3",
                                    source.source_snapshot(), "obsolete");

    const Reader rebuilt = Reader::Open(info_path, generated_index);

    QCOMPARE(rebuilt.index_state(), IndexState::kRebuiltStale);
    QCOMPARE(rebuilt.LookupExact("example").front().data, "article");
}

void StardictReaderTest::RebuildsCorruptGeneratedIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto generated_index = root / "fixture.gdidx";
    const Reader created = Reader::Open(info_path, generated_index);
    QCOMPARE(created.index_state(), IndexState::kCreated);
    test::WriteBinaryFile(generated_index, "corrupt generated index");

    const Reader rebuilt = Reader::Open(info_path, generated_index);
    QCOMPARE(rebuilt.index_state(), IndexState::kRebuiltCorrupt);
    QCOMPARE(rebuilt.LookupExact("example").front().data, "article");

    const Reader reused = Reader::Open(info_path, generated_index);
    QCOMPARE(reused.index_state(), IndexState::kReused);
}

void StardictReaderTest::RejectsDirectoryAsGeneratedIndexTarget() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto generated_index = root / "fixture.gdidx";
    QVERIFY(std::filesystem::create_directory(generated_index));

    try {
        static_cast<void>(Reader::Open(info_path, generated_index));
        QFAIL("Reader::Open should reject a directory index target");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kIndexStorage);
    }
    QVERIFY(std::filesystem::is_directory(generated_index));
}

}  // namespace
}  // namespace goldendict::core::formats::stardict

using goldendict::core::formats::stardict::StardictReaderTest;

QTEST_APPLESS_MAIN(StardictReaderTest)

#include "stardict_reader_test.moc"
