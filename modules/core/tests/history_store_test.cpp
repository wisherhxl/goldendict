// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "goldendict/core/history_store.h"

namespace goldendict::core {
namespace {

class HistoryStoreTest : public QObject {
    Q_OBJECT

   private slots:
    void RoundTripsCurrentHistory();
    void MigratesBoundedLegacyHistoryWithoutChangingIt();
    void CurrentHistoryTakesPrecedence();
    void RejectsMalformedHistoryWithoutPartialMigration();
    void RejectsTruncatedOversizedAndInvalidUtf8LegacyHistory();
    void ImportsBoundedUtf8Text();
    void SupportsLegacyZeroAndMaximumLimits();
    void RejectsInvalidTextImport();
};

std::filesystem::path Path(const QTemporaryDir& directory, const char* name) {
    return std::filesystem::path(directory.path().toStdString()) / name;
}

void Write(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    output.close();
    QVERIFY(output.good());
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

void HistoryStoreTest::RoundTripsCurrentHistory() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = Path(directory, "history-v1");
    const std::vector<HistoryEntry> expected = {{7U, "café"},
                                                {0U, "two words"}};

    SaveHistory(path.string(), expected);

    QCOMPARE(LoadHistory(path.string()), expected);
}

void HistoryStoreTest::MigratesBoundedLegacyHistoryWithoutChangingIt() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto current = Path(directory, "history-v1");
    const auto legacy = Path(directory, "history");
    const std::string legacy_contents = "3 first word\n9 第二个\n12 ignored\n";
    Write(legacy, legacy_contents);

    const auto migrated =
        LoadOrMigrateHistory(current.string(), legacy.string(), 2U);

    QCOMPARE(migrated,
             (std::vector<HistoryEntry>{{3U, "first word"}, {9U, "第二个"}}));
    QCOMPARE(Read(legacy), legacy_contents);
    QCOMPARE(LoadHistory(current.string()), migrated);
}

void HistoryStoreTest::CurrentHistoryTakesPrecedence() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto current = Path(directory, "history-v1");
    const auto legacy = Path(directory, "history");
    const std::vector<HistoryEntry> expected = {{1U, "current"}};
    SaveHistory(current.string(), expected);
    Write(legacy, "malformed");

    QCOMPARE(LoadOrMigrateHistory(current.string(), legacy.string()), expected);
}

void HistoryStoreTest::RejectsMalformedHistoryWithoutPartialMigration() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto current = Path(directory, "history-v1");
    const auto legacy = Path(directory, "history");
    Write(legacy, "not-a-group word\n");

    QVERIFY_EXCEPTION_THROWN(
        LoadOrMigrateHistory(current.string(), legacy.string()),
        std::runtime_error);
    QVERIFY(!std::filesystem::exists(current));
    QVERIFY(!std::filesystem::exists(current.string() + ".tmp"));
    QVERIFY(std::filesystem::exists(legacy));
}

void HistoryStoreTest::RejectsTruncatedOversizedAndInvalidUtf8LegacyHistory() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto current = Path(directory, "history-v1");
    const auto legacy = Path(directory, "history");
    const auto reject = [&](std::string_view contents) {
        Write(legacy, contents);
        QVERIFY_EXCEPTION_THROWN(
            LoadOrMigrateHistory(current.string(), legacy.string()),
            std::runtime_error);
        QVERIFY(!std::filesystem::exists(current));
        QVERIFY(!std::filesystem::exists(current.string() + ".tmp"));
    };

    reject("1 unterminated");
    reject(std::string("1 invalid\xFF\n", 11U));
    reject(std::string(1024U * 1024U + 1U, 'x'));
}

void HistoryStoreTest::ImportsBoundedUtf8Text() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = Path(directory, "history.txt");
    Write(path, "\xEF\xBB\xBF  first  \r\n\n第二个\nignored\n");

    QCOMPARE(ImportHistoryText(path.string(), 2U, 7U),
             (std::vector<HistoryEntry>{{7U, "first"}, {7U, "第二个"}}));
}

void HistoryStoreTest::SupportsLegacyZeroAndMaximumLimits() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = Path(directory, "history.txt");
    Write(path, "first\nsecond\n");

    QVERIFY(ImportHistoryText(path.string(), 0U, 7U).empty());
    QCOMPARE(ImportHistoryText(path.string(), 99999U, 7U).size(),
             std::size_t{2});
    QVERIFY_EXCEPTION_THROWN(ImportHistoryText(path.string(), 100000U, 7U),
                             std::runtime_error);
}

void HistoryStoreTest::RejectsInvalidTextImport() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = Path(directory, "history.txt");
    Write(path, std::string("invalid\xFF\n", 9U));

    QVERIFY_EXCEPTION_THROWN(ImportHistoryText(path.string()),
                             std::runtime_error);
}

}  // namespace
}  // namespace goldendict::core

using goldendict::core::HistoryStoreTest;

QTEST_APPLESS_MAIN(HistoryStoreTest)

#include "history_store_test.moc"
