// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/aard/aard_dictionary.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include <thread>
#include "support/aard_fixture.h"

namespace goldendict::core::formats::aard {
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

class AardDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlAndSuggestions();
    void RejectsCancellationAndHasNoResources();
    void BuildsAndRebuildsUniqueArticleFullTextIndex();
    void ContainsFullTextFailures();
};

void AardDictionaryTest::ExposesIdentityHtmlAndSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "aard-id", test::WriteAardFixture(
                       std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(dictionary.identity().name, "Fixture Aard");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"alias", "example", "redirect"}));
    QCOMPARE(dictionary.identity().description, "fixture");
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.identity().target_language, "de");
    QCOMPARE(dictionary.LookupExact("example").front().format, "text/html");
    QCOMPARE(dictionary.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "example");
}

void AardDictionaryTest::RejectsCancellationAndHasNoResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "aard-id", test::WriteAardFixture(
                       std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;
    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("missing.png").has_value());
}

void AardDictionaryTest::BuildsAndRebuildsUniqueArticleFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto path = test::WriteAardFixture(root, "fixture.aar", false, false, false,
                                       "[\"<b>visible searchable definition</b>"
                                       "<a href=\\\"w:alias\\\">label</a>\"]");
    const auto index = root / "fixture.gdfts";
    const Dictionary created = Dictionary::Open("aard-id", path, index);
    QCOMPARE(created.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    FullTextQuery query;
    query.text = "searchable";
    const auto response = created.SearchFullText(query);
    QCOMPARE(response.results.size(), std::size_t{1});
    QCOMPARE(response.results.front().headword, std::string("example"));
    QCOMPARE(response.results.front().document_id,
             std::string("aard-index:0:0"));
    for (const std::string excluded : {"alias", "bword", "Fixture"}) {
        query.text = excluded;
        QVERIFY2(created.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
    query.text = "example";
    const auto redirect = created.SearchFullText(query);
    QCOMPARE(redirect.results.size(), std::size_t{1});
    QCOMPARE(redirect.results.front().document_id,
             std::string("aard-index:2:1"));
    QCOMPARE(Dictionary::Open("aard-id", path, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));
    path = test::WriteAardFixture(root, "fixture.aar", true, true);
    QCOMPARE(Dictionary::Open("aard-id", path, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ofstream(index, std::ios::binary | std::ios::trunc) << "corrupt";
    QCOMPARE(Dictionary::Open("aard-id", path, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));
    const Dictionary disabled = Dictionary::Open("aard-id", path);
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void AardDictionaryTest::ContainsFullTextFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteAardFixture(root);
    const Dictionary dictionary =
        Dictionary::Open("aard-id", path, root / "fixture.gdfts");
    FullTextQuery query;
    query.text = "definition";
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
    QCOMPARE(storage.LookupExact("example").size(), std::size_t{1});
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);
}
}  // namespace
}  // namespace goldendict::core::formats::aard

using goldendict::core::formats::aard::AardDictionaryTest;
QTEST_APPLESS_MAIN(AardDictionaryTest)
#include "aard_dictionary_test.moc"
