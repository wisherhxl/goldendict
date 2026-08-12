// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/mdict/mdict_reader.h"
#include "support/mdict_fixture.h"

namespace goldendict::core::formats::mdict {

class MdictReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsStylesRedirectsAndMddResources();
    void DecodesUtf16KeysAndRecords();
    void RejectsCorruptionAndUnsupportedEncryption();
};

void MdictReaderTest::ReadsStylesRedirectsAndMddResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteMdictFixture(
        std::filesystem::path(directory.path().toStdString())));

    QCOMPARE(reader.metadata().name, "Fixture MDict");
    QCOMPARE(reader.metadata().description, "Fixture description");
    const auto exact = reader.LookupExact("EXAMPLE");
    QCOMPARE(exact.size(), std::size_t{1});
    QVERIFY(exact.front().data.find("<b>definition</b>") != std::string::npos);
    QCOMPARE(reader.LookupExact("alias").front().data, exact.front().data);
    QCOMPARE(reader.LookupPrefix("exa").size(), std::size_t{1});
    QCOMPARE(reader.SuggestPrefix("exa").front(), "example");
    QVERIFY(reader.LookupPrefix("exa", 0U).empty());
    QVERIFY(reader.SuggestPrefix("exa", 0U).empty());
    QVERIFY(reader.Resource("pixel.png") != nullptr);
    QCOMPARE(*reader.Resource("/pixel.png"), "mdict-png");
    QVERIFY(reader.Resource("../pixel.png") == nullptr);
}

void MdictReaderTest::DecodesUtf16KeysAndRecords() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path =
        test::WriteMdictContainer(root / "utf16.mdx", "UTF16 Fixture",
                                  {{"cafe", "<b>drink</b>"}}, false, true);

    const Reader reader = Reader::Open({path, {}});

    QCOMPARE(reader.LookupExact("CAFE").front().data, "<b>drink</b>");
}

void MdictReaderTest::RejectsCorruptionAndUnsupportedEncryption() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto corrupt = root / "corrupt.mdx";
    std::ofstream(corrupt, std::ios::binary) << "not-mdict";
    QVERIFY_EXCEPTION_THROWN(Reader::Open({corrupt, {}}), Error);

    const auto encrypted = test::WriteMdictContainer(
        root / "encrypted.mdx", "Encrypted Fixture", {{"word", "value"}}, true);
    try {
        (void)Reader::Open({encrypted, {}});
        QFAIL("encrypted MDict fixture was accepted");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kUnsupported);
    }
}

}  // namespace goldendict::core::formats::mdict

using goldendict::core::formats::mdict::MdictReaderTest;
QTEST_APPLESS_MAIN(MdictReaderTest)
#include "mdict_reader_test.moc"
