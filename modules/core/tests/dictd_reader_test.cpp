// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>

#include "../src/formats/dictd/dictd_reader.h"
#include "support/dictd_fixture.h"

namespace goldendict::core::formats::dictd {

class DictdReaderTest : public QObject {
    Q_OBJECT

   private slots:
    void ReadsMetadataAliasesAndRankedMatches();
    void ReadsCompressedData();
    void InvokesScanCheckpoints();
    void RejectsCorruptCompressedData();
    void RejectsMalformedBase64AndOutOfRangeArticles();
};

void DictdReaderTest::ReadsMetadataAliasesAndRankedMatches() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"00databaseshort", "00databaseshort\nFixture Dictd\n", {}},
               {"cafeteria", "long", {}},
               {"Caf\xc3\xa9", "exact", "cafe-original"}});

    const Reader reader = Reader::Open(index);

    QCOMPARE(reader.name(), "Fixture Dictd");
    QCOMPARE(reader.LookupExact("CAFE").front().data, "exact");
    QCOMPARE(reader.LookupExact("cafe original").front().data, "exact");
    const auto prefix = reader.LookupPrefix("CAFE", 2U);
    QCOMPARE(prefix.size(), std::size_t{2});
    QCOMPARE(prefix[0].data, "exact");
    QCOMPARE(prefix[1].data, "long");
    const auto suggestions = reader.SuggestPrefix("CAFE", 3U);
    QCOMPARE(suggestions.size(), std::size_t{3});
    QCOMPARE(suggestions.front(), "Caf\xc3\xa9");
}

void DictdReaderTest::ReadsCompressedData() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"example", "compressed definition", {}}});
    test::CompressDictdFixture(index);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));

    const Reader reader = Reader::Open(index);

    QCOMPARE(reader.LookupExact("example").front().data,
             "compressed definition");
}

void DictdReaderTest::InvokesScanCheckpoints() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"example", "definition", {}}});
    const Reader reader = Reader::Open(index);
    std::size_t checkpoints = 0;

    const auto result =
        reader.LookupPrefix("exa", 1U, [&checkpoints]() { ++checkpoints; });

    QCOMPARE(result.size(), std::size_t{1});
    QCOMPARE(checkpoints, std::size_t{1});
}

void DictdReaderTest::RejectsCorruptCompressedData() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"example", "definition", {}}});
    const auto compressed = test::CompressDictdFixture(index);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    std::ofstream(compressed, std::ios::binary | std::ios::trunc) << "not-gzip";

    try {
        static_cast<void>(Reader::Open(index));
        QFAIL("Corrupt compressed Dictd data should fail");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
    }
}

void DictdReaderTest::RejectsMalformedBase64AndOutOfRangeArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"example", "definition", {}}});
    std::ofstream(index, std::ios::binary | std::ios::trunc)
        << "example\t!\tA\n";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(index), Error);

    std::ofstream(index, std::ios::binary | std::ios::trunc)
        << "example\tA\t/\n";
    try {
        static_cast<void>(Reader::Open(index));
        QFAIL("Out-of-range Dictd article should fail");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
    }
}

}  // namespace goldendict::core::formats::dictd

using goldendict::core::formats::dictd::DictdReaderTest;
QTEST_APPLESS_MAIN(DictdReaderTest)
#include "dictd_reader_test.moc"
