// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "goldendict/core/application.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebEngineUrlScheme>
#include <QtTest>
#include "articles_preferences_test_access.h"
#include "history_application.h"
#include "legacy_configuration_location.h"
#include "preferences_application.h"
#include "preferences_dialog.h"
#include "view_menu_test_access.h"
#include "webengine_storage_paths.h"
#include "widgets_presentation_host.h"

namespace {
void PrepareFixture(const std::filesystem::path& root) {
    if (root.empty() || !root.is_absolute() || std::filesystem::exists(root))
        throw std::runtime_error("Fixture requires a new absolute owned root");
    std::filesystem::create_directories(root / "current-config");
    std::filesystem::create_directories(root / "indexes");
    goldendict::core::CoreConfiguration configuration;
    configuration.index_directory = (root / "indexes").generic_string();
    const auto path = (root / "current-config/core.conf").generic_string();
    goldendict::core::SaveConfiguration(path, configuration);
    const auto loaded = goldendict::core::LoadConfiguration(path);
    if (!std::filesystem::is_regular_file(path) ||
        loaded.index_directory != configuration.index_directory ||
        !(loaded.preferences == configuration.preferences))
        throw std::runtime_error("Fixture serialization round-trip failed");
    std::cout << "Owned fixture saved and loaded: " << path << '\n';
}
}  // namespace

namespace core = goldendict::core;
namespace app = goldendict::app;

class HistoryPreferencesTest : public QObject {
    Q_OBJECT
   public:
    explicit HistoryPreferencesTest(QString owned_root)
        : owned_root_(std::move(owned_root)) {}

   private:
    const QString owned_root_;
   private slots:

    void scenarioThroughRealApplication() {
        const auto profile = owned_root_ + "/profile";
        qInfo() << "Preparing owned fixture" << profile;
        PrepareFixture(profile.toStdString());
        qInfo() << "Fixture round-trip complete";
        const auto configuration_path = profile + "/current-config/core.conf";
        const auto history_path = profile + "/history";
        auto configuration =
            core::LoadConfiguration(configuration_path.toStdString());
        std::vector<core::HistoryEntry> history;
        const auto network_root = (profile + "/network-cache").toStdString();
        auto runtime = goldendict::network::NetworkRuntime::Create(
            goldendict::network::NetworkRuntime::Prepare({}, network_root));
        const goldendict::network::ForvoCredentialMap credentials;
        core::application::DesktopFacadeActivationOwner owner;
        auto initial = app::PrepareProductionFacade(configuration, credentials,
                                                    runtime, owner);
        QVERIFY(owner.Activate(initial.candidate));
        auto facade = initial.facade;
        auto diagnostics = std::move(initial.diagnostics);
        qInfo() << "Initial production facade activated";
        MainWindow window(profile);
        window.SetPreferences(configuration.preferences);
        window.SetNetworkCacheDirectory(
            QString::fromStdString(runtime->cache_directory()));
        window.RestoreMainWindowGeometry(configuration.main_window_geometry);
        window.RestoreMainWindowState(configuration.main_window_state);
        window.SetFullTextDialogGeometry(
            configuration.full_text_dialog_geometry);
        window.SetInspectorGeometry(configuration.inspector_geometry);
        window.SetDictionaryGroups(configuration.dictionary_groups);
        window.SetSourceDirectories(configuration.dictionary_paths,
                                    configuration.sound_directories);
        window.SetFacade(facade.get());
        app::ConfigurationReloadTransactionCoordinator coordinator(
            runtime, owner, window);
        const auto refresh_history = [&]() {
            app::RefreshHistoryPresentation(window, history);
        };
        app::InstallHistoryRecording(window, configuration, history,
                                     history_path);
        app::InstallHistoryImport(window, configuration, history, history_path);
        app::InstallPreferencesApplication(
            window, {configuration, history, facade, owner, runtime,
                     coordinator, credentials, diagnostics, configuration_path,
                     history_path, network_root, refresh_history});
        qInfo() << "Shared Preferences callback installed";
        window.show();
        QApplication::processEvents();
        auto previous_facade = facade;
        const auto shutdown = qScopeGuard([&]() {
            coordinator.Shutdown();
            window.SetFacade(nullptr);
            previous_facade.reset();
            facade.reset();
            initial.facade.reset();
            owner.Shutdown();
            runtime->Shutdown();
        });
        const auto published = [&]() {
            const auto saved =
                core::LoadConfiguration(configuration_path.toStdString());
            const bool valid =
                facade != previous_facade &&
                facade == owner.CurrentSnapshot() &&
                saved.preferences == configuration.preferences &&
                saved.preferences == ViewMenuTestAccess::Preferences(window) &&
                saved.article_tab_session.has_value();
            previous_facade = facade;
            return valid;
        };
        QTimer::singleShot(10000, &window, []() { QCoreApplication::exit(2); });
        auto* preferences_action_ = window.findChild<QAction*>("preferences");
        auto* history_list_ = window.findChild<QListWidget*>("historyList");
        auto* history_count_label_ =
            window.findChild<QLabel*>("historyCountLabel");
        QVERIFY(preferences_action_);
        QVERIFY(history_list_);
        QVERIFY(history_count_label_);
        const auto clear_executor = qScopeGuard([&]() {
            ArticlesPreferencesTestAccess::SetDialogExecutor(window, {});
        });
        history = {{3U, "Newest"}, {2U, "Middle"}, {1U, "Oldest"}};
        core::SaveHistory(history_path.toStdString(), history);
        refresh_history();
        const QString import_path = profile + "/history-preferences.txt";
        QFile import_file(import_path);
        const QByteArray contents("Imported one\nImported two\nIgnored\n");
        const bool prepared = import_file.open(QIODevice::WriteOnly) &&
                              import_file.write(contents) == contents.size();
        import_file.close();
        QVERIFY(prepared);
        const auto initial_session = facade->ExportArticleTabSession();
        bool passed = history_list_->count() == 3 &&
                      history_count_label_->text() ==
                          QStringLiteral("%1/%2")
                              .arg(history.size())
                              .arg(ViewMenuTestAccess::Preferences(window)
                                       .maximum_history_entries);
        const auto apply_history_preferences = [&window, &passed,
                                                preferences_action_](
                                                   bool store, int maximum) {
            ArticlesPreferencesTestAccess::SetDialogExecutor(
                window, [&passed, store, maximum](PreferencesDialog& dialog) {
                    auto* store_history = dialog.findChild<QCheckBox*>(
                        QStringLiteral("storeHistory"));
                    auto* maximum_history = dialog.findChild<QSpinBox*>(
                        QStringLiteral("historyMaxSizeField"));
                    auto* buttons = dialog.findChild<QDialogButtonBox*>(
                        QStringLiteral("preferencesButtonBox"));
                    passed = passed && store_history != nullptr &&
                             maximum_history != nullptr && buttons != nullptr;
                    if (store_history != nullptr)
                        store_history->setChecked(store);
                    if (maximum_history != nullptr)
                        maximum_history->setValue(maximum);
                    if (buttons != nullptr)
                        buttons->button(QDialogButtonBox::Ok)->click();
                    passed = passed && dialog.result() == QDialog::Accepted;
                    return dialog.result();
                });
            preferences_action_->trigger();
            ArticlesPreferencesTestAccess::SetDialogExecutor(window, {});
        };

        apply_history_preferences(false, 2);
        passed =
            passed && !ViewMenuTestAccess::Preferences(window).store_history &&
            ViewMenuTestAccess::Preferences(window).maximum_history_entries ==
                2U &&
            history_list_->count() == 2 &&
            history_count_label_->text() == QStringLiteral("2/2") &&
            history_count_label_->toolTip() ==
                QStringLiteral("History size: 2 entries out of maximum 2") &&
            history_list_->item(0)->text() == QStringLiteral("Newest") &&
            history_list_->item(1)->text() == QStringLiteral("Middle");
        QVERIFY(passed);
        QVERIFY(published());
        QCOMPARE(core::LoadHistory(history_path.toStdString()), history);
        emit window.LookupSubmitted(QStringLiteral("Not recorded"), 9U);
        passed = passed && history_list_->count() == 2 &&
                 history_list_->item(0)->text() == QStringLiteral("Newest");

        QVERIFY(passed);
        QCOMPARE(core::LoadHistory(history_path.toStdString()), history);
        emit window.ImportHistoryRequested(import_path, 7U);
        passed =
            passed && history_list_->count() == 2 &&
            history_count_label_->text() == QStringLiteral("2/2") &&
            history_list_->item(0)->text() == QStringLiteral("Imported one") &&
            history_list_->item(1)->text() == QStringLiteral("Imported two");

        QVERIFY(passed);
        QCOMPARE(core::LoadHistory(history_path.toStdString()), history);
        QCOMPARE(history.front().group_id, 7U);
        apply_history_preferences(true, 1);
        QVERIFY(published());
        emit window.LookupSubmitted(QStringLiteral("Recorded"), 11U);
        passed =
            passed && ViewMenuTestAccess::Preferences(window).store_history &&
            ViewMenuTestAccess::Preferences(window).maximum_history_entries ==
                1U &&
            history_list_->count() == 1 &&
            history_count_label_->text() == QStringLiteral("1/1") &&
            history_count_label_->toolTip() ==
                QStringLiteral("History size: 1 entries out of maximum 1") &&
            history_list_->item(0)->text() == QStringLiteral("Recorded") &&
            facade->ExportArticleTabSession() == initial_session;
        QVERIFY(passed);
        const auto persisted_history =
            core::LoadHistory(history_path.toStdString(), 1U);
        const auto persisted_configuration =
            core::LoadConfiguration(configuration_path.toStdString());
        QCOMPARE(persisted_history.size(), std::size_t{1});
        QCOMPARE(persisted_history.front().word, std::string("Recorded"));
        QVERIFY(persisted_configuration.preferences.store_history);
        QCOMPARE(persisted_configuration.preferences.maximum_history_entries,
                 1U);
        QCOMPARE(core::LoadHistory(history_path.toStdString()), history);
        QCOMPARE(history.front().group_id, 11U);
    }
};

int main(int argc, char** argv) {
    QTemporaryDir profile(QDir::tempPath() + "/hp-XXXXXX");
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
    QTemporaryDir webengine_storage;
    if (!webengine_storage.isValid())
        return 2;
    QApplication application(argc, argv);
    goldendict::app::InitializeWebEngineStorage(
        {}, webengine_storage.filePath("webengine"));
    HistoryPreferencesTest test(profile.path());
    QTimer::singleShot(0, &application, [&]() {
        application.exit(QTest::qExec(&test, argc, argv));
    });
    return application.exec();
}

#include "history_preferences_test.moc"
