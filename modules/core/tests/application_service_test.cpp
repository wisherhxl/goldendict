// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QCryptographicHash>

#include <array>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

#ifdef _WIN32
#include <winerror.h>
#endif

#include "../src/application/configuration_transaction_persistence.h"
#include "../src/application/configuration_transaction_preparation.h"
#include "../src/application/core_facade_activation_test_access.h"
#include "../src/application/desktop_facade_activation_owner.h"
#include "../src/application/full_text_index_lifecycle_inspection.h"
#include "../src/application/pending_configuration_transaction.h"
#include "goldendict/core/application.h"
#include "goldendict/core/headword_export.h"
#include "support/aard_fixture.h"
#include "support/bgl_fixture.h"
#include "support/dictd_fixture.h"
#include "support/dsl_fixture.h"
#include "support/epwing_fixture.h"
#include "support/gls_fixture.h"
#include "support/hunspell_fixture.h"
#include "support/lsa_fixture.h"
#include "support/mdict_fixture.h"
#include "support/sdict_fixture.h"
#include "support/slob_fixture.h"
#include "support/sounddir_fixture.h"
#include "support/stardict_fixture.h"
#include "support/xdxf_fixture.h"
#include "support/zim_fixture.h"
#include "support/zipsounds_fixture.h"

namespace goldendict::core {
namespace {

static_assert(std::is_move_constructible_v<PreparedConfigurationTransaction>);
static_assert(!std::is_move_assignable_v<PreparedConfigurationTransaction>);
static_assert(!std::is_copy_constructible_v<PreparedConfigurationTransaction>);
static_assert(!std::is_copy_assignable_v<PreparedConfigurationTransaction>);
static_assert(std::is_nothrow_move_constructible_v<
              application::PreparedCoreFacadeCandidate>);
static_assert(std::is_nothrow_move_assignable_v<
              application::PreparedCoreFacadeCandidate>);
static_assert(
    !std::is_copy_constructible_v<application::PreparedCoreFacadeCandidate>);
static_assert(
    !std::is_copy_assignable_v<application::PreparedCoreFacadeCandidate>);
static_assert(std::is_nothrow_move_constructible_v<
              application::ReservedCoreFacadeCandidate>);
static_assert(
    !std::is_copy_constructible_v<application::ReservedCoreFacadeCandidate>);

bool IsSymlinkCapabilityUnavailable(const std::error_code& error) {
#ifdef _WIN32
    return error == std::make_error_condition(std::errc::permission_denied) ||
           error ==
               std::make_error_condition(std::errc::operation_not_permitted) ||
           (error.category() == std::system_category() &&
            error.value() == ERROR_PRIVILEGE_NOT_HELD);
#else
    static_cast<void>(error);
    return false;
#endif
}

PendingConfigurationTransactionRecord CompletePendingRecord() {
    PendingConfigurationTransactionRecord record;
    for (std::size_t index = 0U; index < record.transaction_id.size(); ++index)
        record.transaction_id[index] = static_cast<std::uint8_t>(index);
    record.phase = PendingTransactionPhase::kDesiredPersistenceApplied;
    record.desired_recovery_attempt = DesiredRecoveryAttempt::kAttempted;
    record.desired_configuration =
        MakePendingTransactionPayload(std::string("config\0bytes", 12U));
    record.history_intent = PendingHistoryIntent::kReplace;
    record.desired_history = MakePendingTransactionPayload("desired history");
    record.previous_configuration = {
        true, MakePendingTransactionPayload("previous configuration")};
    record.previous_history = {
        true, MakePendingTransactionPayload("previous history")};
    record.failure = PendingFailureIdentity{
        PendingFailureOperation::kPersistDesired,
        PendingFailureDestination::kHistory, PendingFailureCategory::kIo,
        "atomic_replace_failed"};
    return record;
}

PendingConfigurationTransactionRecord FallbackRecord(
    std::uint8_t identity_byte, PendingHistoryIntent history_intent,
    std::string previous_configuration,
    PendingPreviousDestination previous_history = {}) {
    PendingConfigurationTransactionRecord record;
    record.transaction_id.fill(identity_byte);
    record.phase = PendingTransactionPhase::kDesiredPersistenceFailed;
    record.desired_configuration =
        MakePendingTransactionPayload("desired configuration");
    record.history_intent = history_intent;
    if (history_intent == PendingHistoryIntent::kReplace)
        record.desired_history =
            MakePendingTransactionPayload("desired history");
    record.previous_configuration = {
        true, MakePendingTransactionPayload(std::move(previous_configuration))};
    record.previous_history = std::move(previous_history);
    record.failure = PendingFailureIdentity{
        PendingFailureOperation::kPersistDesired,
        PendingFailureDestination::kConfiguration, PendingFailureCategory::kIo,
        "desired_persistence_failed"};
    return record;
}

std::size_t PendingFieldOffset(const std::string& bytes, std::uint8_t field) {
    constexpr std::string_view kHeader = "goldendict-pending-transaction-v1\n";
    std::size_t position = kHeader.size();
    while (position + 5U <= bytes.size()) {
        if (static_cast<std::uint8_t>(bytes[position]) == field)
            return position;
        const auto length =
            (static_cast<std::uint32_t>(
                 static_cast<unsigned char>(bytes[position + 1U]))
             << 24U) |
            (static_cast<std::uint32_t>(
                 static_cast<unsigned char>(bytes[position + 2U]))
             << 16U) |
            (static_cast<std::uint32_t>(
                 static_cast<unsigned char>(bytes[position + 3U]))
             << 8U) |
            static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[position + 4U]));
        position += 5U + length;
    }
    return std::string::npos;
}

class CancelledToken final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class SlowToken final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return false;
    }
};

bool WaitForFullTextLifecycleState(
    const DictionaryService& service, const std::string& dictionary_id,
    dictionary::FullTextIndexLifecycleState expected) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto snapshot =
            application::FullTextIndexLifecycleSnapshot(service, dictionary_id);
        if (snapshot.has_value() && snapshot->state() == expected)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

class InspectionRuntimeSource final : public RuntimeDictionarySource {
   public:
    InspectionRuntimeSource() {
        identity_.id = "inspection";
        identity_.name = "Inspection";
        identity_.source =
            "https://user:secret@example.test/path?token=secret#fragment";
        identity_.description =
            "Author: Test<br><script>unsafe()</script>&lt;plain&gt;";
        identity_.description.push_back('\0');
        identity_.description += "hidden";
        identity_.source_language = "en";
        identity_.target_language = "de";
    }

    const RuntimeDictionaryIdentity& identity() const noexcept override {
        return identity_;
    }

    std::vector<RuntimeDictionaryArticle> LookupExact(
        std::string_view, const RuntimeRequestOptions&) const override {
        return {};
    }

    std::vector<RuntimeDictionaryArticle> LookupPrefix(
        std::string_view, const RuntimeRequestOptions&) const override {
        return {};
    }

    std::vector<std::string> SuggestPrefix(
        std::string_view, const RuntimeRequestOptions&) const override {
        return {};
    }

    std::optional<RuntimeDictionaryResource> GetResource(
        std::string_view, const RuntimeRequestOptions&) const override {
        return std::nullopt;
    }

   private:
    RuntimeDictionaryIdentity identity_;
};

class DiacriticRuntimeSource final : public RuntimeDictionarySource {
   public:
    explicit DiacriticRuntimeSource(bool supported) {
        identity_.id =
            supported ? "diacritic-capable" : "diacritic-unsupported";
        identity_.name = identity_.id;
        identity_.supports_diacritic_insensitive_lookup = supported;
    }

    const RuntimeDictionaryIdentity& identity() const noexcept override {
        return identity_;
    }

    std::vector<RuntimeDictionaryArticle> LookupExact(
        std::string_view, const RuntimeRequestOptions& options) const override {
        if (!options.ignore_diacritics)
            return {};
        return {{"café", "text", "runtime definition"}};
    }

    std::vector<RuntimeDictionaryArticle> LookupPrefix(
        std::string_view, const RuntimeRequestOptions&) const override {
        return {};
    }

    std::vector<std::string> SuggestPrefix(
        std::string_view, const RuntimeRequestOptions&) const override {
        return {};
    }

    std::optional<RuntimeDictionaryResource> GetResource(
        std::string_view, const RuntimeRequestOptions&) const override {
        return std::nullopt;
    }

   private:
    RuntimeDictionaryIdentity identity_;
};

class ApplicationServiceTest : public QObject {
    Q_OBJECT

   private slots:
    void MissingConfigurationIsACleanProfile();
    void ConfigurationRoundTripsEscapedPaths();
    void ConfigurationValidatesLocalSourcePolicyAtomically();
    void ConfigurationRoundTripsOnlineSourcesDeterministically();
    void ConfigurationRejectsMalformedOnlineSourcesAtomically();
    void ConfigurationRoundTripsExternalProgramsDeterministically();
    void ConfigurationRejectsMalformedExternalProgramsAtomically();
    void ConfigurationRoundTripsDictionaryGroups();
    void ConfigurationRoundTripsArticleTabSession();
    void ConfigurationRejectsMalformedArticleTabSessionsAtomically();
    void ApplicationPreferencesCompareByValue();
    void ConfigurationRoundTripsPreferencesDeterministically();
    void ConfigurationRoundTripsAllFullTextSearchModes();
    void ConfigurationRejectsUnknownFullTextSearchModeAtomically();
    void ConfigurationRoundTripsBoundedFullTextDialogGeometry();
    void ConfigurationRoundTripsBoundedMainWindowGeometry();
    void ConfigurationRoundTripsBoundedMainWindowState();
    void ConfigurationAcceptsLegacyMinimumZoom();
    void ConfigurationRejectsMalformedPreferencesAtomically();
    void ConfigurationRejectsGroupBoundsAndDuplicatesAtomically();
    void ConfigurationRejectsMalformedGroups();
    void PendingTransactionRoundTripsDeterministically();
    void PendingTransactionAcceptsBoundaries();
    void PendingTransactionRejectsMalformedRecords();
    void PendingTransactionRejectsInvalidValues();
    void PreparesConfigurationOnlyWithoutChangingDestinations();
    void PreparesExactHistoryReplacementStates();
    void PreparationFailuresAreAbortableAndBounded();
    void PreparationFilesystemFailuresAreDeterministic();
    void PreparedTransactionOwnsStagingUntilReleased();
    void PersistsConfigurationOnlyThroughDurableDecision();
    void AdvancesAndFinalizesDesiredRuntimeDurably();
    void PersistsConfigurationAndReplacementHistory();
    void PersistenceFailuresBeforePublicationRemainAbortable();
    void PublishedDecisionDirectorySyncFailureRetainsRecoveryState();
    void PostDecisionPersistenceFailuresConvergeOnlyForward();
    void FailureEvidenceNeverOverwritesCorruptPendingRecord();
    void PersistenceCrashCheckpointsReportTruthfulBoundaries();
    void RestoresPreviousConfigurationWithoutInspectingUnchangedHistory();
    void RestoresPreviousHistoryPayloadsAndAbsence();
    void RejectsUnsafePreviousHistoryAbsenceTargets();
    void PreviousPersistenceFailuresRemainForwardAndTruthful();
    void RecoveryPolicyDurablyGatesDesiredAttempts();
    void StartupRecoveryReplaysDesiredPersistence();
    void RecoveryPolicyReportsMarkerPublicationTruthfully();
    void RecoveryPolicyCrashCheckpointsRemainBounded();
    void RecoveryPolicyQuarantinesTerminalBlocks();
    void PreviousRuntimeFailureQuarantineIsTerminal();
    void RecoveryPolicyDefersAndRejectsWithoutMutation();
    void MigratesLegacyPathsWithoutTouchingTheSource();
    void MigratesAllLegacyOnlineSourcesAtomically();
    void RejectsUnsupportedLegacyOnlineSourcesAtomically();
    void MissingLegacyPreferencesRetainCurrentDefaults();
    void MigratesAllLegacyFullTextSearchModes();
    void MigratesBoundedLegacyFullTextDialogGeometry();
    void RejectsMalformedLegacyPreferencesAtomically();
    void CurrentConfigurationTakesPrecedenceOverLegacy();
    void RejectsMalformedLegacyWithoutCreatingCurrent();
    void RejectsMalformedConfiguration();
    void CatalogSanitizesInspectionMetadata();
    void DiscoversAndQueriesARealFixture();
    void RemovesStaleIndexesWhenACompanionBecomesUnavailable();
    void CatalogUsesLegacyDiscoveryOrder();
    void RegistersOnlyEnabledMorphologyDictionaries();
    void AppliesIndependentFullTextQueryLimits();
    void SearchesTwelveLocalFormatsFullTextWithMixedFormatErrors();
    void EnumeratesStarDictHeadwordsWithStableCursors();
    void ExportsCompleteHeadwordListsAtomically();
    void ReturnsCanonicalFoldedMatchInformation();
    void ReturnsMdictSourceHeadword();
    void AppliesLegacySynonymSearchWithoutChangingSuggestions();
    void EnforcesRuntimeDiacriticCapability();
    void ReturnsRankedPrefixMatches();
    void ReturnsLightweightHeadwordSuggestions();
    void RanksMdictWordStartSuggestionsLikeLegacyProduct();
    void RanksAllLegacyWordFinderSuggestionCategories();
    void FiltersBoundedHeadwordSuggestionsWithWildcards();
    void FiltersBoundedHeadwordSuggestionsWithRegularExpressions();
    void RejectsInvalidOrUnseededHeadwordPatterns();
    void RanksSuggestionsAcrossDictionaries();
    void SupportsExplicitEmptyDictionaryParticipation();
    void AppliesResolvedDictionaryGroupsConsistently();
    void DiscoversAndQueriesDictdAlongsideStardict();
    void DiscoversSanitizesAndQueriesSdict();
    void DiscoversSanitizesAndQueriesXdxfResources();
    void DiscoversSanitizesAndQueriesGlsResources();
    void DiscoversSanitizesAndQueriesDslResources();
    void PublishesDslTooltipsAfterSanitization();
    void DiscoversSanitizesAndQueriesBglResources();
    void DiscoversSanitizesAndQueriesMdictResources();
    void DiscoversSanitizesAndQueriesAard();
    void AppliesPersistedFullTextPolicyAfterAardDiscovery();
    void ComposesIdleExecutorAfterAardReconciliation();
    void CancelsPreparedAardFullTextLifecycle();
    void ActivatesAndReplacesPrivateDesktopFacadeCompositions();
    void ReloadsUnchangedDictionarySourcesThroughActivationOwner();
    void PreservesInstalledCandidateWhenAnotherBuildFails();
    void SerializesConcurrentCandidateInstallationAndActivation();
    void ActivationHandleIsOneShotAndStopsOnDestruction();
    void ActivationOwnerDestructionStopsRetainedFacadeState();
    void DiscoversSanitizesAndQueriesZimResources();
    void DiscoversSanitizesAndQueriesSlobResources();
    void DiscoversSanitizesAndQueriesEpwingResources();
    void DiscoversSanitizesAndQueriesLsaAudio();
    void DiscoversSanitizesAndQueriesZipSoundsAudio();
    void QueriesExplicitlyConfiguredSoundDirectory();
    void CompletesAnOwnedAsynchronousLookup();
    void AppliesArticlePreferencesBehindTheDesktopFacade();
    void ResolvesTypedArticleUrlsBehindTheDesktopFacade();
    void BuildsRenderedTextMatchPlansBehindTheDesktopFacade();
    void RejectsInvalidRenderedTextMatchPlanRequests();
    void ReportsCancellationAndUnavailableDictionaries();
    void RejectsUnboundedOrMalformedQueries();
    void RejectsConfiguredInputPhraseLimitsForLookupAndSuggestions();
};

std::filesystem::path TemporaryPath(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

void ApplicationServiceTest::MissingConfigurationIsACleanProfile() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const auto configuration =
        LoadConfiguration((TemporaryPath(directory) / "missing.conf").string());

    QVERIFY(configuration.dictionary_paths.empty());
    QVERIFY(configuration.index_directory.empty());
    QVERIFY(configuration.dictionary_groups.empty());
    QVERIFY(configuration.full_text_dialog_geometry.empty());
    QCOMPARE(
        configuration.forvo_sources,
        (std::vector<ForvoSourceConfiguration>{{"forvo",
                                                "Forvo",
                                                false,
                                                "https://apifree.forvo.com",
                                                {"en", "ru"}}}));
}

void ApplicationServiceTest::CatalogSanitizesInspectionMetadata() {
    std::vector<std::unique_ptr<RuntimeDictionarySource>> sources;
    sources.push_back(std::make_unique<InspectionRuntimeSource>());
    const auto service = CreateDictionaryService({}, std::move(sources));
    const auto catalog = service->GetCatalog();

    QCOMPARE(catalog.size(), std::size_t{1});
    QCOMPARE(catalog.front().source, "https://example.test/path");
    QCOMPARE(catalog.front().description, "Author: Test\n<plain>hidden");
    QCOMPARE(catalog.front().source_language, "en");
    QCOMPARE(catalog.front().target_language, "de");
}

void ApplicationServiceTest::
    RejectsConfiguredInputPhraseLimitsForLookupAndSuggestions() {
    CoreConfiguration configuration;
    configuration.preferences.limit_input_phrase_length = true;
    configuration.preferences.input_phrase_length_limit = 2U;
    const auto service = CreateDictionaryService(configuration);

    LookupQuery lookup;
    lookup.text = u8"a😀";
    QVERIFY(service->Lookup(lookup).errors.empty());
    lookup.text = u8"a😀́";
    const auto rejected_lookup = service->Lookup(lookup);
    QCOMPARE(rejected_lookup.errors.size(), std::size_t{1});
    QCOMPARE(rejected_lookup.errors.front().code,
             LookupErrorCode::kInvalidQuery);
    QVERIFY(rejected_lookup.entries.empty());
    const auto asynchronous = service->StartLookup(lookup)->Await();
    QCOMPARE(asynchronous.errors.front().code, LookupErrorCode::kInvalidQuery);

    SuggestionQuery suggestion;
    suggestion.text = u8"😀😀😀";
    const auto rejected_suggestion = service->Suggest(suggestion);
    QCOMPARE(rejected_suggestion.errors.front().code,
             LookupErrorCode::kInvalidQuery);
    QVERIFY(rejected_suggestion.suggestions.empty());

    configuration.preferences.limit_input_phrase_length = false;
    const auto unlimited = CreateDictionaryService(configuration);
    QVERIFY(unlimited->Lookup(lookup).errors.empty());
}

void ApplicationServiceTest::ConfigurationRoundTripsEscapedPaths() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "nested" / "core.conf";
    CoreConfiguration expected;
    expected.dictionary_paths = {"/dictionaries/English & French",
                                 "/dictionaries/CJK=demo"};
    expected.index_directory = "/cache/index files";
    expected.sound_directories = {
        {"/audio/English | examples", "Spoken English & notes"}};

    SaveConfiguration(path.string(), expected);
    const auto actual = LoadConfiguration(path.string());

    QCOMPARE(actual.dictionary_paths, expected.dictionary_paths);
    QCOMPARE(actual.index_directory, expected.index_directory);
    QCOMPARE(actual.sound_directories.size(), std::size_t{1});
    QCOMPARE(actual.sound_directories.front().path,
             expected.sound_directories.front().path);
    QCOMPARE(actual.sound_directories.front().name,
             expected.sound_directories.front().name);
}

void ApplicationServiceTest::
    ConfigurationValidatesLocalSourcePolicyAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration original;
    original.dictionary_paths = {"/original"};
    SaveConfiguration(path.string(), original);
    const std::string original_bytes = ReadFile(path);

    CoreConfiguration boundary;
    boundary.dictionary_paths.assign(kMaximumDictionaryPaths, "/duplicate");
    boundary.sound_directories.assign(kMaximumSoundDirectories,
                                      {"/duplicate-sound", ""});
    ValidateConfiguration(boundary);
    SaveConfiguration(path.string(), boundary);
    const auto round_trip = LoadConfiguration(path.string());
    QCOMPARE(round_trip.dictionary_paths, boundary.dictionary_paths);
    QCOMPARE(round_trip.sound_directories, boundary.sound_directories);

    SaveConfiguration(path.string(), original);
    const auto rejects_atomically = [&](const CoreConfiguration& invalid) {
        QVERIFY_EXCEPTION_THROWN(ValidateConfiguration(invalid),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                                 std::runtime_error);
        QCOMPARE(ReadFile(path), original_bytes);
        QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));
    };
    auto invalid = original;
    invalid.dictionary_paths.assign(kMaximumDictionaryPaths + 1U, "/too-many");
    rejects_atomically(invalid);
    invalid = original;
    invalid.sound_directories.assign(kMaximumSoundDirectories + 1U,
                                     {"/too-many", "sound"});
    rejects_atomically(invalid);
    invalid = original;
    invalid.sound_directories = {{"", "empty path"}};
    rejects_atomically(invalid);
    invalid = original;
    invalid.dictionary_paths = {std::string("bad\0path", 8U)};
    rejects_atomically(invalid);
}

void ApplicationServiceTest::ConfigurationRoundTripsDictionaryGroups() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration expected;
    expected.dictionary_groups = {{7U,
                                   "English | French",
                                   "flags/Europe & Americas.svg",
                                   {"stardict:alpha|edition", "dictd:beta"},
                                   {"dictd:beta"},
                                   {"unknown:popup"},
                                   "Languages/English",
                                   "Ctrl+1",
                                   "aWNvbg=="},
                                  {42U, "Reference", "", {"zim:encyclopedia"}}};

    SaveConfiguration(path.string(), expected);

    QCOMPARE(LoadConfiguration(path.string()).dictionary_groups,
             expected.dictionary_groups);

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\nindex_directory=\n"
                          "dictionary_group=9|G1-compatible||unknown-id\n");
    const auto g1 = LoadConfiguration(path.string());
    QCOMPARE(g1.dictionary_groups.size(), std::size_t{1});
    QCOMPARE(g1.dictionary_groups.front().dictionary_ids,
             (std::vector<std::string>{"unknown-id"}));
    QVERIFY(g1.dictionary_groups.front().favorites_folder.empty());
}

void ApplicationServiceTest::
    ConfigurationRoundTripsOnlineSourcesDeterministically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration expected;
    expected.mediawiki_sources = {
        {"wiki.en", "English Wiki", true, "https://en.example.test/w"},
        {"wiki.fr", u8"Français", false, "http://fr.example.test/api.php"}};
    expected.website_sources = {
        {"web.search", "Search & Read", true,
         "https://example.test/find?q=%GDWORD%&lang=en"}};
    expected.forvo_sources = {
        {"forvo", "Forvo", true, "https://apifree.forvo.com", {"en", "ru"}}};
    expected.dict_server_sources = {
        {"dict.main", "DICT | Main", true, "dict.example.test", 2628U, "*",
         "prefix"},
        {"dict.v6", "DICT IPv6", false, "2001:db8::1", 1234U, "wn", "exact"}};

    SaveConfiguration(path.string(), expected);
    const std::string first = ReadFile(path);
    const std::string expected_prefix =
        "goldendict-core-config-v1\n"
        "index_directory=\n"
        "mediawiki_source=wiki.en|English%20Wiki|1|https://en.example.test/w\n"
        "mediawiki_source=wiki.fr|Fran%C3%A7ais|0|http://fr.example.test/"
        "api.php\n"
        "website_source=web.search|Search%20%26%20Read|1|"
        "https://example.test/find%3Fq%3D%25GDWORD%25%26lang%3Den|0\n"
        "forvo_sources=1\n"
        "forvo_source=forvo|Forvo|1|https://apifree.forvo.com|2|en|ru\n"
        "dict_server_source=dict.main|DICT%20%7C%20Main|1|dict.example.test|"
        "2628|%2A|prefix\n"
        "dict_server_source=dict.v6|DICT%20IPv6|0|2001:db8::1|1234|wn|exact\n";
    QCOMPARE(first.substr(0U, expected_prefix.size()), expected_prefix);

    const auto actual = LoadConfiguration(path.string());
    QCOMPARE(actual.mediawiki_sources, expected.mediawiki_sources);
    QCOMPARE(actual.website_sources, expected.website_sources);
    QCOMPARE(actual.forvo_sources, expected.forvo_sources);
    QCOMPARE(actual.dict_server_sources, expected.dict_server_sources);
    SaveConfiguration(path.string(), actual);
    QCOMPARE(ReadFile(path), first);

    CoreConfiguration explicitly_empty;
    explicitly_empty.forvo_sources.clear();
    SaveConfiguration(path.string(), explicitly_empty);
    const std::string empty_bytes = ReadFile(path);
    QVERIFY(empty_bytes.find("forvo_sources=0\n") != std::string::npos);
    QVERIFY(empty_bytes.find("forvo_source=") == std::string::npos);
    const auto empty_round_trip = LoadConfiguration(path.string());
    QVERIFY(empty_round_trip.forvo_sources.empty());
    SaveConfiguration(path.string(), empty_round_trip);
    QCOMPARE(ReadFile(path), empty_bytes);

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\nindex_directory=\n");
    const auto older = LoadConfiguration(path.string());
    QCOMPARE(
        older.forvo_sources,
        (std::vector<ForvoSourceConfiguration>{{"forvo",
                                                "Forvo",
                                                false,
                                                "https://apifree.forvo.com",
                                                {"en", "ru"}}}));
}

void ApplicationServiceTest::
    ConfigurationRejectsMalformedOnlineSourcesAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration original;
    original.dictionary_paths = {"/original"};
    SaveConfiguration(path.string(), original);
    const std::string original_bytes = ReadFile(path);
    const auto rejects_save = [&](const CoreConfiguration& invalid) {
        QVERIFY_EXCEPTION_THROWN(ValidateConfiguration(invalid),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                                 std::runtime_error);
        QCOMPARE(ReadFile(path), original_bytes);
        QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));
    };

    CoreConfiguration boundary;
    boundary.mediawiki_sources = {
        {"a" + std::string(127U, 'z'), std::string(256U, 'n'), false,
         "https://example.test/" + std::string(4075U, 'x')}};
    boundary.forvo_sources.front().language_codes.clear();
    for (char first = 'a'; first < 'e'; ++first) {
        for (char second = 'a'; second < 'i'; ++second) {
            boundary.forvo_sources.front().language_codes.push_back(
                std::string{first, second});
        }
    }
    ValidateConfiguration(boundary);

    CoreConfiguration invalid = original;
    invalid.mediawiki_sources = {
        {"same", "Wiki", true, "https://example.test/w"}};
    invalid.website_sources = {
        {"same", "Web", false, "https://example.test/%GDWORD%"}};
    rejects_save(invalid);
    invalid = original;
    invalid.website_sources = {
        {"web", "Web", true, "https://example.test/no-marker"}};
    rejects_save(invalid);
    invalid = original;
    invalid.mediawiki_sources = {
        {"wiki", "Wiki\n", true, "https://example.test/w"}};
    rejects_save(invalid);
    invalid = original;
    invalid.forvo_sources = {
        {"forvo", "Forvo", true, "file:///tmp/api", {"en", "en"}}};
    rejects_save(invalid);
    invalid = original;
    invalid.dict_server_sources = {{"dict", "DICT", true,
                                    "https://dict.example.test", 0U, "bad atom",
                                    "prefix"}};
    rejects_save(invalid);
    invalid = original;
    invalid.mediawiki_sources.assign(
        kMaximumOnlineSources + 1U,
        {"wiki", "Wiki", false, "https://example.test/w"});
    rejects_save(invalid);
    invalid = original;
    invalid.mediawiki_sources = {
        {std::string(129U, 'i'), "Wiki", false, "https://example.test/w"}};
    rejects_save(invalid);
    invalid = original;
    invalid.mediawiki_sources = {
        {"wiki", std::string(257U, 'n'), false, "https://example.test/w"}};
    rejects_save(invalid);
    invalid = original;
    invalid.website_sources = {
        {"web", "Web", false,
         "https://example.test/" + std::string(4076U, 'x') + "%GDWORD%"}};
    rejects_save(invalid);
    invalid = original;
    invalid.forvo_sources.front().language_codes.assign(33U, "en");
    rejects_save(invalid);
    invalid = original;
    invalid.mediawiki_sources = {{"wiki", std::string("Control\xc2\x85", 9U),
                                  false, "https://example.test/w"}};
    rejects_save(invalid);

    const std::vector<std::string> malformed = {
        "mediawiki_source=wiki|Wiki|2|https://example.test/w\n",
        "website_source=web|Web|1|https://example.test/no-marker\n",
        "forvo_sources=0\nforvo_sources=0\n",
        "forvo_sources=0\nforvo_source=forvo|Forvo|0|"
        "https://apifree.forvo.com|2|en|ru\n",
        "forvo_sources=2\nforvo_source=forvo|Forvo|0|"
        "https://apifree.forvo.com|2|en|ru\n",
        "forvo_sources=257\n",
        "forvo_sources=invalid\n",
        "forvo_source=forvo|Forvo|0|https://apifree.forvo.com|2|en|ru\n"
        "forvo_sources=1\n",
        "forvo_source=forvo|Forvo|0|https://apifree.forvo.com|2|en\n",
        "forvo_source=forvo|Forvo|0|https://apifree.forvo.com|2|en|en\n",
        "dict_server_source=dict|DICT|1|dict.example.test|0|%2A|prefix\n",
        "dict_server_source=dict|DICT|1|dict.example.test|2628|bad%20atom|"
        "prefix\n",
        "mediawiki_source=wiki|%C3%28|1|https://example.test/w\n"};
    for (const auto& field : malformed) {
        test::WriteBinaryFile(
            path, "goldendict-core-config-v1\nindex_directory=\n" + field);
        QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                                 std::runtime_error);
    }
}

void ApplicationServiceTest::
    ConfigurationRoundTripsExternalProgramsDeterministically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    const std::string root =
        std::filesystem::current_path().root_path().generic_string();
    const std::string text_executable = root + "opt/goldendict/helper";
    const std::string html_executable = root + "usr/bin/html-helper";
    const std::string prefix_executable = root + "usr/bin/prefix-helper";
    const std::string working_directory = root + "var/tmp";
    CoreConfiguration expected;
    expected.external_program_sources = {
        {"program.text",
         "Plain | Text",
         true,
         ExternalProgramOutputKind::kPlainText,
         text_executable,
         {"--word", "%GDWORD%", ""},
         working_directory},
        {"program.html",
         u8"HTML français",
         false,
         ExternalProgramOutputKind::kHtml,
         html_executable,
         {"stdin"},
         ""},
        {"program.prefix",
         "Prefix",
         true,
         ExternalProgramOutputKind::kPrefixMatch,
         prefix_executable,
         {},
         ""}};

    SaveConfiguration(path.string(), expected);
    const std::string first = ReadFile(path);
    const std::string records =
        "external_programs=3\n"
        "external_program=program.text|Plain%20%7C%20Text|1|0|" +
        text_executable + "|" + working_directory + "|3\n" +
        "external_program_argument=program.text|0|--word\n"
        "external_program_argument=program.text|1|%25GDWORD%25\n"
        "external_program_argument=program.text|2|\n"
        "external_program=program.html|HTML%20fran%C3%A7ais|0|1|" +
        html_executable + "||1\n" +
        "external_program_argument=program.html|0|stdin\n"
        "external_program=program.prefix|Prefix|1|2|" +
        prefix_executable + "||0\n";
    QVERIFY(first.find(records) != std::string::npos);
    const auto actual = LoadConfiguration(path.string());
    QCOMPARE(actual.external_program_sources,
             expected.external_program_sources);
    SaveConfiguration(path.string(), actual);
    QCOMPARE(ReadFile(path), first);

    CoreConfiguration explicitly_empty;
    explicitly_empty.external_program_sources.clear();
    SaveConfiguration(path.string(), explicitly_empty);
    const std::string empty_bytes = ReadFile(path);
    QVERIFY(empty_bytes.find("external_programs=0\n") != std::string::npos);
    QVERIFY(empty_bytes.find("external_program=") == std::string::npos);
    const auto empty_round_trip = LoadConfiguration(path.string());
    QVERIFY(empty_round_trip.external_program_sources.empty());
    SaveConfiguration(path.string(), empty_round_trip);
    QCOMPARE(ReadFile(path), empty_bytes);

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\nindex_directory=\n");
    const auto older = LoadConfiguration(path.string());
    QVERIFY(older.external_program_sources.empty());
    SaveConfiguration(path.string(), older);
    QVERIFY(ReadFile(path).find("external_programs=0\n") != std::string::npos);
}

