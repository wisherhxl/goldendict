// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/mdict/mdict_discovery.h"
#include "../src/formats/mdict/mdict_reader.h"
#include "support/mdict_fixture.h"

namespace goldendict::core::formats::mdict {

class MdictDiscoveryTest : public QObject {
    Q_OBJECT
   private slots:
    void DiscoversMdxWithMddCompanionsWithoutDuplicates();
    void TracksExactConsecutiveCompanionRevision();
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

void MdictDiscoveryTest::TracksExactConsecutiveCompanionRevision() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto files = test::WriteMdictFixture(root);
    test::WriteMdictContainer(root / "fixture.2.mdd", "Skipped Volume",
                              {{"\\skipped", "skipped"}});

    auto discovered = Discover({root});
    QCOMPARE(discovered.dictionaries.front().mdd.size(), std::size_t{1});
    const auto original =
        Reader::Open(discovered.dictionaries.front()).ReadIngestionView();

    test::WriteMdictContainer(
        files.mdx, "Fixture MDict Changed",
        {{"alias", "@@@LINK=example"}, {"example", "changed definition"}});
    const auto mdx_mutated =
        Reader::Open(Discover({root}).dictionaries.front()).ReadIngestionView();
    QVERIFY(!(original.source_snapshot == mdx_mutated.source_snapshot));

    test::WriteMdictContainer(root / "fixture.1.mdd", "First Volume",
                              {{"\\first", "first"}});
    discovered = Discover({root});
    QCOMPARE(discovered.dictionaries.front().mdd.size(), std::size_t{3});
    const auto added =
        Reader::Open(discovered.dictionaries.front()).ReadIngestionView();
    QVERIFY(!(mdx_mutated.source_snapshot == added.source_snapshot));
    QCOMPARE(added.records.size(), mdx_mutated.records.size());
    QCOMPARE(added.articles.size(), mdx_mutated.articles.size());

    test::WriteMdictContainer(root / "fixture.mdd", "Resources Changed",
                              {{"\\pixel.png", "changed base resource"}});
    const auto base_mutated =
        Reader::Open(Discover({root}).dictionaries.front()).ReadIngestionView();
    QVERIFY(!(added.source_snapshot == base_mutated.source_snapshot));

    test::WriteMdictContainer(root / "fixture.1.mdd", "First Volume Changed",
                              {{"\\first", "changed resource bytes"}});
    const auto mutated =
        Reader::Open(Discover({root}).dictionaries.front()).ReadIngestionView();
    QVERIFY(!(base_mutated.source_snapshot == mutated.source_snapshot));
    QCOMPARE(mutated.articles.size(), mdx_mutated.articles.size());

    test::WriteMdictContainer(root / "fixture.2.mdd", "Second Volume Changed",
                              {{"\\skipped", "changed second resource"}});
    const auto second_mutated =
        Reader::Open(Discover({root}).dictionaries.front()).ReadIngestionView();
    QVERIFY(!(mutated.source_snapshot == second_mutated.source_snapshot));

    QVERIFY(std::filesystem::remove(root / "fixture.1.mdd"));
    discovered = Discover({root});
    QCOMPARE(discovered.dictionaries.front().mdd.size(), std::size_t{1});
    const auto removed =
        Reader::Open(discovered.dictionaries.front()).ReadIngestionView();
    QVERIFY(!(removed.source_snapshot == second_mutated.source_snapshot));
    QCOMPARE(removed.source_snapshot.size(), std::size_t{2});
}

}  // namespace goldendict::core::formats::mdict

using goldendict::core::formats::mdict::MdictDiscoveryTest;
QTEST_APPLESS_MAIN(MdictDiscoveryTest)
#include "mdict_discovery_test.moc"
