// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <string>
#include <vector>

#include <QTemporaryDir>
#include <QtTest>

#include "../src/morphology/hunspell_provider.h"
#include "support/hunspell_fixture.h"

namespace goldendict::core::morphology::hunspell {

// These minimal dictionaries are generated entirely by this test from
// project-authored GPL-3.0-or-later data. They contain no third-party wordlist
// content and are redistributable with the test source.

class HunspellProviderTest : public QObject {
    Q_OBJECT

   private slots:
    void PreservesExactUtf8AndAffixLookup();
    void PreservesLegacyEncodedLookup();
    void BuildsEscapedSpellingSuggestionArticle();
    void RejectsSpellingArticleInputsAndChecksRequests();
    void PreservesWholeQueryPrefixMembership();
    void BoundsPrefixMembershipAndChecksRequests();
    void SerializesConcurrentPrefixMembership();
    void EnumeratesBoundedTruePrefixes();
    void ChecksTruePrefixRequestsAndSerialization();
    void ReturnsOrderedSingleWordStems();
    void ReconstructsBoundedCompoundExpressionsInLegacyOrder();
    void PreservesCompoundSeparatorsAndLegacyEncoding();
    void BoundsCompoundTokensAndChecksRequests();
    void SerializesConcurrentCompoundAnalysis();
    void DecodesLegacyStemsAndFiltersEquivalentCase();
    void IgnoresMalformedRecordsAndStripsComments();
    void BoundsMorphologyAndChecksRequests();
    void RejectsInvalidContentAndBoundsQueries();
};

static dictionary::SynonymBackend& Synonyms(dictionary::Backend& provider) {
    auto* synonyms = dynamic_cast<dictionary::SynonymBackend*>(&provider);
    Q_ASSERT(synonyms != nullptr);
    return *synonyms;
}

static dictionary::TruePrefixBackend& TruePrefixes(
    dictionary::Backend& provider) {
    auto* prefixes = dynamic_cast<dictionary::TruePrefixBackend*>(&provider);
    Q_ASSERT(prefixes != nullptr);
    return *prefixes;
}

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class CancelAfterChecksSignal final : public dictionary::CancellationSignal {
   public:
    explicit CancelAfterChecksSignal(std::size_t allowed_checks)
        : allowed_checks_(allowed_checks) {}

    bool IsCancellationRequested() const noexcept override {
        return checks_++ >= allowed_checks_;
    }

   private:
    std::size_t allowed_checks_;
    mutable std::size_t checks_ = 0U;
};

void HunspellProviderTest::PreservesExactUtf8AndAffixLookup() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "fixture",
        "SET UTF-8\nTRY abcdefghijklmnopqrstuvwxyz\nREP 1\nREP cot cat\n"
        "SFX S Y 1\nSFX S 0 s .\n",
        "2\ncat/S\ncaf\xC3\xA9\n");
    auto provider = OpenProvider(files);

    QCOMPARE(provider->identity().id, std::string("fixture"));
    QCOMPARE(provider->identity().headword_count, 2U);
    QVERIFY(provider->LookupExact("cat").empty());
    QVERIFY(provider->LookupExact("cats").empty());
    QVERIFY(provider->LookupExact("caf\xC3\xA9").empty());
    const auto misspelled = provider->LookupExact("cot");
    QCOMPARE(misspelled.size(), 1U);
    QCOMPARE(misspelled.front().format, std::string("text/html"));
    QVERIFY(misspelled.front().data.find(">cat</a>") != std::string::npos);
    QVERIFY(provider->LookupPrefix("ca").empty());
    QCOMPARE(provider->LookupPrefix("cats").front().headword,
             std::string("cats"));
    QVERIFY(provider->SuggestPrefix("ca").empty());
}

void HunspellProviderTest::PreservesLegacyEncodedLookup() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "latin1",
        "SET ISO8859-1\nREP 1\nREP manana ma\xF1"
        "ana\n",
        std::string("1\nma") + "\xF1" + "ana\n");
    auto provider = OpenProvider(files);

    const std::string manana = std::string("ma") + "\xC3\xB1" + "ana";
    QVERIFY(provider->LookupExact(manana).empty());
    QCOMPARE(provider->LookupPrefix(manana).front().headword, manana);
    const auto article = provider->LookupExact("manana");
    QCOMPARE(article.size(), 1U);
    QVERIFY(article.front().data.find(manana) != std::string::npos);
    QVERIFY(provider->LookupPrefix("manana").empty());
}

