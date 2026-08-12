// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/aard/aard_reader.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include "support/aard_fixture.h"

namespace goldendict::core::formats::aard {
class AardReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsMetadataAliasesArticlesAndRedirects();
    void ReadsBzip2Payloads();
    void ReadsLegacyRawArticles();
    void RejectsCorruption();
};

void AardReaderTest::ReadsMetadataAliasesArticlesAndRedirects() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteAardFixture(
        std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(reader.metadata().name, "Fixture Aard");
    QCOMPARE(reader.metadata().source_language, "en");
    QCOMPARE(reader.metadata().target_language, "de");
    QCOMPARE(reader.metadata().article_count, std::size_t{2});
    QCOMPARE(reader.LookupExact("EXAMPLE").size(), std::size_t{1});
    QCOMPARE(reader.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(reader.SuggestPrefix("exa").front(), "example");
    QVERIFY(reader.LookupPrefix("exa", 0U).empty());
    QVERIFY(reader.LookupExact("example").front().data.find(
                "<b>definition</b>") != std::string::npos);
    QVERIFY(reader.LookupExact("example").front().data.find("bword://alias") !=
            std::string::npos);
    QVERIFY(
        reader.LookupExact("redirect").front().data.find("bword://example") !=
        std::string::npos);
}

void AardReaderTest::ReadsBzip2Payloads() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteAardFixture(
        std::filesystem::path(directory.path().toStdString()), "bzip.aar", true,
        true));
    QCOMPARE(reader.LookupExact("example").size(), std::size_t{1});
    QVERIFY(reader.LookupExact("example").front().data.find("definition") !=
            std::string::npos);
}

void AardReaderTest::ReadsLegacyRawArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteAardFixture(
        std::filesystem::path(directory.path().toStdString()), "raw.aar", false,
        false, true));
    QCOMPARE(reader.LookupExact("example").size(), std::size_t{1});
}

void AardReaderTest::RejectsCorruption() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path =
        std::filesystem::path(directory.path().toStdString()) / "bad.aar";
    std::ofstream(path, std::ios::binary) << "not aard";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(path), Error);
}
}  // namespace goldendict::core::formats::aard

using goldendict::core::formats::aard::AardReaderTest;
QTEST_APPLESS_MAIN(AardReaderTest)
#include "aard_reader_test.moc"
