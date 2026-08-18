// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/dictionary/full_text_index.h"

namespace goldendict::core::dictionary {
namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() /
                ("goldendict-full-text-" +
                 std::to_string(QRandomGenerator::global()->generate64()));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    const std::filesystem::path& path() const { return path_; }

   private:
    std::filesystem::path path_;
};

FullTextDocument Document(std::string id, std::string headword,
                          std::string text) {
    FullTextDocument document;
    document.dictionary.id = std::move(id);
    document.dictionary.name = document.dictionary.id;
    document.headword = std::move(headword);
    document.document_id = document.headword + "-article";
    document.plain_text = std::move(text);
    return document;
}

class Cancelled final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

}  // namespace

class FullTextIndexTest : public QObject {
    Q_OBJECT
   private slots:
    void Lifecycle();
    void QueryModesAndFilters();
    void ResolvesOpaqueDocumentIdentity();
    void RejectsMalformedAndBoundedWork();
};

void FullTextIndexTest::ResolvesOpaqueDocumentIdentity() {
    TemporaryDirectory directory;
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "reference.gdfts", {},
        {Document("a", "Alpha", "first"), Document("b", "Beta", "second")});
    const auto resolved = index.ResolveDocument("Beta-article");
    QVERIFY(resolved.has_value());
    QCOMPARE(resolved->dictionary.id, std::string("b"));
    QCOMPARE(resolved->document_id, std::string("Beta-article"));
    QCOMPARE(resolved->headword, std::string("Beta"));
    QVERIFY(!index.ResolveDocument("missing").has_value());
    QVERIFY(!index.ResolveDocument("").has_value());
}

void FullTextIndexTest::Lifecycle() {
    TemporaryDirectory directory;
    const auto path = directory.path() / "reference.gdfts";
    const auto source = directory.path() / "source.txt";
    {
        std::ofstream output(source);
        output << "one";
    }
    auto sources = CaptureSourceSnapshot({source});
    const std::vector documents{Document("a", "Alpha", "quick brown fox")};
    QCOMPARE(FullTextIndex::OpenOrBuild(path, sources, documents).state(),
             FullTextIndexState::kCreated);
    QCOMPARE(FullTextIndex::OpenOrBuild(path, sources, documents).state(),
             FullTextIndexState::kReused);
    {
        std::ofstream output(source, std::ios::app);
        output << "two";
    }
    sources = CaptureSourceSnapshot({source});
    QCOMPARE(FullTextIndex::OpenOrBuild(path, sources, documents).state(),
             FullTextIndexState::kRebuiltStale);
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "corrupt";
    }
    QCOMPARE(FullTextIndex::OpenOrBuild(path, sources, documents).state(),
             FullTextIndexState::kRebuiltCorrupt);
}

void FullTextIndexTest::QueryModesAndFilters() {
    TemporaryDirectory directory;
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "reference.gdfts", {},
        {Document("a", "Alpha", "The quick brown fox"),
         Document("b", "Cafe", "A CAFÉ noir")});
    FullTextQuery query;
    query.text = "quick";
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.mode = FullTextQueryMode::kPlainText;
    query.text = "row";
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.mode = FullTextQueryMode::kWildcard;
    query.text = "quick*fox";
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.mode = FullTextQueryMode::kRegularExpression;
    query.text = "brown.+fox";
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.mode = FullTextQueryMode::kWholeWords;
    query.text = "fox quick";
    query.ignore_word_order = true;
    query.maximum_word_distance = 2U;
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.maximum_word_distance = 0U;
    QCOMPARE(index.Search(query).results.size(), 0U);
    query.ignore_word_order = false;
    query.maximum_word_distance.reset();
    query.text = "cafe";
    query.ignore_diacritics = true;
    query.dictionary_filter_active = true;
    query.dictionary_ids = {"b"};
    const auto response = index.Search(query);
    QCOMPARE(response.results.size(), 1U);
    QCOMPARE(response.results.front().dictionary.id, std::string("b"));
    QCOMPARE(response.results.front().match.mode, MatchMode::kFullText);
    QCOMPARE(response.results.front().matches.front().text,
             std::string("CAFÉ"));
}

void FullTextIndexTest::RejectsMalformedAndBoundedWork() {
    TemporaryDirectory directory;
    auto index =
        FullTextIndex::OpenOrBuild(directory.path() / "reference.gdfts", {},
                                   {Document("a", "Alpha", "quick brown fox")});
    FullTextQuery query;
    query.text = "[";
    query.mode = FullTextQueryMode::kRegularExpression;
    QCOMPARE(index.Search(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    query.text.assign(kMaximumFullTextQueryBytes + 1U, 'x');
    QCOMPARE(index.Search(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    query = {};
    query.text = "text";
    query.timeout = std::chrono::milliseconds::zero();
    QCOMPARE(index.Search(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    Cancelled cancelled;
    QVERIFY_EXCEPTION_THROWN(
        FullTextIndex::OpenOrBuild(directory.path() / "cancelled", {},
                                   {Document("a", "A", "text")}, &cancelled),
        FullTextIndexError);
}

}  // namespace goldendict::core::dictionary

using goldendict::core::dictionary::FullTextIndexTest;
QTEST_APPLESS_MAIN(FullTextIndexTest)
#include "full_text_index_test.moc"
