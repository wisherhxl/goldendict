// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/lsa/lsa_discovery.h"
#include <QtTest>
#include "support/lsa_fixture.h"

namespace goldendict::core::formats::lsa {
class LsaDiscoveryTest final : public QObject {
    Q_OBJECT
   private slots:
    void DiscoversDatAndLsaRecursively();
};

void LsaDiscoveryTest::DiscoversDatAndLsaRecursively() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    test::WriteLsaFixture(root / "nested", "fixture.LSA");
    test::WriteLsaFixture(root, "audio.dat");
    const auto result = Discover({root});
    QCOMPARE(result.dictionary_files.size(), 2U);
    QVERIFY(result.issues.empty());
}
}  // namespace goldendict::core::formats::lsa

using goldendict::core::formats::lsa::LsaDiscoveryTest;
QTEST_MAIN(LsaDiscoveryTest)
#include "lsa_discovery_test.moc"
