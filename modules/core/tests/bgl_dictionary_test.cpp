// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <set>
#include <thread>

#include "../src/formats/bgl/bgl_dictionary.h"
#include "support/bgl_fixture.h"

namespace goldendict::core::formats::bgl {
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

class BglDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlSuggestionsAndEmbeddedResources();
    void RejectsCancellationAndUnknownResources();
    void BuildsAndRebuildsOwnedArticleFullTextIndex();
    void ContainsFullTextFailures();
};

void BglDictionaryTest::ExposesIdentityHtmlSuggestionsAndEmbeddedResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "bgl-id", test::WriteBglFixture(
                      std::filesystem::path(directory.path().toStdString())));

    QCOMPARE(dictionary.identity().name, "Fixture BGL");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"alias", "example"}));
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.identity().target_language, "de");
    QVERIFY(dictionary.identity().description.find("Fixture description") !=
            std::string::npos);
    QCOMPARE(dictionary.LookupExact("EXAMPLE").front().format, "text/html");
    QCOMPARE(dictionary.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "example");
    const auto resource = dictionary.GetResource("pixel.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void BglDictionaryTest::RejectsCancellationAndUnknownResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "bgl-id", test::WriteBglFixture(
                      std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("missing.png").has_value());
    QVERIFY(!dictionary.GetResource(std::string("bad\0id", 6U)).has_value());
}

void BglDictionaryTest::BuildsAndRebuildsOwnedArticleFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto path = test::WriteBglFullTextFixture(root);
    const auto index = root / "fixture.gdfts";
    const Dictionary created = Dictionary::Open("bgl-id", path, index);
    QCOMPARE(created.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(created.LookupExact("fourth-alias").size(), std::size_t{1});
    QVERIFY(created.GetResource("resource-name-secret.png").has_value());

    FullTextQuery query;
    query.text = "visible";
    const auto response = created.SearchFullText(query);
    QCOMPARE(response.results.size(), std::size_t{1});
    QCOMPARE(response.results.front().headword, std::string("first-owner"));
    QCOMPARE(response.results.front().document_id,
             std::string("bgl-index:0:0"));

    query.text = "layout";
    const auto layouts = created.SearchFullText(query);
    QCOMPARE(layouts.results.size(), std::size_t{2});
    const std::set<std::string> layout_ids{layouts.results[0].document_id,
                                           layouts.results[1].document_id};
    QCOMPARE(layout_ids,
             (std::set<std::string>{"bgl-index:3:2", "bgl-index:4:3"}));
    for (const std::string excluded :
         {"first-owner", "fourth-alias", "Metadata", "link-target-secret",
          "resource-name-secret", "resource-bytes-secret", "unreferenced",
          "safe"}) {
        query.text = excluded;
        QVERIFY2(created.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
    QCOMPARE(Dictionary::Open("bgl-id", path, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));
    path = test::WriteBglFullTextFixture(
        root,
        "<b>visible searchable definition</b>"
        "<a href=\"link-target-secret\">safe label</a>"
        "<img src=\"resource-name-secret.png\">",
        "changed-resource-bytes");
    QCOMPARE(Dictionary::Open("bgl-id", path, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    path = test::WriteBglFullTextFixture(root, "replacement searchable text");
    const Dictionary replaced = Dictionary::Open("bgl-id", path, index);
    QCOMPARE(replaced.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    query.text = "replacement";
    QCOMPARE(replaced.SearchFullText(query).results.size(), std::size_t{1});
    std::ofstream(index, std::ios::binary | std::ios::trunc) << "corrupt";
    QCOMPARE(Dictionary::Open("bgl-id", path, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));
    const Dictionary disabled = Dictionary::Open("bgl-id", path);
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void BglDictionaryTest::ContainsFullTextFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteBglFullTextFixture(root);
    const Dictionary dictionary =
        Dictionary::Open("bgl-id", path, root / "fixture.gdfts");
    FullTextQuery query;
    query.text = "searchable";
    FullTextCancelledToken cancelled;
    QCOMPARE(dictionary.SearchFullText(query, &cancelled).errors.front().code,
             FullTextErrorCode::kCancelled);
    query.timeout = std::chrono::milliseconds(1);
    SlowFullTextToken slow;
    QCOMPARE(dictionary.SearchFullText(query, &slow).errors.front().code,
             FullTextErrorCode::kDeadlineExceeded);

    const auto blocked = root / "blocked.gdfts";
    QVERIFY(std::filesystem::create_directory(blocked));
    const Dictionary storage = Dictionary::Open("storage-id", path, blocked);
    QCOMPARE(storage.LookupExact("first-owner").size(), std::size_t{1});
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);
}

}  // namespace
}  // namespace goldendict::core::formats::bgl

using goldendict::core::formats::bgl::BglDictionaryTest;
QTEST_APPLESS_MAIN(BglDictionaryTest)
#include "bgl_dictionary_test.moc"
