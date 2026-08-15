// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/gls/gls_reader.h"
#include "support/gls_fixture.h"

namespace goldendict::core::formats::gls {

class GlsReaderTest : public QObject {
    Q_OBJECT

   private slots:
    void ReadsMetadataAliasesRankedMatchesAndHtml();
    void ReadsCompressedAndUtf16TextAndInvokesCheckpoints();
    void RejectsMalformedOrCorruptInput();
};

void GlsReaderTest::ReadsMetadataAliasesRankedMatchesAndHtml() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteGlsFixture(
        root, {{{"Café", "coffee"}, "<b>drink</b>\n<img src=\"cup.png\">"},
               {{"cafeteria"}, "place"}});

    const Reader reader = Reader::Open(path);
    const auto exact = reader.LookupExact("CAFE");
    const auto prefix = reader.LookupPrefix("caf");
    const auto suggestions = reader.SuggestPrefix("caf");

    QCOMPARE(reader.metadata().name, "Fixture GLS");
    QCOMPARE(reader.metadata().source_language, "eng");
    QCOMPARE(reader.metadata().target_language, "deu");
    QCOMPARE(exact.size(), std::size_t{1});
    QVERIFY(exact.front().data.find("<b>drink</b>") != std::string::npos);
    QCOMPARE(prefix.size(), std::size_t{2});
    QCOMPARE(suggestions.front(), "Café");
    QCOMPARE(reader.FindHeadwordsForSynonym("coffee", 20U),
             std::vector<std::string>{"Café"});
    QVERIFY(reader.FindHeadwordsForSynonym("Café", 20U).empty());
}

void GlsReaderTest::ReadsCompressedAndUtf16TextAndInvokesCheckpoints() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Reader compressed =
        Reader::Open(test::CompressGlsFixture(test::WriteGlsFixture(
            root / "compressed", {{{"example"}, "definition"}})));
    int checkpoints = 0;
    QCOMPARE(
        compressed
            .LookupExact("example", 1U, [&checkpoints]() { ++checkpoints; })
            .front()
            .headword,
        "example");
    QVERIFY(checkpoints > 0);

    const Reader utf16 =
        Reader::Open(test::WriteUtf16LeGlsFixture(root / "utf16"));
    QCOMPARE(utf16.metadata().name, "UTF16 GLS");
    QCOMPARE(utf16.LookupExact("example").front().headword, "example");
}

void GlsReaderTest::RejectsMalformedOrCorruptInput() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto missing_section = root / "missing.gls";
    std::ofstream(missing_section)
        << "### Glossary title: Invalid\nword\nvalue";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(missing_section), Error);

    const auto corrupt = root / "corrupt.gls.dz";
    std::ofstream(corrupt, std::ios::binary) << "not gzip data";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(corrupt), Error);

    const auto invalid_utf8 = root / "invalid.gls";
    std::ofstream(invalid_utf8, std::ios::binary)
        << "### Glossary section:\n\nword\n\xff\n";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(invalid_utf8), Error);
}

}  // namespace goldendict::core::formats::gls

using goldendict::core::formats::gls::GlsReaderTest;
QTEST_APPLESS_MAIN(GlsReaderTest)
#include "gls_reader_test.moc"
