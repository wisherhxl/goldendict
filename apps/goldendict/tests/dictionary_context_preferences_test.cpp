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
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QWebEngineUrlScheme>
#include <QtTest>
#include "articles_preferences_test_access.h"
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

class DictionaryContextPreferencesTest : public QObject {
    Q_OBJECT
   public:
    explicit DictionaryContextPreferencesTest(QString owned_root,
                                              bool restarted)
        : owned_root_(std::move(owned_root)), restarted_(restarted) {}

   private:
    const QString owned_root_;
    const bool restarted_;
   private slots:

    void scenarioThroughRealApplication() {
        const auto profile = owned_root_ + "/profile";
        qInfo() << "Preparing owned fixture" << profile;
        if (!restarted_)
            PrepareFixture(profile.toStdString());
        const auto configuration_path = profile + "/current-config/core.conf";
        const auto history_path = profile + "/history";
        auto configuration =
            core::LoadConfiguration(configuration_path.toStdString());
        if (restarted_) {
            QCOMPARE(configuration.preferences.maximum_dictionary_references,
                     0U);
            QVERIFY(configuration.article_tab_session.has_value());
        }
        qInfo() << "Restart pass" << (restarted_ ? 2 : 1) << "loaded profile"
                << configuration_path;
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
            std::vector<HistoryViewItem> items;
            for (const auto& entry : history)
                items.push_back(
                    {QString::fromStdString(entry.word), entry.group_id});
            window.SetHistoryItems(items);
        };
        const auto real_preferences_callback =
            app::InstallPreferencesApplication(
                window,
                {configuration, history, facade, owner, runtime, coordinator,
                 credentials, diagnostics, configuration_path, history_path,
                 network_root, refresh_history});
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
        auto* preferences_action_ = window.findChild<QAction*>("preferences");
        QVERIFY(preferences_action_);
        const auto clear_executor = qScopeGuard([&]() {
            ArticlesPreferencesTestAccess::SetDialogExecutor(window, {});
        });
        const auto initial_preferences =
            ViewMenuTestAccess::Preferences(window);
        const auto initial_session = facade->ExportArticleTabSession();
        const std::string initial_state = window.CaptureMainWindowState();
        bool passed = true;
        const auto inspect = [&passed, initial_preferences](
                                 PreferencesDialog& dialog, int value,
                                 bool accept) {
            auto* label = dialog.findChild<QLabel*>(
                QStringLiteral("maxDictsInContextMenuLabel"));
            auto* limit = dialog.findChild<QSpinBox*>(
                QStringLiteral("maxDictsInContextMenu"));
            auto* buttons = dialog.findChild<QDialogButtonBox*>(
                QStringLiteral("preferencesButtonBox"));
            passed =
                passed && label != nullptr && limit != nullptr &&
                buttons != nullptr &&
                label->text() ==
                    QStringLiteral("Context menu dictionaries limit:") &&
                label->toolTip() ==
                    QStringLiteral(
                        "Adjust this value to avoid huge context menus.") &&
                limit->minimum() == 0 && limit->maximum() == 9999 &&
                limit->singleStep() == 1 &&
                limit->value() ==
                    static_cast<int>(
                        initial_preferences.maximum_dictionary_references);
            if (limit != nullptr)
                limit->setValue(value);
            if (accept && buttons != nullptr)
                buttons->button(QDialogButtonBox::Ok)->click();
            else
                dialog.reject();
            return dialog.result();
        };

        ArticlesPreferencesTestAccess::SetDialogExecutor(
            window, [&inspect](PreferencesDialog& dialog) {
                return inspect(dialog, 0, false);
            });
        preferences_action_->trigger();
        passed = passed &&
                 ViewMenuTestAccess::Preferences(window) == initial_preferences;
        QVERIFY(passed);
        QVERIFY(facade == previous_facade);

        const auto original_callback = real_preferences_callback;
        window.SetPreferencesApplyCallback([](const auto&) {
            return QStringLiteral(
                "forced dictionary context preference failure");
        });
        ArticlesPreferencesTestAccess::SetDialogExecutor(
            window, [&inspect, &passed](PreferencesDialog& dialog) {
                const int result = inspect(dialog, 0, true);
                auto* error = dialog.findChild<QLabel*>(
                    QStringLiteral("preferencesValidationError"));
                passed = passed && result != QDialog::Accepted &&
                         error != nullptr && !error->isHidden();
                dialog.reject();
                return dialog.result();
            });
        preferences_action_->trigger();
        passed = passed &&
                 ViewMenuTestAccess::Preferences(window) == initial_preferences;
        QVERIFY(passed);
        QVERIFY(facade == previous_facade);

        window.SetPreferencesApplyCallback(original_callback);
        ArticlesPreferencesTestAccess::SetDialogExecutor(
            window, [&inspect](PreferencesDialog& dialog) {
                return inspect(dialog, 0, true);
            });
        preferences_action_->trigger();
        ArticlesPreferencesTestAccess::SetDialogExecutor(window, {});
        window.SetPreferencesApplyCallback(original_callback);
        passed = passed &&
                 ViewMenuTestAccess::Preferences(window)
                         .maximum_dictionary_references == 0U &&
                 facade->ExportArticleTabSession() == initial_session &&
                 window.CaptureMainWindowState() == initial_state;
        QVERIFY(passed);
        if (restarted_) {
            // The restored production callback must preserve its
            // unchanged-value no-op contract on the second independent process.
            QVERIFY(facade == previous_facade);
            QVERIFY(facade == owner.CurrentSnapshot());
            QVERIFY(configuration.preferences ==
                    ViewMenuTestAccess::Preferences(window));
        } else {
            QVERIFY(published());
        }
        const auto persisted =
            core::LoadConfiguration(configuration_path.toStdString());
        QCOMPARE(persisted.preferences.maximum_dictionary_references, 0U);
        QVERIFY(persisted.preferences == configuration.preferences);
        QVERIFY(persisted.article_tab_session.has_value());
    }
};

int main(int argc, char** argv) {
    QTemporaryDir profile(QDir::tempPath() + "/dc-XXXXXX");
    if (!profile.isValid())
        return 2;
    const auto restart_root =
        qEnvironmentVariable("GOLDENDICT_CONTEXT_RESTART_ROOT");
    const int restart_pass =
        qEnvironmentVariableIntValue("GOLDENDICT_CONTEXT_RESTART_PASS");
    if ((!restart_root.isEmpty() &&
         (!QDir::isAbsolutePath(restart_root) || !QDir(restart_root).exists() ||
          (restart_pass != 1 && restart_pass != 2))) ||
        (restart_root.isEmpty() && restart_pass != 0))
        return 2;
    const auto owned_root =
        restart_root.isEmpty() ? profile.path() : restart_root;
    for (const auto* name :
         {"HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "APPDATA",
          "LOCALAPPDATA", "GOLDENDICT_TEST_CONFIG_ROOT", "TEMP", "TMP"}) {
        const auto path = QDir(owned_root).filePath(QString::fromLatin1(name));
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
    DictionaryContextPreferencesTest test(owned_root, restart_pass == 2);
    return QTest::qExec(&test, argc, argv);
}

#include "dictionary_context_preferences_test.moc"
