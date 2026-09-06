// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "../src/formats/stardict/stardict_dictionary.h"
#include "support/stardict_fixture.h"

namespace goldendict::core::formats::stardict {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class StardictDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesIdentityAndBoundedArticles();
    void ExposesLegacyPrimaryAndHeadwordCounts();
    void ReturnsBoundedPrefixArticles();
    void ReturnsBoundedHeadwordSuggestions();
    void EnumeratesUniqueHeadwordsInLegacyOrder();
    void PreservesFormattedArticleData();
    void BuildsPrimaryOnlyFullTextIndexWithStableProvenance();
    void ReusesAndRebuildsFullTextIndex();
    void RebuildsFullTextIndexForCurrentStardictSemantics();
    void SkipsLegacyEmptyHeadwordsInFullTextIndex();
    void SearchesCompressedDictionaryText();
    void ContainsFullTextStorageFailures();
    void HonorsCancellationAndDeadline();
    void TranslatesReaderFailures();
    void LoadsTypedResourcesAndLegacyDelimiters();
    void ReturnsMissingResourceWithoutAnError();
    void RejectsUnsafeResourcePaths();
    void RejectsResourceSymlinkEscapes();
    void RejectsOversizedResources();
};

std::filesystem::path TemporaryPath(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
}

void StardictDictionaryTest::ExposesIdentityAndBoundedArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory),
        {{"example", "first"}, {"example", "second"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    dictionary::RequestOptions options;
    options.result_limit = 1;

    const auto articles = dictionary.LookupExact("example", options);
    auto preferred_info_path = info_path;
    preferred_info_path.make_preferred();

    QCOMPARE(dictionary.identity().id, "fixture-id");
    QCOMPARE(dictionary.identity().name, "Generated Test Dictionary en-en");
    QCOMPARE(dictionary.identity().source, preferred_info_path.string());
    QCOMPARE(dictionary.identity().article_count, std::size_t{2});
    QCOMPARE(dictionary.identity().headword_count, std::size_t{2});
    QCOMPARE(dictionary.identity().description,
             "Author: Fixture Author\n\nFixture description");
    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().headword, "example");
    QCOMPARE(articles.front().format, "text/plain");
    QCOMPARE(articles.front().data, "first");
}

void StardictDictionaryTest::ExposesLegacyPrimaryAndHeadwordCounts() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"first", "one"}, {"second", "two"}});
    test::WriteStardictSynonyms(
        info_path, {{"alias", 0U}, {"/discard$", 0U}, {"discard/$", 1U}});

    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    QCOMPARE(dictionary.identity().article_count, std::size_t{2});
    QCOMPARE(dictionary.identity().headword_count, std::size_t{5});
    QCOMPARE(dictionary.LookupExact("alias").front().data, "one");
    QVERIFY(dictionary.LookupExact("/discard$").empty());
    QVERIFY(dictionary.LookupExact("discard/$").empty());
}

void StardictDictionaryTest::ReturnsBoundedPrefixArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory),
        {{"example", "exact"}, {"examples", "prefix"}, {"examine", "other"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    dictionary::RequestOptions options;
    options.result_limit = 2U;

    const auto articles = dictionary.LookupPrefix("EXAMPLE", options);

    QCOMPARE(articles.size(), std::size_t{2});
    QCOMPARE(articles[0].data, "exact");
    QCOMPARE(articles[1].data, "prefix");
}

void StardictDictionaryTest::ReturnsBoundedHeadwordSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory),
        {{"example", "exact"}, {"examples", "prefix"}, {"examine", "other"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    dictionary::RequestOptions options;
    options.result_limit = 2U;

    const auto suggestions = dictionary.SuggestPrefix("EXAMPLE", options);

    QCOMPARE(suggestions.size(), std::size_t{2});
    QCOMPARE(suggestions[0], "example");
    QCOMPARE(suggestions[1], "examples");
}

void StardictDictionaryTest::EnumeratesUniqueHeadwordsInLegacyOrder() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory), {{"zebra", "one"},
                                   {"Apple", "two"},
                                   {"apple", "three"},
                                   {"Apple", "duplicate"},
                                   {"\xf0\x90\x80\x80", "non-bmp"},
                                   {"\xee\x80\x80", "bmp"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    dictionary::RequestOptions options;
    options.result_limit = 2U;

    const auto first = dictionary.EnumerateHeadwords(0U, options);
    const auto second = dictionary.EnumerateHeadwords(2U, options);
    const auto third = dictionary.EnumerateHeadwords(4U, options);

    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(first.headwords, (std::vector<std::string>{"Apple", "apple"}));
    QVERIFY(!first.complete);
    QCOMPARE(second.headwords,
             (std::vector<std::string>{"zebra", "\xf0\x90\x80\x80"}));
    QVERIFY(!second.complete);
    QCOMPARE(third.headwords, (std::vector<std::string>{"\xee\x80\x80"}));
    QVERIFY(third.complete);
}

void StardictDictionaryTest::PreservesFormattedArticleData() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto html =
        "<p><b>Example</b> <a href=\"bword://linked\">linked</a>"
        "<img src=\"images/pixel.png\"></p>";
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", html}}, "h");
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    const auto articles = dictionary.LookupExact("example");

    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(articles.front().data, html);
}

