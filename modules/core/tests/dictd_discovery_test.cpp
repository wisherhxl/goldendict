// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>

#include "../src/formats/dictd/dictd_discovery.h"
#include "support/dictd_fixture.h"

namespace goldendict::core::formats::dictd {

class DictdDiscoveryTest : public QObject {
    Q_OBJECT

   private slots:
    void DiscoversCompleteDictionariesAndReportsMissingData();
};

void DictdDiscoveryTest::DiscoversCompleteDictionariesAndReportsMissingData() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto complete = test::WriteDictdFixture(
        root / "nested", {{"example", "definition", {}}});
    std::ofstream(root / "missing.index") << "missing\tA\tA\n";

    const auto result = Discover({root, complete});

    QCOMPARE(result.index_files.size(), std::size_t{1});
    QCOMPARE(result.index_files.front(), complete);
    QCOMPARE(result.issues.size(), std::size_t{1});
    QCOMPARE(result.issues.front().path, root / "missing.index");
}

}  // namespace goldendict::core::formats::dictd

using goldendict::core::formats::dictd::DictdDiscoveryTest;
QTEST_APPLESS_MAIN(DictdDiscoveryTest)
#include "dictd_discovery_test.moc"
