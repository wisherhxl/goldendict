// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QSemaphore>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "../../core/tests/support/stardict_fixture.h"
#include "goldendict/core/application.h"
#include "goldendict/network/runtime_composition.h"

namespace goldendict::network {
namespace {

std::string MissingExecutable(std::string_view name) {
#ifdef _WIN32
    auto path = std::filesystem::path("C:/definitely/missing");
#else
    auto path = std::filesystem::path("/definitely/missing");
#endif
    path /= name;
    return path.string();
}

class RuntimeFixture final : public QObject {
   public:
    RuntimeFixture() {
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            while (server_.hasPendingConnections()) {
                auto* socket = server_.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                    QByteArray request =
                        socket->property("request").toByteArray();
                    request += socket->readAll();
                    if (!request.contains("\r\n\r\n")) {
                        socket->setProperty("request", request);
                        return;
                    }
                    const QByteArray target = request.split(' ').value(1);
                    QByteArray content_type = "text/html; charset=utf-8";
                    QByteArray body =
                        "<p>Website <script>unsafe()</script>article</p>";
                    if (target.contains("action=parse")) {
                        content_type = "application/json";
                        body =
                            R"({"parse":{"title":"Alpha","text":"<p>Wiki <script>unsafe()</script>article</p>"}})";
                    } else if (target.startsWith("/forvo/key/")) {
                        content_type = "application/xml";
                        body = "<items><item><pathmp3>http://127.0.0.1:" +
                               QByteArray::number(static_cast<unsigned int>(
                                   socket->localPort())) +
                               "/audio/test.mp3</pathmp3><username>Alice</"
                               "username>"
                               "<country>UK</country><sex>f</sex>"
                               "<num_votes>3</num_votes>"
                               "<num_positive_votes>2</num_positive_votes>"
                               "</item></items>";
                    } else if (target == "/audio/test.mp3") {
                        content_type = "audio/mpeg";
                        body = "fixture-audio";
                    }
                    const QByteArray response =
                        "HTTP/1.1 200 OK\r\nContent-Type: " + content_type +
                        "\r\nContent-Length: " +
                        QByteArray::number(body.size()) +
                        "\r\nConnection: close\r\n\r\n" + body;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            qFatal("Could not start runtime composition fixture");
        }
    }

    std::string BaseUrl() const {
        return "http://127.0.0.1:" +
               std::to_string(static_cast<unsigned int>(server_.serverPort()));
    }

   private:
    QTcpServer server_;
};

class ExternalProgramFixture final {
   public:
    std::string Path() const { return GOLDENDICT_EXTERNAL_PROGRAM_TEST_HELPER; }

    std::string Directory() const {
        auto directory = std::filesystem::path(Path()).parent_path();
        directory.make_preferred();
        return directory.string();
    }
};

class Cancelled final : public goldendict::core::RuntimeCancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class DictFixtureThread final : public QThread {
   public:
    void WaitUntilReady() { ready_.acquire(); }

    unsigned short Port() const { return port_; }

   protected:
    void run() override {
        QTcpServer server;
        connect(&server, &QTcpServer::newConnection, &server, [&server]() {
            while (server.hasPendingConnections()) {
                auto* socket = server.nextPendingConnection();
                socket->write("220 fixture ready <id@test>\r\n");
                connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                    QByteArray commands =
                        socket->property("commands").toByteArray();
                    commands += socket->readAll();
                    while (true) {
                        const qsizetype end = commands.indexOf("\r\n");
                        if (end < 0) {
                            break;
                        }
                        const QByteArray command = commands.left(end);
                        commands.remove(0, end + 2);
                        if (command == "CLIENT GoldenDict" ||
                            command == "OPTION MIME") {
                            socket->write("250 ok\r\n");
                        } else if (command == "DEFINE * \"Alpha\"") {
                            socket->write(
                                "150 1 definitions retrieved\r\n"
                                "151 \"Alpha\" db \"Fixture\"\r\n"
                                "Content-Type: text/plain\r\n\r\n"
                                "DICT article\r\n.\r\n250 ok\r\n");
                        } else if (command == "MATCH * prefix \"Al\"") {
                            socket->write(
                                "152 1 matches found\r\n"
                                "db \"Alpha\"\r\n.\r\n250 ok\r\n");
                        } else if (command == "QUIT") {
                            socket->write("221 bye\r\n");
                        } else {
                            socket->write("552 no match\r\n");
                        }
                    }
                    socket->setProperty("commands", commands);
                });
            }
        });
        if (server.listen(QHostAddress::LocalHost, 0)) {
            port_ = static_cast<unsigned short>(server.serverPort());
        }
        ready_.release();
        if (port_ != 0U) {
            exec();
        }
    }

