// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <string>

#include "goldendict/core/application.h"
#include "support/bgl_fixture.h"
#include "support/dictd_fixture.h"
#include "support/dsl_fixture.h"
#include "support/gls_fixture.h"
#include "support/mdict_fixture.h"
#include "support/sdict_fixture.h"
#include "support/stardict_fixture.h"
#include "support/xdxf_fixture.h"

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
    void RejectsMalformedConfiguration();
    void DiscoversAndQueriesARealFixture();
    void ReturnsCanonicalFoldedMatchInformation();
    void ReturnsRankedPrefixMatches();
    void ReturnsLightweightHeadwordSuggestions();
    void RanksSuggestionsAcrossDictionaries();
    void DiscoversAndQueriesDictdAlongsideStardict();
    void DiscoversSanitizesAndQueriesSdict();
    void DiscoversSanitizesAndQueriesXdxfResources();
    void DiscoversSanitizesAndQueriesGlsResources();
    void DiscoversSanitizesAndQueriesDslResources();
    void DiscoversSanitizesAndQueriesBglResources();
    void DiscoversSanitizesAndQueriesMdictResources();
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
}

void ApplicationServiceTest::ConfigurationRoundTripsEscapedPaths() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = TemporaryPath(directory) / "nested" / "core.conf";
    CoreConfiguration expected;
    expected.dictionary_paths = {"/dictionaries/English & French",
                                 "/dictionaries/CJK=demo"};
    expected.index_directory = "/cache/index files";

    SaveConfiguration(path.string(), expected);
    const auto actual = LoadConfiguration(path.string());

    QCOMPARE(actual.dictionary_paths, expected.dictionary_paths);
    QCOMPARE(actual.index_directory, expected.index_directory);
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