void ApplicationServiceTest::
    ConfigurationRejectsMalformedExternalProgramsAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    const std::string root =
        std::filesystem::current_path().root_path().generic_string();
    const std::string absolute_helper = root + "bin/helper";
    CoreConfiguration original;
    original.dictionary_paths = {"/original"};
    SaveConfiguration(path.string(), original);
    const std::string original_bytes = ReadFile(path);
    const auto rejects_save = [&](const CoreConfiguration& invalid) {
        QVERIFY_EXCEPTION_THROWN(ValidateConfiguration(invalid),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                                 std::runtime_error);
        QCOMPARE(ReadFile(path), original_bytes);
        QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));
    };

    CoreConfiguration boundary;
    boundary.external_program_sources = {
        {"program",
         std::string(256U, 'n'),
         false,
         ExternalProgramOutputKind::kPrefixMatch,
         root + std::string(4096U - root.size(), 'x'),
         {std::string(16U * 1024U, 'a'), ""},
         root + std::string(4096U - root.size(), 'w')}};
    ValidateConfiguration(boundary);
    boundary.external_program_sources.clear();
    for (std::size_t index = 0U; index < kMaximumOnlineSources; ++index) {
        boundary.external_program_sources.push_back(
            {"program-" + std::to_string(index),
             "Program",
             false,
             ExternalProgramOutputKind::kPlainText,
             absolute_helper,
             {},
             ""});
    }
    ValidateConfiguration(boundary);
    boundary.external_program_sources = {
        {"program", "Program", false, ExternalProgramOutputKind::kPlainText,
         absolute_helper,
         std::vector<std::string>(kMaximumExternalProgramArguments, "arg"),
         ""}};
    ValidateConfiguration(boundary);

    CoreConfiguration invalid = original;
    invalid.mediawiki_sources = {
        {"same", "Wiki", false, "https://example.test/w"}};
    invalid.external_program_sources = {{"same",
                                         "Program",
                                         false,
                                         ExternalProgramOutputKind::kPlainText,
                                         absolute_helper,
                                         {},
                                         ""}};
    rejects_save(invalid);
    invalid = original;
    invalid.website_sources = {
        {"same", "Website", false, "https://example.test/%GDWORD%"}};
    invalid.external_program_sources = {{"same",
                                         "Program",
                                         false,
                                         ExternalProgramOutputKind::kPlainText,
                                         absolute_helper,
                                         {},
                                         ""}};
    rejects_save(invalid);
    invalid = original;
    invalid.forvo_sources.front().id = "same";
    invalid.external_program_sources = {{"same",
                                         "Program",
                                         false,
                                         ExternalProgramOutputKind::kPlainText,
                                         absolute_helper,
                                         {},
                                         ""}};
    rejects_save(invalid);
    invalid = original;
    invalid.dict_server_sources = {
        {"same", "DICT", false, "dict.example.test"}};
    invalid.external_program_sources = {{"same",
                                         "Program",
                                         false,
                                         ExternalProgramOutputKind::kPlainText,
                                         absolute_helper,
                                         {},
                                         ""}};
    rejects_save(invalid);
    invalid = original;
    invalid.external_program_sources = {{"same",
                                         "Program One",
                                         false,
                                         ExternalProgramOutputKind::kPlainText,
                                         absolute_helper,
                                         {},
                                         ""},
                                        {"same",
                                         "Program Two",
                                         false,
                                         ExternalProgramOutputKind::kHtml,
                                         absolute_helper,
                                         {},
                                         ""}};
    rejects_save(invalid);
    invalid = original;
    invalid.external_program_sources = {{"program",
                                         "Program",
                                         false,
                                         ExternalProgramOutputKind::kPlainText,
                                         "relative/helper",
                                         {},
                                         ""}};
    rejects_save(invalid);
    invalid.external_program_sources.front().executable = absolute_helper;
    invalid.external_program_sources.front().working_directory = "relative";
    rejects_save(invalid);
    invalid.external_program_sources.front().working_directory.clear();
    invalid.external_program_sources.front().output_kind =
        static_cast<ExternalProgramOutputKind>(3U);
    rejects_save(invalid);
    invalid.external_program_sources.front().output_kind =
        ExternalProgramOutputKind::kHtml;
    invalid.external_program_sources.front().argument_templates = {
        std::string("bad\0argument", 12U)};
    rejects_save(invalid);
    invalid.external_program_sources.front().argument_templates = {
        std::string(16U * 1024U + 1U, 'a')};
    rejects_save(invalid);
    invalid.external_program_sources.front().argument_templates = {"bad\n"};
    rejects_save(invalid);
    invalid.external_program_sources.front().argument_templates = {
        std::string(1U, static_cast<char>(0xff))};
    rejects_save(invalid);
    invalid.external_program_sources.front().argument_templates.clear();
    invalid.external_program_sources.front().executable =
        "/" + std::string(4096U, 'x');
    rejects_save(invalid);
    invalid.external_program_sources.front().executable =
        std::string("/bad\0path", 9U);
    rejects_save(invalid);
    invalid.external_program_sources.front().executable =
        std::string("/bad\xc2\x85");
    rejects_save(invalid);
    invalid.external_program_sources.front().executable =
        std::string("/bad") + static_cast<char>(0xff);
    rejects_save(invalid);
    invalid.external_program_sources.assign(
        kMaximumOnlineSources + 1U, {"program",
                                     "Program",
                                     false,
                                     ExternalProgramOutputKind::kPlainText,
                                     absolute_helper,
                                     {},
                                     ""});
    rejects_save(invalid);

    const std::vector<std::string> malformed = {
        "external_programs=0\nexternal_programs=0\n",
        "external_programs=257\n",
        "external_programs=invalid\n",
        "external_program=program|Program|1|0|%2Fbin%2Fhelper||0\n",
        "external_programs=0\nexternal_program=program|Program|1|0|"
        "%2Fbin%2Fhelper||0\n",
        "external_programs=2\nexternal_program=program|Program|1|0|"
        "%2Fbin%2Fhelper||0\n",
        "external_programs=1\nexternal_program=program|Program|2|0|"
        "%2Fbin%2Fhelper||0\n",
        "external_programs=1\nexternal_program=program|Program|1|3|"
        "%2Fbin%2Fhelper||0\n",
        "external_programs=1\nexternal_program=program|Program|1|0|"
        "%2Fbin%2Fhelper||1\n",
        "external_programs=1\nexternal_program_argument=program|0|arg\n",
        "external_programs=1\nexternal_program=program|Program|1|0|"
        "%2Fbin%2Fhelper||1\nindex_directory=\n",
        "external_programs=1\nexternal_program=program|Program|1|0|"
        "%2Fbin%2Fhelper||1\nexternal_program_argument=other|0|arg\n",
        "external_programs=1\nexternal_program=program|Program|1|0|"
        "%2Fbin%2Fhelper||1\nexternal_program_argument=program|1|arg\n",
        "external_programs=1\nexternal_program=program|Program|1|0|"
        "%2Fbin%2Fhelper||1\nexternal_program_argument=program|0|%C3%28\n",
        "external_programs=1\nexternal_program=program|Program|1|0|"
        "relative||0\n"};
    for (const auto& field : malformed) {
        test::WriteBinaryFile(
            path, "goldendict-core-config-v1\nindex_directory=\n" + field);
        QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                                 std::runtime_error);
    }
}

void ApplicationServiceTest::ConfigurationRoundTripsArticleTabSession() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration expected;
    TabNavigationState lookup;
    lookup.kind = TabNavigationKind::kLookup;
    lookup.query = "alpha | beta";
    lookup.group_id = 7U;
    lookup.title = "Alpha & Beta";
    lookup.dictionary_filter_active = true;
    lookup.dictionary_ids = {"second|dictionary", "first", "second|dictionary"};
    lookup.exact_target =
        ExactArticleTarget{"first", "opaque|document/identity"};
    TabNavigationState empty_scope = lookup;
    empty_scope.query = "authoritative empty";
    empty_scope.title = empty_scope.query;
    empty_scope.dictionary_ids.clear();
    empty_scope.exact_target.reset();
    TabNavigationState link;
    link.kind = TabNavigationKind::kInternalLink;
    link.query = "linked";
    link.group_id = 9U;
    link.title = "Linked";
    link.internal_url = "goldendict://lookup/linked%20word";
    link.source_dictionary_id = "dict|source";
    link.source_article_id = "source/article";
    link.target_article_id = "target:article";
    link.target_anchor = "part 2";
    expected.article_tab_session = ArticleTabSession{
        {{7U, {lookup, link}, 1U}, {20U, {empty_scope, lookup}, 0U}}, 20U};

    SaveConfiguration(path.string(), expected);
    const std::string canonical = ReadFile(path);
    const auto actual = LoadConfiguration(path.string());
    QVERIFY(actual.article_tab_session.has_value());
    QCOMPARE(*actual.article_tab_session, *expected.article_tab_session);
    SaveConfiguration(path.string(), actual);
    QCOMPARE(ReadFile(path), canonical);

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\nindex_directory=\n"
                          "article_tab_session=1\narticle_tab=1|0\n"
                          "article_tab_navigation=1|1|legacy|0|legacy|||||\n");
    const auto legacy = LoadConfiguration(path.string());
    QVERIFY(legacy.article_tab_session.has_value());
    const auto& legacy_navigation =
        legacy.article_tab_session->tabs.front().history.front();
    QVERIFY(!legacy_navigation.dictionary_filter_active);
    QVERIFY(legacy_navigation.dictionary_ids.empty());
    QVERIFY(!legacy_navigation.exact_target.has_value());

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\nindex_directory=\n");
    QVERIFY(!LoadConfiguration(path.string()).article_tab_session.has_value());
}

void ApplicationServiceTest::
    ConfigurationRejectsMalformedArticleTabSessionsAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration original;
    original.dictionary_paths = {"/original"};
    SaveConfiguration(path.string(), original);
    const std::string original_bytes = ReadFile(path);

    CoreConfiguration invalid = original;
    invalid.article_tab_session = ArticleTabSession{};
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);
    QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));

    ArticleTabSession over_limit;
    over_limit.active_tab_id = 1U;
    TabNavigationState navigation;
    navigation.kind = TabNavigationKind::kLookup;
    navigation.query = "bounded";
    navigation.title = "bounded";
    for (ArticleTabId id = 1U; id <= kMaximumArticleTabs + 1U; ++id) {
        over_limit.tabs.push_back({id, {navigation}, 0U});
    }
    invalid.article_tab_session = std::move(over_limit);
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);
    QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));

    std::vector<std::string> malformed = {
        "article_tab=1|0\n",
        "article_tab_session=1\n",
        "article_tab_session=1\narticle_tab_session=1\n",
        "article_tab_session=1\narticle_tab=1|0\n",
        "article_tab_session=1\narticle_tab=1|0\narticle_tab=1|0\n",
        "article_tab_session=2\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||\n",
        "article_tab_session=1\narticle_tab=1|1\n"
        "article_tab_navigation=1|1|word|0|word|||||\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=2|1|word|0|word|||||\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|99|word|0|word|||||\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||2|0\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||0|1|id\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|2|id\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|1|\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|1|%C3%28\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|1|embedded%00nul\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|1|id|1\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|1|id|0|extra\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|1|id|1||document\n",
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|1|id|1|dictionary|\n",
        "article_tab_session=18446744073709551615\n"
        "article_tab=18446744073709551615|0\n"
        "article_tab_navigation=18446744073709551615|1|word|0|word|||||\n",
    };
    malformed.push_back(
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|101\n");
    malformed.push_back(
        "article_tab_session=1\narticle_tab=1|0\n"
        "article_tab_navigation=1|1|word|0|word|||||1|1|" +
        std::string(kMaximumLookupFilterBytes + 1U, 'x') + "\n");
    for (const auto& fields : malformed) {
        test::WriteBinaryFile(path, "goldendict-core-config-v1\n" + fields);
        QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                                 std::runtime_error);
    }
}

void ApplicationServiceTest::ApplicationPreferencesCompareByValue() {
    static_assert(
        std::is_same_v<decltype(ApplicationPreferences::
                                    full_text_maximum_dictionary_articles),
                       std::uint32_t>);
    ApplicationPreferences first;
    ApplicationPreferences second;

    QVERIFY(first == second);
    QVERIFY(!(first != second));

    second.interface_language = "fr_FR";
    QVERIFY(first != second);
    QVERIFY(!(first == second));

    first.interface_language = second.interface_language;
    second.open_new_tabs_after_current = true;
    QVERIFY(first != second);
    first.open_new_tabs_after_current = true;
    second.hide_single_tab = true;
    QVERIFY(first != second);
    first.hide_single_tab = true;
    second.mru_tab_order = true;
    QVERIFY(first != second);
    first.mru_tab_order = true;
    second.escape_hides_main_window = true;
    QVERIFY(first != second);
    first.escape_hides_main_window = true;
    second.double_click_translates = false;
    QVERIFY(first != second);
    first.double_click_translates = false;
    second.select_word_by_single_click = true;
    QVERIFY(first != second);
    first.select_word_by_single_click = true;
    second.confirm_favorites_deletion = false;
    QVERIFY(first != second);
    second.confirm_favorites_deletion = true;
    second.full_text_search_mode = FullTextSearchMode::kWildcard;
    QVERIFY(first != second);
}

void ApplicationServiceTest::
    ConfigurationRoundTripsPreferencesDeterministically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration expected;
    auto& preferences = expected.preferences;
    preferences.interface_language = "zh_CN";
    preferences.display_style = "dark | high contrast";
    preferences.open_new_tabs_after_current = true;
    preferences.open_new_tabs_in_background = false;
    preferences.hide_single_tab = true;
    preferences.mru_tab_order = true;
    preferences.escape_hides_main_window = true;
    preferences.double_click_translates = false;
    preferences.select_word_by_single_click = true;
    preferences.enable_tray_icon = false;
    preferences.main_window_hotkey = "Alt+Space";
    preferences.scan_popup_modifiers = 0x021U;
    preferences.scan_popup_alt_mode_seconds = 12U;
    preferences.scan_popup_window_mode = ScanPopupWindowMode::kTool;
    preferences.audio_backend = AudioBackend::kQtMultimedia;
    preferences.proxy_mode = ProxyMode::kManual;
    preferences.proxy_type = ProxyType::kHttpConnect;
    preferences.proxy_host = "proxy.example";
    preferences.proxy_port = 8443U;
    preferences.maximum_network_cache_megabytes = 256U;
    preferences.clear_network_cache_on_exit = false;
    preferences.zoom_factor = 1.375;
    preferences.help_zoom_factor = 0.75;
    preferences.words_zoom_level = -2;
    preferences.maximum_history_entries = 1234U;
    preferences.confirm_favorites_deletion = false;
    preferences.always_expand_optional_parts = true;
    preferences.collapse_large_articles = true;
    preferences.article_size_limit = 4096U;
    preferences.limit_input_phrase_length = true;
    preferences.input_phrase_length_limit = 321U;
    preferences.maximum_dictionary_references = 9999U;
    preferences.synonym_search_enabled = false;
    preferences.full_text_search_mode = FullTextSearchMode::kRegularExpression;
    preferences.full_text_match_case = true;
    preferences.full_text_maximum_articles_per_dictionary = 4321U;
    preferences.full_text_maximum_word_distance = 9U;
    preferences.full_text_maximum_dictionary_articles = 10000000U;
    preferences.full_text_disabled_types = "audio|images";

    SaveConfiguration(path.string(), expected);
    const std::string first = ReadFile(path);
    QVERIFY(first.find("preference=mru_tab_order|1\n") != std::string::npos);
    QVERIFY(first.find("preference=escape_hides_main_window|1\n") !=
            std::string::npos);
    QVERIFY(first.find("preference=double_click_translates|0\n") !=
            std::string::npos);
    QVERIFY(first.find("preference=select_word_by_single_click|1\n") !=
            std::string::npos);
    QVERIFY(
        first.find(
            "preference=full_text_maximum_dictionary_megabytes|10000000\n") !=
        std::string::npos);
    QVERIFY(first.find("full_text_maximum_dictionary_articles") ==
            std::string::npos);
    const auto actual = LoadConfiguration(path.string());

    QCOMPARE(actual.preferences.interface_language,
             preferences.interface_language);
    QCOMPARE(actual.preferences.display_style, preferences.display_style);
    QCOMPARE(actual.preferences.open_new_tabs_after_current, true);
    QCOMPARE(actual.preferences.open_new_tabs_in_background, false);
    QCOMPARE(actual.preferences.hide_single_tab, true);
    QCOMPARE(actual.preferences.mru_tab_order, true);
    QCOMPARE(actual.preferences.escape_hides_main_window, true);
    QCOMPARE(actual.preferences.double_click_translates, false);
    QCOMPARE(actual.preferences.select_word_by_single_click, true);
    QCOMPARE(actual.preferences.enable_tray_icon, preferences.enable_tray_icon);
    QCOMPARE(actual.preferences.scan_popup_modifiers,
             preferences.scan_popup_modifiers);
    QCOMPARE(actual.preferences.scan_popup_window_mode,
             preferences.scan_popup_window_mode);
    QCOMPARE(actual.preferences.audio_backend, preferences.audio_backend);
    QCOMPARE(actual.preferences.proxy_mode, preferences.proxy_mode);
    QCOMPARE(actual.preferences.proxy_type, preferences.proxy_type);
    QCOMPARE(actual.preferences.proxy_host, preferences.proxy_host);
    QCOMPARE(actual.preferences.proxy_port, preferences.proxy_port);
    QCOMPARE(actual.preferences.zoom_factor, preferences.zoom_factor);
    QCOMPARE(actual.preferences.words_zoom_level, preferences.words_zoom_level);
    QCOMPARE(actual.preferences.article_size_limit,
             preferences.article_size_limit);
    QCOMPARE(actual.preferences.limit_input_phrase_length, true);
    QCOMPARE(actual.preferences.input_phrase_length_limit, std::uint32_t{321});
    QCOMPARE(actual.preferences.maximum_dictionary_references,
             std::uint16_t{9999});
    QCOMPARE(actual.preferences.confirm_favorites_deletion, false);
    QCOMPARE(actual.preferences.always_expand_optional_parts, true);
    QCOMPARE(actual.preferences.synonym_search_enabled,
             preferences.synonym_search_enabled);
    QCOMPARE(actual.preferences.full_text_search_mode,
             preferences.full_text_search_mode);
    QCOMPARE(actual.preferences.full_text_match_case,
             preferences.full_text_match_case);
    QCOMPARE(actual.preferences.full_text_maximum_articles_per_dictionary,
             std::uint32_t{4321});
    QCOMPARE(actual.preferences.full_text_maximum_dictionary_articles,
             std::uint32_t{10000000});
    QCOMPARE(actual.preferences.full_text_disabled_types,
             preferences.full_text_disabled_types);

    SaveConfiguration(path.string(), actual);
    QCOMPARE(ReadFile(path), first);

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\nindex_directory=\n");
    const auto older = LoadConfiguration(path.string());
    QCOMPARE(older.preferences.interface_language, std::string{});
    QCOMPARE(older.preferences.enable_tray_icon, true);
    QCOMPARE(older.preferences.open_new_tabs_after_current, false);
    QCOMPARE(older.preferences.open_new_tabs_in_background, true);
    QCOMPARE(older.preferences.hide_single_tab, false);
    QCOMPARE(older.preferences.mru_tab_order, false);
    QCOMPARE(older.preferences.escape_hides_main_window, false);
    QCOMPARE(older.preferences.double_click_translates, true);
    QCOMPARE(older.preferences.select_word_by_single_click, false);
    QCOMPARE(older.preferences.zoom_factor, 1.0);
    QCOMPARE(older.preferences.maximum_history_entries, std::uint32_t{500});
    QCOMPARE(older.preferences.confirm_favorites_deletion, true);
    QCOMPARE(older.preferences.always_expand_optional_parts, false);
    QCOMPARE(older.preferences.limit_input_phrase_length, false);
    QCOMPARE(older.preferences.input_phrase_length_limit, std::uint32_t{1000});
    QCOMPARE(older.preferences.maximum_dictionary_references,
             std::uint16_t{20});
    QCOMPARE(older.preferences.full_text_maximum_dictionary_articles,
             std::uint32_t{0});

    expected.preferences.maximum_dictionary_references = 0U;
    SaveConfiguration(path.string(), expected);
    QCOMPARE(LoadConfiguration(path.string())
                 .preferences.maximum_dictionary_references,
             std::uint16_t{0});

    for (const bool translate : {false, true}) {
        for (const bool select : {false, true}) {
            CoreConfiguration combination;
            combination.preferences.double_click_translates = translate;
            combination.preferences.select_word_by_single_click = select;
            SaveConfiguration(path.string(), combination);
            const auto round_trip = LoadConfiguration(path.string());
            QCOMPARE(round_trip.preferences.double_click_translates, translate);
            QCOMPARE(round_trip.preferences.select_word_by_single_click,
                     select);
        }
    }
}

void ApplicationServiceTest::ConfigurationRoundTripsAllFullTextSearchModes() {
    static_assert(std::is_same_v<std::underlying_type_t<FullTextSearchMode>,
                                 std::uint8_t>);
    const std::pair<FullTextSearchMode, std::uint8_t> modes[] = {
        {FullTextSearchMode::kWholeWords, 0U},
        {FullTextSearchMode::kWildcard, 1U},
        {FullTextSearchMode::kRegularExpression, 2U},
        {FullTextSearchMode::kPlainText, 3U},
    };
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";

    for (const auto& [mode, ordinal] : modes) {
        QCOMPARE(static_cast<std::uint8_t>(mode), ordinal);
        CoreConfiguration configuration;
        configuration.preferences.full_text_search_mode = mode;
        SaveConfiguration(path.string(), configuration);
        QCOMPARE(
            LoadConfiguration(path.string()).preferences.full_text_search_mode,
            mode);
        QVERIFY(ReadFile(path).find(
                    "preference=full_text_search_mode|" +
                    std::to_string(static_cast<unsigned int>(ordinal)) +
                    "\n") != std::string::npos);

        test::WriteBinaryFile(
            path,
            "goldendict-core-config-v1\n"
            "preference=full_text_search_mode|" +
                std::to_string(static_cast<unsigned int>(ordinal)) + "\n");
        QCOMPARE(
            LoadConfiguration(path.string()).preferences.full_text_search_mode,
            mode);
    }
}

void ApplicationServiceTest::
    ConfigurationRejectsUnknownFullTextSearchModeAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration preserved;
    preserved.dictionary_paths = {"/preserved"};
    SaveConfiguration(path.string(), preserved);
    const std::string original = ReadFile(path);

    CoreConfiguration invalid = preserved;
    invalid.preferences.full_text_search_mode =
        static_cast<FullTextSearchMode>(4U);
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original);
    QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\n"
                          "preference=full_text_search_mode|4\n");
    QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                             std::runtime_error);
}

void ApplicationServiceTest::
    ConfigurationRoundTripsBoundedFullTextDialogGeometry() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration expected;
    expected.dictionary_paths = {"/preserved"};
    expected.full_text_dialog_geometry.assign(64U * 1024U, '\0');
    expected.full_text_dialog_geometry[1] = '%';
    expected.full_text_dialog_geometry[2] = '\n';

    SaveConfiguration(path.string(), expected);
    QCOMPARE(LoadConfiguration(path.string()).full_text_dialog_geometry,
             expected.full_text_dialog_geometry);
    const std::string canonical = ReadFile(path);
    QVERIFY(canonical.find("full_text_dialog_geometry=%00%25%0A") !=
            std::string::npos);

    const std::string original = canonical;
    expected.full_text_dialog_geometry.push_back('x');
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), expected),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original);
    QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\n"
                          "dictionary_path=/unchanged\n"
                          "full_text_dialog_geometry=first\n"
                          "full_text_dialog_geometry=second\n");
    QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                             std::runtime_error);

    for (const std::string malformed : {"%", "%GG"}) {
        test::WriteBinaryFile(path,
                              "goldendict-core-config-v1\n"
                              "dictionary_path=/unchanged\n"
                              "full_text_dialog_geometry=" +
                                  malformed + "\n");
        QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                                 std::runtime_error);
    }

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\n"
                          "dictionary_path=/unchanged\n"
                          "full_text_dialog_geometry=" +
                              std::string(64U * 1024U + 1U, 'x') + "\n");
    QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                             std::runtime_error);

    CoreConfiguration empty;
    SaveConfiguration(path.string(), empty);
    QVERIFY(ReadFile(path).find("full_text_dialog_geometry=") ==
            std::string::npos);
    QVERIFY(LoadConfiguration(path.string()).full_text_dialog_geometry.empty());
}

void ApplicationServiceTest::
    ConfigurationRoundTripsBoundedMainWindowGeometry() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration expected;
    expected.dictionary_paths = {"/preserved"};
    expected.main_window_geometry.assign(64U * 1024U, '\0');
    expected.main_window_geometry[1] = '%';
    expected.main_window_geometry[2] = '\n';

    SaveConfiguration(path.string(), expected);
    QCOMPARE(LoadConfiguration(path.string()).main_window_geometry,
             expected.main_window_geometry);

    const std::string original = ReadFile(path);
    expected.main_window_geometry.push_back('x');
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), expected),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original);
    QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\n"
                          "main_window_geometry=first\n"
                          "main_window_geometry=second\n");
    QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                             std::runtime_error);

    for (const std::string malformed : {"%", "%GG"}) {
        test::WriteBinaryFile(path,
                              "goldendict-core-config-v1\n"
                              "main_window_state=" +
                                  malformed + "\n");
        QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                                 std::runtime_error);
    }
}

void ApplicationServiceTest::ConfigurationRoundTripsBoundedMainWindowState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration expected;
    expected.dictionary_paths = {"/preserved"};
    expected.main_window_state.assign(64U * 1024U, '\0');
    expected.main_window_state[1] = '%';
    expected.main_window_state[2] = '\n';

    SaveConfiguration(path.string(), expected);
    QCOMPARE(LoadConfiguration(path.string()).main_window_state,
             expected.main_window_state);

    const std::string original = ReadFile(path);
    expected.main_window_state.push_back('x');
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), expected),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original);
    QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\n"
                          "main_window_state=first\n"
                          "main_window_state=second\n");
    QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                             std::runtime_error);
}

void ApplicationServiceTest::ConfigurationAcceptsLegacyMinimumZoom() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration configuration;
    configuration.dictionary_paths = {"/preserved"};
    configuration.preferences.zoom_factor = 0.1;

    SaveConfiguration(path.string(), configuration);

    QCOMPARE(LoadConfiguration(path.string()).preferences.zoom_factor, 0.1);
}

void ApplicationServiceTest::
    ConfigurationRejectsMalformedPreferencesAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration original;
    original.dictionary_paths = {"/original"};
    SaveConfiguration(path.string(), original);
    const std::string original_bytes = ReadFile(path);

    CoreConfiguration invalid = original;
    invalid.preferences.proxy_mode = ProxyMode::kManual;
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);
    QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));

    invalid = original;
    invalid.preferences.proxy_mode = ProxyMode::kManual;
    invalid.preferences.proxy_type = ProxyType::kHttpConnect;
    invalid.preferences.proxy_host = "proxy.example\r\nInjected: value";
    invalid.preferences.proxy_port = 3128U;
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    invalid = original;
    invalid.preferences.interface_language.assign(4097U, 'x');
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    invalid = original;
    invalid.preferences.interface_language = std::string("bad\xc3\x28", 5U);
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    invalid = original;
    invalid.preferences.maximum_dictionary_references = 10000U;
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    invalid = original;
    invalid.preferences.full_text_maximum_dictionary_articles = 10000001U;
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    invalid = original;
    invalid.preferences.zoom_factor = 5.01;
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    invalid = original;
    invalid.preferences.maximum_network_cache_megabytes = 10241U;
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    invalid = original;
    invalid.preferences.maximum_history_entries = 100000U;
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    invalid = original;
    invalid.preferences.limit_input_phrase_length = true;
    invalid.preferences.input_phrase_length_limit = 2U;
    ArticleTabSession session;
    session.active_tab_id = 1U;
    TabNavigationState oversized_navigation;
    oversized_navigation.kind = TabNavigationKind::kLookup;
    oversized_navigation.query = u8"😀😀😀";
    oversized_navigation.title = oversized_navigation.query;
    session.tabs = {{1U, {oversized_navigation}, 0U}};
    invalid.article_tab_session = session;
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    const std::vector<std::string> malformed = {
        "preference=enable_tray_icon|true\n",
        "preference=open_new_tabs_in_background|true\n",
        "preference=hide_single_tab|true\n",
        "preference=hide_single_tab|1\npreference=hide_single_tab|0\n",
        "preference=mru_tab_order|true\n",
        "preference=mru_tab_order|1\npreference=mru_tab_order|0\n",
        "preference=proxy_type|9\n",
        "preference=maximum_network_cache_megabytes|10241\n",
        "preference=full_text_search_mode|4\n",
        std::string("preference=interface_language|bad\xc3\x28\n"),
        "preference=zoom_factor|nan\n",
        "preference=scan_popup_modifiers|65535\n",
        "preference=store_history|1\npreference=store_history|0\n",
    };
    for (const auto& field : malformed) {
        test::WriteBinaryFile(path, "goldendict-core-config-v1\n" + field);
        QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                                 std::runtime_error);
    }
}

void ApplicationServiceTest::
    ConfigurationRejectsGroupBoundsAndDuplicatesAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    CoreConfiguration original;
    original.dictionary_groups = {{1U, "Original", "", {"dictionary-a"}}};
    SaveConfiguration(path.string(), original);

    CoreConfiguration duplicate_group_ids;
    duplicate_group_ids.dictionary_groups = {
        {2U, "First", "", {"dictionary-a"}},
        {2U, "Second", "", {"dictionary-b"}}};
    QVERIFY_EXCEPTION_THROWN(
        SaveConfiguration(path.string(), duplicate_group_ids),
        std::runtime_error);
    QCOMPARE(LoadConfiguration(path.string()).dictionary_groups,
             original.dictionary_groups);
    QVERIFY(!std::filesystem::exists(path.string() + ".tmp"));

    CoreConfiguration duplicate_dictionary_ids;
    duplicate_dictionary_ids.dictionary_groups = {
        {3U, "Duplicates", "", {"same", "same"}}};
    QVERIFY_EXCEPTION_THROWN(
        SaveConfiguration(path.string(), duplicate_dictionary_ids),
        std::runtime_error);

    CoreConfiguration too_many_groups;
    too_many_groups.dictionary_groups.reserve(257U);
    for (std::uint32_t id = 1U; id <= 257U; ++id) {
        too_many_groups.dictionary_groups.push_back({id, "Bounded", "", {}});
    }
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), too_many_groups),
                             std::runtime_error);

    CoreConfiguration too_many_members;
    DictionaryGroupConfiguration group{4U, "Bounded", "", {}};
    group.dictionary_ids.reserve(257U);
    for (std::size_t index = 0U; index < 257U; ++index) {
        group.dictionary_ids.push_back("dictionary-" + std::to_string(index));
    }
    too_many_members.dictionary_groups.push_back(std::move(group));
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), too_many_members),
                             std::runtime_error);

    CoreConfiguration icon_boundary;
    DictionaryGroupConfiguration bounded_icon{5U, "Icon", "", {}};
    bounded_icon.encoded_icon_data.assign(64U * 1024U, 'A');
    icon_boundary.dictionary_groups.push_back(bounded_icon);
    SaveConfiguration(path.string(), icon_boundary);
    QCOMPARE(LoadConfiguration(path.string()).dictionary_groups,
             icon_boundary.dictionary_groups);
    icon_boundary.dictionary_groups.front().encoded_icon_data.append(4U, 'A');
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), icon_boundary),
                             std::runtime_error);
    QCOMPARE(LoadConfiguration(path.string()).dictionary_groups,
             (std::vector<DictionaryGroupConfiguration>{bounded_icon}));
}

void ApplicationServiceTest::ConfigurationRejectsMalformedGroups() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\nindex_directory=\n"
                          "dictionary_group=not-an-id|Name|\n");
    QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                             std::runtime_error);

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\nindex_directory=\n"
                          "dictionary_group=9|Name|Icon|duplicate|duplicate\n");
    QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                             std::runtime_error);

    test::WriteBinaryFile(path,
                          "goldendict-core-config-v1\nindex_directory=\n"
                          "dictionary_group=9|Name|\n"
                          "dictionary_group_metadata=9|||not-base64|0|0\n");
    QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                             std::runtime_error);
}

void ApplicationServiceTest::PendingTransactionRoundTripsDeterministically() {
    const auto expected = CompletePendingRecord();
    const auto first = SerializePendingConfigurationTransaction(expected);
    const auto actual = ParsePendingConfigurationTransaction(first);
    QCOMPARE(actual, expected);
    QCOMPARE(SerializePendingConfigurationTransaction(actual), first);

    auto unchanged = expected;
    unchanged.history_intent = PendingHistoryIntent::kUnchanged;
    unchanged.desired_history.reset();
    unchanged.previous_configuration = {};
    unchanged.previous_history = {};
    unchanged.failure.reset();
    const auto unchanged_bytes =
        SerializePendingConfigurationTransaction(unchanged);
    QCOMPARE(ParsePendingConfigurationTransaction(unchanged_bytes), unchanged);

    unchanged.failure = PendingFailureIdentity{
        PendingFailureOperation::kValidateRecord,
        PendingFailureDestination::kPendingRecord,
        PendingFailureCategory::kInvalidData, "phase_evidence"};

    for (const auto phase :
         {PendingTransactionPhase::kPrepared,
          PendingTransactionPhase::kDesiredCommit,
          PendingTransactionPhase::kDesiredPersistenceApplying,
          PendingTransactionPhase::kDesiredPersistenceApplied,
          PendingTransactionPhase::kDesiredPersistenceFailed,
          PendingTransactionPhase::kDesiredRuntimeApplying,
          PendingTransactionPhase::kDesiredRuntimeFailed,
          PendingTransactionPhase::kPreviousPersistenceApplying,
          PendingTransactionPhase::kPreviousPersistenceBlocked,
          PendingTransactionPhase::kPreviousRuntimeApplying,
          PendingTransactionPhase::kQuarantined}) {
        unchanged.phase = phase;
        QCOMPARE(ParsePendingConfigurationTransaction(
                     SerializePendingConfigurationTransaction(unchanged)),
                 unchanged);
    }

    for (const auto attempt : {DesiredRecoveryAttempt::kNotAttempted,
                               DesiredRecoveryAttempt::kAttempted}) {
        unchanged.desired_recovery_attempt = attempt;
        QCOMPARE(ParsePendingConfigurationTransaction(
                     SerializePendingConfigurationTransaction(unchanged)),
                 unchanged);
    }

    for (const auto operation : {PendingFailureOperation::kReadRecord,
                                 PendingFailureOperation::kValidateRecord,
                                 PendingFailureOperation::kPersistDesired,
                                 PendingFailureOperation::kReconstructDesired,
                                 PendingFailureOperation::kPersistPrevious,
                                 PendingFailureOperation::kReconstructPrevious,
                                 PendingFailureOperation::kQuarantine}) {
        unchanged.failure = PendingFailureIdentity{
            operation, PendingFailureDestination::kPendingRecord,
            PendingFailureCategory::kUnknown, "classified"};
        QCOMPARE(ParsePendingConfigurationTransaction(
                     SerializePendingConfigurationTransaction(unchanged)),
                 unchanged);
    }
    for (const auto destination :
         {PendingFailureDestination::kPendingRecord,
          PendingFailureDestination::kConfiguration,
          PendingFailureDestination::kHistory,
          PendingFailureDestination::kRuntimeFoundation,
          PendingFailureDestination::kRuntimeTransport,
          PendingFailureDestination::kRuntimePresentation}) {
        unchanged.failure = PendingFailureIdentity{
            PendingFailureOperation::kReconstructDesired, destination,
            PendingFailureCategory::kUnknown, "classified"};
        QCOMPARE(ParsePendingConfigurationTransaction(
                     SerializePendingConfigurationTransaction(unchanged)),
                 unchanged);
    }
    for (const auto category :
         {PendingFailureCategory::kNotFound,
          PendingFailureCategory::kInvalidData, PendingFailureCategory::kIo,
          PendingFailureCategory::kPermission,
          PendingFailureCategory::kResourceLimit,
          PendingFailureCategory::kUnavailable,
          PendingFailureCategory::kRejected, PendingFailureCategory::kCancelled,
          PendingFailureCategory::kTimeout, PendingFailureCategory::kInvariant,
          PendingFailureCategory::kUnknown}) {
        unchanged.failure = PendingFailureIdentity{
            PendingFailureOperation::kReconstructPrevious,
            PendingFailureDestination::kRuntimePresentation, category,
            "classified"};
        QCOMPARE(ParsePendingConfigurationTransaction(
                     SerializePendingConfigurationTransaction(unchanged)),
                 unchanged);
    }
}