   private:
    QSemaphore ready_;
    unsigned short port_ = 0U;
};

class DictFixture final {
   public:
    DictFixture() {
        thread_.start();
        thread_.WaitUntilReady();
        if (thread_.Port() == 0U) {
            qFatal("Could not start runtime DICT fixture");
        }
    }

    ~DictFixture() {
        thread_.quit();
        thread_.wait();
    }

    unsigned short Port() const { return thread_.Port(); }

   private:
    DictFixtureThread thread_;
};

class StubRuntimeSource final
    : public goldendict::core::RuntimeDictionarySource {
   public:
    explicit StubRuntimeSource(std::string id) { identity_.id = std::move(id); }

    const goldendict::core::RuntimeDictionaryIdentity& identity()
        const noexcept override {
        return identity_;
    }

    std::vector<goldendict::core::RuntimeDictionaryArticle> LookupExact(
        std::string_view,
        const goldendict::core::RuntimeRequestOptions&) const override {
        return {};
    }

    std::vector<goldendict::core::RuntimeDictionaryArticle> LookupPrefix(
        std::string_view,
        const goldendict::core::RuntimeRequestOptions&) const override {
        return {};
    }

    std::vector<std::string> SuggestPrefix(
        std::string_view,
        const goldendict::core::RuntimeRequestOptions&) const override {
        return {};
    }

    std::optional<goldendict::core::RuntimeDictionaryResource> GetResource(
        std::string_view,
        const goldendict::core::RuntimeRequestOptions&) const override {
        return std::nullopt;
    }

   private:
    goldendict::core::RuntimeDictionaryIdentity identity_;
};

void VerifyRequestError(goldendict::core::RuntimeSourceErrorCode expected,
                        const std::function<void()>& operation) {
    try {
        operation();
        QFAIL("Expected runtime source request error");
    } catch (const goldendict::core::RuntimeSourceError& error) {
        QCOMPARE(error.code(), expected);
    }
}

void VerifyAllMethodsReject(
    const goldendict::core::RuntimeDictionarySource& source,
    const goldendict::core::RuntimeRequestOptions& options,
    goldendict::core::RuntimeSourceErrorCode expected) {
    VerifyRequestError(expected,
                       [&]() { source.LookupExact("word", options); });
    VerifyRequestError(expected,
                       [&]() { source.LookupPrefix("word", options); });
    VerifyRequestError(expected,
                       [&]() { source.SuggestPrefix("word", options); });
    VerifyRequestError(expected,
                       [&]() { source.GetResource("resource", options); });
}

}  // namespace

class RuntimeCompositionTest : public QObject {
    Q_OBJECT

   private slots:
    void PreservesEnabledFamilyOrderAndIdentity();
    void ReusesAdaptersAndCoreSanitization();
    void HonorsAllRequestOptionsBeforeNetworkActivity();
    void HonorsZeroResultLimitWithoutNetworkActivity();
    void AcceptsGenericUnconfiguredExtensionIdentity();
    void RejectsNullAndEmptyIdentityWithoutProducingAService();
    void RejectsLocalRuntimeCollisionWithoutProducingAService();
    void RejectsDuplicateInjectionWithoutProducingAService();
    void ComposesForvoLanguagesAndDictInFamilyOrder();
    void ReportsMissingCredentialAndRejectsInvalidInjection();
    void ReusesForvoAdapterForLookupAndResources();
    void ReusesDictAdapterForLookupSuggestionsAndResources();
    void ComposesExternalProgramsAfterNetworkFamilies();
    void MapsExternalProgramOutputWithoutAShell();
    void TranslatesExternalProgramRequestFailures();
    void ComposesCompleteApplicationFacadeAtomically();
};

