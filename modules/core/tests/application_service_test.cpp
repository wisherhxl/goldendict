// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "goldendict/core/application.h"
#include "support/aard_fixture.h"
#include "support/bgl_fixture.h"
#include "support/dictd_fixture.h"
#include "support/dsl_fixture.h"
#include "support/epwing_fixture.h"
#include "support/gls_fixture.h"
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

class CancelledToken final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class ApplicationServiceTest : public QObject {
    Q_OBJECT

   private slots:
    void MissingConfigurationIsACleanProfile();
    void ConfigurationRoundTripsEscapedPaths();
    void ConfigurationRoundTripsDictionaryGroups();
    void ConfigurationRejectsGroupBoundsAndDuplicatesAtomically();
    void ConfigurationRejectsMalformedGroups();
    void MigratesLegacyPathsWithoutTouchingTheSource();
    void CurrentConfigurationTakesPrecedenceOverLegacy();
    void RejectsMalformedLegacyWithoutCreatingCurrent();
    void RejectsMalformedConfiguration();
    void DiscoversAndQueriesARealFixture();
    void ReturnsCanonicalFoldedMatchInformation();
    void ReturnsRankedPrefixMatches();
    void ReturnsLightweightHeadwordSuggestions();
    void RanksSuggestionsAcrossDictionaries();
    void AppliesResolvedDictionaryGroupsConsistently();
    void DiscoversAndQueriesDictdAlongsideStardict();
    void DiscoversSanitizesAndQueriesSdict();
    void DiscoversSanitizesAndQueriesXdxfResources();
    void DiscoversSanitizesAndQueriesGlsResources();
    void DiscoversSanitizesAndQueriesDslResources();
    void DiscoversSanitizesAndQueriesBglResources();
    void DiscoversSanitizesAndQueriesMdictResources();
    void DiscoversSanitizesAndQueriesAard();
    void DiscoversSanitizesAndQueriesZimResources();
    void DiscoversSanitizesAndQueriesSlobResources();
    void DiscoversSanitizesAndQueriesEpwingResources();
    void DiscoversSanitizesAndQueriesLsaAudio();
    void DiscoversSanitizesAndQueriesZipSoundsAudio();
    void QueriesExplicitlyConfiguredSoundDirectory();
    void CompletesAnOwnedAsynchronousLookup();
    void ResolvesTypedArticleUrlsBehindTheDesktopFacade();
    void ReportsCancellationAndUnavailableDictionaries();
    void RejectsUnboundedOrMalformedQueries();
};

std::filesystem::path TemporaryPath(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
}

void ApplicationServiceTest::MissingConfigurationIsACleanProfile() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const auto configuration =
        LoadConfiguration((TemporaryPath(directory) / "missing.conf").string());

    QVERIFY(configuration.dictionary_paths.empty());
    QVERIFY(configuration.index_directory.empty());
    QVERIFY(configuration.dictionary_groups.empty());
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
        "<preferences><zoomFactor>1.5</zoomFactor></preferences>"
        "</config>";
    test::WriteBinaryFile(legacy_path, legacy);

    const auto migrated = LoadOrMigrateConfiguration(
        current_path.string(), legacy_path.string(), "/cache/indexes");

    QCOMPARE(
        migrated.dictionary_paths,
        (std::vector<std::string>{"/dicts/English & French", "/dicts/CJK"}));
    QCOMPARE(migrated.index_directory, "/cache/indexes");
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
    QVERIFY(std::filesystem::exists(current_path));
    std::ifstream legacy_input(legacy_path, std::ios::binary);
    const std::string unchanged((std::istreambuf_iterator<char>(legacy_input)),
                                std::istreambuf_iterator<char>());
    QCOMPARE(unchanged, legacy);
    const auto round_trip = LoadConfiguration(current_path.string());
    QCOMPARE(round_trip.dictionary_paths, migrated.dictionary_paths);
    QCOMPARE(round_trip.sound_directories, migrated.sound_directories);
    QCOMPARE(round_trip.dictionary_groups, migrated.dictionary_groups);
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
    SaveConfiguration(current_path.string(), current);

    const auto loaded = LoadOrMigrateConfiguration(
        current_path.string(), legacy_path.string(), "/unused/indexes");

    QCOMPARE(loaded.dictionary_paths, current.dictionary_paths);
    QCOMPARE(loaded.index_directory, current.index_directory);
    QCOMPARE(loaded.dictionary_groups, current.dictionary_groups);
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
    QCOMPARE(catalog.front().source, (root / "fixture.ifo").string());
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

    query.text = "missing";
    const auto missing = service.Lookup(query);
    QVERIFY(missing.entries.empty());
    QVERIFY(missing.errors.empty());
}

void ApplicationServiceTest::ReturnsCanonicalFoldedMatchInformation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    test::WriteStardictFixture(root,
                               {{"Caf\xc3\xa9-au-lait", "folded definition"}});
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "CAFE AU LAIT";

    const auto response = service->Lookup(query);

    QCOMPARE(response.errors.size(), std::size_t{0});
    QCOMPARE(response.entries.size(), std::size_t{1});
    QCOMPARE(response.entries.front().match.requested_headword, "CAFE AU LAIT");
    QCOMPARE(response.entries.front().match.normalized_headword, "cafeaulait");
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
    const auto unavailable = service->Suggest(query);
    QVERIFY(unavailable.suggestions.empty());
    QCOMPARE(unavailable.errors.size(), std::size_t{1});
    QCOMPARE(unavailable.errors.front().code,
             LookupErrorCode::kDictionaryUnavailable);
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
            return dictionary.source.find("/first/") != std::string::npos;
        });
    const auto second = std::find_if(
        discovered.begin(), discovered.end(), [](const auto& dictionary) {
            return dictionary.source.find("/second/") != std::string::npos;
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

    lookup.dictionary_ids = {first->id};
    lookup_response = service.Lookup(lookup);
    QVERIFY(lookup_response.errors.empty());
    QCOMPARE(lookup_response.entries.size(), std::size_t{1});
    QCOMPARE(lookup_response.entries.front().dictionary.id, first->id);

    lookup.dictionary_ids.clear();
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
    test::WriteAardFixture(root);
    CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    auto service = CreateDictionaryService(configuration);
    LookupQuery query;
    query.text = "EXAMPLE";

    const auto catalog = service->GetCatalog();
    const auto response = service->Lookup(query);

    QCOMPARE(catalog.size(), std::size_t{1});
    QVERIFY(catalog.front().id.rfind("aard-", 0) == 0U);
    QCOMPARE(catalog.front().name, "Fixture Aard");
    QCOMPARE(catalog.front().description, "fixture");
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries.size(), std::size_t{1});
    const auto& entry = response.entries.front();
    QCOMPARE(entry.language.source_language, "en");
    QCOMPARE(entry.language.target_language, "de");
    QVERIFY(entry.article.sanitized_html->find("<b>definition</b>") !=
            std::string::npos);
    QVERIFY(entry.article.sanitized_html->find("goldendict://lookup/alias") !=
            std::string::npos);
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

}  // namespace
}  // namespace goldendict::core

using goldendict::core::ApplicationServiceTest;

QTEST_APPLESS_MAIN(ApplicationServiceTest)

#include "application_service_test.moc"