void HunspellProviderTest::BuildsEscapedSpellingSuggestionArticle() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "article",
        "SET UTF-8\nTRY abcdefghijklmnopqrstuvwxyz&\nREP 2\n"
        "REP rox rock&roll\nREP rox rock&roll\n",
        "1\nrock&roll\n");
    auto provider = OpenProvider(files);

    const auto articles = provider->LookupExact("  (rox!)  ");
    QCOMPARE(articles.size(), 1U);
    QCOMPARE(articles.front().headword, std::string("  (rox!)  "));
    QCOMPARE(articles.front().format, std::string("text/html"));
    QVERIFY(articles.front().data.rfind(
                "<div class=\"gdspellsuggestion\">Spelling suggestions: ",
                0U) == 0U);
    QVERIFY(articles.front().data.find(
                "href=\"bword://rock&amp;roll\">rock&amp;roll</a>") !=
            std::string::npos);
    QCOMPARE(articles.front().data.substr(articles.front().data.size() - 6U),
             std::string("</div>"));
    QVERIFY(articles.front().data.size() <= 64U * 1024U);
}

void HunspellProviderTest::RejectsSpellingArticleInputsAndChecksRequests() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "requests",
        "SET UTF-8\nTRY abcdefghijklmnopqrstuvwxyz\nREP 1\nREP cot cat\n",
        "1\ncat\n");
    auto provider = OpenProvider(files);

    QVERIFY(provider->LookupExact("cat").empty());
    QVERIFY(provider->LookupExact("two words").empty());
    QVERIFY(provider->LookupExact("two\xE2\x80\x83words").empty());
    QVERIFY(provider->LookupExact(" \t(!)\n").empty());
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact(std::string("nu\0ll", 5U)),
                             dictionary::Error);

    dictionary::RequestOptions options;
    options.result_limit = 0U;
    QVERIFY(provider->LookupExact("cot", options).empty());

    CancelledSignal cancelled;
    options.result_limit = 1U;
    options.cancellation = &cancelled;
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact("cot", options),
                             dictionary::Error);

    CancelAfterChecksSignal cancel_after_engine(4U);
    options.cancellation = &cancel_after_engine;
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact("cot", options),
                             dictionary::Error);

    options.cancellation = nullptr;
    options.deadline =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact("cot", options),
                             dictionary::Error);
}

void HunspellProviderTest::PreservesWholeQueryPrefixMembership() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "membership",
        "SET UTF-8\nSFX S Y 1\nSFX S 0 s .\n", "2\ncat/S\ncaf\xC3\xA9\n");
    auto provider = OpenProvider(files);

    QCOMPARE(provider->LookupPrefix("cat").front().headword,
             std::string("cat"));
    QCOMPARE(provider->LookupPrefix("cats").front().headword,
             std::string("cats"));
    QCOMPARE(provider->LookupPrefix(" \t(caf\xC3\xA9!)\n").front().headword,
             std::string("caf\xC3\xA9"));
    QVERIFY(provider->LookupPrefix("ca").empty());
    QVERIFY(provider->LookupPrefix("dog").empty());
    QVERIFY(provider->LookupPrefix("two words").empty());
    QVERIFY(provider->LookupPrefix("two\xE2\x80\x83words").empty());
    QVERIFY(provider->LookupPrefix(" \t(!)\n").empty());
}

