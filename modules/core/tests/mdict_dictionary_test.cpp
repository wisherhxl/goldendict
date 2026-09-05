// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <thread>

#include "../src/formats/mdict/mdict_dictionary.h"
#include "support/mdict_fixture.h"

namespace goldendict::core::formats::mdict {
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

class MdictDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlSuggestionsAndResources();
    void HonorsCancellationAndUnknownResources();
    void BuildsOwnedTerminalFullTextIndex();
    void TracksCompleteMdxMddRevision();
    void ContainsFullTextFailures();
};

void MdictDictionaryTest::ExposesIdentityHtmlSuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto files = test::WriteMdictFixture(
        std::filesystem::path(directory.path().toStdString()));
    const Dictionary dictionary = Dictionary::Open("mdict-id", files);

    QCOMPARE(dictionary.identity().name, "Fixture MDict");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"alias", "example"}));
    QCOMPARE(dictionary.identity().description, "Fixture description");
    const auto exact = dictionary.LookupExact("example");
    QCOMPARE(exact.front().format, "text/html");
    constexpr std::string_view kMdictPrefix = "<div class=\"mdict\">";
    constexpr std::string_view kMdictSuffix = "</div>";
    QVERIFY(exact.front().data.compare(0U, kMdictPrefix.size(), kMdictPrefix) ==
            0);
    QVERIFY(exact.front().data.size() >= kMdictSuffix.size());
    QVERIFY(exact.front().data.compare(
                exact.front().data.size() - kMdictSuffix.size(),
                kMdictSuffix.size(), kMdictSuffix) == 0);
    QCOMPARE(dictionary.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "example");
    const auto resource = dictionary.GetResource("pixel.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{9});
}

void MdictDictionaryTest::HonorsCancellationAndUnknownResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto files = test::WriteMdictFixture(
        std::filesystem::path(directory.path().toStdString()));
    const Dictionary dictionary = Dictionary::Open("mdict-id", files);
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("missing.png").has_value());
    QVERIFY(!dictionary.GetResource("../pixel.png").has_value());

    std::ofstream mutation(files.mdd.front(), std::ios::binary | std::ios::app);
    mutation.put('\0');
    mutation.close();
    QVERIFY_EXCEPTION_THROWN(dictionary.GetResource("pixel.png"),
                             dictionary::Error);
}

void MdictDictionaryTest::BuildsOwnedTerminalFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto mdx = test::WriteMdictContainer(
        root / "ownership.mdx", "Ownership Fixture",
        {{"first alias", "@@@LINK=terminal"},
         {"terminal", "<b>terminal searchable</b><script>active</script>"},
         {"chain", "@@@LINK=FIRST ALIAS"},
         {"missing", "@@@LINK=absent"},
         {"cycle a", "@@@LINK=cycle b"},
         {"cycle b", "@@@LINK=cycle a"},
         {"Caf\xC3\xA9", "folded winner"},
         {"cafe", "folded collision"},
         {"folded alias", "@@@LINK=CAFE"},
         {"same one", "equal bytes searchable"},
         {"same two", "equal bytes searchable"},
         {"empty markup", "<script>only active</script>"},
         {"duplicate alias", "@@@LINK=terminal"}});
    const auto mdd =
        test::WriteMdictContainer(root / "ownership.mdd", "Resources",
                                  {{"\\hidden.txt", "resource secret"}});
    const auto index = root / "ownership.gdfts";
    Dictionary dictionary = Dictionary::Open("mdict-id", {mdx, {mdd}}, index);
    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));

    FullTextQuery query;
    query.text = "terminal";
    const auto terminal = dictionary.SearchFullText(query);
    QCOMPARE(terminal.results.size(), std::size_t{1});
    QCOMPARE(terminal.results.front().headword, std::string("first alias"));
    const auto view = Reader::Open({mdx, {mdd}}).ReadIngestionView();
    const auto& owner = view.articles.front();
    const std::string expected =
        "mdict-index:0:0:1:" + std::to_string(owner.terminal.record_offset) +
        ":" + std::to_string(owner.terminal.record_size);
    QCOMPARE(terminal.results.front().document_id, expected);

    query.text = "equal bytes";
    const auto equal = dictionary.SearchFullText(query);
    QCOMPARE(equal.results.size(), std::size_t{2});
    QVERIFY(equal.results[0].document_id != equal.results[1].document_id);
    QVERIFY(equal.results[0].document_id.find("mdict-index:9:3:9:") == 0U);
    QVERIFY(equal.results[1].document_id.find("mdict-index:10:4:10:") == 0U);
    for (const std::string excluded :
         {"chain", "missing", "cycle", "active", "resource secret",
          "hidden.txt", "empty markup", "text/html", "<b>"}) {
        query.text = excluded;
        QVERIFY2(dictionary.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
    QCOMPARE(Dictionary::Open("mdict-id", {mdx, {mdd}}, index)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));
    std::ofstream(index, std::ios::binary | std::ios::trunc) << "corrupt";
    QCOMPARE(Dictionary::Open("mdict-id", {mdx, {mdd}}, index)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));
    const Dictionary disabled = Dictionary::Open("mdict-id", {mdx, {mdd}});
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void MdictDictionaryTest::TracksCompleteMdxMddRevision() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto mdx = test::WriteMdictContainer(root / "fixture.mdx", "Fixture",
                                         {{"word", "searchable article"}});
    auto base = test::WriteMdictContainer(root / "fixture.mdd", "Resources",
                                          {{"\\one", "resource one"}});
    auto numbered = test::WriteMdictContainer(
        root / "fixture.1.mdd", "Resources 1", {{"\\two", "resource two"}});
    const auto index = root / "fixture.gdfts";
    auto files = Discover({root}).dictionaries.front();
    QCOMPARE(Dictionary::Open("mdict-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(Dictionary::Open("mdict-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));

    test::WriteMdictContainer(mdx, "Fixture Changed",
                              {{"word", "searchable article"}});
    files = Discover({root}).dictionaries.front();
    QCOMPARE(Dictionary::Open("mdict-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    test::WriteMdictContainer(base, "Resources Changed",
                              {{"\\one", "resource changed"}});
    files = Discover({root}).dictionaries.front();
    QCOMPARE(Dictionary::Open("mdict-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    test::WriteMdictContainer(numbered, "Resources 1 Changed",
                              {{"\\two", "resource changed"}});
    files = Discover({root}).dictionaries.front();
    QCOMPARE(Dictionary::Open("mdict-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    test::WriteMdictContainer(root / "fixture.2.mdd", "Resources 2",
                              {{"\\three", "resource three"}});
    files = Discover({root}).dictionaries.front();
    QCOMPARE(Dictionary::Open("mdict-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    QVERIFY(std::filesystem::remove(root / "fixture.2.mdd"));
    files = Discover({root}).dictionaries.front();
    QCOMPARE(Dictionary::Open("mdict-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
}

void MdictDictionaryTest::ContainsFullTextFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto files = test::WriteMdictFixture(root);
    const Dictionary dictionary =
        Dictionary::Open("mdict-id", files, root / "fixture.gdfts");
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
    const Dictionary storage = Dictionary::Open("storage-id", files, blocked);
    QCOMPARE(storage.LookupExact("example").size(), std::size_t{1});
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);
}

}  // namespace
}  // namespace goldendict::core::formats::mdict

using goldendict::core::formats::mdict::MdictDictionaryTest;
QTEST_APPLESS_MAIN(MdictDictionaryTest)
#include "mdict_dictionary_test.moc"
