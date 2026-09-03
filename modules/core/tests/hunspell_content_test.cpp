// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <string>

#include "../src/morphology/hunspell_content.h"
#include "support/hunspell_fixture.h"

namespace goldendict::core::morphology::hunspell {
namespace {

template <typename Callback>
ContentError Capture(Callback callback) {
    try {
        callback();
    } catch (const ContentError& error) {
        return error;
    }
    throw std::runtime_error("Expected ContentError");
}

}  // namespace

class HunspellContentTest : public QObject {
    Q_OBJECT

   private slots:
    void LoadsOriginalUtf8AndLegacyEncodedBytes();
    void RejectsAffixAndEncodingFailuresDeterministically();
    void RejectsDictionaryStructureAndResourceFailures();
    void RejectsUnsafeFilesystemInputs();
};

void HunspellContentTest::LoadsOriginalUtf8AndLegacyEncodedBytes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());

    const std::string utf8_affix = "SET UTF-8\nTRY abc\n";
    const std::string utf8_dictionary =
        "2\ncoffee/S\n\xd0\xba\xd0\xbe\xd1\x84\xd0\xb5\n";
    const auto utf8_files = test::WriteHunspellFixture(
        root / "utf8", "en_US", utf8_affix, utf8_dictionary);
    const auto utf8 = LoadContent(utf8_files);
    QCOMPARE(utf8.encoding, std::string("UTF-8"));
    QCOMPARE(utf8.affix_bytes, utf8_affix);
    QCOMPARE(utf8.dictionary_bytes, utf8_dictionary);
    QCOMPARE(utf8.dictionary_entry_count, std::size_t{2});

    const std::string latin1_affix = "SET ISO-8859-1\nTRY abc\xe9\n";
    const std::string latin1_dictionary = "1\ncaf\xe9\n";
    const auto latin1_files = test::WriteHunspellFixture(
        root / "latin1", "fr_FR", latin1_affix, latin1_dictionary);
    const auto latin1 = LoadContent(latin1_files);
    QCOMPARE(latin1.encoding, std::string("ISO-8859-1"));
    QCOMPARE(latin1.affix_bytes, latin1_affix);
    QCOMPARE(latin1.dictionary_bytes, latin1_dictionary);
    QCOMPARE(latin1.dictionary_entry_count, std::size_t{1});
}

void HunspellContentTest::RejectsAffixAndEncodingFailuresDeterministically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());

    auto missing_set =
        test::WriteHunspellFixture(root / "missing", "x", "TRY abc\n", "0\n");
    auto error = Capture([&] { LoadContent(missing_set); });
    QCOMPARE(error.code(), ContentErrorCode::kInvalidAffix);
    QCOMPARE(error.path(), missing_set.affix_file);

    auto duplicate_set = test::WriteHunspellFixture(
        root / "duplicate", "x", "SET UTF-8\nSET UTF-8\n", "0\n");
    error = Capture([&] { LoadContent(duplicate_set); });
    QCOMPARE(error.code(), ContentErrorCode::kInvalidAffix);

    auto unsupported = test::WriteHunspellFixture(
        root / "unsupported", "x", "SET GOLDENDICT-NOT-A-CODEC\n", "0\n");
    error = Capture([&] { LoadContent(unsupported); });
    QCOMPARE(error.code(), ContentErrorCode::kUnsupportedEncoding);
    QCOMPARE(error.path(), unsupported.affix_file);

    auto malformed =
        test::WriteHunspellFixture(root / "malformed", "x", "SET UTF-8\n",
                                   std::string("1\n\xc3\x28\n", 5U));
    error = Capture([&] { LoadContent(malformed); });
    QCOMPARE(error.code(), ContentErrorCode::kInvalidEncoding);
    QCOMPARE(error.path(), malformed.dictionary_file);
}

void HunspellContentTest::RejectsDictionaryStructureAndResourceFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());

    auto bad_header = test::WriteHunspellFixture(root / "header", "x",
                                                 "SET UTF-8\n", "one\nword\n");
    auto error = Capture([&] { LoadContent(bad_header); });
    QCOMPARE(error.code(), ContentErrorCode::kInvalidDictionary);

    auto bad_count = test::WriteHunspellFixture(root / "count", "x",
                                                "SET UTF-8\n", "2\nword\n");
    error = Capture([&] { LoadContent(bad_count); });
    QCOMPARE(error.code(), ContentErrorCode::kInvalidDictionary);

    auto too_many = test::WriteHunspellFixture(
        root / "entries", "x", "SET UTF-8\n",
        std::to_string(kMaximumDictionaryEntries + 1U) + "\n");
    error = Capture([&] { LoadContent(too_many); });
    QCOMPARE(error.code(), ContentErrorCode::kResourceLimit);

    auto long_line = test::WriteHunspellFixture(
        root / "line", "x",
        "SET UTF-8\n" + std::string(kMaximumContentLineBytes + 1U, 'a') + "\n",
        "0\n");
    error = Capture([&] { LoadContent(long_line); });
    QCOMPARE(error.code(), ContentErrorCode::kResourceLimit);
    QCOMPARE(error.path(), long_line.affix_file);

    auto oversized =
        test::WriteHunspellFixture(root / "size", "x", "SET UTF-8\n", "0\n");
    std::filesystem::resize_file(oversized.affix_file, kMaximumAffixBytes + 1U);
    error = Capture([&] { LoadContent(oversized); });
    QCOMPARE(error.code(), ContentErrorCode::kResourceLimit);
}

void HunspellContentTest::RejectsUnsafeFilesystemInputs() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto files = test::WriteHunspellFixture(root / "safe", "en_US",
                                            "SET UTF-8\n", "0\n");

    auto mismatch = files;
    mismatch.dictionary_id = "other";
    auto error = Capture([&] { LoadContent(mismatch); });
    QCOMPARE(error.code(), ContentErrorCode::kUnsafePath);

    const auto target = root / "target.aff";
    test::WriteHunspellBytes(target, "SET UTF-8\n");
    std::filesystem::remove(files.affix_file);
    std::error_code symlink_error;
    std::filesystem::create_symlink(target, files.affix_file, symlink_error);
    if (symlink_error)
        QSKIP("File symlink creation is unavailable");
    error = Capture([&] { LoadContent(files); });
    QCOMPARE(error.code(), ContentErrorCode::kUnsafePath);
    QCOMPARE(error.path(), files.affix_file);

    std::filesystem::remove(files.affix_file);
    error = Capture([&] { LoadContent(files); });
    QCOMPARE(error.code(), ContentErrorCode::kMissingFile);
}

}  // namespace goldendict::core::morphology::hunspell

using goldendict::core::morphology::hunspell::HunspellContentTest;
QTEST_APPLESS_MAIN(HunspellContentTest)
#include "hunspell_content_test.moc"
