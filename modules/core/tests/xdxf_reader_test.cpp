// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/xdxf/xdxf_reader.h"
#include "support/xdxf_fixture.h"

namespace goldendict::core::formats::xdxf {

class XdxfReaderTest : public QObject {
    Q_OBJECT

   private slots:
    void ReadsMetadataAliasesRankedMatchesAndMarkup();
    void ReadsCompressedXmlAndInvokesCheckpoints();
    void RejectsCorruptCompressedXml();
    void ReadsStandardDocumentTypeAndRejectsMalformedInput();
};

void XdxfReaderTest::ReadsMetadataAliasesRankedMatchesAndMarkup() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path =
        test::WriteXdxfFixture(root, {{{"Café", "coffee"},
                                       "<def><b>drink</b> <kref>tea</kref> "
                                       "<rref>images/cup.png</rref></def>"},
                                      {{"cafeteria"}, "<def>place</def>"}});

    const Reader reader = Reader::Open(path);
    const auto exact = reader.LookupExact("CAFE");
    const auto prefix = reader.LookupPrefix("caf");
    const auto suggestions = reader.SuggestPrefix("caf");

    QCOMPARE(reader.metadata().name, "Fixture XDXF");
    QCOMPARE(reader.metadata().source_language, "eng");
    QCOMPARE(reader.metadata().target_language, "deu");
    QCOMPARE(exact.size(), std::size_t{1});
    QVERIFY(exact.front().data.find("bword://tea") != std::string::npos);
    QVERIFY(exact.front().data.find("images/cup.png") != std::string::npos);
    QCOMPARE(prefix.size(), std::size_t{2});
    QCOMPARE(suggestions.front(), "Café");
}

void XdxfReaderTest::ReadsCompressedXmlAndInvokesCheckpoints() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::CompressXdxfFixture(
        test::WriteXdxfFixture(root, {{{"example"}, "<def>definition</def>"}}));
    const Reader reader = Reader::Open(path);
    int checkpoints = 0;

    const auto result =
        reader.LookupExact("example", 1U, [&checkpoints]() { ++checkpoints; });

    QCOMPARE(result.front().headword, "example");
    QVERIFY(checkpoints > 0);
}

void XdxfReaderTest::RejectsCorruptCompressedXml() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path =
        std::filesystem::path(directory.path().toStdString()) / "bad.xdxf.dz";
    std::ofstream(path, std::ios::binary) << "not gzip data";

    QVERIFY_EXCEPTION_THROWN(Reader::Open(path), Error);
}

void XdxfReaderTest::ReadsStandardDocumentTypeAndRejectsMalformedInput() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto malformed = root / "malformed.xdxf";
    std::ofstream(malformed) << "<xdxf><ar><k>word</k></xdxf>";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(malformed), Error);

    const auto invalid = root / "invalid.xdxf";
    std::ofstream(invalid, std::ios::binary) << "<xdxf>\xff</xdxf>";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(invalid), Error);

    const auto dtd = root / "dtd.xdxf";
    std::ofstream(dtd)
        << "<!DOCTYPE xdxf SYSTEM \"https://xdxf.example/xdxf.dtd\">"
           "<xdxf><full_name>DTD fixture</full_name><languages>"
           "<from xml:lang=\"en-US\"/><to xml:lang=\"de-DE\"/>"
           "</languages><ar><k>word</k><def>value</def></ar></xdxf>";
    const Reader reader = Reader::Open(dtd);
    QCOMPARE(reader.metadata().source_language, "en");
    QCOMPARE(reader.metadata().target_language, "de");
    QCOMPARE(reader.LookupExact("word").front().headword, "word");
}

}  // namespace goldendict::core::formats::xdxf

using goldendict::core::formats::xdxf::XdxfReaderTest;
QTEST_APPLESS_MAIN(XdxfReaderTest)
#include "xdxf_reader_test.moc"
