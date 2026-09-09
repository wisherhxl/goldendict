// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <iterator>
#include <sstream>

#include "../src/formats/dictd/dictd_reader.h"
#include "support/dictd_fixture.h"

namespace goldendict::core::formats::dictd {

class DictdReaderTest : public QObject {
    Q_OBJECT

   private slots:
    void ReadsMetadataAliasesAndRankedMatches();
    void ReadsCompressedData();
    void ValidatesRealDictzipFixtureChunks();
    void ReadsCompanionContent_data();
    void ReadsCompanionContent();
    void PrefersSelectedCompanionWithoutFallback();
    void RejectsDamagedDictzip_data();
    void RejectsDamagedDictzip();
    void InvokesScanCheckpoints();
    void RejectsCorruptCompressedData();
    void RejectsMalformedBase64AndOutOfRangeArticles();
    void RecoversMalformedRows_data();
    void RecoversMalformedRows();
    void OpensIndexesWithoutAcceptedRows_data();
    void OpensIndexesWithoutAcceptedRows();
    void PreservesAcceptedRowValidation_data();
    void PreservesAcceptedRowValidation();
};

void DictdReaderTest::RecoversMalformedRows_data() {
    QTest::addColumn<QString>("row");
    QTest::newRow("blank") << "";
    QTest::newRow("no-tabs") << "ignored";
    QTest::newRow("one-tab") << "ignored\t!";
    QTest::newRow("four-tabs") << "ignored\t!\t!\talias\textra";
    QTest::newRow("five-tabs") << "ignored\t!\t!\talias\textra\tmore";
    QTest::newRow("trailing-extra-tab") << "ignored\tA\tA\talias\t";
}

void DictdReaderTest::RecoversMalformedRows() {
    QFETCH(QString, row);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"first", "first searchable", "original"},
               {"second", "second searchable", {}},
               {"00databaseshort", "Recovered title", {}},
               {"00databaseinfo", "Recovered description", {}}});
    std::ifstream input(index, std::ios::binary);
    const std::string original{std::istreambuf_iterator<char>(input), {}};
    input.close();
    std::istringstream lines(original);
    std::string line;
    std::ofstream output(index, std::ios::binary | std::ios::trunc);
    output << row.toStdString() << '\n';
    while (std::getline(lines, line)) {
        output << line << '\n' << row.toStdString() << '\n';
    }
    output << row.toStdString();  // Exercise the final unterminated row too.
    output.close();

    const auto reader = Reader::Open(index);
    QCOMPARE(reader.article_count(), 4U);
    QCOMPARE(reader.headword_count(), 5U);
    QCOMPARE(reader.name(), "Recovered title");
    QCOMPARE(reader.description(), "Recovered description");
    QCOMPARE(reader.LookupExact("first").front().data, "first searchable");
    QCOMPARE(reader.LookupExact("original").front().data, "first searchable");
    QCOMPARE(reader.LookupExact("second").front().data, "second searchable");
    QVERIFY(reader.LookupExact("ignored").empty());
    const auto articles = reader.ReadFullTextArticles();
    QCOMPARE(articles.size(), 2U);
    QCOMPARE(articles[0].record_ordinal, 1U);
    QCOMPARE(articles[1].record_ordinal, 3U);
    QCOMPARE(reader.source_snapshot(),
             dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"}));
}

void DictdReaderTest::OpensIndexesWithoutAcceptedRows_data() {
    QTest::addColumn<QString>("index_text");
    QTest::newRow("empty") << "";
    QTest::newRow("all-skipped") << "\nignored\nignored\t!\nignored\t!\t!\ta\textra";
}

void DictdReaderTest::OpensIndexesWithoutAcceptedRows() {
    QFETCH(QString, index_text);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(root, {});
    std::ofstream(index, std::ios::binary | std::ios::trunc)
        << index_text.toStdString();
    const auto reader = Reader::Open(index);
    QCOMPARE(reader.name(), "fixture");
    QVERIFY(reader.description().empty());
    QCOMPARE(reader.article_count(), 0U);
    QCOMPARE(reader.headword_count(), 0U);
    QVERIFY(reader.LookupExact("ignored").empty());
    QVERIFY(reader.ReadFullTextArticles().empty());
    QVERIFY(reader.EnumerateHeadwords(0U, 10U, 1024U).first.empty());
}

