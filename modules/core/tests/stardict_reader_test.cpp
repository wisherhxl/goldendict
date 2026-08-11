// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

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

}  // namespace
}  // namespace goldendict::core::formats::stardict

using goldendict::core::formats::stardict::StardictReaderTest;

QTEST_APPLESS_MAIN(StardictReaderTest)

#include "stardict_reader_test.moc"