void HunspellProviderTest::BoundsPrefixMembershipAndChecksRequests() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    const auto files = test::WriteHunspellFixture(
        root, "bounded-prefix", "SET ISO8859-1\n", "1\nword\n");
    auto provider = OpenProvider(files);

    const auto expect_error = [&](std::string_view query,
                                  const dictionary::RequestOptions& options,
                                  dictionary::ErrorCode expected) {
        try {
            provider->LookupPrefix(query, options);
            QFAIL("prefix lookup did not throw");
        } catch (const dictionary::Error& error) {
            QCOMPARE(error.code(), expected);
        }
    };

    expect_error(std::string("bad\xFF", 4U), {},
                 dictionary::ErrorCode::kInvalidData);
    expect_error(std::string("nu\0ll", 5U), {},
                 dictionary::ErrorCode::kInvalidData);
    expect_error(std::string(4097U, 'a'), {},
                 dictionary::ErrorCode::kInvalidData);
    expect_error("\xE2\x98\x83", {}, dictionary::ErrorCode::kInvalidData);

    dictionary::RequestOptions options;
    options.result_limit = 0U;
    QVERIFY(provider->LookupPrefix("word", options).empty());

    CancelledSignal cancelled;
    options.result_limit = 1U;
    options.cancellation = &cancelled;
    expect_error("word", options, dictionary::ErrorCode::kCancelled);

    CancelAfterChecksSignal cancel_after_engine(3U);
    options.cancellation = &cancel_after_engine;
    expect_error("word", options, dictionary::ErrorCode::kCancelled);

    options.cancellation = nullptr;
    options.deadline =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    expect_error("word", options, dictionary::ErrorCode::kDeadlineExceeded);
}

void HunspellProviderTest::SerializesConcurrentPrefixMembership() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    auto first = OpenProvider(test::WriteHunspellFixture(
        root / "first", "first", "SET UTF-8\n", "1\nfirst\n"));
    auto second = OpenProvider(test::WriteHunspellFixture(
        root / "second", "second", "SET UTF-8\n", "1\nsecond\n"));

    const auto exercise = [](dictionary::Backend& provider,
                             std::string_view word) {
        for (std::size_t iteration = 0U; iteration < 100U; ++iteration) {
            const auto result = provider.LookupPrefix(word);
            if (result.size() != 1U || result.front().headword != word)
                return false;
        }
        return true;
    };
    auto first_result =
        std::async(std::launch::async, exercise, std::ref(*first), "first");
    auto second_result =
        std::async(std::launch::async, exercise, std::ref(*second), "second");
    QVERIFY(first_result.get());
    QVERIFY(second_result.get());
}

void HunspellProviderTest::EnumeratesBoundedTruePrefixes() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    const auto files = test::WriteHunspellFixture(
        root, "true-prefixes", "SET UTF-8\nSFX S Y 1\nSFX S 0 s .\n",
        "4\nc\ncat/S\ncaf\ncaf\xC3\xA9\n");
    auto provider = OpenProvider(files);
    auto& prefixes = TruePrefixes(*provider);

    QCOMPARE(prefixes.EnumerateTruePrefixes("catsup", {}),
             (std::vector<std::string>{"cats", "cat", "c"}));
    QCOMPARE(prefixes.EnumerateTruePrefixes("  (caf\xC3\xA9ine!)  ", {}),
             (std::vector<std::string>{"caf\xC3\xA9", "caf", "c"}));
    QCOMPARE(prefixes.EnumerateTruePrefixes("cat", {}),
             (std::vector<std::string>{"c"}));
    QVERIFY(prefixes.EnumerateTruePrefixes("dog", {}).empty());
    QVERIFY(prefixes.EnumerateTruePrefixes("two words", {}).empty());
    QVERIFY(prefixes.EnumerateTruePrefixes(" \t(!)\n", {}).empty());
    QVERIFY(provider->SuggestPrefix("cat").empty());

    dictionary::RequestOptions options;
    options.result_limit = 2U;
    QCOMPARE(prefixes.EnumerateTruePrefixes("catsup", options),
             (std::vector<std::string>{"cats", "cat"}));

    const auto latin_files = test::WriteHunspellFixture(
        root / "latin", "latin", "SET ISO8859-1\n",
        std::string("2\nma") + "\xF1" + "ana\nma\xF1\n");
    auto latin_provider = OpenProvider(latin_files);
    const std::string manana = std::string("ma") + "\xC3\xB1" + "ana";
    QCOMPARE(
        TruePrefixes(*latin_provider).EnumerateTruePrefixes(manana + "X", {}),
        (std::vector<std::string>{manana, std::string("ma") + "\xC3\xB1"}));
}

