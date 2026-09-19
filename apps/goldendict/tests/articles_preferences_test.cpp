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

class ArticlesPreferencesTest : public QObject {
    Q_OBJECT
   public:
    explicit ArticlesPreferencesTest(QString owned_root)
        : owned_root_(std::move(owned_root)) {}

   private:
    const QString owned_root_;
   private slots:

    void articlesPreferencesThroughRealApplication() {
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
        const auto active_tabs = [&]() {
            auto* host = dynamic_cast<WidgetsPresentationHost*>(
                window.findChild<QWidget*>(
                    "widgetsArticleTabsPresentationHost"));
            return host ? qobject_cast<QTabWidget*>(host->ActivePage())
                        : nullptr;
        };
        const auto clear_executor = qScopeGuard([&]() {
            ArticlesPreferencesTestAccess::SetDialogExecutor(window, {});
        });
        const auto initial_preferences =
            ViewMenuTestAccess::Preferences(window);
        const auto initial_session = facade->ExportArticleTabSession();
        const std::string initial_state = window.CaptureMainWindowState();
        bool passed = true;

        ArticlesPreferencesTestAccess::SetDialogExecutor(
            window, [&passed, initial_preferences](PreferencesDialog& dialog) {
                auto* group = dialog.findChild<QGroupBox*>(
                    QStringLiteral("preferencesArticlesGroup"));
                auto* collapse = dialog.findChild<QCheckBox*>(
                    QStringLiteral("collapseBigArticles"));
                auto* limit = dialog.findChild<QSpinBox*>(
                    QStringLiteral("articleSizeLimit"));
                auto* label = dialog.findChild<QLabel*>(
                    QStringLiteral("articleSizeLimitLabel"));
                auto* ignore = dialog.findChild<QCheckBox*>(
                    QStringLiteral("ignoreDiacritics"));
                passed =
                    passed && group != nullptr && collapse != nullptr &&
                    limit != nullptr && label != nullptr && ignore != nullptr &&
                    group->title() == QStringLiteral("Articles") &&
                    collapse->text() ==
                        QStringLiteral("Collapse articles more than") &&
                    collapse->toolTip() ==
                        QStringLiteral(
                            "Select this option to automatic collapse big "
                            "articles") &&
                    collapse->isChecked() ==
                        initial_preferences.collapse_large_articles &&
                    limit->minimum() == 1 && limit->maximum() == 100000 &&
                    limit->singleStep() == 50 &&
                    limit->value() ==
                        static_cast<int>(
                            initial_preferences.article_size_limit) &&
                    limit->isEnabled() ==
                        initial_preferences.collapse_large_articles &&
                    label->text() == QStringLiteral("symbols") &&
                    ignore->text() ==
                        QStringLiteral("Ignore diacritics while searching") &&
                    ignore->toolTip() == QStringLiteral(
                                             "Turn this option on to ignore "
                                             "diacritics while searching "
                                             "articles") &&
                    ignore->isChecked() ==
                        initial_preferences.ignore_diacritics &&
                    dialog.findChild<QWidget*>(
                        QStringLiteral("displayStyle")) == nullptr;
                collapse->setChecked(true);
                ignore->setChecked(true);
                limit->setValue(3450);
                passed = passed && limit->isEnabled();
                dialog.reject();
                return dialog.result();
            });
        preferences_action_->trigger();
        passed =
            passed &&
            ViewMenuTestAccess::Preferences(window) == initial_preferences &&
            facade->ExportArticleTabSession() == initial_session;
        QVERIFY(passed);
        QVERIFY(facade == previous_facade);

        const auto original_callback = real_preferences_callback;
        window.SetPreferencesApplyCallback([](const auto&) {
            return QStringLiteral("forced article preferences failure");
        });
        ArticlesPreferencesTestAccess::SetDialogExecutor(
            window, [&passed](PreferencesDialog& dialog) {
                auto* collapse = dialog.findChild<QCheckBox*>(
                    QStringLiteral("collapseBigArticles"));
                auto* limit = dialog.findChild<QSpinBox*>(
                    QStringLiteral("articleSizeLimit"));
                auto* buttons = dialog.findChild<QDialogButtonBox*>(
                    QStringLiteral("preferencesButtonBox"));
                auto* ignore = dialog.findChild<QCheckBox*>(
                    QStringLiteral("ignoreDiacritics"));
                passed = passed && collapse != nullptr && limit != nullptr &&
                         ignore != nullptr && buttons != nullptr;
                collapse->setChecked(true);
                ignore->setChecked(true);
                limit->setValue(3450);
                buttons->button(QDialogButtonBox::Ok)->click();
                auto* error = dialog.findChild<QLabel*>(
                    QStringLiteral("preferencesValidationError"));
                passed = passed && dialog.result() != QDialog::Accepted &&
                         error != nullptr && !error->isHidden();
                dialog.reject();
                return dialog.result();
            });
        preferences_action_->trigger();
        passed =
            passed &&
            ViewMenuTestAccess::Preferences(window) == initial_preferences &&
            facade->ExportArticleTabSession() == initial_session;
        QVERIFY(passed);
        QVERIFY(facade == previous_facade);

        window.SetPreferencesApplyCallback(original_callback);
        ArticlesPreferencesTestAccess::SetDialogExecutor(
            window, [&passed](PreferencesDialog& dialog) {
                auto* collapse = dialog.findChild<QCheckBox*>(
                    QStringLiteral("collapseBigArticles"));
                auto* limit = dialog.findChild<QSpinBox*>(
                    QStringLiteral("articleSizeLimit"));
                auto* buttons = dialog.findChild<QDialogButtonBox*>(
                    QStringLiteral("preferencesButtonBox"));
                auto* ignore = dialog.findChild<QCheckBox*>(
                    QStringLiteral("ignoreDiacritics"));
                passed = passed && collapse != nullptr && limit != nullptr &&
                         ignore != nullptr && buttons != nullptr;
                collapse->setChecked(true);
                ignore->setChecked(true);
                limit->setValue(3450);
                buttons->button(QDialogButtonBox::Ok)->click();
                passed = passed && dialog.result() == QDialog::Accepted;
                return dialog.result();
            });
        preferences_action_->trigger();
        ArticlesPreferencesTestAccess::SetDialogExecutor(window, {});
        window.SetPreferencesApplyCallback(original_callback);
        passed =
            passed &&
            ViewMenuTestAccess::Preferences(window).collapse_large_articles &&
            ViewMenuTestAccess::Preferences(window).ignore_diacritics &&
            ViewMenuTestAccess::Preferences(window).article_size_limit ==
                3450U &&
            facade->ExportArticleTabSession() == initial_session &&
            window.CaptureMainWindowState() == initial_state &&
            window.centralWidget() != nullptr && active_tabs() != nullptr &&
            active_tabs()->isVisible();
        QVERIFY(passed);
        QVERIFY(published());
        const auto persisted =
            core::LoadConfiguration(configuration_path.toStdString());
        QVERIFY(persisted.preferences.collapse_large_articles);
        QVERIFY(persisted.preferences.ignore_diacritics);
        QCOMPARE(persisted.preferences.article_size_limit, 3450U);
    }
};

int main(int argc, char** argv) {
    QTemporaryDir profile(QDir::tempPath() + "/ap-XXXXXX");
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
    ArticlesPreferencesTest test(profile.path());
    return QTest::qExec(&test, argc, argv);
}

#include "articles_preferences_test.moc"
