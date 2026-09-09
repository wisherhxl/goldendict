// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
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
    void SearchesGzipAndContainsFullTextFailures();
    void ReusesRealDictzipCompanions_data();
    void ReusesRealDictzipCompanions();
    void RebuildsPreContentDetectionFullTextIndex();
    void ReusesRecoveredIndexRows();
    void RejectsAcceptedCorruptionAfterSkippedRow();
};

void DictdDictionaryTest::ReusesRecoveredIndexRows() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"first", "first searchable", "original"},
               {"second", "second searchable", {}}});
    std::ifstream input(index, std::ios::binary);
    const std::string original{std::istreambuf_iterator<char>(input), {}};
    input.close();
    std::ofstream(index, std::ios::binary | std::ios::trunc)
        << "ignored\t!\t!\talias\textra\n" << original << "ignored\n";
    const auto sources =
        dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"});
    const auto full_text_path = root / "fixture.gdfts";
    FullTextQuery query;
    query.text = "searchable";
    for (const bool warm : {false, true}) {
        const auto dictionary =
            Dictionary::Open("dictd-id", index, full_text_path);
        QCOMPARE(dictionary.identity().id, "dictd-id");
        QCOMPARE(dictionary.identity().source,
                 std::filesystem::weakly_canonical(index).string());
        QCOMPARE(dictionary.identity().article_count, 2U);
        QCOMPARE(dictionary.identity().headword_count, 3U);
        QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
                 (std::vector<std::string>{"first", "original", "second"}));
        QCOMPARE(dictionary.LookupExact("original", {}).front().data,
                 "first searchable");
        QVERIFY(dictionary.LookupExact("ignored", {}).empty());
        QCOMPARE(dictionary.full_text_index_state(),
                 std::optional(warm ? dictionary::FullTextIndexState::kReused
                                    : dictionary::FullTextIndexState::kCreated));
        const auto response = dictionary.SearchFullText(query);
        QVERIFY(response.errors.empty());
        QVERIFY(!response.partial);
        QCOMPARE(response.results.size(), 2U);
        QCOMPARE(response.results[0].headword, "first");
        QCOMPARE(response.results[0].document_id, "dictd-index:1:0:16");
        QCOMPARE(response.results[1].headword, "second");
        QCOMPARE(response.results[1].document_id, "dictd-index:2:16:17");
        QCOMPARE(dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"}),
                 sources);
    }
}

void DictdDictionaryTest::RejectsAcceptedCorruptionAfterSkippedRow() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(root, {{"entry", "data", {}}});
    std::ofstream(index, std::ios::binary | std::ios::trunc)
        << "ignored\nentry\t!\tB\n";
    const auto full_text_path = root / "fixture.gdfts";
    try {
        static_cast<void>(Dictionary::Open("dictd-id", index, full_text_path));
        QFAIL("Accepted corrupt rows must still report invalid data");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
        QVERIFY(std::string(error.what()).find("line 2") != std::string::npos);
    }
    QVERIFY(!std::filesystem::exists(full_text_path));
}

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

