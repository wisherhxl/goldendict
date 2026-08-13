// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../core/tests/support/stardict_fixture.h"
#include "../src/runtime_composition.h"
#include "goldendict/core/application.h"

namespace goldendict::network {
namespace {

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

class Cancelled final : public goldendict::core::RuntimeCancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
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

    auto sources = ComposeConfiguredRuntimeSources(configuration);
    QCOMPARE(sources.size(), std::size_t{4});
    auto service = goldendict::core::CreateDictionaryService(
        configuration, std::move(sources));
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
        configuration, ComposeConfiguredRuntimeSources(configuration));

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
    auto sources = ComposeConfiguredRuntimeSources(configuration);
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
    auto sources = ComposeConfiguredRuntimeSources(configuration);
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
    auto sources = ComposeConfiguredRuntimeSources(configuration);
    auto duplicate = ComposeConfiguredRuntimeSources(configuration);
    sources.push_back(std::move(duplicate.front()));

    QVERIFY_EXCEPTION_THROWN(goldendict::core::CreateDictionaryService(
                                 configuration, std::move(sources)),
                             std::runtime_error);

    auto unchanged = goldendict::core::CreateDictionaryService(configuration);
    QVERIFY(unchanged->GetCatalog().empty());
}

}  // namespace goldendict::network

using goldendict::network::RuntimeCompositionTest;

QTEST_GUILESS_MAIN(RuntimeCompositionTest)

#include "runtime_composition_test.moc"
