// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>

#include "../src/formats/mdict/mdict_discovery.h"
#include "support/mdict_fixture.h"

namespace goldendict::core::formats::mdict {

class MdictDiscoveryTest : public QObject {
    Q_OBJECT
   private slots:
    void DiscoversMdxWithMddCompanionsWithoutDuplicates();
};

void MdictDiscoveryTest::DiscoversMdxWithMddCompanionsWithoutDuplicates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto files = test::WriteMdictFixture(root / "nested");
    test::WriteMdictContainer(root / "nested" / "fixture.1.mdd", "Volume",
                              {{"\\second.png", "second"}});

    const auto result = Discover({root, files.mdx});

    QVERIFY(result.issues.empty());
    QCOMPARE(result.dictionaries.size(), std::size_t{1});
    QCOMPARE(result.dictionaries.front().mdx, files.mdx);
    QCOMPARE(result.dictionaries.front().mdd.size(), std::size_t{2});
}

}  // namespace goldendict::core::formats::mdict

using goldendict::core::formats::mdict::MdictDiscoveryTest;
QTEST_APPLESS_MAIN(MdictDiscoveryTest)
#include "mdict_discovery_test.moc"
