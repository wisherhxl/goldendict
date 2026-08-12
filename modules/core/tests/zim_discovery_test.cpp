// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/zim/zim_discovery.h"
#include <QtTest>
#include <filesystem>
#include <fstream>

namespace goldendict::core::formats::zim {
class ZimDiscoveryTest : public QObject {
    Q_OBJECT
   private slots:
    void FindsSingleAndConsecutiveSplitFiles();
};

void ZimDiscoveryTest::FindsSingleAndConsecutiveSplitFiles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    std::ofstream(root / "single.zim") << "x";
    std::ofstream(root / "split.zimaa") << "a";
    std::ofstream(root / "split.zimab") << "b";
    std::ofstream(root / "split.zimac") << "c";
    const auto result = Discover({root});
    QCOMPARE(result.dictionaries.size(), std::size_t{2});
    const auto split =
        std::find_if(result.dictionaries.begin(), result.dictionaries.end(),
                     [](const Files& files) {
                         return files.primary.extension() == ".zimaa";
                     });
    QVERIFY(split != result.dictionaries.end());
    QCOMPARE(split->parts.size(), std::size_t{3});
    QVERIFY(result.issues.empty());
}
}  // namespace goldendict::core::formats::zim

using goldendict::core::formats::zim::ZimDiscoveryTest;
QTEST_APPLESS_MAIN(ZimDiscoveryTest)
#include "zim_discovery_test.moc"
