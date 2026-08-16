// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <thread>

#include "../src/formats/sdict/sdict_dictionary.h"
#include "support/sdict_fixture.h"

namespace goldendict::core::formats::sdict {
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

class SdictDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesHtmlArticlesIdentityAndSuggestions();
    void HonorsCancellationAndHasNoResources();
    void BuildsDistinctOffsetFullTextIndexFromInertArticles();
    void ReusesAndRebuildsFullTextIndex();
    void ContainsFullTextStorageFailures();
    void SearchesCompressedTextAndContainsBoundedFailures();
};

void SdictDictionaryTest::ExposesHtmlArticlesIdentityAndSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path =
        test::WriteSdictFixture(root, {{"example", "<b>definition</b>"}});
    const Dictionary dictionary = Dictionary::Open("sdict-id", path);

    const auto articles = dictionary.LookupExact("EXAMPLE");
    const auto suggestions = dictionary.SuggestPrefix("EXA");

    QCOMPARE(dictionary.identity().id, "sdict-id");
    QCOMPARE(dictionary.identity().name, "Fixture SDict");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"example"}));
    QCOMPARE(dictionary.identity().source_language, "eng");
    QCOMPARE(dictionary.identity().target_language, "deu");
    QVERIFY(dictionary.identity().description.find("Fixture copyright") !=
            std::string::npos);
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(articles.front().data, "<b>definition</b>");
    QCOMPARE(suggestions.front(), "example");
}

void SdictDictionaryTest::HonorsCancellationAndHasNoResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Dictionary dictionary = Dictionary::Open(
        "sdict-id", test::WriteSdictFixture(root, {{"example", "definition"}}));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("missing").has_value());
}

void SdictDictionaryTest::BuildsDistinctOffsetFullTextIndexFromInertArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteSdictFixture(
        root, {{"canonical", "<b>visible searchable</b>"},
               {"alias", "ignored alias data", 0U},
               {"second", "other searchable"},
               {"suppressed", "<script>scriptsecret</script>"},
               {"link", "<a href=\"bword://targetsecret\">link label</a>"},
               {"resource", "<img src=\"resourcesecret.png\">"}});
    const auto full_text_path = root / "fixture.gdfts";
    const Dictionary dictionary =
        Dictionary::Open("sdict-id", path, full_text_path);

    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);

    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(response.results.size(), std::size_t{2});
    QCOMPARE(response.results[0].headword, std::string("canonical"));
    QVERIFY(response.results[0].document_id.rfind("sdict-index:0:", 0U) == 0U);
    QCOMPARE(response.results[1].headword, std::string("second"));
    QVERIFY(response.results[1].document_id.rfind("sdict-index:2:", 0U) == 0U);
    for (const std::string excluded :
         {"ignored", "scriptsecret", "targetsecret", "resourcesecret",
          "Fixture"}) {
        query.text = excluded;
        QVERIFY(dictionary.SearchFullText(query).results.empty());
    }
    query.text = "visible";
    QCOMPARE(dictionary.SearchFullText(query).results.size(), std::size_t{1});
}

void SdictDictionaryTest::ReusesAndRebuildsFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path =
        test::WriteSdictFixture(root, {{"entry", "first searchable text"}});
    const auto full_text_path = root / "fixture.gdfts";
    QCOMPARE(Dictionary::Open("sdict-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(Dictionary::Open("sdict-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));

    test::WriteSdictFixture(root,
                            {{"entry", "second searchable text expanded"}});
    QCOMPARE(Dictionary::Open("sdict-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ofstream(full_text_path, std::ios::binary | std::ios::trunc)
        << "corrupt";
    QCOMPARE(Dictionary::Open("sdict-id", path, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));

    const Dictionary disabled = Dictionary::Open("sdict-id", path);
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void SdictDictionaryTest::ContainsFullTextStorageFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path =
        test::WriteSdictFixture(root, {{"entry", "searchable text"}});
    const auto full_text_path = root / "fixture.gdfts";
    QVERIFY(std::filesystem::create_directory(full_text_path));

    const Dictionary dictionary =
        Dictionary::Open("sdict-id", path, full_text_path);
    QCOMPARE(dictionary.LookupExact("entry").size(), std::size_t{1});
    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QVERIFY(response.results.empty());
    QCOMPARE(response.errors.size(), std::size_t{1});
    QCOMPARE(response.errors.front().code, FullTextErrorCode::kInternal);
    QCOMPARE(response.errors.front().dictionary_id, std::string("sdict-id"));
}

void SdictDictionaryTest::SearchesCompressedTextAndContainsBoundedFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    for (const std::uint8_t compression : {std::uint8_t{1}, std::uint8_t{2}}) {
        const auto fixture_root =
            root / std::to_string(static_cast<unsigned int>(compression));
        const auto path = test::WriteSdictFixture(
            fixture_root, {{"compressed", "compressed searchable text"}},
            compression);
        const Dictionary dictionary =
            Dictionary::Open("sdict-id", path, fixture_root / "fixture.gdfts");
        FullTextQuery query;
        query.text = "searchable";
        QCOMPARE(dictionary.SearchFullText(query).results.size(),
                 std::size_t{1});
    }

    const auto path = test::WriteSdictFixture(root / "cancel",
                                              {{"entry", "searchable text"}});
    const Dictionary dictionary =
        Dictionary::Open("sdict-id", path, root / "cancel" / "fixture.gdfts");
    FullTextQuery query;
    query.text = "searchable";
    FullTextCancelledToken cancelled;
    QCOMPARE(dictionary.SearchFullText(query, &cancelled).errors.front().code,
             FullTextErrorCode::kCancelled);
    query.timeout = std::chrono::milliseconds(1);
    SlowFullTextToken slow;
    QCOMPARE(dictionary.SearchFullText(query, &slow).errors.front().code,
             FullTextErrorCode::kDeadlineExceeded);

    const auto oversized_path = test::WriteSdictFixture(
        root / "oversized",
        {{"oversized", std::string(16U * 1024U * 1024U + 1U, 'x')}});
    const Dictionary oversized = Dictionary::Open(
        "sdict-id", oversized_path, root / "oversized" / "fixture.gdfts");
    query.timeout = std::chrono::seconds(5);
    const auto oversized_response = oversized.SearchFullText(query);
    QVERIFY(oversized_response.results.empty());
    QCOMPARE(oversized_response.errors.front().code,
             FullTextErrorCode::kResourceLimit);
}

}  // namespace
}  // namespace goldendict::core::formats::sdict

using goldendict::core::formats::sdict::SdictDictionaryTest;
QTEST_APPLESS_MAIN(SdictDictionaryTest)
#include "sdict_dictionary_test.moc"
