// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <filesystem>
#include <fstream>
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
    void ReturnsOrderedSingleWordStems();
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
        "SET UTF-8\nSFX S Y 1\nSFX S 0 s .\n", "2\ncat/S\ncaf\xC3\xA9\n");
    auto provider = OpenProvider(files);

    QCOMPARE(provider->identity().id, std::string("fixture"));
    QCOMPARE(provider->identity().headword_count, 2U);
    QCOMPARE(provider->LookupExact("cat").size(), 1U);
    QCOMPARE(provider->LookupExact("cats").size(), 1U);
    QCOMPARE(provider->LookupExact("caf\xC3\xA9").front().headword,
             std::string("caf\xC3\xA9"));
    QVERIFY(provider->LookupExact("dog").empty());
    QVERIFY(provider->LookupPrefix("ca").empty());
    QVERIFY(provider->SuggestPrefix("ca").empty());
}

void HunspellProviderTest::PreservesLegacyEncodedLookup() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "latin1",
        "SET ISO8859-1\n", std::string("1\nma") + "\xF1" + "ana\n");
    auto provider = OpenProvider(files);

    const std::string manana = std::string("ma") + "\xC3\xB1" + "ana";
    QCOMPARE(provider->LookupExact(manana).size(), 1U);
    QVERIFY(provider->LookupExact("manana").empty());
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
    QCOMPARE(provider->LookupExact("cats").size(), 1U);
    QVERIFY(provider->LookupPrefix("cat").empty());
    QVERIFY(provider->SuggestPrefix("cat").empty());
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
    QVERIFY(synonyms.FindHeadwordsForSynonym("two words", {}).empty());
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
