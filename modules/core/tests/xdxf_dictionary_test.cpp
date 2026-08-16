// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <thread>

#include "../src/formats/xdxf/xdxf_dictionary.h"
#include "support/xdxf_fixture.h"

namespace goldendict::core::formats::xdxf {
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

class XdxfDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesHtmlIdentitySuggestionsAndResources();
    void RejectsCancellationAndUnsafeResources();
    void BuildsArticleOrdinalFullTextIndexFromInertArticles();
    void ReusesAndRebuildsFullTextIndex();
    void ContainsFullTextStorageFailures();
    void SearchesCompressedTextAndContainsBoundedFailures();
};

void XdxfDictionaryTest::ExposesHtmlIdentitySuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteXdxfFixture(
        root,
        {{{"example"}, "<def>definition <rref>images/pixel.png</rref></def>"}});
    test::WriteXdxfResource(path, "images/pixel.png", "png-data");
    const Dictionary dictionary = Dictionary::Open("xdxf-id", path);

    const auto articles = dictionary.LookupExact("EXAMPLE");
    const auto suggestions = dictionary.SuggestPrefix("exa");
    const auto resource = dictionary.GetResource("images/pixel.png");

    QCOMPARE(dictionary.identity().id, "xdxf-id");
    QCOMPARE(dictionary.identity().name, "Fixture XDXF");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"example"}));
    QCOMPARE(dictionary.identity().source_language, "eng");
    QCOMPARE(dictionary.identity().target_language, "deu");
    QCOMPARE(dictionary.identity().description, "Fixture description");
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(suggestions.front(), "example");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void XdxfDictionaryTest::RejectsCancellationAndUnsafeResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Dictionary dictionary = Dictionary::Open(
        "xdxf-id",
        test::WriteXdxfFixture(root, {{{"example"}, "<def>definition</def>"}}));
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

void XdxfDictionaryTest::BuildsArticleOrdinalFullTextIndexFromInertArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteXdxfFixture(
        root, {{{"canonical", "aliasonlysecret"},
                "<def><b>visible searchable</b> <kref>linklabel</kref>"
                "<rref>images/resourcesecret.png</rref></def>"},
               {{"second"}, "<def>other searchable</def>"}});
    test::WriteXdxfResource(path, "images/resourcesecret.png",
                            "resourcecontentsecret");
    const auto full_text_path = root / "fixture.gdfts";
    const Dictionary dictionary =
        Dictionary::Open("xdxf-id", path, full_text_path);

    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);

    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(response.results.size(), std::size_t{2});
    QCOMPARE(response.results[0].headword, std::string("canonical"));
    QCOMPARE(response.results[0].document_id, std::string("xdxf-index:0:0"));
    QCOMPARE(response.results[1].headword, std::string("second"));
    QCOMPARE(response.results[1].document_id, std::string("xdxf-index:2:1"));
    for (const std::string excluded :
         {"aliasonlysecret", "bword", "resourcesecret", "resourcecontentsecret",
          "Fixture", "def"}) {
        query.text = excluded;
        QVERIFY2(dictionary.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
    query.text = "visible";
    QCOMPARE(dictionary.SearchFullText(query).results.size(), std::size_t{1});
}

void XdxfDictionaryTest::ReusesAndRebuildsFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteXdxfFixture(
        root, {{{"entry"}, "<def>first searchable text</def>"}});
    const auto full_text_path = root / "fixture.gdfts";
    QCOMPARE(Dictionary::Open("xdxf-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(Dictionary::Open("xdxf-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));

    test::WriteXdxfFixture(
        root, {{{"entry"}, "<def>second searchable text expanded</def>"}});
    QCOMPARE(Dictionary::Open("xdxf-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ofstream(full_text_path, std::ios::binary | std::ios::trunc)
        << "corrupt";
    QCOMPARE(Dictionary::Open("xdxf-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));

    const Dictionary disabled = Dictionary::Open("xdxf-id", path);
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void XdxfDictionaryTest::ContainsFullTextStorageFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteXdxfFixture(
        root, {{{"entry"}, "<def>searchable text</def>"}});
    const auto full_text_path = root / "fixture.gdfts";
    QVERIFY(std::filesystem::create_directory(full_text_path));

    const Dictionary dictionary =
        Dictionary::Open("xdxf-id", path, full_text_path);
    QCOMPARE(dictionary.LookupExact("entry").size(), std::size_t{1});
    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QVERIFY(response.results.empty());
    QCOMPARE(response.errors.size(), std::size_t{1});
    QCOMPARE(response.errors.front().code, FullTextErrorCode::kInternal);
    QCOMPARE(response.errors.front().dictionary_id, std::string("xdxf-id"));
}

void XdxfDictionaryTest::SearchesCompressedTextAndContainsBoundedFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto compressed_root = root / "compressed";
    const auto compressed_path =
        test::CompressXdxfFixture(test::WriteXdxfFixture(
            compressed_root,
            {{{"compressed"}, "<def>compressed searchable text</def>"}}));
    const Dictionary compressed = Dictionary::Open(
        "xdxf-id", compressed_path, compressed_root / "fixture.gdfts");
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

    const auto oversized_root = root / "oversized";
    std::vector<test::XdxfFixtureEntry> entries;
    entries.reserve(dictionary::kMaximumFullTextDocuments + 1U);
    for (std::size_t i = 0U; i <= dictionary::kMaximumFullTextDocuments; ++i) {
        entries.push_back(
            {{"entry-" + std::to_string(i)}, "<def>bounded</def>"});
    }
    const auto oversized_path = test::WriteXdxfFixture(oversized_root, entries);
    const Dictionary oversized = Dictionary::Open(
        "xdxf-id", oversized_path, oversized_root / "fixture.gdfts");
    query.timeout = std::chrono::seconds(5);
    const auto oversized_response = oversized.SearchFullText(query);
    QVERIFY(oversized_response.results.empty());
    QCOMPARE(oversized_response.errors.front().code,
             FullTextErrorCode::kResourceLimit);
}

}  // namespace
}  // namespace goldendict::core::formats::xdxf

using goldendict::core::formats::xdxf::XdxfDictionaryTest;
QTEST_APPLESS_MAIN(XdxfDictionaryTest)
#include "xdxf_dictionary_test.moc"
