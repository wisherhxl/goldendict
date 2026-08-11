// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>

#include "../src/formats/stardict/stardict_discovery.h"
#include "support/stardict_fixture.h"

namespace goldendict::core::formats::stardict {
namespace {

class StardictDiscoveryTest : public QObject {
    Q_OBJECT

   private slots:
    void FindsNestedInfoFilesInStableOrder();
    void AcceptsAnExplicitInfoFileAndDeduplicatesIt();
    void ReportsMissingRootsWithoutDiscardingValidResults();
};

std::filesystem::path TemporaryPath(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
}

void StardictDiscoveryTest::FindsNestedInfoFilesInStableOrder() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto nested = root / "nested";
    QVERIFY(std::filesystem::create_directory(nested));
    const auto nested_info =
        test::WriteStardictFixture(nested, {{"nested", "article"}});
    const auto root_info =
        test::WriteStardictFixture(root, {{"root", "article"}});
    test::WriteBinaryFile(root / "unrelated.txt", "not a dictionary");

    const DiscoveryResult result = Discover({root});

    QVERIFY(result.issues.empty());
    QCOMPARE(result.info_files.size(), std::size_t{2});
    QCOMPARE(result.info_files[0], root_info);
    QCOMPARE(result.info_files[1], nested_info);
}

void StardictDiscoveryTest::AcceptsAnExplicitInfoFileAndDeduplicatesIt() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});

    const DiscoveryResult result = Discover({root, info_path});

    QVERIFY(result.issues.empty());
    QCOMPARE(result.info_files, std::vector<std::filesystem::path>{info_path});
}

void StardictDiscoveryTest::ReportsMissingRootsWithoutDiscardingValidResults() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});

    const DiscoveryResult result = Discover({root / "missing", root});

    QCOMPARE(result.info_files, std::vector<std::filesystem::path>{info_path});
    QCOMPARE(result.issues.size(), std::size_t{1});
    QCOMPARE(result.issues.front().path, root / "missing");
    QVERIFY(!result.issues.front().message.empty());
}

}  // namespace
}  // namespace goldendict::core::formats::stardict

using goldendict::core::formats::stardict::StardictDiscoveryTest;

QTEST_APPLESS_MAIN(StardictDiscoveryTest)

#include "stardict_discovery_test.moc"
