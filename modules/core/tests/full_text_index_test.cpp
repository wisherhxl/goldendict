// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <type_traits>

#include "../src/dictionary/full_text_index.h"
#include "../src/dictionary/full_text_index_lifecycle.h"
#include "../src/dictionary/full_text_matcher.h"
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

class FakeFormatWorkPort final : public FullTextIndexFormatWorkPort {
   public:
    enum class Behavior { kComplete, kFail, kThrowStandard, kThrowUnknown };

    bool supported = true;
    std::string source_revision = "revision-1";
    Behavior behavior = Behavior::kComplete;
    std::optional<FullTextIndexWorkRequest> request;
    bool cancellation_observed = false;

    bool IsFullTextIndexSupported() const noexcept override {
        return supported;
    }

    std::string FullTextIndexSourceRevision() const override {
        return source_revision;
    }

   private:
    FullTextIndexWorkResult DoPerformFullTextIndexWork(
        const FullTextIndexWorkRequest& work_request) override {
        request = work_request;
        cancellation_observed =
            work_request.cancellation != nullptr &&
            work_request.cancellation->IsCancellationRequested();
        switch (behavior) {
            case Behavior::kComplete:
                return {cancellation_observed
                            ? FullTextIndexWorkStatus::kCancelled
                            : FullTextIndexWorkStatus::kCompleted,
                        {}};
            case Behavior::kFail:
                return {FullTextIndexWorkStatus::kFailed, "adapter failure"};
            case Behavior::kThrowStandard:
                throw std::runtime_error("escaped adapter failure");
            case Behavior::kThrowUnknown:
                throw 7;
        }
        return {};
    }
};

}  // namespace

class FullTextIndexTest : public QObject {
    Q_OBJECT
   private slots:
    void Lifecycle();
    void LifecycleContract();
    void QueryModesAndFilters();
    void AppliesIndependentIcuNormalizationPolicies();
    void PreservesNormalizedMatchOriginsAndProgress();
    void PreservesQueryModesAndWordConstraints();
    void ConstructsBoundedMatchCenteredExcerpts();
    void PreservesUtf8MatchAndExcerptBoundaries();
    void ResolvesOpaqueDocumentIdentity();
    void RejectsMalformedAndBoundedWork();
};

