// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/slob/slob_discovery.h"
#include <QtTest>
#include <filesystem>
#include "support/slob_fixture.h"

namespace goldendict::core::formats::slob {
class SlobDiscoveryTest : public QObject {
    Q_OBJECT
   private slots:
    void FindsSlobRecursively();
};

void SlobDiscoveryTest::FindsSlobRecursively() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    test::WriteSlobFixture(root / "nested");
    const auto result = Discover({root, root});
    QCOMPARE(result.dictionary_files.size(), std::size_t{1});
    QVERIFY(result.issues.empty());
}
}  // namespace goldendict::core::formats::slob

using goldendict::core::formats::slob::SlobDiscoveryTest;
QTEST_APPLESS_MAIN(SlobDiscoveryTest)
#include "slob_discovery_test.moc"
