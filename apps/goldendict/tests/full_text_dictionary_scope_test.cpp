// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QToolBar>
#include <QWebEngineUrlScheme>
#include <QtTest>

#include <algorithm>

#include "../../../modules/core/src/application/desktop_facade_activation_owner.h"
#include "full_text_query_composer.h"
#include "goldendict/core/application.h"
#include "goldendict/network/network_runtime.h"
#include "goldendict/network/runtime_composition.h"
#include "main_window.h"
#include "widgets_presentation_host.h"

namespace core = goldendict::core;

#include "dictionary_scope_test_access.h"

class FullTextDictionaryScopeTest : public QObject {
    Q_OBJECT
   private slots:

    void projectionThroughRealWindow() {
        QTemporaryDir profile;
        QVERIFY(profile.isValid());
        core::CoreConfiguration configuration;
        configuration.dictionary_paths = {FULL_TEXT_SCOPE_FIXTURE};
        configuration.index_directory =
            profile.filePath("indexes").toStdString();
        configuration.mediawiki_sources = {
            {"smoke.wiki", "Smoke Wiki", false, "https://wiki.example.test/w"}};
        configuration.website_sources = {
            {"smoke.website", "Smoke Website", false,
             "https://website.example.test/?q=%GDWORD%"}};
        configuration.forvo_sources = {{"smoke.forvo",
                                        "Smoke Forvo",
                                        false,
                                        "https://apifree.forvo.com",
                                        {"en", "ru"}}};
        configuration.dict_server_sources = {{"smoke.dict", "Smoke DICT", false,
                                              "dict.example.test", 2628U, "*",
                                              "prefix"}};
        configuration.external_program_sources = {
            {"smoke.external",
             "Smoke External",
             true,
             core::ExternalProgramOutputKind::kPlainText,
             QCoreApplication::applicationFilePath().toStdString(),
             {"--smoke", "%GDWORD%"},
             ""}};
        auto runtime = goldendict::network::NetworkRuntime::Create(
            goldendict::network::NetworkRuntime::Prepare(
                {}, profile.filePath("network-cache").toStdString()));
        auto composition = goldendict::network::ComposeConfiguredRuntimeSources(
            configuration, {}, runtime);
        core::application::DesktopFacadeActivationOwner owner;
        auto candidate = owner.PrepareCandidate(configuration,
                                                std::move(composition.sources));
        QVERIFY(candidate);
        auto facade = owner.PreparedFacadeSnapshot(candidate);
        QVERIFY(facade);
        QVERIFY(owner.Activate(candidate));
        // MainWindow dies before the facade owner and Network runtime.
        MainWindow window(profile.path());
        window.SetPreferences(configuration.preferences);
        window.SetDictionaryGroups(configuration.dictionary_groups);
        window.SetFacade(facade.get());
        window.show();
        QApplication::processEvents();
        auto* bar =
            window.findChild<QToolBar*>(QStringLiteral("dictionaryBar"));
        QVERIFY(bar);
        auto* host = dynamic_cast<DictionaryBarPresentationHost*>(
            bar->findChild<QWidget*>(
                QStringLiteral("widgetsDictionaryPresentationHost")));
        QVERIFY(host);
        std::vector<std::string> supported;
        for (const auto& identity :
             facade->GetDictionaryService().GetCatalog()) {
            if (identity.supports_full_text_search)
                supported.push_back(identity.id);
        }
        QVERIFY2(!supported.empty(),
                 "Real Dictd fixture must offer full-text search");
        goldendict::app::FullTextQueryComposer composer(
            configuration.preferences, &window);
        const auto request_count =
            DictionaryScopeTestAccess::RequestCount(window);
        DictionaryScopeTestAccess::SelectGroup(window, 0U, true);
        bar->show();
        QApplication::processEvents();
        const auto all =
            DictionaryScopeTestAccess::Compose(window, composer);
        QVERIFY(all.dictionary_filter_active);
        QVERIFY(all.dictionary_ids == supported);

        QAction* supported_action = nullptr;
        for (auto* action : host->ActiveActions()) {
            if (action->data().toString().toStdString() == supported.front()) {
                supported_action = action;
                break;
            }
        }
        QVERIFY(supported_action);
        supported_action->trigger();
        const auto unchecked =
            DictionaryScopeTestAccess::Compose(window, composer);
        QVERIFY(unchecked.dictionary_filter_active);
        QVERIFY(std::find(unchecked.dictionary_ids.begin(),
                          unchecked.dictionary_ids.end(),
                          supported.front()) == unchecked.dictionary_ids.end());
        bar->hide();
        QApplication::processEvents();
        const auto hidden =
            DictionaryScopeTestAccess::Compose(window, composer);
        QVERIFY(hidden.dictionary_ids == supported);
        window.SetDictionaryGroups(
            {{7U,
              "Full Text Projection Smoke",
              "",
              {supported.front(), "unresolved.dictionary"},
              {supported.front()}}});
        DictionaryScopeTestAccess::SelectGroup(window, 7U);
        const auto muted =
            DictionaryScopeTestAccess::Compose(window, composer);
        QApplication::processEvents();
        QVERIFY(muted.dictionary_filter_active);
        QVERIFY(muted.dictionary_ids.empty());
        QCOMPARE(DictionaryScopeTestAccess::RequestCount(window),
                 request_count);
        QVERIFY(!composer.isVisible());
    }
};

int main(int argc, char** argv) {
    QTemporaryDir profile;
    if (!profile.isValid())
        return 2;
    for (const auto* name :
         {"HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "APPDATA",
          "LOCALAPPDATA", "GOLDENDICT_TEST_CONFIG_ROOT", "TEMP", "TMP"}) {
        const auto path = profile.filePath(QString::fromLatin1(name));
        if (!QDir().mkpath(path))
            return 2;
        qputenv(name, path.toUtf8());
    }
    QWebEngineUrlScheme scheme(QByteArrayLiteral("goldendict"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(0);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                    QWebEngineUrlScheme::LocalScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
    QApplication application(argc, argv);
    FullTextDictionaryScopeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "full_text_dictionary_scope_test.moc"