void FullTextIndexTest::LifecycleContract() {
    const FullTextIndexPolicy defaults;
    QVERIFY(defaults.enabled);
    QCOMPARE(defaults.maximum_dictionary_megabytes, 0U);
    QVERIFY(defaults.disabled_format_types.empty());

    FullTextIndexPolicy policy = defaults;
    policy.enabled = false;
    policy.maximum_dictionary_megabytes = 512U;
    policy.disabled_format_types = "AARD,DSL";
    QVERIFY(policy != defaults);
    QCOMPARE(policy, FullTextIndexPolicy(policy));

    const FullTextIndexWorkIdentity identity{42U, "dictionary-a"};
    const FullTextIndexWorkIdentity stale_generation{41U, "dictionary-a"};
    const FullTextIndexWorkIdentity stale_dictionary{42U, "dictionary-b"};
    QVERIFY(identity != stale_generation);
    QVERIFY(identity != stale_dictionary);
    QCOMPARE((FullTextIndexRebuildIntent{identity, policy}),
             (FullTextIndexRebuildIntent{identity, policy}));
    QVERIFY(!(FullTextIndexRebuildIntent{identity, policy} ==
              FullTextIndexRebuildIntent{stale_generation, policy}));
    QCOMPARE(FullTextIndexCancelIntent{identity},
             (FullTextIndexCancelIntent{identity}));
    QVERIFY(!(FullTextIndexCancelIntent{identity} ==
              FullTextIndexCancelIntent{stale_dictionary}));

    const FullTextIndexLifecycleSnapshot snapshot{
        identity, FullTextIndexLifecycleState::kWorkRequested, true,
        "revision-1"};
    static_assert(std::is_same_v<decltype(snapshot.identity()),
                                 const FullTextIndexWorkIdentity&>);
    static_assert(std::is_same_v<decltype(snapshot.source_revision()),
                                 const std::string&>);
    QCOMPARE(snapshot.identity(), identity);
    QCOMPARE(snapshot.state(), FullTextIndexLifecycleState::kWorkRequested);
    QVERIFY(snapshot.format_capable());
    QCOMPARE(snapshot.source_revision(), std::string("revision-1"));
    QCOMPARE(snapshot,
             FullTextIndexLifecycleSnapshot(
                 identity, FullTextIndexLifecycleState::kWorkRequested, true,
                 "revision-1"));
    QVERIFY(!(snapshot == FullTextIndexLifecycleSnapshot(
                              stale_generation,
                              FullTextIndexLifecycleState::kWorkRequested, true,
                              "revision-1")));
    QVERIFY(!(snapshot == FullTextIndexLifecycleSnapshot(
                              stale_dictionary,
                              FullTextIndexLifecycleState::kWorkRequested, true,
                              "revision-1")));

    FakeFormatWorkPort port;
    QVERIFY(port.IsFullTextIndexSupported());
    QCOMPARE(port.FullTextIndexSourceRevision(), std::string("revision-1"));
    port.supported = false;
    QVERIFY(!port.IsFullTextIndexSupported());
    port.supported = true;
    port.source_revision = "revision-2";
    QCOMPARE(port.FullTextIndexSourceRevision(), std::string("revision-2"));

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    FullTextIndexWorkRequest request;
    request.identity = identity;
    request.policy = policy;
    request.source_revision = "revision-2";
    request.maximum_documents = 101U;
    request.maximum_document_bytes = 1024U;
    request.maximum_corpus_bytes = 4096U;
    request.deadline = deadline;
    auto result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kCompleted);
    QVERIFY(result.message.empty());
    QVERIFY(port.request.has_value());
    QCOMPARE(port.request->identity, identity);
    QCOMPARE(port.request->policy, policy);
    QCOMPARE(port.request->source_revision, std::string("revision-2"));
    QCOMPARE(port.request->maximum_documents, 101U);
    QCOMPARE(port.request->maximum_document_bytes, 1024U);
    QCOMPARE(port.request->maximum_corpus_bytes, 4096U);
    QCOMPARE(port.request->deadline, deadline);
    QVERIFY(port.request->cancellation == nullptr);

    Cancelled cancelled;
    request.cancellation = &cancelled;
    result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kCancelled);
    QVERIFY(port.cancellation_observed);

    port.behavior = FakeFormatWorkPort::Behavior::kFail;
    result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kFailed);
    QCOMPARE(result.message, std::string("adapter failure"));
    port.behavior = FakeFormatWorkPort::Behavior::kThrowStandard;
    result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kFailed);
    QCOMPARE(result.message, std::string("escaped adapter failure"));
    port.behavior = FakeFormatWorkPort::Behavior::kThrowUnknown;
    result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kFailed);
    QCOMPARE(result.message,
             std::string("Unknown full-text index work failure"));
}

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
    query.text = u8"é";
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string(u8"é"));
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

void FullTextIndexTest::AppliesIndependentIcuNormalizationPolicies() {
    const auto match = [](std::string_view text, std::string_view query,
                          bool match_case, bool ignore_diacritics) {
        return MatchFullText(text,
                             {query, FullTextQueryMode::kPlainText, match_case,
                              ignore_diacritics, false, std::nullopt},
                             nullptr,
                             std::chrono::steady_clock::time_point::max());
    };

    QVERIFY(!match(u8"CAFÉ", u8"café", false, false).empty());
    QVERIFY(match(u8"CAFÉ", u8"café", true, false).empty());
    QVERIFY(!match(u8"CAFÉ", "CAFE", true, true).empty());
    QVERIFY(match(u8"CAFÉ", "cafe", true, true).empty());
    QVERIFY(!match(u8"CAFÉ", "cafe", false, true).empty());
    QVERIFY(match(u8"CAFÉ", "cafe", false, false).empty());

    const std::string reordered = u8"ạ́";
    const auto canonical = match(reordered, u8"ạ́", true, false);
    QCOMPARE(canonical.size(), 1U);
    QCOMPARE(canonical.front().byte_offset, 0U);
    QCOMPARE(canonical.front().byte_length, reordered.size());

    QCOMPARE(NormalizeFullTextQuery(u8"İ", false, false), std::string(u8"i̇"));
    QCOMPARE(NormalizeFullTextQuery(u8"İ", false, true), std::string("i"));
    QCOMPARE(NormalizeFullTextQuery(u8"가", true, false), std::string(u8"가"));
    QCOMPARE(NormalizeFullTextQuery(u8"가", true, false), std::string(u8"가"));
    const auto folded_mark = match(u8"İ", "i", false, true);
    QCOMPARE(folded_mark.size(), 1U);
    QCOMPARE(folded_mark.front().byte_length, std::string(u8"İ").size());
}