void RuntimeCompositionTest::PreservesEnabledFamilyOrderAndIdentity() {
    goldendict::core::CoreConfiguration configuration;
    configuration.mediawiki_sources = {
        {"wiki.first", "Wiki First", true, "https://first.example/wiki"},
        {"wiki.disabled", "Wiki Disabled", false,
         "https://disabled.example/wiki"},
        {"wiki.second", "Wiki Second", true, "https://second.example/wiki"}};
    configuration.website_sources = {
        {"site.first", "Site First", true,
         "https://first.example/lookup?q=%GDWORD%"},
        {"site.disabled", "Site Disabled", false,
         "https://disabled.example/lookup?q=%GDWORD%"},
        {"site.second", "Site Second", true,
         "https://second.example/lookup?q=%GDWORD%"}};
    const auto original = configuration;

    auto composition = ComposeConfiguredRuntimeSources(configuration);
    QCOMPARE(composition.sources.size(), std::size_t{4});
    auto service = goldendict::core::CreateDictionaryService(
        configuration, std::move(composition.sources));
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{4});
    QCOMPARE(catalog[0].id, std::string("wiki.first"));
    QCOMPARE(catalog[0].name, std::string("Wiki First"));
    QCOMPARE(catalog[1].id, std::string("wiki.second"));
    QCOMPARE(catalog[2].id, std::string("site.first"));
    QCOMPARE(catalog[3].id, std::string("site.second"));
    QCOMPARE(configuration.mediawiki_sources, original.mediawiki_sources);
    QCOMPARE(configuration.website_sources, original.website_sources);
}

void RuntimeCompositionTest::ReusesAdaptersAndCoreSanitization() {
    RuntimeFixture fixture;
    goldendict::core::CoreConfiguration configuration;
    configuration.mediawiki_sources = {
        {"wiki", "Wiki", true, fixture.BaseUrl() + "/wiki"}};
    configuration.website_sources = {
        {"site", "Site", true, fixture.BaseUrl() + "/lookup?q=%GDWORD%"}};
    auto service = goldendict::core::CreateDictionaryService(
        configuration,
        std::move(ComposeConfiguredRuntimeSources(configuration).sources));

    goldendict::core::LookupQuery query;
    query.text = "Alpha";
    query.result_limit = 2U;
    const auto response = service->Lookup(query);
    QCOMPARE(response.entries.size(), std::size_t{2});
    QVERIFY(response.errors.empty());
    QCOMPARE(response.entries[0].dictionary.id, std::string("wiki"));
    QCOMPARE(response.entries[1].dictionary.id, std::string("site"));
    QVERIFY(response.entries[0].article.sanitized_html.has_value());
    QVERIFY(response.entries[0].article.sanitized_html->find("<script") ==
            std::string::npos);
    QVERIFY(response.entries[1].article.sanitized_html->find("<script") ==
            std::string::npos);
}

