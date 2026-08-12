// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>

#include "../src/formats/sdict/sdict_discovery.h"
#include "support/sdict_fixture.h"

namespace goldendict::core::formats::sdict {

class SdictDiscoveryTest : public QObject {
    Q_OBJECT

   private slots:
    void DiscoversDctFilesRecursivelyAndDeduplicatesRoots();
};

void SdictDiscoveryTest::DiscoversDctFilesRecursivelyAndDeduplicatesRoots() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto dictionary =
        test::WriteSdictFixture(root / "nested", {{"example", "definition"}});
    std::ofstream(root / "ignored.txt") << "not a dictionary";

    const auto result = Discover({root, dictionary});

    QVERIFY(result.issues.empty());
    QCOMPARE(result.dictionary_files.size(), std::size_t{1});
    QCOMPARE(result.dictionary_files.front(), dictionary);
}

}  // namespace goldendict::core::formats::sdict

using goldendict::core::formats::sdict::SdictDiscoveryTest;
QTEST_APPLESS_MAIN(SdictDiscoveryTest)
#include "sdict_discovery_test.moc"
