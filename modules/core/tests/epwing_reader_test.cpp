// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/epwing/epwing_reader.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include "support/epwing_fixture.h"

namespace goldendict::core::formats::epwing {
class EpwingReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsCatalogIndexTextReferencesAndResources();
    void DecodesDefaultJisX0208();
    void ExposesPhysicalOwnershipAndOrderedRevision();
    void RevisionTracksSelectedTreeOnly();
    void OrdersMultipleSubbooksAndTextFiles();
    void CancelsWithoutPublishingAView();
    void RejectsCorruption();
};

void EpwingReaderTest::ReadsCatalogIndexTextReferencesAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Reader reader = Reader::Open(test::WriteEpwingFixture(root));
    QCOMPARE(reader.metadata().name, "Fixture EPWING");
    const auto articles = reader.LookupExact("EXAMPLE");
    QCOMPARE(articles.size(), std::size_t{1});
    QVERIFY(articles.front().data.find("definition") != std::string::npos);
    QVERIFY(articles.front().data.find("bword://second") != std::string::npos);
    QCOMPARE(reader.LookupPrefix("sec").front().headword, "second");
    QCOMPARE(reader.SuggestPrefix("exa").front(), "example");
    QCOMPARE(*reader.Resource("FIXTURE/GAIJI/pixel.png"), "png-data");
}

void EpwingReaderTest::ExposesPhysicalOwnershipAndOrderedRevision() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Reader reader = Reader::Open(test::WriteEpwingOwnershipFixture(root));
    const auto& view = reader.ingestion_view();
    QCOMPARE(view.records.size(), std::size_t{12});
    QCOMPARE(view.articles.size(), std::size_t{11});
    QCOMPARE(view.records[0].record_ordinal, std::size_t{0});
    QCOMPARE(view.records[1].article_ordinal, std::size_t{0});
    QCOMPARE(view.articles[0].headword, "owner");
    QCOMPARE(view.articles[0].aliases, std::vector<std::string>{"alias"});
    QCOMPARE(view.articles[0].physical.text_file_ordinal, std::size_t{0});
    QCOMPARE(view.articles[0].physical.page, std::uint32_t{5});
    QCOMPARE(view.articles[0].physical.offset, std::uint16_t{0});
    QVERIFY(view.articles[1].html == view.articles[2].html);
    QVERIFY(!(view.articles[1].physical == view.articles[2].physical));
    QCOMPARE(view.records[10].record_ordinal, std::size_t{10});

    QCOMPARE(view.source_snapshot.size(), std::size_t{5});
    QVERIFY(view.source_snapshot[0].path.find("/CATALOGS") !=
            std::string::npos);
    QVERIFY(view.source_snapshot[1].path.find("/LANGUAGE") !=
            std::string::npos);
    QVERIFY(view.source_snapshot[2].path.find("/OWNER/DATA/HONMON") !=
            std::string::npos);
    QVERIFY(view.source_snapshot[3].path.find("/OWNER/GAIJI/a.bin") !=
            std::string::npos);
    QVERIFY(view.source_snapshot[4].path.find("/OWNER/GAIJI/z.bin") !=
            std::string::npos);
}

void EpwingReaderTest::CancelsWithoutPublishingAView() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto catalog = test::WriteEpwingOwnershipFixture(root);
    std::size_t checkpoints = 0;
    QVERIFY_EXCEPTION_THROWN(
        Reader::Open(catalog,
                     [&checkpoints]() {
                         if (++checkpoints == 5U)
                             throw std::runtime_error("cancelled");
                     }),
        std::runtime_error);
}

void EpwingReaderTest::RevisionTracksSelectedTreeOnly() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto catalog = test::WriteEpwingOwnershipFixture(root);
    const auto original =
        Reader::Open(catalog).ingestion_view().source_snapshot;

    test::EpwingWrite(root / "UNSELECTED" / "other.bin", "unrelated");
    QCOMPARE(Reader::Open(catalog).ingestion_view().source_snapshot, original);

    test::EpwingWrite(root / "OWNER" / "GAIJI" / "resource.bin",
                      "resource-only");
    const auto added = Reader::Open(catalog).ingestion_view().source_snapshot;
    QVERIFY(added != original);
    QVERIFY(std::filesystem::remove(root / "OWNER" / "GAIJI" / "resource.bin"));
    QCOMPARE(Reader::Open(catalog).ingestion_view().source_snapshot, original);

    test::EpwingWrite(root / "OWNER" / "GAIJI" / "a.bin",
                      "replacement-with-new-size");
    QVERIFY(Reader::Open(catalog).ingestion_view().source_snapshot != original);

    std::error_code error;
    std::filesystem::create_symlink(root / "UNSELECTED" / "other.bin",
                                    root / "OWNER" / "GAIJI" / "linked.bin",
                                    error);
    if (!error) {
        const auto snapshot =
            Reader::Open(catalog).ingestion_view().source_snapshot;
        for (const auto& stamp : snapshot)
            QVERIFY(stamp.path.find("linked.bin") == std::string::npos);
    }
}

void EpwingReaderTest::OrdersMultipleSubbooksAndTextFiles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Reader reader = Reader::Open(test::WriteEpwingMultiBookFixture(root));
    const auto& view = reader.ingestion_view();
    QCOMPARE(view.records.size(), std::size_t{2});
    QCOMPARE(view.records[0].headword, "book0");
    QCOMPARE(view.records[0].physical.text_file_ordinal, std::size_t{0});
    QCOMPARE(view.records[1].headword, "book1");
    QCOMPARE(view.records[1].physical.text_file_ordinal, std::size_t{1});
    QCOMPARE(view.source_snapshot.size(), std::size_t{6});
    QVERIFY(view.source_snapshot[2].path.find("/FIRST/DATA/HONMON") !=
            std::string::npos);
    QVERIFY(view.source_snapshot[3].path.find("/FIRST/asset.bin") !=
            std::string::npos);
    QVERIFY(view.source_snapshot[4].path.find("/SECOND/DATA/HONMON") !=
            std::string::npos);
    QVERIFY(view.source_snapshot[5].path.find("/SECOND/asset.bin") !=
            std::string::npos);
}

void EpwingReaderTest::DecodesDefaultJisX0208() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Reader reader = Reader::Open(test::WriteEpwingFixture(root, false));
    QCOMPARE(reader.metadata().name, std::string(u8"日本"));
    const auto articles = reader.LookupExact(u8"日本");
    QCOMPARE(articles.size(), std::size_t{1});
    QVERIFY(articles.front().data.find(u8"定義") != std::string::npos);
}

void EpwingReaderTest::RejectsCorruption() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path =
        std::filesystem::path(directory.path().toStdString()) / "CATALOGS";
    std::ofstream(path, std::ios::binary) << "broken";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(path), Error);
}
}  // namespace goldendict::core::formats::epwing

using goldendict::core::formats::epwing::EpwingReaderTest;
QTEST_APPLESS_MAIN(EpwingReaderTest)
#include "epwing_reader_test.moc"