void RuntimeCompositionTest::HonorsAllRequestOptionsBeforeNetworkActivity() {
    goldendict::core::CoreConfiguration configuration;
    configuration.mediawiki_sources = {
        {"wiki", "Wiki", true, "https://example.test/wiki"}};
    configuration.website_sources = {
        {"site", "Site", true, "https://example.test/?q=%GDWORD%"}};
    configuration.forvo_sources = {
        {"forvo", "Forvo", true, "https://example.test", {"en"}}};
    configuration.dict_server_sources = {
        {"dict", "DICT", true, "127.0.0.1", 2628U, "*", "prefix"}};
    auto sources = std::move(
        ComposeConfiguredRuntimeSources(configuration, {{"forvo", "secret"}})
            .sources);
    Cancelled cancelled;
    goldendict::core::RuntimeRequestOptions cancelled_options;
    cancelled_options.cancellation = &cancelled;
    for (const auto& source : sources) {
        VerifyAllMethodsReject(
            *source, cancelled_options,
            goldendict::core::RuntimeSourceErrorCode::kCancelled);
    }

    goldendict::core::RuntimeRequestOptions expired_options;
    expired_options.deadline = std::chrono::steady_clock::time_point::min();
    for (const auto& source : sources) {
        VerifyAllMethodsReject(
            *source, expired_options,
            goldendict::core::RuntimeSourceErrorCode::kDeadlineExceeded);
    }
}

void RuntimeCompositionTest::HonorsZeroResultLimitWithoutNetworkActivity() {
    goldendict::core::CoreConfiguration configuration;
    configuration.mediawiki_sources = {
        {"wiki", "Wiki", true, "https://example.test/wiki"}};
    configuration.website_sources = {
        {"site", "Site", true, "https://example.test/?q=%GDWORD%"}};
    configuration.forvo_sources = {
        {"forvo", "Forvo", true, "https://example.test", {"en"}}};
    configuration.dict_server_sources = {
        {"dict", "DICT", true, "127.0.0.1", 2628U, "*", "prefix"}};
    auto sources = std::move(
        ComposeConfiguredRuntimeSources(configuration, {{"forvo", "secret"}})
            .sources);
    goldendict::core::RuntimeRequestOptions options;
    options.result_limit = 0U;
    for (const auto& source : sources) {
        QVERIFY(source->LookupExact("word", options).empty());
        QVERIFY(source->LookupPrefix("word", options).empty());
        QVERIFY(source->SuggestPrefix("word", options).empty());
    }
}

void RuntimeCompositionTest::AcceptsGenericUnconfiguredExtensionIdentity() {
    goldendict::core::CoreConfiguration configuration;
    std::vector<std::unique_ptr<goldendict::core::RuntimeDictionarySource>>
        sources;
    sources.push_back(std::make_unique<StubRuntimeSource>("derived.forvo.en"));
    auto service = goldendict::core::CreateDictionaryService(
        configuration, std::move(sources));
    const auto catalog = service->GetCatalog();
    QCOMPARE(catalog.size(), std::size_t{1});
    QCOMPARE(catalog.front().id, std::string("derived.forvo.en"));
}

void RuntimeCompositionTest::
    RejectsNullAndEmptyIdentityWithoutProducingAService() {
    goldendict::core::CoreConfiguration configuration;
    std::vector<std::unique_ptr<goldendict::core::RuntimeDictionarySource>>
        null_source;
    null_source.push_back(nullptr);
    QVERIFY_EXCEPTION_THROWN(goldendict::core::CreateDictionaryService(
                                 configuration, std::move(null_source)),
                             std::runtime_error);

    std::vector<std::unique_ptr<goldendict::core::RuntimeDictionarySource>>
        empty_id;
    empty_id.push_back(std::make_unique<StubRuntimeSource>(""));
    QVERIFY_EXCEPTION_THROWN(goldendict::core::CreateDictionaryService(
                                 configuration, std::move(empty_id)),
                             std::runtime_error);

    QVERIFY(goldendict::core::CreateDictionaryService(configuration)
                ->GetCatalog()
                .empty());
}