void HunspellProviderTest::ChecksTruePrefixRequestsAndSerialization() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    auto first = OpenProvider(
        test::WriteHunspellFixture(root / "first-prefixes", "first-prefixes",
                                   "SET UTF-8\n", "2\nf\nfirst\n"));
    auto second = OpenProvider(
        test::WriteHunspellFixture(root / "second-prefixes", "second-prefixes",
                                   "SET UTF-8\n", "2\ns\nsecond\n"));

    auto& prefixes = TruePrefixes(*first);
    QVERIFY_EXCEPTION_THROWN(
        prefixes.EnumerateTruePrefixes(std::string("bad\xFF", 4U), {}),
        dictionary::Error);
    QVERIFY_EXCEPTION_THROWN(
        prefixes.EnumerateTruePrefixes(std::string("nu\0ll", 5U), {}),
        dictionary::Error);
    QVERIFY_EXCEPTION_THROWN(
        prefixes.EnumerateTruePrefixes(std::string(4097U, 'a'), {}),
        dictionary::Error);

    dictionary::RequestOptions options;
    options.result_limit = 0U;
    QVERIFY(prefixes.EnumerateTruePrefixes("firstx", options).empty());
    CancelledSignal cancelled;
    options.result_limit = 1U;
    options.cancellation = &cancelled;
    QVERIFY_EXCEPTION_THROWN(prefixes.EnumerateTruePrefixes("firstx", options),
                             dictionary::Error);
    CancelAfterChecksSignal cancel_during_engine(3U);
    options.cancellation = &cancel_during_engine;
    QVERIFY_EXCEPTION_THROWN(prefixes.EnumerateTruePrefixes("firstx", options),
                             dictionary::Error);
    options.cancellation = nullptr;
    options.deadline =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    QVERIFY_EXCEPTION_THROWN(prefixes.EnumerateTruePrefixes("firstx", options),
                             dictionary::Error);

    const auto exercise = [](dictionary::Backend& provider,
                             std::string_view word, std::string_view expected) {
        for (std::size_t iteration = 0U; iteration < 100U; ++iteration) {
            const auto result =
                TruePrefixes(provider).EnumerateTruePrefixes(word, {});
            if (result.empty() || result.front() != expected)
                return false;
        }
        return true;
    };
    auto first_result = std::async(std::launch::async, exercise,
                                   std::ref(*first), "firstx", "first");
    auto second_result = std::async(std::launch::async, exercise,
                                    std::ref(*second), "secondx", "second");
    QVERIFY(first_result.get());
    QVERIFY(second_result.get());
}

void HunspellProviderTest::ReturnsOrderedSingleWordStems() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "stems",
        "SET UTF-8\n"
        "SFX A Y 1\nSFX A 0 s .\n"
        "SFX B Y 1\nSFX B 0 s .\n"
        "SFX C Y 1\nSFX C feline cats .\n",
        "2\ncat/AB\nfeline/C\n");
    auto provider = OpenProvider(files);

    QCOMPARE(Synonyms(*provider).FindHeadwordsForSynonym("cats", {}),
             (std::vector<std::string>{"cat", "cat"}));
    QVERIFY(provider->LookupExact("cats").empty());
    QCOMPARE(provider->LookupPrefix("cat").front().headword,
             std::string("cat"));
    QVERIFY(provider->SuggestPrefix("cat").empty());
}

void HunspellProviderTest::
    ReconstructsBoundedCompoundExpressionsInLegacyOrder() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "compound",
        "SET UTF-8\n"
        "SFX A Y 1\nSFX A 0 s .\n"
        "SFX B Y 1\nSFX B 0 s .\n",
        "2\ncat/AB\ndog/AB\n");
    auto provider = OpenProvider(files);
    auto& synonyms = Synonyms(*provider);

    QCOMPARE(synonyms.FindHeadwordsForSynonym("cats dogs", {}),
             (std::vector<std::string>{"cat dogs", "cat dogs", "cats dog",
                                       "cats dog", "cat dog", "cat dog",
                                       "cat dog", "cat dog"}));

    dictionary::RequestOptions options;
    options.result_limit = 3U;
    QCOMPARE(synonyms.FindHeadwordsForSynonym("cats dogs", options),
             (std::vector<std::string>{"cat dogs", "cat dogs", "cats dog"}));
    options.result_limit = 0U;
    QVERIFY(synonyms.FindHeadwordsForSynonym("cats dogs", options).empty());
}