void ApplicationServiceTest::PendingTransactionAcceptsBoundaries() {
    const auto empty = MakePendingTransactionPayload({});
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(empty.sha256.data()),
                        static_cast<qsizetype>(empty.sha256.size()))
                 .toHex(),
             QByteArray("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934"
                        "ca495991b7852b855"));
    const auto abc = MakePendingTransactionPayload("abc");
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(abc.sha256.data()),
                        static_cast<qsizetype>(abc.sha256.size()))
                 .toHex(),
             QByteArray("ba7816bf8f01cfea414140de5dae2223b00361a396177a9c"
                        "b410ff61f20015ad"));

    PendingConfigurationTransactionRecord canonical;
    for (std::size_t index = 0U; index < canonical.transaction_id.size();
         ++index)
        canonical.transaction_id[index] = static_cast<std::uint8_t>(index);
    canonical.desired_configuration = empty;
    const auto canonical_bytes =
        SerializePendingConfigurationTransaction(canonical);
    QCOMPARE(canonical_bytes.size(), 193U);
    QCOMPARE(
        QCryptographicHash::hash(QByteArray::fromStdString(canonical_bytes),
                                 QCryptographicHash::Sha256)
            .toHex(),
        QByteArray("1286aebc6231087894d7e27d6cb859eda614bd81028b04b6"
                   "0a9c3ddc0b0e85f1"));

    PendingConfigurationTransactionRecord record;
    record.transaction_id.fill(0xffU);
    record.desired_configuration =
        MakePendingTransactionPayload(std::string(1024U * 1024U, '\0'));
    record.history_intent = PendingHistoryIntent::kReplace;
    record.desired_history =
        MakePendingTransactionPayload(std::string(1024U * 1024U, 'h'));
    record.previous_configuration = {
        true, MakePendingTransactionPayload(std::string(1024U * 1024U, 'c'))};
    record.previous_history = {
        true, MakePendingTransactionPayload(std::string(1024U * 1024U, 'p'))};
    record.failure = PendingFailureIdentity{
        PendingFailureOperation::kPersistPrevious,
        PendingFailureDestination::kPendingRecord,
        PendingFailureCategory::kResourceLimit, std::string(64U, 'a')};
    QCOMPARE(ParsePendingConfigurationTransaction(
                 SerializePendingConfigurationTransaction(record)),
             record);

    record.desired_configuration = MakePendingTransactionPayload({});
    record.desired_history = MakePendingTransactionPayload({});
    record.previous_configuration.payload = MakePendingTransactionPayload({});
    record.previous_history.payload = MakePendingTransactionPayload({});
    QCOMPARE(ParsePendingConfigurationTransaction(
                 SerializePendingConfigurationTransaction(record)),
             record);
}

void ApplicationServiceTest::PendingTransactionRejectsMalformedRecords() {
    const auto valid =
        SerializePendingConfigurationTransaction(CompletePendingRecord());
    for (std::size_t size = 0U; size < valid.size(); ++size) {
        QVERIFY_EXCEPTION_THROWN(
            ParsePendingConfigurationTransaction(valid.substr(0U, size)),
            std::runtime_error);
    }

    auto malformed = valid;
    malformed[0] = 'X';
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);

    malformed = valid;
    malformed[PendingFieldOffset(malformed, 1U) + 5U] = 2;
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);

    malformed = valid;
    std::fill_n(malformed.begin() + static_cast<std::ptrdiff_t>(
                                        PendingFieldOffset(malformed, 2U) + 5U),
                32U, '0');
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);

    malformed = valid;
    const auto identity = PendingFieldOffset(malformed, 2U);
    malformed[identity + 5U] = 'A';
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);
    malformed = valid;
    malformed[identity + 1U] = 0U;
    malformed[identity + 2U] = 0U;
    malformed[identity + 3U] = 0U;
    malformed[identity + 4U] = 31U;
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);

    malformed = valid;
    malformed[PendingFieldOffset(malformed, 3U)] = 1;
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);
    malformed = valid;
    malformed[PendingFieldOffset(malformed, 3U)] = 99;
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);

    for (const auto field : {4U, 6U, 13U, 14U, 15U}) {
        malformed = valid;
        malformed[PendingFieldOffset(malformed, field) + 5U] =
            static_cast<char>(0xffU);
        QVERIFY_EXCEPTION_THROWN(
            ParsePendingConfigurationTransaction(malformed),
            std::runtime_error);
    }

    for (const auto field : {8U, 10U, 12U}) {
        malformed = valid;
        malformed[PendingFieldOffset(malformed, field) + 5U] = 2;
        QVERIFY_EXCEPTION_THROWN(
            ParsePendingConfigurationTransaction(malformed),
            std::runtime_error);
    }

    malformed = valid;
    const auto payload = PendingFieldOffset(malformed, 5U) + 5U;
    malformed[payload + 7U] ^= 1;
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);
    malformed = valid;
    malformed[payload + 8U] ^= 1;
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);

    malformed = valid;
    malformed.append("trailing");
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(malformed),
                             std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(ParsePendingConfigurationTransaction(
                                 std::string(4U * 1024U * 1024U + 4097U, 'x')),
                             std::runtime_error);
}

void ApplicationServiceTest::PendingTransactionRejectsInvalidValues() {
    auto record = CompletePendingRecord();
    record.version = static_cast<PendingTransactionVersion>(0xffU);
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.transaction_id.fill(0U);
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.phase = static_cast<PendingTransactionPhase>(0xffU);
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.desired_recovery_attempt =
        static_cast<DesiredRecoveryAttempt>(0xffU);
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.history_intent = PendingHistoryIntent::kUnchanged;
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.desired_history.reset();
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.history_intent = PendingHistoryIntent::kUnchanged;
    record.desired_history.reset();
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.history_intent = PendingHistoryIntent::kUnchanged;
    record.desired_history.reset();
    record.previous_history.existed = false;
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.previous_configuration.existed = false;
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);

    for (const auto phase :
         {PendingTransactionPhase::kDesiredPersistenceFailed,
          PendingTransactionPhase::kDesiredRuntimeFailed,
          PendingTransactionPhase::kPreviousPersistenceBlocked,
          PendingTransactionPhase::kQuarantined}) {
        record = CompletePendingRecord();
        record.phase = phase;
        record.failure.reset();
        QVERIFY_EXCEPTION_THROWN(
            SerializePendingConfigurationTransaction(record),
            std::runtime_error);
    }

    record = CompletePendingRecord();
    ++record.desired_configuration.size;
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.desired_configuration.sha256[0] ^= 1U;
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(
        MakePendingTransactionPayload(std::string(1024U * 1024U + 1U, 'x')),
        std::runtime_error);

    for (const auto identifier :
         {"", "contains space", "../secret", "/absolute",
          "https://example.test", "message:detail", "line\nbreak"}) {
        record = CompletePendingRecord();
        record.failure->identifier = identifier;
        QVERIFY_EXCEPTION_THROWN(
            SerializePendingConfigurationTransaction(record),
            std::runtime_error);
    }
    record = CompletePendingRecord();
    record.failure->identifier.assign(65U, 'x');
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
    record = CompletePendingRecord();
    record.failure->category = static_cast<PendingFailureCategory>(0xffU);
    QVERIFY_EXCEPTION_THROWN(SerializePendingConfigurationTransaction(record),
                             std::runtime_error);
}

void ApplicationServiceTest::
    PreparesConfigurationOnlyWithoutChangingDestinations() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    const auto history_path = root / "history-v1";

    CoreConfiguration previous;
    previous.index_directory = "previous-indexes";
    SaveConfiguration(configuration_path.string(), previous);
    QVERIFY(std::filesystem::create_directory(history_path));
    test::WriteBinaryFile(history_path / "marker",
                          "history must remain unread");
    const auto previous_bytes = ReadFile(configuration_path);

    ConfigurationTransactionPreparationInput input;
    input.configuration_path = configuration_path;
    input.history_path = history_path;
    input.desired_configuration.index_directory = "desired-indexes";
    input.history_intent = PendingHistoryIntent::kUnchanged;
    bool observed_history_checkpoint = false;
    ConfigurationTransactionPreparationDependencies dependencies;
    dependencies.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        for (std::size_t index = 0U; index < identity.size(); ++index)
            identity[index] = static_cast<std::uint8_t>(index + 1U);
        return std::optional{identity};
    };
    dependencies.checkpoint = [&observed_history_checkpoint](
                                  PreparationCheckpoint checkpoint,
                                  const std::filesystem::path&) {
        if (checkpoint == PreparationCheckpoint::kAfterPreviousHistoryRead ||
            checkpoint == PreparationCheckpoint::kAfterDesiredHistoryStaged)
            observed_history_checkpoint = true;
    };

    auto result =
        PrepareConfigurationTransaction(input, std::move(dependencies));
    QVERIFY(result);
    QVERIFY(!result.error.has_value());
    QVERIFY(!observed_history_checkpoint);
    QCOMPARE(ReadFile(configuration_path), previous_bytes);
    QCOMPARE(ReadFile(history_path / "marker"),
             std::string("history must remain unread"));

    const auto& prepared = *result.prepared;
    const auto& record = prepared.record();
    QCOMPARE(record.phase, PendingTransactionPhase::kPrepared);
    QCOMPARE(record.desired_recovery_attempt,
             DesiredRecoveryAttempt::kNotAttempted);
    QCOMPARE(record.history_intent, PendingHistoryIntent::kUnchanged);
    QVERIFY(!record.desired_history.has_value());
    QVERIFY(!record.previous_history.existed);
    QVERIFY(!record.previous_history.payload.has_value());
    QVERIFY(record.previous_configuration.existed);
    QCOMPARE(record.previous_configuration.payload->bytes, previous_bytes);
    QCOMPARE(ParsePendingConfigurationTransaction(prepared.serialized_record()),
             record);
    QCOMPARE(ReadFile(prepared.staged_record_path()),
             prepared.serialized_record());
    QCOMPARE(QCryptographicHash::hash(
                 QByteArray::fromStdString(record.desired_configuration.bytes),
                 QCryptographicHash::Sha256),
             QByteArray(reinterpret_cast<const char*>(
                            record.desired_configuration.sha256.data()),
                        static_cast<qsizetype>(
                            record.desired_configuration.sha256.size())));
    QCOMPARE(
        QCryptographicHash::hash(QByteArray::fromStdString(previous_bytes),
                                 QCryptographicHash::Sha256),
        QByteArray(reinterpret_cast<const char*>(
                       record.previous_configuration.payload->sha256.data()),
                   static_cast<qsizetype>(
                       record.previous_configuration.payload->sha256.size())));
}

void ApplicationServiceTest::PreparesExactHistoryReplacementStates() {
    for (const auto& previous_history :
         {std::optional<std::string>{}, std::optional<std::string>{""},
          std::optional<std::string>{"goldendict-history-v1\n4 old\n"}}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto configuration_path = root / "core.conf";
        const auto history_path = root / "history-v1";
        SaveConfiguration(configuration_path.string(), {});
        if (previous_history)
            test::WriteBinaryFile(history_path, *previous_history);
        const auto configuration_before = ReadFile(configuration_path);

        ConfigurationTransactionPreparationInput input;
        input.configuration_path = configuration_path;
        input.history_path = history_path;
        input.history_intent = PendingHistoryIntent::kReplace;
        input.desired_history = {{7U, "caf\xC3\xA9"}, {9U, "dictionary"}};
        ConfigurationTransactionPreparationDependencies dependencies;
        dependencies.generate_transaction_id = [] {
            std::array<std::uint8_t, 16U> identity{};
            identity.fill(0x5aU);
            return std::optional{identity};
        };

        auto result = PrepareConfigurationTransaction(input, dependencies);
        QVERIFY(result);
        const auto& record = result.prepared->record();
        QVERIFY(record.desired_history.has_value());
        QCOMPARE(record.desired_history->bytes,
                 std::string("goldendict-history-v1\n7 caf\xC3\xA9\n"
                             "9 dictionary\n"));
        QCOMPARE(record.previous_history.existed, previous_history.has_value());
        QCOMPARE(record.previous_history.payload.has_value(),
                 previous_history.has_value());
        if (previous_history)
            QCOMPARE(record.previous_history.payload->bytes, *previous_history);
        QCOMPARE(ReadFile(configuration_path), configuration_before);
        QCOMPARE(std::filesystem::exists(history_path),
                 previous_history.has_value());
        if (previous_history)
            QCOMPARE(ReadFile(history_path), *previous_history);
    }
}

void ApplicationServiceTest::PreparationFailuresAreAbortableAndBounded() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    const auto history_path = root / "history-v1";
    SaveConfiguration(configuration_path.string(), {});
    test::WriteBinaryFile(history_path, "");
    const auto configuration_before = ReadFile(configuration_path);
    const auto history_before = ReadFile(history_path);

    ConfigurationTransactionPreparationInput input;
    input.configuration_path = configuration_path;
    input.history_path = history_path;
    input.history_intent = PendingHistoryIntent::kReplace;
    input.desired_history = {{1U, "word"}};
    ConfigurationTransactionPreparationDependencies dependencies;
    dependencies.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x33U);
        return std::optional{identity};
    };
    dependencies.checkpoint = [](PreparationCheckpoint checkpoint,
                                 const std::filesystem::path& path) {
        if (checkpoint == PreparationCheckpoint::kAfterPendingRecordStaged)
            test::WriteBinaryFile(path, "corrupt");
    };
    auto mismatch = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!mismatch);
    QCOMPARE(mismatch.error->code,
             ConfigurationPreparationErrorCode::kVerification);
    QVERIFY(!std::filesystem::exists(
        root / ".goldendict-transaction-33333333333333333333333333333333"));

    dependencies.checkpoint = {};
    const auto collision =
        root / ".goldendict-transaction-33333333333333333333333333333333";
    QVERIFY(std::filesystem::create_directory(collision));
    test::WriteBinaryFile(collision / "owned-by-someone-else", "keep");
    auto collided = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!collided);
    QCOMPARE(collided.error->code,
             ConfigurationPreparationErrorCode::kStagingCollision);
    QCOMPARE(ReadFile(collision / "owned-by-someone-else"),
             std::string("keep"));

    dependencies.generate_transaction_id = [] {
        return std::optional<std::array<std::uint8_t, 16U>>{};
    };
    auto no_identity = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!no_identity);
    QCOMPARE(no_identity.error->code,
             ConfigurationPreparationErrorCode::kIdentityGeneration);

    auto missing_input = input;
    missing_input.configuration_path = root / "missing.conf";
    auto missing = PrepareConfigurationTransaction(missing_input, dependencies);
    QCOMPARE(missing.error->code,
             ConfigurationPreparationErrorCode::kIdentityGeneration);
    dependencies.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity[0] = 1U;
        return std::optional{identity};
    };
    auto missing_after_identity =
        PrepareConfigurationTransaction(missing_input, dependencies);
    QVERIFY(!missing_after_identity);
    QCOMPARE(missing_after_identity.error->code,
             ConfigurationPreparationErrorCode::kConfigurationRead);

    auto invalid = input;
    invalid.desired_configuration.index_directory =
        std::string("bad\0path", 8U);
    auto invalid_result =
        PrepareConfigurationTransaction(invalid, dependencies);
    QVERIFY(!invalid_result);
    QCOMPARE(invalid_result.error->code,
             ConfigurationPreparationErrorCode::kInvalidInput);
    QCOMPARE(ReadFile(configuration_path), configuration_before);
    QCOMPARE(ReadFile(history_path), history_before);
}

void ApplicationServiceTest::PreparationFilesystemFailuresAreDeterministic() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    const auto history_path = root / "history-v1";
    SaveConfiguration(configuration_path.string(), {});
    test::WriteBinaryFile(history_path, "");
    const auto configuration_before = ReadFile(configuration_path);
    const auto history_before = ReadFile(history_path);

    ConfigurationTransactionPreparationInput input;
    input.configuration_path = configuration_path;
    input.history_path = history_path;
    input.history_intent = PendingHistoryIntent::kReplace;
    input.desired_history = {{1U, "word"}};
    const auto staging_directory =
        root / ".goldendict-transaction-66666666666666666666666666666666";

    bool identity_generated = false;
    ConfigurationTransactionPreparationDependencies dependencies;
    dependencies.generate_transaction_id = [&identity_generated] {
        identity_generated = true;
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x66U);
        return std::optional{identity};
    };
    const auto fail = [](PreparationFilesystemOperation failed_operation,
                         const std::filesystem::path& failed_path = {}) {
        return [failed_operation, failed_path](
                   PreparationFilesystemOperation operation,
                   const std::filesystem::path& path)
                   -> std::optional<std::error_code> {
            if (operation == failed_operation &&
                (failed_path.empty() || path.filename() == failed_path)) {
                return std::make_error_code(std::errc::io_error);
            }
            return std::nullopt;
        };
    };

    dependencies.filesystem_failure =
        fail(PreparationFilesystemOperation::kReadConfiguration);
    auto configuration_read =
        PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(identity_generated);
    QVERIFY(!configuration_read);
    QCOMPARE(configuration_read.error->code,
             ConfigurationPreparationErrorCode::kConfigurationRead);

    dependencies.filesystem_failure =
        fail(PreparationFilesystemOperation::kInspectHistory);
    auto history_inspect = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!history_inspect);
    QCOMPARE(history_inspect.error->code,
             ConfigurationPreparationErrorCode::kHistoryRead);

    dependencies.filesystem_failure =
        fail(PreparationFilesystemOperation::kReadHistory);
    auto history_read = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!history_read);
    QCOMPARE(history_read.error->code,
             ConfigurationPreparationErrorCode::kHistoryRead);

    dependencies.filesystem_failure =
        fail(PreparationFilesystemOperation::kCreateStagingDirectory);
    auto staging_create = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!staging_create);
    QCOMPARE(staging_create.error->code,
             ConfigurationPreparationErrorCode::kStagingIo);
    QVERIFY(!std::filesystem::exists(staging_directory));

    dependencies.filesystem_failure =
        fail(PreparationFilesystemOperation::kWriteStagingArtifact);
    auto staging_write = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!staging_write);
    QCOMPARE(staging_write.error->code,
             ConfigurationPreparationErrorCode::kStagingIo);
    QVERIFY(!std::filesystem::exists(staging_directory));

    dependencies.filesystem_failure =
        fail(PreparationFilesystemOperation::kReadStagingArtifact,
             "desired-configuration");
    auto staging_read = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!staging_read);
    QCOMPARE(staging_read.error->code,
             ConfigurationPreparationErrorCode::kStagingIo);
    QVERIFY(!std::filesystem::exists(staging_directory));

    dependencies.filesystem_failure =
        [](PreparationFilesystemOperation operation,
           const std::filesystem::path& path)
        -> std::optional<std::error_code> {
        if ((operation ==
                 PreparationFilesystemOperation::kRemoveStagingArtifact &&
             path.filename() == "previous-configuration") ||
            operation ==
                PreparationFilesystemOperation::kRemoveStagingDirectory) {
            return std::make_error_code(std::errc::io_error);
        }
        return std::nullopt;
    };
    auto cleanup = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!cleanup);
    QCOMPARE(cleanup.error->code, ConfigurationPreparationErrorCode::kCleanup);
    QCOMPARE(cleanup.error->residual_staging_directory, staging_directory);
    QVERIFY(std::filesystem::exists(staging_directory));
    std::filesystem::remove_all(staging_directory);

    dependencies.filesystem_failure = {};
    dependencies.generate_transaction_id = [] {
        return std::optional{std::array<std::uint8_t, 16U>{}};
    };
    auto zero_identity = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!zero_identity);
    QCOMPARE(zero_identity.error->code,
             ConfigurationPreparationErrorCode::kIdentityGeneration);

    dependencies.generate_transaction_id =
        []() -> std::optional<std::array<std::uint8_t, 16U>> {
        throw std::runtime_error("injected identity failure");
    };
    auto throwing_identity =
        PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(!throwing_identity);
    QCOMPARE(throwing_identity.error->code,
             ConfigurationPreparationErrorCode::kIdentityGeneration);

    QCOMPARE(ReadFile(configuration_path), configuration_before);
    QCOMPARE(ReadFile(history_path), history_before);
}

void ApplicationServiceTest::PreparedTransactionOwnsStagingUntilReleased() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    SaveConfiguration(configuration_path.string(), {});
    ConfigurationTransactionPreparationInput input;
    input.configuration_path = configuration_path;
    ConfigurationTransactionPreparationDependencies dependencies;
    dependencies.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x44U);
        return std::optional{identity};
    };

    std::filesystem::path staging;
    {
        auto result = PrepareConfigurationTransaction(input, dependencies);
        QVERIFY(result);
        staging = result.prepared->staged_record_path().parent_path();
        QVERIFY(std::filesystem::exists(staging));
        auto moved = std::move(*result.prepared);
        QVERIFY(!moved.Abort().has_value());
        QVERIFY(!std::filesystem::exists(staging));
    }

    dependencies.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x45U);
        return std::optional{identity};
    };
    auto released = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(released);
    staging = released.prepared->staged_record_path().parent_path();
    released.prepared->ReleaseForDecision();
    released.prepared.reset();
    QVERIFY(std::filesystem::exists(staging));
    std::filesystem::remove_all(staging);

    dependencies.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x46U);
        return std::optional{identity};
    };
    dependencies.filesystem_failure =
        [](PreparationFilesystemOperation operation,
           const std::filesystem::path&) -> std::optional<std::error_code> {
        if (operation ==
            PreparationFilesystemOperation::kRemoveStagingDirectory) {
            return std::make_error_code(std::errc::io_error);
        }
        return std::nullopt;
    };
    auto cleanup_failure = PrepareConfigurationTransaction(input, dependencies);
    QVERIFY(cleanup_failure);
    staging = cleanup_failure.prepared->staged_record_path().parent_path();
    const auto cleanup_error = cleanup_failure.prepared->Abort();
    QVERIFY(cleanup_error.has_value());
    QCOMPARE(cleanup_error->code, ConfigurationPreparationErrorCode::kCleanup);
    QCOMPARE(cleanup_error->residual_staging_directory, staging);
    cleanup_failure.prepared->ReleaseForDecision();
    cleanup_failure.prepared.reset();
    std::filesystem::remove_all(staging);
}

void ApplicationServiceTest::PersistsConfigurationOnlyThroughDurableDecision() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    const auto history_path = root / "history-v1";
    CoreConfiguration previous;
    previous.index_directory = "previous";
    SaveConfiguration(configuration_path.string(), previous);
    QVERIFY(std::filesystem::create_directory(history_path));
    test::WriteBinaryFile(history_path / "marker", "untouched");

    ConfigurationTransactionPreparationInput input;
    input.configuration_path = configuration_path;
    input.history_path = history_path;
    input.desired_configuration.index_directory = "desired";
    ConfigurationTransactionPreparationDependencies preparation;
    preparation.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x71U);
        return std::optional{identity};
    };
    auto prepared = PrepareConfigurationTransaction(input, preparation);
    QVERIFY(prepared);
    const auto desired_bytes =
        prepared.prepared->record().desired_configuration.bytes;
    const auto staging = prepared.prepared->staged_record_path().parent_path();
    std::vector<PendingTransactionPhase> published_phases;
    ConfigurationPersistenceDependencies persistence;
    persistence.checkpoint = [&](ConfigurationPersistenceCheckpoint checkpoint,
                                 const std::filesystem::path& path) {
        if (checkpoint ==
                ConfigurationPersistenceCheckpoint::kAfterDecisionPublished ||
            checkpoint ==
                ConfigurationPersistenceCheckpoint::kAfterRecordReplaced) {
            published_phases.push_back(
                ParsePendingConfigurationTransaction(ReadFile(path)).phase);
        }
    };

    const auto result =
        PersistDesiredConfiguration(std::move(*prepared.prepared), persistence);
    QCOMPARE(result.outcome,
             ConfigurationPersistenceOutcome::kDesiredPersistenceApplied);
    QVERIFY(result.decision_path_published);
    QCOMPARE(
        result.namespace_published_phase,
        std::optional{PendingTransactionPhase::kDesiredPersistenceApplied});
    QCOMPARE(
        result.confirmed_durable_phase,
        std::optional{PendingTransactionPhase::kDesiredPersistenceApplied});
    QCOMPARE(published_phases,
             (std::vector<PendingTransactionPhase>{
                 PendingTransactionPhase::kDesiredCommit,
                 PendingTransactionPhase::kDesiredPersistenceApplying,
                 PendingTransactionPhase::kDesiredPersistenceApplied}));
    QCOMPARE(ReadFile(configuration_path), desired_bytes);
    QCOMPARE(ReadFile(history_path / "marker"), std::string("untouched"));
    QCOMPARE(
        ParsePendingConfigurationTransaction(
            ReadFile(PendingConfigurationTransactionPath(configuration_path)))
            .phase,
        PendingTransactionPhase::kDesiredPersistenceApplied);
    QVERIFY(std::filesystem::exists(staging));
    std::filesystem::remove_all(staging);
}

void ApplicationServiceTest::AdvancesAndFinalizesDesiredRuntimeDurably() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    const auto history_path = root / "history-v1";
    auto record = CompletePendingRecord();
    record.failure.reset();
    test::WriteBinaryFile(configuration_path,
                          record.desired_configuration.bytes);
    test::WriteBinaryFile(history_path, record.desired_history->bytes);
    const auto pending_path =
        PendingConfigurationTransactionPath(configuration_path);
    test::WriteBinaryFile(pending_path,
                          SerializePendingConfigurationTransaction(record));
    const ConfigurationRecoveryRequest request{configuration_path,
                                               record.transaction_id};

    const auto applying = BeginDesiredRuntimePublication(request);
    QCOMPARE(applying.outcome, RuntimeTransitionOutcome::kApplied);
    QCOMPARE(applying.confirmed_durable_phase,
             std::optional{PendingTransactionPhase::kDesiredRuntimeApplying});
    QCOMPARE(ParsePendingConfigurationTransaction(ReadFile(pending_path)).phase,
             PendingTransactionPhase::kDesiredRuntimeApplying);

    const auto failure = RecordDesiredRuntimeFailure(
        request, PendingFailureDestination::kRuntimeTransport,
        PendingFailureCategory::kUnavailable, "network_cleanup_failed");
    QCOMPARE(failure.outcome, RuntimeTransitionOutcome::kApplied);
    const auto failed_record =
        ParsePendingConfigurationTransaction(ReadFile(pending_path));
    QCOMPARE(failed_record.phase,
             PendingTransactionPhase::kDesiredRuntimeFailed);
    QCOMPARE(failed_record.failure->destination,
             PendingFailureDestination::kRuntimeTransport);

    record.phase = PendingTransactionPhase::kDesiredRuntimeApplying;
    record.failure.reset();
    test::WriteBinaryFile(pending_path,
                          SerializePendingConfigurationTransaction(record));
    test::WriteBinaryFile(history_path, "wrong history");
    const auto mismatch =
        FinishDesiredConfigurationTransaction(request, history_path);
    QCOMPARE(mismatch.outcome,
             RuntimeTransitionOutcome::kRejectedBeforePublication);
    QVERIFY(std::filesystem::exists(pending_path));
    test::WriteBinaryFile(history_path, record.desired_history->bytes);
    const auto finished =
        FinishDesiredConfigurationTransaction(request, history_path);
    QCOMPARE(finished.outcome, RuntimeTransitionOutcome::kApplied);
    QVERIFY(finished.pending_record_removed);
    QVERIFY(finished.removal_confirmed_durable);
    QVERIFY(!std::filesystem::exists(pending_path));

    test::WriteBinaryFile(pending_path,
                          SerializePendingConfigurationTransaction(record));
    ConfigurationPersistenceDependencies injected;
    injected.filesystem_failure =
        [](ConfigurationPersistenceOperation operation,
           const std::filesystem::path&) -> std::optional<std::error_code> {
        if (operation == ConfigurationPersistenceOperation::kSyncDirectory)
            return std::make_error_code(std::errc::io_error);
        return std::nullopt;
    };
    const auto post_removal =
        FinishDesiredConfigurationTransaction(request, history_path, injected);
    QCOMPARE(post_removal.outcome,
             RuntimeTransitionOutcome::kPostPublicationFailure);
    QVERIFY(post_removal.pending_record_removed);
    QVERIFY(!post_removal.removal_confirmed_durable);
    QVERIFY(!std::filesystem::exists(pending_path));
}

void ApplicationServiceTest::PersistsConfigurationAndReplacementHistory() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    const auto history_path = root / "history-v1";
    SaveConfiguration(configuration_path.string(), {});
    test::WriteBinaryFile(history_path, "");

    ConfigurationTransactionPreparationInput input;
    input.configuration_path = configuration_path;
    input.history_path = history_path;
    input.desired_configuration.index_directory = "replacement";
    input.history_intent = PendingHistoryIntent::kReplace;
    input.desired_history = {{7U, "first"}, {9U, "second"}};
    ConfigurationTransactionPreparationDependencies preparation;
    preparation.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x72U);
        return std::optional{identity};
    };
    auto prepared = PrepareConfigurationTransaction(input, preparation);
    QVERIFY(prepared);
    const auto expected_configuration =
        prepared.prepared->record().desired_configuration.bytes;
    const auto expected_history =
        prepared.prepared->record().desired_history->bytes;
    const auto staging = prepared.prepared->staged_record_path().parent_path();

    const auto result =
        PersistDesiredConfiguration(std::move(*prepared.prepared));
    QCOMPARE(result.outcome,
             ConfigurationPersistenceOutcome::kDesiredPersistenceApplied);
    QCOMPARE(ReadFile(configuration_path), expected_configuration);
    QCOMPARE(ReadFile(history_path), expected_history);
    const auto pending = ParsePendingConfigurationTransaction(
        ReadFile(PendingConfigurationTransactionPath(configuration_path)));
    QCOMPARE(pending.phase,
             PendingTransactionPhase::kDesiredPersistenceApplied);
    QCOMPARE(pending.desired_configuration.bytes, expected_configuration);
    QCOMPARE(pending.desired_history->bytes, expected_history);
    std::filesystem::remove_all(staging);
}

void ApplicationServiceTest::
    PersistenceFailuresBeforePublicationRemainAbortable() {
    for (const auto failed_operation :
         {ConfigurationPersistenceOperation::kSyncTemporary,
          ConfigurationPersistenceOperation::kPublishDecision}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto configuration_path = root / "core.conf";
        SaveConfiguration(configuration_path.string(), {});
        const auto before = ReadFile(configuration_path);

        ConfigurationTransactionPreparationInput input;
        input.configuration_path = configuration_path;
        input.desired_configuration.index_directory = "desired";
        ConfigurationTransactionPreparationDependencies preparation;
        preparation.generate_transaction_id = [] {
            std::array<std::uint8_t, 16U> identity{};
            identity.fill(0x73U);
            return std::optional{identity};
        };
        auto prepared = PrepareConfigurationTransaction(input, preparation);
        QVERIFY(prepared);
        const auto staging =
            prepared.prepared->staged_record_path().parent_path();
        ConfigurationPersistenceDependencies persistence;
        persistence.filesystem_failure =
            [failed_operation](ConfigurationPersistenceOperation operation,
                               const std::filesystem::path&)
            -> std::optional<std::error_code> {
            if (operation == failed_operation)
                return std::make_error_code(std::errc::io_error);
            return std::nullopt;
        };

        auto result = PersistDesiredConfiguration(std::move(*prepared.prepared),
                                                  persistence);
        QCOMPARE(result.outcome,
                 ConfigurationPersistenceOutcome::kPreDecisionFailure);
        QVERIFY(!result.decision_path_published);
        QVERIFY(!result.namespace_published_phase.has_value());
        QVERIFY(!result.confirmed_durable_phase.has_value());
        QVERIFY(result.abortable_prepared.has_value());
        QCOMPARE(ReadFile(configuration_path), before);
        QVERIFY(!std::filesystem::exists(
            PendingConfigurationTransactionPath(configuration_path)));
        QVERIFY(std::filesystem::exists(staging));
        QVERIFY(!result.abortable_prepared->Abort().has_value());
        QVERIFY(!std::filesystem::exists(staging));
    }

    QTemporaryDir collision_directory;
    QVERIFY(collision_directory.isValid());
    const auto collision_root = TemporaryPath(collision_directory);
    const auto configuration_path = collision_root / "core.conf";
    SaveConfiguration(configuration_path.string(), {});
    ConfigurationTransactionPreparationInput input;
    input.configuration_path = configuration_path;
    ConfigurationTransactionPreparationDependencies preparation;
    preparation.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x73U);
        return std::optional{identity};
    };
    auto prepared = PrepareConfigurationTransaction(input, preparation);
    QVERIFY(prepared);
    const auto pending_path =
        PendingConfigurationTransactionPath(configuration_path);
    test::WriteBinaryFile(pending_path, "unrelated-pending-artifact");
    auto collision = PersistDesiredConfiguration(std::move(*prepared.prepared));
    QCOMPARE(collision.outcome,
             ConfigurationPersistenceOutcome::kPreDecisionFailure);
    QCOMPARE(ReadFile(pending_path), std::string("unrelated-pending-artifact"));
    QVERIFY(collision.abortable_prepared.has_value());
    QVERIFY(!collision.abortable_prepared->Abort().has_value());

    QVERIFY(std::filesystem::remove(pending_path));
    const auto unrelated_target = collision_root / "unrelated-target";
    test::WriteBinaryFile(unrelated_target, "do-not-overwrite");
    std::error_code symlink_error;
    std::filesystem::create_symlink(unrelated_target, pending_path,
                                    symlink_error);
    if (IsSymlinkCapabilityUnavailable(symlink_error))
        QSKIP("Windows symlink capability is unavailable");
    QVERIFY2(!symlink_error, symlink_error.message().c_str());
    preparation.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x79U);
        return std::optional{identity};
    };
    auto symlink_prepared = PrepareConfigurationTransaction(input, preparation);
    QVERIFY(symlink_prepared);
    auto symlink_result =
        PersistDesiredConfiguration(std::move(*symlink_prepared.prepared));
    QCOMPARE(symlink_result.outcome,
             ConfigurationPersistenceOutcome::kPreDecisionFailure);
    QVERIFY(std::filesystem::is_symlink(
        std::filesystem::symlink_status(pending_path)));
    QCOMPARE(ReadFile(unrelated_target), std::string("do-not-overwrite"));
    QVERIFY(symlink_result.abortable_prepared.has_value());
    QVERIFY(!symlink_result.abortable_prepared->Abort().has_value());
}