void RuntimeCompositionTest::
    RejectsLocalRuntimeCollisionWithoutProducingAService() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    goldendict::core::test::WriteStardictFixture(root, {{"word", "article"}});
    goldendict::core::CoreConfiguration configuration;
    configuration.dictionary_paths = {root.string()};
    const auto original =
        goldendict::core::CreateDictionaryService(configuration)->GetCatalog();
    QCOMPARE(original.size(), std::size_t{1});

    std::vector<std::unique_ptr<goldendict::core::RuntimeDictionarySource>>
        collision;
    collision.push_back(
        std::make_unique<StubRuntimeSource>(original.front().id));
    QVERIFY_EXCEPTION_THROWN(goldendict::core::CreateDictionaryService(
                                 configuration, std::move(collision)),
                             std::runtime_error);

    QCOMPARE(goldendict::core::CreateDictionaryService(configuration)
                 ->GetCatalog()
                 .front()
                 .id,
             original.front().id);
}

void RuntimeCompositionTest::
    RejectsDuplicateInjectionWithoutProducingAService() {
    goldendict::core::CoreConfiguration configuration;
    configuration.mediawiki_sources = {
        {"wiki", "Wiki", true, "https://example.test/wiki"}};
    auto sources =
        std::move(ComposeConfiguredRuntimeSources(configuration).sources);
    auto duplicate =
        std::move(ComposeConfiguredRuntimeSources(configuration).sources);
    sources.push_back(std::move(duplicate.front()));

    QVERIFY_EXCEPTION_THROWN(goldendict::core::CreateDictionaryService(
                                 configuration, std::move(sources)),
                             std::runtime_error);

    auto unchanged = goldendict::core::CreateDictionaryService(configuration);
    QVERIFY(unchanged->GetCatalog().empty());
}

void RuntimeCompositionTest::ComposesForvoLanguagesAndDictInFamilyOrder() {
    goldendict::core::CoreConfiguration configuration;
    configuration.mediawiki_sources = {
        {"wiki", "Wiki", true, "https://example.test/wiki"}};
    configuration.website_sources = {
        {"site", "Site", true, "https://example.test/?q=%GDWORD%"}};
    configuration.forvo_sources = {
        {"pronunciation", "Forvo", true, "https://example.test", {"en", "ru"}}};
    configuration.dict_server_sources = {
        {"dict", "DICT", true, "127.0.0.1", 2628U, "*", "prefix"}};
    const auto original = configuration;

    auto composition = ComposeConfiguredRuntimeSources(
        configuration, {{"pronunciation", "secret"}});
    QVERIFY(composition.diagnostics.empty());
    QCOMPARE(composition.sources.size(), std::size_t{5});
    QCOMPARE(composition.sources[0]->identity().id, std::string("wiki"));
    QCOMPARE(composition.sources[1]->identity().id, std::string("site"));
    QCOMPARE(composition.sources[2]->identity().id,
             std::string("forvo:13:pronunciation:656e"));
    QCOMPARE(composition.sources[2]->identity().description,
             std::string("Configured Forvo source ID: pronunciation"));
    QCOMPARE(composition.sources[2]->identity().source_language,
             std::string("en"));
    QCOMPARE(composition.sources[3]->identity().id,
             std::string("forvo:13:pronunciation:7275"));
    QCOMPARE(composition.sources[4]->identity().id, std::string("dict"));
    QCOMPARE(configuration.forvo_sources, original.forvo_sources);
}

