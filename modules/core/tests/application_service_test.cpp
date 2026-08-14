// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "goldendict/core/application.h"
#include "goldendict/core/headword_export.h"
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
    void ConfigurationRoundTripsBoundedMainWindowGeometry();
    void ConfigurationRejectsMalformedPreferencesAtomically();
    void ConfigurationRejectsGroupBoundsAndDuplicatesAtomically();
    void ConfigurationRejectsMalformedGroups();
    void MigratesLegacyPathsWithoutTouchingTheSource();
    void MigratesAllLegacyOnlineSourcesAtomically();
    void RejectsUnsupportedLegacyOnlineSourcesAtomically();
    void MissingLegacyPreferencesRetainCurrentDefaults();
    void RejectsMalformedLegacyPreferencesAtomically();
    void CurrentConfigurationTakesPrecedenceOverLegacy();
    void RejectsMalformedLegacyWithoutCreatingCurrent();
    void RejectsMalformedConfiguration();
    void CatalogSanitizesInspectionMetadata();
    void DiscoversAndQueriesARealFixture();
    void EnumeratesStarDictHeadwordsWithStableCursors();
    void ExportsCompleteHeadwordListsAtomically();
    void ReturnsCanonicalFoldedMatchInformation();
    void ReturnsRankedPrefixMatches();
    void ReturnsLightweightHeadwordSuggestions();
    void FiltersBoundedHeadwordSuggestionsWithWildcards();
    void FiltersBoundedHeadwordSuggestionsWithRegularExpressions();
    void RejectsInvalidOrUnseededHeadwordPatterns();
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
        "https://example.test/find%3Fq%3D%25GDWORD%25%26lang%3Den\n"
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
    expected.article_tab_session =
        ArticleTabSession{{{7U, {lookup, link}, 1U}, {20U, {lookup}, 0U}}, 20U};

    SaveConfiguration(path.string(), expected);
    const std::string canonical = ReadFile(path);
    const auto actual = LoadConfiguration(path.string());
    QVERIFY(actual.article_tab_session.has_value());
    QCOMPARE(*actual.article_tab_session, *expected.article_tab_session);
    SaveConfiguration(path.string(), actual);
    QCOMPARE(ReadFile(path), canonical);

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

    const std::vector<std::string> malformed = {
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
        "article_tab_session=18446744073709551615\n"
        "article_tab=18446744073709551615|0\n"
        "article_tab_navigation=18446744073709551615|1|word|0|word|||||\n",
    };
    for (const auto& fields : malformed) {
        test::WriteBinaryFile(path, "goldendict-core-config-v1\n" + fields);
        QVERIFY_EXCEPTION_THROWN(LoadConfiguration(path.string()),
                                 std::runtime_error);
    }
}