void ApplicationServiceTest::
    PublishedDecisionDirectorySyncFailureRetainsRecoveryState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    SaveConfiguration(configuration_path.string(), {});
    const auto configuration_before = ReadFile(configuration_path);

    ConfigurationTransactionPreparationInput input;
    input.configuration_path = configuration_path;
    input.desired_configuration.index_directory = "desired";
    ConfigurationTransactionPreparationDependencies preparation;
    preparation.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x74U);
        return std::optional{identity};
    };
    auto prepared = PrepareConfigurationTransaction(input, preparation);
    QVERIFY(prepared);
    const auto staging = prepared.prepared->staged_record_path().parent_path();
    int directory_syncs = 0;
    ConfigurationPersistenceDependencies persistence;
    persistence.filesystem_failure =
        [&directory_syncs](
            ConfigurationPersistenceOperation operation,
            const std::filesystem::path&) -> std::optional<std::error_code> {
        if (operation == ConfigurationPersistenceOperation::kSyncDirectory &&
            directory_syncs++ == 0) {
            return std::make_error_code(std::errc::io_error);
        }
        return std::nullopt;
    };

    const auto result =
        PersistDesiredConfiguration(std::move(*prepared.prepared), persistence);
    QCOMPARE(result.outcome,
             ConfigurationPersistenceOutcome::kPostDecisionFailure);
    QVERIFY(result.decision_path_published);
    QVERIFY(!result.abortable_prepared.has_value());
    QVERIFY(result.failure_evidence_namespace_published);
    QVERIFY(result.failure_evidence_durable);
    QCOMPARE(result.confirmed_durable_phase,
             std::optional{PendingTransactionPhase::kDesiredPersistenceFailed});
    QCOMPARE(ReadFile(configuration_path), configuration_before);
    QVERIFY(std::filesystem::exists(staging));
    const auto pending = ParsePendingConfigurationTransaction(
        ReadFile(PendingConfigurationTransactionPath(configuration_path)));
    QCOMPARE(pending.phase, PendingTransactionPhase::kDesiredPersistenceFailed);
    QCOMPARE(pending.failure->identifier, std::string("directory_sync_failed"));
    std::filesystem::remove_all(staging);

    QTemporaryDir secondary_directory;
    QVERIFY(secondary_directory.isValid());
    const auto secondary_root = TemporaryPath(secondary_directory);
    const auto secondary_configuration = secondary_root / "core.conf";
    SaveConfiguration(secondary_configuration.string(), {});
    input.configuration_path = secondary_configuration;
    preparation.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x77U);
        return std::optional{identity};
    };
    auto secondary_prepared =
        PrepareConfigurationTransaction(input, preparation);
    QVERIFY(secondary_prepared);
    const auto secondary_staging =
        secondary_prepared.prepared->staged_record_path().parent_path();
    persistence.filesystem_failure =
        [](ConfigurationPersistenceOperation operation,
           const std::filesystem::path&) -> std::optional<std::error_code> {
        if (operation == ConfigurationPersistenceOperation::kSyncDirectory)
            return std::make_error_code(std::errc::io_error);
        return std::nullopt;
    };
    const auto secondary = PersistDesiredConfiguration(
        std::move(*secondary_prepared.prepared), persistence);
    QCOMPARE(secondary.outcome,
             ConfigurationPersistenceOutcome::kPostDecisionFailure);
    QVERIFY(secondary.decision_path_published);
    QVERIFY(secondary.failure_evidence_namespace_published);
    QVERIFY(!secondary.failure_evidence_durable);
    QVERIFY(!secondary.confirmed_durable_phase.has_value());
    QVERIFY(secondary.failure_evidence_error.has_value());
    QCOMPARE(secondary.failure_evidence_error->identity.identifier,
             std::string("directory_sync_failed"));
    QCOMPARE(ParsePendingConfigurationTransaction(
                 ReadFile(PendingConfigurationTransactionPath(
                     secondary_configuration)))
                 .phase,
             PendingTransactionPhase::kDesiredPersistenceFailed);
    QVERIFY(std::filesystem::exists(secondary_staging));
    std::filesystem::remove_all(secondary_staging);
}

void ApplicationServiceTest::
    PostDecisionPersistenceFailuresConvergeOnlyForward() {
    std::vector<ConfigurationPersistenceOperation> failed_operations = {
        ConfigurationPersistenceOperation::kInspectPath,
        ConfigurationPersistenceOperation::kCreateTemporary,
        ConfigurationPersistenceOperation::kWriteTemporary,
        ConfigurationPersistenceOperation::kFlushTemporary,
        ConfigurationPersistenceOperation::kSyncTemporary,
        ConfigurationPersistenceOperation::kReplaceDestination,
        ConfigurationPersistenceOperation::kSyncDirectory,
        ConfigurationPersistenceOperation::kReadVerification,
        ConfigurationPersistenceOperation::kVerifyPayload};
#ifndef _WIN32
    failed_operations.push_back(
        ConfigurationPersistenceOperation::kRemoveTemporary);
#endif
    for (const auto failed_operation : failed_operations) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto configuration_path = root / "core.conf";
        CoreConfiguration previous;
        previous.index_directory = "previous";
        SaveConfiguration(configuration_path.string(), previous);
        const auto previous_bytes = ReadFile(configuration_path);

        ConfigurationTransactionPreparationInput input;
        input.configuration_path = configuration_path;
        input.desired_configuration.index_directory = "desired";
        ConfigurationTransactionPreparationDependencies preparation;
        preparation.generate_transaction_id = [] {
            std::array<std::uint8_t, 16U> identity{};
            identity.fill(0x75U);
            return std::optional{identity};
        };
        auto prepared = PrepareConfigurationTransaction(input, preparation);
        QVERIFY(prepared);
        const auto desired_bytes =
            prepared.prepared->record().desired_configuration.bytes;
        const auto staging =
            prepared.prepared->staged_record_path().parent_path();
        int directory_syncs = 0;
        ConfigurationPersistenceDependencies persistence;
        persistence.filesystem_failure =
            [failed_operation, &directory_syncs, &configuration_path](
                ConfigurationPersistenceOperation operation,
                const std::filesystem::path& path)
            -> std::optional<std::error_code> {
            if (operation != failed_operation)
                return std::nullopt;
            bool selected = false;
            if (operation ==
                ConfigurationPersistenceOperation::kSyncDirectory) {
                selected = directory_syncs++ == 2;
            } else if (operation ==
                       ConfigurationPersistenceOperation::kRemoveTemporary) {
                selected = true;
            } else {
                selected = path == configuration_path ||
                           path.string().find("-configuration.tmp") !=
                               std::string::npos;
            }
            return selected ? std::optional{std::make_error_code(
                                  std::errc::io_error)}
                            : std::nullopt;
        };

        auto result = PersistDesiredConfiguration(std::move(*prepared.prepared),
                                                  persistence);
        QCOMPARE(result.outcome,
                 ConfigurationPersistenceOutcome::kPostDecisionFailure);
        QVERIFY(result.decision_path_published);
        QVERIFY(!result.abortable_prepared.has_value());
        QVERIFY(result.failure_evidence_durable);
        QCOMPARE(
            result.confirmed_durable_phase,
            std::optional{PendingTransactionPhase::kDesiredPersistenceFailed});
        const auto persisted = ReadFile(configuration_path);
        QVERIFY(persisted == previous_bytes || persisted == desired_bytes);
        const auto pending = ParsePendingConfigurationTransaction(
            ReadFile(PendingConfigurationTransactionPath(configuration_path)));
        QCOMPARE(pending.phase,
                 PendingTransactionPhase::kDesiredPersistenceFailed);
        QCOMPARE(pending.failure, std::optional{result.error->identity});
        QVERIFY(std::filesystem::exists(staging));
        std::filesystem::remove_all(staging);
    }
}

void ApplicationServiceTest::
    FailureEvidenceNeverOverwritesCorruptPendingRecord() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    SaveConfiguration(configuration_path.string(), {});

    ConfigurationTransactionPreparationInput input;
    input.configuration_path = configuration_path;
    input.desired_configuration.index_directory = "desired";
    ConfigurationTransactionPreparationDependencies preparation;
    preparation.generate_transaction_id = [] {
        std::array<std::uint8_t, 16U> identity{};
        identity.fill(0x76U);
        return std::optional{identity};
    };
    auto prepared = PrepareConfigurationTransaction(input, preparation);
    QVERIFY(prepared);
    const auto staging = prepared.prepared->staged_record_path().parent_path();
    const auto pending_path =
        PendingConfigurationTransactionPath(configuration_path);
    ConfigurationPersistenceDependencies persistence;
    persistence.checkpoint = [](ConfigurationPersistenceCheckpoint checkpoint,
                                const std::filesystem::path& path) {
        if (checkpoint ==
            ConfigurationPersistenceCheckpoint::kAfterDecisionPublished) {
            test::WriteBinaryFile(path, "unrelated-corrupt-record");
            throw std::runtime_error("simulated crash boundary");
        }
    };

    const auto result =
        PersistDesiredConfiguration(std::move(*prepared.prepared), persistence);
    QCOMPARE(result.outcome,
             ConfigurationPersistenceOutcome::kPostDecisionFailure);
    QVERIFY(result.decision_path_published);
    QCOMPARE(result.namespace_published_phase,
             std::optional{PendingTransactionPhase::kDesiredCommit});
    QVERIFY(!result.confirmed_durable_phase.has_value());
    QVERIFY(!result.failure_evidence_namespace_published);
    QVERIFY(!result.failure_evidence_durable);
    QVERIFY(result.failure_evidence_error.has_value());
    QCOMPARE(result.failure_evidence_error->identity.identifier,
             std::string("pending_record_invalid"));
    QCOMPARE(ReadFile(pending_path), std::string("unrelated-corrupt-record"));
    QVERIFY(std::filesystem::exists(staging));
    std::filesystem::remove_all(staging);
}

void ApplicationServiceTest::
    PersistenceCrashCheckpointsReportTruthfulBoundaries() {
    for (const auto failed_checkpoint :
         {ConfigurationPersistenceCheckpoint::kAfterTemporarySynced,
          ConfigurationPersistenceCheckpoint::kAfterDecisionPublished,
          ConfigurationPersistenceCheckpoint::kAfterRecordReplaced,
          ConfigurationPersistenceCheckpoint::kAfterDirectorySynced,
          ConfigurationPersistenceCheckpoint::kAfterConfigurationReplaced,
          ConfigurationPersistenceCheckpoint::kAfterConfigurationVerified,
          ConfigurationPersistenceCheckpoint::kAfterHistoryReplaced,
          ConfigurationPersistenceCheckpoint::kAfterHistoryVerified}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto configuration_path = root / "core.conf";
        const auto history_path = root / "history-v1";
        SaveConfiguration(configuration_path.string(), {});
        test::WriteBinaryFile(history_path, "goldendict-history-v1\n1 old\n");

        ConfigurationTransactionPreparationInput input;
        input.configuration_path = configuration_path;
        input.history_path = history_path;
        input.desired_configuration.index_directory = "desired";
        input.history_intent = PendingHistoryIntent::kReplace;
        input.desired_history = {{2U, "new"}};
        ConfigurationTransactionPreparationDependencies preparation;
        preparation.generate_transaction_id = [] {
            std::array<std::uint8_t, 16U> identity{};
            identity.fill(0x78U);
            return std::optional{identity};
        };
        auto prepared = PrepareConfigurationTransaction(input, preparation);
        QVERIFY(prepared);
        const auto staging =
            prepared.prepared->staged_record_path().parent_path();
        bool injected = false;
        ConfigurationPersistenceDependencies persistence;
        persistence.checkpoint =
            [failed_checkpoint, &injected](
                ConfigurationPersistenceCheckpoint checkpoint,
                const std::filesystem::path&) {
                if (!injected && checkpoint == failed_checkpoint) {
                    injected = true;
                    throw std::runtime_error("simulated crash");
                }
            };

        auto result = PersistDesiredConfiguration(std::move(*prepared.prepared),
                                                  persistence);
        QVERIFY(injected);
        if (failed_checkpoint ==
            ConfigurationPersistenceCheckpoint::kAfterTemporarySynced) {
            QCOMPARE(result.outcome,
                     ConfigurationPersistenceOutcome::kPreDecisionFailure);
            QVERIFY(!result.decision_path_published);
            QVERIFY(result.abortable_prepared.has_value());
            QVERIFY(!result.abortable_prepared->Abort().has_value());
        } else {
            QCOMPARE(result.outcome,
                     ConfigurationPersistenceOutcome::kPostDecisionFailure);
            QVERIFY(result.decision_path_published);
            QVERIFY(!result.abortable_prepared.has_value());
            QVERIFY(result.failure_evidence_durable);
            QCOMPARE(result.confirmed_durable_phase,
                     std::optional{
                         PendingTransactionPhase::kDesiredPersistenceFailed});
            QVERIFY(std::filesystem::exists(staging));
            std::filesystem::remove_all(staging);
        }
    }
}

void ApplicationServiceTest::
    RestoresPreviousConfigurationWithoutInspectingUnchangedHistory() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    const auto history_path = root / "history-v1";
    const std::string previous("previous\0configuration", 22U);
    test::WriteBinaryFile(configuration_path, "partially desired");
    test::WriteBinaryFile(history_path, "must remain untouched");
    const auto record =
        FallbackRecord(0x81U, PendingHistoryIntent::kUnchanged, previous);
    const auto pending_path =
        PendingConfigurationTransactionPath(configuration_path);
    test::WriteBinaryFile(pending_path,
                          SerializePendingConfigurationTransaction(record));

    std::vector<PendingTransactionPhase> phases;
    int history_operations = 0;
    ConfigurationPersistenceDependencies dependencies;
    dependencies.filesystem_failure = [&history_path, &history_operations](
                                          ConfigurationPersistenceOperation,
                                          const std::filesystem::path& path)
        -> std::optional<std::error_code> {
        if (path == history_path)
            ++history_operations;
        return std::nullopt;
    };
    dependencies.checkpoint = [&phases, &pending_path](
                                  ConfigurationPersistenceCheckpoint checkpoint,
                                  const std::filesystem::path& path) {
        if (checkpoint ==
                ConfigurationPersistenceCheckpoint::kAfterRecordReplaced &&
            path == pending_path) {
            phases.push_back(
                ParsePendingConfigurationTransaction(ReadFile(path)).phase);
        }
    };

    const auto result = PersistPreviousConfiguration(
        {configuration_path, history_path, record.transaction_id},
        dependencies);
    QCOMPARE(result.outcome,
             PreviousPersistenceOutcome::kPreviousPersistenceApplied);
    QCOMPARE(result.confirmed_durable_phase,
             std::optional{PendingTransactionPhase::kPreviousRuntimeApplying});
    QCOMPARE(phases, (std::vector<PendingTransactionPhase>{
                         PendingTransactionPhase::kPreviousPersistenceApplying,
                         PendingTransactionPhase::kPreviousRuntimeApplying}));
    QCOMPARE(ReadFile(configuration_path), previous);
    QCOMPARE(ReadFile(history_path), std::string("must remain untouched"));
    QCOMPARE(history_operations, 0);
}

void ApplicationServiceTest::RestoresPreviousHistoryPayloadsAndAbsence() {
    for (const std::string& previous_history :
         {std::string("old history"), std::string{}}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto configuration_path = root / "core.conf";
        const auto history_path = root / "history-v1";
        test::WriteBinaryFile(configuration_path, "desired configuration");
        test::WriteBinaryFile(history_path, "desired history");
        const auto record = FallbackRecord(
            0x82U, PendingHistoryIntent::kReplace, "previous configuration",
            {true, MakePendingTransactionPayload(previous_history)});
        test::WriteBinaryFile(
            PendingConfigurationTransactionPath(configuration_path),
            SerializePendingConfigurationTransaction(record));

        const auto result = PersistPreviousConfiguration(
            {configuration_path, history_path, record.transaction_id});
        QCOMPARE(result.outcome,
                 PreviousPersistenceOutcome::kPreviousPersistenceApplied);
        QCOMPARE(ReadFile(history_path), previous_history);
    }

    for (const bool initially_present : {true, false}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto configuration_path = root / "core.conf";
        const auto history_path = root / "history-v1";
        test::WriteBinaryFile(configuration_path, "desired configuration");
        if (initially_present)
            test::WriteBinaryFile(history_path, "desired history");
        const auto record = FallbackRecord(
            0x83U, PendingHistoryIntent::kReplace, "previous configuration");
        test::WriteBinaryFile(
            PendingConfigurationTransactionPath(configuration_path),
            SerializePendingConfigurationTransaction(record));
        int absence_verifications = 0;
        ConfigurationPersistenceDependencies dependencies;
        dependencies.filesystem_failure =
            [&absence_verifications](
                ConfigurationPersistenceOperation operation,
                const std::filesystem::path&)
            -> std::optional<std::error_code> {
            if (operation == ConfigurationPersistenceOperation::kVerifyAbsence)
                ++absence_verifications;
            return std::nullopt;
        };
        const auto result = PersistPreviousConfiguration(
            {configuration_path, history_path, record.transaction_id},
            dependencies);
        QCOMPARE(result.outcome,
                 PreviousPersistenceOutcome::kPreviousPersistenceApplied);
        QVERIFY(!std::filesystem::exists(history_path));
        QCOMPARE(absence_verifications, 1);
    }
}

void ApplicationServiceTest::RejectsUnsafePreviousHistoryAbsenceTargets() {
    for (const bool symlink : {false, true}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = TemporaryPath(directory);
        const auto configuration_path = root / "core.conf";
        const auto history_path = root / "history-v1";
        test::WriteBinaryFile(configuration_path, "desired configuration");
        const auto record = FallbackRecord(
            0x84U, PendingHistoryIntent::kReplace, "previous configuration");
        test::WriteBinaryFile(
            PendingConfigurationTransactionPath(configuration_path),
            SerializePendingConfigurationTransaction(record));
        const auto unrelated = root / "unrelated";
        if (symlink) {
            test::WriteBinaryFile(unrelated, "preserved");
            std::error_code error;
            std::filesystem::create_symlink(unrelated, history_path, error);
            if (IsSymlinkCapabilityUnavailable(error))
                QSKIP("Windows symlink capability is unavailable");
            QVERIFY2(!error, error.message().c_str());
        } else {
            QVERIFY(std::filesystem::create_directory(history_path));
            test::WriteBinaryFile(history_path / "marker", "preserved");
        }

        const auto result = PersistPreviousConfiguration(
            {configuration_path, history_path, record.transaction_id});
        QCOMPARE(result.outcome,
                 PreviousPersistenceOutcome::kPreviousPersistenceFailed);
        QVERIFY(result.error.has_value());
        QCOMPARE(result.error->identity.operation,
                 PendingFailureOperation::kPersistPrevious);
        QCOMPARE(result.error->identity.destination,
                 PendingFailureDestination::kHistory);
        QCOMPARE(result.error->identity.identifier,
                 std::string("unsafe_path_type"));
        QVERIFY(result.failure_evidence_durable);
        if (symlink) {
            QVERIFY(std::filesystem::is_symlink(
                std::filesystem::symlink_status(history_path)));
            QCOMPARE(ReadFile(unrelated), std::string("preserved"));
        } else {
            QVERIFY(std::filesystem::is_directory(history_path));
            QCOMPARE(ReadFile(history_path / "marker"),
                     std::string("preserved"));
        }
    }
}

void ApplicationServiceTest::
    PreviousPersistenceFailuresRemainForwardAndTruthful() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    const auto history_path = root / "history-v1";
    test::WriteBinaryFile(configuration_path, "desired configuration");
    test::WriteBinaryFile(history_path, "desired history");
    auto record = FallbackRecord(
        0x85U, PendingHistoryIntent::kReplace, "previous configuration",
        {true, MakePendingTransactionPayload("previous history")});
    const auto pending_path =
        PendingConfigurationTransactionPath(configuration_path);
    test::WriteBinaryFile(pending_path,
                          SerializePendingConfigurationTransaction(record));

    ConfigurationPersistenceDependencies failure;
    failure.filesystem_failure =
        [&history_path](ConfigurationPersistenceOperation operation,
                        const std::filesystem::path& path)
        -> std::optional<std::error_code> {
        if (operation == ConfigurationPersistenceOperation::kVerifyPayload &&
            path == history_path)
            return std::make_error_code(std::errc::io_error);
        return std::nullopt;
    };
    const auto failed = PersistPreviousConfiguration(
        {configuration_path, history_path, record.transaction_id}, failure);
    QCOMPARE(failed.outcome,
             PreviousPersistenceOutcome::kPreviousPersistenceFailed);
    QVERIFY(failed.failure_evidence_durable);
    QCOMPARE(failed.error->identity.operation,
             PendingFailureOperation::kPersistPrevious);
    QCOMPARE(failed.error->identity.destination,
             PendingFailureDestination::kHistory);
    QCOMPARE(failed.error->identity.identifier,
             std::string("verification_failed"));
    QCOMPARE(ReadFile(configuration_path),
             std::string("previous configuration"));
    QCOMPARE(ReadFile(history_path), std::string("previous history"));
    QCOMPARE(ParsePendingConfigurationTransaction(ReadFile(pending_path)).phase,
             PendingTransactionPhase::kPreviousPersistenceBlocked);

    const auto replay = PersistPreviousConfiguration(
        {configuration_path, history_path, record.transaction_id});
    QCOMPARE(replay.outcome,
             PreviousPersistenceOutcome::kPreviousPersistenceApplied);
    QCOMPARE(replay.confirmed_durable_phase,
             std::optional{PendingTransactionPhase::kPreviousRuntimeApplying});

    const auto successful_pending_bytes = ReadFile(pending_path);
    auto wrong_identity = record.transaction_id;
    wrong_identity[0] ^= 0xffU;
    const auto mismatched = PersistPreviousConfiguration(
        {configuration_path, history_path, wrong_identity});
    QCOMPARE(mismatched.error->identity.identifier,
             std::string("pending_identity_mismatch"));
    QVERIFY(!mismatched.failure_evidence_namespace_published);
    QCOMPARE(ReadFile(pending_path), successful_pending_bytes);

    test::WriteBinaryFile(pending_path, "corrupt pending bytes");
    const auto corrupt = PersistPreviousConfiguration(
        {configuration_path, history_path, record.transaction_id});
    QCOMPARE(corrupt.error->identity.identifier,
             std::string("pending_record_invalid"));
    QVERIFY(!corrupt.failure_evidence_namespace_published);
    QCOMPARE(ReadFile(pending_path), std::string("corrupt pending bytes"));

    const auto unrelated = root / "unrelated";
    test::WriteBinaryFile(unrelated, "unrelated pending bytes");
    std::error_code remove_error;
    std::filesystem::remove(pending_path, remove_error);
    QVERIFY(!remove_error);
    std::filesystem::create_symlink(unrelated, pending_path, remove_error);
    if (IsSymlinkCapabilityUnavailable(remove_error))
        QSKIP("Windows symlink capability is unavailable");
    QVERIFY2(!remove_error, remove_error.message().c_str());
    const auto rejected = PersistPreviousConfiguration(
        {configuration_path, history_path, record.transaction_id});
    QCOMPARE(rejected.outcome,
             PreviousPersistenceOutcome::kPreviousPersistenceFailed);
    QVERIFY(!rejected.failure_evidence_namespace_published);
    QCOMPARE(ReadFile(unrelated), std::string("unrelated pending bytes"));

    QTemporaryDir unconfirmed_directory;
    QVERIFY(unconfirmed_directory.isValid());
    const auto unconfirmed_root = TemporaryPath(unconfirmed_directory);
    const auto unconfirmed_configuration = unconfirmed_root / "core.conf";
    const auto unconfirmed_history = unconfirmed_root / "history-v1";
    test::WriteBinaryFile(unconfirmed_configuration, "desired configuration");
    record = FallbackRecord(0x86U, PendingHistoryIntent::kUnchanged,
                            "previous configuration");
    const auto unconfirmed_pending =
        PendingConfigurationTransactionPath(unconfirmed_configuration);
    test::WriteBinaryFile(unconfirmed_pending,
                          SerializePendingConfigurationTransaction(record));
    ConfigurationPersistenceDependencies unconfirmed_failure;
    unconfirmed_failure.filesystem_failure =
        [&unconfirmed_pending](ConfigurationPersistenceOperation operation,
                               const std::filesystem::path& path)
        -> std::optional<std::error_code> {
        if (operation == ConfigurationPersistenceOperation::kSyncDirectory &&
            path == unconfirmed_pending.parent_path())
            return std::make_error_code(std::errc::io_error);
        return std::nullopt;
    };
    const auto unconfirmed = PersistPreviousConfiguration(
        {unconfirmed_configuration, unconfirmed_history, record.transaction_id},
        unconfirmed_failure);
    QCOMPARE(unconfirmed.outcome,
             PreviousPersistenceOutcome::kPreviousPersistenceFailed);
    QCOMPARE(
        unconfirmed.namespace_published_phase,
        std::optional{PendingTransactionPhase::kPreviousPersistenceBlocked});
    QVERIFY(!unconfirmed.confirmed_durable_phase.has_value());
    QVERIFY(unconfirmed.failure_evidence_namespace_published);
    QVERIFY(!unconfirmed.failure_evidence_durable);
    QVERIFY(unconfirmed.failure_evidence_error.has_value());
    QCOMPARE(unconfirmed.failure_evidence_error->identity.operation,
             PendingFailureOperation::kPersistPrevious);
    QCOMPARE(unconfirmed.failure_evidence_error->identity.identifier,
             std::string("directory_sync_failed"));
    QCOMPARE(ParsePendingConfigurationTransaction(ReadFile(unconfirmed_pending))
                 .phase,
             PendingTransactionPhase::kPreviousPersistenceBlocked);
}

void ApplicationServiceTest::RecoveryPolicyDurablyGatesDesiredAttempts() {
    for (const auto phase :
         {PendingTransactionPhase::kDesiredCommit,
          PendingTransactionPhase::kDesiredPersistenceApplying,
          PendingTransactionPhase::kDesiredPersistenceFailed}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto configuration_path = TemporaryPath(directory) / "core.conf";
        auto record = FallbackRecord(0x91U, PendingHistoryIntent::kUnchanged,
                                     "previous configuration");
        record.phase = phase;
        const auto pending_path =
            PendingConfigurationTransactionPath(configuration_path);
        test::WriteBinaryFile(pending_path,
                              SerializePendingConfigurationTransaction(record));

        const auto authorized = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id});
        QCOMPARE(authorized.disposition,
                 ConfigurationRecoveryDisposition::
                     kAutomaticDesiredRecoveryAuthorized);
        const ConfigurationRecoverySnapshot expected{
            phase, DesiredRecoveryAttempt::kAttempted};
        QCOMPARE(authorized.namespace_visible_snapshot,
                 std::optional{expected});
        QCOMPARE(authorized.directory_sync_confirmed_snapshot,
                 std::optional{expected});
        QVERIFY(authorized.exact_verification_succeeded);
        auto persisted =
            ParsePendingConfigurationTransaction(ReadFile(pending_path));
        QCOMPARE(persisted.phase, phase);
        QCOMPARE(persisted.desired_recovery_attempt,
                 DesiredRecoveryAttempt::kAttempted);
        QCOMPARE(persisted.failure, record.failure);
        record.desired_recovery_attempt = DesiredRecoveryAttempt::kAttempted;
        QCOMPARE(ReadFile(pending_path),
                 SerializePendingConfigurationTransaction(record));

        const auto bytes = ReadFile(pending_path);
        const auto fallback = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id});
        QCOMPARE(fallback.disposition,
                 ConfigurationRecoveryDisposition::kPreviousFallbackSelected);
        QVERIFY(!fallback.namespace_visible_snapshot.has_value());
        QCOMPARE(ReadFile(pending_path), bytes);
    }
}

void ApplicationServiceTest::RecoveryPolicyCrashCheckpointsRemainBounded() {
    for (const auto failed_checkpoint :
         {ConfigurationPersistenceCheckpoint::kAfterRecordReplaced,
          ConfigurationPersistenceCheckpoint::kAfterDirectorySynced,
          ConfigurationPersistenceCheckpoint::kAfterPendingRecordVerified}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto configuration_path = TemporaryPath(directory) / "core.conf";
        auto record = FallbackRecord(0x96U, PendingHistoryIntent::kUnchanged,
                                     "previous configuration");
        const auto pending_path =
            PendingConfigurationTransactionPath(configuration_path);
        test::WriteBinaryFile(pending_path,
                              SerializePendingConfigurationTransaction(record));
        ConfigurationPersistenceDependencies dependencies;
        dependencies.checkpoint =
            [failed_checkpoint](ConfigurationPersistenceCheckpoint checkpoint,
                                const std::filesystem::path&) {
                if (checkpoint == failed_checkpoint)
                    throw std::runtime_error("simulated crash");
            };
        const auto result = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id}, dependencies);
        QCOMPARE(result.disposition,
                 ConfigurationRecoveryDisposition::kRejectedOrFailed);
        const ConfigurationRecoverySnapshot expected{
            record.phase, DesiredRecoveryAttempt::kAttempted};
        QCOMPARE(result.namespace_visible_snapshot, std::optional{expected});
        QCOMPARE(result.directory_sync_confirmed_snapshot.has_value(),
                 failed_checkpoint !=
                     ConfigurationPersistenceCheckpoint::kAfterRecordReplaced);
        QVERIFY(!result.exact_verification_succeeded);
        record.desired_recovery_attempt = DesiredRecoveryAttempt::kAttempted;
        QCOMPARE(ReadFile(pending_path),
                 SerializePendingConfigurationTransaction(record));

        const auto next = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id});
        QCOMPARE(next.disposition,
                 ConfigurationRecoveryDisposition::kPreviousFallbackSelected);
    }
}

void ApplicationServiceTest::StartupRecoveryReplaysDesiredPersistence() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configuration_path = root / "core.conf";
    const auto desired_path = root / "desired.conf";
    const auto history_path = root / "history-v1";
    CoreConfiguration previous;
    previous.index_directory = "previous-index";
    CoreConfiguration desired;
    desired.index_directory = "desired-index";
    SaveConfiguration(configuration_path.string(), previous);
    SaveConfiguration(desired_path.string(), desired);
    const std::vector<HistoryEntry> desired_history{{7U, "recovered"}};
    SaveHistory(history_path.string(), desired_history);
    const auto desired_history_bytes = ReadFile(history_path);
    std::filesystem::remove(history_path);

    auto record =
        FallbackRecord(0xa2U, PendingHistoryIntent::kReplace,
                       ReadFile(configuration_path), {false, std::nullopt});
    record.phase = PendingTransactionPhase::kDesiredCommit;
    record.failure.reset();
    record.desired_configuration =
        MakePendingTransactionPayload(ReadFile(desired_path));
    record.desired_history =
        MakePendingTransactionPayload(desired_history_bytes);
    const auto pending_path =
        PendingConfigurationTransactionPath(configuration_path);
    test::WriteBinaryFile(pending_path,
                          SerializePendingConfigurationTransaction(record));
    const ConfigurationRecoveryRequest request{configuration_path,
                                               record.transaction_id};
    const auto authorized = EvaluateConfigurationRecovery(request);
    QCOMPARE(
        authorized.disposition,
        ConfigurationRecoveryDisposition::kAutomaticDesiredRecoveryAuthorized);
    const auto replay = ReplayDesiredConfiguration(request, history_path);
    QCOMPARE(replay.outcome,
             ConfigurationPersistenceOutcome::kDesiredPersistenceApplied);
    QCOMPARE(LoadConfiguration(configuration_path.string()).index_directory,
             desired.index_directory);
    QCOMPARE(LoadHistory(history_path.string(), 10U), desired_history);
    QCOMPARE(ParsePendingConfigurationTransaction(ReadFile(pending_path)).phase,
             PendingTransactionPhase::kDesiredPersistenceApplied);
}

void ApplicationServiceTest::
    RecoveryPolicyReportsMarkerPublicationTruthfully() {
    for (const bool fail_verification : {false, true}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto configuration_path = TemporaryPath(directory) / "core.conf";
        auto record = FallbackRecord(0x92U, PendingHistoryIntent::kUnchanged,
                                     "previous configuration");
        const auto pending_path =
            PendingConfigurationTransactionPath(configuration_path);
        test::WriteBinaryFile(pending_path,
                              SerializePendingConfigurationTransaction(record));
        ConfigurationPersistenceDependencies dependencies;
        dependencies.filesystem_failure =
            [fail_verification, &pending_path](
                ConfigurationPersistenceOperation operation,
                const std::filesystem::path& path)
            -> std::optional<std::error_code> {
            if ((!fail_verification &&
                 operation ==
                     ConfigurationPersistenceOperation::kSyncDirectory &&
                 path == pending_path.parent_path()) ||
                (fail_verification &&
                 operation ==
                     ConfigurationPersistenceOperation::kVerifyPendingRecord &&
                 path == pending_path))
                return std::make_error_code(std::errc::io_error);
            return std::nullopt;
        };
        const auto result = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id}, dependencies);
        QCOMPARE(result.disposition,
                 ConfigurationRecoveryDisposition::kRejectedOrFailed);
        const ConfigurationRecoverySnapshot expected{
            record.phase, DesiredRecoveryAttempt::kAttempted};
        QCOMPARE(result.namespace_visible_snapshot, std::optional{expected});
        QCOMPARE(result.directory_sync_confirmed_snapshot.has_value(),
                 fail_verification);
        QVERIFY(!result.exact_verification_succeeded);
        QVERIFY(result.primary_error.has_value());

        const auto next = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id});
        QCOMPARE(next.disposition,
                 ConfigurationRecoveryDisposition::kPreviousFallbackSelected);
    }
}