void FullTextIndexTest::PreservesNormalizedMatchOriginsAndProgress() {
    const auto match = [](std::string_view text, std::string_view query,
                          FullTextQueryMode mode =
                              FullTextQueryMode::kPlainText,
                          bool match_case = false,
                          bool ignore_diacritics = false) {
        return MatchFullText(
            text,
            {query, mode, match_case, ignore_diacritics, false, std::nullopt},
            nullptr, std::chrono::steady_clock::time_point::max());
    };

    const std::string expansion = u8"ß ß";
    const auto expanded = match(expansion, "s");
    QCOMPARE(expanded.size(), 2U);
    QCOMPARE(expanded[0].byte_offset, 0U);
    QCOMPARE(expanded[0].byte_length, std::string(u8"ß").size());
    QCOMPARE(expansion.substr(expanded[0].byte_offset, expanded[0].byte_length),
             std::string(u8"ß"));
    QCOMPARE(expansion.substr(expanded[1].byte_offset, expanded[1].byte_length),
             std::string(u8"ß"));

    const std::string marks = u8"á का x⃝ .́ ́";
    for (const auto& [query, literal] :
         std::vector<std::pair<std::string, std::string>>{
             {"a", u8"á"}, {u8"क", u8"का"}, {"x", u8"x⃝"}}) {
        const auto ranges =
            match(marks, query, FullTextQueryMode::kPlainText, true, true);
        QCOMPARE(ranges.size(), 1U);
        QCOMPARE(marks.substr(ranges.front().byte_offset,
                              ranges.front().byte_length),
                 literal);
    }
    QVERIFY(
        match(u8"́⃝", u8"́", FullTextQueryMode::kPlainText, true, true).empty());

    const std::string hangul = u8"가";
    const auto contracted =
        match(hangul, u8"가", FullTextQueryMode::kPlainText, true, false);
    QCOMPARE(contracted.size(), 1U);
    QCOMPARE(contracted.front().byte_length, hangul.size());

    const std::string supplementary = u8"😀😀";
    const auto regex =
        match(supplementary, ".", FullTextQueryMode::kRegularExpression, true);
    QCOMPARE(regex.size(), 2U);
    for (const auto& range : regex) {
        QCOMPARE(range.byte_length, std::string(u8"😀").size());
        QCOMPARE(supplementary.substr(range.byte_offset, range.byte_length),
                 std::string(u8"😀"));
    }
    const auto wildcard =
        match(u8"😀", "?", FullTextQueryMode::kWildcard, true);
    QCOMPARE(wildcard.size(), 1U);
    QCOMPARE(wildcard.front().byte_length, std::string(u8"😀").size());

    TemporaryDirectory directory;
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "agreement.gdfts", {},
        {Document("agreement", "Agreement", expansion)});
    FullTextQuery query;
    query.text = "ss";
    query.mode = FullTextQueryMode::kRegularExpression;
    const auto indexed = index.Search(query);
    QCOMPARE(indexed.results.size(), 1U);
    QCOMPARE(indexed.results.front().matches.front().byte_offset,
             expanded.front().byte_offset);
    QCOMPARE(indexed.results.front().matches.front().byte_length,
             expanded.front().byte_length);
    QCOMPARE(indexed.results.front().matches.front().text, std::string(u8"ß"));
}

void FullTextIndexTest::PreservesQueryModesAndWordConstraints() {
    const std::string source = "alpha beta gamma alpha gamma beta";
    for (const auto mode :
         {FullTextQueryMode::kWholeWords, FullTextQueryMode::kPlainText}) {
        for (const bool ignore_order : {false, true}) {
            for (const auto distance : {std::optional<std::uint32_t>{},
                                        std::optional<std::uint32_t>{0U},
                                        std::optional<std::uint32_t>{2U}}) {
                const auto ranges = MatchFullText(
                    source,
                    {"alpha beta", mode, true, false, ignore_order, distance},
                    nullptr, std::chrono::steady_clock::time_point::max());
                QVERIFY(!ranges.empty());
            }
        }
    }
    for (const auto mode : {FullTextQueryMode::kWildcard,
                            FullTextQueryMode::kRegularExpression}) {
        const auto query = mode == FullTextQueryMode::kWildcard
                               ? std::string_view("alpha*beta")
                               : std::string_view("alpha.+beta");
        QVERIFY(!MatchFullText(
                     source, {query, mode, true, false, false, std::nullopt},
                     nullptr, std::chrono::steady_clock::time_point::max())
                     .empty());
    }
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
    query = {};
    query.text = "*";
    query.mode = FullTextQueryMode::kWildcard;
    query.ignore_word_order = true;
    QCOMPARE(index.Search(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    query.ignore_word_order = false;
    query.maximum_word_distance = 1U;
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