void StardictDictionaryTest::
    BuildsPrimaryOnlyFullTextIndexWithStableProvenance() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root,
        {{"plain", "primary searchable text"},
         {"html", "<script>hidden term</script><b>HTML searchable</b>"}},
        "h");
    test::WriteStardictSynonyms(info_path, {{"alias", 0U}});
    const auto full_text_path = root / "fixture.gdfts";
    const Dictionary dictionary = Dictionary::Open(
        "fixture-id", info_path, root / "fixture.gdidx", full_text_path);

    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);

    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(response.results.size(), 2U);
    QCOMPARE(response.results[0].dictionary.id, std::string("fixture-id"));
    QCOMPARE(response.results[0].headword, std::string("html"));
    QVERIFY(response.results[0].document_id.rfind("stardict-idx:1:", 0U) == 0U);
    QCOMPARE(response.results[1].headword, std::string("plain"));
    QVERIFY(response.results[1].document_id.rfind("stardict-idx:0:", 0U) == 0U);
    query.text = "hidden";
    QVERIFY(dictionary.SearchFullText(query).results.empty());
    query.text = "primary";
    const auto primary = dictionary.SearchFullText(query);
    QCOMPARE(primary.results.size(), 1U);
    QCOMPARE(primary.results.front().headword, std::string("plain"));
}

void StardictDictionaryTest::ReusesAndRebuildsFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"entry", "first text"}});
    const auto generated_path = root / "fixture.gdidx";
    const auto full_text_path = root / "fixture.gdfts";
    QCOMPARE(Dictionary::Open("fixture-id", info_path, generated_path,
                              full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(Dictionary::Open("fixture-id", info_path, generated_path,
                              full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));

    test::WriteStardictFixture(root, {{"entry", "second text"}});
    QCOMPARE(Dictionary::Open("fixture-id", info_path, generated_path,
                              full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    test::WriteBinaryFile(full_text_path, "corrupt");
    QCOMPARE(Dictionary::Open("fixture-id", info_path, generated_path,
                              full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));
}

void StardictDictionaryTest::
    RebuildsFullTextIndexForCurrentStardictSemantics() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"entry", "searchable text"}});
    const auto full_text_path = root / "fixture.gdfts";
    const Reader reader = Reader::Open(info_path);
    dictionary::FullTextDocument old_document;
    old_document.dictionary.id = "fixture-id";
    old_document.headword = "obsolete";
    old_document.document_id = "obsolete:0";
    old_document.plain_text = "obsolete text";
    QCOMPARE(dictionary::FullTextIndex::OpenOrBuild(
                 full_text_path, reader.source_snapshot(), {old_document})
                 .state(),
             dictionary::FullTextIndexState::kCreated);

    const Dictionary dictionary = Dictionary::Open(
        "fixture-id", info_path, root / "fixture.gdidx", full_text_path);

    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(dictionary.SearchFullText(query).results.size(), 1U);
    query.text = "obsolete";
    QVERIFY(dictionary.SearchFullText(query).results.empty());
}

void StardictDictionaryTest::SkipsLegacyEmptyHeadwordsInFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root, {{"broken&#65;", "hidden text"}, {"visible", "searchable text"}});
    const Dictionary dictionary =
        Dictionary::Open("fixture-id", info_path, root / "fixture.gdidx",
                         root / "fixture.gdfts");

    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QCOMPARE(response.results.size(), 1U);
    QCOMPARE(response.results.front().headword, std::string("visible"));
}

void StardictDictionaryTest::SearchesCompressedDictionaryText() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root, {{"compressed", "compressed searchable text"}});
    test::CompressStardictDictionary(info_path);
    std::filesystem::remove(root / "fixture.dict");
    const Dictionary dictionary =
        Dictionary::Open("fixture-id", info_path, root / "fixture.gdidx",
                         root / "fixture.gdfts");
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(dictionary.SearchFullText(query).results.size(), 1U);
}