void ApplicationServiceTest::RecoveryPolicyQuarantinesTerminalBlocks() {
    for (const int failure_point : {0, 1, 2}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto configuration_path = TemporaryPath(directory) / "core.conf";
        auto record = FallbackRecord(0x93U, PendingHistoryIntent::kUnchanged,
                                     "previous configuration");
        record.phase = PendingTransactionPhase::kPreviousPersistenceBlocked;
        record.desired_recovery_attempt = DesiredRecoveryAttempt::kAttempted;
        const auto root_cause = record.failure;
        const auto pending_path =
            PendingConfigurationTransactionPath(configuration_path);
        test::WriteBinaryFile(pending_path,
                              SerializePendingConfigurationTransaction(record));
        ConfigurationPersistenceDependencies dependencies;
        if (failure_point != 0) {
            dependencies.filesystem_failure =
                [failure_point, &pending_path](
                    ConfigurationPersistenceOperation operation,
                    const std::filesystem::path& path)
                -> std::optional<std::error_code> {
                if ((failure_point == 1 &&
                     operation ==
                         ConfigurationPersistenceOperation::kSyncDirectory &&
                     path == pending_path.parent_path()) ||
                    (failure_point == 2 &&
                     operation == ConfigurationPersistenceOperation::
                                      kVerifyPendingRecord &&
                     path == pending_path))
                    return std::make_error_code(std::errc::io_error);
                return std::nullopt;
            };
        }
        const auto result = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id}, dependencies);
        const ConfigurationRecoverySnapshot expected{
            PendingTransactionPhase::kQuarantined,
            DesiredRecoveryAttempt::kAttempted};
        QCOMPARE(result.namespace_visible_snapshot, std::optional{expected});
        QCOMPARE(result.directory_sync_confirmed_snapshot.has_value(),
                 failure_point != 1);
        QCOMPARE(result.exact_verification_succeeded, failure_point == 0);
        QCOMPARE(result.disposition,
                 failure_point == 0
                     ? ConfigurationRecoveryDisposition::kQuarantinedTerminal
                     : ConfigurationRecoveryDisposition::kRejectedOrFailed);
        QVERIFY(result.primary_error.has_value());
        QCOMPARE(result.primary_error->identity, *root_cause);
        QCOMPARE(result.secondary_error.has_value(), failure_point != 0);
        const auto quarantined =
            ParsePendingConfigurationTransaction(ReadFile(pending_path));
        QCOMPARE(quarantined.phase, PendingTransactionPhase::kQuarantined);
        QCOMPARE(quarantined.failure, root_cause);
        record.phase = PendingTransactionPhase::kQuarantined;
        QCOMPARE(ReadFile(pending_path),
                 SerializePendingConfigurationTransaction(record));

        int mutations = 0;
        ConfigurationPersistenceDependencies observe;
        observe.filesystem_failure =
            [&mutations](ConfigurationPersistenceOperation operation,
                         const std::filesystem::path&)
            -> std::optional<std::error_code> {
            if (operation ==
                    ConfigurationPersistenceOperation::kCreateTemporary ||
                operation ==
                    ConfigurationPersistenceOperation::kReplaceDestination ||
                operation == ConfigurationPersistenceOperation::kSyncDirectory)
                ++mutations;
            return std::nullopt;
        };
        const auto repeated = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id}, observe);
        QCOMPARE(repeated.disposition,
                 ConfigurationRecoveryDisposition::kQuarantinedTerminal);
        QCOMPARE(mutations, 0);
    }
}

void ApplicationServiceTest::RecoveryPolicyDefersAndRejectsWithoutMutation() {
    for (const auto attempt : {DesiredRecoveryAttempt::kNotAttempted,
                               DesiredRecoveryAttempt::kAttempted}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto configuration_path = TemporaryPath(directory) / "core.conf";
        auto record = FallbackRecord(0x97U, PendingHistoryIntent::kUnchanged,
                                     "previous configuration");
        record.phase = PendingTransactionPhase::kPreviousPersistenceApplying;
        record.desired_recovery_attempt = attempt;
        const auto pending_path =
            PendingConfigurationTransactionPath(configuration_path);
        const auto bytes = SerializePendingConfigurationTransaction(record);
        test::WriteBinaryFile(pending_path, bytes);
        const auto replay = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id});
        QCOMPARE(
            replay.disposition,
            ConfigurationRecoveryDisposition::kPreviousFallbackReplaySelected);
        QCOMPARE(ReadFile(pending_path), bytes);
    }

    for (const auto phase :
         {PendingTransactionPhase::kPrepared,
          PendingTransactionPhase::kDesiredPersistenceApplied,
          PendingTransactionPhase::kDesiredRuntimeApplying,
          PendingTransactionPhase::kDesiredRuntimeFailed,
          PendingTransactionPhase::kPreviousRuntimeApplying}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto configuration_path = TemporaryPath(directory) / "core.conf";
        auto record = FallbackRecord(0x94U, PendingHistoryIntent::kUnchanged,
                                     "previous configuration");
        record.phase = phase;
        const auto pending_path =
            PendingConfigurationTransactionPath(configuration_path);
        const auto bytes = SerializePendingConfigurationTransaction(record);
        test::WriteBinaryFile(pending_path, bytes);
        const auto result = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id});
        QCOMPARE(result.disposition,
                 ConfigurationRecoveryDisposition::kNoActionDeferToRuntime);
        QCOMPARE(ReadFile(pending_path), bytes);
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto configuration_path = TemporaryPath(directory) / "core.conf";
    auto record = FallbackRecord(0x95U, PendingHistoryIntent::kUnchanged,
                                 "previous configuration");
    const auto pending_path =
        PendingConfigurationTransactionPath(configuration_path);
    const auto bytes = SerializePendingConfigurationTransaction(record);
    test::WriteBinaryFile(pending_path, bytes);
    auto wrong_identity = record.transaction_id;
    wrong_identity[0] ^= 0xffU;
    const auto wrong =
        EvaluateConfigurationRecovery({configuration_path, wrong_identity});
    QCOMPARE(wrong.disposition,
             ConfigurationRecoveryDisposition::kRejectedOrFailed);
    QCOMPARE(ReadFile(pending_path), bytes);

    test::WriteBinaryFile(pending_path, "corrupt pending bytes");
    const auto corrupt = EvaluateConfigurationRecovery(
        {configuration_path, record.transaction_id});
    QCOMPARE(corrupt.disposition,
             ConfigurationRecoveryDisposition::kRejectedOrFailed);
    QCOMPARE(ReadFile(pending_path), std::string("corrupt pending bytes"));

    const auto unrelated = TemporaryPath(directory) / "unrelated";
    test::WriteBinaryFile(unrelated, "unrelated pending bytes");
    std::error_code error;
    std::filesystem::remove(pending_path, error);
    QVERIFY(!error);
    std::filesystem::create_symlink(unrelated, pending_path, error);
    if (!error) {
        const auto unsafe = EvaluateConfigurationRecovery(
            {configuration_path, record.transaction_id});
        QCOMPARE(unsafe.disposition,
                 ConfigurationRecoveryDisposition::kRejectedOrFailed);
        QCOMPARE(ReadFile(unrelated), std::string("unrelated pending bytes"));
        std::filesystem::remove(pending_path, error);
        QVERIFY(!error);
    }
    QVERIFY(std::filesystem::create_directory(pending_path));
    test::WriteBinaryFile(pending_path / "marker", "preserved");
    const auto directory_hazard = EvaluateConfigurationRecovery(
        {configuration_path, record.transaction_id});
    QCOMPARE(directory_hazard.disposition,
             ConfigurationRecoveryDisposition::kRejectedOrFailed);
    QCOMPARE(ReadFile(pending_path / "marker"), std::string("preserved"));
}

void ApplicationServiceTest::PreviousRuntimeFailureQuarantineIsTerminal() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto configuration_path = TemporaryPath(directory) / "core.conf";
    const auto pending_path =
        PendingConfigurationTransactionPath(configuration_path);
    auto record = FallbackRecord(0xa1U, PendingHistoryIntent::kUnchanged,
                                 "previous configuration");
    record.phase = PendingTransactionPhase::kPreviousRuntimeApplying;
    record.failure.reset();
    test::WriteBinaryFile(configuration_path, "previous configuration");
    test::WriteBinaryFile(pending_path,
                          SerializePendingConfigurationTransaction(record));

    const ConfigurationRecoveryRequest request{configuration_path,
                                               record.transaction_id};
    const auto quarantined = QuarantineConfigurationTransaction(
        request, PendingFailureOperation::kReconstructPrevious,
        PendingFailureDestination::kRuntimeFoundation,
        PendingFailureCategory::kUnavailable, "core_construction_failed");
    QCOMPARE(quarantined.outcome, RuntimeTransitionOutcome::kApplied);
    QCOMPARE(quarantined.namespace_published_phase,
             std::optional{PendingTransactionPhase::kQuarantined});
    QCOMPARE(quarantined.confirmed_durable_phase,
             std::optional{PendingTransactionPhase::kQuarantined});
    const auto persisted =
        ParsePendingConfigurationTransaction(ReadFile(pending_path));
    QCOMPARE(persisted.phase, PendingTransactionPhase::kQuarantined);
    QCOMPARE(persisted.failure->operation,
             PendingFailureOperation::kReconstructPrevious);
    QCOMPARE(persisted.failure->destination,
             PendingFailureDestination::kRuntimeFoundation);

    int mutations = 0;
    ConfigurationPersistenceDependencies observe;
    observe.filesystem_failure =
        [&mutations](
            ConfigurationPersistenceOperation operation,
            const std::filesystem::path&) -> std::optional<std::error_code> {
        if (operation == ConfigurationPersistenceOperation::kCreateTemporary ||
            operation ==
                ConfigurationPersistenceOperation::kReplaceDestination ||
            operation == ConfigurationPersistenceOperation::kSyncDirectory)
            ++mutations;
        return std::nullopt;
    };
    const auto repeated = EvaluateConfigurationRecovery(request, observe);
    QCOMPARE(repeated.disposition,
             ConfigurationRecoveryDisposition::kQuarantinedTerminal);
    QCOMPARE(mutations, 0);

    record.phase = PendingTransactionPhase::kPreviousRuntimeApplying;
    test::WriteBinaryFile(pending_path,
                          SerializePendingConfigurationTransaction(record));
    ConfigurationPersistenceDependencies uncertain;
    uncertain.filesystem_failure =
        [&pending_path](ConfigurationPersistenceOperation operation,
                        const std::filesystem::path& path)
        -> std::optional<std::error_code> {
        if (operation == ConfigurationPersistenceOperation::kSyncDirectory &&
            path == pending_path.parent_path())
            return std::make_error_code(std::errc::io_error);
        return std::nullopt;
    };
    const auto namespace_only = QuarantineConfigurationTransaction(
        request, PendingFailureOperation::kReconstructPrevious,
        PendingFailureDestination::kRuntimeTransport,
        PendingFailureCategory::kIo, "network_construction_failed", uncertain);
    QCOMPARE(namespace_only.outcome,
             RuntimeTransitionOutcome::kPostPublicationFailure);
    QCOMPARE(namespace_only.namespace_published_phase,
             std::optional{PendingTransactionPhase::kQuarantined});
    QVERIFY(!namespace_only.confirmed_durable_phase.has_value());
    QCOMPARE(ParsePendingConfigurationTransaction(ReadFile(pending_path)).phase,
             PendingTransactionPhase::kQuarantined);
}

void ApplicationServiceTest::MigratesLegacyPathsWithoutTouchingTheSource() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";
    const std::string legacy =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<config><paths>"
        "<path recursive=\"1\">/dicts/English &amp; French</path>"
        "<path recursive=\"0\">/dicts/CJK</path>"
        "</paths><sounddirs>"
        "<sounddir name=\"Spoken &amp; examples\" icon=\"ignored.png\">"
        "/audio/words</sounddir>"
        "</sounddirs><groups nextId=\"9\">"
        "<group id=\"7\" name=\"English &amp; French\" "
        "icon=\"flags/world.svg\" iconData=\"aWNvbg==\" "
        "favoritesFolder=\"Languages/English\" shortcut=\"Ctrl+1\">"
        "<dictionary name=\"Known name\">known-id</dictionary>"
        "<dictionary name=\"Recovery hint only\">unknown-id</dictionary>"
        "<mutedDictionaries><mutedDictionary>known-id</mutedDictionary>"
        "<popupMutedDictionary>unknown-id</popupMutedDictionary>"
        "</mutedDictionaries></group>"
        "<group id=\"8\" name=\"Reference\">"
        "<dictionary>reference-id</dictionary></group></groups>"
        "<hunspell dictionariesPath=\"/morphology\">"
        "<enabled>en_US</enabled><enabled>fr_FR</enabled></hunspell>"
        "<preferences>"
        "<interfaceLanguage>zh_CN</interfaceLanguage>"
        "<helpLanguage>fr_FR</helpLanguage>"
        "<displayStyle>dark</displayStyle><addonStyle>contrast.css</addonStyle>"
        "<hideMenubar>1</hideMenubar><enableTrayIcon>0</enableTrayIcon>"
        "<doubleClickTranslates>0</doubleClickTranslates>"
        "<selectWordBySingleClick>1</selectWordBySingleClick>"
        "<enableMainWindowHotkey>1</enableMainWindowHotkey>"
        "<mainWindowHotkey>Alt+Space</mainWindowHotkey>"
        "<scanPopupModifiers>33</scanPopupModifiers>"
        "<scanPopupAltModeSecs>12</scanPopupAltModeSecs>"
        "<scanPopupUnpinnedWindowFlags>2</scanPopupUnpinnedWindowFlags>"
        "<internalPlayerBackend>Qt Multimedia</internalPlayerBackend>"
        "<audioPlaybackProgram>mpv --no-video</audioPlaybackProgram>"
        "<proxyserver enabled=\"1\" useSystemProxy=\"0\">"
        "<type>1</type><host>proxy.example</host><port>8443</port>"
        "<user>ignored-user</user><password>ignored-secret</password>"
        "</proxyserver>"
        "<maxNetworkCacheSize>256</maxNetworkCacheSize>"
        "<clearNetworkCacheOnExit>0</clearNetworkCacheOnExit>"
        "<zoomFactor>1.375</zoomFactor><helpZoomFactor>0.75</helpZoomFactor>"
        "<wordsZoomLevel>-2</wordsZoomLevel>"
        "<maxStringsInHistory>1234</maxStringsInHistory>"
        "<confirmFavoritesDeletion>0</confirmFavoritesDeletion>"
        "<alwaysExpandOptionalParts>1</alwaysExpandOptionalParts>"
        "<collapseBigArticles>1</collapseBigArticles>"
        "<articleSizeLimit>4096</articleSizeLimit>"
        "<maxDictionaryRefsInContextMenu>0</maxDictionaryRefsInContextMenu>"
        "<synonymSearchEnabled>0</synonymSearchEnabled>"
        "<newTabsOpenAfterCurrentOne>1</newTabsOpenAfterCurrentOne>"
        "<newTabsOpenInBackground>0</newTabsOpenInBackground>"
        "<hideSingleTab>1</hideSingleTab>"
        "<mruTabOrder>1</mruTabOrder>"
        "<escKeyHidesMainWindow>1</escKeyHidesMainWindow>"
        "<fullTextSearch><searchMode>3</searchMode><matchCase>1</matchCase>"
        "<maxArticlesPerDictionary>321</maxArticlesPerDictionary>"
        "<maxDistanceBetweenWords>9</maxDistanceBetweenWords>"
        "<maxDictionarySize>7654321</maxDictionarySize>"
        "<disabledTypes>audio|images</disabledTypes>"
        "<dialogGeometry>ZnRzLWdlb21ldHJ5</dialogGeometry></fullTextSearch>"
        "</preferences><mainWindowGeometry>Z2VvbWV0cnk=</mainWindowGeometry>"
        "</config>";
    test::WriteBinaryFile(legacy_path, legacy);

    const auto migrated = LoadOrMigrateConfiguration(
        current_path.string(), legacy_path.string(), "/cache/indexes");

    QCOMPARE(
        migrated.dictionary_paths,
        (std::vector<std::string>{"/dicts/English & French", "/dicts/CJK"}));
    QCOMPARE(migrated.index_directory, "/cache/indexes");
    QCOMPARE(migrated.morphology_dictionary_path, "/morphology");
    QCOMPARE(migrated.enabled_morphology_dictionary_ids,
             (std::vector<std::string>{"en_US", "fr_FR"}));
    QCOMPARE(migrated.sound_directories.size(), std::size_t{1});
    QCOMPARE(migrated.sound_directories.front().path, "/audio/words");
    QCOMPARE(migrated.sound_directories.front().name, "Spoken & examples");
    QCOMPARE(migrated.dictionary_groups.size(), std::size_t{2});
    QCOMPARE(migrated.dictionary_groups[0].id, std::uint32_t{7});
    QCOMPARE(migrated.dictionary_groups[0].name, "English & French");
    QCOMPARE(migrated.dictionary_groups[0].icon, "flags/world.svg");
    QCOMPARE(migrated.dictionary_groups[0].dictionary_ids,
             (std::vector<std::string>{"known-id", "unknown-id"}));
    QCOMPARE(migrated.dictionary_groups[0].muted_dictionary_ids,
             (std::vector<std::string>{"known-id"}));
    QCOMPARE(migrated.dictionary_groups[0].popup_muted_dictionary_ids,
             (std::vector<std::string>{"unknown-id"}));
    QCOMPARE(migrated.dictionary_groups[0].favorites_folder,
             "Languages/English");
    QCOMPARE(migrated.dictionary_groups[0].shortcut, "Ctrl+1");
    QCOMPARE(migrated.dictionary_groups[0].encoded_icon_data, "aWNvbg==");
    QCOMPARE(migrated.dictionary_groups[1].id, std::uint32_t{8});
    const auto& preferences = migrated.preferences;
    QCOMPARE(preferences.interface_language, "zh_CN");
    QCOMPARE(preferences.help_language, "fr_FR");
    QCOMPARE(preferences.display_style, "dark");
    QCOMPARE(preferences.addon_style, "contrast.css");
    QCOMPARE(preferences.open_new_tabs_after_current, true);
    QCOMPARE(preferences.open_new_tabs_in_background, false);
    QCOMPARE(preferences.hide_single_tab, true);
    QCOMPARE(preferences.mru_tab_order, true);
    QCOMPARE(preferences.escape_hides_main_window, true);
    QCOMPARE(preferences.hide_menubar, true);
    QCOMPARE(preferences.enable_tray_icon, false);
    QCOMPARE(preferences.double_click_translates, false);
    QCOMPARE(preferences.select_word_by_single_click, true);
    QCOMPARE(preferences.main_window_hotkey, "Alt+Space");
    QCOMPARE(preferences.scan_popup_modifiers, std::uint32_t{33});
    QCOMPARE(preferences.scan_popup_alt_mode_seconds, std::uint32_t{12});
    QCOMPARE(preferences.scan_popup_window_mode, ScanPopupWindowMode::kTool);
    QCOMPARE(preferences.audio_backend, AudioBackend::kQtMultimedia);
    QCOMPARE(preferences.audio_playback_program, "mpv --no-video");
    QCOMPARE(preferences.proxy_mode, ProxyMode::kManual);
    QCOMPARE(preferences.proxy_type, ProxyType::kHttpConnect);
    QCOMPARE(preferences.proxy_host, "proxy.example");
    QCOMPARE(preferences.proxy_port, std::uint16_t{8443});
    QCOMPARE(preferences.maximum_network_cache_megabytes, std::uint32_t{256});
    QCOMPARE(preferences.clear_network_cache_on_exit, false);
    QCOMPARE(preferences.zoom_factor, 1.375);
    QCOMPARE(preferences.help_zoom_factor, 0.75);
    QCOMPARE(preferences.words_zoom_level, std::int32_t{-2});
    QCOMPARE(preferences.maximum_history_entries, std::uint32_t{1234});
    QCOMPARE(preferences.collapse_large_articles, true);
    QCOMPARE(preferences.article_size_limit, std::uint32_t{4096});
    QCOMPARE(preferences.maximum_dictionary_references, std::uint16_t{0});
    QCOMPARE(preferences.synonym_search_enabled, false);
    QCOMPARE(preferences.full_text_search_mode,
             FullTextSearchMode::kRegularExpression);
    QCOMPARE(preferences.full_text_match_case, true);
    QCOMPARE(preferences.full_text_maximum_articles_per_dictionary,
             std::uint32_t{321});
    QCOMPARE(preferences.full_text_maximum_word_distance, std::uint32_t{9});
    QCOMPARE(preferences.full_text_maximum_dictionary_articles,
             std::uint32_t{7654321});
    QCOMPARE(preferences.full_text_disabled_types, "audio|images");
    QCOMPARE(preferences.confirm_favorites_deletion, false);
    QCOMPARE(preferences.always_expand_optional_parts, true);
    QCOMPARE(migrated.full_text_dialog_geometry, "fts-geometry");
    QCOMPARE(migrated.main_window_geometry, "geometry");
    QVERIFY(std::filesystem::exists(current_path));
    std::ifstream legacy_input(legacy_path, std::ios::binary);
    const std::string unchanged((std::istreambuf_iterator<char>(legacy_input)),
                                std::istreambuf_iterator<char>());
    QCOMPARE(unchanged, legacy);
    const auto round_trip = LoadConfiguration(current_path.string());
    QCOMPARE(round_trip.dictionary_paths, migrated.dictionary_paths);
    QCOMPARE(round_trip.sound_directories, migrated.sound_directories);
    QCOMPARE(round_trip.dictionary_groups, migrated.dictionary_groups);
    QCOMPARE(round_trip.preferences, migrated.preferences);
    QCOMPARE(round_trip.full_text_dialog_geometry,
             migrated.full_text_dialog_geometry);
    QCOMPARE(round_trip.main_window_geometry, migrated.main_window_geometry);
}

void ApplicationServiceTest::MigratesAllLegacyOnlineSourcesAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";
#ifdef _WIN32
    constexpr std::string_view tool_path = "C:/goldendict-fixture/tool.exe";
    constexpr std::string_view render_path = "C:/goldendict-fixture/render.exe";
    constexpr std::string_view prefix_path = "C:/goldendict-fixture/prefix.exe";
    constexpr std::string_view quote_path = "C:/goldendict-fixture/quote.exe";
#else
    constexpr std::string_view tool_path = "/usr/bin/tool";
    constexpr std::string_view render_path = "/opt/render";
    constexpr std::string_view prefix_path = "/opt/prefix";
    constexpr std::string_view quote_path = "/opt/quote";
#endif
    const std::string legacy =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><config>"
        "<mediawikis>"
        "<mediawiki id=\"wiki-a\" name=\"Wiki A\" "
        "url=\"https://a.example/w\" enabled=\"1\" icon=\"secret-a\"/>"
        "<mediawiki id=\"wiki-b\" name=\"Wiki B\" "
        "url=\"http://b.example/w\" enabled=\"0\" icon=\"\"/>"
        "</mediawikis><websites>"
        "<website id=\"site\" name=\"Site\" "
        "url=\"https://site.example/?q=%GDWORD%\" enabled=\"1\" "
        "icon=\"private-icon\" inside_iframe=\"0\"/>"
        "</websites><forvo><enable>1</enable>"
        "<apiKey>never-persist-this-key</apiKey>"
        "<languageCodes> en, zh-CN </languageCodes></forvo>"
        "<dictservers>"
        "<server id=\"dict-a\" name=\"DICT A\" "
        "url=\"dict://dict.example:2630\" enabled=\"1\" "
        "databases=\"wn\" strategies=\"exact\" icon=\"ignored\"/>"
        "<server id=\"dict-defaults\" name=\"DICT Defaults\" "
        "url=\"default.example\" enabled=\"0\" databases=\"\" "
        "strategies=\"\" icon=\"\"/>"
        "</dictservers><programs>"
        "<program id=\"plain\" name=\"Plain\" commandLine=\"" +
        std::string(tool_path) +
        " &quot;two words&quot; "
        "&quot;&quot; %GDWORD%\" enabled=\"1\" type=\"1\" icon=\"x\"/>"
        "<program id=\"html\" name=\"HTML\" commandLine=\"" +
        std::string(render_path) +
        " --html\" "
        "enabled=\"0\" type=\"2\" icon=\"\"/>"
        "<program id=\"prefix\" name=\"Prefix\" commandLine=\"" +
        std::string(prefix_path) +
        " %GDWORD%\" enabled=\"1\" type=\"3\" "
        "icon=\"\"/>"
        "<program id=\"quotes\" name=\"Quotes\" commandLine=\"" +
        std::string(quote_path) +
        " &quot;a&quot;&quot;b&quot; &quot;open a|b\" "
        "enabled=\"0\" type=\"1\" icon=\"\"/>"
        "</programs></config>";
    test::WriteBinaryFile(legacy_path, legacy);

    const auto migrated = LoadOrMigrateConfiguration(
        current_path.string(), legacy_path.string(), "/indexes");

    QCOMPARE(migrated.mediawiki_sources,
             (std::vector<MediaWikiSourceConfiguration>{
                 {"wiki-a", "Wiki A", true, "https://a.example/w"},
                 {"wiki-b", "Wiki B", false, "http://b.example/w"}}));
    QCOMPARE(migrated.website_sources,
             (std::vector<WebsiteSourceConfiguration>{
                 {"site", "Site", true, "https://site.example/?q=%GDWORD%"}}));
    QCOMPARE(
        migrated.forvo_sources,
        (std::vector<ForvoSourceConfiguration>{{"forvo",
                                                "Forvo",
                                                true,
                                                "https://apifree.forvo.com",
                                                {"en", "zh-CN"}}}));
    QCOMPARE(
        migrated.dict_server_sources,
        (std::vector<DictServerSourceConfiguration>{
            {"dict-a", "DICT A", true, "dict.example", 2630U, "wn", "exact"},
            {"dict-defaults", "DICT Defaults", false, "default.example", 2628U,
             "*", "prefix"}}));
    QCOMPARE(migrated.external_program_sources.size(), std::size_t{4});
    QCOMPARE(migrated.external_program_sources[0].executable,
             std::string(tool_path));
    QCOMPARE(migrated.external_program_sources[0].argument_templates,
             (std::vector<std::string>{"two words", "", "%GDWORD%"}));
    QCOMPARE(migrated.external_program_sources[0].output_kind,
             ExternalProgramOutputKind::kPlainText);
    QCOMPARE(migrated.external_program_sources[1].output_kind,
             ExternalProgramOutputKind::kHtml);
    QCOMPARE(migrated.external_program_sources[2].output_kind,
             ExternalProgramOutputKind::kPrefixMatch);
    QCOMPARE(migrated.external_program_sources[3].argument_templates,
             (std::vector<std::string>{"a\"b", "open a|b"}));
    QCOMPARE(LoadConfiguration(current_path.string()).external_program_sources,
             migrated.external_program_sources);
    QCOMPARE(ReadFile(legacy_path), legacy);
    QVERIFY(ReadFile(current_path).find("never-persist-this-key") ==
            std::string::npos);
    QVERIFY(ReadFile(current_path).find("private-icon") == std::string::npos);

    const auto defaults_legacy_path = root / "defaults-config";
    const auto defaults_current_path = root / "defaults-core.conf";
    const std::string defaults_legacy =
        "<config><mediawikis/><websites/><dictservers/><programs/></config>";
    test::WriteBinaryFile(defaults_legacy_path, defaults_legacy);
    const auto defaults =
        LoadOrMigrateConfiguration(defaults_current_path.string(),
                                   defaults_legacy_path.string(), "/indexes");
    QVERIFY(defaults.mediawiki_sources.empty());
    QVERIFY(defaults.website_sources.empty());
    QVERIFY(defaults.dict_server_sources.empty());
    QVERIFY(defaults.external_program_sources.empty());
    QCOMPARE(
        defaults.forvo_sources,
        (std::vector<ForvoSourceConfiguration>{{"forvo",
                                                "Forvo",
                                                false,
                                                "https://apifree.forvo.com",
                                                {"en", "ru"}}}));
    QCOMPARE(ReadFile(defaults_legacy_path), defaults_legacy);
}

void ApplicationServiceTest::RejectsUnsupportedLegacyOnlineSourcesAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";
    std::vector<std::string> invalid = {
        "<mediawikis/><mediawikis/>",
        "<mediawikis><mediawiki id=\"same\" name=\"A\" url=\"https://a/w\" "
        "enabled=\"1\"/></mediawikis><websites><website id=\"same\" name=\"B\" "
        "url=\"https://b/?q=%GDWORD%\" enabled=\"0\" "
        "inside_iframe=\"0\"/></websites>",
        "<mediawikis><mediawiki id=\"wiki\" name=\"A\" url=\"https://a/w\" "
        "enabled=\"true\"/></mediawikis>",
        "<websites><website id=\"site\" name=\"S\" "
        "url=\"https://s/?q=%GDWORD%\" enabled=\"1\" "
        "inside_iframe=\"1\"/></websites>",
        "<websites><website id=\"site\" name=\"S\" "
        "url=\"https://s/?q=%GD1251%\" enabled=\"1\" "
        "inside_iframe=\"0\"/></websites>",
        "<forvo><enable>1</enable><languageCodes>en,,ru</languageCodes></"
        "forvo>",
        "<forvo><enable><nested/></enable><languageCodes>en</languageCodes></"
        "forvo>",
        "<dictservers><server id=\"d\" name=\"D\" "
        "url=\"dict://user:secret@host\" enabled=\"1\" databases=\"*\" "
        "strategies=\"prefix\"/></dictservers>",
        "<dictservers><server id=\"d\" name=\"D\" url=\"host:70000\" "
        "enabled=\"1\" databases=\"*\" strategies=\"prefix\"/></dictservers>",
        "<dictservers><server id=\"d\" name=\"D\" url=\"host\" enabled=\"1\" "
        "databases=\"one,two\" strategies=\"prefix\"/></dictservers>",
        "<programs><program id=\"p\" name=\"P\" commandLine=\"/bin/play\" "
        "enabled=\"1\" type=\"0\"/></programs>",
        "<programs><program id=\"p\" name=\"P\" commandLine=\"relative --arg\" "
        "enabled=\"1\" type=\"1\"/></programs>",
        "<programs><program id=\"p\" name=\"P\" commandLine=\"/bin/sh -c "
        "echo\" enabled=\"1\" type=\"1\"/></programs>",
        "<programs><program id=\"p\" name=\"P\" commandLine=\"\" enabled=\"1\" "
        "type=\"1\"/></programs>",
        "<programs><program id=\"p\" name=\"P\" commandLine=\"/opt/%GDWORD%\" "
        "enabled=\"1\" type=\"1\"/></programs>",
    };
    std::string oversized = "<mediawikis>";
    for (std::size_t index = 0U; index <= kMaximumOnlineSources; ++index) {
        oversized +=
            "<mediawiki id=\"w" + std::to_string(index) +
            "\" name=\"W\" url=\"https://w.example/w\" enabled=\"0\"/>";
    }
    invalid.push_back(oversized + "</mediawikis>");
    invalid.push_back(
        "<mediawikis><mediawiki id=\"w\" name=\"" +
        std::string(64U * 1024U + 1U, 'x') +
        "\" url=\"https://w.example/w\" enabled=\"0\"/></mediawikis>");
    std::string excessive_arguments = "/opt/tool";
    for (std::size_t index = 0U; index <= kMaximumExternalProgramArguments;
         ++index) {
        excessive_arguments += " argument";
    }
    invalid.push_back("<programs><program id=\"p\" name=\"P\" commandLine=\"" +
                      excessive_arguments +
                      "\" enabled=\"1\" type=\"1\"/></programs>");
    invalid.push_back(
        std::string("<mediawikis><mediawiki id=\"w\" name=\"") +
        std::string("bad\xC3", 4U) +
        "\" url=\"https://w.example/w\" enabled=\"0\"/></mediawikis>");

    for (const auto& body : invalid) {
        const std::string legacy = "<config>" + body + "</config>";
        test::WriteBinaryFile(legacy_path, legacy);
        QVERIFY_EXCEPTION_THROWN(
            LoadOrMigrateConfiguration(current_path.string(),
                                       legacy_path.string(), "/indexes"),
            std::runtime_error);
        QVERIFY(!std::filesystem::exists(current_path));
        QVERIFY(!std::filesystem::exists(current_path.string() + ".tmp"));
        QCOMPARE(ReadFile(legacy_path), legacy);
    }
}

void ApplicationServiceTest::MissingLegacyPreferencesRetainCurrentDefaults() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";
    const std::string legacy =
        "<config><preferences><interfaceLanguage>de_DE</interfaceLanguage>"
        "<proxyserver enabled=\"0\" useSystemProxy=\"0\">"
        "<user>secret-user</user><password>secret-password</password>"
        "</proxyserver></preferences><mainWindowState>excluded</"
        "mainWindowState>"
        "</config>";
    test::WriteBinaryFile(legacy_path, legacy);

    const auto migrated = LoadOrMigrateConfiguration(
        current_path.string(), legacy_path.string(), "/cache/indexes");
    ApplicationPreferences defaults;
    defaults.interface_language = "de_DE";

    QCOMPARE(migrated.preferences, defaults);
    QVERIFY(migrated.full_text_dialog_geometry.empty());
    QVERIFY(migrated.main_window_geometry.empty());
    QVERIFY(migrated.main_window_state.empty());
    QCOMPARE(LoadConfiguration(current_path.string()).preferences, defaults);
    QCOMPARE(ReadFile(legacy_path), legacy);
}

void ApplicationServiceTest::MigratesAllLegacyFullTextSearchModes() {
    const std::pair<std::uint8_t, FullTextSearchMode> modes[] = {
        {0U, FullTextSearchMode::kWholeWords},
        {1U, FullTextSearchMode::kPlainText},
        {2U, FullTextSearchMode::kWildcard},
        {3U, FullTextSearchMode::kRegularExpression},
    };
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";

    for (const auto& [ordinal, mode] : modes) {
        const std::string legacy =
            "<config><preferences><fullTextSearch><searchMode>" +
            std::to_string(static_cast<unsigned int>(ordinal)) +
            "</searchMode></fullTextSearch></preferences></config>";
        test::WriteBinaryFile(legacy_path, legacy);
        const auto migrated = LoadOrMigrateConfiguration(
            current_path.string(), legacy_path.string(), "/indexes");
        QCOMPARE(migrated.preferences.full_text_search_mode, mode);
        QCOMPARE(LoadConfiguration(current_path.string())
                     .preferences.full_text_search_mode,
                 mode);
        QCOMPARE(ReadFile(legacy_path), legacy);
        std::filesystem::remove(current_path);
    }
}

