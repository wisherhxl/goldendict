// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "../src/formats/stardict/stardict_reader.h"
#include "support/stardict_fixture.h"

namespace goldendict::core::formats::stardict {
namespace {

class StardictReaderTest : public QObject {
    Q_OBJECT

   private slots:
    void ReadsMetadataAndExactArticles();
    void MatchesFoldEquivalentHeadwords();
    void RanksFoldedPrefixMatches();
    void SuggestsDistinctRankedHeadwords();
    void ReadsSynonymsAndPreservesPrimaryHeadwordsInGeneratedIndex();
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
    void RejectsTruncatedIndex();
    void RejectsArticleOutsideDictionaryData();
    void ReportsMissingCompanionFile();
    void ReadsCompressedDictionary();
    void PrefersUncompressedDictionary();
    void RejectsCorruptCompressedDictionary();
    void RejectsTruncatedCompressedDictionary();
    void RejectsOversizedCompressedDictionary();
    void RebuildsGeneratedIndexWhenCompressedSourceChanges();
    void CreatesAndReusesGeneratedIndex();
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

    QCOMPARE(reader.metadata().book_name, "Generated Test Dictionary");
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

void StardictReaderTest::RejectsTruncatedIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "A test definition."}});
    test::WriteBinaryFile(root / "fixture.idx", "example\0\0\0");
    std::string info =
        "StarDict's dict ifo file\nversion=2.4.2\n"
        "bookname=Generated Test Dictionary\nwordcount=1\n"
        "idxfilesize=10\nsametypesequence=m\n";
    test::WriteBinaryFile(info_path, info);

    try {
        static_cast<void>(Reader::Open(info_path));
        QFAIL("Reader::Open should reject a truncated index");
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