void ApplicationServiceTest::ApplicationPreferencesCompareByValue() {
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
    preferences.zoom_factor = 1.375;
    preferences.help_zoom_factor = 0.75;
    preferences.words_zoom_level = -2;
    preferences.maximum_history_entries = 1234U;
    preferences.collapse_large_articles = true;
    preferences.article_size_limit = 4096U;
    preferences.synonym_search_enabled = false;
    preferences.full_text_search_mode = FullTextSearchMode::kRegularExpression;
    preferences.full_text_match_case = true;
    preferences.full_text_maximum_word_distance = 9U;
    preferences.full_text_disabled_types = "audio|images";

    SaveConfiguration(path.string(), expected);
    const std::string first = ReadFile(path);
    const auto actual = LoadConfiguration(path.string());

    QCOMPARE(actual.preferences.interface_language,
             preferences.interface_language);
    QCOMPARE(actual.preferences.display_style, preferences.display_style);
    QCOMPARE(actual.preferences.open_new_tabs_after_current, true);
    QCOMPARE(actual.preferences.open_new_tabs_in_background, false);
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
    QCOMPARE(actual.preferences.synonym_search_enabled,
             preferences.synonym_search_enabled);
    QCOMPARE(actual.preferences.full_text_search_mode,
             preferences.full_text_search_mode);
    QCOMPARE(actual.preferences.full_text_match_case,
             preferences.full_text_match_case);
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
    QCOMPARE(older.preferences.zoom_factor, 1.0);
    QCOMPARE(older.preferences.maximum_history_entries, std::uint32_t{500});
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
    invalid.preferences.zoom_factor = 5.01;
    QVERIFY_EXCEPTION_THROWN(SaveConfiguration(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(ReadFile(path), original_bytes);

    const std::vector<std::string> malformed = {
        "preference=enable_tray_icon|true\n",
        "preference=open_new_tabs_in_background|true\n",
        "preference=proxy_type|9\n",
        "preference=full_text_search_mode|3\n",
        std::string("preference=interface_language|bad\xc3\x28\n"),
        "preference=zoom_factor|nan\n",
        "preference=scan_popup_modifiers|65535\n",
        "preference=unknown_future_key|1\n",
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
        "<preferences>"
        "<interfaceLanguage>zh_CN</interfaceLanguage>"
        "<helpLanguage>fr_FR</helpLanguage>"
        "<displayStyle>dark</displayStyle><addonStyle>contrast.css</addonStyle>"
        "<hideMenubar>1</hideMenubar><enableTrayIcon>0</enableTrayIcon>"
        "<doubleClickTranslates>0</doubleClickTranslates>"
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
        "<zoomFactor>1.375</zoomFactor><helpZoomFactor>0.75</helpZoomFactor>"
        "<wordsZoomLevel>-2</wordsZoomLevel>"
        "<maxStringsInHistory>1234</maxStringsInHistory>"
        "<collapseBigArticles>1</collapseBigArticles>"
        "<articleSizeLimit>4096</articleSizeLimit>"
        "<synonymSearchEnabled>0</synonymSearchEnabled>"
        "<newTabsOpenAfterCurrentOne>1</newTabsOpenAfterCurrentOne>"
        "<newTabsOpenInBackground>0</newTabsOpenInBackground>"
        "<fullTextSearch><searchMode>2</searchMode><matchCase>1</matchCase>"
        "<maxDistanceBetweenWords>9</maxDistanceBetweenWords>"
        "<disabledTypes>audio|images</disabledTypes>"
        "<dialogGeometry>excluded</dialogGeometry></fullTextSearch>"
        "</preferences><mainWindowGeometry>Z2VvbWV0cnk=</mainWindowGeometry>"
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
    const auto& preferences = migrated.preferences;
    QCOMPARE(preferences.interface_language, "zh_CN");
    QCOMPARE(preferences.help_language, "fr_FR");
    QCOMPARE(preferences.display_style, "dark");
    QCOMPARE(preferences.addon_style, "contrast.css");
    QCOMPARE(preferences.open_new_tabs_after_current, true);
    QCOMPARE(preferences.open_new_tabs_in_background, false);
    QCOMPARE(preferences.hide_menubar, true);
    QCOMPARE(preferences.enable_tray_icon, false);
    QCOMPARE(preferences.double_click_translates, false);
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
    QCOMPARE(preferences.zoom_factor, 1.375);
    QCOMPARE(preferences.help_zoom_factor, 0.75);
    QCOMPARE(preferences.words_zoom_level, std::int32_t{-2});
    QCOMPARE(preferences.maximum_history_entries, std::uint32_t{1234});
    QCOMPARE(preferences.collapse_large_articles, true);
    QCOMPARE(preferences.article_size_limit, std::uint32_t{4096});
    QCOMPARE(preferences.synonym_search_enabled, false);
    QCOMPARE(preferences.full_text_search_mode,
             FullTextSearchMode::kRegularExpression);
    QCOMPARE(preferences.full_text_match_case, true);
    QCOMPARE(preferences.full_text_maximum_word_distance, std::uint32_t{9});
    QCOMPARE(preferences.full_text_disabled_types, "audio|images");
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
    QCOMPARE(round_trip.main_window_geometry, migrated.main_window_geometry);
}

void ApplicationServiceTest::MigratesAllLegacyOnlineSourcesAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto legacy_path = root / "config";
    const auto current_path = root / "core.conf";
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
        "<program id=\"plain\" name=\"Plain\" "
        "commandLine=\"/usr/bin/tool &quot;two words&quot; "
        "&quot;&quot; %GDWORD%\" enabled=\"1\" type=\"1\" icon=\"x\"/>"
        "<program id=\"html\" name=\"HTML\" commandLine=\"/opt/render --html\" "
        "enabled=\"0\" type=\"2\" icon=\"\"/>"
        "<program id=\"prefix\" name=\"Prefix\" "
        "commandLine=\"/opt/prefix %GDWORD%\" enabled=\"1\" type=\"3\" "
        "icon=\"\"/>"
        "<program id=\"quotes\" name=\"Quotes\" "
        "commandLine=\"/opt/quote &quot;a&quot;&quot;b&quot; &quot;open a|b\" "
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
    QCOMPARE(migrated.external_program_sources[0].executable, "/usr/bin/tool");
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
    QVERIFY(migrated.main_window_geometry.empty());
    QCOMPARE(LoadConfiguration(current_path.string()).preferences, defaults);
    QCOMPARE(ReadFile(legacy_path), legacy);
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
        "<zoomFactor>nan</zoomFactor>",
        "<wordsZoomLevel>2x</wordsZoomLevel>",
        "<scanPopupModifiers>65535</scanPopupModifiers>",
        "<scanPopupUnpinnedWindowFlags>3</scanPopupUnpinnedWindowFlags>",
        "<internalPlayerBackend>unknown</internalPlayerBackend>",
        "<articleSizeLimit>0</articleSizeLimit>",
        "<interfaceLanguage><nested/></interfaceLanguage>",
        "<storeHistory>1</storeHistory><storeHistory>0</storeHistory>",
        "<proxyserver enabled=\"yes\" useSystemProxy=\"0\"/>",
        "<proxyserver useSystemProxy=\"0\"/>",
        "<proxyserver enabled=\"1\"/>",
        "<proxyserver enabled=\"0\" useSystemProxy=\"0\"/>"
        "<proxyserver enabled=\"1\" useSystemProxy=\"0\"/>",
        "<proxyserver enabled=\"1\" useSystemProxy=\"0\"><type>9</type>"
        "<host>proxy</host><port>80</port></proxyserver>",
        "<fullTextSearch><searchMode>3</searchMode></fullTextSearch>",
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
    QVERIFY(catalog.front().supports_headword_enumeration);
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