void ApplicationServiceTest::MigratesBoundedLegacyFullTextDialogGeometry() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";
    const std::string boundary(64U * 1024U, '\0');
    const std::string encoded_boundary =
        QByteArray(boundary.data(), static_cast<qsizetype>(boundary.size()))
            .toBase64()
            .toStdString();
    const std::string valid =
        "<config><preferences><fullTextSearch><dialogGeometry>" +
        encoded_boundary +
        "</dialogGeometry></fullTextSearch></preferences></config>";
    test::WriteBinaryFile(legacy_path, valid);

    const auto migrated = LoadOrMigrateConfiguration(
        current_path.string(), legacy_path.string(), "/indexes");
    QCOMPARE(migrated.full_text_dialog_geometry, boundary);
    QCOMPARE(LoadConfiguration(current_path.string()).full_text_dialog_geometry,
             boundary);
    QCOMPARE(ReadFile(legacy_path), valid);

    std::filesystem::remove(current_path);
    const std::vector<std::string> invalid = {
        "<dialogGeometry>not-base64</dialogGeometry>",
        "<dialogGeometry>Zg=</dialogGeometry>",
        "<dialogGeometry>Zh==</dialogGeometry>",
        "<dialogGeometry>Zg==<nested/></dialogGeometry>",
        "<dialogGeometry>Zg==</dialogGeometry>"
        "<dialogGeometry>Zg==</dialogGeometry>",
        "<dialogGeometry>" + std::string(87384U, 'A') + "AAAA</dialogGeometry>",
    };
    for (const auto& geometry : invalid) {
        const std::string legacy =
            "<config><preferences><interfaceLanguage>de_DE</interfaceLanguage>"
            "<fullTextSearch>" +
            geometry + "</fullTextSearch></preferences></config>";
        test::WriteBinaryFile(legacy_path, legacy);
        QVERIFY_EXCEPTION_THROWN(
            LoadOrMigrateConfiguration(current_path.string(),
                                       legacy_path.string(), "/indexes"),
            std::runtime_error);
        QVERIFY(!std::filesystem::exists(current_path));
        QVERIFY(!std::filesystem::exists(current_path.string() + ".tmp"));
        QCOMPARE(ReadFile(legacy_path), legacy);
    }
}

void ApplicationServiceTest::RejectsMalformedLegacyPreferencesAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";
    std::vector<std::string> malformed = {
        "<enableTrayIcon>true</enableTrayIcon>",
        "<newTabsOpenAfterCurrentOne>true</newTabsOpenAfterCurrentOne>",
        "<hideSingleTab>true</hideSingleTab>",
        "<hideSingleTab>1</hideSingleTab><hideSingleTab>0</hideSingleTab>",
        "<mruTabOrder>true</mruTabOrder>",
        "<mruTabOrder>1</mruTabOrder><mruTabOrder>0</mruTabOrder>",
        "<doubleClickTranslates>1</doubleClickTranslates>"
        "<doubleClickTranslates>0</doubleClickTranslates>",
        "<selectWordBySingleClick>1</selectWordBySingleClick>"
        "<selectWordBySingleClick>0</selectWordBySingleClick>",
        "<zoomFactor>nan</zoomFactor>",
        "<wordsZoomLevel>2x</wordsZoomLevel>",
        "<maxDictionaryRefsInContextMenu>10000</"
        "maxDictionaryRefsInContextMenu>",
        "<scanPopupModifiers>65535</scanPopupModifiers>",
        "<scanPopupUnpinnedWindowFlags>3</scanPopupUnpinnedWindowFlags>",
        "<internalPlayerBackend>unknown</internalPlayerBackend>",
        "<articleSizeLimit>0</articleSizeLimit>",
        "<interfaceLanguage><nested/></interfaceLanguage>",
        "<storeHistory>1</storeHistory><storeHistory>0</storeHistory>",
        "<confirmFavoritesDeletion>true</confirmFavoritesDeletion>",
        "<alwaysExpandOptionalParts>true</alwaysExpandOptionalParts>",
        "<proxyserver enabled=\"yes\" useSystemProxy=\"0\"/>",
        "<proxyserver useSystemProxy=\"0\"/>",
        "<proxyserver enabled=\"1\"/>",
        "<proxyserver enabled=\"0\" useSystemProxy=\"0\"/>"
        "<proxyserver enabled=\"1\" useSystemProxy=\"0\"/>",
        "<proxyserver enabled=\"1\" useSystemProxy=\"0\"><type>9</type>"
        "<host>proxy</host><port>80</port></proxyserver>",
        "<fullTextSearch><searchMode>4</searchMode></fullTextSearch>",
    };
    malformed.push_back("<interfaceLanguage>" + std::string(4097U, 'x') +
                        "</interfaceLanguage>");
    for (const auto& preference : malformed) {
        const std::string legacy =
            "<config><preferences>" + preference + "</preferences></config>";
        test::WriteBinaryFile(legacy_path, legacy);
        QVERIFY_EXCEPTION_THROWN(
            LoadOrMigrateConfiguration(current_path.string(),
                                       legacy_path.string(), "/cache/indexes"),
            std::runtime_error);
        QVERIFY(!std::filesystem::exists(current_path));
        QVERIFY(!std::filesystem::exists(current_path.string() + ".tmp"));
        QCOMPARE(ReadFile(legacy_path), legacy);
    }

    const std::vector<std::string> malformed_geometry = {
        "not-base64", "Zg=", "Zh==", "Zg==<nested/>"};
    for (const auto& geometry : malformed_geometry) {
        const std::string legacy = "<config><mainWindowGeometry>" + geometry +
                                   "</mainWindowGeometry></config>";
        test::WriteBinaryFile(legacy_path, legacy);
        QVERIFY_EXCEPTION_THROWN(
            LoadOrMigrateConfiguration(current_path.string(),
                                       legacy_path.string(), "/cache/indexes"),
            std::runtime_error);
        QVERIFY(!std::filesystem::exists(current_path));
        QVERIFY(!std::filesystem::exists(current_path.string() + ".tmp"));
        QCOMPARE(ReadFile(legacy_path), legacy);
    }

    const std::string oversized_geometry(87384U, 'A');
    const std::string oversized_legacy = "<config><mainWindowGeometry>" +
                                         oversized_geometry +
                                         "AAAA</mainWindowGeometry></config>";
    test::WriteBinaryFile(legacy_path, oversized_legacy);
    QVERIFY_EXCEPTION_THROWN(
        LoadOrMigrateConfiguration(current_path.string(), legacy_path.string(),
                                   "/cache/indexes"),
        std::runtime_error);
    QVERIFY(!std::filesystem::exists(current_path));
    QCOMPARE(ReadFile(legacy_path), oversized_legacy);
}

void ApplicationServiceTest::CurrentConfigurationTakesPrecedenceOverLegacy() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";
    test::WriteBinaryFile(legacy_path, "<not-config>");
    CoreConfiguration current;
    current.dictionary_paths = {"/current"};
    current.index_directory = "/current/indexes";
    current.dictionary_groups = {{3U, "Current", "", {"current-id"}}};
    current.preferences.interface_language = "current-language";
    SaveConfiguration(current_path.string(), current);

    const auto loaded = LoadOrMigrateConfiguration(
        current_path.string(), legacy_path.string(), "/unused/indexes");

    QCOMPARE(loaded.dictionary_paths, current.dictionary_paths);
    QCOMPARE(loaded.index_directory, current.index_directory);
    QCOMPARE(loaded.dictionary_groups, current.dictionary_groups);
    QCOMPARE(loaded.preferences, current.preferences);
    QCOMPARE(ReadFile(legacy_path), "<not-config>");
}

void ApplicationServiceTest::RejectsMalformedLegacyWithoutCreatingCurrent() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";
    test::WriteBinaryFile(
        legacy_path,
        "<!DOCTYPE config [<!ENTITY unsafe 'expanded'>]>"
        "<config><paths><path>&unsafe;</path></paths></config>");

    QVERIFY_EXCEPTION_THROWN(
        LoadOrMigrateConfiguration(current_path.string(), legacy_path.string(),
                                   "/cache/indexes"),
        std::runtime_error);
    QVERIFY(!std::filesystem::exists(current_path));
    QVERIFY(std::filesystem::exists(legacy_path));
    std::ifstream legacy_input(legacy_path, std::ios::binary);
    const std::string unchanged((std::istreambuf_iterator<char>(legacy_input)),
                                std::istreambuf_iterator<char>());
    QCOMPARE(unchanged,
             "<!DOCTYPE config [<!ENTITY unsafe 'expanded'>]>"
             "<config><paths><path>&unsafe;</path></paths></config>");

    test::WriteBinaryFile(
        legacy_path,
        "<config><groups><group id=\"1\" name=\"First\">"
        "<dictionary>same</dictionary><dictionary>same</dictionary>"
        "</group></groups></config>");
    QVERIFY_EXCEPTION_THROWN(
        LoadOrMigrateConfiguration(current_path.string(), legacy_path.string(),
                                   "/cache/indexes"),
        std::runtime_error);
    QVERIFY(!std::filesystem::exists(current_path));

    test::WriteBinaryFile(
        legacy_path,
        "<config><groups><group id=\"1\" name=\"First\"/>"
        "<group id=\"1\" name=\"Duplicate\"/></groups></config>");
    QVERIFY_EXCEPTION_THROWN(
        LoadOrMigrateConfiguration(current_path.string(), legacy_path.string(),
                                   "/cache/indexes"),
        std::runtime_error);
    QVERIFY(!std::filesystem::exists(current_path));

    const std::string oversized_icon_data(64U * 1024U + 4U, 'A');
    test::WriteBinaryFile(
        legacy_path,
        "<config><groups><group id=\"1\" name=\"Icon\" iconData=\"" +
            oversized_icon_data + "\"/></groups></config>");
    QVERIFY_EXCEPTION_THROWN(
        LoadOrMigrateConfiguration(current_path.string(), legacy_path.string(),
                                   "/cache/indexes"),
        std::runtime_error);
    QVERIFY(!std::filesystem::exists(current_path));
}

void ApplicationServiceTest::RejectsMalformedConfiguration() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "core.conf";
    test::WriteBinaryFile(path, "unrecognized\n");

    QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                             std::runtime_error);
}

void ApplicationServiceTest::DiscoversAndQueriesARealFixture() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(
        root,
        {{"example",
          "<p>UTF-8: caf\xc3\xa9 <img src=\"images/pixel.png\"></p>"}},
        "h");
    const std::string image_data("\x89PNG\r\n\x1a\nfixture", 15);
    test::WriteStardictResource(root, "images/pixel.png", image_data);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.index_directory = (root / "indexes").string();
    auto facade = CreateDesktopFacade(configuration);
    auto& service = facade->GetDictionaryService();

    const auto catalog = service.GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{1});
    QCOMPARE(catalog.front().name, "Generated Test Dictionary");
    QCOMPARE(catalog.front().source,
             (root / "fixture.ifo").make_preferred().string());
    QCOMPARE(catalog.front().article_count, std::size_t{1});
    QCOMPARE(catalog.front().headword_count, std::size_t{1});

    LookupQuery query;
    query.text = "example";
    query.result_limit = 1U;
    const auto response = service.Lookup(query);

    QCOMPARE(response.errors.size(), std::size_t{0});
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QCOMPARE(entry.dictionary.id, catalog.front().id);
    QCOMPARE(entry.language.source_language, "en");
    QCOMPARE(entry.language.target_language, "en");
    QCOMPARE(entry.match.normalized_headword, "example");
    QVERIFY(entry.article.plain_text.find("caf\xc3\xa9") != std::string::npos);
    QVERIFY(entry.article.sanitized_html.has_value());
    QVERIFY(entry.article.sanitized_html->find("<script") == std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    QCOMPARE(entry.resources.front().media_type, "image/png");

    const auto resource = service.GetResource(entry.resources.front());
    const std::string loaded(reinterpret_cast<const char*>(resource.data()),
                             resource.size());
    QCOMPARE(loaded, image_data);
    QVERIFY(std::filesystem::exists(root / "indexes" /
                                    (catalog.front().id + ".gdidx")));
    QVERIFY(std::filesystem::exists(root / "indexes" /
                                    (catalog.front().id + ".gdfts")));

    query.text = "missing";
    const auto missing = service.Lookup(query);
    QVERIFY(missing.entries.empty());
    QVERIFY(missing.errors.empty());
}

void ApplicationServiceTest::
    RemovesStaleIndexesWhenACompanionBecomesUnavailable() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto dictionary_root = root / "dictionary";
    const auto index_root = root / "indexes";
    QVERIFY(std::filesystem::create_directory(dictionary_root));
    test::WriteStardictFixture(dictionary_root, {{"example", "article"}});

    CoreConfiguration configuration;
    configuration.dictionary_paths = {dictionary_root.string()};
    configuration.index_directory = index_root.string();
    auto service = CreateDictionaryService(configuration);
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{1});
    const auto headword_index = index_root / (catalog.front().id + ".gdidx");
    const auto full_text_index = index_root / (catalog.front().id + ".gdfts");
    QVERIFY(std::filesystem::is_regular_file(headword_index));
    QVERIFY(std::filesystem::is_regular_file(full_text_index));
    const auto unrelated = index_root / "operator-note.txt";
    test::WriteBinaryFile(unrelated, "preserve");
    QVERIFY(std::filesystem::create_directory(index_root / "foreign.gdidx"));
    service.reset();

    const auto companion = dictionary_root / "fixture.dict";
    const auto unavailable = dictionary_root / "fixture.dict.unavailable";
    std::filesystem::rename(companion, unavailable);
    auto unavailable_service = CreateDictionaryService(configuration);
    QVERIFY(unavailable_service->GetCatalog().empty());
    LookupQuery unavailable_query;
    unavailable_query.text = "example";
    const auto unavailable_lookup =
        unavailable_service->Lookup(unavailable_query);
    QVERIFY(unavailable_lookup.entries.empty());
    QVERIFY(unavailable_lookup.errors.empty());
    QVERIFY(!std::filesystem::exists(headword_index));
    QVERIFY(!std::filesystem::exists(full_text_index));
    QVERIFY(std::filesystem::is_regular_file(unrelated));
    QVERIFY(std::filesystem::is_directory(index_root / "foreign.gdidx"));
    unavailable_service.reset();

    std::filesystem::rename(unavailable, companion);
    auto recovered_service = CreateDictionaryService(configuration);
    QCOMPARE(recovered_service->GetCatalog().size(), std::size_t{1});
    QVERIFY(std::filesystem::is_regular_file(headword_index));
    QVERIFY(std::filesystem::is_regular_file(full_text_index));
}

void ApplicationServiceTest::CatalogUsesLegacyDiscoveryOrder() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto configured_first = root / "configured-z";
    const auto configured_second = root / "configured-a";

    const auto alpha = test::WriteDslFixture(configured_first / "alpha");
    const auto beta_bgl = test::WriteBglFixture(configured_first / "Beta");
    const auto beta_dsl = test::WriteDslFixture(configured_first / "Beta");
    const auto beta_mdict =
        test::WriteMdictContainer(configured_first / "Beta" / "fixture.mdx",
                                  "Fixture MDict", {{"word", "article"}});
    const auto first_top = test::WriteDslFixture(configured_first);
    const auto second_top = test::WriteDslFixture(configured_second);

    const auto sounds =
        formats::sounddir::test::WriteSoundDirectoryFixture(root / "sounds");
    const auto morphology = root / "morphology";
    const auto hunspell = test::WriteHunspellFixture(
        morphology, "en_US", "SET UTF-8\n", "1\nword\n");

    CoreConfiguration configuration;
    configuration.dictionary_paths = {configured_first.string(),
                                      configured_second.string()};
    configuration.sound_directories = {{sounds.string(), "Fixture sounds"}};
    configuration.morphology_dictionary_path = morphology.string();
    configuration.enabled_morphology_dictionary_ids = {"en_US"};

    const auto catalog = CreateDictionaryService(configuration)->GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{8U});
    const auto preferred = [](std::filesystem::path path) {
        return path.make_preferred().string();
    };
    const std::vector<std::string> expected_sources = {
        preferred(alpha),     preferred(beta_bgl),
        preferred(beta_dsl),  preferred(beta_mdict),
        preferred(first_top), preferred(second_top),
        preferred(sounds),    preferred(hunspell.affix_file),
    };
    std::vector<std::string> actual_sources;
    actual_sources.reserve(catalog.size());
    for (const auto& identity : catalog)
        actual_sources.push_back(identity.source);
    QCOMPARE(actual_sources, expected_sources);
}

void ApplicationServiceTest::RegistersOnlyEnabledMorphologyDictionaries() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteHunspellFixture(
        root, "en_US",
        "SET UTF-8\nTRY abcdefghijklmnopqrstuvwxyz\nREP 1\nREP cot cat\n",
        "1\ncat\n");
    test::WriteHunspellFixture(root, "disabled", "SET UTF-8\n", "1\nword\n");
    test::WriteStardictFixture(root, {{"star", "StarDict remains active"}});

    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.morphology_dictionary_path = root.string();
    configuration.enabled_morphology_dictionary_ids = {"en_US"};

    const auto configuration_path = root / "core.conf";
    SaveConfiguration(configuration_path.string(), configuration);
    const auto round_trip = LoadConfiguration(configuration_path.string());
    QCOMPARE(round_trip.morphology_dictionary_path, root.string());
    QCOMPARE(round_trip.enabled_morphology_dictionary_ids,
             std::vector<std::string>{"en_US"});

    auto service = CreateDictionaryService(round_trip);
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{2});
    const auto morphology =
        std::find_if(catalog.begin(), catalog.end(), [](const auto& identity) {
            return identity.id.rfind("hunspell-", 0U) == 0U;
        });
    QVERIFY(morphology != catalog.end());
    QCOMPARE(morphology->name, std::string("English (US) Morphology"));
    QCOMPARE(morphology->source,
             (root / "en_US.aff").make_preferred().string());
    QCOMPARE(morphology->headword_count, std::size_t{1});
    QVERIFY(
        std::any_of(catalog.begin(), catalog.end(), [](const auto& identity) {
            return identity.id.rfind("stardict-", 0U) == 0U;
        }));

    auto rebuilt = CreateDictionaryService(round_trip);
    const auto rebuilt_catalog = rebuilt->GetCatalog();
    const auto rebuilt_morphology =
        std::find_if(rebuilt_catalog.begin(), rebuilt_catalog.end(),
                     [](const auto& identity) {
                         return identity.id.rfind("hunspell-", 0U) == 0U;
                     });
    QVERIFY(rebuilt_morphology != rebuilt_catalog.end());
    QCOMPARE(rebuilt_morphology->id, morphology->id);
    QCOMPARE(rebuilt_morphology->source, morphology->source);

    LookupQuery query;
    query.text = "cot";
    query.result_limit = 1U;
    const auto response = service->Lookup(query);
    QCOMPARE(response.errors.size(), std::size_t{0});
    QCOMPARE(response.entries.size(), std::size_t{1});
    QCOMPARE(response.entries[0].dictionary.id, morphology->id);
    QVERIFY(response.entries[0].article.plain_text.find("cat") !=
            std::string::npos);
    QVERIFY(response.entries[0].article.sanitized_html.has_value());
    QVERIFY(response.entries[0].article.sanitized_html->find(
                "goldendict://lookup/cat") != std::string::npos);

    SuggestionQuery suggestion;
    suggestion.text = "ca";
    QVERIFY(service->Suggest(suggestion).suggestions.empty());

    const CancelledToken cancelled;
    const auto cancelled_response = service->Lookup(query, &cancelled);
    QVERIFY(std::any_of(cancelled_response.errors.begin(),
                        cancelled_response.errors.end(), [](const auto& error) {
                            return error.code == LookupErrorCode::kCancelled;
                        }));

    configuration.enabled_morphology_dictionary_ids.clear();
    const auto disabled_catalog =
        CreateDictionaryService(configuration)->GetCatalog();
    QCOMPARE(disabled_catalog.size(), std::size_t{1});
    QVERIFY(disabled_catalog.front().id.rfind("stardict-", 0U) == 0U);

    configuration.enabled_morphology_dictionary_ids = {"missing"};
    const auto unavailable =
        CreateDictionaryService(configuration)->Lookup(query);
    QVERIFY(std::any_of(unavailable.errors.begin(), unavailable.errors.end(),
                        [](const auto& error) {
                            return error.dictionary_id == "missing" &&
                                   error.code ==
                                       LookupErrorCode::kDictionaryUnavailable;
                        }));
}

