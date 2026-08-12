// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/aard/aard_discovery.h"
#include <QtTest>
#include <filesystem>
#include "support/aard_fixture.h"

namespace goldendict::core::formats::aard {
class AardDiscoveryTest : public QObject {
    Q_OBJECT
   private slots:
    void FindsAardRecursivelyAndDeduplicates();
};

void AardDiscoveryTest::FindsAardRecursivelyAndDeduplicates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteAardFixture(root / "nested", "fixture.AAR");
    const auto result = Discover({root, path});
    QCOMPARE(result.dictionary_files.size(), std::size_t{1});
    QCOMPARE(result.dictionary_files.front(), path);
    QVERIFY(result.issues.empty());
}
}  // namespace goldendict::core::formats::aard

using goldendict::core::formats::aard::AardDiscoveryTest;
QTEST_APPLESS_MAIN(AardDiscoveryTest)
#include "aard_discovery_test.moc"