void RuntimeCompositionTest::
    ReportsMissingCredentialAndRejectsInvalidInjection() {
    goldendict::core::CoreConfiguration configuration;
    configuration.forvo_sources = {
        {"forvo", "Forvo", true, "https://example.test", {"en"}}};
    configuration.mediawiki_sources = {
        {"wiki", "Wiki", true, "https://example.test/wiki"}};
    const auto original = configuration;
    auto missing = ComposeConfiguredRuntimeSources(configuration);
    QCOMPARE(missing.sources.size(), std::size_t{1});
    QCOMPARE(missing.sources.front()->identity().id, std::string("wiki"));
    QCOMPARE(missing.diagnostics.size(), std::size_t{1});
    QCOMPARE(missing.diagnostics.front().code,
             RuntimeCompositionDiagnosticCode::kMissingForvoCredential);
    QCOMPARE(missing.diagnostics.front().source_id, std::string("forvo"));
    QCOMPARE(configuration.forvo_sources, original.forvo_sources);

    const std::string sentinel = "DO_NOT_DISCLOSE_SENTINEL";
    try {
        static_cast<void>(ComposeConfiguredRuntimeSources(
            configuration, {{"unknown", sentinel}}));
        QFAIL("Expected invalid credential injection");
    } catch (const std::invalid_argument& error) {
        QVERIFY(std::string(error.what()).find(sentinel) == std::string::npos);
    }
    QVERIFY_EXCEPTION_THROWN(
        ComposeConfiguredRuntimeSources(configuration, {{"wiki", sentinel}}),
        std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(
        ComposeConfiguredRuntimeSources(configuration, {{"forvo", ""}}),
        std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(
        ComposeConfiguredRuntimeSources(configuration,
                                        {{"forvo", std::string("\xff", 1U)}}),
        std::invalid_argument);
}

void RuntimeCompositionTest::ReusesForvoAdapterForLookupAndResources() {
    RuntimeFixture fixture;
    goldendict::core::CoreConfiguration configuration;
    configuration.forvo_sources = {
        {"forvo", "Forvo", true, fixture.BaseUrl() + "/forvo", {"en"}}};
    auto composition = ComposeConfiguredRuntimeSources(
        configuration, {{"forvo", "DO_NOT_DISCLOSE_SENTINEL"}});
    QCOMPARE(composition.sources.size(), std::size_t{1});
    const auto& source = *composition.sources.front();
    QVERIFY(source.identity().source.find("DO_NOT_DISCLOSE_SENTINEL") ==
            std::string::npos);
    QVERIFY(source.identity().description.find("DO_NOT_DISCLOSE_SENTINEL") ==
            std::string::npos);

    const auto articles = source.LookupExact("Alpha");
    QCOMPARE(articles.size(), std::size_t{1});
    const auto start = articles.front().data.find("audio/");
    QVERIFY(start != std::string::npos);
    const auto end = articles.front().data.find('"', start);
    const auto resource_id = articles.front().data.substr(start, end - start);
    const auto resource = source.GetResource(resource_id);
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, std::string("audio/mpeg"));
    QCOMPARE(resource->data.size(), std::size_t{13});
}

void RuntimeCompositionTest::
    ReusesDictAdapterForLookupSuggestionsAndResources() {
    DictFixture fixture;
    goldendict::core::CoreConfiguration configuration;
    configuration.dict_server_sources = {
        {"dict", "DICT", true, "127.0.0.1", fixture.Port(), "*", "prefix"}};
    auto composition = ComposeConfiguredRuntimeSources(configuration);
    QCOMPARE(composition.sources.size(), std::size_t{1});
    const auto& source = *composition.sources.front();
    const auto articles = source.LookupExact("Alpha");
    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().format, std::string("text"));
    QCOMPARE(articles.front().data, std::string("DICT article"));
    QCOMPARE(source.LookupPrefix("Alpha").size(), std::size_t{1});
    QCOMPARE(source.SuggestPrefix("Al"), std::vector<std::string>({"Alpha"}));
    QVERIFY(!source.GetResource("unused").has_value());
}