void ApplicationServiceTest::AppliesIndependentFullTextQueryLimits() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    std::filesystem::create_directories(root / "first");
    std::filesystem::create_directories(root / "second");
    test::WriteStardictFixture(root / "first",
                               {{"First one", "shared first one"},
                                {"First two", "shared first two"},
                                {"First three", "shared first three"}});
    test::WriteStardictFixture(root / "second",
                               {{"Second one", "shared second one"},
                                {"Second two", "shared second two"},
                                {"Second three", "shared second three"}});

    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.index_directory = (root / "indexes").string();
    auto service = CreateDictionaryService(configuration);
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{2});

    FullTextQuery query;
    QCOMPARE(query.result_limit, std::size_t{20});
    QVERIFY(query.maximum_articles_per_dictionary.has_value());
    QCOMPARE(*query.maximum_articles_per_dictionary, std::size_t{100});
    query.text = "shared";
    query.dictionary_filter_active = true;
    query.dictionary_ids = {catalog[0].id};
    query.result_limit = 6U;
    query.maximum_articles_per_dictionary = 2U;
    const auto one_dictionary = service->SearchFullText(query);
    QCOMPARE(one_dictionary.results.size(), std::size_t{2});
    QVERIFY(std::all_of(one_dictionary.results.begin(),
                        one_dictionary.results.end(), [&](const auto& result) {
                            return result.dictionary.id == catalog[0].id;
                        }));

    query.dictionary_ids = {catalog[0].id, catalog[1].id};
    query.result_limit = 6U;
    const auto multiple_dictionaries = service->SearchFullText(query);
    QCOMPARE(multiple_dictionaries.results.size(), std::size_t{4});
    QCOMPARE(multiple_dictionaries.results[0].dictionary.id, catalog[0].id);
    QCOMPARE(multiple_dictionaries.results[1].dictionary.id, catalog[0].id);
    QCOMPARE(multiple_dictionaries.results[2].dictionary.id, catalog[1].id);
    QCOMPARE(multiple_dictionaries.results[3].dictionary.id, catalog[1].id);

    query.result_limit = 3U;
    query.maximum_articles_per_dictionary = 100000U;
    const auto global_precedence = service->SearchFullText(query);
    QCOMPARE(global_precedence.results.size(), std::size_t{3});
    QCOMPARE(global_precedence.results[0].dictionary.id, catalog[0].id);
    QCOMPARE(global_precedence.results[1].dictionary.id, catalog[0].id);
    QCOMPARE(global_precedence.results[2].dictionary.id, catalog[0].id);

    query.result_limit = 4U;
    query.maximum_articles_per_dictionary = std::nullopt;
    const auto unlimited_per_dictionary = service->SearchFullText(query);
    QCOMPARE(unlimited_per_dictionary.results.size(), std::size_t{4});
    QCOMPARE(unlimited_per_dictionary.results[0].dictionary.id, catalog[0].id);
    QCOMPARE(unlimited_per_dictionary.results[1].dictionary.id, catalog[0].id);
    QCOMPARE(unlimited_per_dictionary.results[2].dictionary.id, catalog[0].id);
    QCOMPARE(unlimited_per_dictionary.results[3].dictionary.id, catalog[1].id);

    query.result_limit = 1U;
    query.maximum_articles_per_dictionary = 1U;
    QVERIFY(service->SearchFullText(query).errors.empty());
    query.maximum_articles_per_dictionary = 100000U;
    QVERIFY(service->SearchFullText(query).errors.empty());
    query.maximum_articles_per_dictionary = 0U;
    const auto zero_dictionary_limit = service->SearchFullText(query);
    QCOMPARE(zero_dictionary_limit.errors.size(), std::size_t{1});
    QCOMPARE(zero_dictionary_limit.errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    query.maximum_articles_per_dictionary = 100001U;
    const auto excessive_dictionary_limit = service->SearchFullText(query);
    QCOMPARE(excessive_dictionary_limit.errors.size(), std::size_t{1});
    QCOMPARE(excessive_dictionary_limit.errors.front().code,
             FullTextErrorCode::kInvalidQuery);

    query.maximum_articles_per_dictionary = std::nullopt;
    query.result_limit = 1000000U;
    QVERIFY(service->SearchFullText(query).errors.empty());
    query.result_limit = 0U;
    QCOMPARE(service->SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    query.result_limit = kMaximumFullTextResults + 1U;
    QCOMPARE(service->SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);

    query.result_limit = 1U;
    QVERIFY(service->SearchFullText(query).errors.empty());
    query.maximum_articles_per_dictionary = 1U;
    query.result_limit = kMaximumFullTextResults;
    query.dictionary_ids.push_back("unavailable");
    const auto preserved_error = service->SearchFullText(query);
    QCOMPARE(preserved_error.results.size(), std::size_t{2});
    QCOMPARE(preserved_error.errors.size(), std::size_t{1});
    QCOMPARE(preserved_error.errors.front().code,
             FullTextErrorCode::kDictionaryUnavailable);
    QVERIFY(preserved_error.partial);

    CancelledToken cancelled;
    const auto cancelled_response = service->SearchFullText(query, &cancelled);
    QVERIFY(cancelled_response.results.empty());
    QCOMPARE(cancelled_response.errors.front().code,
             FullTextErrorCode::kCancelled);
}

void ApplicationServiceTest::
    SearchesTwelveLocalFormatsFullTextWithMixedFormatErrors() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    std::filesystem::create_directories(root / "first");
    std::filesystem::create_directories(root / "sdict");
    std::filesystem::create_directories(root / "xdxf");
    std::filesystem::create_directories(root / "dictd");
    std::filesystem::create_directories(root / "gls");
    std::filesystem::create_directories(root / "dsl");
    std::filesystem::create_directories(root / "aard");
    std::filesystem::create_directories(root / "bgl");
    std::filesystem::create_directories(root / "slob");
    std::filesystem::create_directories(root / "zim");
    std::filesystem::create_directories(root / "mdict");
    std::filesystem::create_directories(root / "epwing");
    test::WriteStardictFixture(root / "first",
                               {{"Alpha", "shared searchable first"}});
    test::WriteSdictFixture(root / "sdict",
                            {{"Beta", "shared searchable second"}});
    test::WriteXdxfFixture(
        root / "xdxf",
        {{{"Gamma", "Gamma alias"}, "<def>shared searchable third</def>"}});
    test::WriteDictdFixture(root / "dictd",
                            {{"Delta", "shared searchable fourth", ""}});
    test::WriteGlsFixture(root / "gls", {{{"Epsilon", "Epsilon alias"},
                                          "<b>shared searchable fifth</b>"}});
    test::WriteDslTextFixture(
        root / "dsl", "Zet(a)\n~ alias\n\t[b]shared searchable sixth[/b]\n");
    test::WriteAardFixture(root / "aard", "fixture.aar", false, false, false,
                           "[\"<b>shared searchable seventh</b>"
                           "<a href=\\\"w:alias\\\">alias</a>\"]");
    test::WriteBglFullTextFixture(root / "bgl",
                                  "<b>shared searchable eighth</b>");
    test::WriteSlobFullTextFixture(root / "slob",
                                   "<b>shared searchable ninth</b>");
    test::WriteZimFixture(root / "zim", "fixture.zim", 1U, false,
                          "<b>shared searchable tenth</b>");
    test::WriteMdictContainer(
        root / "mdict" / "fixture.mdx", "Fixture MDict",
        {{"Lambda", "<b>shared searchable eleventh</b>"}});
    test::WriteEpwingFixture(root / "epwing", true,
                             "shared searchable twelfth");
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.index_directory = (root / "indexes").string();
    std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources;
    runtime_sources.push_back(std::make_unique<InspectionRuntimeSource>());
    auto service_candidate =
        application::CreateDictionaryServiceActivationCandidate(
            configuration, std::move(runtime_sources));
    auto service = std::move(service_candidate.service);
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), 13U);
    QCOMPARE(std::count_if(catalog.begin(), catalog.end(),
                           [](const auto& item) {
                               return item.supports_full_text_search;
                           }),
             12);
    const auto unsupported_catalog =
        std::find_if(catalog.begin(), catalog.end(),
                     [](const auto& item) { return item.id == "inspection"; });
    QVERIFY(unsupported_catalog != catalog.end());
    QVERIFY(!unsupported_catalog->supports_full_text_search);
    const auto aard = std::find_if(
        catalog.begin(), catalog.end(),
        [](const auto& item) { return item.id.rfind("aard-", 0U) == 0U; });
    QVERIFY(aard != catalog.end());
    QVERIFY(service_candidate.activation.SubmitOnceWithDefaults());
    QVERIFY(WaitForFullTextLifecycleState(
        *service, aard->id, dictionary::FullTextIndexLifecycleState::kCurrent));
    auto facade = CreateDesktopFacade(configuration);

    FullTextQuery query;
    query.text = "shared";
    query.result_limit = 1U;
    const auto bounded = service->SearchFullText(query);
    QCOMPARE(bounded.results.size(), 1U);
    QCOMPARE(bounded.errors.size(), 1U);
    QCOMPARE(bounded.errors.front().code, FullTextErrorCode::kUnsupported);
    QCOMPARE(bounded.errors.front().dictionary_id, std::string("inspection"));
    QVERIFY(bounded.partial);

    query.dictionary_filter_active = true;
    query.dictionary_ids.clear();
    for (const auto& item : catalog) {
        query.dictionary_ids.push_back(item.id);
    }
    query.dictionary_ids.push_back("unavailable");
    query.result_limit = kMaximumFullTextResults;
    const auto filtered = service->SearchFullText(query);
    QCOMPARE(filtered.results.size(), 12U);
    std::vector<std::string> adapted_ids;
    for (const auto& result : filtered.results) {
        QVERIFY(result.dictionary.supports_full_text_search);
        const auto catalog_identity = std::find_if(
            catalog.begin(), catalog.end(),
            [&](const auto& item) { return item.id == result.dictionary.id; });
        QVERIFY(catalog_identity != catalog.end());
        QCOMPARE(result.dictionary.supports_full_text_search,
                 catalog_identity->supports_full_text_search);
        adapted_ids.push_back(result.dictionary.id);
        const ExactArticleTarget target{result.dictionary.id,
                                        result.document_id};
        const auto resolved = facade->ResolveExactArticleTarget(target);
        QVERIFY(resolved);
        QCOMPARE(resolved.dictionary.id, result.dictionary.id);
        QCOMPARE(resolved.document_id, result.document_id);
        QCOMPARE(resolved.headword, result.headword);
    }
    QCOMPARE(facade
                 ->ResolveExactArticleTarget(
                     {"unavailable", filtered.results.front().document_id})
                 .error,
             ExactArticleTargetError::kDictionaryUnavailable);
    QCOMPARE(facade
                 ->ResolveExactArticleTarget(
                     {filtered.results.front().dictionary.id, "stale-document"})
                 .error,
             ExactArticleTargetError::kDocumentNotFound);
    QCOMPARE(facade->ResolveExactArticleTarget({"", "document"}).error,
             ExactArticleTargetError::kInvalidTarget);

    TabNavigationState exact_navigation;
    exact_navigation.kind = TabNavigationKind::kLookup;
    exact_navigation.query = filtered.results.front().headword;
    exact_navigation.group_id = 17U;
    exact_navigation.title = "Exact title";
    exact_navigation.dictionary_filter_active = true;
    exact_navigation.dictionary_ids = adapted_ids;
    exact_navigation.exact_target =
        ExactArticleTarget{filtered.results.front().dictionary.id,
                           filtered.results.front().document_id};
    const auto initial_session = facade->ExportArticleTabSession();
    QVERIFY(facade->OpenArticleTab(exact_navigation, TabOpenPolicy::kCurrentTab,
                                   TabActivationPolicy::kActivate));
    const auto exact_session = facade->ExportArticleTabSession();
    QCOMPARE(exact_session.tabs.front().history.back(), exact_navigation);
    QVERIFY(facade->GoBackInArticleTab(exact_session.active_tab_id));
    QVERIFY(facade->GoForwardInArticleTab(exact_session.active_tab_id));
    QCOMPARE(facade->GetArticleTabsState().tabs.front().navigation,
             exact_navigation);

    auto restored = CreateDesktopFacade(configuration);
    QVERIFY(restored->RestoreArticleTabSession(exact_session));
    QCOMPARE(restored->ExportArticleTabSession(), exact_session);

    auto invalid_navigation = exact_navigation;
    invalid_navigation.exact_target->document_id = "stale-document";
    const auto before_invalid = facade->ExportArticleTabSession();
    QCOMPARE(
        facade
            ->OpenArticleTab(invalid_navigation, TabOpenPolicy::kCurrentTab,
                             TabActivationPolicy::kActivate)
            .error,
        TabOperationError::kExactTargetDocumentNotFound);
    QCOMPARE(facade->ExportArticleTabSession(), before_invalid);

    auto invalid_session = exact_session;
    invalid_session.tabs.front().history.back() = invalid_navigation;
    const auto before_restore = restored->ExportArticleTabSession();
    QCOMPARE(restored->RestoreArticleTabSession(invalid_session).error,
             TabOperationError::kExactTargetDocumentNotFound);
    QCOMPARE(restored->ExportArticleTabSession(), before_restore);
    QVERIFY(!(initial_session == exact_session));
    QCOMPARE(std::count_if(adapted_ids.begin(), adapted_ids.end(),
                           [](const auto& id) {
                               return id.rfind("stardict-", 0U) == 0U;
                           }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("dictd-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("sdict-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("xdxf-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("gls-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("dsl-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("aard-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("bgl-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("slob-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("zim-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("mdict-", 0U) == 0U; }),
             1);
    QCOMPARE(std::count_if(
                 adapted_ids.begin(), adapted_ids.end(),
                 [](const auto& id) { return id.rfind("epwing-", 0U) == 0U; }),
             1);
    std::vector<std::string> expected_adapted_ids;
    for (const auto& item : catalog) {
        if (std::find(adapted_ids.begin(), adapted_ids.end(), item.id) !=
            adapted_ids.end()) {
            expected_adapted_ids.push_back(item.id);
        }
    }
    QCOMPARE(adapted_ids, expected_adapted_ids);
    const auto repeated = service->SearchFullText(query);
    QCOMPARE(repeated.results[0].dictionary.id,
             filtered.results[0].dictionary.id);
    QCOMPARE(repeated.results[1].dictionary.id,
             filtered.results[1].dictionary.id);
    QCOMPARE(repeated.results[2].dictionary.id,
             filtered.results[2].dictionary.id);
    QCOMPARE(repeated.results[3].dictionary.id,
             filtered.results[3].dictionary.id);
    QCOMPARE(repeated.results[4].dictionary.id,
             filtered.results[4].dictionary.id);
    QCOMPARE(repeated.results[5].dictionary.id,
             filtered.results[5].dictionary.id);
    QCOMPARE(repeated.results[6].dictionary.id,
             filtered.results[6].dictionary.id);
    QCOMPARE(repeated.results[7].dictionary.id,
             filtered.results[7].dictionary.id);
    QCOMPARE(repeated.results[8].dictionary.id,
             filtered.results[8].dictionary.id);
    QCOMPARE(repeated.results[9].dictionary.id,
             filtered.results[9].dictionary.id);
    QCOMPARE(repeated.results[10].dictionary.id,
             filtered.results[10].dictionary.id);
    QCOMPARE(repeated.results[11].dictionary.id,
             filtered.results[11].dictionary.id);
    QCOMPARE(filtered.results.front().match.mode, MatchMode::kFullText);
    QCOMPARE(filtered.results.front().matches.size(), 1U);
    QCOMPARE(filtered.errors.size(), 2U);
    QCOMPARE(filtered.errors[0].code, FullTextErrorCode::kUnsupported);
    QCOMPARE(filtered.errors[0].dictionary_id, std::string("inspection"));
    QCOMPARE(filtered.errors[1].code,
             FullTextErrorCode::kDictionaryUnavailable);
    QCOMPARE(filtered.errors[1].dictionary_id, std::string("unavailable"));

    query.dictionary_ids = adapted_ids;
    query.text = "absent-from-adapted-dictionaries";
    const auto adapted_no_match = service->SearchFullText(query);
    QVERIFY(adapted_no_match.results.empty());
    QVERIFY(adapted_no_match.errors.empty());

    query.dictionary_ids.clear();
    query.text = "shared";
    const auto active_empty = service->SearchFullText(query);
    QVERIFY(active_empty.results.empty());
    QVERIFY(active_empty.errors.empty());
    QVERIFY(!active_empty.partial);

    query.dictionary_filter_active = false;
    query.dictionary_ids.clear();
    CancelledToken cancelled;
    const auto cancelled_response = service->SearchFullText(query, &cancelled);
    QVERIFY(cancelled_response.results.empty());
    QCOMPARE(cancelled_response.errors.size(), 1U);
    QCOMPARE(cancelled_response.errors.front().code,
             FullTextErrorCode::kCancelled);

    query.dictionary_filter_active = true;
    query.dictionary_ids = {bounded.results.front().dictionary.id};
    query.timeout = std::chrono::milliseconds(1);
    SlowToken slow;
    const auto expired = service->SearchFullText(query, &slow);
    QVERIFY(expired.results.empty());
    QCOMPARE(expired.errors.size(), 1U);
    QCOMPARE(expired.errors.front().code, FullTextErrorCode::kDeadlineExceeded);
    QCOMPARE(expired.errors.front().dictionary_id,
             bounded.results.front().dictionary.id);

    const auto dsl_id = *std::find_if(
        adapted_ids.begin(), adapted_ids.end(),
        [](const auto& id) { return id.rfind("dsl-", 0U) == 0U; });
    service.reset();
    const auto dsl_index = root / "indexes" / (dsl_id + ".gdfts");
    QVERIFY(std::filesystem::remove(dsl_index));
    QVERIFY(std::filesystem::create_directory(dsl_index));
    runtime_sources.clear();
    runtime_sources.push_back(std::make_unique<InspectionRuntimeSource>());
    service =
        CreateDictionaryService(configuration, std::move(runtime_sources));
    const auto dsl_result =
        std::find_if(filtered.results.begin(), filtered.results.end(),
                     [&dsl_id](const auto& result) {
                         return result.dictionary.id == dsl_id;
                     });
    QVERIFY(dsl_result != filtered.results.end());
    auto unavailable_facade = CreateDesktopFacade(configuration);
    QCOMPARE(unavailable_facade
                 ->ResolveExactArticleTarget({dsl_id, dsl_result->document_id})
                 .error,
             ExactArticleTargetError::kDictionaryUnavailable);
    const auto replacement_catalog = service->GetCatalog();
    QCOMPARE(replacement_catalog.size(), catalog.size());
    QCOMPARE(
        std::count_if(
            replacement_catalog.begin(), replacement_catalog.end(),
            [](const auto& item) { return item.supports_full_text_search; }),
        12);
    const auto replaced_dsl =
        std::find_if(replacement_catalog.begin(), replacement_catalog.end(),
                     [&dsl_id](const auto& item) { return item.id == dsl_id; });
    QVERIFY(replaced_dsl != replacement_catalog.end());
    QVERIFY(replaced_dsl->supports_full_text_search);
    query = {};
    query.text = "shared";
    const auto contained = service->SearchFullText(query);
    QCOMPARE(contained.results.size(), std::size_t{11});
    QVERIFY(std::any_of(contained.results.begin(), contained.results.end(),
                        [](const auto& result) {
                            return result.dictionary.id.rfind("aard-", 0U) ==
                                   0U;
                        }));
    QVERIFY(std::any_of(contained.errors.begin(), contained.errors.end(),
                        [&dsl_id](const auto& error) {
                            return error.dictionary_id == dsl_id &&
                                   error.code == FullTextErrorCode::kInternal;
                        }));
}

void ApplicationServiceTest::EnumeratesStarDictHeadwordsWithStableCursors() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root, {{"zebra", "one"},
                                      {"Apple", "two"},
                                      {"Apple", "duplicate"},
                                      {"apple", "three"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().supports_headword_enumeration);

    HeadwordEnumerationQuery query;
    query.dictionary_id = catalog.front().id;
    query.page_size = 2U;
    const auto first = service->EnumerateHeadwords(query);
    QVERIFY(!first.error.has_value());
    QCOMPARE(first.headwords, (std::vector<std::string>{"Apple", "apple"}));
    QVERIFY(!first.complete);
    QVERIFY(!first.next_cursor.empty());

    query.cursor = first.next_cursor;
    const auto second = service->EnumerateHeadwords(query);
    QVERIFY(!second.error.has_value());
    QCOMPARE(second.headwords, (std::vector<std::string>{"zebra"}));
    QVERIFY(second.complete);
    QVERIFY(second.next_cursor.empty());

    query.cursor.back() = query.cursor.back() == '0' ? '1' : '0';
    const auto malformed = service->EnumerateHeadwords(query);
    QVERIFY(malformed.error.has_value());
    QCOMPARE(malformed.error->code,
             HeadwordEnumerationErrorCode::kMalformedCursor);

    auto replacement = CreateDictionaryService(configuration);
    query.cursor = first.next_cursor;
    const auto stale = replacement->EnumerateHeadwords(query);
    QVERIFY(stale.error.has_value());
    QCOMPARE(stale.error->code, HeadwordEnumerationErrorCode::kStaleCursor);

    const CancelledToken cancelled;
    query.cursor.clear();
    const auto cancelled_page = service->EnumerateHeadwords(query, &cancelled);
    QVERIFY(cancelled_page.error.has_value());
    QCOMPARE(cancelled_page.error->code,
             HeadwordEnumerationErrorCode::kCancelled);
}

void ApplicationServiceTest::ExportsCompleteHeadwordListsAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    std::filesystem::create_directory(root / "dictionary");
    test::WriteStardictFixture(root / "dictionary",
                               {{"zebra", "one"},
                                {"Apple", "two"},
                                {"Apple", "exact duplicate"},
                                {"apple", "three"},
                                {"caf\xc3\xa9", "unicode"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {(root / "dictionary").string()};
    auto service = CreateDictionaryService(configuration);
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{1});

    const auto destination = root / "headwords.txt";
    test::WriteBinaryFile(destination, "previous destination");
    HeadwordExportRequest request;
    request.dictionary_id = catalog.front().id;
    request.destination_path = destination.string();
    request.page_size = 2U;
    auto operation = StartHeadwordExport(*service, request);
    const auto result = operation->Await();
    QVERIFY(result);
    QCOMPARE(result.exported_headwords, std::size_t{4});
    std::ifstream input(destination, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(input)), {});
    QCOMPARE(contents, std::string("\xef\xbb\xbf") +
                           "Apple\napple\ncaf\xc3\xa9\nzebra\n");

    const auto collision = root / ".headwords.txt.goldendict-export-1.tmp";
    test::WriteBinaryFile(collision, "collision");
    request.dictionary_id = "missing";
    auto failed = StartHeadwordExport(*service, request);
    const auto failure = failed->Await();
    QCOMPARE(failure.error, HeadwordExportErrorCode::kDictionaryUnavailable);
    std::ifstream preserved(destination, std::ios::binary);
    const std::string preserved_contents(
        (std::istreambuf_iterator<char>(preserved)), {});
    QCOMPARE(preserved_contents, contents);

    request.dictionary_id = catalog.front().id;
    request.destination_path = (root / "missing" / "headwords.txt").string();
    auto unopened = StartHeadwordExport(*service, request);
    QCOMPARE(unopened->Await().error, HeadwordExportErrorCode::kOpenFailed);

    request.destination_path = destination.string();
    request.timeout = std::chrono::milliseconds::zero();
    auto expired = StartHeadwordExport(*service, request);
    QCOMPARE(expired->Await().error, HeadwordExportErrorCode::kInvalidRequest);
    std::ifstream still_preserved(destination, std::ios::binary);
    QCOMPARE(std::string((std::istreambuf_iterator<char>(still_preserved)), {}),
             contents);
}

void ApplicationServiceTest::ReturnsCanonicalFoldedMatchInformation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root,
                               {{"Caf\xc3\xa9 au lait", "folded definition"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "CAFE AU LAIT";

    const auto sensitive = service->Lookup(query);

    QVERIFY(sensitive.errors.empty());
    QVERIFY(sensitive.entries.empty());

    configuration.preferences.ignore_diacritics = true;
    service = CreateDictionaryService(configuration);
    const auto response = service->Lookup(query);

    QCOMPARE(response.errors.size(), std::size_t{0});
    QCOMPARE(response.entries.size(), std::size_t{1});
    QCOMPARE(response.entries.front().headword, "Caf\xc3\xa9 au lait");
    QCOMPARE(response.entries.front().match.requested_headword, "CAFE AU LAIT");
    QCOMPARE(response.entries.front().match.normalized_headword, "cafeaulait");
}

void ApplicationServiceTest::ReturnsMdictSourceHeadword() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteMdictContainer(root / "fixture.mdx", "Fixture MDict",
                              {{"Accordion", "source headword definition"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "ACCORDION";

    const auto response = service->Lookup(query);

    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    QCOMPARE(response.entries.front().headword, "Accordion");
    QCOMPARE(response.entries.front().match.requested_headword, "ACCORDION");
    QCOMPARE(response.entries.front().match.normalized_headword, "accordion");
}

void ApplicationServiceTest::
    AppliesLegacySynonymSearchWithoutChangingSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    std::filesystem::create_directories(root / "synonyms");
    std::filesystem::create_directories(root / "primary");
    const auto synonym_info = test::WriteStardictFixture(
        root / "synonyms", {{"primary", "synonym source article"}});
    test::WriteStardictSynonyms(synonym_info, {{"alias", 0U}});
    test::WriteStardictFixture(root / "primary",
                               {{"primary", "cross-dictionary article"}});

    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.preferences.synonym_search_enabled = false;
    auto service = CreateDictionaryService(configuration);
    LookupQuery lookup;
    lookup.text = "alias";
    const auto disabled = service->Lookup(lookup);
    QVERIFY(disabled.errors.empty());
    QCOMPARE(disabled.entries.size(), std::size_t{1});
    QCOMPARE(disabled.entries.front().article.plain_text,
             "synonym source article");

    SuggestionQuery suggestion;
    suggestion.text = "ali";
    const auto disabled_suggestions = service->Suggest(suggestion);
    QCOMPARE(disabled_suggestions.suggestions.size(), std::size_t{1});
    QCOMPARE(disabled_suggestions.suggestions.front().headword, "alias");

    configuration.preferences.synonym_search_enabled = true;
    service = CreateDictionaryService(configuration);
    const auto enabled = service->Lookup(lookup);
    QVERIFY(enabled.errors.empty());
    QCOMPARE(enabled.entries.size(), std::size_t{2});
    std::set<std::string> articles;
    for (const auto& entry : enabled.entries) {
        articles.insert(entry.article.plain_text);
        QCOMPARE(entry.match.requested_headword, "alias");
        QCOMPARE(entry.match.mode, MatchMode::kExact);
    }
    QCOMPARE(articles, (std::set<std::string>{"cross-dictionary article",
                                              "synonym source article"}));

    const auto enabled_suggestions = service->Suggest(suggestion);
    QCOMPARE(enabled_suggestions.suggestions.size(), std::size_t{1});
    QCOMPARE(enabled_suggestions.suggestions.front().headword, "alias");
}

void ApplicationServiceTest::EnforcesRuntimeDiacriticCapability() {
    CoreConfiguration configuration;
    configuration.preferences.ignore_diacritics = true;
    std::vector<std::unique_ptr<RuntimeDictionarySource>> sources;
    sources.push_back(std::make_unique<DiacriticRuntimeSource>(false));
    sources.push_back(std::make_unique<DiacriticRuntimeSource>(true));
    auto service = CreateDictionaryService(configuration, std::move(sources));
    LookupQuery query;
    query.text = "cafe";

    const auto response = service->Lookup(query);

    QCOMPARE(response.entries.size(), std::size_t{1});
    QCOMPARE(response.entries.front().article.plain_text, "runtime definition");
    QCOMPARE(response.errors.size(), std::size_t{1});
    QCOMPARE(response.errors.front().dictionary_id, "diacritic-unsupported");
    QCOMPARE(response.errors.front().code, LookupErrorCode::kUnsupported);
    QVERIFY(response.partial);
}

void ApplicationServiceTest::ReturnsRankedPrefixMatches() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root, {{"cafeteria", "long"},
                                      {"caf\xc3\xa9 noir", "medium"},
                                      {"Caf\xc3\xa9", "exact"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "CAFE";
    query.match_mode = MatchMode::kPrefix;

    const auto response = service->Lookup(query);

    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{3});
    QCOMPARE(response.entries[0].match.normalized_headword, "cafe");
    QCOMPARE(response.entries[0].match.mode, MatchMode::kExact);
    QCOMPARE(response.entries[0].match.score, 1.0);
    QCOMPARE(response.entries[1].match.normalized_headword, "cafenoir");
    QCOMPARE(response.entries[1].match.mode, MatchMode::kPrefix);
    QCOMPARE(response.entries[1].match.score, 0.5);
    QVERIFY(response.entries[2].match.score < response.entries[1].match.score);
}

void ApplicationServiceTest::ReturnsLightweightHeadwordSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root, {{"cafeteria", "long article"},
                                      {"caf\xc3\xa9 noir", "medium article"},
                                      {"Caf\xc3\xa9", "exact article"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    SuggestionQuery query;
    query.text = "CAFE";
    query.result_limit = 2U;

    const auto response = service->Suggest(query);

    QVERIFY(response.errors.empty());
    QCOMPARE(response.suggestions.size(), std::size_t{2});
    QCOMPARE(response.suggestions[0].headword, "Caf\xc3\xa9");
    QCOMPARE(response.suggestions[0].match.mode, MatchMode::kExact);
    QCOMPARE(response.suggestions[0].match.score, 1.0);
    QCOMPARE(response.suggestions[1].headword, "caf\xc3\xa9 noir");
    QCOMPARE(response.suggestions[1].match.mode, MatchMode::kPrefix);
    QCOMPARE(response.suggestions[1].match.normalized_headword, "cafenoir");
    QCOMPARE(response.suggestions[1].dictionary.name,
             "Generated Test Dictionary");

    const CancelledToken cancelled;
    const auto cancelled_response = service->Suggest(query, &cancelled);
    QVERIFY(cancelled_response.suggestions.empty());
    QCOMPARE(cancelled_response.errors.size(), std::size_t{1});
    QCOMPARE(cancelled_response.errors.front().code,
             LookupErrorCode::kCancelled);

    query.dictionary_ids = {"unavailable"};
    query.dictionary_filter_active = true;
    const auto unavailable = service->Suggest(query);
    QVERIFY(unavailable.suggestions.empty());
    QCOMPARE(unavailable.errors.size(), std::size_t{1});
    QCOMPARE(unavailable.errors.front().code,
             LookupErrorCode::kDictionaryUnavailable);
}

void ApplicationServiceTest::RanksMdictWordStartSuggestionsLikeLegacyProduct() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteMdictContainer(root / "fixture.mdx", "Fixture MDict",
                              {{"accord", "exact"},
                               {"accordable", "prefix"},
                               {"accordance", "prefix"},
                               {"accordances", "prefix"},
                               {"in accord", "word start"},
                               {"in accordance with", "word start"},
                               {"of your own accord", "word start"},
                               {"with one accord", "word start"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    SuggestionQuery query;
    query.text = "accor";
    query.result_limit = 8U;

    const auto response = service->Suggest(query);

    QVERIFY(response.errors.empty());
    std::vector<std::string> headwords;
    for (const auto& suggestion : response.suggestions)
        headwords.push_back(suggestion.headword);
    QCOMPARE(headwords, (std::vector<std::string>{
                            "accord", "accordable", "accordance", "accordances",
                            "in accord", "in accordance with",
                            "of your own accord", "with one accord"}));

    query.text = "accord";
    const auto whole_word_response = service->Suggest(query);
    QVERIFY(whole_word_response.errors.empty());
    headwords.clear();
    for (const auto& suggestion : whole_word_response.suggestions)
        headwords.push_back(suggestion.headword);
    QCOMPARE(headwords, (std::vector<std::string>{
                            "accord", "in accord", "with one accord",
                            "of your own accord", "accordable", "accordance",
                            "accordances", "in accordance with"}));
}

void ApplicationServiceTest::RanksAllLegacyWordFinderSuggestionCategories() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteMdictContainer(root / "fixture.mdx", "Fixture MDict",
                              {{"strasse", "exact"},
                               {"straße", "full case"},
                               {"strássé", "diacritics"},
                               {"s.t.r.a.s.s.e", "punctuation"},
                               {"s t r a s s e", "whitespace"},
                               {"a strasse", "inside at two"},
                               {"with strasse", "inside at five"},
                               {"a strássé", "diacritic inside"},
                               {"a s.t.r.a.s.s.e", "punctuation inside"},
                               {"strassex", "prefix"},
                               {"strásséx", "diacritic prefix"},
                               {"s.t.r.a.s.s.ex", "punctuation prefix"},
                               {"s t r a s s ex", "whitespace prefix"},
                               {"in strassex", "worst"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    SuggestionQuery query;
    query.text = "strasse";
    query.result_limit = 14U;

    const auto response = service->Suggest(query);

    QVERIFY(response.errors.empty());
    std::vector<std::string> headwords;
    for (const auto& suggestion : response.suggestions)
        headwords.push_back(suggestion.headword);
    QCOMPARE(headwords,
             (std::vector<std::string>{
                 "strasse", "straße", "strássé", "s.t.r.a.s.s.e",
                 "s t r a s s e", "a strasse", "with strasse", "a strássé",
                 "a s.t.r.a.s.s.e", "strassex", "strásséx", "s.t.r.a.s.s.ex",
                 "s t r a s s ex", "in strassex"}));
}

void ApplicationServiceTest::FiltersBoundedHeadwordSuggestionsWithWildcards() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root, {{"Cafeteria", "long article"},
                                      {"caf\xc3\xa9 noir", "medium article"},
                                      {"Caf\xc3\xa9", "exact article"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    SuggestionQuery query;
    query.text = "caf*noir";
    query.filter_mode = HeadwordFilterMode::kWildcard;
    query.result_limit = 100U;

    const auto response = service->Suggest(query);

    QVERIFY(response.errors.empty());
    QCOMPARE(response.suggestions.size(), std::size_t{1});
    QCOMPARE(response.suggestions.front().headword, "caf\xc3\xa9 noir");

    query.text = "caf?";
    const auto single_character = service->Suggest(query);
    QVERIFY(single_character.errors.empty());
    QCOMPARE(single_character.suggestions.size(), std::size_t{3});
    QCOMPARE(single_character.suggestions.front().headword, "Caf\xc3\xa9");
}

void ApplicationServiceTest::
    FiltersBoundedHeadwordSuggestionsWithRegularExpressions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root, {{"Cafeteria", "long article"},
                                      {"caf\xc3\xa9 noir", "medium article"},
                                      {"Caf\xc3\xa9", "exact article"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    SuggestionQuery query;
    query.text = "^caf(?:eteria|\xc3\xa9)$";
    query.filter_mode = HeadwordFilterMode::kRegularExpression;
    query.result_limit = 100U;

    const auto response = service->Suggest(query);

    QVERIFY(response.errors.empty());
    QCOMPARE(response.suggestions.size(), std::size_t{2});
    QCOMPARE(response.suggestions[0].headword, "Caf\xc3\xa9");
    QCOMPARE(response.suggestions[1].headword, "Cafeteria");

    query.text = "^caf.*";
    query.match_case = true;
    const auto case_sensitive = service->Suggest(query);
    QVERIFY(case_sensitive.errors.empty());
    QCOMPARE(case_sensitive.suggestions.size(), std::size_t{1});
    QCOMPARE(case_sensitive.suggestions.front().headword, "caf\xc3\xa9 noir");
}

void ApplicationServiceTest::RejectsInvalidOrUnseededHeadwordPatterns() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root, {{"example", "article"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    SuggestionQuery query;
    query.text = ".*ample";
    query.filter_mode = HeadwordFilterMode::kRegularExpression;

    const auto unseeded = service->Suggest(query);
    QVERIFY(unseeded.suggestions.empty());
    QCOMPARE(unseeded.errors.size(), std::size_t{1});
    QCOMPARE(unseeded.errors.front().code, LookupErrorCode::kInvalidQuery);

    query.text = "example(";
    const auto invalid = service->Suggest(query);
    QVERIFY(invalid.suggestions.empty());
    QCOMPARE(invalid.errors.size(), std::size_t{1});
    QCOMPARE(invalid.errors.front().code, LookupErrorCode::kInvalidQuery);

    query.filter_mode = HeadwordFilterMode::kWildcard;
    query.text = "*ample";
    const auto wildcard = service->Suggest(query);
    QVERIFY(wildcard.suggestions.empty());
    QCOMPARE(wildcard.errors.size(), std::size_t{1});
    QCOMPARE(wildcard.errors.front().code, LookupErrorCode::kInvalidQuery);

    query.text = std::string(kMaximumHeadwordPatternBytes + 1U, 'a');
    const auto oversized = service->Suggest(query);
    QVERIFY(oversized.suggestions.empty());
    QCOMPARE(oversized.errors.size(), std::size_t{1});
    QCOMPARE(oversized.errors.front().code, LookupErrorCode::kInvalidQuery);

    query.text = "example*";
    query.filter_mode = static_cast<HeadwordFilterMode>(99);
    const auto invalid_mode = service->Suggest(query);
    QVERIFY(invalid_mode.suggestions.empty());
    QCOMPARE(invalid_mode.errors.size(), std::size_t{1});
    QCOMPARE(invalid_mode.errors.front().code, LookupErrorCode::kInvalidQuery);
}

void ApplicationServiceTest::RanksSuggestionsAcrossDictionaries() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    QVERIFY(std::filesystem::create_directories(root / "prefix"));
    QVERIFY(std::filesystem::create_directories(root / "exact"));
    test::WriteStardictFixture(root / "prefix",
                               {{"cafeteria", "prefix article"}});
    test::WriteStardictFixture(root / "exact", {{"Caf\xc3\xa9", "exact"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    SuggestionQuery query;
    query.text = "CAFE";
    query.result_limit = 1U;

    const auto response = service->Suggest(query);

    QVERIFY(response.errors.empty());
    QCOMPARE(response.suggestions.size(), std::size_t{1});
    QCOMPARE(response.suggestions.front().headword, "Caf\xc3\xa9");
    QCOMPARE(response.suggestions.front().match.mode, MatchMode::kExact);
}

void ApplicationServiceTest::SupportsExplicitEmptyDictionaryParticipation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root, {{"example", "article"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);

    LookupQuery lookup;
    lookup.text = "example";
    const auto unfiltered_lookup = service->Lookup(lookup);
    QCOMPARE(unfiltered_lookup.entries.size(), std::size_t{1});
    QVERIFY(unfiltered_lookup.errors.empty());

    lookup.dictionary_filter_active = true;
    const auto empty_lookup = service->Lookup(lookup);
    QVERIFY(empty_lookup.entries.empty());
    QVERIFY(empty_lookup.errors.empty());

    const CancelledToken cancelled;
    const auto cancelled_empty_lookup = service->Lookup(lookup, &cancelled);
    QVERIFY(cancelled_empty_lookup.entries.empty());
    QVERIFY(cancelled_empty_lookup.errors.empty());

    SuggestionQuery suggestion;
    suggestion.text = "exa";
    const auto unfiltered_suggestion = service->Suggest(suggestion);
    QCOMPARE(unfiltered_suggestion.suggestions.size(), std::size_t{1});
    QVERIFY(unfiltered_suggestion.errors.empty());

    suggestion.dictionary_filter_active = true;
    const auto empty_suggestion = service->Suggest(suggestion);
    QVERIFY(empty_suggestion.suggestions.empty());
    QVERIFY(empty_suggestion.errors.empty());

    const auto cancelled_empty_suggestion =
        service->Suggest(suggestion, &cancelled);
    QVERIFY(cancelled_empty_suggestion.suggestions.empty());
    QVERIFY(cancelled_empty_suggestion.errors.empty());
}

void ApplicationServiceTest::AppliesResolvedDictionaryGroupsConsistently() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    QVERIFY(std::filesystem::create_directories(root / "first"));
    QVERIFY(std::filesystem::create_directories(root / "second"));
    test::WriteStardictFixture(root / "first",
                               {{"shared", "first group article"}});
    test::WriteStardictFixture(root / "second",
                               {{"shared", "second group article"}});

    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    const auto discovered =
        CreateDictionaryService(configuration)->GetCatalog();
    QCOMPARE(discovered.size(), std::size_t{2});
    const auto first = std::find_if(
        discovered.begin(), discovered.end(), [](const auto& dictionary) {
            return std::filesystem::path(dictionary.source)
                       .parent_path()
                       .filename() == "first";
        });
    const auto second = std::find_if(
        discovered.begin(), discovered.end(), [](const auto& dictionary) {
            return std::filesystem::path(dictionary.source)
                       .parent_path()
                       .filename() == "second";
        });
    QVERIFY(first != discovered.end());
    QVERIFY(second != discovered.end());
    configuration.dictionary_groups = {
        {7U, "Ordered", "", {second->id, "stale-id", first->id, second->id}},
        {8U, "Empty", "", {"stale-id"}}};

    auto facade = CreateDesktopFacade(configuration);
    auto& service = facade->GetDictionaryService();
    LookupQuery lookup;
    lookup.text = "shared";
    lookup.group_id = 7U;
    auto lookup_response = service.Lookup(lookup);
    QVERIFY(lookup_response.errors.empty());
    QCOMPARE(lookup_response.entries.size(), std::size_t{2});
    QCOMPARE(lookup_response.entries[0].dictionary.id, second->id);
    QCOMPARE(lookup_response.entries[1].dictionary.id, first->id);
    const auto page = facade->ComposeLookupPage(lookup_response);
    QVERIFY(page.plain_text.find("second group article") <
            page.plain_text.find("first group article"));

    SuggestionQuery suggestion;
    suggestion.text = "shared";
    suggestion.group_id = 7U;
    const auto suggestion_response = service.Suggest(suggestion);
    QVERIFY(suggestion_response.errors.empty());
    QCOMPARE(suggestion_response.suggestions.size(), std::size_t{2});
    QCOMPARE(suggestion_response.suggestions[0].dictionary.id, second->id);
    QCOMPARE(suggestion_response.suggestions[1].dictionary.id, first->id);

    lookup.dictionary_ids = {first->id, first->id};
    lookup.dictionary_filter_active = true;
    lookup_response = service.Lookup(lookup);
    QVERIFY(lookup_response.errors.empty());
    QCOMPARE(lookup_response.entries.size(), std::size_t{1});
    QCOMPARE(lookup_response.entries.front().dictionary.id, first->id);

    suggestion.dictionary_ids = {first->id, first->id};
    suggestion.dictionary_filter_active = true;
    const auto filtered_suggestion = service.Suggest(suggestion);
    QVERIFY(filtered_suggestion.errors.empty());
    QCOMPARE(filtered_suggestion.suggestions.size(), std::size_t{1});
    QCOMPARE(filtered_suggestion.suggestions.front().dictionary.id, first->id);

    lookup.dictionary_ids.clear();
    lookup.dictionary_filter_active = false;
    lookup.group_id = 8U;
    lookup_response = service.Lookup(lookup);
    QVERIFY(lookup_response.entries.empty());
    QVERIFY(lookup_response.errors.empty());

    lookup.group_id = 999U;
    const auto missing_group = service.Lookup(lookup);
    QCOMPARE(missing_group.entries.size(), std::size_t{2});
    lookup.group_id = 0U;
    const auto all_group = service.Lookup(lookup);
    QCOMPARE(all_group.entries.size(), std::size_t{2});
    QCOMPARE(missing_group.entries[0].dictionary.id,
             all_group.entries[0].dictionary.id);
    QCOMPARE(missing_group.entries[1].dictionary.id,
             all_group.entries[1].dictionary.id);

    lookup.group_id = 7U;
    lookup.result_limit = 1U;
    QCOMPARE(service.Lookup(lookup).entries.front().dictionary.id, second->id);
    const CancelledToken cancelled;
    const auto cancelled_response = service.Lookup(lookup, &cancelled);
    QVERIFY(cancelled_response.entries.empty());
    QCOMPARE(cancelled_response.errors.front().code,
             LookupErrorCode::kCancelled);
}

void ApplicationServiceTest::DiscoversAndQueriesDictdAlongsideStardict() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    QVERIFY(std::filesystem::create_directories(root / "stardict"));
    test::WriteStardictFixture(root / "stardict",
                               {{"example", "StarDict article"}});
    test::WriteDictdFixture(root / "dictd", {{"example", "Dictd article", {}}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "EXAMPLE";
    query.result_limit = 5U;

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);

    QCOMPARE(catalog.size(), std::size_t{2});
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{2});
    QVERIFY(std::any_of(response.entries.begin(), response.entries.end(),
                        [](const auto& entry) {
                            return entry.dictionary.id.rfind("dictd-", 0) ==
                                       0U &&
                                   entry.article.plain_text.find(
                                       "Dictd article") != std::string::npos;
                        }));
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesSdict() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteSdictFixture(
        root, {{"example", "<b>SDict article</b> <r>target</r>"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "EXAMPLE";

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);

    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("sdict-", 0) == 0U);
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    QVERIFY(response.entries.front().article.sanitized_html.has_value());
    QVERIFY(response.entries.front().article.sanitized_html->find(
                "<b>SDict article</b>") != std::string::npos);
    QVERIFY(response.entries.front().article.sanitized_html->find(
                "goldendict://lookup/target") != std::string::npos);
    QVERIFY(response.entries.front().article.plain_text.find("target") !=
            std::string::npos);
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesXdxfResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto path = test::WriteXdxfFixture(
        root, {{{"example"},
                "<def><b>XDXF article</b> <kref>target</kref> "
                "<rref>images/pixel.png</rref></def>"}});
    test::WriteXdxfResource(path, "images/pixel.png", "png-data");
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "EXAMPLE";

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);

    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("xdxf-", 0) == 0U);
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QVERIFY(entry.article.sanitized_html->find("<b>XDXF article</b>") !=
            std::string::npos);
    QVERIFY(entry.article.sanitized_html->find("goldendict://lookup/target") !=
            std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    const auto data = service->GetResource(entry.resources.front());
    QCOMPARE(data.size(), std::size_t{8});
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesGlsResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto path = test::WriteGlsFixture(
        root,
        {{{"example"}, "<b>GLS article</b> <img src=\"images/pixel.png\">"}});
    test::WriteGlsResource(path, "images/pixel.png", "png-data");
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "EXAMPLE";

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);

    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("gls-", 0) == 0U);
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QVERIFY(entry.article.sanitized_html->find("<b>GLS article</b>") !=
            std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    const auto data = service->GetResource(entry.resources.front());
    QCOMPARE(data.size(), std::size_t{8});
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesDslResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto path = test::WriteDslFixture(root);
    test::WriteDslResource(path, "images/cup.png", "png-data");
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "CAFE";

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);

    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("dsl-", 0) == 0U);
    QCOMPARE(catalog.front().source_language, "en");
    QCOMPARE(catalog.front().target_language, "de");
    QCOMPARE(catalog.front().description, "Fixture DSL annotation");
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QCOMPARE(entry.language.source_language, "en");
    QCOMPARE(entry.language.target_language, "de");
    QVERIFY(entry.article.sanitized_html->find("<b>drink</b>") !=
            std::string::npos);
    QVERIFY(entry.article.sanitized_html->find("goldendict://lookup/coffee") !=
            std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    QCOMPARE(service->GetResource(entry.resources.front()).size(),
             std::size_t{8});
}

void ApplicationServiceTest::PublishesDslTooltipsAfterSanitization() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteDslTextFixture(root, "entry\n\t[p]abbr[/p]\n");
    test::WriteDslTextFixture(root, "abbr\n\tNorth [i]American[/i]-English\n",
                              "fixture_abrv.dsl");
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "entry";

    const auto response = service->Lookup(query);

    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    QVERIFY(response.entries.front().article.sanitized_html.has_value());
    const std::string nbsp{"\xc2\xa0", 2U};
    QVERIFY(response.entries.front().article.sanitized_html->find(
                "<span class=\"dsl_p\" title=\"North" + nbsp + "American" +
                "\xe2\x80\x91"
                "English\">abbr</span>") != std::string::npos);
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesBglResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteBglFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "EXAMPLE";

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);

    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("bgl-", 0) == 0U);
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QCOMPARE(entry.language.source_language, "en");
    QCOMPARE(entry.language.target_language, "de");
    QVERIFY(entry.article.sanitized_html->find("<b>definition</b>") !=
            std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    QCOMPARE(service->GetResource(entry.resources.front()).size(),
             std::size_t{8});
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesMdictResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteMdictFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "ALIAS";

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);

    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("mdict-", 0) == 0U);
    QCOMPARE(catalog.front().name, "Fixture MDict");
    QCOMPARE(catalog.front().description, "Fixture description");
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QVERIFY(entry.article.sanitized_html.has_value());
    QVERIFY(entry.article.sanitized_html->find("<b>definition</b>") !=
            std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    const auto data = service->GetResource(entry.resources.front());
    QCOMPARE(data.size(), std::size_t{9});
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesAard() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteAardFixture(root, "fixture.aar", false, false, false,
                           "[\"<b>visible searchable definition</b>"
                           "<a href=\\\"w:alias\\\">alias</a>\"]");
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.index_directory = (root / "indexes").string();
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "EXAMPLE";

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);
    QVERIFY(WaitForFullTextLifecycleState(
        *service, catalog.front().id,
        dictionary::FullTextIndexLifecycleState::kCurrent));
    FullTextQuery full_text_query;
    full_text_query.text = "searchable";
    const auto full_text_response = service->SearchFullText(full_text_query);

    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("aard-", 0) == 0U);
    QCOMPARE(catalog.front().name, "Fixture Aard");
    QCOMPARE(catalog.front().description, "fixture");
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    QVERIFY(full_text_response.errors.empty());
    QCOMPARE(full_text_response.results.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QCOMPARE(entry.language.source_language, "en");
    QCOMPARE(entry.language.target_language, "de");
    QVERIFY(entry.article.sanitized_html->find(
                "visible searchable definition") != std::string::npos);
    QVERIFY(entry.article.sanitized_html->find("goldendict://lookup/alias") !=
            std::string::npos);
    const auto index_path =
        std::filesystem::path(configuration.index_directory) /
        (catalog.front().id + ".gdfts");
    const auto canonical = ReadFile(index_path);
    QVERIFY(!canonical.empty());
    service.reset();

    test::WriteAardFixture(root, "fixture.aar", true, true);
    service = CreateDictionaryService(configuration);
    QVERIFY(WaitForFullTextLifecycleState(
        *service, catalog.front().id,
        dictionary::FullTextIndexLifecycleState::kCurrent));
    const auto rebuilt_stale = ReadFile(index_path);
    QVERIFY(!rebuilt_stale.empty());
    QVERIFY(rebuilt_stale != canonical);
    QCOMPARE(service->GetCatalog().front().article_count, std::size_t{2});
    service.reset();

    std::ofstream(index_path, std::ios::binary | std::ios::trunc) << "corrupt";
    auto facade = CreateDesktopFacade(configuration);
    auto& facade_service = facade->GetDictionaryService();
    QVERIFY(WaitForFullTextLifecycleState(
        facade_service, catalog.front().id,
        dictionary::FullTextIndexLifecycleState::kCurrent));
    const auto rebuilt_corrupt = ReadFile(index_path);
    QVERIFY(!rebuilt_corrupt.empty());
    QVERIFY(rebuilt_corrupt != "corrupt");
}

void ApplicationServiceTest::
    AppliesPersistedFullTextPolicyAfterAardDiscovery() {
    CoreConfiguration empty_configuration;
    auto empty_service = CreateDictionaryService(empty_configuration);
    QVERIFY(empty_service->GetCatalog().empty());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteAardFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.index_directory = (root / "indexes").string();
    auto service = CreateDictionaryService(configuration);
    const auto dictionary_id = service->GetCatalog().front().id;
    QVERIFY(WaitForFullTextLifecycleState(
        *service, dictionary_id,
        dictionary::FullTextIndexLifecycleState::kCurrent));
    const auto current =
        application::FullTextIndexLifecycleSnapshot(*service, dictionary_id);
    QVERIFY(current.has_value());
    QCOMPARE(current->identity().generation, 1U);
    QCOMPARE(current->state(),
             dictionary::FullTextIndexLifecycleState::kCurrent);
    const auto artifact = std::filesystem::path(configuration.index_directory) /
                          (dictionary_id + ".gdfts");
    const auto canonical = ReadFile(artifact);

    service.reset();
    service = CreateDictionaryService(configuration);
    const auto reused =
        application::FullTextIndexLifecycleSnapshot(*service, dictionary_id);
    QVERIFY(reused.has_value());
    QCOMPARE(reused->identity().generation, 1U);
    QCOMPARE(reused->state(),
             dictionary::FullTextIndexLifecycleState::kCurrent);
    QCOMPARE(ReadFile(artifact), canonical);

    configuration.preferences.full_text_disabled_types = "aard";
    service = CreateDictionaryService(configuration);
    const auto excluded =
        application::FullTextIndexLifecycleSnapshot(*service, dictionary_id);
    QVERIFY(excluded.has_value());
    QCOMPARE(excluded->identity().generation, 1U);
    QCOMPARE(excluded->state(),
             dictionary::FullTextIndexLifecycleState::kPolicyExcluded);
    QCOMPARE(ReadFile(artifact), canonical);

    configuration.preferences.full_text_disabled_types.clear();
    configuration.preferences.full_text_maximum_dictionary_articles = 1U;
    service = CreateDictionaryService(configuration);
    QCOMPARE(
        application::FullTextIndexLifecycleSnapshot(*service, dictionary_id)
            ->state(),
        dictionary::FullTextIndexLifecycleState::kPolicyExcluded);
    QCOMPARE(ReadFile(artifact), canonical);
}

void ApplicationServiceTest::ComposesIdleExecutorAfterAardReconciliation() {
    auto empty_service = CreateDictionaryService({});
    QVERIFY(empty_service->GetCatalog().empty());
    empty_service.reset();

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteAardFixture(root, "first.aar");
    test::WriteAardFixture(root, "second.aar", true, true);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.index_directory = (root / "indexes").string();

    auto initial_candidate =
        application::CreateDictionaryServiceActivationCandidate(configuration,
                                                                {});
    auto service = std::move(initial_candidate.service);
    const auto initial_catalog = service->GetCatalog();
    QCOMPARE(initial_catalog.size(), 2U);
    QVERIFY(initial_candidate.activation.SubmitOnceWithDefaults());
    for (const auto& dictionary : initial_catalog) {
        QVERIFY(WaitForFullTextLifecycleState(
            *service, dictionary.id,
            dictionary::FullTextIndexLifecycleState::kCurrent));
    }
    initial_candidate.activation.ShutdownAndJoin();

    service = CreateDictionaryService(configuration);
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), 2U);
    std::vector<std::pair<std::filesystem::path, std::string>> artifacts;
    for (const auto& dictionary : catalog) {
        const auto lifecycle = application::FullTextIndexLifecycleSnapshot(
            *service, dictionary.id);
        QVERIFY(lifecycle.has_value());
        QCOMPARE(lifecycle->identity().generation, 1U);
        QCOMPARE(lifecycle->state(),
                 dictionary::FullTextIndexLifecycleState::kCurrent);
        const auto artifact =
            std::filesystem::path(configuration.index_directory) /
            (dictionary.id + ".gdfts");
        artifacts.emplace_back(artifact, ReadFile(artifact));
        QVERIFY(!artifacts.back().second.empty());
    }

    service.reset();
    for (const auto& [path, contents] : artifacts)
        QCOMPARE(ReadFile(path), contents);
}

void ApplicationServiceTest::CancelsPreparedAardFullTextLifecycle() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteAardFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.index_directory = (root / "indexes").string();

    application::DesktopFacadeActivationOwner owner;
    auto candidate = owner.PrepareCandidate(configuration);
    const auto facade = owner.PreparedFacadeSnapshot(candidate);
    QVERIFY(facade);
    auto& service = facade->GetDictionaryService();
    const auto dictionary_id = service.GetCatalog().front().id;
    QVERIFY(
        application::CancelFullTextIndexLifecycleWork(service, dictionary_id));
    QCOMPARE(application::FullTextIndexLifecycleSnapshot(service, dictionary_id)
                 ->state(),
             dictionary::FullTextIndexLifecycleState::kCancelled);

    QVERIFY(owner.Activate(candidate));
    QVERIFY(owner.Shutdown());
    QVERIFY(application::IsFullTextIndexExecutorStopped(service));
    QCOMPARE(application::FullTextIndexLifecycleSnapshot(service, dictionary_id)
                 ->state(),
             dictionary::FullTextIndexLifecycleState::kCancelled);
    QVERIFY(!std::filesystem::exists(
        std::filesystem::path(configuration.index_directory) /
        (dictionary_id + ".gdfts")));
}

void ApplicationServiceTest::
    ActivatesAndReplacesPrivateDesktopFacadeCompositions() {
    application::DesktopFacadeActivationOwner owner;
    QVERIFY(!owner.CurrentSnapshot());
    application::PreparedCoreFacadeCandidate invalid;
    QVERIFY(!owner.Activate(invalid));

    auto abort_candidate = owner.PrepareCandidate({});
    auto aborted = owner.Reserve(abort_candidate);
    QVERIFY(aborted);
    QVERIFY(!abort_candidate);
    QVERIFY(!owner.Shutdown());
    aborted.Abort();
    QVERIFY(!aborted);
    QVERIFY(!owner.CurrentSnapshot());

    auto initial_candidate = owner.PrepareCandidate({});
    QVERIFY(initial_candidate);
    QVERIFY(!owner.CurrentSnapshot());
    QVERIFY(owner.Activate(initial_candidate));
    const auto initial = owner.CurrentSnapshot();
    QVERIFY(initial);
    QVERIFY(initial->GetDictionaryService().GetCatalog().empty());
    QVERIFY(!owner.Activate(initial_candidate));

    auto replacement_candidate = owner.PrepareCandidate({});
    QVERIFY(replacement_candidate);
    const auto prepared_facade =
        application::CoreFacadeActivationTestAccess::Facade(
            replacement_candidate);
    QVERIFY(prepared_facade);
    QVERIFY(!application::IsFullTextIndexExecutorStopped(
        prepared_facade->GetDictionaryService()));

    struct HandoffObservation {
        const DictionaryService* old_service = nullptr;
        std::array<application::CoreFacadeActivationEvent, 3U> events{};
        std::array<bool, 3U> old_stopped{};
        std::size_t count = 0U;
    } observation{&initial->GetDictionaryService()};

    application::CoreFacadeActivationTestAccess::Observe(
        replacement_candidate,
        [](void* context,
           application::CoreFacadeActivationEvent event) noexcept {
            auto& observed = *static_cast<HandoffObservation*>(context);
            observed.events[observed.count] = event;
            observed.old_stopped[observed.count] =
                application::IsFullTextIndexExecutorStopped(
                    *observed.old_service);
            ++observed.count;
        },
        &observation);
    QCOMPARE(owner.CurrentSnapshot(), initial);
    QVERIFY(!application::IsFullTextIndexExecutorStopped(
        initial->GetDictionaryService()));
    QVERIFY(initial->GetDictionaryService().GetCatalog().empty());
    auto reserved_replacement = owner.Reserve(replacement_candidate);
    QVERIFY(reserved_replacement);
    QVERIFY(!replacement_candidate);
    QVERIFY(!owner.Shutdown());
    auto published_replacement =
        owner.PublishReservedOnly(reserved_replacement);
    QVERIFY(!reserved_replacement);
    QVERIFY(published_replacement);
    QCOMPARE(observation.count, 1U);
    QCOMPARE(observation.events[0],
             application::CoreFacadeActivationEvent::kPublished);
    QVERIFY(!observation.old_stopped[0]);
    QCOMPARE(owner.CurrentSnapshot(), prepared_facade);
    QVERIFY(!application::IsFullTextIndexExecutorStopped(
        initial->GetDictionaryService()));
    QVERIFY(!owner.Shutdown());
    owner.FinishPublished(published_replacement);
    QVERIFY(!published_replacement);
    QCOMPARE(observation.count, 3U);
    QCOMPARE(observation.events[1],
             application::CoreFacadeActivationEvent::kOldExecutorStopped);
    QVERIFY(observation.old_stopped[1]);
    QCOMPARE(observation.events[2],
             application::CoreFacadeActivationEvent::kNewExecutorSubmitted);
    QVERIFY(observation.old_stopped[2]);
    const auto replacement = owner.CurrentSnapshot();
    QVERIFY(replacement);
    QVERIFY(replacement != initial);
    QVERIFY(initial->GetDictionaryService().GetCatalog().empty());
    QVERIFY(application::IsFullTextIndexExecutorStopped(
        initial->GetDictionaryService()));

    QVERIFY(owner.Shutdown());
    QVERIFY(owner.Shutdown());
    QVERIFY(!owner.CurrentSnapshot());
    QVERIFY(!owner.PrepareCandidate({}));
    auto shutdown_candidate = owner.PrepareCandidate({});
    QVERIFY(!owner.Activate(shutdown_candidate));
    QVERIFY(initial->GetDictionaryService().GetCatalog().empty());
    QVERIFY(replacement->GetDictionaryService().GetCatalog().empty());
}

void ApplicationServiceTest::
    ReloadsUnchangedDictionarySourcesThroughActivationOwner() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteAardFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.index_directory = (root / "indexes").string();

    application::DesktopFacadeActivationOwner owner;
    auto initial_candidate = owner.PrepareCandidate(configuration);
    QVERIFY(initial_candidate);
    QVERIFY(owner.Activate(initial_candidate));
    const auto initial = owner.CurrentSnapshot();
    QVERIFY(initial);
    const auto initial_catalog = initial->GetDictionaryService().GetCatalog();
    QCOMPARE(initial_catalog.size(), std::size_t{1});

    auto reload_candidate = owner.PrepareCandidate(configuration);
    QVERIFY(reload_candidate);
    const auto prepared = owner.PreparedFacadeSnapshot(reload_candidate);
    QVERIFY(prepared);
    QVERIFY(prepared != initial);
    QVERIFY(owner.Activate(reload_candidate));
    const auto reloaded = owner.CurrentSnapshot();
    QCOMPARE(reloaded, prepared);
    QVERIFY(reloaded != initial);
    QVERIFY(application::IsFullTextIndexExecutorStopped(
        initial->GetDictionaryService()));
    const auto reloaded_catalog = reloaded->GetDictionaryService().GetCatalog();
    QCOMPARE(reloaded_catalog.size(), initial_catalog.size());
    QCOMPARE(reloaded_catalog.front().id, initial_catalog.front().id);
    QCOMPARE(reloaded_catalog.front().source, initial_catalog.front().source);

    LookupQuery query;
    query.text = "EXAMPLE";
    const auto response = reloaded->GetDictionaryService().Lookup(query);
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
}

void ApplicationServiceTest::
    PreservesInstalledCandidateWhenAnotherBuildFails() {
    application::DesktopFacadeActivationOwner owner;
    auto prepared = owner.PrepareCandidate({});
    QVERIFY(prepared);

    std::vector<std::unique_ptr<RuntimeDictionarySource>> invalid;
    invalid.push_back(std::make_unique<InspectionRuntimeSource>());
    invalid.push_back(std::make_unique<InspectionRuntimeSource>());
    QVERIFY_EXCEPTION_THROWN(owner.PrepareCandidate({}, std::move(invalid)),
                             std::runtime_error);

    QVERIFY(owner.Activate(prepared));
    const auto current = owner.CurrentSnapshot();
    QVERIFY(current);
    QVERIFY(current->GetDictionaryService().GetCatalog().empty());

    auto abandoned = owner.PrepareCandidate({});
    QVERIFY(abandoned);
    abandoned.Abandon();
    QVERIFY(!abandoned);
    QVERIFY(!owner.Activate(abandoned));
    QCOMPARE(owner.CurrentSnapshot(), current);
}

void ApplicationServiceTest::
    SerializesConcurrentCandidateInstallationAndActivation() {
    application::DesktopFacadeActivationOwner owner;
    std::mutex mutex;
    std::condition_variable condition;
    std::size_t ready = 0U;
    bool start = false;
    const auto wait_for_start = [&] {
        std::unique_lock lock(mutex);
        ++ready;
        condition.notify_all();
        condition.wait(lock, [&] { return start; });
    };
    const auto release = [&] {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return ready == 2U; });
        start = true;
        condition.notify_all();
    };

    auto first_build = std::async(std::launch::async, [&] {
        wait_for_start();
        return owner.PrepareCandidate({});
    });
    auto second_build = std::async(std::launch::async, [&] {
        wait_for_start();
        return owner.PrepareCandidate({});
    });
    release();
    auto first_candidate = first_build.get();
    auto second_candidate = second_build.get();
    QVERIFY(first_candidate);
    QVERIFY(second_candidate);

    ready = 0U;
    start = false;
    auto first_activation = std::async(std::launch::async, [&] {
        wait_for_start();
        return owner.Activate(first_candidate);
    });
    auto second_activation = std::async(std::launch::async, [&] {
        wait_for_start();
        return owner.Activate(second_candidate);
    });
    release();
    QCOMPARE(static_cast<unsigned>(first_activation.get()) +
                 static_cast<unsigned>(second_activation.get()),
             1U);
    QVERIFY(owner.CurrentSnapshot());
    QVERIFY(!owner.Activate(first_candidate));
    QVERIFY(!owner.Activate(second_candidate));

    application::DesktopFacadeActivationOwner other;
    auto cross_owner = other.PrepareCandidate({});
    QVERIFY(cross_owner);
    QVERIFY(!owner.Activate(cross_owner));
    QVERIFY(other.Activate(cross_owner));
}

void ApplicationServiceTest::ActivationHandleIsOneShotAndStopsOnDestruction() {
    auto candidate =
        application::CreateDictionaryServiceActivationCandidate({}, {});
    QVERIFY(candidate.service);
    QVERIFY(candidate.activation.SubmitOnceWithDefaults());
    QVERIFY(!candidate.activation.SubmitOnceWithDefaults());
    candidate.activation.ShutdownAndJoin();
    candidate.activation.ShutdownAndJoin();
    QVERIFY(!candidate.activation.SubmitOnceWithDefaults());
    QVERIFY(candidate.service->GetCatalog().empty());
}

void ApplicationServiceTest::
    ActivationOwnerDestructionStopsRetainedFacadeState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteAardFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    configuration.index_directory = (root / "indexes").string();

    std::shared_ptr<DesktopFacade> retained;
    std::string dictionary_id;
    {
        application::DesktopFacadeActivationOwner owner;
        auto candidate = owner.PrepareCandidate(configuration);
        QVERIFY(candidate);
        QVERIFY(owner.Activate(candidate));
        retained = owner.CurrentSnapshot();
        QVERIFY(retained);
        dictionary_id =
            retained->GetDictionaryService().GetCatalog().front().id;
    }

    const auto lifecycle = application::FullTextIndexLifecycleSnapshot(
        retained->GetDictionaryService(), dictionary_id);
    QVERIFY(lifecycle.has_value());
    QVERIFY(application::IsFullTextIndexExecutorStopped(
        retained->GetDictionaryService()));
    QCOMPARE(lifecycle->state(),
             dictionary::FullTextIndexLifecycleState::kCancelled);
    LookupQuery query;
    query.text = "headword";
    const auto response = retained->GetDictionaryService().Lookup(query);
    QVERIFY(response.errors.empty());
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesZimResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteZimFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "ALIAS";

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);

    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("zim-", 0) == 0U);
    QCOMPARE(catalog.front().name, "Fixture ZIM");
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QCOMPARE(entry.language.source_language, "en");
    QVERIFY(entry.article.sanitized_html.has_value());
    QVERIFY(entry.article.sanitized_html->find("<b>definition</b>") !=
            std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    QCOMPARE(service->GetResource(entry.resources.front()).size(),
             std::size_t{8});
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesSlobResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteSlobFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "ALIAS";
    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);
    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("slob-", 0) == 0U);
    QCOMPARE(catalog.front().name, "Fixture SLOB");
    QCOMPARE(catalog.front().description, "fixture");
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QCOMPARE(entry.language.source_language, "en");
    QCOMPARE(entry.language.target_language, "de");
    QVERIFY(entry.article.sanitized_html->find("<b>definition</b>") !=
            std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    QCOMPARE(service->GetResource(entry.resources.front()).size(),
             std::size_t{8});
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesEpwingResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteEpwingFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "EXAMPLE";
    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);
    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("epwing-", 0) == 0U);
    QCOMPARE(catalog.front().name, "Fixture EPWING");
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    QVERIFY(response.entries.front().article.sanitized_html->find(
                "goldendict://lookup/second") != std::string::npos);
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesLsaAudio() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteLsaFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "EXAMPLE";
    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);
    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("lsa-", 0) == 0U);
    QVERIFY(catalog.front().supports_headword_enumeration);
    QVERIFY(!catalog.front().supports_full_text_search);
    HeadwordEnumerationQuery enumeration_query;
    enumeration_query.dictionary_id = catalog.front().id;
    const auto enumeration = service->EnumerateHeadwords(enumeration_query);
    QVERIFY(!enumeration.error.has_value());
    QCOMPARE(enumeration.headwords,
             (std::vector<std::string>{"Apple", "apple", "duplicate", "example",
                                       "second"}));
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QVERIFY(entry.article.sanitized_html->find("<audio controls") !=
            std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    const auto wav = service->GetResource(entry.resources.front());
    QCOMPARE(wav.size(), std::size_t{44U + 16U * 2U});
}

