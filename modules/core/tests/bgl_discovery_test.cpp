// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "../src/formats/bgl/bgl_discovery.h"
#include "support/bgl_fixture.h"

namespace goldendict::core::formats::bgl {

class BglDiscoveryTest : public QObject {
    Q_OBJECT
   private slots:
    void DiscoversBglFilesWithoutDuplicates();
};

void BglDiscoveryTest::DiscoversBglFilesWithoutDuplicates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto dictionary = test::WriteBglFixture(root / "nested");
    std::ofstream(root / "ignored.txt") << "not a dictionary";

    const auto result = Discover({root, dictionary});

    QVERIFY(result.issues.empty());
    QCOMPARE(result.dictionary_files.size(), std::size_t{1});
    QCOMPARE(result.dictionary_files.front(), dictionary);
}

}  // namespace goldendict::core::formats::bgl

using goldendict::core::formats::bgl::BglDiscoveryTest;
QTEST_APPLESS_MAIN(BglDiscoveryTest)
#include "bgl_discovery_test.moc"
