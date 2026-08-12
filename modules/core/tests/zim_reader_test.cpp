// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/zim/zim_reader.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include "support/zim_fixture.h"

namespace goldendict::core::formats::zim {
class ZimReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsArticlesRedirectsMetadataAndResources();
    void ReadsCompressedAndWideClusters();
    void ReadsSplitArchives();
    void RejectsCorruption();
};

void ZimReaderTest::ReadsArticlesRedirectsMetadataAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = test::WriteZimFixture(
        std::filesystem::path(directory.path().toStdString()));
    const Reader reader = Reader::Open({path, {path}});
    QCOMPARE(reader.metadata().name, "Fixture ZIM");
    QCOMPARE(reader.metadata().source_language, "en");
    QCOMPARE(reader.LookupExact("example").front().data,
             "<b>definition</b><img src=\"I/pixel.png\">");
    QCOMPARE(reader.LookupExact("ALIAS").front().headword, "Alias");
    QCOMPARE(reader.SuggestPrefix("ex").front(), "Example");
    QCOMPARE(*reader.Resource("I/pixel.png"), "png-data");
    QCOMPARE(reader.LookupExact("plain").front().data,
             "<pre>plain &lt;unsafe&gt;</pre>");
}

void ZimReaderTest::ReadsCompressedAndWideClusters() {
    for (const unsigned compression : {2U, 3U}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = test::WriteZimFixture(
            std::filesystem::path(directory.path().toStdString()),
            "fixture.zim", compression, compression == 3U);
        QCOMPARE(Reader::Open({path, {path}}).LookupExact("alias").size(),
                 std::size_t{1});
    }
}

void ZimReaderTest::ReadsSplitArchives() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto source = test::WriteZimFixture(root);
    std::ifstream input(source, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    const auto first = root / "fixture.zimaa";
    const auto second = root / "fixture.zimab";
    const auto middle = data.size() / 2U;
    std::ofstream(first, std::ios::binary)
        .write(data.data(), static_cast<std::streamsize>(middle));
    std::ofstream(second, std::ios::binary)
        .write(data.data() + middle,
               static_cast<std::streamsize>(data.size() - middle));
    const Reader reader = Reader::Open({first, {first, second}});
    QCOMPARE(reader.LookupExact("example").size(), std::size_t{1});
}

void ZimReaderTest::RejectsCorruption() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path =
        std::filesystem::path(directory.path().toStdString()) / "broken.zim";
    std::ofstream(path, std::ios::binary) << "not zim";
    QVERIFY_EXCEPTION_THROWN(Reader::Open({path, {path}}), Error);
}
}  // namespace goldendict::core::formats::zim

using goldendict::core::formats::zim::ZimReaderTest;
QTEST_APPLESS_MAIN(ZimReaderTest)
#include "zim_reader_test.moc"