void DictdDictionaryTest::SearchesGzipAndContainsFullTextFailures() {
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

void DictdDictionaryTest::ReusesRealDictzipCompanions_data() {
    QTest::addColumn<QString>("suffix");
    QTest::newRow("ra-dict") << ".dict";
    QTest::newRow("ra-dz") << ".dict.dz";
}

void DictdDictionaryTest::ReusesRealDictzipCompanions() {
    QFETCH(QString, suffix);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const std::string title = "00databaseshort\nRA title\n";
    const std::string info = "RA description";
    const std::string article =
        "searchable " + std::string(140U, 'x') + " ending";
    const auto index =
        test::WriteDictdFixture(root, {{"00databaseshort", title, {}},
                                       {"00databaseinfo", info, {}},
                                       {"entry", article, "alias"}});
    const auto selected = root / ("fixture" + suffix.toStdString());
    const auto bytes = test::EncodeDictzipFixture(title + info + article);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    std::ofstream(selected, std::ios::binary)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const auto original_sources =
        dictionary::CaptureSourceSnapshot({index, selected});
    const auto full_text_path = root / "fixture.gdfts";
    FullTextQuery query;
    query.text = "searchable";
    for (const bool warm : {false, true}) {
        const auto dictionary =
            Dictionary::Open("dictd-id", index, full_text_path);
        QCOMPARE(dictionary.identity().name, "RA title");
        QCOMPARE(dictionary.identity().description, info);
        QCOMPARE(dictionary.identity().article_count, 3U);
        QCOMPARE(dictionary.identity().headword_count, 4U);
        QCOMPARE(dictionary.identity().id, "dictd-id");
        QCOMPARE(dictionary.identity().source,
                 std::filesystem::weakly_canonical(index).string());
        QCOMPARE(dictionary.LookupExact("entry", {}).front().data, article);
        QCOMPARE(dictionary.LookupExact("alias", {}).front().data, article);
        QCOMPARE(
            dictionary.full_text_index_state(),
            std::optional(warm ? dictionary::FullTextIndexState::kReused
                               : dictionary::FullTextIndexState::kCreated));
        const auto response = dictionary.SearchFullText(query);
        QVERIFY(response.errors.empty());
        QVERIFY(!response.partial);
        QCOMPARE(response.results.size(), 1U);
        QCOMPARE(response.results.front().headword, "entry");
        QCOMPARE(response.results.front().dictionary.name, "RA title");
        QCOMPARE(response.results.front().document_id,
                 "dictd-index:2:" + std::to_string(title.size() + info.size()) +
                     ":" + std::to_string(article.size()));
        QCOMPARE(dictionary::CaptureSourceSnapshot({index, selected}),
                 original_sources);
    }
    auto damaged = bytes;
    damaged[damaged.size() - 8U] ^= 1;
    std::ofstream(selected, std::ios::binary | std::ios::trunc)
        .write(damaged.data(), static_cast<std::streamsize>(damaged.size()));
    try {
        static_cast<void>(Dictionary::Open("dictd-id", index, full_text_path));
        QFAIL("Damaged selected dictzip must report invalid data");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

void DictdDictionaryTest::RebuildsPreContentDetectionFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = root / "fixture.index";
    const auto data = root / "fixture.dict";
    const auto full_text_path = root / "fixture.gdfts";
    std::ofstream(index, std::ios::binary) << "entry\tM\tC\n";
    const auto bytes = test::EncodeDictzipFixture(std::string(12U, 'x') + "OK");
    std::ofstream(data, std::ios::binary)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    // The former raw .dict path loaded the valid UTF-8 RA header at this range.
    // Seed precisely that old document against the unchanged original sources.
    QCOMPARE(bytes.substr(12U, 2U), "RA");
    dictionary::FullTextDocument old_document;
    old_document.dictionary.id = "dictd-id";
    old_document.dictionary.name = "fixture";
    old_document.headword = "entry";
    old_document.document_id = "dictd-index:0:12:2";
    old_document.plain_text = bytes.substr(12U, 2U);
    const auto sources = dictionary::CaptureSourceSnapshot({index, data});
    const auto old_index = dictionary::FullTextIndex::OpenOrBuild(
        full_text_path, sources, {old_document});
    FullTextQuery query;
    query.text = "RA";
    QCOMPARE(old_index.Search(query).results.size(), 1U);
    for (const bool warm : {false, true}) {
        const auto dictionary =
            Dictionary::Open("dictd-id", index, full_text_path);
        QCOMPARE(dictionary.full_text_index_state(),
                 std::optional(
                     warm ? dictionary::FullTextIndexState::kReused
                          : dictionary::FullTextIndexState::kRebuiltStale));
        query.text = "RA";
        QVERIFY(dictionary.SearchFullText(query).results.empty());
        query.text = "OK";
        QCOMPARE(dictionary.SearchFullText(query).results.size(), 1U);
        QCOMPARE(dictionary.LookupExact("entry", {}).front().data, "OK");
        QCOMPARE(dictionary::CaptureSourceSnapshot({index, data}), sources);
    }
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
