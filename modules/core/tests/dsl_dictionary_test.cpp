// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <string_view>
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

std::string ResourceText(const dictionary::Resource& resource) {
    return {reinterpret_cast<const char*>(resource.data.data()),
            resource.data.size()};
}

class DslDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlSuggestionsAndResources();
    void ReadsClassicAndZip64ResourceArchives();
    void ReadsOverflowedClassicResourceArchiveCount();
    void PreservesResourcePrecedenceAndArchiveSafety();
    void RejectsCancellationAndUnsafeResources();
    void BuildsDeduplicatedFullTextFromInertArticles();
    void PreservesPrimaryFullTextOwnershipAfterLegacyMerge();
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

void DslDictionaryTest::ReadsClassicAndZip64ResourceArchives() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());

    const auto plain_path = test::WriteDslFixture(root / "classic");
    test::WriteDslResourceZip(
        plain_path, {{"images/stored.png", "stored-image", false},
                     {"styles/main.css", "body { color: green; }", true}});
    const Dictionary plain = Dictionary::Open("classic-id", plain_path);
    const auto stored = plain.GetResource("images/stored.png");
    QVERIFY(stored.has_value());
    QCOMPARE(stored->media_type, "image/png");
    QCOMPARE(ResourceText(*stored), "stored-image");
    const auto deflated = plain.GetResource("styles/main.css");
    QVERIFY(deflated.has_value());
    QCOMPARE(deflated->media_type, "text/css");
    QCOMPARE(ResourceText(*deflated), "body { color: green; }");

    const auto compressed_path =
        test::CompressDslFixture(test::WriteDslFixture(root / "zip64"));
    test::WriteDslResourceZip(
        compressed_path, {{"audio/sample.wav", "RIFF-zip64-resource", true}},
        true);
    const Dictionary zip64 = Dictionary::Open("zip64-id", compressed_path);
    const auto audio = zip64.GetResource("audio/sample.wav");
    QVERIFY(audio.has_value());
    QCOMPARE(audio->media_type, "audio/wav");
    QCOMPARE(ResourceText(*audio), "RIFF-zip64-resource");
}

void DslDictionaryTest::ReadsOverflowedClassicResourceArchiveCount() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteDslFixture(root);
    std::vector<test::DslZipResource> resources;
    resources.reserve(65537U);
    for (std::size_t index = 0U; index < 65537U; ++index)
        resources.push_back(
            {"bulk/" + std::to_string(index) + ".dat", {}, false});
    resources.back().data = "last-resource";
    test::WriteDslResourceZip(path, resources);

    const Dictionary dictionary = Dictionary::Open("overflow-id", path);
    const auto resource = dictionary.GetResource("bulk/65536.dat");
    QVERIFY(resource.has_value());
    QCOMPARE(ResourceText(*resource), "last-resource");
}

void DslDictionaryTest::PreservesResourcePrecedenceAndArchiveSafety() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteDslFixture(root);
    const auto archive = test::WriteDslResourceZip(
        path,
        {{"shared.txt", "archive", true}, {"../outside.txt", "unsafe", false}});
    test::WriteDslResource(path, "shared.txt", "directory");
    test::WriteDslResourceZip(root / "orphan.dsl",
                              {{"orphan.txt", "orphan", false}});

    const Dictionary dictionary = Dictionary::Open("dsl-id", path);
    const auto preferred = dictionary.GetResource("shared.txt");
    QVERIFY(preferred.has_value());
    QCOMPARE(ResourceText(*preferred), "directory");
    QVERIFY(!dictionary.GetResource("../outside.txt").has_value());
    QVERIFY(!dictionary.GetResource("orphan.txt").has_value());

    const auto archive_only_path =
        test::WriteDslFixture(root / "source-change");
    const auto changed_archive = test::WriteDslResourceZip(
        archive_only_path, {{"changed.txt", "before", false}});
    const Dictionary source_change =
        Dictionary::Open("source-change-id", archive_only_path);
    std::ofstream(changed_archive, std::ios::binary | std::ios::app).put('x');
    QVERIFY_EXCEPTION_THROWN(source_change.GetResource("changed.txt"),
                             dictionary::Error);

    const auto corrupt_path = test::WriteDslFixture(root / "corrupt");
    const auto corrupt_archive = test::WriteDslResourceZip(
        corrupt_path, {{"bad.txt", "original", false}});
    std::fstream corrupt(corrupt_archive,
                         std::ios::binary | std::ios::in | std::ios::out);
    QVERIFY(corrupt.is_open());
    constexpr std::streamoff kLocalHeaderSize = 30;
    corrupt.seekp(kLocalHeaderSize + static_cast<std::streamoff>(
                                         std::string_view("bad.txt").size()));
    corrupt.put('X');
    corrupt.close();
    const Dictionary corrupted = Dictionary::Open("corrupt-id", corrupt_path);
    QVERIFY_EXCEPTION_THROWN(corrupted.GetResource("bad.txt"),
                             dictionary::Error);
    QVERIFY(std::filesystem::is_regular_file(archive));
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

void DslDictionaryTest::PreservesPrimaryFullTextOwnershipAfterLegacyMerge() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteDslTextFixture(
        root,
        "z\n"
        "a\n"
        "~x\n"
        "\trolling ownership marker\n");
    const Dictionary dictionary =
        Dictionary::Open("dsl-id", path, {}, root / "fixture.gdfts");

    QCOMPARE(dictionary.LookupExact("a").size(), std::size_t{1});
    QCOMPARE(dictionary.LookupExact("ax").size(), std::size_t{1});
    QCOMPARE(dictionary.LookupExact("z").size(), std::size_t{1});
    FullTextQuery query;
    query.text = "marker";
    const auto response = dictionary.SearchFullText(query);
    QCOMPARE(response.results.size(), std::size_t{1});
    QCOMPARE(response.results.front().headword, std::string("z"));
    QCOMPARE(response.results.front().document_id,
             std::string("dsl-index:0:0"));

    const std::string long_primary(16U * 1024U + 1U, 'z');
    const auto filtered_path = test::WriteDslTextFixture(
        root / "filtered",
        long_primary + "\nshort\n\tfiltered ownership marker\n");
    const Dictionary filtered = Dictionary::Open(
        "filtered-dsl-id", filtered_path, {},
        root / "filtered" / "fixture.gdfts");
    QCOMPARE(filtered.LookupExact("short").size(), std::size_t{1});
    const auto filtered_response = filtered.SearchFullText(query);
    QCOMPARE(filtered_response.results.size(), std::size_t{1});
    QCOMPARE(filtered_response.results.front().headword, long_primary);
    QCOMPARE(filtered_response.results.front().document_id,
             std::string("dsl-index:0:0"));

    const auto empty_path = test::WriteDslTextFixture(
        root / "empty-filtered", "(x)\n\tempty ownership marker\n");
    const Dictionary empty_filtered = Dictionary::Open(
        "empty-filtered-dsl-id", empty_path, {},
        root / "empty-filtered" / "fixture.gdfts");
    QCOMPARE(empty_filtered.identity().headword_count, std::size_t{2});
    QCOMPARE(empty_filtered.LookupExact("").size(), std::size_t{0});
    QCOMPARE(empty_filtered.LookupExact("x").size(), std::size_t{1});
    const auto empty_response = empty_filtered.SearchFullText(query);
    QCOMPARE(empty_response.results.size(), std::size_t{1});
    QCOMPARE(empty_response.results.front().headword, std::string("x"));
    QCOMPARE(empty_response.results.front().document_id,
             std::string("dsl-index:0:0"));
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