void HunspellProviderTest::PreservesCompoundSeparatorsAndLegacyEncoding() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "latin-compound",
        "SET ISO8859-1\n"
        "SFX A Y 1\nSFX A 0 s .\n",
        std::string("2\nma") + "\xF1" + "ana/A\ngato/A\n");
    auto provider = OpenProvider(files);

    const std::string expression =
        std::string("  (ma") + "\xC3\xB1" + "anas,\xE2\x80\x83 gatos!)  ";
    QCOMPARE(Synonyms(*provider).FindHeadwordsForSynonym(expression, {}),
             (std::vector<std::string>{
                 std::string("ma") + "\xC3\xB1" + "ana,\xE2\x80\x83 gatos",
                 std::string("ma") + "\xC3\xB1" + "anas,\xE2\x80\x83 gato",
                 std::string("ma") + "\xC3\xB1" + "ana,\xE2\x80\x83 gato"}));
    QVERIFY_EXCEPTION_THROWN(
        Synonyms(*provider).FindHeadwordsForSynonym("snowman \xE2\x98\x83", {}),
        dictionary::Error);
}

void HunspellProviderTest::BoundsCompoundTokensAndChecksRequests() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()),
        "bounded-compound", "SET UTF-8\nSFX A Y 1\nSFX A 0 s .\n",
        "1\nword/A\n");
    auto provider = OpenProvider(files);
    auto& synonyms = Synonyms(*provider);

    const std::string at_run_limit =
        "words words words words words words words words words words words";
    QCOMPARE(synonyms.FindHeadwordsForSynonym(at_run_limit, {}).size(), 20U);
    const std::string above_run_limit = at_run_limit + " words";
    QVERIFY(synonyms.FindHeadwordsForSynonym(above_run_limit, {}).empty());

    CancelAfterChecksSignal cancel_during_compound(8U);
    dictionary::RequestOptions options;
    options.cancellation = &cancel_during_compound;
    QVERIFY_EXCEPTION_THROWN(
        synonyms.FindHeadwordsForSynonym("words words", options),
        dictionary::Error);

    options.cancellation = nullptr;
    options.deadline =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    QVERIFY_EXCEPTION_THROWN(
        synonyms.FindHeadwordsForSynonym("words words", options),
        dictionary::Error);
}

void HunspellProviderTest::SerializesConcurrentCompoundAnalysis() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    auto first = OpenProvider(test::WriteHunspellFixture(
        root / "first-compound", "first-compound",
        "SET UTF-8\nSFX A Y 1\nSFX A 0 s .\n", "1\nfirst/A\n"));
    auto second = OpenProvider(test::WriteHunspellFixture(
        root / "second-compound", "second-compound",
        "SET UTF-8\nSFX A Y 1\nSFX A 0 s .\n", "1\nsecond/A\n"));

    const auto exercise = [](dictionary::Backend& provider,
                             std::string_view expression) {
        for (std::size_t iteration = 0U; iteration < 100U; ++iteration) {
            if (Synonyms(provider)
                    .FindHeadwordsForSynonym(expression, {})
                    .empty()) {
                return false;
            }
        }
        return true;
    };
    auto first_result = std::async(std::launch::async, exercise,
                                   std::ref(*first), "firsts firsts");
    auto second_result = std::async(std::launch::async, exercise,
                                    std::ref(*second), "seconds seconds");
    QVERIFY(first_result.get());
    QVERIFY(second_result.get());
}