void DictdReaderTest::PreservesAcceptedRowValidation_data() {
    QTest::addColumn<QByteArray>("row");
    QTest::addColumn<bool>("invalid_dictionary");
    QTest::newRow("base64") << QByteArray("entry\t!\tA") << false;
    QTest::newRow("range") << QByteArray("entry\tA\t/") << true;
    QTest::newRow("utf8") << QByteArray("\xff\tA\tB") << false;
    QTest::newRow("alias-utf8") << QByteArray("entry\tA\tB\t\xff") << false;
    QTest::newRow("oversized-malformed-row")
        << QByteArray(16U * 1024U + 1U, 'x') << false;
}

void DictdReaderTest::PreservesAcceptedRowValidation() {
    QFETCH(QByteArray, row);
    QFETCH(bool, invalid_dictionary);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(root, {{"entry", "data", {}}});
    std::ofstream output(index, std::ios::binary | std::ios::trunc);
    output << "ignored\n";
    output.write(row.data(), row.size());
    output.close();
    try {
        static_cast<void>(Reader::Open(index));
        QFAIL("Accepted corrupt rows and oversized rows must fail");
    } catch (const Error& error) {
        QCOMPARE(error.code(), invalid_dictionary ? ErrorCode::kInvalidDictionary
                                                  : ErrorCode::kInvalidIndex);
        QCOMPARE(error.path(), index);
        if (row.size() <= 16U * 1024U) {
            QVERIFY(std::string(error.what()).find("line 2") != std::string::npos);
        }
    }
}

void DictdReaderTest::ValidatesRealDictzipFixtureChunks() {
    const std::string source =
        "first chunk " + std::string(140U, 'x') + " last";
    const std::string bytes = test::EncodeDictzipFixture(source, 64U);
    const auto read16 = [&bytes](std::size_t offset) {
        return static_cast<unsigned char>(bytes.at(offset)) |
               (static_cast<unsigned char>(bytes.at(offset + 1U)) << 8U);
    };
    const auto read32 = [&bytes](std::size_t offset) {
        std::uint32_t value = 0U;
        for (unsigned byte = 0U; byte < 4U; ++byte) {
            value |= static_cast<std::uint32_t>(
                         static_cast<unsigned char>(bytes.at(offset + byte)))
                     << (byte * 8U);
        }
        return value;
    };
    QCOMPARE(bytes.substr(0U, 4U), std::string("\x1f\x8b\x08\x04", 4U));
    QCOMPARE(bytes.substr(12U, 2U), "RA");
    QCOMPARE(read16(16U), 1);
    QCOMPARE(read16(18U), 64);
    QCOMPARE(read16(20U), 3);
    QCOMPARE(read16(14U), 12);
    QCOMPARE(read16(10U), 16);
    std::vector<std::size_t> offsets{28U};
    for (std::size_t chunk = 0U; chunk < 3U; ++chunk) {
        QVERIFY(read16(22U + 2U * chunk) > 0);
        offsets.push_back(offsets.back() + read16(22U + 2U * chunk));
    }
    QVERIFY(offsets.back() < bytes.size() - 8U);
    // Read later chunks first with fresh raw inflaters, independently of Reader
    // and without relying on the preceding chunk's decompression state. Match
    // the frozen reader's chunk-sized output buffer and inflate flush mode.
    for (const std::size_t chunk : {2U, 0U, 1U}) {
        z_stream stream{};
        QCOMPARE(inflateInit2(&stream, -MAX_WBITS), Z_OK);
        std::string decoded(64U, '\0');
        stream.next_in = reinterpret_cast<Bytef*>(
            const_cast<char*>(bytes.data() + offsets[chunk]));
        stream.avail_in =
            static_cast<uInt>(offsets[chunk + 1U] - offsets[chunk]);
        stream.next_out = reinterpret_cast<Bytef*>(decoded.data());
        stream.avail_out = static_cast<uInt>(decoded.size());
        const int status = inflate(&stream, Z_PARTIAL_FLUSH);
        const auto consumed = stream.avail_in;
        decoded.resize(decoded.size() - stream.avail_out);
        QCOMPARE(inflateEnd(&stream), Z_OK);
        QCOMPARE(status, Z_OK);
        QCOMPARE(consumed, 0U);
        QCOMPARE(decoded, source.substr(chunk * 64U, 64U));
    }
    QCOMPARE(read32(bytes.size() - 8U),
             static_cast<std::uint32_t>(
                 crc32(0U, reinterpret_cast<const Bytef*>(source.data()),
                       static_cast<uInt>(source.size()))));
    QCOMPARE(read32(bytes.size() - 4U), source.size());
}

