// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QLineEdit>
#include <QListWidget>
#include <QTemporaryDir>
#include <QTimer>
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

class DictionaryBarTest : public QObject {
    Q_OBJECT
   private slots:

    void dictionaryBarThroughRealWindow() {
        QTemporaryDir profile;
        QVERIFY(profile.isValid());
        core::CoreConfiguration configuration;
        configuration.dictionary_paths = {DICTIONARY_BAR_FIXTURE};
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
             std::string(DICTIONARY_BAR_EXTERNAL_FIXTURE),
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
        auto* bar = window.findChild<QToolBar*>("dictionaryBar");
        QVERIFY(bar);
        auto* host = dynamic_cast<DictionaryBarPresentationHost*>(
            bar->findChild<QWidget*>("widgetsDictionaryPresentationHost"));
        QVERIFY(host);
        auto* query = window.findChild<QLineEdit*>("translateLine");
        auto* results = window.findChild<QListWidget*>("dictsList");
        auto* suggestions = window.findChild<QListWidget*>("wordList");
        QVERIFY(query);
        QVERIFY(results);
        QVERIFY(suggestions);
        QVERIFY(facade->GetDictionaryService().GetCatalog().size() >= 2U);
        const auto catalog = facade->GetDictionaryService().GetCatalog();
        const auto toolbar_actions = host->ActiveActions();
        bool identities =
            toolbar_actions.size() == static_cast<qsizetype>(catalog.size());
        for (qsizetype index = 0; identities && index < toolbar_actions.size();
             ++index) {
            const auto& dictionary = catalog[static_cast<std::size_t>(index)];
            const QString label = QString::fromStdString(
                dictionary.name.empty() ? dictionary.id : dictionary.name);
            auto* widget = host->ActiveWidgetForAction(toolbar_actions[index]);
            identities = toolbar_actions[index]->isCheckable() &&
                         toolbar_actions[index]->isChecked() &&
                         toolbar_actions[index]->data().toString() ==
                             QString::fromStdString(dictionary.id) &&
                         toolbar_actions[index]->text() == label &&
                         toolbar_actions[index]->toolTip() == label &&
                         widget != nullptr && widget->accessibleName() == label;
        }
        const bool hierarchy =
            bar->objectName() == QStringLiteral("dictionaryBar") &&
            bar->toggleViewAction() != nullptr &&
            window.toolBarArea(bar) == Qt::TopToolBarArea && bar->isVisible() &&
            bar->isMovable() && bar->isFloatable() &&
            bar->allowedAreas() == Qt::AllToolBarAreas;

        window.SetDictionaryGroups({{7U,
                                     "Smoke Group",
                                     "",
                                     {catalog[1].id, catalog[0].id},
                                     {catalog[0].id}}});
        DictionaryScopeTestAccess::SelectGroup(window, 7U, true);
        const auto group_actions = host->ActiveActions();
        const bool group_baseline = group_actions.size() == 2 &&
                                    group_actions[0]->isChecked() &&
                                    !group_actions[1]->isChecked() &&
                                    group_actions[0]->data().toString() ==
                                        QString::fromStdString(catalog[1].id) &&
                                    group_actions[1]->data().toString() ==
                                        QString::fromStdString(catalog[0].id);
        DictionaryScopeTestAccess::SelectGroup(window, 0U, true);
        const auto all_actions = host->ActiveActions();
        if (all_actions.empty()) {
            QFAIL("Real all-dictionaries actions must exist");
        }
        all_actions.front()->trigger();
        DictionaryScopeTestAccess::SelectGroup(window, 7U, true);
        const bool group_isolation = host->ActiveActions()[0]->isChecked() &&
                                     !host->ActiveActions()[1]->isChecked();
        DictionaryScopeTestAccess::SelectGroup(window, 0U, true);
        const bool all_scope_retained = !host->ActiveActions()[0]->isChecked();
        for (auto* action : host->ActiveActions()) {
            if (action->isChecked())
                action->trigger();
        }
        query->setText(QStringLiteral("application"));
        QVERIFY(QMetaObject::invokeMethod(&window, "StartLookup",
                                          Qt::DirectConnection));

        // The original scenario polls real completion every 10 ms, waiting for
        // five empty-request observations before the hidden-toolbar stage.
        QEventLoop loop;
        QTimer poll;
        poll.setInterval(10);
        int settled = 0;
        bool hidden_stage = false;
        bool all_off = false;
        bool hidden_unfiltered = false;
        bool hidden_requests_completed = false;
        bool completion_invoked = true;
        QObject::connect(&poll, &QTimer::timeout, &window, [&]() {
            if (!QMetaObject::invokeMethod(&window, "FinishLookup",
                                           Qt::DirectConnection)) {
                completion_invoked = false;
                loop.quit();
                return;
            }
            if (DictionaryScopeTestAccess::RequestCount(window) != 0U)
                return;
            if (!hidden_stage) {
                if (++settled < 5)
                    return;
                all_off = results->count() == 0 && suggestions->count() == 0;
                bar->hide();
                hidden_stage = true;
                completion_invoked = QMetaObject::invokeMethod(
                    &window, "StartLookup", Qt::DirectConnection);
                if (!completion_invoked)
                    loop.quit();
                return;
            }
            hidden_requests_completed =
                DictionaryScopeTestAccess::RequestCount(window) == 0U;
            hidden_unfiltered = results->count() > 0;
            bar->show();
            loop.quit();
        });
        poll.start();
        loop.exec();
        poll.stop();
        QVERIFY(completion_invoked);
        QVERIFY(identities);
        QVERIFY(hierarchy);
        QVERIFY(group_baseline);
        QVERIFY(group_isolation);
        QVERIFY(all_scope_retained);
        QVERIFY(all_off);
        QVERIFY(hidden_stage);
        QVERIFY(hidden_unfiltered);
        QVERIFY(hidden_requests_completed);
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
    DictionaryBarTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "dictionary_bar_test.moc"
