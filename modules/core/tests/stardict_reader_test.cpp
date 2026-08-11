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
    void ReturnsNoArticleForMissingHeadword();
    void RejectsInvalidInfoSignature();
    void RejectsInvalidNumericMetadata();
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