void DictdReaderTest::ReadsCompanionContent_data() {
    QTest::addColumn<QString>("suffix");
    QTest::addColumn<bool>("compressed");
    QTest::newRow("plain-dict") << ".dict" << false;
    QTest::newRow("plain-dz") << ".dict.dz" << false;
    QTest::newRow("ra-dict") << ".dict" << true;
    QTest::newRow("ra-dz") << ".dict.dz" << true;
}

void DictdReaderTest::ReadsCompanionContent() {
    QFETCH(QString, suffix);
    QFETCH(bool, compressed);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const std::string first(16U, 'a');
    const std::string crossing =
        "crossing " + std::string(100U, 'b') + " \xc3\xa9";
    const std::string later = "later article";
    const std::string title = "00databaseshort\nRA title\n";
    const std::string info = "RA description";
    const auto index =
        test::WriteDictdFixture(root, {{"first", first, {}},
                                       {"crossing", crossing, "alias"},
                                       {"later", later, {}},
                                       {"00databaseshort", title, {}},
                                       {"00databaseinfo", info, {}}});
    const auto selected = root / ("fixture" + suffix.toStdString());
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    const std::string data = first + crossing + later + title + info;
    const std::string bytes =
        compressed ? test::EncodeDictzipFixture(data) : data;
    std::ofstream(selected, std::ios::binary)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

    const Reader reader = Reader::Open(index);
    QCOMPARE(reader.LookupExact("first").front().data, first);
    QCOMPARE(reader.LookupExact("crossing").front().data, crossing);
    QCOMPARE(reader.LookupExact("alias").front().data, crossing);
    QCOMPARE(reader.LookupExact("later").front().data, later);
    QCOMPARE(reader.name(), "RA title");
    QCOMPARE(reader.description(), info);
    QCOMPARE(reader.article_count(), 5U);
    QCOMPARE(reader.headword_count(), 6U);
    QCOMPARE(reader.source_snapshot(),
             dictionary::CaptureSourceSnapshot({index, selected}));
    const auto restarted = Reader::Open(index);
    QCOMPARE(restarted.LookupExact("crossing").front().data, crossing);
    QCOMPARE(restarted.source_snapshot(), reader.source_snapshot());
}

void DictdReaderTest::PrefersSelectedCompanionWithoutFallback() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"entry", "selected", {}}});
    const auto preferred = root / "fixture.dict";
    const auto other = root / "fixture.dict.dz";
    const auto bytes = test::EncodeDictzipFixture("selected");
    std::ofstream(preferred, std::ios::binary | std::ios::trunc)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    std::ofstream(other, std::ios::binary) << "fallback";
    const auto reader = Reader::Open(index);
    QCOMPARE(reader.LookupExact("entry").front().data, "selected");
    QCOMPARE(reader.source_snapshot(),
             dictionary::CaptureSourceSnapshot({index, preferred}));

    std::ofstream(preferred, std::ios::binary | std::ios::trunc)
        << std::string("\x1f\x8b\x08\x04", 4U);
    try {
        static_cast<void>(Reader::Open(index));
        QFAIL("Invalid preferred companion must not fall back");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
        QCOMPARE(error.path(), preferred);
    }
}

