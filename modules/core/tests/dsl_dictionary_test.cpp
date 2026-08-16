// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <thread>

#include "../src/formats/dsl/dsl_dictionary.h"
#include "support/dsl_fixture.h"

namespace goldendict::core::formats::dsl {
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

class DslDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlSuggestionsAndResources();
    void RejectsCancellationAndUnsafeResources();
    void BuildsDeduplicatedFullTextFromInertArticles();
    void ReusesAndRebuildsForOnlySelectedSource();
    void SearchesCompressedAndContainsFailures();
};

void DslDictionaryTest::ExposesIdentityHtmlSuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteDslFixture(root);
    test::WriteDslResource(path, "images/cup.png", "png-data");
    std::ofstream(root / "fixture.ann")
        << "#LANGUAGE \"en\"\nEnglish annotation\n"
           "#LANGUAGE \"de\"\nGerman annotation";
    const Dictionary dictionary = Dictionary::Open("dsl-id", path, "de");
    QCOMPARE(dictionary.identity().name, "Fixture DSL");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QVERIFY(!dictionary.EnumerateHeadwords(0U).headwords.empty());
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.identity().target_language, "de");
    QCOMPARE(dictionary.identity().description, "German annotation");
    QCOMPARE(dictionary.LookupExact("CAFE").front().format, "text/html");
    QVERIFY(!dictionary.SuggestPrefix("caf").empty());
    const auto resource = dictionary.GetResource("images/cup.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
}

void DslDictionaryTest::BuildsDeduplicatedFullTextFromInertArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteDslTextFixture(
        root,
        "#NAME \"DirectiveSecret\"\n"
        "Can(onical)\n"
        "~ alternatealiasonlysecret\n"
        "lateraliasonlysecret\n"
        "\t[b]visible searchable[/b] [*]optionaltext[/*] "
        "<<visiblelink>> [s]images/resourcesecret.png[/s] "
        "{{annotationsecret}}\n"
        "second\n"
        "\tother searchable\n");
    std::ofstream(root / "fixture.ann") << "annotationfilesecret";
    test::WriteDslResource(path, "images/resourcesecret.png",
                           "resourcecontentsecret");
    const Dictionary dictionary =
        Dictionary::Open("dsl-id", path, {}, root / "fixture.gdfts");

    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(response.results.size(), std::size_t{2});
    QCOMPARE(response.results[0].headword, std::string("Can"));
    QCOMPARE(response.results[0].document_id, std::string("dsl-index:0:0"));
    QCOMPARE(response.results[1].headword, std::string("second"));
    QCOMPARE(response.results[1].document_id, std::string("dsl-index:4:1"));
    for (const std::string excluded :
         {"alternatealiasonlysecret", "lateraliasonlysecret", "resourcesecret",
          "resourcecontentsecret", "annotationsecret", "annotationfilesecret",
          "DirectiveSecret", "gd-optional", "img"}) {
        query.text = excluded;
        QVERIFY2(dictionary.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
    for (const std::string included :
         {"visible", "optionaltext", "visiblelink"}) {
        query.text = included;
        QCOMPARE(dictionary.SearchFullText(query).results.size(),
                 std::size_t{1});
    }
    QCOMPARE(dictionary.LookupExact("canonical").size(), std::size_t{1});
    QCOMPARE(dictionary.LookupExact("lateraliasonlysecret").size(),
             std::size_t{1});
}

void DslDictionaryTest::ReusesAndRebuildsForOnlySelectedSource() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    constexpr std::string_view first = "entry\n\tfirst searchable text\n";
    auto path = test::WriteDslTextFixture(root, first);
    const auto index = root / "fixture.gdfts";
    QCOMPARE(
        Dictionary::Open("dsl-id", path, {}, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(
        Dictionary::Open("dsl-id", path, {}, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kReused));

    std::ofstream(root / "fixture.ann") << "changed annotation";
    test::WriteDslResource(path, "resource.txt", "changed resource");
    QCOMPARE(
        Dictionary::Open("dsl-id", path, {}, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kReused));
    path = test::WriteDslTextFixture(
        root, "entry\n\tsecond searchable text expanded\n");
    QCOMPARE(
        Dictionary::Open("dsl-id", path, {}, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    path = test::CompressDslFixture(path);
    QCOMPARE(
        Dictionary::Open("dsl-id", path, {}, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ofstream(index, std::ios::binary | std::ios::trunc) << "corrupt";
    QCOMPARE(
        Dictionary::Open("dsl-id", path, {}, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));

    const Dictionary disabled = Dictionary::Open("dsl-id", path);
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void DslDictionaryTest::SearchesCompressedAndContainsFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto source = test::CompressDslFixture(test::WriteDslTextFixture(
        root / "compressed", "entry\n\tcompressed searchable text\n"));
    const Dictionary compressed = Dictionary::Open(
        "dsl-id", source, {}, root / "compressed" / "fixture.gdfts");
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(compressed.SearchFullText(query).results.size(), std::size_t{1});
    FullTextCancelledToken cancelled;
    QCOMPARE(compressed.SearchFullText(query, &cancelled).errors.front().code,
             FullTextErrorCode::kCancelled);
    query.timeout = std::chrono::milliseconds(1);
    SlowFullTextToken slow;
    QCOMPARE(compressed.SearchFullText(query, &slow).errors.front().code,
             FullTextErrorCode::kDeadlineExceeded);

    const auto storage_root = root / "storage";
    const auto storage_source =
        test::WriteDslTextFixture(storage_root, "entry\n\tsearchable\n");
    const auto storage_path = storage_root / "fixture.gdfts";
    QVERIFY(std::filesystem::create_directory(storage_path));
    const Dictionary storage =
        Dictionary::Open("storage-id", storage_source, {}, storage_path);
    QCOMPARE(storage.LookupExact("entry").size(), std::size_t{1});
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);

    std::string oversized_text;
    oversized_text.reserve((dictionary::kMaximumFullTextDocuments + 1U) * 24U);
    for (std::size_t i = 0U; i <= dictionary::kMaximumFullTextDocuments; ++i) {
        oversized_text += "entry-" + std::to_string(i) + "\n\tbounded\n";
    }
    const auto oversized_source =
        test::WriteDslTextFixture(root / "oversized", oversized_text);
    const Dictionary oversized =
        Dictionary::Open("oversized-id", oversized_source, {},
                         root / "oversized" / "fixture.gdfts");
    query.timeout = std::chrono::seconds(5);
    QCOMPARE(oversized.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kResourceLimit);
}

void DslDictionaryTest::RejectsCancellationAndUnsafeResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "dsl-id", test::WriteDslFixture(
                      std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;
    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("cafe", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("../outside.txt").has_value());
}

}  // namespace
}  // namespace goldendict::core::formats::dsl

using goldendict::core::formats::dsl::DslDictionaryTest;
QTEST_APPLESS_MAIN(DslDictionaryTest)
#include "dsl_dictionary_test.moc"
