// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/zipsounds/zipsounds_discovery.h"
#include <QtTest>
#include "support/zipsounds_fixture.h"

namespace goldendict::core::formats::zipsounds {
class ZipSoundsDiscoveryTest final : public QObject {
    Q_OBJECT
   private slots:
    void DiscoversPacksRecursively();
};

void ZipSoundsDiscoveryTest::DiscoversPacksRecursively() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    test::WriteZipSoundsFixture(root / "nested", "audio.ZIPS");
    const auto result = Discover({root});
    QCOMPARE(result.dictionary_files.size(), 1U);
    QVERIFY(result.issues.empty());
}
}  // namespace goldendict::core::formats::zipsounds

using goldendict::core::formats::zipsounds::ZipSoundsDiscoveryTest;
QTEST_MAIN(ZipSoundsDiscoveryTest)
#include "zipsounds_discovery_test.moc"