void DictdReaderTest::RejectsDamagedDictzip_data() {
    QTest::addColumn<QString>("suffix");
    QTest::addColumn<QString>("damage");
    for (const QString suffix : {QString(".dict"), QString(".dict.dz")}) {
        for (const QString damage : {QString("header"), QString("payload"),
                                     QString("checksum"), QString("trailer")}) {
            QTest::newRow(qPrintable(suffix + damage)) << suffix << damage;
        }
    }
}

void DictdReaderTest::RejectsDamagedDictzip() {
    QFETCH(QString, suffix);
    QFETCH(QString, damage);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(root, {{"entry", "data", {}}});
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    const auto selected = root / ("fixture" + suffix.toStdString());
    auto bytes = test::EncodeDictzipFixture("data");
    if (damage == "header") {
        bytes.resize(5U);
    } else if (damage == "payload") {
        bytes[24U] = '\x07';  // Reserved DEFLATE block type.
    } else if (damage == "checksum") {
        bytes[bytes.size() - 8U] ^= 1;
    } else {
        bytes.resize(bytes.size() - 3U);
    }
    std::ofstream(selected, std::ios::binary)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    try {
        static_cast<void>(Reader::Open(index));
        QFAIL("Damaged dictzip must fail");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
        QCOMPARE(error.path(), selected);
    }
}

void DictdReaderTest::ReadsMetadataAliasesAndRankedMatches() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"00databaseshort", "00databaseshort\nFixture Dictd\n", {}},
               {"cafeteria", "long", {}},
               {"Caf\xc3\xa9", "exact", "cafe-original"}});

    const Reader reader = Reader::Open(index);

    QCOMPARE(reader.name(), "Fixture Dictd");
    QCOMPARE(reader.LookupExact("CAFE").front().data, "exact");
    QCOMPARE(reader.LookupExact("cafe original").front().data, "exact");
    const auto prefix = reader.LookupPrefix("CAFE", 2U);
    QCOMPARE(prefix.size(), std::size_t{2});
    QCOMPARE(prefix[0].data, "exact");
    QCOMPARE(prefix[1].data, "long");
    const auto suggestions = reader.SuggestPrefix("CAFE", 3U);
    QCOMPARE(suggestions.size(), std::size_t{3});
    QCOMPARE(suggestions.front(), "Caf\xc3\xa9");
}

void DictdReaderTest::ReadsCompressedData() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"example", "compressed definition", {}}});
    test::CompressDictdFixture(index);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));

    const Reader reader = Reader::Open(index);

    QCOMPARE(reader.LookupExact("example").front().data,
             "compressed definition");
}

void DictdReaderTest::InvokesScanCheckpoints() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"example", "definition", {}}});
    const Reader reader = Reader::Open(index);
    std::size_t checkpoints = 0;

    const auto result =
        reader.LookupPrefix("exa", 1U, [&checkpoints]() { ++checkpoints; });

    QCOMPARE(result.size(), std::size_t{1});
    QCOMPARE(checkpoints, std::size_t{1});
}

void DictdReaderTest::RejectsCorruptCompressedData() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"example", "definition", {}}});
    const auto compressed = test::CompressDictdFixture(index);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    std::ofstream(compressed, std::ios::binary | std::ios::trunc) << "not-gzip";

    try {
        static_cast<void>(Reader::Open(index));
        QFAIL("Corrupt compressed Dictd data should fail");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
    }
}

void DictdReaderTest::RejectsMalformedBase64AndOutOfRangeArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"example", "definition", {}}});
    std::ofstream(index, std::ios::binary | std::ios::trunc)
        << "example\t!\tA\n";
    QVERIFY_EXCEPTION_THROWN(Reader::Open(index), Error);

    std::ofstream(index, std::ios::binary | std::ios::trunc)
        << "example\tA\t/\n";
    try {
        static_cast<void>(Reader::Open(index));
        QFAIL("Out-of-range Dictd article should fail");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kInvalidDictionary);
    }
}

}  // namespace goldendict::core::formats::dictd

using goldendict::core::formats::dictd::DictdReaderTest;
QTEST_APPLESS_MAIN(DictdReaderTest)
#include "dictd_reader_test.moc"
