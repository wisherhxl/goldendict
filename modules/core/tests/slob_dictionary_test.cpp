// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/slob/slob_dictionary.h"
#include <QtTest>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include "support/slob_fixture.h"

namespace goldendict::core::formats::slob {
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

class SlobDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityArticlesSuggestionsAndResources();
    void RejectsCancellation();
    void BuildsOwnedItemBinFullTextIndex();
    void ReusesAndRebuildsFullTextIndex();
    void ContainsFullTextFailures();
};

void SlobDictionaryTest::ExposesIdentityArticlesSuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "slob-id", test::WriteSlobFixture(
                       std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(dictionary.identity().name, "Fixture SLOB");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"alias", "example"}));
    QCOMPARE(dictionary.identity().description, "fixture");
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.identity().target_language, "de");
    QCOMPARE(dictionary.LookupExact("alias").front().format, "text/html");
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "example");
    const auto resource = dictionary.GetResource("pixel.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void SlobDictionaryTest::RejectsCancellation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "slob-id", test::WriteSlobFixture(
                       std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;
    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
}

void SlobDictionaryTest::BuildsOwnedItemBinFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteSlobFullTextFixture(root);
    const Dictionary dictionary =
        Dictionary::Open("slob-id", path, root / "fixture.gdfts");
    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(dictionary.LookupExact("first-alias-secret").size(),
             std::size_t{1});

    FullTextQuery query;
    query.text = "visible";
    const auto visible = dictionary.SearchFullText(query);
    QCOMPARE(visible.results.size(), std::size_t{2});
    QCOMPARE(visible.results[0].headword, std::string("first-owner"));
    QCOMPARE(visible.results[0].document_id, std::string("slob-index:0:0:0:0"));
    QCOMPARE(visible.results[1].headword, std::string("plain-owner"));
    QCOMPARE(visible.results[1].document_id, std::string("slob-index:2:1:0:1"));

    query.text = "article 10";
    const auto multi_digit = dictionary.SearchFullText(query);
    QCOMPARE(multi_digit.results.size(), std::size_t{1});
    QCOMPARE(multi_digit.results.front().headword, std::string("owner-10"));
    QCOMPARE(multi_digit.results.front().document_id,
             std::string("slob-index:13:11:10:0"));

    query.text = "article 1";
    const auto same_bin_other_item = dictionary.SearchFullText(query);
    QVERIFY(std::any_of(same_bin_other_item.results.begin(),
                        same_bin_other_item.results.end(), [](const auto& hit) {
                            return hit.document_id == "slob-index:3:2:1:0";
                        }));
    for (const std::string excluded :
         {"first-alias-secret", "plain-alias-secret", "owner-10-alias-secret",
          "metadata-secret", "resource-name-secret", "resource-bytes-secret",
          "link-target-secret", "raw-markup-secret", "text/html"}) {
        query.text = excluded;
        QVERIFY2(dictionary.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
}

void SlobDictionaryTest::ReusesAndRebuildsFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto path = test::WriteSlobFullTextFixture(root);
    const auto index = root / "fixture.gdfts";
    Dictionary::Open("slob-id", path, index);
    QCOMPARE(Dictionary::Open("slob-id", path, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));
    path = test::WriteSlobFullTextFixture(
        root, "<b>visible searchable definition</b>", "changed-resource");
    QCOMPARE(Dictionary::Open("slob-id", path, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    path = test::WriteSlobFullTextFixture(root, "replacement searchable text");
    const Dictionary replaced = Dictionary::Open("slob-id", path, index);
    QCOMPARE(replaced.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    FullTextQuery query;
    query.text = "replacement";
    QCOMPARE(replaced.SearchFullText(query).results.size(), std::size_t{1});
    std::ofstream(index, std::ios::binary | std::ios::trunc) << "corrupt";
    QCOMPARE(Dictionary::Open("slob-id", path, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));
    const Dictionary disabled = Dictionary::Open("slob-id", path);
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void SlobDictionaryTest::ContainsFullTextFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteSlobFullTextFixture(root);
    const Dictionary dictionary =
        Dictionary::Open("slob-id", path, root / "fixture.gdfts");
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
}  // namespace goldendict::core::formats::slob

using goldendict::core::formats::slob::SlobDictionaryTest;
QTEST_APPLESS_MAIN(SlobDictionaryTest)
#include "slob_dictionary_test.moc"