void StardictDictionaryTest::ContainsFullTextStorageFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"entry", "searchable text"}});
    const auto full_text_path = root / "fixture.gdfts";
    QVERIFY(std::filesystem::create_directory(full_text_path));

    const Dictionary dictionary = Dictionary::Open(
        "fixture-id", info_path, root / "fixture.gdidx", full_text_path);
    QCOMPARE(dictionary.LookupExact("entry").size(), 1U);
    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QVERIFY(response.results.empty());
    QCOMPARE(response.errors.size(), 1U);
    QCOMPARE(response.errors.front().code, FullTextErrorCode::kInternal);
    QCOMPARE(response.errors.front().dictionary_id, std::string("fixture-id"));
}

void StardictDictionaryTest::HonorsCancellationAndDeadline() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", "article"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    CancelledSignal cancellation;
    dictionary::RequestOptions cancelled;
    cancelled.cancellation = &cancellation;

    try {
        static_cast<void>(dictionary.LookupExact("example", cancelled));
        QFAIL("LookupExact should honor cancellation");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kCancelled);
    }
    try {
        static_cast<void>(dictionary.GetResource("resource.bin", cancelled));
        QFAIL("GetResource should honor cancellation");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kCancelled);
    }

    dictionary::RequestOptions expired;
    expired.deadline = std::chrono::steady_clock::time_point::min();
    try {
        static_cast<void>(dictionary.LookupExact("example", expired));
        QFAIL("LookupExact should honor an expired deadline");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kDeadlineExceeded);
    }
}

void StardictDictionaryTest::TranslatesReaderFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto missing = TemporaryPath(directory) / "missing.ifo";

    try {
        static_cast<void>(Dictionary::Open("missing", missing));
        QFAIL("Dictionary::Open should translate missing input");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kUnavailable);
    }
}

void StardictDictionaryTest::LoadsTypedResourcesAndLegacyDelimiters() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const std::string image_data("\x89PNG\r\n\x1a\nfixture", 15);
    test::WriteStardictResource(root, "images/pixel.png", image_data);
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    const auto resource = dictionary.GetResource("\x1eimages/pixel.png\x1f");

    QVERIFY(resource.has_value());
    QCOMPARE(resource->id, "images/pixel.png");
    QCOMPARE(resource->media_type, "image/png");
    const std::string loaded(
        reinterpret_cast<const char*>(resource->data.data()),
        resource->data.size());
    QCOMPARE(loaded, image_data);
}

void StardictDictionaryTest::ReturnsMissingResourceWithoutAnError() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", "article"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    QVERIFY(!dictionary.GetResource("images/missing.png").has_value());
}

void StardictDictionaryTest::RejectsUnsafeResourcePaths() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    test::WriteStardictResource(root, "safe.txt", "safe");
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    const std::vector<std::string> unsafe_paths = {
        "../fixture.dict", "images/../../fixture.dict", "/etc/passwd",
        "..\\fixture.dict"};
    for (const auto& path : unsafe_paths) {
        try {
            static_cast<void>(dictionary.GetResource(path));
            QFAIL("GetResource should reject an unsafe path");
        } catch (const dictionary::Error& error) {
            QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
        }
    }
}

void StardictDictionaryTest::RejectsResourceSymlinkEscapes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto outside = root / "outside.txt";
    test::WriteBinaryFile(outside, "outside");
    const auto resource_root = root / "res";
    QVERIFY(std::filesystem::create_directory(resource_root));
    std::error_code symlink_error;
    std::filesystem::create_symlink(outside, resource_root / "escape.txt",
                                    symlink_error);
    if (symlink_error)
        QSKIP("File symlink creation is unavailable");
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    try {
        static_cast<void>(dictionary.GetResource("escape.txt"));
        QFAIL("GetResource should reject a symlink escape");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

void StardictDictionaryTest::RejectsOversizedResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto resource = test::WriteStardictResource(root, "large.bin", "");
    std::filesystem::resize_file(resource, 64U * 1024U * 1024U + 1U);
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    try {
        static_cast<void>(dictionary.GetResource("large.bin"));
        QFAIL("GetResource should reject an oversized resource");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

}  // namespace
}  // namespace goldendict::core::formats::stardict

using goldendict::core::formats::stardict::StardictDictionaryTest;

QTEST_APPLESS_MAIN(StardictDictionaryTest)

#include "stardict_dictionary_test.moc"
