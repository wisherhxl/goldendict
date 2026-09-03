// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/zim/zim_dictionary.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include <thread>
#include "support/zim_fixture.h"

namespace goldendict::core::formats::zim {
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

class ZimDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityArticlesSuggestionsAndResources();
    void RejectsCancellation();
    void BuildsAndRebuildsTerminalFullTextIndex();
    void TracksCompleteSplitRevision();
    void ContainsFullTextFailures();
};

void ZimDictionaryTest::ExposesIdentityArticlesSuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = test::WriteZimFixture(
        std::filesystem::path(directory.path().toStdString()));
    const Dictionary dictionary = Dictionary::Open("zim-id", {path, {path}});
    QCOMPARE(dictionary.identity().name, "Fixture ZIM");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"Alias", "Example", "Plain"}));
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.LookupExact("alias").front().format, "text/html");
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "Example");
    const auto resource = dictionary.GetResource("I/pixel.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void ZimDictionaryTest::RejectsCancellation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = test::WriteZimFixture(
        std::filesystem::path(directory.path().toStdString()));
    const Dictionary dictionary = Dictionary::Open("zim-id", {path, {path}});
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;
    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
}

void ZimDictionaryTest::BuildsAndRebuildsTerminalFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto path = test::WriteZimFixture(root);
    const auto index = root / "fixture.gdfts";
    Dictionary created = Dictionary::Open("zim-id", {path, {path}}, index);
    QCOMPARE(created.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    FullTextQuery query;
    query.text = "definition";
    const auto result = created.SearchFullText(query);
    QCOMPARE(result.results.size(), std::size_t{1});
    QCOMPARE(result.results.front().headword, std::string("Example"));
    QCOMPARE(result.results.front().document_id,
             std::string("zim-index:0:0:0:0:0"));
    query.text = "plain";
    const auto plain = created.SearchFullText(query);
    QCOMPARE(plain.results.size(), std::size_t{1});
    QCOMPARE(plain.results.front().document_id,
             std::string("zim-index:2:1:5:0:4"));
    for (const std::string excluded :
         {"Alias", "pixel.png", "png-data", "text/html", "<b>"}) {
        query.text = excluded;
        QVERIFY2(created.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
    QCOMPARE(Dictionary::Open("zim-id", {path, {path}}, index)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));
    path = test::WriteZimFixture(root, "fixture.zim", 1U, false,
                                 "<b>definition</b><img src=\"I/pixel.png\">",
                                 "changed-resource");
    QCOMPARE(Dictionary::Open("zim-id", {path, {path}}, index)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    path = test::WriteZimFixture(root, "fixture.zim", 2U, true);
    QCOMPARE(Dictionary::Open("zim-id", {path, {path}}, index)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ofstream(index, std::ios::binary | std::ios::trunc) << "corrupt";
    QCOMPARE(Dictionary::Open("zim-id", {path, {path}}, index)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));
    const Dictionary disabled = Dictionary::Open("zim-id", {path, {path}});
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void ZimDictionaryTest::TracksCompleteSplitRevision() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto source = test::WriteZimFixture(root);
    std::string data;
    {
        std::ifstream input(source, std::ios::binary);
        QVERIFY(input.is_open());
        data.assign(std::istreambuf_iterator<char>(input),
                    std::istreambuf_iterator<char>());
    }
    QVERIFY(std::filesystem::remove(source));
    const auto first = root / "fixture.zimaa";
    const auto second = root / "fixture.zimab";
    const auto write_parts = [&](std::size_t middle) {
        std::ofstream(first, std::ios::binary | std::ios::trunc)
            .write(data.data(), static_cast<std::streamsize>(middle));
        std::ofstream(second, std::ios::binary | std::ios::trunc)
            .write(data.data() + middle,
                   static_cast<std::streamsize>(data.size() - middle));
    };
    write_parts(data.size() / 2U);
    auto files = Discover({root}).dictionaries.front();
    const auto index = root / "fixture.gdfts";
    QCOMPARE(Dictionary::Open("zim-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(Dictionary::Open("zim-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));
    write_parts(data.size() / 2U + 1U);
    files = Discover({root}).dictionaries.front();
    QCOMPARE(Dictionary::Open("zim-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    const auto third = root / "fixture.zimac";
    std::ofstream(third, std::ios::binary) << "resource-only-part";
    files = Discover({root}).dictionaries.front();
    QCOMPARE(Dictionary::Open("zim-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    QVERIFY(std::filesystem::remove(third));
    files = Discover({root}).dictionaries.front();
    QCOMPARE(Dictionary::Open("zim-id", files, index).full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
}

void ZimDictionaryTest::ContainsFullTextFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteZimFixture(root);
    const Dictionary dictionary =
        Dictionary::Open("zim-id", {path, {path}}, root / "fixture.gdfts");
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
    const Dictionary storage =
        Dictionary::Open("storage-id", {path, {path}}, blocked);
    QCOMPARE(storage.LookupExact("example").size(), std::size_t{1});
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);
}
}  // namespace
}  // namespace goldendict::core::formats::zim

using goldendict::core::formats::zim::ZimDictionaryTest;
QTEST_APPLESS_MAIN(ZimDictionaryTest)
#include "zim_dictionary_test.moc"