void RuntimeCompositionTest::ComposesExternalProgramsAfterNetworkFamilies() {
    const auto first_program = MissingExecutable("first");
    goldendict::core::CoreConfiguration configuration;
    configuration.mediawiki_sources = {
        {"wiki", "Wiki", true, "https://example.test/wiki"}};
    configuration.website_sources = {
        {"site", "Site", true, "https://example.test/?q=%GDWORD%"}};
    configuration.forvo_sources = {
        {"forvo", "Forvo", true, "https://example.test", {"en"}}};
    configuration.dict_server_sources = {
        {"dict", "DICT", true, "127.0.0.1", 2628U, "*", "prefix"}};
    configuration.external_program_sources = {
        {"program.first",
         "Program First",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         first_program,
         {"%GDWORD%"},
         ""},
        {"program.disabled",
         "Program Disabled",
         false,
         goldendict::core::ExternalProgramOutputKind::kHtml,
         MissingExecutable("disabled"),
         {},
         ""},
        {"program.second",
         "Program Second",
         true,
         goldendict::core::ExternalProgramOutputKind::kPrefixMatch,
         MissingExecutable("second"),
         {},
         ""}};
    const auto original = configuration;

    auto composition =
        ComposeConfiguredRuntimeSources(configuration, {{"forvo", "secret"}});
    QCOMPARE(composition.sources.size(), std::size_t{6});
    QCOMPARE(composition.sources[0]->identity().id, std::string("wiki"));
    QCOMPARE(composition.sources[1]->identity().id, std::string("site"));
    QCOMPARE(composition.sources[2]->identity().id,
             std::string("forvo:5:forvo:656e"));
    QCOMPARE(composition.sources[3]->identity().id, std::string("dict"));
    QCOMPARE(composition.sources[4]->identity().id,
             std::string("program.first"));
    QCOMPARE(composition.sources[4]->identity().name,
             std::string("Program First"));
    QCOMPARE(composition.sources[4]->identity().source, first_program);
    QCOMPARE(composition.sources[5]->identity().id,
             std::string("program.second"));
    QCOMPARE(configuration.external_program_sources,
             original.external_program_sources);
}

void RuntimeCompositionTest::MapsExternalProgramOutputWithoutAShell() {
    ExternalProgramFixture fixture;
    goldendict::core::CoreConfiguration configuration;
    configuration.external_program_sources = {
        {"text",
         "Text",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         fixture.Path(),
         {"argv", "prefix:%GDWORD%:%GDWORD%:suffix"},
         ""},
        {"stdin",
         "Stdin",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         fixture.Path(),
         {"stdin"},
         ""},
        {"html",
         "HTML",
         true,
         goldendict::core::ExternalProgramOutputKind::kHtml,
         fixture.Path(),
         {"html"},
         ""},
        {"prefix",
         "Prefix",
         true,
         goldendict::core::ExternalProgramOutputKind::kPrefixMatch,
         fixture.Path(),
         {"prefix"},
         ""},
        {"working",
         "Working",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         fixture.Path(),
         {"pwd", "%GDWORD%"},
         fixture.Directory()}};
    auto composition = ComposeConfiguredRuntimeSources(configuration);
    QCOMPARE(composition.sources.size(), std::size_t{5});

    const std::string literal = "A&B;$(touch should-not-run)";
    const auto text = composition.sources[0]->LookupExact(literal);
    QCOMPARE(text.size(), std::size_t{1});
    QCOMPARE(text.front().format, std::string("text"));
    QCOMPARE(text.front().data,
             std::string("prefix:") + literal + ":" + literal + ":suffix");
    QCOMPARE(composition.sources[1]->LookupPrefix("héllo").front().data,
             std::string("héllo"));
    QCOMPARE(composition.sources[2]->LookupExact("word").front().format,
             std::string("html"));

    goldendict::core::RuntimeRequestOptions options;
    options.result_limit = 2U;
    QCOMPARE(composition.sources[3]->SuggestPrefix("Al", options),
             std::vector<std::string>({"Alpha", "Alpine"}));
    QVERIFY(composition.sources[3]->LookupExact("Al").empty());
    QVERIFY(composition.sources[3]->LookupPrefix("Al").empty());
    QVERIFY(composition.sources[0]->SuggestPrefix("Al").empty());
    QVERIFY(!composition.sources[0]->GetResource("unused").has_value());
    auto working = composition.sources[4]->LookupExact("word").front().data;
    while (!working.empty() &&
           (working.back() == '\n' || working.back() == '\r')) {
        working.pop_back();
    }
    QCOMPARE(working, fixture.Directory());

    auto application = ComposeConfiguredApplication(configuration);
    goldendict::core::LookupQuery query;
    query.text = "word";
    query.dictionary_ids = {"html"};
    const auto response =
        application.facade->GetDictionaryService().Lookup(query);
    QCOMPARE(response.entries.size(), std::size_t{1});
    QVERIFY(response.entries.front().article.sanitized_html.has_value());
    QVERIFY(response.entries.front().article.sanitized_html->find("<script") ==
            std::string::npos);
}

