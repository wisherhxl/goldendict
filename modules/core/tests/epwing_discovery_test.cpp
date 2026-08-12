// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/epwing/epwing_discovery.h"
#include <QtTest>
#include <filesystem>
#include "support/epwing_fixture.h"

namespace goldendict::core::formats::epwing {
class EpwingDiscoveryTest : public QObject {
    Q_OBJECT
   private slots:
    void FindsCatalogsRecursively();
};

void EpwingDiscoveryTest::FindsCatalogsRecursively() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    test::WriteEpwingFixture(root / "nested");
    const auto result = Discover({root, root});
    QCOMPARE(result.catalog_files.size(), std::size_t{1});
    QVERIFY(result.issues.empty());
}
}  // namespace goldendict::core::formats::epwing

using goldendict::core::formats::epwing::EpwingDiscoveryTest;
QTEST_APPLESS_MAIN(EpwingDiscoveryTest)
#include "epwing_discovery_test.moc"