void HunspellProviderTest::DecodesLegacyStemsAndFiltersEquivalentCase() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "latin1-stems",
        "SET ISO8859-1\n"
        "SFX A Y 1\nSFX A 0 s .\n"
        "SFX B Y 1\nSFX B OTRAS ma\xF1"
        "anas .\n",
        "2\nma\xF1"
        "ana/A\nOTRAS/B\n");
    auto provider = OpenProvider(files);

    const std::string mananas = std::string("ma") + "\xC3\xB1" + "anas";
    QCOMPARE(Synonyms(*provider).FindHeadwordsForSynonym(mananas, {}),
             (std::vector<std::string>{"ma\xC3\xB1"
                                       "ana"}));

    const auto equivalent = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()) / "equivalent",
        "equivalent", "SET UTF-8\n", "1\nCATS\n");
    auto equivalent_provider = OpenProvider(equivalent);
    QVERIFY(Synonyms(*equivalent_provider)
                .FindHeadwordsForSynonym("cats", {})
                .empty());
}

void HunspellProviderTest::IgnoresMalformedRecordsAndStripsComments() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QVERIFY(!detail::ExtractStem("xx:not-a-stem fl:A").has_value());
    QVERIFY(!detail::ExtractStem("broken-st:value").has_value());
    QCOMPARE(detail::ExtractStem("fl:A st:kept#comment xx:ignored"),
             std::optional<std::string_view>("kept"));
    QCOMPARE(detail::ExtractStem("st:first st:second"),
             std::optional<std::string_view>("first"));
}

void HunspellProviderTest::BoundsMorphologyAndChecksRequests() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "bounded",
        "SET UTF-8\nSFX A Y 1\nSFX A 0 s .\n", "1\nword/A\n");
    auto provider = OpenProvider(files);
    auto& synonyms = Synonyms(*provider);

    QCOMPARE(synonyms.FindHeadwordsForSynonym("  (words!)  ", {}),
             (std::vector<std::string>{"word"}));
    QVERIFY(synonyms.FindHeadwordsForSynonym("two unknown", {}).empty());
    QVERIFY(
        synonyms.FindHeadwordsForSynonym(std::string(81U, 'a'), {}).empty());
    QVERIFY_EXCEPTION_THROWN(
        synonyms.FindHeadwordsForSynonym(std::string("bad\xFF", 4U), {}),
        dictionary::Error);
    QVERIFY_EXCEPTION_THROWN(
        synonyms.FindHeadwordsForSynonym(std::string(4097U, 'a'), {}),
        dictionary::Error);

    CancelledSignal cancelled;
    dictionary::RequestOptions options;
    options.cancellation = &cancelled;
    QVERIFY_EXCEPTION_THROWN(synonyms.FindHeadwordsForSynonym("words", options),
                             dictionary::Error);
    CancelAfterChecksSignal cancel_after_engine(3U);
    options.cancellation = &cancel_after_engine;
    QVERIFY_EXCEPTION_THROWN(synonyms.FindHeadwordsForSynonym("words", options),
                             dictionary::Error);
    options.cancellation = nullptr;
    options.deadline =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    QVERIFY_EXCEPTION_THROWN(synonyms.FindHeadwordsForSynonym("words", options),
                             dictionary::Error);
}

void HunspellProviderTest::RejectsInvalidContentAndBoundsQueries() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    const auto invalid =
        test::WriteHunspellFixture(root / "invalid", "x", "TRY abc\n", "0\n");
    try {
        OpenProvider(invalid);
        QFAIL("invalid content was accepted");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }

    const auto valid = test::WriteHunspellFixture(root / "valid", "x",
                                                  "SET UTF-8\n", "1\nword\n");
    auto provider = OpenProvider(valid);
    QVERIFY(provider->LookupExact("two words").empty());
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact(std::string(4097U, 'a')),
                             dictionary::Error);
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact(std::string("bad\xFF", 4U)),
                             dictionary::Error);
    dictionary::RequestOptions options;
    options.result_limit = 0U;
    QVERIFY(provider->LookupExact("word", options).empty());
    options.result_limit = 1U;
    options.deadline =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact("word", options),
                             dictionary::Error);
}

}  // namespace goldendict::core::morphology::hunspell

using goldendict::core::morphology::hunspell::HunspellProviderTest;
QTEST_APPLESS_MAIN(HunspellProviderTest)
#include "hunspell_provider_test.moc"