void RuntimeCompositionTest::TranslatesExternalProgramRequestFailures() {
    ExternalProgramFixture fixture;
    goldendict::core::CoreConfiguration configuration;
    configuration.external_program_sources = {
        {"missing",
         "Missing",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         MissingExecutable("goldendict-runtime-helper"),
         {},
         ""},
        {"invalid",
         "Invalid",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         fixture.Path(),
         {"invalid"},
         ""},
        {"fail",
         "Fail",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         fixture.Path(),
         {"fail"},
         ""},
        {"slow",
         "Slow",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         fixture.Path(),
         {"slow"},
         ""}};
    auto composition = ComposeConfiguredRuntimeSources(configuration);

    VerifyRequestError(goldendict::core::RuntimeSourceErrorCode::kUnavailable,
                       [&]() { composition.sources[0]->LookupExact("word"); });
    VerifyRequestError(goldendict::core::RuntimeSourceErrorCode::kInvalidData,
                       [&]() { composition.sources[1]->LookupExact("word"); });
    VerifyRequestError(goldendict::core::RuntimeSourceErrorCode::kUnavailable,
                       [&]() { composition.sources[2]->LookupExact("word"); });
    goldendict::core::RuntimeRequestOptions deadline;
    deadline.deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    VerifyRequestError(
        goldendict::core::RuntimeSourceErrorCode::kDeadlineExceeded,
        [&]() { composition.sources[3]->LookupExact("word", deadline); });

    Cancelled cancelled;
    goldendict::core::RuntimeRequestOptions cancelled_options;
    cancelled_options.cancellation = &cancelled;
    VerifyAllMethodsReject(
        *composition.sources[0], cancelled_options,
        goldendict::core::RuntimeSourceErrorCode::kCancelled);
    goldendict::core::RuntimeRequestOptions zero;
    zero.result_limit = 0U;
    QVERIFY(composition.sources[0]->LookupExact("word", zero).empty());
    QVERIFY(composition.sources[0]->LookupPrefix("word", zero).empty());
    QVERIFY(composition.sources[0]->SuggestPrefix("word", zero).empty());
}

void RuntimeCompositionTest::ComposesCompleteApplicationFacadeAtomically() {
    ExternalProgramFixture fixture;
    goldendict::core::CoreConfiguration configuration;
    configuration.external_program_sources = {
        {"program",
         "Program",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         fixture.Path(),
         {"stdin"},
         ""}};
    auto active = ComposeConfiguredApplication(configuration);
    QCOMPARE(active.facade->GetDictionaryService().GetCatalog().size(),
             std::size_t{1});
    const auto session = active.facade->ExportArticleTabSession();

    auto invalid = configuration;
    invalid.external_program_sources.push_back(
        {"program",
         "Duplicate",
         true,
         goldendict::core::ExternalProgramOutputKind::kPlainText,
         fixture.Path(),
         {"stdin"},
         ""});
    QVERIFY_EXCEPTION_THROWN(ComposeConfiguredApplication(invalid),
                             std::runtime_error);
    QCOMPARE(active.facade->GetDictionaryService().GetCatalog().front().id,
             std::string("program"));
    QCOMPARE(active.facade->ExportArticleTabSession(), session);

    auto replacement = ComposeConfiguredApplication(configuration);
    QVERIFY(replacement.facade->RestoreArticleTabSession(session));
    QCOMPARE(replacement.facade->ExportArticleTabSession(), session);
}

}  // namespace goldendict::network

using goldendict::network::RuntimeCompositionTest;

QTEST_GUILESS_MAIN(RuntimeCompositionTest)

#include "runtime_composition_test.moc"
