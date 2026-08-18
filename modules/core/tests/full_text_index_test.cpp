// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/dictionary/full_text_index.h"
#include "../src/foundation/utf8.h"

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
    void ConstructsBoundedMatchCenteredExcerpts();
    void PreservesUtf8MatchAndExcerptBoundaries();
    void ResolvesOpaqueDocumentIdentity();
    void RejectsMalformedAndBoundedWork();
};

void FullTextIndexTest::ConstructsBoundedMatchCenteredExcerpts() {
    TemporaryDirectory directory;
    const std::string middle =
        std::string(2999U, 'a') + " MATCH " + std::string(2999U, 'b');
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "excerpts.gdfts", {},
        {Document("a", "Start", "MATCH " + std::string(5000U, 'a')),
         Document("b", "Middle", middle),
         Document("c", "End", std::string(5000U, 'a') + " MATCH")});
    FullTextQuery query;
    query.text = "MATCH";
    query.mode = FullTextQueryMode::kPlainText;
    query.result_limit = 3U;
    const auto response = index.Search(query);
    QCOMPARE(response.results.size(), 3U);

    const auto verify = [](const FullTextResult& result,
                           std::string_view document) {
        QCOMPARE(result.matches.size(), 1U);
        const auto& match = result.matches.front();
        QVERIFY(match.byte_offset <= document.size());
        QVERIFY(match.byte_length <= document.size() - match.byte_offset);
        QCOMPARE(match.text, std::string(document.substr(match.byte_offset,
                                                         match.byte_length)));
        QVERIFY(result.excerpt.size() <= kMaximumFullTextExcerptBytes);
        QVERIFY(foundation::IsValidUtf8(result.excerpt));
        QVERIFY(match.byte_offset >= result.excerpt_byte_offset);
        const auto relative = match.byte_offset - result.excerpt_byte_offset;
        QVERIFY(relative <= result.excerpt.size());
        QVERIFY(match.byte_length <= result.excerpt.size() - relative);
        QCOMPARE(result.excerpt.substr(relative, match.byte_length),
                 match.text);
    };
    verify(response.results[0], "MATCH " + std::string(5000U, 'a'));
    verify(response.results[1], middle);
    verify(response.results[2], std::string(5000U, 'a') + " MATCH");
    QCOMPARE(response.results[0].excerpt_byte_offset, 0U);
    QCOMPARE(response.results[0].excerpt.size(), kMaximumFullTextExcerptBytes);
    QCOMPARE(response.results[1].excerpt_byte_offset, 954U);
    QCOMPARE(response.results[1].excerpt.size(), kMaximumFullTextExcerptBytes);
    QCOMPARE(response.results[2].excerpt_byte_offset, 910U);
    QCOMPARE(response.results[2].excerpt.size(), kMaximumFullTextExcerptBytes);
    QCOMPARE(index.Search(query).results[1].excerpt,
             response.results[1].excerpt);
    QCOMPARE(index.Search(query).results[1].excerpt_byte_offset, 954U);

    query.text = "missing";
    QVERIFY(index.Search(query).results.empty());
    FullTextResult empty;
    QVERIFY(empty.excerpt.empty());
    QCOMPARE(empty.excerpt_byte_offset, 0U);
}

void FullTextIndexTest::PreservesUtf8MatchAndExcerptBoundaries() {
    TemporaryDirectory directory;
    std::string multibyte;
    for (std::size_t i = 0U; i < 1000U; ++i)
        multibyte += u8"日";
    multibyte += " MATCH ";
    for (std::size_t i = 0U; i < 1000U; ++i)
        multibyte += u8"本";
    const std::string oversized = std::string(5000U, 'x') + u8"日";
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "utf8-excerpts.gdfts", {},
        {Document("a", "Multibyte", multibyte),
         Document("b", "Pattern", u8"A é é Z"),
         Document("c", "Oversized", oversized),
         Document("d", "Normalized", u8"A CAFÉ noir"),
         Document("e", "Utf8Start", u8"日 " + std::string(5000U, 'a')),
         Document("f", "Utf8Middle",
                  std::string(2999U, 'a') + u8" 日 " + std::string(2999U, 'b')),
         Document("g", "Utf8End", std::string(5000U, 'a') + u8" 日")});

    FullTextQuery query;
    query.text = "MATCH";
    query.mode = FullTextQueryMode::kPlainText;
    auto result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string("MATCH"));
    QVERIFY(result.excerpt.size() <= kMaximumFullTextExcerptBytes);
    QVERIFY(result.excerpt.size() >= kMaximumFullTextExcerptBytes - 2U);
    QVERIFY(foundation::IsValidUtf8(result.excerpt));
    QVERIFY((static_cast<unsigned char>(multibyte[result.excerpt_byte_offset]) &
             0xc0U) != 0x80U);

    query.mode = FullTextQueryMode::kRegularExpression;
    query.match_case = true;
    query.text = ".";
    query.dictionary_filter_active = true;
    query.dictionary_ids = {"b"};
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().byte_offset, 0U);
    QCOMPARE(result.matches.front().byte_length, 1U);
    QCOMPARE(result.matches.front().text, std::string("A"));
    query.text = u8"é";
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string(u8"é"));
    QCOMPARE(result.matches.front().byte_length, std::string(u8"é").size());
    query.text = u8"e.";
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string(u8"é"));
    QVERIFY(foundation::IsValidUtf8(result.matches.front().text));

    query.text = "x+";
    query.dictionary_ids = {"c"};
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().byte_length, 5000U);
    QCOMPARE(result.excerpt_byte_offset, 0U);
    QCOMPARE(result.excerpt.size(), kMaximumFullTextExcerptBytes);
    QCOMPARE(result.excerpt, std::string(kMaximumFullTextExcerptBytes, 'x'));

    query = {};
    query.text = "cafe";
    query.ignore_diacritics = true;
    query.dictionary_filter_active = true;
    query.dictionary_ids = {"d"};
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string(u8"CAFÉ"));
    QCOMPARE(result.matches.front().byte_offset, 2U);
    QCOMPARE(result.matches.front().byte_length, std::string(u8"CAFÉ").size());

    query = {};
    query.text = u8"日";
    query.dictionary_filter_active = true;
    for (const auto& [id, expected_origin] :
         std::vector<std::pair<std::string, std::size_t>>{
             {"e", 0U}, {"f", 953U}, {"g", 908U}}) {
        query.dictionary_ids = {id};
        result = index.Search(query).results.front();
        QCOMPARE(result.matches.front().text, std::string(u8"日"));
        QCOMPARE(result.matches.front().byte_length,
                 std::string(u8"日").size());
        QCOMPARE(result.excerpt_byte_offset, expected_origin);
        QVERIFY(foundation::IsValidUtf8(result.excerpt));
        QVERIFY(result.excerpt.size() <= kMaximumFullTextExcerptBytes);
    }

    query.mode = FullTextQueryMode::kWildcard;
    query.match_case = true;
    query.text = "?";
    query.dictionary_ids = {"e"};
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string(u8"日"));
    QCOMPARE(result.matches.front().byte_length, std::string(u8"日").size());
}

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
