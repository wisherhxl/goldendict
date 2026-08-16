// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <thread>

#include "../src/formats/gls/gls_dictionary.h"
#include "support/gls_fixture.h"

namespace goldendict::core::formats::gls {
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

class GlsDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesHtmlIdentitySuggestionsAndResources();
    void RejectsCancellationAndUnsafeResources();
    void BuildsArticleOrdinalFullTextIndexFromInertArticles();
    void ReusesAndRebuildsFullTextIndexForOnlyTheSource();
    void SearchesCompressedTextAndContainsFullTextFailures();
};

void GlsDictionaryTest::ExposesHtmlIdentitySuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteGlsFixture(
        root,
        {{{"example"}, "<b>definition</b> <img src=\"images/pixel.png\">"}});
    test::WriteGlsResource(path, "images/pixel.png", "png-data");
    const Dictionary dictionary = Dictionary::Open("gls-id", path);

    const auto articles = dictionary.LookupExact("EXAMPLE");
    const auto suggestions = dictionary.SuggestPrefix("exa");
    const auto resource = dictionary.GetResource("images/pixel.png");

    QCOMPARE(dictionary.identity().id, "gls-id");
    QCOMPARE(dictionary.identity().name, "Fixture GLS");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"example"}));
    QCOMPARE(dictionary.identity().source_language, "eng");
    QCOMPARE(dictionary.identity().target_language, "deu");
    QCOMPARE(dictionary.identity().description,
             "Author: GoldenDict tests\n\nFixture description");
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(suggestions.front(), "example");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void GlsDictionaryTest::BuildsArticleOrdinalFullTextIndexFromInertArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteGlsFixture(
        root,
        {{{"canonical", "aliasonlysecret"},
          "<b>visible searchable</b> <a href=\"linktargetsecret\">label</a> "
          "<img src=\"images/resourcesecret.png\">"},
         {{"second"}, "other searchable"}});
    test::WriteGlsResource(path, "images/resourcesecret.png",
                           "resourcecontentsecret");
    test::WriteGlsResource(path, "fixture.gls.files/hidden.txt",
                           "filescontentsecret");
    const Dictionary dictionary =
        Dictionary::Open("gls-id", path, root / "fixture.gdfts");

    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(response.results.size(), std::size_t{2});
    QCOMPARE(response.results[0].headword, std::string("canonical"));
    QCOMPARE(response.results[0].document_id, std::string("gls-index:0:0"));
    QCOMPARE(response.results[1].headword, std::string("second"));
    QCOMPARE(response.results[1].document_id, std::string("gls-index:2:1"));
    for (const std::string excluded :
         {"aliasonlysecret", "linktargetsecret", "resourcesecret",
          "resourcecontentsecret", "filescontentsecret", "Fixture", "img"}) {
        query.text = excluded;
        QVERIFY2(dictionary.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
    query.text = "visible";
    QCOMPARE(dictionary.SearchFullText(query).results.size(), std::size_t{1});
}

void GlsDictionaryTest::ReusesAndRebuildsFullTextIndexForOnlyTheSource() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto path =
        test::WriteGlsFixture(root, {{{"entry"}, "first searchable text"}});
    const auto full_text_path = root / "fixture.gdfts";
    QCOMPARE(Dictionary::Open("gls-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(Dictionary::Open("gls-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));

    test::WriteGlsResource(path, "fixture.gls.files/resource.txt", "changed");
    QCOMPARE(Dictionary::Open("gls-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));
    path = test::WriteGlsFixture(
        root, {{{"entry"}, "second searchable text expanded"}});
    QCOMPARE(Dictionary::Open("gls-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    path = test::CompressGlsFixture(path);
    QCOMPARE(Dictionary::Open("gls-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ofstream(full_text_path, std::ios::binary | std::ios::trunc)
        << "corrupt";
    QCOMPARE(Dictionary::Open("gls-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));

    const Dictionary disabled = Dictionary::Open("gls-id", path);
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void GlsDictionaryTest::SearchesCompressedTextAndContainsFullTextFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto compressed_root = root / "compressed";
    const auto compressed_path = test::CompressGlsFixture(test::WriteGlsFixture(
        compressed_root, {{{"compressed"}, "compressed searchable text"}}));
    const Dictionary compressed = Dictionary::Open(
        "gls-id", compressed_path, compressed_root / "fixture.gdfts");
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
        test::WriteGlsFixture(storage_root, {{{"entry"}, "searchable"}});
    const auto storage_path = storage_root / "fixture.gdfts";
    QVERIFY(std::filesystem::create_directory(storage_path));
    const Dictionary storage =
        Dictionary::Open("storage-id", storage_source, storage_path);
    QCOMPARE(storage.LookupExact("entry").size(), std::size_t{1});
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);

    const auto oversized_root = root / "oversized";
    std::vector<test::GlsFixtureEntry> entries;
    entries.reserve(dictionary::kMaximumFullTextDocuments + 1U);
    for (std::size_t i = 0U; i <= dictionary::kMaximumFullTextDocuments; ++i) {
        entries.push_back({{"entry-" + std::to_string(i)}, "bounded"});
    }
    const auto oversized_source =
        test::WriteGlsFixture(oversized_root, entries);
    const Dictionary oversized = Dictionary::Open(
        "oversized-id", oversized_source, oversized_root / "fixture.gdfts");
    query.timeout = std::chrono::seconds(5);
    QCOMPARE(oversized.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kResourceLimit);
}

void GlsDictionaryTest::RejectsCancellationAndUnsafeResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Dictionary dictionary = Dictionary::Open(
        "gls-id", test::WriteGlsFixture(root, {{{"example"}, "definition"}}));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("../outside.txt").has_value());

    const auto oversized = root / "oversized.bin";
    std::ofstream(oversized, std::ios::binary).put('\0');
    std::filesystem::resize_file(oversized, 16U * 1024U * 1024U + 1U);
    QVERIFY_EXCEPTION_THROWN(dictionary.GetResource("oversized.bin"),
                             dictionary::Error);
}

}  // namespace
}  // namespace goldendict::core::formats::gls

using goldendict::core::formats::gls::GlsDictionaryTest;
QTEST_APPLESS_MAIN(GlsDictionaryTest)
#include "gls_dictionary_test.moc"