void ApplicationServiceTest::DiscoversSanitizesAndQueriesZipSoundsAudio() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    formats::zipsounds::test::WriteZipSoundsFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "NESTED/SECOND";
    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);
    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("zipsounds-", 0) == 0U);
    QVERIFY(catalog.front().supports_headword_enumeration);
    QVERIFY(!catalog.front().supports_full_text_search);
    HeadwordEnumerationQuery enumeration_query;
    enumeration_query.dictionary_id = catalog.front().id;
    const auto enumeration = service->EnumerateHeadwords(enumeration_query);
    QVERIFY(!enumeration.error.has_value());
    QCOMPARE(enumeration.headwords,
             (std::vector<std::string>{" spaced", "Apple", "apple", "duplicate",
                                       "example", "nested/second"}));
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QVERIFY(entry.article.sanitized_html->find("type=\"audio/ogg\"") !=
            std::string::npos);
    QCOMPARE(entry.resources.size(), std::size_t{1});
    QCOMPARE(entry.resources.front().media_type, "audio/ogg");
    const auto audio = service->GetResource(entry.resources.front());
    QCOMPARE(audio.size(), std::size_t{15U});
}

void ApplicationServiceTest::QueriesExplicitlyConfiguredSoundDirectory() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    formats::sounddir::test::WriteSoundDirectoryFixture(root);
    CoreConfiguration configuration;
    configuration.sound_directories = {{root.string(), "Fixture sounds"}};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "SECOND";
    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);
    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("sounddir-", 0) == 0U);
    QCOMPARE(catalog.front().name, "Fixture sounds");
    QVERIFY(catalog.front().supports_headword_enumeration);
    QVERIFY(!catalog.front().supports_full_text_search);
    HeadwordEnumerationQuery enumeration_query;
    enumeration_query.dictionary_id = catalog.front().id;
    const auto enumeration = service->EnumerateHeadwords(enumeration_query);
    QVERIFY(!enumeration.error.has_value());
    QCOMPARE(enumeration.headwords,
             (std::vector<std::string>{".hidden", "Apple", "apple", "duplicate",
                                       "example", "second"}));
    QCOMPARE(response.entries.size(), std::size_t{1});
    QCOMPARE(response.entries.front().resources.size(), std::size_t{1});
    QCOMPARE(
        service->GetResource(response.entries.front().resources.front()).size(),
        std::size_t{15U});
}

void ApplicationServiceTest::ReportsCancellationAndUnavailableDictionaries() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root, {{"example", "article"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{1});

    LookupQuery query;
    query.text = "example";
    const CancelledToken cancelled;
    const auto cancelled_response = service->Lookup(query, &cancelled);
    QVERIFY(cancelled_response.entries.empty());
    QCOMPARE(cancelled_response.errors.size(), std::size_t{1});
    QCOMPARE(cancelled_response.errors.front().code,
             LookupErrorCode::kCancelled);

    query.dictionary_ids = {"unknown"};
    const auto unavailable = service->Lookup(query);
    QVERIFY(unavailable.entries.empty());
    QCOMPARE(unavailable.errors.size(), std::size_t{1});
    QCOMPARE(unavailable.errors.front().code,
             LookupErrorCode::kDictionaryUnavailable);
}

void ApplicationServiceTest::RejectsUnboundedOrMalformedQueries() {
    auto service = CreateDictionaryService({});
    LookupQuery query;
    query.text = std::string(kMaximumLookupTextBytes + 1U, 'x');

    auto response = service->Lookup(query);
    QCOMPARE(response.errors.size(), std::size_t{1});
    QCOMPARE(response.errors.front().code, LookupErrorCode::kInvalidQuery);

    query.text = std::string("\xc3\x28", 2);
    response = service->Lookup(query);
    QCOMPARE(response.errors.size(), std::size_t{1});
    QCOMPARE(response.errors.front().code, LookupErrorCode::kInvalidQuery);

    query.text = "example";
    query.dictionary_ids.assign(kMaximumLookupDictionaryFilters + 1U,
                                "dictionary");
    response = service->Lookup(query);
    QCOMPARE(response.errors.size(), std::size_t{1});
    QCOMPARE(response.errors.front().code, LookupErrorCode::kInvalidQuery);

    query.dictionary_ids.clear();
    query.languages = {std::string(kMaximumLookupFilterBytes + 1U, 'x')};
    response = service->Lookup(query);
    QCOMPARE(response.errors.size(), std::size_t{1});
    QCOMPARE(response.errors.front().code, LookupErrorCode::kInvalidQuery);

    query.languages.clear();
    query.match_mode = MatchMode::kFuzzy;
    response = service->Lookup(query);
    QCOMPARE(response.errors.size(), std::size_t{1});
    QCOMPARE(response.errors.front().code, LookupErrorCode::kInvalidQuery);
}

void ApplicationServiceTest::CompletesAnOwnedAsynchronousLookup() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root, {{"example", "article"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "example";

    auto request = service->StartLookup(query);
    service.reset();
    const auto response = request->Await();

    QVERIFY(request->IsFinished());
    QCOMPARE(response.errors.size(), std::size_t{0});
    QCOMPARE(response.entries.size(), std::size_t{1});
    QCOMPARE(response.entries.front().match.normalized_headword, "example");
}

void ApplicationServiceTest::AppliesArticlePreferencesBehindTheDesktopFacade() {
    CoreConfiguration configuration;
    configuration.preferences.collapse_large_articles = true;
    configuration.preferences.article_size_limit = 3U;
    auto facade = CreateDesktopFacade(configuration);
    LookupResponse response;
    DictionaryEntry large;
    large.dictionary.name = "Large";
    large.article.plain_text = "four";
    DictionaryEntry small;
    small.dictionary.name = "Small";
    small.article.plain_text = "two";
    response.entries = {std::move(large), std::move(small)};

    const auto page = facade->ComposeLookupPage(response);

    QVERIFY(page.sanitized_html.has_value());
    QVERIFY(page.sanitized_html->find(
                "<details class=\"gd-collapsed-article\"><summary><h2>Large") !=
            std::string::npos);
    QVERIFY(page.sanitized_html->find(
                "<details class=\"gd-collapsed-article\"><summary><h2>Small") ==
            std::string::npos);
}

void ApplicationServiceTest::ResolvesTypedArticleUrlsBehindTheDesktopFacade() {
    auto facade = CreateDesktopFacade({});

    const auto lookup =
        facade->ResolveArticleUrl("goldendict://lookup/linked%20word");
    QVERIFY(lookup.has_value());
    QCOMPARE(lookup->kind, ArticleUrlKind::kLookup);
    QCOMPARE(lookup->lookup_text, "linked word");

    const auto resource = facade->ResolveArticleUrl(
        "goldendict://resource/fixture/images%2Fpixel.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->kind, ArticleUrlKind::kResource);
    QCOMPARE(resource->resource.dictionary_id, "fixture");
    QCOMPARE(resource->resource.resource_id, "images/pixel.png");
    QCOMPARE(resource->resource.media_type, "image/png");

    QVERIFY(!facade->ResolveArticleUrl("https://example.test").has_value());
}

void ApplicationServiceTest::
    BuildsRenderedTextMatchPlansBehindTheDesktopFacade() {
    auto facade = CreateDesktopFacade({});
    RenderedTextMatchPlanRequest request;
    QCOMPARE(request.ignore_diacritics, false);
    request.rendered_text = u8"CAFÉ café cafe café";
    request.query_text = u8"café";

    struct PolicyCase {
        bool match_case;
        bool ignore_diacritics;
        std::vector<std::string> literals;
    };

    const std::vector<PolicyCase> policies = {
        {false, false, {u8"CAFÉ", u8"café", u8"café"}},
        {true, false, {u8"café", u8"café"}},
        {false, true, {u8"CAFÉ", u8"café", "cafe", u8"café"}},
        {true, true, {u8"café", "cafe", u8"café"}},
    };
    for (const auto& policy : policies) {
        request.match_case = policy.match_case;
        request.ignore_diacritics = policy.ignore_diacritics;
        const auto policy_result = facade->BuildRenderedTextMatchPlan(request);
        QVERIFY(policy_result);
        QCOMPARE(policy_result.ranges.size(), policy.literals.size());
        std::size_t search_offset = 0U;
        for (std::size_t index = 0U; index < policy.literals.size(); ++index) {
            const auto expected_offset = request.rendered_text.find(
                policy.literals[index], search_offset);
            QVERIFY(expected_offset != std::string::npos);
            QCOMPARE(policy_result.ranges[index].byte_offset, expected_offset);
            QCOMPARE(policy_result.ranges[index].byte_length,
                     policy.literals[index].size());
            QCOMPARE(policy_result.ranges[index].literal,
                     policy.literals[index]);
            search_offset = expected_offset + policy.literals[index].size();
        }
    }

    request = {};
    request.rendered_text = u8"CAFÉ café test test";
    request.query_text = u8"café";

    auto result = facade->BuildRenderedTextMatchPlan(request);
    QVERIFY(result);
    QCOMPARE(result.ranges.size(), std::size_t{2});
    QCOMPARE(result.ranges[0].byte_offset, std::size_t{0});
    QCOMPARE(result.ranges[0].literal, std::string(u8"CAFÉ"));
    QCOMPARE(result.ranges[1].byte_offset, std::string(u8"CAFÉ ").size());
    QCOMPARE(result.ranges[1].literal, std::string(u8"café"));
    for (const auto& range : result.ranges) {
        QCOMPARE(
            request.rendered_text.substr(range.byte_offset, range.byte_length),
            range.literal);
    }

    request.query_text = "test";
    request.match_case = true;
    request.mode = FullTextQueryMode::kPlainText;
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.ranges.size(), std::size_t{2});
    QCOMPARE(result.ranges[0].byte_offset + result.ranges[0].byte_length + 1U,
             result.ranges[1].byte_offset);

    request.rendered_text = "alpha beta beta alpha";
    request.query_text = "alpha beta";
    request.mode = FullTextQueryMode::kWholeWords;
    request.ignore_word_order = true;
    request.maximum_word_distance = 0U;
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.ranges.size(), std::size_t{2});
    QCOMPARE(result.ranges[0].literal, std::string("alpha beta"));
    QCOMPARE(result.ranges[1].literal, std::string("beta alpha"));

    request = {};
    request.rendered_text = "ab12 cd34";
    request.query_text = "[a-z]+[0-9]+";
    request.mode = FullTextQueryMode::kRegularExpression;
    request.match_case = true;
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.ranges.size(), std::size_t{2});
    QCOMPARE(result.ranges[0].literal, std::string("ab12"));
    QCOMPARE(result.ranges[1].literal, std::string("cd34"));

    request.query_text = "*";
    request.mode = FullTextQueryMode::kWildcard;
    result = facade->BuildRenderedTextMatchPlan(request);
    QVERIFY(result);
    QCOMPARE(result.ranges.size(), std::size_t{1});
    QCOMPARE(result.ranges.front().literal, request.rendered_text);

    request.rendered_text.clear();
    result = facade->BuildRenderedTextMatchPlan(request);
    QVERIFY(result);
    QVERIFY(result.ranges.empty());

    request = {};
    request.rendered_text = u8"Straße é 😀";
    request.query_text = "ss";
    request.mode = FullTextQueryMode::kRegularExpression;
    result = facade->BuildRenderedTextMatchPlan(request);
    QVERIFY(result);
    QCOMPARE(result.ranges.size(), std::size_t{1});
    QCOMPARE(result.ranges.front().literal, std::string(u8"ß"));
    QCOMPARE(request.rendered_text.substr(result.ranges.front().byte_offset,
                                          result.ranges.front().byte_length),
             result.ranges.front().literal);

    request.query_text = u8"é";
    request.match_case = true;
    result = facade->BuildRenderedTextMatchPlan(request);
    QVERIFY(result);
    QCOMPARE(result.ranges.size(), std::size_t{1});
    QCOMPARE(result.ranges.front().literal, std::string(u8"é"));

    request.query_text = ".";
    request.rendered_text = u8"😀";
    request.mode = FullTextQueryMode::kRegularExpression;
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.ranges.size(), std::size_t{1});
    QCOMPARE(result.ranges.front().byte_length, std::string(u8"😀").size());
    QCOMPARE(result.ranges.front().literal, std::string(u8"😀"));
}

void ApplicationServiceTest::RejectsInvalidRenderedTextMatchPlanRequests() {
    auto facade = CreateDesktopFacade({});
    RenderedTextMatchPlanRequest request;
    request.rendered_text = "text";
    auto result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kInvalidRequest);
    QVERIFY(result.ranges.empty());

    request.query_text = "[";
    request.mode = FullTextQueryMode::kRegularExpression;
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kMalformedPattern);
    QVERIFY(result.ranges.empty());

    request.query_text = "text";
    request.ignore_word_order = true;
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kInvalidRequest);

    request = {};
    request.rendered_text = std::string("bad\xff", 4U);
    request.query_text = "bad";
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kInvalidRequest);

    request.rendered_text = "text";
    request.timeout = std::chrono::milliseconds::zero();
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kInvalidRequest);

    request.timeout = std::chrono::seconds(5);
    request.maximum_word_distance = kMaximumFullTextWordDistance + 1U;
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kInvalidRequest);

    request.maximum_word_distance.reset();
    request.query_text.assign(kMaximumFullTextQueryBytes + 1U, 'x');
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kInvalidRequest);

    request.query_text = "text";
    request.rendered_text.assign(kMaximumRenderedTextMatchPlanBytes + 1U, 'x');
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kInvalidRequest);

    request.rendered_text = "text";
    request.mode = static_cast<FullTextQueryMode>(999);
    result = facade->BuildRenderedTextMatchPlan(request);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kInvalidRequest);

    request.mode = FullTextQueryMode::kWholeWords;
    const CancelledToken cancelled;
    result = facade->BuildRenderedTextMatchPlan(request, &cancelled);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kCancelled);
    QVERIFY(result.ranges.empty());

    const SlowToken slow;
    request.rendered_text.assign(100U, 'x');
    request.query_text = "x";
    request.timeout = std::chrono::milliseconds(1);
    result = facade->BuildRenderedTextMatchPlan(request, &slow);
    QCOMPARE(result.error, RenderedTextMatchPlanError::kDeadlineExceeded);
    QVERIFY(result.ranges.empty());
}

}  // namespace
}  // namespace goldendict::core

using goldendict::core::ApplicationServiceTest;

QTEST_APPLESS_MAIN(ApplicationServiceTest)

#include "application_service_test.moc"
