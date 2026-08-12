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
