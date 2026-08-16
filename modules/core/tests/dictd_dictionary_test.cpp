// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "../src/formats/dictd/dictd_dictionary.h"
#include "support/dictd_fixture.h"

namespace goldendict::core::formats::dictd {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class FullTextCancelledToken final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class SlowFullTextToken final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return false;
    }
};

class DictdDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesPlainArticlesAndSuggestions();
    void HonorsCancellationAndHasNoResources();
    void BuildsRangeDeduplicatedFullTextIndex();
    void ReusesAndRebuildsFullTextIndexForBothSources();
    void SearchesDictzipAndContainsFullTextFailures();
};

void DictdDictionaryTest::ExposesPlainArticlesAndSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"example", "definition", "Example Original"},
               {"examples", "plural", {}},
               {"00databaseinfo", "Fixture description", {}}});
    const Dictionary dictionary = Dictionary::Open("dictd-id", index);
    dictionary::RequestOptions options;
    options.result_limit = 1U;

    const auto articles = dictionary.LookupPrefix("EXAMPLE", options);
    const auto suggestions = dictionary.SuggestPrefix("EXAMPLE", options);

    QCOMPARE(dictionary.identity().id, "dictd-id");
    QCOMPARE(dictionary.identity().name, "fixture");
    QCOMPARE(dictionary.identity().article_count, std::size_t{3});
    QCOMPARE(dictionary.identity().headword_count, std::size_t{4});
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"00databaseinfo", "Example Original",
                                       "example", "examples"}));
    QCOMPARE(dictionary.identity().description, "Fixture description");
    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().format, "text/plain");
    QCOMPARE(articles.front().data, "definition");
    QCOMPARE(suggestions.size(), std::size_t{1});
    QCOMPARE(suggestions.front(), "example");
}

void DictdDictionaryTest::BuildsRangeDeduplicatedFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root,
        {{"00databaseshort", "a", ""},
         {"00-database-short", "b", ""},
         {"00databaseinfo", "c", ""},
         {"00-database-info", "d", ""},
         {"canonical", "visible searchable <b>literal</b>", "originalalias"},
         {"lateralias", "alias-only-secret", "", 4U},
         {"second", "other searchable", ""}});
    const Dictionary dictionary =
        Dictionary::Open("dictd-id", index, root / "fixture.gdfts");

    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(response.results.size(), std::size_t{2});
    QCOMPARE(response.results[0].headword, std::string("canonical"));
    QCOMPARE(response.results[0].document_id,
             std::string("dictd-index:4:4:33"));
    QCOMPARE(response.results[1].headword, std::string("second"));
    for (const std::string excluded :
         {"originalalias", "lateralias", "alias-only-secret"}) {
        query.text = excluded;
        QVERIFY2(dictionary.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
}

void DictdDictionaryTest::ReusesAndRebuildsFullTextIndexForBothSources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto index =
        test::WriteDictdFixture(root, {{"entry", "first searchable text", ""}});
    const auto full_text_path = root / "fixture.gdfts";
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));

    index = test::WriteDictdFixture(
        root, {{"entry", "second searchable text expanded", ""}});
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    const auto compressed = test::CompressDictdFixture(index);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ifstream compressed_input(compressed, std::ios::binary);
    const std::string compressed_data{
        std::istreambuf_iterator<char>(compressed_input),
        std::istreambuf_iterator<char>()};
    compressed_input.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::ofstream(compressed, std::ios::binary | std::ios::trunc)
        .write(compressed_data.data(),
               static_cast<std::streamsize>(compressed_data.size()));
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ofstream(full_text_path, std::ios::binary | std::ios::trunc)
        << "corrupt";
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));

    const Dictionary disabled = Dictionary::Open("dictd-id", index);
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void DictdDictionaryTest::SearchesDictzipAndContainsFullTextFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"entry", "compressed searchable text", ""}});
    test::CompressDictdFixture(index);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    const Dictionary compressed =
        Dictionary::Open("dictd-id", index, root / "fixture.gdfts");
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(compressed.SearchFullText(query).results.size(), std::size_t{1});
    FullTextCancelledToken cancelled;
    QCOMPARE(compressed.SearchFullText(query, &cancelled).errors.front().code,
             FullTextErrorCode::kCancelled);
    SlowFullTextToken slow;
    query.timeout = std::chrono::milliseconds(1);
    QCOMPARE(compressed.SearchFullText(query, &slow).errors.front().code,
             FullTextErrorCode::kDeadlineExceeded);

    const auto storage_root = root / "storage";
    const auto storage_index =
        test::WriteDictdFixture(storage_root, {{"entry", "searchable", ""}});
    const auto storage_path = storage_root / "fixture.gdfts";
    QVERIFY(std::filesystem::create_directory(storage_path));
    const Dictionary storage =
        Dictionary::Open("storage-id", storage_index, storage_path);
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);

    const auto oversized_root = root / "oversized";
    const auto oversized_index = test::WriteDictdFixture(
        oversized_root,
        {{"oversized", std::string(16U * 1024U * 1024U + 1U, 'x'), ""}});
    const Dictionary oversized = Dictionary::Open(
        "oversized-id", oversized_index, oversized_root / "fixture.gdfts");
    query.timeout = std::chrono::seconds(5);
    QCOMPARE(oversized.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kResourceLimit);
}

void DictdDictionaryTest::HonorsCancellationAndHasNoResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"example", "definition", {}}});
    const Dictionary dictionary = Dictionary::Open("dictd-id", index);
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    dictionary::RequestOptions active;
    QVERIFY(!dictionary.GetResource("missing", active).has_value());
}

}  // namespace
}  // namespace goldendict::core::formats::dictd

using goldendict::core::formats::dictd::DictdDictionaryTest;
QTEST_APPLESS_MAIN(DictdDictionaryTest)
#include "dictd_dictionary_test.moc"
