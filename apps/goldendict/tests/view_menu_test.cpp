// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "goldendict/core/application.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QScopeGuard>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QWebEngineUrlScheme>
#include <QtTest>
#include <algorithm>
#include <cstdio>
#include "article_view.h"
#include "legacy_configuration_location.h"
#include "preferences_application.h"
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

class ViewMenuTest : public QObject {
    Q_OBJECT
   public:
    explicit ViewMenuTest(QString owned_root)
        : owned_root_(std::move(owned_root)) {}

   private:
    const QString owned_root_;
   private slots:

    void viewMenuThroughRealApplication() {
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
        auto* toggle_menubar_action_ =
            window.findChild<QAction*>("toggleMenuBar");
        QVERIFY(toggle_menubar_action_);
        auto* show_dictionary_bar_names_action_ =
            window.findChild<QAction*>("showDictBarNames");
        QVERIFY(show_dictionary_bar_names_action_);
        auto* use_small_toolbar_icons_action_ =
            window.findChild<QAction*>("useSmallIconsInToolbars");
        QVERIFY(use_small_toolbar_icons_action_);
        auto* always_on_top_action_ = window.findChild<QAction*>("alwaysOnTop");
        QVERIFY(always_on_top_action_);
        auto* zoom_in_action_ = window.findChild<QAction*>("zoomIn");
        QVERIFY(zoom_in_action_);
        auto* zoom_out_action_ = window.findChild<QAction*>("zoomOut");
        QVERIFY(zoom_out_action_);
        auto* zoom_reset_action_ = window.findChild<QAction*>("zoomBase");
        QVERIFY(zoom_reset_action_);
        auto* words_zoom_in_action_ = window.findChild<QAction*>("wordsZoomIn");
        QVERIFY(words_zoom_in_action_);
        auto* words_zoom_out_action_ =
            window.findChild<QAction*>("wordsZoomOut");
        QVERIFY(words_zoom_out_action_);
        auto* words_zoom_reset_action_ =
            window.findChild<QAction*>("wordsZoomBase");
        QVERIFY(words_zoom_reset_action_);
        auto* article_tabs_ = window.findChild<QTabWidget*>("articleTabs");
        QVERIFY(article_tabs_);
        auto* suggestions_list_ = window.findChild<QListWidget*>("wordList");
        QVERIFY(suggestions_list_);
        auto* query_ = window.findChild<QLineEdit*>("translateLine");
        QVERIFY(query_);
        auto* group_selector_ = window.findChild<QComboBox*>("groupSelector");
        QVERIFY(group_selector_);
        auto* dock_group_selector_ =
            window.findChild<QComboBox*>("dockGroupSelector");
        QVERIFY(dock_group_selector_);
        auto* results_list_ = window.findChild<QListWidget*>("dictsList");
        QVERIFY(results_list_);
        auto* history_list_ = window.findChild<QListWidget*>("historyList");
        QVERIFY(history_list_);
        auto* favorites_tree_ = window.findChild<QTreeWidget*>("favoritesTree");
        QVERIFY(favorites_tree_);
        auto* lookup_controls_ = window.findChild<QWidget*>("lookupControls");
        QVERIFY(lookup_controls_);
        auto* article_view_ =
            qobject_cast<ArticleView*>(article_tabs_->currentWidget());
        QVERIFY(article_view_);

        auto* app_menu_bar =
            window.findChild<QMenuBar*>(QStringLiteral("menubar"));
        auto* view_menu = window.findChild<QMenu*>(QStringLiteral("menuView"));
        auto* search_dock =
            window.findChild<QDockWidget*>(QString::fromLatin1("searchPane"));
        auto* results_dock =
            window.findChild<QDockWidget*>(QString::fromLatin1("dictsPane"));
        auto* favorites_dock = window.findChild<QDockWidget*>(
            QString::fromLatin1("favoritesPane"));
        auto* history_dock =
            window.findChild<QDockWidget*>(QString::fromLatin1("historyPane"));
        auto* nav_toolbar =
            window.findChild<QToolBar*>(QStringLiteral("navToolbar"));
        auto* dictionary_bar =
            window.findChild<QToolBar*>(QStringLiteral("dictionaryBar"));
        auto* zoom_menu = window.findChild<QMenu*>(QStringLiteral("menuZoom"));
        auto* menu_button =
            window.findChild<QToolButton*>(QStringLiteral("menuButton"));
        if (app_menu_bar == nullptr || view_menu == nullptr ||
            search_dock == nullptr || results_dock == nullptr ||
            favorites_dock == nullptr || history_dock == nullptr ||
            nav_toolbar == nullptr || dictionary_bar == nullptr ||
            zoom_menu == nullptr || menu_button == nullptr) {
            std::fprintf(
                stderr,
                "view menu missing: bar=%d menu=%d search=%d results=%d "
                "favorites=%d history=%d nav=%d dictionary=%d zoom=%d "
                "button=%d\n",
                app_menu_bar != nullptr, view_menu != nullptr,
                search_dock != nullptr, results_dock != nullptr,
                favorites_dock != nullptr, history_dock != nullptr,
                nav_toolbar != nullptr, dictionary_bar != nullptr,
                zoom_menu != nullptr, menu_button != nullptr);
            QFAIL("Required View menu object is missing");
        }

        const QList<QAction*> expected_actions = {
            zoom_menu->menuAction(),
            toggle_menubar_action_,
            nullptr,
            search_dock->toggleViewAction(),
            results_dock->toggleViewAction(),
            favorites_dock->toggleViewAction(),
            history_dock->toggleViewAction(),
            nullptr,
            dictionary_bar->toggleViewAction(),
            nav_toolbar->toggleViewAction(),
            nullptr,
            show_dictionary_bar_names_action_,
            use_small_toolbar_icons_action_,
            nullptr,
            always_on_top_action_,
        };
        const auto actions = view_menu->actions();
        bool passed =
            app_menu_bar == window.menuBar() &&
            window.findChildren<QMenuBar*>(QStringLiteral("menubar")).size() ==
                1 &&
            window.findChildren<QMenu*>(QStringLiteral("menuView")).size() ==
                1 &&
            app_menu_bar->actions().size() == 7 &&
            app_menu_bar->actions()[1]->menu() == view_menu &&
            view_menu->title() == QStringLiteral("&View") &&
            actions.size() == expected_actions.size();
        for (qsizetype index = 0; passed && index < actions.size(); ++index) {
            if (expected_actions[index] == nullptr) {
                passed = actions[index]->isSeparator();
            } else if (index == 0) {
                passed = actions[index] == expected_actions[index] &&
                         actions[index]->menu() == zoom_menu &&
                         zoom_menu->title() == QStringLiteral("&Zoom");
            } else {
                QString accessible_text = actions[index]->text();
                accessible_text.remove('&');
                passed =
                    actions[index] == expected_actions[index] &&
                    !actions[index]->isSeparator() &&
                    actions[index]->isCheckable() &&
                    actions[index]->isEnabled() &&
                    actions[index]->menuRole() != QAction::PreferencesRole &&
                    actions[index]->menuRole() != QAction::AboutRole &&
                    actions[index]->menuRole() != QAction::QuitRole &&
                    !accessible_text.trimmed().isEmpty();
            }
        }
        passed =
            passed &&
            toggle_menubar_action_->text() == QStringLiteral("&Menubar") &&
            toggle_menubar_action_->shortcut() ==
                QKeySequence(Qt::CTRL | Qt::Key_M) &&
            show_dictionary_bar_names_action_->text() ==
                QStringLiteral("Show Names in Dictionary &Bar") &&
            use_small_toolbar_icons_action_->text() ==
                QStringLiteral("Show Small Icons in &Toolbars") &&
            always_on_top_action_->text() == QStringLiteral("&Always on Top") &&
            always_on_top_action_->shortcut() ==
                QKeySequence(Qt::CTRL | Qt::Key_O) &&
            search_dock->toggleViewAction()->shortcut() ==
                QKeySequence(Qt::CTRL | Qt::Key_S) &&
            results_dock->toggleViewAction()->shortcut() ==
                QKeySequence(Qt::CTRL | Qt::Key_R) &&
            favorites_dock->toggleViewAction()->shortcut() ==
                QKeySequence(Qt::CTRL | Qt::Key_I) &&
            history_dock->toggleViewAction()->shortcut() ==
                QKeySequence(Qt::CTRL | Qt::Key_H) &&
            dictionary_bar->toggleViewAction()->shortcut().isEmpty() &&
            nav_toolbar->toggleViewAction()->shortcut().isEmpty();
        const auto all_actions = window.findChildren<QAction*>();
        for (const auto& shortcut : {QKeySequence(Qt::CTRL | Qt::Key_M),
                                     QKeySequence(Qt::CTRL | Qt::Key_O),
                                     QKeySequence(Qt::CTRL | Qt::Key_S),
                                     QKeySequence(Qt::CTRL | Qt::Key_R),
                                     QKeySequence(Qt::CTRL | Qt::Key_I),
                                     QKeySequence(Qt::CTRL | Qt::Key_H)}) {
            passed =
                passed &&
                std::count_if(all_actions.cbegin(), all_actions.cend(),
                              [&shortcut](const QAction* action) {
                                  return action->shortcuts().contains(shortcut);
                              }) == 1;
        }
        qInfo() << "Structure stage" << passed;
        const bool structure_passed = passed;

        const auto zoom_actions = zoom_menu->actions();
        passed = passed && zoom_actions.size() == 7 &&
                 zoom_actions[0] == zoom_in_action_ &&
                 zoom_actions[1] == zoom_out_action_ &&
                 zoom_actions[2] == zoom_reset_action_ &&
                 zoom_actions[3]->isSeparator() &&
                 zoom_actions[4] == words_zoom_in_action_ &&
                 zoom_actions[5] == words_zoom_out_action_ &&
                 zoom_actions[6] == words_zoom_reset_action_;
        const auto original_preferences_callback = real_preferences_callback;
        int display_preference_updates = 0;
        window.SetPreferencesApplyCallback(
            [&display_preference_updates](const auto&) {
                ++display_preference_updates;
                return QString{};
            });
        auto* secondary_view = new ArticleView(article_tabs_);
        const int secondary_index = article_tabs_->addTab(
            secondary_view, QStringLiteral("Zoom fixture"));
        article_view_->setZoomFactor(1.0);
        secondary_view->setZoomFactor(1.0);
        zoom_in_action_->trigger();
        passed = passed &&
                 ViewMenuTestAccess::Preferences(window).zoom_factor == 1.1 &&
                 article_view_->zoomFactor() == 1.1 &&
                 secondary_view->zoomFactor() == 1.1 &&
                 display_preference_updates == 1 &&
                 zoom_reset_action_->isEnabled();
        zoom_reset_action_->trigger();
        passed = passed &&
                 ViewMenuTestAccess::Preferences(window).zoom_factor == 1.0 &&
                 article_view_->zoomFactor() == 1.0 &&
                 secondary_view->zoomFactor() == 1.0 &&
                 display_preference_updates == 2;
        article_tabs_->removeTab(secondary_index);
        delete secondary_view;
        const qreal initial_word_size = suggestions_list_->font().pointSizeF();
        const qreal initial_query_size = query_->font().pointSizeF();
        const qreal initial_group_size = group_selector_->font().pointSizeF();
        const qreal initial_dock_group_size =
            dock_group_selector_->font().pointSizeF();
        const QFont initial_results_font = results_list_->font();
        const QFont initial_history_font = history_list_->font();
        const QFont initial_favorites_font = favorites_tree_->font();
        words_zoom_in_action_->trigger();
        passed = passed &&
                 suggestions_list_->font().pointSizeF() > initial_word_size &&
                 query_->font().pointSizeF() > initial_query_size &&
                 group_selector_->font().pointSizeF() > initial_group_size &&
                 dock_group_selector_->font().pointSizeF() >
                     initial_dock_group_size &&
                 results_list_->font() == initial_results_font &&
                 history_list_->font() == initial_history_font &&
                 favorites_tree_->font() == initial_favorites_font &&
                 display_preference_updates == 3 &&
                 words_zoom_reset_action_->isEnabled();
        words_zoom_reset_action_->trigger();
        passed =
            passed &&
            ViewMenuTestAccess::Preferences(window).words_zoom_level == 0 &&
            suggestions_list_->font().pointSizeF() == initial_word_size &&
            query_->font().pointSizeF() == initial_query_size &&
            group_selector_->font().pointSizeF() == initial_group_size &&
            dock_group_selector_->font().pointSizeF() ==
                initial_dock_group_size &&
            display_preference_updates == 4;
        window.SetPreferencesApplyCallback(original_preferences_callback);
        qInfo() << "Zoom stage" << passed;
        const bool zoom_passed = passed;

        toggle_menubar_action_->setChecked(false);
        QVERIFY(published());
        QApplication::processEvents();
        passed =
            passed && !app_menu_bar->isVisible() && menu_button->isVisible();
        toggle_menubar_action_->setChecked(true);
        QVERIFY(published());
        QApplication::processEvents();
        passed =
            passed && app_menu_bar->isVisible() && !menu_button->isVisible();
        const bool menubar_passed = passed;

        show_dictionary_bar_names_action_->setChecked(true);
        QVERIFY(published());
        passed = passed && dictionary_bar->toolButtonStyle() ==
                               Qt::ToolButtonTextBesideIcon;
        show_dictionary_bar_names_action_->setChecked(false);
        QVERIFY(published());
        passed = passed &&
                 dictionary_bar->toolButtonStyle() == Qt::ToolButtonIconOnly;
        const bool dictionary_style_passed = passed;

        use_small_toolbar_icons_action_->setChecked(true);
        QVERIFY(published());
        passed =
            passed && nav_toolbar->iconSize().width() ==
                          window.style()->pixelMetric(QStyle::PM_SmallIconSize);
        use_small_toolbar_icons_action_->setChecked(false);
        QVERIFY(published());
        passed = passed &&
                 nav_toolbar->iconSize().width() ==
                     window.style()->pixelMetric(QStyle::PM_ToolBarIconSize);
        const bool icon_size_passed = passed;

        // A successful View change must survive presentation reconstruction;
        // rejected persistence must leave both the action and the widget
        // unchanged.
        const auto view_preferences = ViewMenuTestAccess::Preferences(window);
        auto saved_view_preferences = ViewMenuTestAccess::Preferences(window);
        int view_preference_updates = 0;
        window.SetPreferencesApplyCallback([&](const auto& updated) {
            saved_view_preferences = updated;
            ++view_preference_updates;
            return QString{};
        });
        toggle_menubar_action_->setChecked(false);
        show_dictionary_bar_names_action_->setChecked(true);
        use_small_toolbar_icons_action_->setChecked(true);
        window.SetPreferences(view_preferences);
        window.SetPreferences(saved_view_preferences);
        passed =
            passed && view_preference_updates == 3 &&
            !toggle_menubar_action_->isChecked() &&
            !app_menu_bar->isVisible() && menu_button->isVisible() &&
            show_dictionary_bar_names_action_->isChecked() &&
            dictionary_bar->toolButtonStyle() == Qt::ToolButtonTextBesideIcon &&
            use_small_toolbar_icons_action_->isChecked() &&
            nav_toolbar->iconSize().width() ==
                window.style()->pixelMetric(QStyle::PM_SmallIconSize);
        window.SetPreferencesApplyCallback([](const auto&) {
            return QStringLiteral("injected View preference save failure");
        });
        toggle_menubar_action_->trigger();
        show_dictionary_bar_names_action_->trigger();
        use_small_toolbar_icons_action_->trigger();
        passed =
            passed &&
            ViewMenuTestAccess::Preferences(window) == saved_view_preferences &&
            !toggle_menubar_action_->isChecked() &&
            !app_menu_bar->isVisible() && menu_button->isVisible() &&
            show_dictionary_bar_names_action_->isChecked() &&
            dictionary_bar->toolButtonStyle() == Qt::ToolButtonTextBesideIcon &&
            use_small_toolbar_icons_action_->isChecked() &&
            nav_toolbar->iconSize().width() ==
                window.style()->pixelMetric(QStyle::PM_SmallIconSize);
        window.SetPreferencesApplyCallback(original_preferences_callback);
        window.SetPreferences(view_preferences);
        if (!passed)
            qCritical() << "View preference persistence/rollback mismatch"
                        << view_preference_updates;

        always_on_top_action_->setChecked(true);
        passed =
            passed && window.windowFlags().testFlag(Qt::WindowStaysOnTopHint);
        always_on_top_action_->setChecked(false);
        passed =
            passed && !window.windowFlags().testFlag(Qt::WindowStaysOnTopHint);
        const bool always_on_top_passed = passed;

        auto toolbar_preferences = ViewMenuTestAccess::Preferences(window);
        toolbar_preferences.search_in_dock = false;
        window.SetPreferences(toolbar_preferences);
        for (auto* widget : {static_cast<QWidget*>(results_dock),
                             static_cast<QWidget*>(favorites_dock),
                             static_cast<QWidget*>(history_dock),
                             static_cast<QWidget*>(dictionary_bar),
                             static_cast<QWidget*>(nav_toolbar)}) {
            widget->show();
        }
        QApplication::processEvents();

        const auto placement_callback = real_preferences_callback;
        int placement_updates = 0;
        window.SetPreferencesApplyCallback([&placement_updates](const auto&) {
            ++placement_updates;
            return QString{};
        });
        int search_toggles = 0;
        const auto search_connection = connect(
            search_dock->toggleViewAction(), &QAction::toggled, &window,
            [&search_toggles](bool) { ++search_toggles; },
            Qt::DirectConnection);
        passed = passed && !search_dock->isVisible() &&
                 !search_dock->toggleViewAction()->isChecked() &&
                 query_->parentWidget() == lookup_controls_;
        search_dock->toggleViewAction()->trigger();
        passed = passed && search_dock->isVisible() &&
                 search_dock->toggleViewAction()->isChecked() &&
                 ViewMenuTestAccess::Preferences(window).search_in_dock &&
                 query_->parentWidget() == search_dock->widget() &&
                 search_toggles == 1 && placement_updates == 1;
        search_dock->toggleViewAction()->trigger();
        passed = passed && !search_dock->isVisible() &&
                 !search_dock->toggleViewAction()->isChecked() &&
                 !ViewMenuTestAccess::Preferences(window).search_in_dock &&
                 query_->parentWidget() == lookup_controls_ &&
                 search_toggles == 2 && placement_updates == 2;
        qInfo() << "Search placement stage" << passed << search_toggles
                << placement_updates;
        disconnect(search_connection);
        window.SetPreferencesApplyCallback(placement_callback);
        QApplication::processEvents();
        const std::string initial_state = window.CaptureMainWindowState();
        const QList<QPair<QWidget*, QAction*>> exposed_widgets = {
            {results_dock, results_dock->toggleViewAction()},
            {favorites_dock, favorites_dock->toggleViewAction()},
            {history_dock, history_dock->toggleViewAction()},
            {dictionary_bar, dictionary_bar->toggleViewAction()},
            {nav_toolbar, nav_toolbar->toggleViewAction()},
        };
        for (const auto& [widget, action] : exposed_widgets) {
            int toggles = 0;
            const auto connection = connect(
                action, &QAction::toggled, &window,
                [&toggles](bool) { ++toggles; }, Qt::DirectConnection);
            passed = passed && widget->isVisible() && action->isChecked();
            action->trigger();
            passed = passed && !widget->isVisible() && !action->isChecked() &&
                     toggles == 1;
            action->trigger();
            passed = passed && widget->isVisible() && action->isChecked() &&
                     toggles == 2;
            widget->hide();
            passed = passed && !action->isChecked() && toggles == 3;
            widget->show();
            passed = passed && action->isChecked() && toggles == 4;
            qInfo() << "Visibility stage" << widget->objectName() << passed
                    << toggles;
            disconnect(connection);
        }
        QApplication::processEvents();
        auto* tabs_host = dynamic_cast<WidgetsPresentationHost*>(
            window.findChild<QWidget*>("widgetsArticleTabsPresentationHost"));
        QVERIFY(tabs_host);
        article_tabs_ = qobject_cast<QTabWidget*>(tabs_host->ActivePage());
        QVERIFY(article_tabs_);
        qInfo() << "Layout stage" << passed
                << (window.CaptureMainWindowState() == initial_state)
                << article_tabs_->isVisible() << article_tabs_->size()
                << (window.saveState(7).toStdString() ==
                    window.CaptureMainWindowState());
        passed = passed && window.CaptureMainWindowState() == initial_state &&
                 window.centralWidget() != nullptr &&
                 article_tabs_ != nullptr && article_tabs_->isVisible() &&
                 article_tabs_->size().width() > 0 &&
                 article_tabs_->size().height() > 0 &&
                 window.saveState(7).toStdString() ==
                     window.CaptureMainWindowState();
        if (!passed) {
            qWarning() << "view menu smoke check failed" << actions.size()
                       << app_menu_bar->actions().size();
            std::fprintf(
                stderr,
                "view menu result: actions=%lld menubar=%d button=%d "
                "wordZoom=%d alwaysOnTop=%d stages=%d%d%d%d%d%d\n",
                static_cast<long long>(actions.size()),
                app_menu_bar->isVisible(), menu_button->isVisible(),
                ViewMenuTestAccess::Preferences(window).words_zoom_level,
                window.windowFlags().testFlag(Qt::WindowStaysOnTopHint),
                structure_passed, zoom_passed, menubar_passed,
                dictionary_style_passed, icon_size_passed,
                always_on_top_passed);
        }
        QVERIFY(passed);
    }
};

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--prepare-fixture") {
        try {
            PrepareFixture(argv[2]);
            return 0;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
    }
    QTemporaryDir profile(QDir::tempPath() + "/vm-XXXXXX");
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
    ViewMenuTest test(profile.path());
    return QTest::qExec(&test, argc, argv);
}

#include "view_menu_test.moc"
