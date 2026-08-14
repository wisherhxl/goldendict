// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.h"

#include <algorithm>
#include <exception>

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QComboBox>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPixmap>
#include <QPointer>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWebEngineFindTextResult>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWidget>

#include "article_page.h"
#include "article_scheme_handler.h"
#include "article_view.h"
#include "dictionary_browser.h"
#include "favorites_tree_widget.h"
#include "goldendict/core/desktop_facade.h"
#include "group_editor.h"
#include "source_directories_dialog.h"
#include "suggestion_worker.h"

namespace {

constexpr int kMainWindowStateVersion = 7;
constexpr int kPreviousMainWindowStateVersion = 6;
constexpr int kOlderMainWindowStateVersion = 5;
constexpr int kOldestMainWindowStateVersion = 4;
constexpr int kEarlierMainWindowStateVersion = 3;
constexpr int kEarliestMainWindowStateVersion = 2;
constexpr qsizetype kMaximumMainWindowStateBytes = 64 * 1024;
constexpr auto kHistoryPaneName = "historyPane";
constexpr auto kFavoritesPaneName = "favoritesPane";
constexpr auto kResultsPaneName = "dictsPane";
constexpr auto kSearchPaneName = "searchPane";
constexpr auto kPreviousHistoryDockName = "historyDock";
constexpr auto kPreviousFavoritesDockName = "favoritesDock";

QString EscapeHtml(QString text) {
    return text.replace('&', QStringLiteral("&amp;"))
        .replace('<', QStringLiteral("&lt;"))
        .replace('>', QStringLiteral("&gt;"));
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("GoldenDict"));
    resize(960, 640);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    auto* controls = new QHBoxLayout();
    auto* directory_button =
        new QPushButton(QStringLiteral("Dictionary Sources..."), central);
    group_selector_ = new QComboBox(central);
    group_selector_->setObjectName(QStringLiteral("groupSelector"));
    group_selector_->setToolTip(
        QStringLiteral("Choose a dictionary group (Alt+G)"));
    group_selector_->setMaxVisibleItems(30);
    edit_groups_button_ =
        new QPushButton(QStringLiteral("Edit Groups..."), central);
    edit_groups_button_->setObjectName(QStringLiteral("editGroupsButton"));
    query_ = new QLineEdit(central);
    query_->setObjectName(QStringLiteral("translateLine"));
    query_->setPlaceholderText(QStringLiteral("Enter a word"));
    query_->setMinimumWidth(200);
    lookup_button_ = new QToolButton(central);
    lookup_button_->setObjectName(QStringLiteral("lookupButton"));
    lookup_button_->setText(QStringLiteral("Lookup"));
    lookup_button_->setPopupMode(QToolButton::MenuButtonPopup);
    auto* lookup_menu = new QMenu(lookup_button_);
    auto* lookup_new_tab =
        lookup_menu->addAction(QStringLiteral("Lookup in New Tab"));
    auto* lookup_background_tab =
        lookup_menu->addAction(QStringLiteral("Lookup in Background Tab"));
    lookup_button_->setMenu(lookup_menu);
    controls->addWidget(directory_button);
    controls->addWidget(edit_groups_button_);
    controls->addStretch();
    layout->addLayout(controls);

    status_ = new QLabel(QStringLiteral("No dictionary configured"), central);
    layout->addWidget(status_);
    article_tabs_ = new QTabWidget(central);
    article_tabs_->setObjectName(QStringLiteral("articleTabs"));
    article_tabs_->setTabsClosable(true);
    article_tabs_->setDocumentMode(true);
    article_tabs_->setMovable(false);
    article_tabs_->tabBar()->installEventFilter(this);
    article_tabs_->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    auto* add_tab_button = new QToolButton(article_tabs_);
    add_tab_button->setObjectName(QStringLiteral("addArticleTabButton"));
    add_tab_button->setText(QStringLiteral("+"));
    add_tab_button->setToolTip(QStringLiteral("New Tab"));
    auto* add_tab_menu = new QMenu(add_tab_button);
    new_tab_action_ = new QAction(QStringLiteral("&New Tab"), this);
    new_tab_action_->setObjectName(QStringLiteral("newTab"));
    new_tab_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    new_tab_action_->setShortcutContext(Qt::WidgetShortcut);
    new_tab_action_->setMenuRole(QAction::NoRole);
    add_tab_menu->addAction(new_tab_action_);
    auto* add_background_tab =
        add_tab_menu->addAction(QStringLiteral("New Background Tab"));
    add_tab_button->setMenu(add_tab_menu);
    add_tab_button->setPopupMode(QToolButton::MenuButtonPopup);
    article_tabs_->setCornerWidget(add_tab_button, Qt::TopLeftCorner);
    layout->addWidget(article_tabs_, 1);

    auto* history_dock = new QDockWidget(QStringLiteral("&History Pane"), this);
    history_dock->setObjectName(QString::fromLatin1(kHistoryPaneName));
    auto* history_widget = new QWidget(history_dock);
    auto* history_layout = new QVBoxLayout(history_widget);
    history_layout->setContentsMargins(4, 4, 4, 4);
    history_filter_ = new QLineEdit(history_widget);
    history_filter_->setObjectName(QStringLiteral("historyFilter"));
    history_filter_->setPlaceholderText(QStringLiteral("Filter history"));
    history_filter_->setClearButtonEnabled(true);
    history_list_ = new QListWidget(history_widget);
    history_list_->setObjectName(QStringLiteral("historyList"));
    history_list_->setAlternatingRowColors(true);
    clear_history_button_ =
        new QPushButton(QStringLiteral("Clear History"), history_widget);
    clear_history_button_->setObjectName(QStringLiteral("clearHistoryButton"));
    export_history_button_ =
        new QPushButton(QStringLiteral("Export History..."), history_widget);
    export_history_button_->setObjectName(
        QStringLiteral("exportHistoryButton"));
    import_history_button_ =
        new QPushButton(QStringLiteral("Import History..."), history_widget);
    import_history_button_->setObjectName(
        QStringLiteral("importHistoryButton"));
    auto* history_buttons = new QHBoxLayout();
    history_buttons->addWidget(import_history_button_);
    history_buttons->addWidget(export_history_button_);
    history_buttons->addWidget(clear_history_button_);
    history_layout->addWidget(history_filter_);
    history_layout->addWidget(history_list_, 1);
    history_layout->addLayout(history_buttons);
    history_dock->setWidget(history_widget);

    auto* favorites_dock =
        new QDockWidget(QStringLiteral("Favor&ites Pane"), this);
    favorites_dock->setObjectName(QString::fromLatin1(kFavoritesPaneName));
    favorites_tree_ = new FavoritesTreeWidget(favorites_dock);
    favorites_tree_->setObjectName(QStringLiteral("favoritesTree"));
    favorites_tree_->setHeaderHidden(true);
    favorites_tree_->SetMoveRequest([this](const QList<int>& source_path,
                                           const QList<int>& destination_path,
                                           int destination_index) {
        emit MoveFavoriteAcrossFoldersRequested(source_path, destination_path,
                                                destination_index,
                                                ExpandedFavoriteFolderPaths());
    });
    favorites_dock->setWidget(favorites_tree_);

    auto* results_dock =
        new QDockWidget(QStringLiteral("&Results Navigation Pane"), this);
    results_dock->setObjectName(QString::fromLatin1(kResultsPaneName));
    results_list_ = new QListWidget(results_dock);
    results_list_->setObjectName(QStringLiteral("dictsList"));
    results_list_->setAlternatingRowColors(true);
    results_dock->setWidget(results_list_);

    auto* search_dock = new QDockWidget(QStringLiteral("&Search Pane"), this);
    search_dock->setObjectName(QString::fromLatin1(kSearchPaneName));
    auto* search_widget = new QWidget(search_dock);
    auto* search_layout = new QVBoxLayout(search_widget);
    search_layout->setContentsMargins(2, 1, 2, 1);
    suggestions_list_ = new QListWidget(search_widget);
    suggestions_list_->setObjectName(QStringLiteral("wordList"));
    suggestions_list_->setAlternatingRowColors(true);
    search_layout->addWidget(suggestions_list_, 1);
    search_dock->setWidget(search_widget);
    ApplyDefaultPaneLayout();

    auto* nav_toolbar = addToolBar(QStringLiteral("&Navigation"));
    nav_toolbar->setObjectName(QStringLiteral("navToolbar"));
    nav_toolbar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
    back_action_ = nav_toolbar->addAction(QStringLiteral("Back"));
    back_action_->setShortcut(QKeySequence::Back);
    nav_toolbar->widgetForAction(back_action_)
        ->setObjectName(QStringLiteral("backButton"));
    forward_action_ = nav_toolbar->addAction(QStringLiteral("Forward"));
    forward_action_->setShortcut(QKeySequence::Forward);
    nav_toolbar->widgetForAction(forward_action_)
        ->setObjectName(QStringLiteral("forwardButton"));
    auto* lookup_controls = new QWidget(nav_toolbar);
    auto* lookup_layout = new QHBoxLayout(lookup_controls);
    lookup_layout->setContentsMargins(0, 0, 0, 0);
    lookup_layout->setSpacing(0);
    group_selector_->setParent(lookup_controls);
    query_->setParent(lookup_controls);
    lookup_button_->setParent(lookup_controls);
    group_selector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    lookup_layout->addWidget(group_selector_);
    lookup_layout->addWidget(query_, 1);
    lookup_layout->addWidget(lookup_button_);
    lookup_controls->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
    nav_toolbar->addWidget(lookup_controls);
    setTabOrder(group_selector_, query_);
    setTabOrder(query_, lookup_button_);
    setTabOrder(lookup_button_, suggestions_list_);
    setTabOrder(suggestions_list_, results_list_);
    query_->installEventFilter(this);
    suggestions_list_->installEventFilter(this);

    auto* focus_query_action = new QAction(this);
    focus_query_action->setShortcuts({QKeySequence(QStringLiteral("Alt+D")),
                                      QKeySequence(QStringLiteral("Ctrl+L"))});
    focus_query_action->setShortcutContext(Qt::WindowShortcut);
    addAction(focus_query_action);
    connect(focus_query_action, &QAction::triggered, query_, [this]() {
        query_->setFocus();
        query_->selectAll();
    });

    auto* article_toolbar = addToolBar(QStringLiteral("Article"));
    article_toolbar->setObjectName(QStringLiteral("articleToolbar"));
    insertToolBar(article_toolbar, nav_toolbar);
    auto* reload_action = article_toolbar->addAction(QStringLiteral("Reload"));
    article_toolbar->addSeparator();
    add_favorite_action_ =
        article_toolbar->addAction(QStringLiteral("Add to Favorites"));
    add_favorite_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    add_favorite_folder_action_ =
        article_toolbar->addAction(QStringLiteral("New Favorite Folder"));
    rename_favorite_action_ =
        article_toolbar->addAction(QStringLiteral("Rename Favorite"));
    rename_favorite_action_->setEnabled(false);
    move_favorite_up_action_ =
        article_toolbar->addAction(QStringLiteral("Move Favorite Up"));
    move_favorite_up_action_->setEnabled(false);
    move_favorite_down_action_ =
        article_toolbar->addAction(QStringLiteral("Move Favorite Down"));
    move_favorite_down_action_->setEnabled(false);
    move_favorite_to_root_action_ =
        article_toolbar->addAction(QStringLiteral("Move Favorite to Root"));
    move_favorite_to_root_action_->setEnabled(false);
    import_favorites_action_ =
        article_toolbar->addAction(QStringLiteral("Import Favorites"));
    export_favorites_action_ =
        article_toolbar->addAction(QStringLiteral("Export Favorites"));
    remove_favorite_action_ =
        article_toolbar->addAction(QStringLiteral("Remove Favorite"));
    remove_favorite_action_->setShortcut(QKeySequence::Delete);
    remove_favorite_action_->setEnabled(false);
    dictionary_browser_action_ =
        article_toolbar->addAction(QStringLiteral("Dictionaries"));
    article_toolbar->addSeparator();
    article_search_ = new QLineEdit(article_toolbar);
    article_search_->setPlaceholderText(QStringLiteral("Find in article"));
    article_search_->setClearButtonEnabled(true);
    article_search_->setMaximumWidth(240);
    article_toolbar->addWidget(article_search_);
    auto* previous_action =
        article_toolbar->addAction(QStringLiteral("Previous"));
    auto* next_action = article_toolbar->addAction(QStringLiteral("Next"));
    article_search_status_ = new QLabel(article_toolbar);
    article_search_status_->setMinimumWidth(72);
    article_toolbar->addWidget(article_search_status_);
    article_toolbar->addSeparator();
    auto* zoom_out_action =
        article_toolbar->addAction(QStringLiteral("Zoom Out"));
    zoom_out_action->setShortcut(QKeySequence::ZoomOut);
    auto* zoom_reset_action =
        article_toolbar->addAction(QStringLiteral("Reset Zoom"));
    zoom_reset_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    auto* zoom_in_action =
        article_toolbar->addAction(QStringLiteral("Zoom In"));
    zoom_in_action->setShortcut(QKeySequence::ZoomIn);
    article_toolbar->addSeparator();
    auto* copy_action = article_toolbar->addAction(QStringLiteral("Copy"));
    copy_action->setShortcut(QKeySequence::Copy);
    save_article_action_ =
        article_toolbar->addAction(QStringLiteral("&Save Article"));
    save_article_action_->setObjectName(QStringLiteral("saveArticle"));
    save_article_action_->setShortcut(QKeySequence(Qt::Key_F2));
    save_article_action_->setMenuRole(QAction::NoRole);
    print_action_ = article_toolbar->addAction(QStringLiteral("&Print"));
    print_action_->setObjectName(QStringLiteral("print"));
    print_action_->setShortcut(QKeySequence::Print);
    print_action_->setMenuRole(QAction::NoRole);
    print_preview_action_ =
        article_toolbar->addAction(QStringLiteral("Print Pre&view"));
    print_preview_action_->setObjectName(QStringLiteral("printPreview"));
    print_preview_action_->setMenuRole(QAction::NoRole);
    auto* print_pdf_action =
        article_toolbar->addAction(QStringLiteral("Print PDF"));
    dictionary_bar_ = addToolBar(QStringLiteral("&Dictionary Bar"));
    dictionary_bar_->setObjectName(QStringLiteral("dictionaryBar"));
    dictionary_bar_->setAllowedAreas(Qt::AllToolBarAreas);
    dictionary_bar_->setMinimumWidth(0);
    dictionary_bar_->setSizePolicy(QSizePolicy::Ignored,
                                   QSizePolicy::Preferred);

    auto* app_menu_bar = menuBar();
    app_menu_bar->setObjectName(QStringLiteral("menubar"));
    auto* file_menu = app_menu_bar->addMenu(QStringLiteral("&File"));
    file_menu->setObjectName(QStringLiteral("menuFile"));
    file_menu->addAction(new_tab_action_);
    file_menu->addSeparator();
    file_menu->addAction(print_preview_action_);
    file_menu->addAction(print_action_);
    file_menu->addSeparator();
    file_menu->addAction(save_article_action_);
    file_menu->addSeparator();
    quit_action_ = new QAction(QStringLiteral("&Quit"), this);
    quit_action_->setObjectName(QStringLiteral("quit"));
    quit_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    quit_action_->setMenuRole(QAction::QuitRole);
    file_menu->addAction(quit_action_);
    auto* view_menu = app_menu_bar->addMenu(QStringLiteral("&View"));
    view_menu->setObjectName(QStringLiteral("menuView"));
    search_dock->toggleViewAction()->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_S));
    results_dock->toggleViewAction()->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_R));
    favorites_dock->toggleViewAction()->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_I));
    history_dock->toggleViewAction()->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_H));
    history_dock->toggleViewAction()->setObjectName(
        QStringLiteral("showHideHistory"));
    history_dock->toggleViewAction()->setMenuRole(QAction::NoRole);
    view_menu->addAction(search_dock->toggleViewAction());
    view_menu->addAction(results_dock->toggleViewAction());
    view_menu->addAction(favorites_dock->toggleViewAction());
    view_menu->addAction(history_dock->toggleViewAction());
    view_menu->addSeparator();
    view_menu->addAction(dictionary_bar_->toggleViewAction());
    view_menu->addAction(nav_toolbar->toggleViewAction());
    auto* history_menu = app_menu_bar->addMenu(QStringLiteral("H&istory"));
    history_menu->setObjectName(QStringLiteral("menuHistory"));
    history_menu->addAction(history_dock->toggleViewAction());
    export_history_action_ = new QAction(QStringLiteral("&Export"), this);
    export_history_action_->setObjectName(QStringLiteral("exportHistory"));
    export_history_action_->setMenuRole(QAction::NoRole);
    history_menu->addAction(export_history_action_);
    import_history_action_ = new QAction(QStringLiteral("&Import"), this);
    import_history_action_->setObjectName(QStringLiteral("importHistory"));
    import_history_action_->setMenuRole(QAction::NoRole);
    history_menu->addAction(import_history_action_);
    history_menu->addSeparator();
    clear_history_action_ = new QAction(QStringLiteral("&Clear"), this);
    clear_history_action_->setObjectName(QStringLiteral("clearHistory"));
    clear_history_action_->setMenuRole(QAction::NoRole);
    history_menu->addAction(clear_history_action_);
    UpdateHistoryActions();
    setCentralWidget(central);

    scheme_handler_ = new ArticleSchemeHandler(this);
    QWebEngineProfile::defaultProfile()->installUrlSchemeHandler(
        QByteArrayLiteral("goldendict"), scheme_handler_);
    completion_timer_ = new QTimer(this);
    completion_timer_->setInterval(15);

    connect(directory_button, &QPushButton::clicked, this,
            &MainWindow::EditSourceDirectories);
    connect(group_selector_, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                if (index >= 0) {
                    selected_group_id_ =
                        group_selector_->itemData(index).toUInt();
                }
            });
    connect(edit_groups_button_, &QPushButton::clicked, this,
            &MainWindow::EditDictionaryGroups);
    connect(lookup_button_, &QToolButton::clicked, this,
            &MainWindow::StartLookup);
    connect(lookup_new_tab, &QAction::triggered, this, [this]() {
        StartLookupInTab(goldendict::core::TabOpenPolicy::kNewTab,
                         goldendict::core::TabActivationPolicy::kActivate);
    });
    connect(lookup_background_tab, &QAction::triggered, this, [this]() {
        StartLookupInTab(goldendict::core::TabOpenPolicy::kNewTab,
                         goldendict::core::TabActivationPolicy::kKeepActive);
    });
    connect(add_tab_button, &QToolButton::clicked, new_tab_action_,
            &QAction::trigger);
    connect(new_tab_action_, &QAction::triggered, this, [this]() {
        CreateEmptyArticleTab(!preferences_.open_new_tabs_in_background);
    });
    connect(add_background_tab, &QAction::triggered, this,
            [this]() { CreateEmptyArticleTab(false); });
    connect(article_tabs_, &QTabWidget::currentChanged, this,
            &MainWindow::ActivateArticleTab);
    connect(article_tabs_, &QTabWidget::tabCloseRequested, this,
            &MainWindow::CloseArticleTab);
    connect(article_tabs_->tabBar(), &QWidget::customContextMenuRequested, this,
            &MainWindow::ShowTabContextMenu);
    connect(query_, &QLineEdit::returnPressed, this, &MainWindow::StartLookup);
    connect(query_, &QLineEdit::textChanged, this,
            &MainWindow::StartSuggestionLookup);
    connect(group_selector_, &QComboBox::currentIndexChanged, this, [this]() {
        selected_group_id_ = group_selector_->currentData().value<quint32>();
        RefreshDictionaryBar();
        StartSuggestionLookup();
    });
    connect(dictionary_bar_, &QToolBar::visibilityChanged, this,
            [this](bool) { ApplyDictionaryParticipation(); });
    connect(suggestions_list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem*) { ActivateSuggestion(); });
    connect(results_list_, &QListWidget::itemSelectionChanged, this,
            &MainWindow::NavigateToSelectedResult);
    connect(results_list_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem*) { NavigateToSelectedResult(); });
    connect(completion_timer_, &QTimer::timeout, this,
            &MainWindow::FinishLookup);
    connect(history_list_, &QListWidget::itemActivated, this,
            [this](const QListWidgetItem* item) {
                if (item == nullptr) {
                    return;
                }
                SelectGroup(item->data(Qt::UserRole).value<quint32>());
                query_->setText(item->text());
                StartLookup();
            });
    connect(history_filter_, &QLineEdit::textChanged, this,
            &MainWindow::RefreshHistoryList);
    connect(clear_history_button_, &QPushButton::clicked, clear_history_action_,
            &QAction::trigger);
    connect(export_history_button_, &QPushButton::clicked,
            export_history_action_, &QAction::trigger);
    connect(import_history_button_, &QPushButton::clicked,
            import_history_action_, &QAction::trigger);
    connect(clear_history_action_, &QAction::triggered, this,
            &MainWindow::ClearHistory);
    connect(export_history_action_, &QAction::triggered, this,
            &MainWindow::ExportHistory);
    connect(import_history_action_, &QAction::triggered, this,
            &MainWindow::ImportHistory);
    connect(favorites_tree_, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem* item, int) {
                if (item == nullptr || item->data(0, Qt::UserRole).toBool()) {
                    return;
                }
                query_->setText(item->text(0));
                StartLookup();
            });
    connect(add_favorite_action_, &QAction::triggered, this, [this]() {
        const QString word = query_->text().trimmed();
        if (!word.isEmpty()) {
            emit AddFavoriteRequested(word, SelectedFavoriteFolderPath());
        }
    });
    connect(add_favorite_folder_action_, &QAction::triggered, this,
            &MainWindow::CreateFavoriteFolder);
    connect(favorites_tree_, &QTreeWidget::itemSelectionChanged, this,
            [this]() {
                const bool selected = favorites_tree_->currentItem() != nullptr;
                remove_favorite_action_->setEnabled(selected);
                rename_favorite_action_->setEnabled(selected);
                move_favorite_up_action_->setEnabled(selected);
                move_favorite_down_action_->setEnabled(selected);
                move_favorite_to_root_action_->setEnabled(
                    selected &&
                    favorites_tree_->currentItem()->parent() != nullptr);
            });
    connect(rename_favorite_action_, &QAction::triggered, this,
            &MainWindow::RenameFavorite);
    connect(move_favorite_up_action_, &QAction::triggered, this, [this]() {
        const auto* item = favorites_tree_->currentItem();
        if (item != nullptr) {
            emit MoveFavoriteRequested(
                item->data(0, Qt::UserRole + 1).value<QList<int>>(), -1);
        }
    });
    connect(move_favorite_down_action_, &QAction::triggered, this, [this]() {
        const auto* item = favorites_tree_->currentItem();
        if (item != nullptr) {
            emit MoveFavoriteRequested(
                item->data(0, Qt::UserRole + 1).value<QList<int>>(), 1);
        }
    });
    connect(move_favorite_to_root_action_, &QAction::triggered, this, [this]() {
        const auto* item = favorites_tree_->currentItem();
        if (item != nullptr && item->parent() != nullptr) {
            emit MoveFavoriteToRootRequested(
                item->data(0, Qt::UserRole + 1).value<QList<int>>());
        }
    });
    connect(import_favorites_action_, &QAction::triggered, this,
            &MainWindow::ImportFavorites);
    connect(export_favorites_action_, &QAction::triggered, this,
            &MainWindow::ExportFavorites);
    connect(remove_favorite_action_, &QAction::triggered, this, [this]() {
        const auto* item = favorites_tree_->currentItem();
        if (item != nullptr) {
            emit RemoveFavoriteRequested(
                item->data(0, Qt::UserRole + 1).value<QList<int>>());
        }
    });
    connect(dictionary_browser_action_, &QAction::triggered, this,
            &MainWindow::ShowDictionaryBrowser);
    connect(back_action_, &QAction::triggered, this,
            [this]() { NavigateArticleTab(false); });
    connect(forward_action_, &QAction::triggered, this,
            [this]() { NavigateArticleTab(true); });
    connect(reload_action, &QAction::triggered, this, [this]() {
        if (article_view_ != nullptr)
            article_view_->reload();
    });
    connect(article_search_, &QLineEdit::returnPressed, this,
            [this]() { FindInArticle(false); });
    connect(article_search_, &QLineEdit::textChanged, this,
            [this](const QString& text) {
                if (text.isEmpty()) {
                    article_view_->findText(QString());
                    article_search_status_->clear();
                }
            });
    connect(previous_action, &QAction::triggered, this,
            [this]() { FindInArticle(true); });
    connect(next_action, &QAction::triggered, this,
            [this]() { FindInArticle(false); });
    connect(zoom_out_action, &QAction::triggered, this,
            [this]() { ZoomArticle(-0.1); });
    connect(zoom_reset_action, &QAction::triggered, this,
            [this]() { article_view_->setZoomFactor(1.0); });
    connect(zoom_in_action, &QAction::triggered, this,
            [this]() { ZoomArticle(0.1); });
    connect(copy_action, &QAction::triggered, this, [this]() {
        if (article_page_ != nullptr)
            article_page_->triggerAction(QWebEnginePage::Copy);
    });
    connect(save_article_action_, &QAction::triggered, this,
            &MainWindow::SaveArticle);
    connect(print_action_, &QAction::triggered, this,
            &MainWindow::PrintArticle);
    connect(print_preview_action_, &QAction::triggered, this,
            &MainWindow::PreviewArticle);
    connect(quit_action_, &QAction::triggered, this, [this]() {
        if (quit_dispatcher_)
            quit_dispatcher_();
        else
            QApplication::quit();
    });
    connect(print_pdf_action, &QAction::triggered, this,
            &MainWindow::SaveArticleAsPdf);
    auto* find_action = new QAction(this);
    find_action->setShortcut(QKeySequence::Find);
    find_action->setShortcutContext(Qt::WindowShortcut);
    addAction(find_action);
    connect(find_action, &QAction::triggered, article_search_,
            qOverload<>(&QLineEdit::setFocus));
    suggestion_worker_ = std::make_unique<SuggestionWorker>(
        [this](goldendict::core::ArticleTabId tab_id, std::uint64_t generation,
               goldendict::core::SuggestionResponse response) {
            QMetaObject::invokeMethod(
                this, [this, tab_id, generation,
                       response = std::move(response)]() mutable {
                    FinishSuggestionLookup(tab_id, generation,
                                           std::move(response));
                });
        });
    UpdateNavigationActions();
    UpdateFileActions();
}

void MainWindow::RunViewMenuSmokeCheck(std::function<void(bool)> completion) {
    auto* app_menu_bar = findChild<QMenuBar*>(QStringLiteral("menubar"));
    auto* view_menu = findChild<QMenu*>(QStringLiteral("menuView"));
    auto* search_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    auto* results_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    auto* favorites_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    auto* history_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* nav_toolbar = findChild<QToolBar*>(QStringLiteral("navToolbar"));
    auto* dictionary_bar =
        findChild<QToolBar*>(QStringLiteral("dictionaryBar"));
    if (app_menu_bar == nullptr || view_menu == nullptr ||
        search_dock == nullptr || results_dock == nullptr ||
        favorites_dock == nullptr || history_dock == nullptr ||
        nav_toolbar == nullptr || dictionary_bar == nullptr) {
        completion(false);
        return;
    }

    const QList<QAction*> expected_actions = {
        search_dock->toggleViewAction(),
        results_dock->toggleViewAction(),
        favorites_dock->toggleViewAction(),
        history_dock->toggleViewAction(),
        nullptr,
        dictionary_bar->toggleViewAction(),
        nav_toolbar->toggleViewAction(),
    };
    const auto actions = view_menu->actions();
    bool passed =
        app_menu_bar == menuBar() &&
        findChildren<QMenuBar*>(QStringLiteral("menubar")).size() == 1 &&
        findChildren<QMenu*>(QStringLiteral("menuView")).size() == 1 &&
        app_menu_bar->actions().size() == 3 &&
        app_menu_bar->actions()[1]->menu() == view_menu &&
        view_menu->title() == QStringLiteral("&View") &&
        actions.size() == expected_actions.size();
    for (qsizetype index = 0; passed && index < actions.size(); ++index) {
        if (expected_actions[index] == nullptr) {
            passed = actions[index]->isSeparator();
        } else {
            QString accessible_text = actions[index]->text();
            accessible_text.remove('&');
            passed = actions[index] == expected_actions[index] &&
                     !actions[index]->isSeparator() &&
                     actions[index]->isCheckable() &&
                     actions[index]->isEnabled() &&
                     actions[index]->menuRole() != QAction::PreferencesRole &&
                     actions[index]->menuRole() != QAction::AboutRole &&
                     actions[index]->menuRole() != QAction::QuitRole &&
                     !accessible_text.trimmed().isEmpty();
        }
    }
    passed = passed &&
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
    const auto all_actions = findChildren<QAction*>();
    for (const auto& shortcut : {QKeySequence(Qt::CTRL | Qt::Key_S),
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

    const std::string initial_state = CaptureMainWindowState();
    const QList<QPair<QWidget*, QAction*>> exposed_widgets = {
        {search_dock, search_dock->toggleViewAction()},
        {results_dock, results_dock->toggleViewAction()},
        {favorites_dock, favorites_dock->toggleViewAction()},
        {history_dock, history_dock->toggleViewAction()},
        {dictionary_bar, dictionary_bar->toggleViewAction()},
        {nav_toolbar, nav_toolbar->toggleViewAction()},
    };
    for (const auto& [widget, action] : exposed_widgets) {
        int toggles = 0;
        const auto connection = connect(
            action, &QAction::toggled, this, [&toggles](bool) { ++toggles; },
            Qt::DirectConnection);
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
        disconnect(connection);
    }
    passed = passed && CaptureMainWindowState() == initial_state &&
             centralWidget() != nullptr && article_tabs_ != nullptr &&
             article_tabs_->isVisible() && article_tabs_->size().width() > 0 &&
             article_tabs_->size().height() > 0 && kMainWindowStateVersion == 7;
    if (!passed) {
        qWarning() << "view menu smoke check failed" << actions.size()
                   << app_menu_bar->actions().size();
    }
    completion(passed);
}

void MainWindow::RunHistoryMenuSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    auto* history_menu = findChild<QMenu*>(QStringLiteral("menuHistory"));
    auto* history_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    if (history_menu == nullptr || history_dock == nullptr) {
        completion(false);
        return;
    }
    const auto actions = history_menu->actions();
    bool passed =
        menuBar()->actions().size() == 3 &&
        menuBar()->actions()[2]->menu() == history_menu &&
        history_menu->title() == QStringLiteral("H&istory") &&
        actions.size() == 5 && actions[0] == history_dock->toggleViewAction() &&
        actions[1] == export_history_action_ &&
        actions[2] == import_history_action_ && actions[3]->isSeparator() &&
        actions[4] == clear_history_action_ &&
        actions[0]->objectName() == QStringLiteral("showHideHistory") &&
        actions[1]->objectName() == QStringLiteral("exportHistory") &&
        actions[2]->objectName() == QStringLiteral("importHistory") &&
        actions[4]->objectName() == QStringLiteral("clearHistory") &&
        actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_H) &&
        actions[1]->shortcut().isEmpty() && actions[2]->shortcut().isEmpty() &&
        actions[4]->shortcut().isEmpty();
    for (const auto* action :
         {actions[0], actions[1], actions[2], actions[4]}) {
        passed = passed && action->menuRole() == QAction::NoRole;
    }

    SetDictionaryGroups({{7U, "History Menu Group", "", {}}});
    SelectGroup(7U);
    passed = passed && export_history_action_->isEnabled() &&
             import_history_action_->isEnabled() &&
             clear_history_action_->isEnabled();
    const std::string state = CaptureMainWindowState();
    actions[0]->trigger();
    actions[0]->trigger();
    passed = passed && history_dock->isVisible() && actions[0]->isChecked() &&
             CaptureMainWindowState() == state;

    int export_triggers = 0;
    const auto export_connection = connect(
        export_history_action_, &QAction::triggered, this,
        [&export_triggers]() { ++export_triggers; }, Qt::DirectConnection);
    history_export_path_provider_ = [path]() {
        return path;
    };
    export_history_button_->click();
    QFile exported(path);
    const bool export_opened = exported.open(QIODevice::ReadOnly);
    passed =
        passed && export_triggers == 1 && export_opened &&
        exported.readAll() == QByteArray::fromHex("efbbbf") + "Alpha\nBeta\n";
    disconnect(export_connection);

    int import_requests = 0;
    const auto import_connection = connect(
        this, &MainWindow::ImportHistoryRequested, this,
        [&import_requests, &path](const QString& requested,
                                  std::uint32_t group_id) {
            if (requested == path && group_id == 7U) {
                ++import_requests;
            }
        },
        Qt::DirectConnection);
    history_import_path_provider_ = [path]() {
        return path;
    };
    import_history_action_->trigger();
    passed = passed && import_requests == 1 && history_items_.size() == 2 &&
             history_items_[0].group_id == 7U;
    history_import_path_provider_ = []() {
        return QString();
    };
    import_history_button_->click();
    passed = passed && import_requests == 1;
    disconnect(import_connection);

    history_command_busy_ = true;
    UpdateHistoryActions();
    passed = passed && !export_history_action_->isEnabled() &&
             !import_history_action_->isEnabled() &&
             !clear_history_action_->isEnabled();
    history_command_busy_ = false;
    UpdateHistoryActions();
    clear_history_action_->trigger();
    passed = passed && history_items_.empty() &&
             !export_history_action_->isEnabled() &&
             import_history_action_->isEnabled() &&
             !clear_history_action_->isEnabled() &&
             centralWidget() != nullptr && article_tabs_->isVisible() &&
             kMainWindowStateVersion == 7;
    history_export_path_provider_ = {};
    history_import_path_provider_ = {};
    completion(passed);
}

void MainWindow::RunFileMenuSmokeCheck(const QString& path,
                                       std::function<void(bool)> completion) {
    auto* file_menu = findChild<QMenu*>(QStringLiteral("menuFile"));
    auto* article_toolbar =
        findChild<QToolBar*>(QStringLiteral("articleToolbar"));
    auto* add_tab_button =
        findChild<QToolButton*>(QStringLiteral("addArticleTabButton"));
    if (file_menu == nullptr || article_toolbar == nullptr ||
        add_tab_button == nullptr || add_tab_button->menu() == nullptr ||
        facade_ == nullptr || article_view_ == nullptr) {
        completion(false);
        return;
    }

    const auto actions = file_menu->actions();
    bool passed =
        menuBar()->actions().size() == 3 &&
        menuBar()->actions()[0]->menu() == file_menu &&
        menuBar()->actions()[1]->menu()->objectName() ==
            QStringLiteral("menuView") &&
        menuBar()->actions()[2]->menu()->objectName() ==
            QStringLiteral("menuHistory") &&
        findChildren<QMenu*>(QStringLiteral("menuFile")).size() == 1 &&
        file_menu->title() == QStringLiteral("&File") && actions.size() == 8 &&
        actions[0] == new_tab_action_ && actions[1]->isSeparator() &&
        actions[2] == print_preview_action_ && actions[3] == print_action_ &&
        actions[4]->isSeparator() && actions[5] == save_article_action_ &&
        actions[6]->isSeparator() && actions[7] == quit_action_ &&
        new_tab_action_->objectName() == QStringLiteral("newTab") &&
        print_preview_action_->objectName() == QStringLiteral("printPreview") &&
        print_action_->objectName() == QStringLiteral("print") &&
        save_article_action_->objectName() == QStringLiteral("saveArticle") &&
        quit_action_->objectName() == QStringLiteral("quit") &&
        new_tab_action_->text() == QStringLiteral("&New Tab") &&
        print_preview_action_->text() == QStringLiteral("Print Pre&view") &&
        print_action_->text() == QStringLiteral("&Print") &&
        save_article_action_->text() == QStringLiteral("&Save Article") &&
        quit_action_->text() == QStringLiteral("&Quit") &&
        new_tab_action_->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_T) &&
        new_tab_action_->shortcutContext() == Qt::WidgetShortcut &&
        print_preview_action_->shortcut().isEmpty() &&
        print_action_->shortcut() == QKeySequence::Print &&
        save_article_action_->shortcut() == QKeySequence(Qt::Key_F2) &&
        quit_action_->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_Q) &&
        new_tab_action_->menuRole() == QAction::NoRole &&
        print_preview_action_->menuRole() == QAction::NoRole &&
        print_action_->menuRole() == QAction::NoRole &&
        save_article_action_->menuRole() == QAction::NoRole &&
        quit_action_->menuRole() == QAction::QuitRole &&
        article_toolbar->actions().contains(print_preview_action_) &&
        article_toolbar->actions().contains(print_action_) &&
        article_toolbar->actions().contains(save_article_action_) &&
        add_tab_button->menu()->actions().contains(new_tab_action_);

    const auto all_actions = findChildren<QAction*>();
    for (const auto& shortcut :
         {QKeySequence(Qt::CTRL | Qt::Key_T), QKeySequence(QKeySequence::Print),
          QKeySequence(Qt::Key_F2), QKeySequence(Qt::CTRL | Qt::Key_Q)}) {
        passed =
            passed &&
            std::count_if(all_actions.cbegin(), all_actions.cend(),
                          [&shortcut](const QAction* action) {
                              return action->shortcuts().contains(shortcut);
                          }) == 1;
    }

    int new_tab_triggers = 0;
    const auto new_tab_connection = connect(
        new_tab_action_, &QAction::triggered, this,
        [&new_tab_triggers]() { ++new_tab_triggers; }, Qt::DirectConnection);
    const int tab_count = article_tabs_->count();
    add_tab_button->click();
    passed = passed && new_tab_triggers == 1 &&
             article_tabs_->count() == tab_count + 1;
    disconnect(new_tab_connection);
    const auto session = facade_->ExportArticleTabSession();
    const std::string window_state = CaptureMainWindowState();

    int print_triggers = 0;
    int preview_triggers = 0;
    int print_dialogs = 0;
    int print_dispatches = 0;
    const auto print_connection = connect(
        print_action_, &QAction::triggered, this,
        [&print_triggers]() { ++print_triggers; }, Qt::DirectConnection);
    const auto preview_connection = connect(
        print_preview_action_, &QAction::triggered, this,
        [&preview_triggers]() { ++preview_triggers; }, Qt::DirectConnection);
    printer_available_ = []() {
        return true;
    };
    print_dialog_executor_ = [&print_dialogs](QPrinter*) {
        ++print_dialogs;
        return false;
    };
    print_dispatcher_ = [&print_dispatches](ArticleView*, QPrinter*) {
        ++print_dispatches;
    };
    print_action_->trigger();
    passed = passed && print_triggers == 1 && print_dialogs == 1 &&
             print_dispatches == 0 && print_action_->isEnabled() &&
             print_preview_action_->isEnabled();
    print_dialog_executor_ = [&print_dialogs](QPrinter*) {
        ++print_dialogs;
        return true;
    };
    print_action_->trigger();
    passed = passed && print_triggers == 2 && print_dialogs == 2 &&
             print_dispatches == 1 && !print_action_->isEnabled() &&
             !print_preview_action_->isEnabled();
    print_preview_action_->trigger();
    passed = passed && preview_triggers == 0 && print_dispatches == 1;
    emit article_view_->printFinished(false);
    print_preview_executor_ =
        [&print_dispatches](QPrinter*, const std::function<void()>& paint) {
            static_cast<void>(print_dispatches);
            paint();
        };
    print_preview_action_->trigger();
    passed = passed && preview_triggers == 1 && print_dispatches == 2 &&
             !print_action_->isEnabled() && !print_preview_action_->isEnabled();
    emit article_view_->printFinished(true);
    disconnect(print_connection);
    disconnect(preview_connection);
    printer_available_ = {};
    print_dialog_executor_ = {};
    print_preview_executor_ = {};
    print_dispatcher_ = {};

    int quit_triggers = 0;
    int quit_dispatches = 0;
    const auto quit_connection = connect(
        quit_action_, &QAction::triggered, this,
        [&quit_triggers]() { ++quit_triggers; }, Qt::DirectConnection);
    quit_dispatcher_ = [&quit_dispatches]() {
        ++quit_dispatches;
    };
    quit_action_->trigger();
    passed = passed && quit_triggers == 1 && quit_dispatches == 1;
    disconnect(quit_connection);
    quit_dispatcher_ = {};

    QFile destination(path);
    passed = passed && destination.open(QIODevice::WriteOnly) &&
             destination.write("original") == 8;
    destination.close();
    int save_triggers = 0;
    auto save_writes = std::make_shared<int>(0);
    const auto save_connection = connect(
        save_article_action_, &QAction::triggered, this,
        [&save_triggers]() { ++save_triggers; }, Qt::DirectConnection);
    save_article_path_provider_ = []() {
        return QString();
    };
    save_article_action_->trigger();
    passed = passed && save_triggers == 1 && !save_in_progress_;
    save_article_path_provider_ = [path]() {
        return path;
    };
    article_save_writer_ = [save_writes](const QString&, const QString&) {
        ++*save_writes;
        return false;
    };
    save_article_action_->trigger();
    passed = passed && save_triggers == 2 && save_in_progress_ &&
             !save_article_action_->isEnabled();

    auto poll = std::make_shared<std::function<void(int)>>();
    *poll = [this, path, completion = std::move(completion), poll,
             save_connection, save_writes, passed, session,
             window_state](int remaining) mutable {
        if (save_in_progress_ && remaining > 0) {
            QTimer::singleShot(10, this,
                               [poll, remaining]() { (*poll)(remaining - 1); });
            return;
        }
        QFile preserved(path);
        const bool opened = preserved.open(QIODevice::ReadOnly);
        bool final_passed =
            passed && !save_in_progress_ && *save_writes == 1 && opened &&
            preserved.readAll() == "original" &&
            status_->text() == QStringLiteral("HTML save failed") &&
            facade_->ExportArticleTabSession() == session &&
            CaptureMainWindowState() == window_state &&
            centralWidget() != nullptr && article_tabs_->isVisible() &&
            article_tabs_->size().width() > 0 &&
            article_tabs_->size().height() > 0 && kMainWindowStateVersion == 7;
        disconnect(save_connection);
        save_article_path_provider_ = {};
        article_save_writer_ = {};
        completion(final_passed);
    };
    QTimer::singleShot(0, this, [poll]() { (*poll)(100); });
}

void MainWindow::RunWebEngineInteractionCheck(
    std::function<void(bool)> completion) {
    connect(
        article_view_, &QWebEngineView::loadFinished, this,
        [this, completion = std::move(completion)](bool loaded) mutable {
            if (!loaded) {
                completion(false);
                return;
            }
            article_view_->setZoomFactor(1.25);
            article_view_->findText(
                QStringLiteral("needle"), {},
                [this, completion = std::move(completion)](
                    const QWebEngineFindTextResult& result) mutable {
                    const bool interaction_passed =
                        result.numberOfMatches() == 2 &&
                        article_view_->zoomFactor() == 1.25;
                    article_view_->findText(QString());
                    article_view_->page()->toHtml(
                        [this, interaction_passed,
                         completion = std::move(completion)](
                            const QString& html) mutable {
                            article_view_->printToPdf(
                                [interaction_passed, html,
                                 completion = std::move(completion)](
                                    const QByteArray& pdf) mutable {
                                    completion(interaction_passed &&
                                               html.contains(
                                                   QStringLiteral("needle")) &&
                                               pdf.startsWith("%PDF-"));
                                });
                        });
                });
        },
        Qt::SingleShotConnection);
    article_view_->setHtml(QStringLiteral(
        "<!doctype html><html><body><p>needle</p><p>needle</p></body></html>"));
}

void MainWindow::RunArticleContextMenuCheck(
    std::function<void(bool)> completion) {
    ArticleView view;
    view.SetFacade(facade_);
    const ArticleContext internal{
        QStringLiteral(" selected "),
        QUrl(QStringLiteral("goldendict://lookup/linked%20word")), true};
    const auto internal_actions = view.AvailableContextActions(internal);
    const ArticleContext external{
        {}, QUrl(QStringLiteral("https://example.test/article")), false};
    const auto external_actions = view.AvailableContextActions(external);
    const ArticleContext rejected{QString(60, QLatin1Char('x')),
                                  QUrl(QStringLiteral("file:///tmp/untrusted")),
                                  false};
    const auto rejected_actions = view.AvailableContextActions(rejected);
    const auto credential_actions = view.AvailableContextActions(ArticleContext{
        {},
        QUrl(QStringLiteral("https://user:secret@example.test/article")),
        false});
    QString selection;
    QUrl link;
    connect(&view, &ArticleView::SelectionLookupRequested, this,
            [&selection](const QString& text, ArticleLinkDisposition) {
                selection = text;
            });
    connect(&view, &ArticleView::LinkRequested, this,
            [&link](const QUrl& url, ArticleLinkDisposition) { link = url; });
    view.TriggerContextActionForTest(ArticleContextAction::kLookupSelection,
                                     internal);
    view.TriggerContextActionForTest(ArticleContextAction::kOpenLink, internal);
    view.TriggerContextActionForTest(ArticleContextAction::kCopyLink, external);
    const bool link_copied =
        QApplication::clipboard()->text() == external.link_url.toString();
    const auto session_before_copy = facade_->ExportArticleTabSession();
    article_view_->TriggerContextActionForTest(
        ArticleContextAction::kCopyAsText,
        ArticleContext{QStringLiteral("clipboard selection"), {}, false});
    const bool copy_preserved_session =
        facade_->ExportArticleTabSession() == session_before_copy;
    const auto state_before_navigation = facade_->GetArticleTabsState();
    const auto original_tab_id = state_before_navigation.active_tab_id;
    const auto original_tab = std::find_if(state_before_navigation.tabs.begin(),
                                           state_before_navigation.tabs.end(),
                                           [original_tab_id](const auto& tab) {
                                               return tab.id == original_tab_id;
                                           });
    const std::uint32_t original_group =
        original_tab == state_before_navigation.tabs.end()
            ? 0U
            : original_tab->navigation.group_id;
    article_view_->TriggerContextActionForTest(
        ArticleContextAction::kLookupSelectionInNewTab, internal);
    const auto navigation_state = facade_->GetArticleTabsState();
    const bool navigation_preserved_origin =
        navigation_state.tabs.size() ==
            state_before_navigation.tabs.size() + 1U &&
        navigation_state.active_tab_id == original_tab_id &&
        navigation_state.tabs.back().navigation.query == " selected " &&
        navigation_state.tabs.back().navigation.group_id == original_group;
    const bool restored = static_cast<bool>(
        facade_->RestoreArticleTabSession(session_before_copy));
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    const bool passed =
        internal_actions ==
            QList<ArticleContextAction>{
                ArticleContextAction::kOpenLink,
                ArticleContextAction::kOpenLinkInNewTab,
                ArticleContextAction::kLookupSelection,
                ArticleContextAction::kLookupSelectionInNewTab,
                ArticleContextAction::kSendSelectionToInput,
                ArticleContextAction::kCopy,
                ArticleContextAction::kCopyAsText,
                ArticleContextAction::kCopyImage} &&
        external_actions ==
            QList<ArticleContextAction>{ArticleContextAction::kOpenExternalLink,
                                        ArticleContextAction::kCopyLink,
                                        ArticleContextAction::kSelectAll} &&
        rejected_actions ==
            QList<ArticleContextAction>{ArticleContextAction::kCopy,
                                        ArticleContextAction::kCopyAsText} &&
        credential_actions ==
            QList<ArticleContextAction>{ArticleContextAction::kSelectAll} &&
        selection == internal.selected_text && link == internal.link_url &&
        link_copied && copy_preserved_session && navigation_preserved_origin &&
        restored;
    if (!passed) {
        qWarning() << "context menu smoke mismatch" << internal_actions.size()
                   << external_actions.size() << rejected_actions.size()
                   << credential_actions.size() << selection << link
                   << QApplication::clipboard()->text();
    }
    completion(passed);
}

void MainWindow::RunSystemPrintCheck(std::function<void(bool)> completion) {
    int dialog_calls = 0;
    int dispatch_calls = 0;
    int preview_calls = 0;
    const auto session_before = facade_->ExportArticleTabSession();
    printer_available_ = []() {
        return false;
    };
    print_dialog_executor_ = [&dialog_calls](QPrinter*) {
        ++dialog_calls;
        return false;
    };
    print_dispatcher_ = [&dispatch_calls](ArticleView*, QPrinter*) {
        ++dispatch_calls;
    };
    PrintArticle();
    const bool unavailable_safe =
        dialog_calls == 0 && dispatch_calls == 0 &&
        status_->text() == QStringLiteral("No printer is available");
    printer_available_ = []() {
        return true;
    };
    PrintArticle();
    const bool cancellation_safe = dialog_calls == 1 && dispatch_calls == 0;
    print_dialog_executor_ = [&dialog_calls](QPrinter*) {
        ++dialog_calls;
        return true;
    };
    PrintArticle();
    const bool accepted =
        dialog_calls == 2 && dispatch_calls == 1 && print_in_progress_;
    PrintArticle();
    const bool overlap_rejected = dialog_calls == 2 && dispatch_calls == 1;
    emit article_view_->printFinished(true);
    print_preview_executor_ =
        [&preview_calls](QPrinter*, const std::function<void()>& paint) {
            ++preview_calls;
            paint();
        };
    PreviewArticle();
    const bool preview =
        preview_calls == 1 && dispatch_calls == 2 && print_in_progress_;
    emit article_view_->printFinished(false);
    const bool failure_reported =
        status_->text() == QStringLiteral("Printing failed");
    printer_available_ = {};
    print_dialog_executor_ = {};
    print_preview_executor_ = {};
    print_dispatcher_ = {};
    completion(unavailable_safe && cancellation_safe && accepted &&
               overlap_rejected && preview && failure_reported &&
               !print_in_progress_ &&
               facade_->ExportArticleTabSession() == session_before);
}

void MainWindow::RunSuggestionPaneSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().empty()) {
        completion(false);
        return;
    }
    auto* search_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    if (search_dock == nullptr || suggestions_list_ == nullptr) {
        completion(false);
        return;
    }
    ApplyDefaultPaneLayout();
    auto lookup_count = std::make_shared<int>(0);
    connect(this, &MainWindow::LookupSubmitted, this,
            [lookup_count](const QString&, std::uint32_t) { ++*lookup_count; });
    query_->setText(QStringLiteral("a"));
    query_->setText(QStringLiteral("app"));
    auto attempts = std::make_shared<int>(0);
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [this, search_dock, lookup_count, attempts, poll,
             completion = std::move(completion)]() mutable {
        if (suggestions_list_->count() == 0 && ++*attempts < 100) {
            QTimer::singleShot(10, this, *poll);
            return;
        }
        bool passed =
            findChildren<QDockWidget*>(QString::fromLatin1(kSearchPaneName))
                    .size() == 1 &&
            dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
            search_dock->isVisible() && suggestions_list_->count() == 2 &&
            suggestions_list_->item(0)->text() == QStringLiteral("apple") &&
            suggestions_list_->item(1)->text() ==
                QStringLiteral("application") &&
            suggestions_list_->count() <= 20;
        query_->setFocus();
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(query_, &down);
        passed = passed && suggestions_list_->hasFocus() &&
                 suggestions_list_->currentRow() == 0;
        QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
        QApplication::sendEvent(suggestions_list_, &up);
        passed = passed && query_->hasFocus();
        suggestions_list_->setCurrentRow(0);
        emit suggestions_list_->itemClicked(suggestions_list_->item(0));
        passed = passed && query_->text() == QStringLiteral("apple") &&
                 suggestions_list_->count() == 0 && *lookup_count == 1 &&
                 article_view_ != nullptr && article_view_->hasFocus();
        query_->setText(QStringLiteral("app"));
        auto keyboard_attempts = std::make_shared<int>(0);
        auto keyboard_poll = std::make_shared<std::function<void()>>();
        *keyboard_poll = [this, passed, lookup_count, keyboard_attempts,
                          keyboard_poll,
                          completion = std::move(completion)]() mutable {
            if (suggestions_list_->count() < 2 && ++*keyboard_attempts < 100) {
                QTimer::singleShot(10, this, *keyboard_poll);
                return;
            }
            bool keyboard_passed = passed && suggestions_list_->count() == 2;
            suggestions_list_->setCurrentRow(1);
            suggestions_list_->setFocus();
            QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(suggestions_list_, &enter);
            keyboard_passed = keyboard_passed &&
                              query_->text() == QStringLiteral("application") &&
                              suggestions_list_->count() == 0 &&
                              *lookup_count == 2;
            query_->setText(QStringLiteral("app"));
            query_->clear();
            keyboard_passed =
                keyboard_passed && suggestions_list_->count() == 0;
            query_->setText(QString(5000, QLatin1Char('x')));
            QTimer::singleShot(100, this,
                               [this, keyboard_passed,
                                completion = std::move(completion)]() mutable {
                                   FinishLookup();
                                   completion(keyboard_passed &&
                                              suggestions_list_->count() == 0);
                               });
        };
        QTimer::singleShot(10, this, *keyboard_poll);
    };
    QTimer::singleShot(10, this, *poll);
}

void MainWindow::RunArticleTabsSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().empty()) {
        completion(false);
        return;
    }
    goldendict::core::TabNavigationState initial_navigation;
    initial_navigation.title = "(untitled)";
    const goldendict::core::ArticleTabSession initial_session = {
        {{1U, {initial_navigation}, 0U}}, 1U};
    if (!facade_->RestoreArticleTabSession(initial_session)) {
        completion(false);
        return;
    }
    SyncArticleTabs();
    const QSize default_size = size();
    const std::string saved_geometry = CaptureMainWindowGeometry();
    resize(default_size + QSize(37, 29));
    const bool geometry_accepted = RestoreMainWindowGeometry(saved_geometry);
    QApplication::processEvents();
    const bool restored_geometry = geometry_accepted;
    const QRect before_rejected = geometry();
    const bool rejected_geometry =
        !RestoreMainWindowGeometry("not-qt-geometry") &&
        geometry() == before_rejected;
    auto* history_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* favorites_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    auto* results_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    auto* search_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    auto* article_toolbar =
        findChild<QToolBar*>(QStringLiteral("articleToolbar"));
    auto* nav_toolbar = findChild<QToolBar*>(QStringLiteral("navToolbar"));
    auto* dictionary_bar =
        findChild<QToolBar*>(QStringLiteral("dictionaryBar"));
    auto* back_button = findChild<QWidget*>(QStringLiteral("backButton"));
    auto* forward_button = findChild<QWidget*>(QStringLiteral("forwardButton"));
    if (history_dock == nullptr || favorites_dock == nullptr ||
        results_dock == nullptr || search_dock == nullptr ||
        results_list_ == nullptr || suggestions_list_ == nullptr ||
        article_toolbar == nullptr || nav_toolbar == nullptr ||
        dictionary_bar == nullptr || back_button == nullptr ||
        forward_button == nullptr) {
        completion(false);
        return;
    }
    ApplyDefaultPaneLayout();
    const auto nav_actions = nav_toolbar->actions();
    const auto top_level_toolbars =
        findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
    const bool toolbar_order =
        top_level_toolbars.indexOf(nav_toolbar) >= 0 &&
        top_level_toolbars.indexOf(article_toolbar) >= 0 &&
        top_level_toolbars.indexOf(nav_toolbar) <
            top_level_toolbars.indexOf(article_toolbar);
    const bool default_navigation_toolbar =
        findChildren<QToolBar*>(QStringLiteral("navToolbar")).size() == 1 &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea &&
        toolBarArea(article_toolbar) == Qt::TopToolBarArea && toolbar_order &&
        nav_toolbar->isVisible() && nav_toolbar->isMovable() &&
        nav_toolbar->isFloatable() &&
        nav_toolbar->allowedAreas() ==
            (Qt::TopToolBarArea | Qt::BottomToolBarArea) &&
        nav_actions.size() == 3 && nav_actions[0] == back_action_ &&
        nav_actions[1] == forward_action_ &&
        nav_toolbar->widgetForAction(nav_actions[0]) == back_button &&
        nav_toolbar->widgetForAction(nav_actions[1]) == forward_button &&
        group_selector_->parentWidget() == query_->parentWidget() &&
        query_->parentWidget() == lookup_button_->parentWidget() &&
        query_->parentWidget()->parentWidget() == nav_toolbar &&
        group_selector_->objectName() == QStringLiteral("groupSelector") &&
        query_->objectName() == QStringLiteral("translateLine") &&
        lookup_button_->objectName() == QStringLiteral("lookupButton") &&
        query_->minimumWidth() >= 200 &&
        group_selector_->nextInFocusChain() == query_ &&
        query_->nextInFocusChain() == lookup_button_;
    const bool default_dictionary_toolbar =
        findChildren<QToolBar*>(QStringLiteral("dictionaryBar")).size() == 1 &&
        toolBarArea(dictionary_bar) == Qt::TopToolBarArea &&
        dictionary_bar->isVisible() && dictionary_bar->isMovable() &&
        dictionary_bar->isFloatable() &&
        dictionary_bar->allowedAreas() == Qt::AllToolBarAreas;
    query_->setText(QStringLiteral("focus selection"));
    bool focus_shortcuts = false;
    for (auto* action : actions()) {
        if (action->shortcuts().contains(
                QKeySequence(QStringLiteral("Ctrl+L")))) {
            action->trigger();
            QApplication::processEvents();
            focus_shortcuts = query_->focusPolicy() != Qt::NoFocus &&
                              query_->selectedText() == query_->text() &&
                              action->shortcuts().contains(
                                  QKeySequence(QStringLiteral("Alt+D")));
            break;
        }
    }
    query_->clear();
    const bool default_shell =
        findChildren<QDockWidget*>(QString::fromLatin1(kHistoryPaneName))
                .size() == 1 &&
        findChildren<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName))
                .size() == 1 &&
        findChildren<QDockWidget*>(QString::fromLatin1(kResultsPaneName))
                .size() == 1 &&
        findChildren<QDockWidget*>(QString::fromLatin1(kSearchPaneName))
                .size() == 1 &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(results_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible() &&
        tabifiedDockWidgets(history_dock).empty() &&
        tabifiedDockWidgets(favorites_dock).empty() &&
        tabifiedDockWidgets(results_dock).empty() &&
        tabifiedDockWidgets(search_dock).empty() &&
        results_dock->geometry().top() <= favorites_dock->geometry().top() &&
        favorites_dock->geometry().top() <= history_dock->geometry().top() &&
        history_dock->findChild<QListWidget*>(QStringLiteral("historyList")) !=
            nullptr &&
        favorites_dock->findChild<QTreeWidget*>(
            QStringLiteral("favoritesTree")) == favorites_tree_ &&
        results_dock->findChild<QListWidget*>(QStringLiteral("dictsList")) ==
            results_list_ &&
        search_dock->findChild<QListWidget*>(QStringLiteral("wordList")) ==
            suggestions_list_ &&
        lookup_button_->nextInFocusChain() == suggestions_list_ &&
        centralWidget() != nullptr && article_tabs_ != nullptr &&
        !article_tabs_->isHidden() && article_tabs_->size().width() > 0 &&
        article_tabs_->size().height() > 0;
    history_dock->toggleViewAction()->trigger();
    const bool history_hidden = !history_dock->isVisible();
    history_dock->toggleViewAction()->trigger();
    favorites_dock->toggleViewAction()->trigger();
    const bool favorites_hidden = !favorites_dock->isVisible();
    favorites_dock->toggleViewAction()->trigger();
    results_dock->toggleViewAction()->trigger();
    const bool results_hidden = !results_dock->isVisible();
    results_dock->toggleViewAction()->trigger();
    search_dock->toggleViewAction()->trigger();
    const bool search_hidden = !search_dock->isVisible();
    search_dock->toggleViewAction()->trigger();
    const bool toggle_actions =
        history_hidden && favorites_hidden && results_hidden && search_hidden &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible();
    const std::string default_state = CaptureMainWindowState();
    dictionary_bar->setObjectName(
        QStringLiteral("preDictionaryBarPlaceholder"));
    const QByteArray previous_current_state =
        saveState(kPreviousMainWindowStateVersion);
    search_dock->setObjectName(QStringLiteral("preSearchPanePlaceholder"));
    const QByteArray older_current_state =
        saveState(kOlderMainWindowStateVersion);
    results_dock->setObjectName(QStringLiteral("preResultsPanePlaceholder"));
    const QByteArray oldest_current_state =
        saveState(kOldestMainWindowStateVersion);
    nav_toolbar->setObjectName(
        QStringLiteral("preNavigationToolbarPlaceholder"));
    const QByteArray earlier_current_state =
        saveState(kEarlierMainWindowStateVersion);
    history_dock->setObjectName(QString::fromLatin1(kPreviousHistoryDockName));
    favorites_dock->setObjectName(
        QString::fromLatin1(kPreviousFavoritesDockName));
    const QByteArray earliest_current_state =
        saveState(kEarliestMainWindowStateVersion);
    history_dock->setObjectName(QString::fromLatin1(kHistoryPaneName));
    favorites_dock->setObjectName(QString::fromLatin1(kFavoritesPaneName));
    results_dock->setObjectName(QString::fromLatin1(kResultsPaneName));
    search_dock->setObjectName(QString::fromLatin1(kSearchPaneName));
    nav_toolbar->setObjectName(QStringLiteral("navToolbar"));
    dictionary_bar->setObjectName(QStringLiteral("dictionaryBar"));
    addDockWidget(Qt::BottomDockWidgetArea, history_dock);
    addDockWidget(Qt::RightDockWidgetArea, favorites_dock);
    addDockWidget(Qt::LeftDockWidgetArea, results_dock);
    addDockWidget(Qt::RightDockWidgetArea, search_dock);
    addToolBar(Qt::BottomToolBarArea, article_toolbar);
    addToolBar(Qt::BottomToolBarArea, nav_toolbar);
    favorites_dock->hide();
    results_dock->hide();
    search_dock->hide();
    nav_toolbar->hide();
    dictionary_bar->hide();
    const std::string changed_state = CaptureMainWindowState();
    const bool reset_state = RestoreMainWindowState(default_state);
    const bool restored_state =
        RestoreMainWindowState(changed_state) &&
        dockWidgetArea(history_dock) == Qt::BottomDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(results_dock) == Qt::LeftDockWidgetArea &&
        dockWidgetArea(search_dock) == Qt::RightDockWidgetArea &&
        toolBarArea(article_toolbar) == Qt::BottomToolBarArea &&
        toolBarArea(nav_toolbar) == Qt::BottomToolBarArea &&
        !favorites_dock->isVisible() && !results_dock->isVisible() &&
        !search_dock->isVisible() && !nav_toolbar->isVisible() &&
        !dictionary_bar->isVisible();
    const bool restored_previous_current_state =
        RestoreMainWindowState(
            {previous_current_state.constData(),
             static_cast<std::size_t>(previous_current_state.size())}) &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible() &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea &&
        toolBarArea(dictionary_bar) == Qt::TopToolBarArea &&
        dictionary_bar->isVisible();
    const bool restored_older_current_state =
        RestoreMainWindowState(
            {older_current_state.constData(),
             static_cast<std::size_t>(older_current_state.size())}) &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible() &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea;
    const bool restored_oldest_current_state =
        RestoreMainWindowState(
            {oldest_current_state.constData(),
             static_cast<std::size_t>(oldest_current_state.size())}) &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(results_dock) == Qt::RightDockWidgetArea &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible() &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea;
    const bool restored_earlier_current_state =
        RestoreMainWindowState(
            {earlier_current_state.constData(),
             static_cast<std::size_t>(earlier_current_state.size())}) &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        results_dock->isVisible() && search_dock->isVisible() &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea &&
        toolBarArea(dictionary_bar) == Qt::TopToolBarArea &&
        dictionary_bar->isVisible();
    const bool restored_earliest_current_state =
        RestoreMainWindowState(
            {earliest_current_state.constData(),
             static_cast<std::size_t>(earliest_current_state.size())}) &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        search_dock->isVisible() && results_dock->isVisible() &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea;
    const std::string before_bad_state = CaptureMainWindowState();
    const QByteArray incompatible_state = saveState(1);
    const bool rejected_state =
        !RestoreMainWindowState("not-qt-main-window-state") &&
        CaptureMainWindowState() == before_bad_state &&
        !RestoreMainWindowState(
            {incompatible_state.constData(),
             static_cast<std::size_t>(incompatible_state.size())}) &&
        CaptureMainWindowState() == before_bad_state &&
        !RestoreMainWindowState(std::string(64U * 1024U + 1U, 'x')) &&
        CaptureMainWindowState() == before_bad_state;
    favorites_dock->show();
    favorites_dock->setFloating(true);
    favorites_dock->setGeometry(100000, 100000, 240, 320);
    const std::string unreachable_state = CaptureMainWindowState();
    RestoreMainWindowState(default_state);
    const std::string before_unreachable = CaptureMainWindowState();
    const bool topology_restored = RestoreMainWindowState(unreachable_state);
    const bool topology_safe =
        topology_restored ? HasUsableMainWindowLayout()
                          : CaptureMainWindowState() == before_unreachable;
    const bool final_default = RestoreMainWindowState(default_state);
    bool passed =
        article_tabs_->count() == 1 &&
        facade_->GetArticleTabsState().tabs.size() == 1U && restored_geometry &&
        rejected_geometry && default_shell && default_navigation_toolbar &&
        default_dictionary_toolbar && focus_shortcuts && toggle_actions &&
        reset_state && restored_state && restored_previous_current_state &&
        rejected_state && restored_older_current_state &&
        restored_oldest_current_state && restored_earlier_current_state &&
        restored_earliest_current_state && topology_safe && final_default;
    if (!passed) {
        qWarning() << "shell state check failed" << restored_geometry
                   << rejected_geometry << default_shell << toggle_actions
                   << default_navigation_toolbar << default_dictionary_toolbar
                   << focus_shortcuts << reset_state << restored_state
                   << restored_previous_current_state << rejected_state
                   << restored_older_current_state
                   << restored_oldest_current_state
                   << restored_earlier_current_state << topology_safe
                   << restored_earliest_current_state << final_default;
    }
    query_->setText(QStringLiteral("application"));
    SelectGroup(0U);
    StartLookup();
    query_->setText(QStringLiteral("apple"));
    StartLookupInTab(goldendict::core::TabOpenPolicy::kNewTab,
                     goldendict::core::TabActivationPolicy::kActivate);
    query_->setText(QStringLiteral("application"));
    StartLookupInTab(goldendict::core::TabOpenPolicy::kNewTab,
                     goldendict::core::TabActivationPolicy::kKeepActive);

    auto poll = std::make_shared<std::function<void()>>();
    auto rendered = std::make_shared<bool>(false);
    *poll = [this, passed, completion = std::move(completion), poll,
             rendered]() mutable {
        FinishLookup();
        if (!requests_.empty()) {
            QTimer::singleShot(10, this, *poll);
            return;
        }
        if (!*rendered) {
            *rendered = true;
            QTimer::singleShot(250, this, *poll);
            return;
        }
        auto state = facade_->GetArticleTabsState();
        bool ok =
            passed && state.tabs.size() == 3U && article_tabs_->count() == 3 &&
            state.tabs[0].navigation.query == "application" &&
            state.tabs[1].navigation.query == "apple" &&
            state.tabs[2].navigation.query == "application" &&
            state.active_tab_id == state.tabs[1].id &&
            results_list_->count() == 1 && results_list_->currentRow() == 0 &&
            results_list_->item(0)->text() ==
                QStringLiteral("Fixture Dictionary") &&
            results_list_->item(0)->data(Qt::UserRole).toString() ==
                QString::fromStdString(
                    facade_->GetDictionaryService().GetCatalog().front().id);
        if (!ok) {
            qWarning()
                << "results navigation check failed" << passed
                << state.tabs.size() << article_tabs_->count()
                << results_list_->count()
                << (results_list_->count() == 0
                        ? QString()
                        : results_list_->item(0)->text())
                << facade_->GetDictionaryService().GetCatalog().front().id;
        }
        auto* first = ArticleViewForTab(state.tabs[0].id);
        auto* second = ArticleViewForTab(state.tabs[1].id);
        auto* third = ArticleViewForTab(state.tabs[2].id);
        if (first == nullptr || second == nullptr || third == nullptr) {
            completion(false);
            return;
        }
        first->page()->toPlainText([this, second, third, ok,
                                    completion = std::move(completion)](
                                       const QString& first_text) mutable {
            second->page()->toPlainText([this, third, ok, first_text,
                                         completion = std::move(completion)](
                                            const QString&
                                                second_text) mutable {
                third->page()->toPlainText([this, ok, first_text, second_text,
                                            completion = std::move(completion)](
                                               const QString&
                                                   third_text) mutable {
                    bool smoke_passed = ok &&
                                        first_text.contains("A program") &&
                                        second_text.contains("A fruit") &&
                                        third_text.contains("A program");
                    auto current_state = facade_->GetArticleTabsState();
                    facade_->ActivateArticleTab(current_state.tabs[0].id);
                    SyncArticleTabs();
                    smoke_passed = smoke_passed &&
                                   query_->text() == "application" &&
                                   selected_group_id_ == 0U &&
                                   results_list_->count() == 1 &&
                                   results_list_->currentRow() == 0;
                    auto activations = std::make_shared<int>(0);
                    connect(
                        results_list_, &QListWidget::itemActivated, this,
                        [activations](QListWidgetItem*) { ++*activations; },
                        Qt::SingleShotConnection);
                    results_list_->setFocus();
                    QKeyEvent activate_press(QEvent::KeyPress, Qt::Key_Return,
                                             Qt::NoModifier);
                    QKeyEvent activate_release(QEvent::KeyRelease,
                                               Qt::Key_Return, Qt::NoModifier);
                    QApplication::sendEvent(results_list_, &activate_press);
                    QApplication::sendEvent(results_list_, &activate_release);
                    smoke_passed = smoke_passed && *activations == 1;
                    auto mouse_activations = std::make_shared<int>(0);
                    connect(
                        results_list_, &QListWidget::itemActivated, this,
                        [mouse_activations](QListWidgetItem*) {
                            ++*mouse_activations;
                        },
                        Qt::SingleShotConnection);
                    results_list_->setFocus();
                    const QPoint result_position =
                        results_list_->visualItemRect(results_list_->item(0))
                            .center();
                    QMouseEvent mouse_press(
                        QEvent::MouseButtonPress, result_position,
                        results_list_->viewport()->mapToGlobal(result_position),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent mouse_release(
                        QEvent::MouseButtonRelease, result_position,
                        results_list_->viewport()->mapToGlobal(result_position),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QMouseEvent double_click(
                        QEvent::MouseButtonDblClick, result_position,
                        results_list_->viewport()->mapToGlobal(result_position),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(results_list_->viewport(),
                                            &mouse_press);
                    QApplication::sendEvent(results_list_->viewport(),
                                            &mouse_release);
                    QApplication::sendEvent(results_list_->viewport(),
                                            &double_click);
                    QApplication::sendEvent(results_list_->viewport(),
                                            &mouse_release);
                    smoke_passed = smoke_passed && *mouse_activations == 1;
                    NavigateArticleTab(false);
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed &&
                        current_state.tabs[0].navigation.kind ==
                            goldendict::core::TabNavigationKind::kEmpty;
                    NavigateArticleTab(true);
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed &&
                        current_state.tabs[0].navigation.query == "application";
                    NavigateArticleTab(false);
                    query_->setText(QStringLiteral("apple"));
                    StartLookup();
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed && !current_state.tabs[0].can_go_forward;

                    article_page_->LookupRequested(
                        QStringLiteral("application"),
                        QStringLiteral("goldendict://lookup/application"),
                        ArticleLinkDisposition::kCurrentTab);
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed && current_state.tabs.size() == 3U &&
                        current_state.tabs.front().navigation.kind ==
                            goldendict::core::TabNavigationKind::kInternalLink;
                    article_page_->LookupRequested(
                        QStringLiteral("application"),
                        QStringLiteral("goldendict://lookup/application"),
                        ArticleLinkDisposition::kNewBackgroundTab);
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed = smoke_passed &&
                                   current_state.tabs.size() == 4U &&
                                   current_state.tabs.back().navigation.kind ==
                                       goldendict::core::TabNavigationKind::
                                           kInternalLink &&
                                   current_state.active_tab_id ==
                                       current_state.tabs.front().id;

                    const int before_middle = article_tabs_->count();
                    const int middle_index = before_middle - 1;
                    const QPoint middle_position =
                        article_tabs_->tabBar()->tabRect(middle_index).center();
                    QMouseEvent middle_event(
                        QEvent::MouseButtonPress, middle_position,
                        middle_position,
                        article_tabs_->tabBar()->mapToGlobal(middle_position),
                        Qt::MiddleButton, Qt::MiddleButton, {});
                    QApplication::sendEvent(article_tabs_->tabBar(),
                                            &middle_event);
                    smoke_passed = smoke_passed &&
                                   article_tabs_->count() == before_middle - 1;

                    current_state = facade_->GetArticleTabsState();
                    const auto expected_fallback = current_state.tabs[1].id;
                    CloseArticleTab(article_tabs_->currentIndex());
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed &&
                        current_state.active_tab_id == expected_fallback;

                    while (current_state.tabs.size() <
                           goldendict::core::kMaximumArticleTabs) {
                        goldendict::core::TabNavigationState extra;
                        extra.kind =
                            goldendict::core::TabNavigationKind::kLookup;
                        extra.query = "limit-" +
                                      std::to_string(current_state.tabs.size());
                        extra.title = extra.query;
                        if (!facade_->OpenArticleTab(
                                extra, goldendict::core::TabOpenPolicy::kNewTab,
                                goldendict::core::TabActivationPolicy::
                                    kKeepActive)) {
                            smoke_passed = false;
                            break;
                        }
                        current_state = facade_->GetArticleTabsState();
                    }
                    goldendict::core::TabNavigationState overflow;
                    overflow.kind =
                        goldendict::core::TabNavigationKind::kLookup;
                    overflow.query = "overflow";
                    overflow.title = overflow.query;
                    smoke_passed =
                        smoke_passed &&
                        facade_->OpenArticleTab(
                                   overflow,
                                   goldendict::core::TabOpenPolicy::kNewTab,
                                   goldendict::core::TabActivationPolicy::
                                       kKeepActive)
                                .error == goldendict::core::TabOperationError::
                                              kTabLimitReached;

                    CloseOtherArticleTabs(article_tabs_->currentIndex());
                    smoke_passed = smoke_passed && article_tabs_->count() == 1;
                    goldendict::core::TabOperationError navigation_error =
                        goldendict::core::TabOperationError::kNone;
                    for (std::size_t index = 0;
                         index <=
                         goldendict::core::kMaximumTabNavigationEntries;
                         ++index) {
                        goldendict::core::TabNavigationState bounded;
                        bounded.kind =
                            goldendict::core::TabNavigationKind::kLookup;
                        bounded.query = "navigation-" + std::to_string(index);
                        bounded.title = bounded.query;
                        const auto result = facade_->OpenArticleTab(
                            bounded,
                            goldendict::core::TabOpenPolicy::kCurrentTab,
                            goldendict::core::TabActivationPolicy::kActivate);
                        if (!result) {
                            navigation_error = result.error;
                            break;
                        }
                    }
                    smoke_passed = smoke_passed &&
                                   navigation_error ==
                                       goldendict::core::TabOperationError::
                                           kNavigationLimitReached;
                    SyncArticleTabs();
                    const auto retained_id =
                        facade_->GetArticleTabsState().active_tab_id;
                    CloseArticleTab(article_tabs_->currentIndex());
                    const auto replacement = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed && replacement.tabs.size() == 1U &&
                        replacement.active_tab_id != retained_id &&
                        replacement.tabs.front().navigation.kind ==
                            goldendict::core::TabNavigationKind::kEmpty;
                    goldendict::core::TabNavigationState empty;
                    empty.title = "(untitled)";
                    const goldendict::core::ArticleTabSession placement = {
                        {{10U, {empty}, 0U},
                         {20U, {empty}, 0U},
                         {30U, {empty}, 0U}},
                        20U};
                    smoke_passed = smoke_passed &&
                                   facade_->RestoreArticleTabSession(placement);
                    SyncArticleTabs();
                    auto configured = preferences_;
                    configured.open_new_tabs_after_current = true;
                    configured.open_new_tabs_in_background = true;
                    SetPreferences(configured);
                    auto* add_button = findChild<QToolButton*>(
                        QStringLiteral("addArticleTabButton"));
                    if (add_button == nullptr) {
                        completion(false);
                        return;
                    }
                    add_button->click();
                    auto configured_state = facade_->GetArticleTabsState();
                    smoke_passed = smoke_passed &&
                                   configured_state.tabs.size() == 4U &&
                                   configured_state.tabs[0].id == 10U &&
                                   configured_state.tabs[1].id == 20U &&
                                   configured_state.tabs[3].id == 30U &&
                                   configured_state.active_tab_id == 20U;
                    configured.open_new_tabs_in_background = false;
                    SetPreferences(configured);
                    add_button->click();
                    configured_state = facade_->GetArticleTabsState();
                    smoke_passed = smoke_passed &&
                                   configured_state.tabs.size() == 5U &&
                                   configured_state.tabs[1].id == 20U &&
                                   configured_state.active_tab_id ==
                                       configured_state.tabs[2].id;
                    completion(smoke_passed);
                });
            });
        });
    };
    QTimer::singleShot(10, this, *poll);
}

void MainWindow::RunArticleTabSessionRestartSmokeCheck(
    bool prepare, std::function<void(bool)> completion) {
    if (facade_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().empty()) {
        completion(false);
        return;
    }
    goldendict::core::TabNavigationState empty;
    empty.title = "(untitled)";
    goldendict::core::TabNavigationState lookup;
    lookup.kind = goldendict::core::TabNavigationKind::kLookup;
    lookup.query = "application";
    lookup.group_id = 7U;
    lookup.title = "application";
    goldendict::core::TabNavigationState link;
    link.kind = goldendict::core::TabNavigationKind::kInternalLink;
    link.query = "apple";
    link.group_id = 7U;
    link.title = "apple link";
    link.internal_url = "goldendict://lookup/apple";
    link.source_dictionary_id = "fixture-source";
    link.source_article_id = "source-article";
    link.target_article_id = "target-article";
    link.target_anchor = "target-anchor";
    goldendict::core::TabNavigationState active_link = link;
    active_link.query = "application";
    active_link.group_id = 9U;
    active_link.title = "application link";
    active_link.internal_url = "goldendict://lookup/application";
    active_link.target_article_id = "active-target";
    const goldendict::core::ArticleTabSession expected = {
        {{7U, {empty, lookup, link}, 1U}, {42U, {link, active_link}, 1U}}, 42U};
    auto* history_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* favorites_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    auto* results_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    auto* search_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    auto* article_toolbar =
        findChild<QToolBar*>(QStringLiteral("articleToolbar"));
    auto* nav_toolbar = findChild<QToolBar*>(QStringLiteral("navToolbar"));
    auto* dictionary_bar =
        findChild<QToolBar*>(QStringLiteral("dictionaryBar"));
    if (history_dock == nullptr || favorites_dock == nullptr ||
        results_dock == nullptr || search_dock == nullptr ||
        article_toolbar == nullptr || nav_toolbar == nullptr ||
        dictionary_bar == nullptr) {
        completion(false);
        return;
    }
    if (prepare) {
        const auto restored = facade_->RestoreArticleTabSession(expected);
        if (!restored) {
            completion(false);
            return;
        }
        RebuildArticleTabs();
        addDockWidget(Qt::BottomDockWidgetArea, history_dock);
        addDockWidget(Qt::RightDockWidgetArea, favorites_dock);
        addDockWidget(Qt::LeftDockWidgetArea, results_dock);
        addDockWidget(Qt::RightDockWidgetArea, search_dock);
        addToolBar(Qt::BottomToolBarArea, article_toolbar);
        addToolBar(Qt::BottomToolBarArea, nav_toolbar);
        addToolBar(Qt::BottomToolBarArea, dictionary_bar);
        favorites_dock->hide();
        results_dock->hide();
        search_dock->hide();
        dictionary_bar->hide();
        emit ArticleTabSessionMutated();
        completion(true);
        return;
    }
    bool passed = facade_->ExportArticleTabSession() == expected &&
                  article_tabs_->count() == 2 && TabIdAt(0) == 7U &&
                  TabIdAt(1) == 42U &&
                  TabIdAt(article_tabs_->currentIndex()) == 42U &&
                  dockWidgetArea(history_dock) == Qt::BottomDockWidgetArea &&
                  dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
                  dockWidgetArea(results_dock) == Qt::LeftDockWidgetArea &&
                  dockWidgetArea(search_dock) == Qt::RightDockWidgetArea &&
                  toolBarArea(article_toolbar) == Qt::BottomToolBarArea &&
                  toolBarArea(nav_toolbar) == Qt::BottomToolBarArea &&
                  toolBarArea(dictionary_bar) == Qt::BottomToolBarArea &&
                  nav_toolbar->isVisible() && !favorites_dock->isVisible() &&
                  !results_dock->isVisible() && !search_dock->isVisible() &&
                  !dictionary_bar->isVisible();
    if (!passed) {
        qWarning() << "restart shell check failed"
                   << (facade_->ExportArticleTabSession() == expected)
                   << article_tabs_->count() << TabIdAt(0) << TabIdAt(1)
                   << TabIdAt(article_tabs_->currentIndex())
                   << dockWidgetArea(history_dock)
                   << dockWidgetArea(favorites_dock)
                   << dockWidgetArea(results_dock)
                   << dockWidgetArea(search_dock)
                   << toolBarArea(article_toolbar) << toolBarArea(nav_toolbar)
                   << toolBarArea(dictionary_bar) << nav_toolbar->isVisible()
                   << favorites_dock->isVisible() << results_dock->isVisible()
                   << search_dock->isVisible() << dictionary_bar->isVisible();
    }
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [this, passed, completion = std::move(completion), poll]() mutable {
        FinishLookup();
        if (!requests_.empty()) {
            QTimer::singleShot(10, this, *poll);
            return;
        }
        auto* first = ArticleViewForTab(7U);
        auto* second = ArticleViewForTab(42U);
        if (first == nullptr || second == nullptr) {
            completion(false);
            return;
        }
        first->page()->toPlainText([this, second, passed,
                                    completion = std::move(completion)](
                                       const QString& first_text) mutable {
            second->page()->toPlainText(
                [this, passed, first_text, completion = std::move(completion)](
                    const QString& second_text) mutable {
                    goldendict::core::TabNavigationState next;
                    next.title = "(untitled)";
                    const auto opened = facade_->OpenArticleTab(
                        next, goldendict::core::TabOpenPolicy::kNewTab,
                        goldendict::core::TabActivationPolicy::kActivate);
                    bool ok = passed && first_text.contains("A program") &&
                              second_text.contains("A program") && opened &&
                              opened.tab_id == 43U;
                    if (opened) {
                        SyncArticleTabs();
                        emit ArticleTabSessionMutated();
                    }
                    completion(ok);
                });
        });
    };
    QTimer::singleShot(250, this, *poll);
}

void MainWindow::RunHistorySmokeCheck(std::function<void(bool)> completion) {
    const QString expected = QStringLiteral("history-smoke-entry");
    SetDictionaryGroups({{7U, "History Smoke Group", "", {}}});
    connect(
        this, &MainWindow::LookupSubmitted, this,
        [this, expected, completion = std::move(completion)](
            const QString& submitted, std::uint32_t group_id) mutable {
            const bool recorded =
                submitted == expected && group_id == 7U &&
                history_list_->count() > 0 &&
                history_list_->item(0)->text() == expected &&
                history_list_->item(0)->data(Qt::UserRole).value<quint32>() ==
                    7U;
            SelectGroup(0U);
            connect(
                this, &MainWindow::LookupSubmitted, this,
                [recorded, completion = std::move(completion)](
                    const QString& restored, std::uint32_t restored_group) {
                    completion(recorded &&
                               restored ==
                                   QStringLiteral("history-smoke-entry") &&
                               restored_group == 7U);
                },
                Qt::SingleShotConnection);
            emit history_list_->itemActivated(history_list_->item(0));
        },
        Qt::SingleShotConnection);
    SelectGroup(7U);
    query_->setText(expected);
    StartLookup();
}

void MainWindow::RunHistoryManagementSmokeCheck(
    std::function<void(bool)> completion) {
    SetHistoryWords({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                     QStringLiteral("Alpine")});
    history_filter_->setText(QStringLiteral("alp"));
    const bool filtered =
        history_list_->count() == 2 &&
        history_list_->item(0)->text() == QStringLiteral("Alpha") &&
        history_list_->item(1)->text() == QStringLiteral("Alpine");
    connect(
        this, &MainWindow::ClearHistoryRequested, this,
        [this, filtered, completion = std::move(completion)]() mutable {
            completion(filtered && history_items_.empty() &&
                       history_list_->count() == 0);
        },
        Qt::SingleShotConnection);
    clear_history_button_->click();
}

void MainWindow::RunHistoryExportSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    const QString error = history_export_callback_(path);
    QFile file(path);
    const bool opened = file.open(QIODevice::ReadOnly);
    const QByteArray expected = QByteArray::fromHex("efbbbf") + "Alpha\nBeta\n";
    completion(error.isEmpty() && opened && file.readAll() == expected);
}

void MainWindow::RunHistoryImportSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    connect(
        this, &MainWindow::ImportHistoryRequested, this,
        [this, path, completion = std::move(completion)](
            const QString& requested_path, std::uint32_t group_id) mutable {
            completion(requested_path == path && group_id == 0U &&
                       history_items_.size() == 2 &&
                       history_items_[0].word == QStringLiteral("Alpha") &&
                       history_items_[1].word == QStringLiteral("第二个"));
        },
        Qt::SingleShotConnection);
    emit ImportHistoryRequested(path, selected_group_id_);
}

void MainWindow::RunFavoritesSmokeCheck(std::function<void(bool)> completion) {
    const QString expected = QStringLiteral("favorites-smoke-entry");
    const int initial_count = favorites_tree_->topLevelItemCount();
    connect(
        this, &MainWindow::AddFavoriteFolderRequested, this,
        [this, expected, initial_count, completion = std::move(completion)](
            const QString& folder_name,
            const QList<int>& folder_parent_path) mutable {
            auto* folder = favorites_tree_->topLevelItem(initial_count);
            const bool folder_added =
                folder_name == QStringLiteral("Smoke Folder") &&
                folder_parent_path.isEmpty() && folder != nullptr &&
                folder->data(0, Qt::UserRole).toBool();
            favorites_tree_->setCurrentItem(folder);
            connect(
                this, &MainWindow::AddFavoriteRequested, this,
                [this, expected, folder_added, initial_count,
                 completion = std::move(completion)](
                    const QString& submitted,
                    const QList<int>& word_parent_path) mutable {
                    auto* refreshed_folder =
                        favorites_tree_->topLevelItem(initial_count);
                    const bool nested =
                        folder_added && submitted == expected &&
                        word_parent_path == QList<int>{initial_count} &&
                        refreshed_folder != nullptr &&
                        refreshed_folder->childCount() == 1 &&
                        refreshed_folder->child(0)->text(0) == expected;
                    connect(
                        this, &MainWindow::RenameFavoriteRequested, this,
                        [this, nested, initial_count,
                         completion = std::move(completion)](
                            const QList<int>& path,
                            const QString& renamed_name) mutable {
                            auto* renamed_folder =
                                favorites_tree_->topLevelItem(initial_count);
                            const bool renamed =
                                nested &&
                                path == QList<int>{initial_count, 0} &&
                                renamed_name ==
                                    QStringLiteral("renamed-entry") &&
                                renamed_folder != nullptr &&
                                renamed_folder->childCount() == 1 &&
                                renamed_folder->child(0)->text(0) ==
                                    renamed_name;
                            if (!renamed) {
                                completion(false);
                                return;
                            }
                            connect(
                                this, &MainWindow::AddFavoriteRequested, this,
                                [this, renamed, initial_count,
                                 completion = std::move(completion)](
                                    const QString& second_word,
                                    const QList<int>&
                                        second_parent_path) mutable {
                                    auto* folder_after_add =
                                        favorites_tree_->topLevelItem(
                                            initial_count);
                                    const bool second_added =
                                        renamed &&
                                        second_word ==
                                            QStringLiteral("second-entry") &&
                                        second_parent_path ==
                                            QList<int>{initial_count} &&
                                        folder_after_add != nullptr &&
                                        folder_after_add->childCount() == 2 &&
                                        folder_after_add->child(0)->text(0) ==
                                            QStringLiteral("renamed-entry") &&
                                        folder_after_add->child(1)->text(0) ==
                                            second_word;
                                    if (!second_added) {
                                        completion(false);
                                        return;
                                    }
                                    connect(
                                        this,
                                        &MainWindow::MoveFavoriteRequested,
                                        this,
                                        [this, second_added, initial_count,
                                         completion = std::move(completion)](
                                            const QList<int>& move_path,
                                            int offset) mutable {
                                            auto* folder_after_move =
                                                favorites_tree_->topLevelItem(
                                                    initial_count);
                                            const bool moved =
                                                second_added && offset == -1 &&
                                                move_path ==
                                                    QList<int>{initial_count,
                                                               1} &&
                                                folder_after_move != nullptr &&
                                                folder_after_move
                                                        ->childCount() == 2 &&
                                                folder_after_move->child(0)
                                                        ->text(0) ==
                                                    QStringLiteral(
                                                        "second-entry") &&
                                                folder_after_move->child(1)
                                                        ->text(0) ==
                                                    QStringLiteral(
                                                        "renamed-entry");
                                            if (!moved) {
                                                completion(false);
                                                return;
                                            }
                                            connect(
                                                this,
                                                &MainWindow::
                                                    MoveFavoriteToRootRequested,
                                                this,
                                                [this, moved, initial_count,
                                                 completion =
                                                     std::move(completion)](
                                                    const QList<int>&
                                                        root_path) mutable {
                                                    auto* root_item =
                                                        favorites_tree_
                                                            ->topLevelItem(
                                                                initial_count +
                                                                1);
                                                    const bool moved_to_root =
                                                        moved &&
                                                        root_path ==
                                                            QList<int>{
                                                                initial_count,
                                                                0} &&
                                                        root_item != nullptr &&
                                                        root_item->text(0) ==
                                                            QStringLiteral(
                                                                "second-entry");
                                                    connect(
                                                        this,
                                                        &MainWindow::
                                                            RemoveFavoriteRequested,
                                                        this,
                                                        [this, moved_to_root,
                                                         initial_count,
                                                         completion = std::move(
                                                             completion)](
                                                            const QList<int>&
                                                                root_remove_path) mutable {
                                                            const bool root_removed =
                                                                moved_to_root &&
                                                                root_remove_path ==
                                                                    QList<int>{
                                                                        initial_count +
                                                                        1} &&
                                                                favorites_tree_
                                                                        ->topLevelItemCount() ==
                                                                    initial_count +
                                                                        1;
                                                            connect(
                                                                this,
                                                                &MainWindow::
                                                                    RemoveFavoriteRequested,
                                                                this,
                                                                [this,
                                                                 root_removed,
                                                                 initial_count,
                                                                 completion =
                                                                     std::move(
                                                                         completion)](
                                                                    const QList<
                                                                        int>&
                                                                        folder_remove_path) mutable {
                                                                    completion(
                                                                        root_removed &&
                                                                        folder_remove_path ==
                                                                            QList<
                                                                                int>{
                                                                                initial_count} &&
                                                                        favorites_tree_
                                                                                ->topLevelItemCount() ==
                                                                            initial_count);
                                                                },
                                                                Qt::SingleShotConnection);
                                                            emit RemoveFavoriteRequested(
                                                                {initial_count});
                                                        },
                                                        Qt::SingleShotConnection);
                                                    emit
                                                        RemoveFavoriteRequested(
                                                            {initial_count +
                                                             1});
                                                },
                                                Qt::SingleShotConnection);
                                            emit MoveFavoriteToRootRequested(
                                                {initial_count, 0});
                                        },
                                        Qt::SingleShotConnection);
                                    favorites_tree_->setCurrentItem(
                                        folder_after_add->child(1));
                                    move_favorite_up_action_->trigger();
                                },
                                Qt::SingleShotConnection);
                            emit AddFavoriteRequested(
                                QStringLiteral("second-entry"),
                                {initial_count});
                        },
                        Qt::SingleShotConnection);
                    emit RenameFavoriteRequested(
                        {initial_count, 0}, QStringLiteral("renamed-entry"));
                },
                Qt::SingleShotConnection);
            query_->setText(expected);
            add_favorite_action_->trigger();
        },
        Qt::SingleShotConnection);
    emit AddFavoriteFolderRequested(QStringLiteral("Smoke Folder"), {});
}

void MainWindow::RunFavoritesCrossFolderMoveSmokeCheck(
    std::function<void(bool)> completion) {
    while (favorites_tree_->topLevelItemCount() > 0) {
        emit RemoveFavoriteRequested(
            {favorites_tree_->topLevelItemCount() - 1});
    }
    const int initial_count = favorites_tree_->topLevelItemCount();
    emit AddFavoriteFolderRequested(QStringLiteral("Source"), {});
    emit AddFavoriteFolderRequested(QStringLiteral("Target"), {});
    emit AddFavoriteFolderRequested(QStringLiteral("Subtree"), {initial_count});
    emit AddFavoriteRequested(QStringLiteral("nested-entry"),
                              {initial_count, 0});

    auto* source = favorites_tree_->topLevelItem(initial_count);
    auto* target = favorites_tree_->topLevelItem(initial_count + 1);
    if (source == nullptr || target == nullptr || source->childCount() != 1) {
        completion(false);
        return;
    }
    source->setExpanded(true);
    source->child(0)->setExpanded(true);
    connect(
        this, &MainWindow::MoveFavoriteAcrossFoldersRequested, this,
        [this, initial_count, completion = std::move(completion)](
            const QList<int>&, const QList<int>&, int,
            const QList<QList<int>>&) mutable {
            auto* refreshed_target =
                favorites_tree_->topLevelItem(initial_count + 1);
            const bool subtree_moved =
                refreshed_target != nullptr &&
                refreshed_target->childCount() == 1 &&
                refreshed_target->child(0)->text(0) ==
                    QStringLiteral("Subtree") &&
                refreshed_target->child(0)->isExpanded() &&
                favorites_tree_->currentItem() == refreshed_target->child(0);
            if (!subtree_moved) {
                completion(false);
                return;
            }
            connect(
                this, &MainWindow::MoveFavoriteAcrossFoldersRequested, this,
                [this, initial_count, completion = std::move(completion)](
                    const QList<int>&, const QList<int>&, int,
                    const QList<QList<int>>&) mutable {
                    auto* root_word =
                        favorites_tree_->topLevelItem(initial_count + 2);
                    completion(root_word != nullptr &&
                               root_word->text(0) ==
                                   QStringLiteral("nested-entry") &&
                               favorites_tree_->currentItem() == root_word &&
                               favorites_tree_->topLevelItem(initial_count + 1)
                                       ->child(0)
                                       ->childCount() == 0);
                },
                Qt::SingleShotConnection);
            emit MoveFavoriteAcrossFoldersRequested(
                {initial_count + 1, 0, 0}, {}, initial_count + 2,
                ExpandedFavoriteFolderPaths());
        },
        Qt::SingleShotConnection);
    emit MoveFavoriteAcrossFoldersRequested({initial_count, 0},
                                            {initial_count + 1}, 0,
                                            ExpandedFavoriteFolderPaths());
}

void MainWindow::RunFavoritesTransferSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    const int initial_count = favorites_tree_->topLevelItemCount();
    connect(
        this, &MainWindow::AddFavoriteFolderRequested, this,
        [this, path, initial_count, completion = std::move(completion)](
            const QString& folder_name, const QList<int>& parent_path) mutable {
            auto* folder = favorites_tree_->topLevelItem(initial_count);
            const bool folder_added =
                folder_name == QStringLiteral("Transfer Folder") &&
                parent_path.isEmpty() && folder != nullptr;
            connect(
                this, &MainWindow::ExportFavoritesRequested, this,
                [this, path, folder_added, folder, initial_count,
                 completion = std::move(completion)](
                    const QString& export_path) mutable {
                    const bool exported = folder_added && export_path == path &&
                                          QFileInfo(export_path).size() > 0;
                    connect(
                        this, &MainWindow::RemoveFavoriteRequested, this,
                        [this, path, exported, initial_count,
                         completion = std::move(completion)](
                            const QList<int>& remove_path) mutable {
                            const bool removed =
                                exported &&
                                remove_path == QList<int>{initial_count} &&
                                favorites_tree_->topLevelItemCount() ==
                                    initial_count;
                            connect(
                                this, &MainWindow::ImportFavoritesRequested,
                                this,
                                [this, path, removed, initial_count,
                                 completion = std::move(completion)](
                                    const QString& import_path) mutable {
                                    auto* restored =
                                        favorites_tree_->topLevelItem(
                                            initial_count);
                                    completion(
                                        removed && import_path == path &&
                                        favorites_tree_->topLevelItemCount() ==
                                            initial_count + 1 &&
                                        restored != nullptr &&
                                        restored->text(0) ==
                                            QStringLiteral("Transfer Folder"));
                                },
                                Qt::SingleShotConnection);
                            emit ImportFavoritesRequested(path);
                        },
                        Qt::SingleShotConnection);
                    favorites_tree_->setCurrentItem(folder);
                    remove_favorite_action_->trigger();
                },
                Qt::SingleShotConnection);
            emit ExportFavoritesRequested(path);
        },
        Qt::SingleShotConnection);
    emit AddFavoriteFolderRequested(QStringLiteral("Transfer Folder"), {});
}

void MainWindow::RunDictionaryBrowserSmokeCheck(
    std::function<void(bool)> completion) {
    auto lookup_passed = std::make_shared<bool>(false);
    connect(
        this, &MainWindow::LookupSubmitted, this,
        [lookup_passed](const QString& submitted, std::uint32_t) {
            *lookup_passed = submitted == QStringLiteral("application");
        },
        Qt::SingleShotConnection);
    ShowDictionaryBrowser();
    dictionary_browser_->RunSmokeCheck(
        QStringLiteral("Fixture Dictionary"), QStringLiteral("app"),
        QStringLiteral("application"),
        [lookup_passed,
         completion = std::move(completion)](bool browser_passed) mutable {
            completion(browser_passed && *lookup_passed);
        });
}

void MainWindow::RunDictionaryBrowserExportSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    ShowDictionaryBrowser();
    dictionary_browser_->RunExportSmokeCheck(
        path, QStringLiteral("app"),
        QByteArray::fromHex("efbbbf") + "00databaseshort\napple\napplication\n",
        std::move(completion));
}

MainWindow::~MainWindow() {
    StopSuggestionWorker();
    for (auto& [id, request] : requests_) {
        static_cast<void>(id);
        request->Cancel();
    }
    QWebEngineProfile::defaultProfile()->removeUrlSchemeHandler(
        scheme_handler_);
}

goldendict::core::ArticleTabId MainWindow::TabIdAt(int index) const {
    if (index < 0 || index >= article_tabs_->count())
        return 0U;
    return article_tabs_->widget(index)->property("articleTabId").toULongLong();
}

ArticleView* MainWindow::ArticleViewForTab(
    goldendict::core::ArticleTabId tab_id) const {
    for (int index = 0; index < article_tabs_->count(); ++index) {
        if (TabIdAt(index) == tab_id) {
            return qobject_cast<ArticleView*>(article_tabs_->widget(index));
        }
    }
    return nullptr;
}

ArticleView* MainWindow::CreateArticleView(
    goldendict::core::ArticleTabId tab_id) {
    auto* view = new ArticleView(article_tabs_);
    view->SetFacade(facade_);
    view->setProperty("articleTabId", QVariant::fromValue<qulonglong>(tab_id));
    auto* page = new ArticlePage(view);
    page->SetFacade(facade_);
    page->SetOpenNewTabsInBackground(preferences_.open_new_tabs_in_background);
    view->setPage(page);
    connect(page, &ArticlePage::LookupRequested, this,
            [this, tab_id](const QString& text, const QString& internal_url,
                           ArticleLinkDisposition disposition) {
                static_cast<void>(text);
                OpenArticleLink(tab_id, QUrl(internal_url), disposition);
            });
    connect(page, &ArticlePage::ExternalUrlRequested, this,
            [](const QUrl& url) { QDesktopServices::openUrl(url); });
    connect(
        view, &ArticleView::LinkRequested, this,
        [this, tab_id](const QUrl& url, ArticleLinkDisposition disposition) {
            if (disposition == ArticleLinkDisposition::kNewForegroundTab &&
                preferences_.open_new_tabs_in_background) {
                disposition = ArticleLinkDisposition::kNewBackgroundTab;
            }
            OpenArticleLink(tab_id, url, disposition);
        });
    connect(view, &ArticleView::SelectionLookupRequested, this,
            [this, tab_id](const QString& text,
                           ArticleLinkDisposition disposition) {
                if (disposition == ArticleLinkDisposition::kNewForegroundTab &&
                    preferences_.open_new_tabs_in_background) {
                    disposition = ArticleLinkDisposition::kNewBackgroundTab;
                }
                LookupArticleSelection(tab_id, text, disposition);
            });
    connect(view, &ArticleView::SelectionToInputRequested, this,
            [this](const QString& text) { query_->setText(text); });
    connect(view, &ArticleView::ExternalUrlRequested, this,
            [](const QUrl& url) { QDesktopServices::openUrl(url); });
    connect(view, &QWebEngineView::urlChanged, this,
            &MainWindow::UpdateNavigationActions);
    connect(view, &QWebEngineView::pdfPrintingFinished, this,
            [this, tab_id](const QString&, bool success) {
                if (TabIdAt(article_tabs_->currentIndex()) == tab_id) {
                    status_->setText(success
                                         ? QStringLiteral("PDF saved")
                                         : QStringLiteral("PDF save failed"));
                }
            });
    connect(view, &QWebEngineView::printFinished, this, [this](bool success) {
        print_in_progress_ = false;
        UpdateFileActions();
        status_->setText(success ? QStringLiteral("Article printed")
                                 : QStringLiteral("Printing failed"));
    });
    view->setHtml(QStringLiteral(
        "<!doctype html><html><body><h1>GoldenDict</h1>"
        "<p>Choose a dictionary folder to begin.</p></body></html>"));
    return view;
}

void MainWindow::OpenArticleLink(goldendict::core::ArticleTabId tab_id,
                                 const QUrl& url,
                                 ArticleLinkDisposition disposition) {
    if (facade_ == nullptr)
        return;
    const auto resolved =
        facade_->ResolveArticleUrl(url.toEncoded().toStdString());
    if (!resolved.has_value() ||
        resolved->kind != goldendict::core::ArticleUrlKind::kLookup) {
        return;
    }
    const auto state = facade_->GetArticleTabsState();
    const auto found =
        std::find_if(state.tabs.begin(), state.tabs.end(),
                     [tab_id](const auto& tab) { return tab.id == tab_id; });
    if (found == state.tabs.end())
        return;
    goldendict::core::TabNavigationState navigation;
    navigation.kind = goldendict::core::TabNavigationKind::kInternalLink;
    navigation.query = resolved->lookup_text;
    navigation.group_id = found->navigation.group_id;
    navigation.title = navigation.query;
    navigation.internal_url = url.toEncoded().toStdString();
    const bool new_tab = disposition != ArticleLinkDisposition::kCurrentTab;
    const bool background =
        disposition == ArticleLinkDisposition::kNewBackgroundTab;
    const auto result = facade_->OpenArticleTab(
        navigation,
        new_tab ? goldendict::core::TabOpenPolicy::kNewTab
                : goldendict::core::TabOpenPolicy::kCurrentTab,
        background ? goldendict::core::TabActivationPolicy::kKeepActive
                   : goldendict::core::TabActivationPolicy::kActivate,
        NewTabPlacementPolicy());
    if (!result) {
        status_->setText(QStringLiteral("Unable to open article tab"));
        return;
    }
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    StartNavigationLookup(result.tab_id, navigation, true);
}

void MainWindow::LookupArticleSelection(goldendict::core::ArticleTabId tab_id,
                                        const QString& text,
                                        ArticleLinkDisposition disposition) {
    if (facade_ == nullptr || text.trimmed().isEmpty() ||
        text.trimmed().size() >= 60) {
        return;
    }
    const auto state = facade_->GetArticleTabsState();
    const auto found =
        std::find_if(state.tabs.begin(), state.tabs.end(),
                     [tab_id](const auto& tab) { return tab.id == tab_id; });
    if (found == state.tabs.end())
        return;
    goldendict::core::TabNavigationState navigation;
    navigation.kind = goldendict::core::TabNavigationKind::kLookup;
    navigation.query = text.toStdString();
    navigation.group_id = found->navigation.group_id;
    navigation.title = navigation.query;
    const bool new_tab = disposition != ArticleLinkDisposition::kCurrentTab;
    const bool background =
        disposition == ArticleLinkDisposition::kNewBackgroundTab;
    const auto result = facade_->OpenArticleTab(
        navigation,
        new_tab ? goldendict::core::TabOpenPolicy::kNewTab
                : goldendict::core::TabOpenPolicy::kCurrentTab,
        background ? goldendict::core::TabActivationPolicy::kKeepActive
                   : goldendict::core::TabActivationPolicy::kActivate,
        NewTabPlacementPolicy());
    if (!result)
        return;
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    StartNavigationLookup(result.tab_id, navigation, true);
}

void MainWindow::SyncArticleTabs() {
    if (facade_ == nullptr)
        return;
    const auto state = facade_->GetArticleTabsState();
    const QSignalBlocker blocker(article_tabs_);
    for (int index = article_tabs_->count() - 1; index >= 0; --index) {
        const auto id = TabIdAt(index);
        const bool retained =
            std::any_of(state.tabs.begin(), state.tabs.end(),
                        [id](const auto& tab) { return tab.id == id; });
        if (!retained) {
            if (auto request = requests_.find(id); request != requests_.end()) {
                request->second->Cancel();
                requests_.erase(request);
            }
            lookup_results_.erase(id);
            suggestions_.erase(id);
            QWidget* widget = article_tabs_->widget(index);
            article_tabs_->removeTab(index);
            widget->deleteLater();
        }
    }
    for (std::size_t desired = 0; desired < state.tabs.size(); ++desired) {
        const auto& tab = state.tabs[desired];
        auto* view = ArticleViewForTab(tab.id);
        if (view == nullptr) {
            view = CreateArticleView(tab.id);
            article_tabs_->insertTab(
                static_cast<int>(desired), view,
                QString::fromStdString(tab.navigation.title));
        }
        const int current = article_tabs_->indexOf(view);
        if (current != static_cast<int>(desired)) {
            article_tabs_->removeTab(current);
            article_tabs_->insertTab(
                static_cast<int>(desired), view,
                QString::fromStdString(tab.navigation.title));
        }
        article_tabs_->setTabText(
            static_cast<int>(desired),
            QString::fromStdString(tab.navigation.title).replace('&', "&&"));
    }
    const auto active = std::find_if(
        state.tabs.begin(), state.tabs.end(),
        [&](const auto& tab) { return tab.id == state.active_tab_id; });
    if (active == state.tabs.end())
        return;
    auto* active_view = ArticleViewForTab(active->id);
    article_tabs_->setCurrentWidget(active_view);
    article_view_ = active_view;
    article_page_ = qobject_cast<ArticlePage*>(active_view->page());
    query_->setText(QString::fromStdString(active->navigation.query));
    SelectGroup(active->navigation.group_id);
    setWindowTitle(
        active->navigation.kind == goldendict::core::TabNavigationKind::kEmpty
            ? QStringLiteral("GoldenDict")
            : QStringLiteral("%1 — GoldenDict")
                  .arg(QString::fromStdString(active->navigation.title)));
    UpdateNavigationActions();
    RefreshResultsNavigation();
    RefreshSuggestions();
}

void MainWindow::RebuildArticleTabs() {
    if (facade_ == nullptr)
        return;
    SyncArticleTabs();
    const auto state = facade_->GetArticleTabsState();
    for (const auto& tab : state.tabs) {
        if (tab.navigation.kind !=
            goldendict::core::TabNavigationKind::kEmpty) {
            StartNavigationLookup(tab.id, tab.navigation, false);
        }
    }
}

void MainWindow::ActivateArticleTab(int index) {
    if (facade_ == nullptr || index < 0)
        return;
    const auto id = TabIdAt(index);
    if (id != 0U && facade_->ActivateArticleTab(id)) {
        SyncArticleTabs();
        emit ArticleTabSessionMutated();
    }
}

void MainWindow::CloseArticleTab(int index) {
    if (facade_ == nullptr || index < 0)
        return;
    const auto id = TabIdAt(index);
    if (!facade_->CloseArticleTab(id)) {
        status_->setText(QStringLiteral("Unable to close article tab"));
        return;
    }
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
}

void MainWindow::CloseOtherArticleTabs(int index) {
    if (facade_ == nullptr || index < 0)
        return;
    if (facade_->CloseOtherArticleTabs(TabIdAt(index))) {
        SyncArticleTabs();
        emit ArticleTabSessionMutated();
    }
}

void MainWindow::CreateEmptyArticleTab(bool activate) {
    if (facade_ == nullptr)
        return;
    goldendict::core::TabNavigationState navigation;
    navigation.title = "(untitled)";
    const auto result = facade_->OpenArticleTab(
        navigation, goldendict::core::TabOpenPolicy::kNewTab,
        activate ? goldendict::core::TabActivationPolicy::kActivate
                 : goldendict::core::TabActivationPolicy::kKeepActive,
        NewTabPlacementPolicy());
    if (!result) {
        status_->setText(QStringLiteral("Article tab limit reached"));
        return;
    }
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
}

goldendict::core::TabPlacementPolicy MainWindow::NewTabPlacementPolicy() const {
    return preferences_.open_new_tabs_after_current
               ? goldendict::core::TabPlacementPolicy::kAfterActive
               : goldendict::core::TabPlacementPolicy::kAppend;
}

void MainWindow::NavigateArticleTab(bool forward) {
    if (facade_ == nullptr)
        return;
    const auto id = TabIdAt(article_tabs_->currentIndex());
    const auto result = forward ? facade_->GoForwardInArticleTab(id)
                                : facade_->GoBackInArticleTab(id);
    if (!result)
        return;
    const auto state = facade_->GetArticleTabsState();
    const auto tab =
        std::find_if(state.tabs.begin(), state.tabs.end(),
                     [id](const auto& item) { return item.id == id; });
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    if (tab != state.tabs.end() &&
        tab->navigation.kind != goldendict::core::TabNavigationKind::kEmpty) {
        StartNavigationLookup(id, tab->navigation, false);
    } else {
        if (auto request = requests_.find(id); request != requests_.end()) {
            request->second->Cancel();
            requests_.erase(request);
        }
        if (auto* view = ArticleViewForTab(id); view != nullptr) {
            view->setHtml(QStringLiteral(
                "<!doctype html><html><body><h1>GoldenDict</h1>"
                "<p>Choose a dictionary folder to begin.</p></body></html>"));
        }
    }
}

void MainWindow::ShowTabContextMenu(const QPoint& position) {
    const int index = article_tabs_->tabBar()->tabAt(position);
    if (index < 0)
        return;
    QMenu menu(this);
    auto* close = menu.addAction(QStringLiteral("Close Tab"));
    auto* close_others = menu.addAction(QStringLiteral("Close Other Tabs"));
    close_others->setEnabled(article_tabs_->count() > 1);
    QAction* selected =
        menu.exec(article_tabs_->tabBar()->mapToGlobal(position));
    if (selected == close)
        CloseArticleTab(index);
    if (selected == close_others)
        CloseOtherArticleTabs(index);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == query_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::MoveToNextLine) &&
            suggestions_list_->count() > 0) {
            suggestions_list_->setFocus(Qt::ShortcutFocusReason);
            suggestions_list_->setCurrentRow(0);
            return true;
        }
    }
    if (watched == suggestions_list_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::MoveToPreviousLine) &&
            suggestions_list_->currentRow() == 0) {
            suggestions_list_->clearSelection();
            query_->setFocus(Qt::ShortcutFocusReason);
            return true;
        }
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            suggestions_list_->currentItem() != nullptr) {
            ActivateSuggestion();
            return true;
        }
    }
    if (watched == article_tabs_->tabBar() &&
        event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::MiddleButton) {
            const int index =
                article_tabs_->tabBar()->tabAt(mouse->position().toPoint());
            if (index >= 0)
                CloseArticleTab(index);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::SetDictionaryGroups(
    const std::vector<goldendict::core::DictionaryGroupConfiguration>& groups) {
    groups_ = groups;
    RefreshGroupSelector();
    RefreshDictionaryBar();
}

void MainWindow::SetSourceDirectories(
    const std::vector<std::string>& dictionary_paths,
    const std::vector<goldendict::core::SoundDirectoryConfiguration>&
        sound_directories) {
    dictionary_paths_ = dictionary_paths;
    sound_directories_ = sound_directories;
}

void MainWindow::SetOnlineSources(
    const std::vector<goldendict::core::MediaWikiSourceConfiguration>&
        mediawiki_sources,
    const std::vector<goldendict::core::WebsiteSourceConfiguration>&
        website_sources,
    const std::vector<goldendict::core::ForvoSourceConfiguration>&
        forvo_sources,
    const std::vector<goldendict::core::DictServerSourceConfiguration>&
        dict_server_sources,
    const std::vector<goldendict::core::ExternalProgramSourceConfiguration>&
        external_program_sources,
    SourceApplyCallback apply_callback) {
    mediawiki_sources_ = mediawiki_sources;
    website_sources_ = website_sources;
    forvo_sources_ = forvo_sources;
    dict_server_sources_ = dict_server_sources;
    external_program_sources_ = external_program_sources;
    if (apply_callback)
        source_apply_callback_ = std::move(apply_callback);
}

void MainWindow::SetHistoryExportCallback(HistoryExportCallback callback) {
    history_export_callback_ = std::move(callback);
}

const std::vector<goldendict::core::DictionaryGroupConfiguration>&
MainWindow::DictionaryGroups() const noexcept {
    return groups_;
}

void MainWindow::SelectGroup(std::uint32_t group_id) {
    for (int index = 0; index < group_selector_->count(); ++index) {
        if (group_selector_->itemData(index).toUInt() == group_id) {
            group_selector_->setCurrentIndex(index);
            selected_group_id_ = group_id;
            return;
        }
    }
    group_selector_->setCurrentIndex(0);
    selected_group_id_ = 0U;
}

void MainWindow::RefreshGroupSelector() {
    const std::uint32_t previous = selected_group_id_;
    for (auto* shortcut : group_shortcuts_)
        delete shortcut;
    group_shortcuts_.clear();
    group_selector_->clear();
    group_selector_->addItem(QStringLiteral("All Dictionaries"),
                             QVariant::fromValue<quint32>(0U));
    for (const auto& group : groups_) {
        QIcon icon;
        if (!group.encoded_icon_data.empty()) {
            const QByteArray bytes = QByteArray::fromBase64(
                QByteArray::fromStdString(group.encoded_icon_data));
            QPixmap pixmap;
            if (pixmap.loadFromData(bytes))
                icon = QIcon(pixmap);
        }
        group_selector_->addItem(icon, QString::fromStdString(group.name),
                                 QVariant::fromValue<quint32>(group.id));
        const QKeySequence sequence(QString::fromStdString(group.shortcut));
        if (!sequence.isEmpty()) {
            auto* shortcut = new QShortcut(sequence, this);
            const std::uint32_t id = group.id;
            connect(shortcut, &QShortcut::activated, this,
                    [this, id]() { SelectGroup(id); });
            group_shortcuts_.push_back(shortcut);
        }
    }
    SelectGroup(previous);
}

std::vector<std::string> MainWindow::ParticipatingDictionaryIds(
    std::uint32_t group_id) const {
    const auto found = participating_ids_.find(group_id);
    return found == participating_ids_.end() ? std::vector<std::string>{}
                                             : found->second;
}

void MainWindow::RefreshDictionaryBar() {
    if (dictionary_bar_ == nullptr)
        return;
    const auto catalog =
        facade_ == nullptr ? std::vector<goldendict::core::DictionaryIdentity>{}
                           : facade_->GetDictionaryService().GetCatalog();
    std::map<std::string, goldendict::core::DictionaryIdentity> identities;
    for (const auto& dictionary : catalog)
        identities.emplace(dictionary.id, dictionary);

    std::vector<std::string> members;
    std::vector<std::string> baseline;
    if (selected_group_id_ == 0U) {
        for (const auto& dictionary : catalog) {
            members.push_back(dictionary.id);
            baseline.push_back(dictionary.id);
        }
    } else {
        const auto group = std::find_if(
            groups_.begin(), groups_.end(), [this](const auto& candidate) {
                return candidate.id == selected_group_id_;
            });
        if (group == groups_.end()) {
            SelectGroup(0U);
            return;
        }
        for (const auto& id : group->dictionary_ids) {
            if (identities.count(id) == 0U)
                continue;
            members.push_back(id);
            if (std::find(group->muted_dictionary_ids.begin(),
                          group->muted_dictionary_ids.end(),
                          id) == group->muted_dictionary_ids.end()) {
                baseline.push_back(id);
            }
        }
    }

    auto previous = participating_ids_.find(selected_group_id_);
    const auto previous_members = dictionary_members_.find(selected_group_id_);
    std::vector<std::string> reconciled;
    for (const auto& id : members) {
        const bool was_member = previous_members != dictionary_members_.end() &&
                                std::find(previous_members->second.begin(),
                                          previous_members->second.end(),
                                          id) != previous_members->second.end();
        const bool was_enabled =
            previous != participating_ids_.end() &&
            std::find(previous->second.begin(), previous->second.end(), id) !=
                previous->second.end();
        const bool baseline_enabled =
            std::find(baseline.begin(), baseline.end(), id) != baseline.end();
        if ((was_member && was_enabled) || (!was_member && baseline_enabled))
            reconciled.push_back(id);
    }
    participating_ids_[selected_group_id_] = reconciled;
    dictionary_members_[selected_group_id_] = members;

    dictionary_bar_->clear();
    for (const auto& id : members) {
        const auto& identity = identities.at(id);
        const QString label = QString::fromStdString(
            identity.name.empty() ? identity.id : identity.name);
        auto* action = dictionary_bar_->addAction(label);
        action->setCheckable(true);
        action->setChecked(std::find(reconciled.begin(), reconciled.end(),
                                     id) != reconciled.end());
        action->setData(QString::fromStdString(id));
        action->setToolTip(label);
        action->setWhatsThis(
            tr("Include %1 in dictionary lookups and suggestions").arg(label));
        if (auto* widget = dictionary_bar_->widgetForAction(action)) {
            widget->setAccessibleName(label);
            widget->setToolTip(label);
        }
        connect(
            action, &QAction::triggered, this,
            [this, action, id](bool checked) {
                auto& enabled = participating_ids_[selected_group_id_];
                const auto modifiers = QApplication::keyboardModifiers();
                if (modifiers.testFlag(Qt::ControlModifier) ||
                    modifiers.testFlag(Qt::ShiftModifier)) {
                    const bool was_solo =
                        enabled.size() == 1U && enabled.front() == id;
                    if (was_solo) {
                        if (modifiers.testFlag(Qt::ShiftModifier)) {
                            enabled = solo_restore_ids_[selected_group_id_];
                        } else {
                            enabled.clear();
                            for (auto* candidate : dictionary_bar_->actions())
                                enabled.push_back(
                                    candidate->data().toString().toStdString());
                        }
                        solo_restore_ids_.erase(selected_group_id_);
                    } else {
                        solo_restore_ids_[selected_group_id_] = enabled;
                        enabled = {id};
                    }
                } else {
                    solo_restore_ids_.erase(selected_group_id_);
                    const auto found =
                        std::find(enabled.begin(), enabled.end(), id);
                    if (checked && found == enabled.end())
                        enabled.push_back(id);
                    else if (!checked && found != enabled.end())
                        enabled.erase(found);
                }
                const auto ordered_actions = dictionary_bar_->actions();
                std::vector<std::string> ordered;
                for (auto* candidate : ordered_actions) {
                    const auto candidate_id =
                        candidate->data().toString().toStdString();
                    const bool participates =
                        std::find(enabled.begin(), enabled.end(),
                                  candidate_id) != enabled.end();
                    candidate->setChecked(participates);
                    if (participates)
                        ordered.push_back(candidate_id);
                }
                enabled = std::move(ordered);
                ApplyDictionaryParticipation();
            });
    }
}

void MainWindow::ApplyDictionaryFilter(
    goldendict::core::LookupQuery* query) const {
    if (dictionary_bar_ == nullptr || !dictionary_bar_->isVisible())
        return;
    query->dictionary_filter_active = true;
    query->dictionary_ids = ParticipatingDictionaryIds(query->group_id);
}

void MainWindow::ApplyDictionaryFilter(
    goldendict::core::SuggestionQuery* query) const {
    if (dictionary_bar_ == nullptr || !dictionary_bar_->isVisible())
        return;
    query->dictionary_filter_active = true;
    query->dictionary_ids = ParticipatingDictionaryIds(query->group_id);
}

void MainWindow::ApplyDictionaryParticipation() {
    if (restoring_main_window_state_ || facade_ == nullptr ||
        article_tabs_ == nullptr)
        return;
    StartSuggestionLookup();
    const auto active_id = TabIdAt(article_tabs_->currentIndex());
    const auto tabs = facade_->GetArticleTabsState();
    const auto active = std::find_if(
        tabs.tabs.begin(), tabs.tabs.end(),
        [active_id](const auto& tab) { return tab.id == active_id; });
    if (active != tabs.tabs.end() &&
        active->navigation.kind !=
            goldendict::core::TabNavigationKind::kEmpty &&
        !active->navigation.query.empty()) {
        StartNavigationLookup(active_id, active->navigation, false);
    }
}

void MainWindow::EditDictionaryGroups() {
    std::vector<goldendict::core::DictionaryIdentity> catalog;
    if (facade_ != nullptr) {
        catalog = facade_->GetDictionaryService().GetCatalog();
    }
    GroupEditor editor(groups_, std::move(catalog), this);
    if (editor.exec() != QDialog::Accepted)
        return;
    groups_ = editor.Groups();
    emit DictionaryGroupsEdited();
}

void MainWindow::RunDictionaryGroupsSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().empty()) {
        completion(false);
        return;
    }
    const auto catalog = facade_->GetDictionaryService().GetCatalog();
    const std::string first_id = catalog.front().id;
    GroupEditor editor({}, catalog, this);
    const bool edited = editor.RunSmokeEdits();
    groups_ = editor.Groups();
    emit DictionaryGroupsEdited();
    const bool saved = edited && groups_.size() == 1U &&
                       groups_.front().id == 7U &&
                       groups_.front().dictionary_ids.size() == 2U &&
                       groups_.front().muted_dictionary_ids ==
                           std::vector<std::string>{first_id} &&
                       groups_.front().popup_muted_dictionary_ids ==
                           std::vector<std::string>{"unavailable-id"} &&
                       group_selector_->count() == 2;
    SetHistoryItems({{QStringLiteral("missing"), 999U}});
    emit history_list_->itemActivated(history_list_->item(0));
    const bool fallback = selected_group_id_ == 0U;
    SetHistoryItems({{QStringLiteral("application"), 7U}});
    emit history_list_->itemActivated(history_list_->item(0));
    const bool restored = selected_group_id_ == 7U;
    auto lookup_group = std::make_shared<std::uint32_t>(0U);
    connect(
        this, &MainWindow::LookupSubmitted, this,
        [lookup_group](const QString&, std::uint32_t group_id) {
            *lookup_group = group_id;
        },
        Qt::SingleShotConnection);
    query_->setText(QStringLiteral("application"));
    StartLookup();
    completion(saved && restored && fallback && *lookup_group == 7U);
}

void MainWindow::RunDictionaryBarSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr || dictionary_bar_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().size() < 2U) {
        completion(false);
        return;
    }
    show();
    QApplication::processEvents();
    const auto catalog = facade_->GetDictionaryService().GetCatalog();
    const auto toolbar_actions = dictionary_bar_->actions();
    bool identities =
        toolbar_actions.size() == static_cast<qsizetype>(catalog.size());
    for (qsizetype index = 0; identities && index < toolbar_actions.size();
         ++index) {
        const auto& dictionary = catalog[static_cast<std::size_t>(index)];
        const QString label = QString::fromStdString(
            dictionary.name.empty() ? dictionary.id : dictionary.name);
        auto* widget = dictionary_bar_->widgetForAction(toolbar_actions[index]);
        identities = toolbar_actions[index]->isCheckable() &&
                     toolbar_actions[index]->isChecked() &&
                     toolbar_actions[index]->data().toString() ==
                         QString::fromStdString(dictionary.id) &&
                     toolbar_actions[index]->text() == label &&
                     toolbar_actions[index]->toolTip() == label &&
                     widget != nullptr && widget->accessibleName() == label;
    }
    const bool hierarchy =
        dictionary_bar_->objectName() == QStringLiteral("dictionaryBar") &&
        dictionary_bar_->toggleViewAction() != nullptr &&
        toolBarArea(dictionary_bar_) == Qt::TopToolBarArea &&
        dictionary_bar_->isVisible() && dictionary_bar_->isMovable() &&
        dictionary_bar_->isFloatable() &&
        dictionary_bar_->allowedAreas() == Qt::AllToolBarAreas;

    SetDictionaryGroups({{7U,
                          "Smoke Group",
                          "",
                          {catalog[1].id, catalog[0].id},
                          {catalog[0].id}}});
    SelectGroup(7U);
    RefreshDictionaryBar();
    const auto group_actions = dictionary_bar_->actions();
    const bool group_baseline = group_actions.size() == 2 &&
                                group_actions[0]->isChecked() &&
                                !group_actions[1]->isChecked() &&
                                group_actions[0]->data().toString() ==
                                    QString::fromStdString(catalog[1].id) &&
                                group_actions[1]->data().toString() ==
                                    QString::fromStdString(catalog[0].id);
    SelectGroup(0U);
    RefreshDictionaryBar();
    const auto all_actions = dictionary_bar_->actions();
    if (all_actions.empty()) {
        completion(false);
        return;
    }
    all_actions.front()->trigger();
    SelectGroup(7U);
    RefreshDictionaryBar();
    const bool group_isolation = dictionary_bar_->actions()[0]->isChecked() &&
                                 !dictionary_bar_->actions()[1]->isChecked();
    SelectGroup(0U);
    RefreshDictionaryBar();
    const bool all_scope_retained = !dictionary_bar_->actions()[0]->isChecked();
    for (auto* action : dictionary_bar_->actions()) {
        if (action->isChecked())
            action->trigger();
    }
    query_->setText(QStringLiteral("application"));
    StartLookup();
    auto poll = std::make_shared<std::function<void()>>();
    auto attempts = std::make_shared<int>(0);
    *poll = [this, identities, hierarchy, group_baseline, group_isolation,
             all_scope_retained, completion = std::move(completion), poll,
             attempts]() mutable {
        FinishLookup();
        if ((!requests_.empty() || ++*attempts < 5) && *attempts < 100) {
            QTimer::singleShot(10, this, *poll);
            return;
        }
        const bool all_off = requests_.empty() && results_list_->count() == 0 &&
                             suggestions_list_->count() == 0;
        dictionary_bar_->hide();
        StartLookup();
        auto hidden_poll = std::make_shared<std::function<void()>>();
        *hidden_poll = [this, identities, hierarchy, group_baseline,
                        group_isolation, all_scope_retained, all_off,
                        completion = std::move(completion),
                        hidden_poll]() mutable {
            FinishLookup();
            if (!requests_.empty()) {
                QTimer::singleShot(10, this, *hidden_poll);
                return;
            }
            const bool hidden_unfiltered = results_list_->count() > 0;
            dictionary_bar_->show();
            completion(identities && hierarchy && group_baseline &&
                       group_isolation && all_scope_retained && all_off &&
                       hidden_unfiltered);
        };
        QTimer::singleShot(10, this, *hidden_poll);
    };
    QTimer::singleShot(10, this, *poll);
}

void MainWindow::SetFacade(goldendict::core::DesktopFacade* facade) {
    StopSuggestionWorker();
    ++suggestion_generation_;
    suggestions_.clear();
    RefreshSuggestions();
    completion_timer_->stop();
    for (auto& [id, request] : requests_) {
        static_cast<void>(id);
        request->Cancel();
    }
    requests_.clear();
    lookup_results_.clear();
    RefreshResultsNavigation();
    while (article_tabs_->count() > 0) {
        QWidget* widget = article_tabs_->widget(0);
        article_tabs_->removeTab(0);
        delete widget;
    }
    article_view_ = nullptr;
    article_page_ = nullptr;
    facade_ = facade;
    RefreshDictionaryBar();
    scheme_handler_->SetFacade(facade);
    if (dictionary_browser_ != nullptr) {
        dictionary_browser_->SetFacade(facade);
    }
    const auto count = facade == nullptr
                           ? std::size_t{0}
                           : facade->GetDictionaryService().GetCatalog().size();
    status_->setText(
        tr("%1 dictionary loaded").arg(static_cast<qulonglong>(count)));
    suggestion_worker_ = std::make_unique<SuggestionWorker>(
        [this](goldendict::core::ArticleTabId tab_id, std::uint64_t generation,
               goldendict::core::SuggestionResponse response) {
            QMetaObject::invokeMethod(
                this, [this, tab_id, generation,
                       response = std::move(response)]() mutable {
                    FinishSuggestionLookup(tab_id, generation,
                                           std::move(response));
                });
        });
    if (facade_ != nullptr)
        RebuildArticleTabs();
    UpdateFileActions();
}

void MainWindow::SetPreferences(
    const goldendict::core::ApplicationPreferences& preferences) {
    preferences_ = preferences;
    for (int index = 0; index < article_tabs_->count(); ++index) {
        auto* view =
            qobject_cast<QWebEngineView*>(article_tabs_->widget(index));
        if (view != nullptr) {
            auto* page = qobject_cast<ArticlePage*>(view->page());
            if (page != nullptr) {
                page->SetOpenNewTabsInBackground(
                    preferences_.open_new_tabs_in_background);
            }
        }
    }
}

bool MainWindow::RestoreMainWindowGeometry(const std::string& geometry) {
    if (geometry.empty())
        return false;
    const QRect default_geometry = this->geometry();
    const Qt::WindowStates default_state = windowState();
    const QByteArray encoded(geometry.data(),
                             static_cast<qsizetype>(geometry.size()));
    if (restoreGeometry(encoded))
        return true;
    setWindowState(default_state);
    setGeometry(default_geometry);
    return false;
}

std::string MainWindow::CaptureMainWindowGeometry() const {
    const QByteArray geometry = saveGeometry();
    return {geometry.constData(), static_cast<std::size_t>(geometry.size())};
}

bool MainWindow::RestoreMainWindowState(const std::string& state) {
    if (state.empty() ||
        state.size() > static_cast<std::size_t>(kMaximumMainWindowStateBytes)) {
        return false;
    }
    const QScopedValueRollback restoring(restoring_main_window_state_, true);
    const QByteArray previous = saveState(kMainWindowStateVersion);
    const QByteArray encoded(state.data(),
                             static_cast<qsizetype>(state.size()));
    const auto place_missing_results_pane = [this]() {
        auto* results =
            findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
        auto* favorites =
            findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
        if (results == nullptr || favorites == nullptr)
            return;
        removeDockWidget(results);
        addDockWidget(Qt::RightDockWidgetArea, results);
        splitDockWidget(results, favorites, Qt::Vertical);
        results->show();
    };
    const auto place_missing_search_pane = [this]() {
        auto* search =
            findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
        if (search == nullptr)
            return;
        removeDockWidget(search);
        addDockWidget(Qt::LeftDockWidgetArea, search);
        search->show();
    };
    const auto place_missing_dictionary_bar = [this]() {
        if (dictionary_bar_ == nullptr)
            return;
        removeToolBar(dictionary_bar_);
        addToolBar(Qt::TopToolBarArea, dictionary_bar_);
        dictionary_bar_->show();
    };
    if (restoreState(encoded, kMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    if (restoreState(encoded, kPreviousMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    place_missing_results_pane();
    auto* nav_toolbar = findChild<QToolBar*>(QStringLiteral("navToolbar"));
    if (nav_toolbar != nullptr)
        addToolBar(Qt::TopToolBarArea, nav_toolbar);
    if (restoreState(encoded, kOlderMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    place_missing_results_pane();
    if (nav_toolbar != nullptr)
        addToolBar(Qt::TopToolBarArea, nav_toolbar);
    if (restoreState(encoded, kOldestMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    place_missing_results_pane();
    if (nav_toolbar != nullptr)
        addToolBar(Qt::TopToolBarArea, nav_toolbar);
    if (restoreState(encoded, kEarlierMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    place_missing_results_pane();
    auto* history =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* favorites =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    if (history != nullptr && favorites != nullptr) {
        if (nav_toolbar != nullptr)
            addToolBar(Qt::TopToolBarArea, nav_toolbar);
        history->setObjectName(QString::fromLatin1(kPreviousHistoryDockName));
        favorites->setObjectName(
            QString::fromLatin1(kPreviousFavoritesDockName));
        const bool restored_previous =
            restoreState(encoded, kEarliestMainWindowStateVersion);
        history->setObjectName(QString::fromLatin1(kHistoryPaneName));
        favorites->setObjectName(QString::fromLatin1(kFavoritesPaneName));
        const bool previous_usable = HasUsableMainWindowLayout();
        if (restored_previous && previous_usable)
            return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    return false;
}

std::string MainWindow::CaptureMainWindowState() const {
    const QByteArray state = saveState(kMainWindowStateVersion);
    return {state.constData(), static_cast<std::size_t>(state.size())};
}

void MainWindow::RefreshResultsNavigation() {
    if (results_list_ == nullptr)
        return;
    const QSignalBlocker blocker(results_list_);
    results_list_->clear();
    if (article_tabs_ == nullptr)
        return;
    const auto id = TabIdAt(article_tabs_->currentIndex());
    const auto found = lookup_results_.find(id);
    if (found == lookup_results_.end())
        return;
    for (const auto& dictionary : found->second) {
        bool already_present = false;
        for (int row = 0; row < results_list_->count(); ++row) {
            if (results_list_->item(row)->data(Qt::UserRole).toString() ==
                QString::fromStdString(dictionary.id)) {
                already_present = true;
                break;
            }
        }
        if (already_present)
            continue;
        auto* item = new QListWidgetItem(
            QString::fromStdString(dictionary.name.empty() ? dictionary.id
                                                           : dictionary.name),
            results_list_);
        item->setData(Qt::UserRole, QString::fromStdString(dictionary.id));
        item->setToolTip(item->text());
    }
    if (results_list_->count() > 0)
        results_list_->setCurrentRow(0);
}

void MainWindow::RefreshSuggestions() {
    if (suggestions_list_ == nullptr)
        return;
    const QSignalBlocker blocker(suggestions_list_);
    suggestions_list_->clear();
    if (article_tabs_ == nullptr)
        return;
    const auto found =
        suggestions_.find(TabIdAt(article_tabs_->currentIndex()));
    if (found == suggestions_.end())
        return;
    for (const auto& suggestion : found->second.rows) {
        auto* item = new QListWidgetItem(
            QString::fromStdString(suggestion.headword), suggestions_list_);
        item->setToolTip(item->text());
        item->setData(Qt::UserRole,
                      QString::fromStdString(suggestion.dictionary.id));
        item->setTextAlignment(Qt::AlignLeft);
    }
    if (suggestions_list_->count() > 0)
        suggestions_list_->scrollToTop();
}

void MainWindow::StartSuggestionLookup() {
    if (suggestions_list_ == nullptr || article_tabs_ == nullptr)
        return;
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    if (tab_id == 0U)
        return;
    const QString text = query_->text().trimmed();
    const std::uint64_t generation = ++suggestion_generation_;
    auto& presentation = suggestions_[tab_id];
    presentation.query = text;
    presentation.group_id = selected_group_id_;
    presentation.generation = generation;
    presentation.rows.clear();
    RefreshSuggestions();
    if (text.isEmpty() || facade_ == nullptr || suggestion_worker_ == nullptr) {
        if (text.isEmpty()) {
            if (suggestion_worker_ != nullptr)
                suggestion_worker_->Cancel();
            suggestions_.erase(tab_id);
        }
        return;
    }
    goldendict::core::SuggestionQuery query;
    query.text = text.toStdString();
    query.group_id = selected_group_id_;
    ApplyDictionaryFilter(&query);
    suggestion_worker_->Submit(&facade_->GetDictionaryService(),
                               std::move(query), tab_id, generation);
}

void MainWindow::FinishSuggestionLookup(
    goldendict::core::ArticleTabId tab_id, std::uint64_t generation,
    goldendict::core::SuggestionResponse response) {
    const auto found = suggestions_.find(tab_id);
    if (found == suggestions_.end() || found->second.generation != generation) {
        return;
    }
    found->second.rows.clear();
    if (response.errors.empty() || response.partial)
        found->second.rows = std::move(response.suggestions);
    const bool active = TabIdAt(article_tabs_->currentIndex()) == tab_id;
    if (active) {
        RefreshSuggestions();
        if (!response.errors.empty() && !response.partial)
            status_->setText(QStringLiteral("Suggestion lookup failed"));
    }
}

void MainWindow::ActivateSuggestion() {
    if (suggestions_list_ == nullptr ||
        suggestions_list_->currentItem() == nullptr)
        return;
    const QString word = suggestions_list_->currentItem()->text();
    {
        const QSignalBlocker blocker(query_);
        query_->setText(word);
    }
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    suggestions_.erase(tab_id);
    ++suggestion_generation_;
    if (suggestion_worker_ != nullptr)
        suggestion_worker_->Cancel();
    RefreshSuggestions();
    StartLookup();
    if (article_view_ != nullptr)
        article_view_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::StopSuggestionWorker() {
    if (suggestion_worker_ != nullptr) {
        suggestion_worker_->Stop();
        suggestion_worker_.reset();
    }
}

void MainWindow::NavigateToSelectedResult() {
    if (article_view_ == nullptr || results_list_ == nullptr ||
        results_list_->currentRow() < 0) {
        return;
    }
    const int row = results_list_->currentRow();
    article_view_->page()->runJavaScript(
        QStringLiteral(
            "const entries=document.querySelectorAll('.gd-dictionary-result');"
            "if(entries[%1]) entries[%1].scrollIntoView(true);")
            .arg(row));
    article_view_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::ApplyDefaultPaneLayout() {
    auto* history =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* favorites =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    auto* results =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    auto* search =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    if (history == nullptr || favorites == nullptr || results == nullptr ||
        search == nullptr)
        return;
    removeDockWidget(history);
    removeDockWidget(favorites);
    removeDockWidget(results);
    removeDockWidget(search);
    addDockWidget(Qt::LeftDockWidgetArea, search);
    addDockWidget(Qt::RightDockWidgetArea, results);
    addDockWidget(Qt::RightDockWidgetArea, favorites);
    addDockWidget(Qt::RightDockWidgetArea, history);
    splitDockWidget(results, favorites, Qt::Vertical);
    splitDockWidget(favorites, history, Qt::Vertical);
    results->show();
    favorites->show();
    history->show();
    search->show();
}

bool MainWindow::HasUsableMainWindowLayout() const {
    const auto* history =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    const auto* favorites =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    const auto* results =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    const auto* search =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    const auto* article_toolbar =
        findChild<QToolBar*>(QStringLiteral("articleToolbar"));
    const auto* nav_toolbar =
        findChild<QToolBar*>(QStringLiteral("navToolbar"));
    if (history == nullptr || favorites == nullptr || results == nullptr ||
        search == nullptr || article_toolbar == nullptr ||
        nav_toolbar == nullptr || centralWidget() == nullptr ||
        article_tabs_ == nullptr || article_tabs_->isHidden()) {
        return false;
    }
    const auto intersects_screen = [](const QWidget* widget) {
        if (!widget->isWindow())
            return true;
        const QRect geometry = widget->frameGeometry();
        for (const QScreen* screen : QApplication::screens()) {
            const QRect visible =
                geometry.intersected(screen->availableGeometry());
            if (visible.width() >= 32 && visible.height() >= 32)
                return true;
        }
        return false;
    };
    return intersects_screen(history) && intersects_screen(favorites) &&
           intersects_screen(results) && intersects_screen(search) &&
           intersects_screen(article_toolbar) && intersects_screen(nav_toolbar);
}

void MainWindow::ShowDictionaryBrowser() {
    if (dictionary_browser_ == nullptr) {
        dictionary_browser_ = new DictionaryBrowser(this);
        dictionary_browser_->SetFacade(facade_);
        connect(dictionary_browser_, &DictionaryBrowser::HeadwordSelected, this,
                [this](const QString& word) {
                    query_->setText(word);
                    StartLookup();
                });
    }
    dictionary_browser_->show();
    dictionary_browser_->raise();
    dictionary_browser_->activateWindow();
}

void MainWindow::SetHistoryWords(const QStringList& words) {
    history_items_.clear();
    history_items_.reserve(static_cast<std::size_t>(words.size()));
    for (const auto& word : words) {
        history_items_.push_back({word, 0U});
    }
    RefreshHistoryList();
    UpdateHistoryActions();
}

void MainWindow::SetHistoryItems(const std::vector<HistoryViewItem>& items) {
    history_items_ = items;
    RefreshHistoryList();
    UpdateHistoryActions();
}

void MainWindow::RefreshHistoryList() {
    history_list_->clear();
    const QString filter = history_filter_->text().trimmed();
    for (const auto& entry : history_items_) {
        if (filter.isEmpty() ||
            entry.word.contains(filter, Qt::CaseInsensitive)) {
            auto* item = new QListWidgetItem(entry.word, history_list_);
            item->setData(Qt::UserRole,
                          QVariant::fromValue<quint32>(entry.group_id));
        }
    }
}

void MainWindow::UpdateHistoryActions() {
    const bool idle = !history_command_busy_;
    const bool has_history = !history_items_.empty();
    import_history_action_->setEnabled(idle);
    export_history_action_->setEnabled(idle && has_history);
    clear_history_action_->setEnabled(idle && has_history);
    import_history_button_->setEnabled(import_history_action_->isEnabled());
    export_history_button_->setEnabled(export_history_action_->isEnabled());
    clear_history_button_->setEnabled(clear_history_action_->isEnabled());
}

void MainWindow::ExportHistory() {
    if (history_command_busy_) {
        return;
    }
    const QString path =
        history_export_path_provider_
            ? history_export_path_provider_()
            : QFileDialog::getSaveFileName(
                  this, QStringLiteral("Export history to file"), QString(),
                  QStringLiteral("Text files (*.txt);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    history_command_busy_ = true;
    UpdateHistoryActions();
    const QString error = history_export_callback_
                              ? history_export_callback_(path)
                              : QStringLiteral("History export is unavailable");
    history_command_busy_ = false;
    UpdateHistoryActions();
    status_->setText(error.isEmpty() ? QStringLiteral("History export complete")
                                     : QStringLiteral("History export failed"));
}

void MainWindow::ImportHistory() {
    if (history_command_busy_) {
        return;
    }
    const QString path =
        history_import_path_provider_
            ? history_import_path_provider_()
            : QFileDialog::getOpenFileName(
                  this, QStringLiteral("Import history from file"), QString(),
                  QStringLiteral("Text files (*.txt);;All files (*.*)"));
    if (!path.isEmpty()) {
        history_command_busy_ = true;
        UpdateHistoryActions();
        emit ImportHistoryRequested(path, selected_group_id_);
        history_command_busy_ = false;
        UpdateHistoryActions();
    }
}

void MainWindow::ClearHistory() {
    if (history_command_busy_ || history_items_.empty()) {
        return;
    }
    history_command_busy_ = true;
    UpdateHistoryActions();
    emit ClearHistoryRequested();
    history_command_busy_ = false;
    UpdateHistoryActions();
}

void MainWindow::CreateFavoriteFolder() {
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("New favorite folder"),
                              QStringLiteral("Folder name:"), QLineEdit::Normal,
                              QString(), &accepted)
            .trimmed();
    if (accepted && !name.isEmpty()) {
        emit AddFavoriteFolderRequested(name, SelectedFavoriteFolderPath());
    }
}

void MainWindow::RenameFavorite() {
    const auto* item = favorites_tree_->currentItem();
    if (item == nullptr) {
        return;
    }
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("Rename favorite"),
                              QStringLiteral("Name:"), QLineEdit::Normal,
                              item->text(0), &accepted)
            .trimmed();
    if (accepted && !name.isEmpty() && name != item->text(0)) {
        emit RenameFavoriteRequested(
            item->data(0, Qt::UserRole + 1).value<QList<int>>(), name);
    }
}

void MainWindow::ImportFavorites() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import favorites from file"), QString(),
        QStringLiteral("GoldenDict favorites (*.xml);;All files (*.*)"));
    if (!path.isEmpty()) {
        emit ImportFavoritesRequested(path);
    }
}

void MainWindow::ExportFavorites() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export favorites to file"), QString(),
        QStringLiteral("GoldenDict favorites (*.xml);;All files (*.*)"));
    if (!path.isEmpty()) {
        emit ExportFavoritesRequested(path);
    }
}

QList<int> MainWindow::SelectedFavoriteFolderPath() const {
    const auto* item = favorites_tree_->currentItem();
    if (item == nullptr) {
        return {};
    }
    if (!item->data(0, Qt::UserRole).toBool()) {
        item = item->parent();
    }
    return item == nullptr
               ? QList<int>{}
               : item->data(0, Qt::UserRole + 1).value<QList<int>>();
}

QList<QList<int>> MainWindow::ExpandedFavoriteFolderPaths() const {
    QList<QList<int>> paths;
    const auto collect = [&](const auto& self,
                             const QTreeWidgetItem* item) -> void {
        if (item->data(0, Qt::UserRole).toBool() && item->isExpanded()) {
            paths.push_back(
                item->data(0, Qt::UserRole + 1).value<QList<int>>());
        }
        for (int index = 0; index < item->childCount(); ++index) {
            self(self, item->child(index));
        }
    };
    for (int index = 0; index < favorites_tree_->topLevelItemCount(); ++index) {
        collect(collect, favorites_tree_->topLevelItem(index));
    }
    return paths;
}

void MainWindow::SetFavoriteItems(const std::vector<FavoriteViewItem>& items,
                                  const QList<int>& current_path) {
    const bool restore_focus = favorites_tree_->hasFocus();
    QTreeWidgetItem* restored_current = nullptr;
    const auto append = [&](const auto& self, QTreeWidgetItem* parent,
                            const FavoriteViewItem& item) -> void {
        auto* tree_item = parent == nullptr
                              ? new QTreeWidgetItem(favorites_tree_)
                              : new QTreeWidgetItem(parent);
        tree_item->setText(0, item.text);
        tree_item->setData(0, Qt::UserRole, item.folder);
        tree_item->setData(0, Qt::UserRole + 1, QVariant::fromValue(item.path));
        if (item.path == current_path) {
            restored_current = tree_item;
        }
        for (const auto& child : item.children) {
            self(self, tree_item, child);
        }
        tree_item->setExpanded(item.expanded);
    };
    favorites_tree_->clear();
    for (const auto& item : items) {
        append(append, nullptr, item);
    }
    if (restored_current != nullptr) {
        favorites_tree_->setCurrentItem(restored_current);
        restored_current->setSelected(true);
        favorites_tree_->scrollToItem(restored_current);
    }
    if (restore_focus) {
        favorites_tree_->setFocus();
    }
}

void MainWindow::RunWebEngineSmokeCheck(std::function<void(bool)> completion) {
    connect(
        article_view_, &QWebEngineView::loadFinished, this,
        [this, completion = std::move(completion)](bool loaded) mutable {
            if (!loaded) {
                completion(false);
                return;
            }
            article_view_->page()->toPlainText(
                [completion = std::move(completion)](const QString& text) {
                    completion(
                        text.contains(QStringLiteral("WebEngine smoke ready")));
                });
        },
        Qt::SingleShotConnection);
    article_view_->setHtml(
        QStringLiteral("<!doctype html><html><body><p>WebEngine smoke ready</p>"
                       "</body></html>"));
}

void MainWindow::RunSourceDirectoriesSmokeCheck(
    std::function<void(bool)> completion) {
    const std::vector<std::string> paths = {"/one", "/two", "/three"};
    const std::vector<goldendict::core::SoundDirectoryConfiguration> sounds = {
        {"/sound-one", "One"}, {"/sound-two", "Two"}};
    SourceDirectoriesDialog dialog(paths, sounds, this);
    auto* path_list =
        dialog.findChild<QListWidget*>(QStringLiteral("dictionaryPathList"));
    auto* path_up =
        dialog.findChild<QPushButton*>(QStringLiteral("moveDictionaryPathUp"));
    auto* path_remove =
        dialog.findChild<QPushButton*>(QStringLiteral("removeDictionaryPath"));
    auto* sound_list =
        dialog.findChild<QTreeWidget*>(QStringLiteral("soundDirectoryList"));
    auto* sound_up =
        dialog.findChild<QPushButton*>(QStringLiteral("moveSoundDirectoryUp"));
    bool passed = path_list != nullptr && path_up != nullptr &&
                  path_remove != nullptr && sound_list != nullptr &&
                  sound_up != nullptr;
    if (passed) {
        path_list->setCurrentRow(2);
        path_up->click();
        path_list->setCurrentRow(0);
        path_remove->click();
        sound_list->setCurrentItem(sound_list->topLevelItem(1));
        sound_up->click();
        sound_list->topLevelItem(0)->setText(1, QStringLiteral("Edited"));
        passed =
            dialog.DictionaryPaths() ==
                (std::vector<std::string>{"/three", "/two"}) &&
            dialog.SoundDirectories() ==
                (std::vector<goldendict::core::SoundDirectoryConfiguration>{
                    {"/sound-two", "Edited"}, {"/sound-one", "One"}});
    }
    SourceDirectoriesDialog cancelled(paths, sounds, this);
    cancelled.reject();
    passed = passed && cancelled.result() == QDialog::Rejected &&
             cancelled.DictionaryPaths() == paths &&
             cancelled.SoundDirectories() == sounds;
    const std::vector<goldendict::core::MediaWikiSourceConfiguration> wikis = {
        {"wiki.one", "One", true, "https://one.example/w"},
        {"wiki.two", "Two", false, "https://two.example/w"}};
    const std::vector<goldendict::core::WebsiteSourceConfiguration> websites = {
        {"web.one", "Website", true, "https://website.example/?q=%GDWORD%"}};
    const std::vector<goldendict::core::ForvoSourceConfiguration> forvo = {
        {"forvo.one",
         "Forvo",
         false,
         "https://apifree.forvo.com",
         {"en", "ru"}}};
    const std::vector<goldendict::core::DictServerSourceConfiguration> dicts = {
        {"dict.one", "DICT", true, "dict.example", 2628U, "*", "prefix"}};
    const std::vector<goldendict::core::ExternalProgramSourceConfiguration>
        programs = {{"program.one",
                     "Plain",
                     false,
                     goldendict::core::ExternalProgramOutputKind::kPlainText,
                     "/bin/echo",
                     {},
                     ""},
                    {"program.two",
                     "Prefix",
                     true,
                     goldendict::core::ExternalProgramOutputKind::kPrefixMatch,
                     "/usr/bin/printf",
                     {"%GDWORD%"},
                     "/tmp"}};
    bool reject_once = true;
    bool callback_received = false;
    SourceDirectoriesDialog online(
        paths, sounds, wikis, websites, forvo, dicts, programs,
        [&](const auto&, const auto&, const auto& edited_wikis,
            const auto& edited_websites, const auto& edited_forvo,
            const auto& edited_dicts, const auto& edited_programs) {
            callback_received =
                edited_wikis.front().id == "wiki.two" &&
                edited_wikis.front().enabled && edited_websites == websites &&
                edited_forvo.front().language_codes ==
                    (std::vector<std::string>{"ru", "en"}) &&
                edited_dicts == dicts &&
                edited_programs.front().id == "program.two" &&
                edited_programs.front().enabled &&
                edited_programs.front().output_kind ==
                    goldendict::core::ExternalProgramOutputKind::kHtml &&
                edited_programs.front().executable == "/usr/bin/printf" &&
                edited_programs.front().working_directory.empty() &&
                edited_programs.front().argument_templates ==
                    (std::vector<std::string>{"", "%GDWORD%"}) &&
                edited_programs[1].id == "program.one" &&
                edited_programs[1].argument_templates.empty();
            if (reject_once) {
                reject_once = false;
                return QStringLiteral("forced apply failure");
            }
            return QString{};
        },
        this);
    auto* wiki_list =
        online.findChild<QTreeWidget*>(QStringLiteral("mediaWikiList"));
    auto* wiki_up =
        online.findChild<QPushButton*>(QStringLiteral("mediaWikiUp"));
    auto* forvo_list =
        online.findChild<QTreeWidget*>(QStringLiteral("forvoList"));
    auto* buttons = online.findChild<QDialogButtonBox*>();
    auto* program_list =
        online.findChild<QTreeWidget*>(QStringLiteral("externalProgramList"));
    auto* program_up =
        online.findChild<QPushButton*>(QStringLiteral("externalProgramUp"));
    auto* result_kind = online.findChild<QComboBox*>(
        QStringLiteral("externalProgramResultKind"));
    auto* executable = online.findChild<QLineEdit*>(
        QStringLiteral("externalProgramExecutable"));
    auto* working_directory = online.findChild<QLineEdit*>(
        QStringLiteral("externalProgramWorkingDirectory"));
    auto* clear_working = online.findChild<QPushButton*>(
        QStringLiteral("externalProgramClearWorkingDirectory"));
    auto* arguments = online.findChild<QListWidget*>(
        QStringLiteral("externalProgramArgumentList"));
    auto* argument_add =
        online.findChild<QPushButton*>(QStringLiteral("externalArgumentAdd"));
    auto* argument_up =
        online.findChild<QPushButton*>(QStringLiteral("externalArgumentUp"));
    passed = passed && wiki_list != nullptr && wiki_up != nullptr &&
             forvo_list != nullptr && buttons != nullptr &&
             program_list != nullptr && program_up != nullptr &&
             result_kind != nullptr && executable != nullptr &&
             working_directory != nullptr && clear_working != nullptr &&
             arguments != nullptr && argument_add != nullptr &&
             argument_up != nullptr;
    if (passed) {
        auto* website_list =
            online.findChild<QTreeWidget*>(QStringLiteral("websiteList"));
        website_list->topLevelItem(0)->setText(2, QStringLiteral("invalid"));
        buttons->button(QDialogButtonBox::Apply)->click();
        auto* error =
            online.findChild<QLabel*>(QStringLiteral("sourceValidationError"));
        passed = !callback_received && online.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        website_list->topLevelItem(0)->setText(
            2, QStringLiteral("https://website.example/?q=%GDWORD%"));
        wiki_list->setCurrentItem(wiki_list->topLevelItem(1));
        wiki_up->click();
        wiki_list->topLevelItem(0)->setCheckState(0, Qt::Checked);
        forvo_list->topLevelItem(0)->setText(3, QStringLiteral("ru,en"));
        program_list->setCurrentItem(program_list->topLevelItem(1));
        program_up->click();
        result_kind->setCurrentIndex(static_cast<int>(
            goldendict::core::ExternalProgramOutputKind::kHtml));
        clear_working->click();
        argument_add->click();
        arguments->currentItem()->setText(QString{});
        argument_up->click();
        executable->setText(QStringLiteral("relative"));
        buttons->button(QDialogButtonBox::Apply)->click();
        passed = passed && !callback_received &&
                 online.result() != QDialog::Accepted && error != nullptr &&
                 !error->isHidden();
        executable->setText(QStringLiteral("/usr/bin/printf"));
        buttons->button(QDialogButtonBox::Apply)->click();
        passed = passed && callback_received &&
                 online.result() != QDialog::Accepted && error != nullptr &&
                 !error->isHidden();
        buttons->button(QDialogButtonBox::Apply)->click();
        passed = passed && online.result() == QDialog::Accepted;
    }
    bool received_empty = false;
    SourceDirectoriesDialog empty_online(
        paths, sounds, {}, {}, {}, {}, {},
        [&](const auto&, const auto&, const auto& empty_wikis,
            const auto& empty_websites, const auto& empty_forvo,
            const auto& empty_dicts, const auto& empty_programs) {
            received_empty = empty_wikis.empty() && empty_websites.empty() &&
                             empty_forvo.empty() && empty_dicts.empty() &&
                             empty_programs.empty();
            return QString{};
        },
        this);
    empty_online.findChild<QDialogButtonBox*>()
        ->button(QDialogButtonBox::Apply)
        ->click();
    passed =
        passed && received_empty && empty_online.result() == QDialog::Accepted;
    completion(passed);
}

void MainWindow::EditSourceDirectories() {
    SourceDirectoriesDialog dialog(
        dictionary_paths_, sound_directories_, mediawiki_sources_,
        website_sources_, forvo_sources_, dict_server_sources_,
        external_program_sources_, source_apply_callback_, this);
    dialog.exec();
}

void MainWindow::StartLookup() {
    StartLookupInTab(goldendict::core::TabOpenPolicy::kCurrentTab,
                     goldendict::core::TabActivationPolicy::kActivate);
}

void MainWindow::StartLookupInTab(
    goldendict::core::TabOpenPolicy open_policy,
    goldendict::core::TabActivationPolicy activation,
    const QString& internal_url) {
    if (facade_ == nullptr || query_->text().trimmed().isEmpty()) {
        return;
    }
    const QString word = query_->text().trimmed();
    goldendict::core::TabNavigationState navigation;
    navigation.kind = internal_url.isEmpty()
                          ? goldendict::core::TabNavigationKind::kLookup
                          : goldendict::core::TabNavigationKind::kInternalLink;
    navigation.query = word.toStdString();
    navigation.group_id = selected_group_id_;
    navigation.title = navigation.query;
    navigation.internal_url = internal_url.toStdString();
    const auto tab_result = facade_->OpenArticleTab(
        navigation, open_policy, activation, NewTabPlacementPolicy());
    if (!tab_result) {
        status_->setText(QStringLiteral("Unable to update article state"));
        return;
    }
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    StartNavigationLookup(tab_result.tab_id, navigation, true);
}

void MainWindow::StartNavigationLookup(
    goldendict::core::ArticleTabId tab_id,
    const goldendict::core::TabNavigationState& navigation,
    bool record_history) {
    if (facade_ == nullptr || navigation.query.empty())
        return;
    suggestions_.erase(tab_id);
    ++suggestion_generation_;
    if (suggestion_worker_ != nullptr)
        suggestion_worker_->Cancel();
    if (TabIdAt(article_tabs_->currentIndex()) == tab_id)
        RefreshSuggestions();
    if (auto existing = requests_.find(tab_id); existing != requests_.end()) {
        existing->second->Cancel();
        requests_.erase(existing);
    }
    lookup_results_.erase(tab_id);
    if (TabIdAt(article_tabs_->currentIndex()) == tab_id)
        RefreshResultsNavigation();
    goldendict::core::LookupQuery query;
    query.text = navigation.query;
    query.group_id = navigation.group_id;
    ApplyDictionaryFilter(&query);
    if (record_history) {
        emit LookupSubmitted(QString::fromStdString(navigation.query),
                             navigation.group_id);
    }
    requests_[tab_id] =
        facade_->GetDictionaryService().StartLookup(std::move(query));
    if (TabIdAt(article_tabs_->currentIndex()) == tab_id) {
        status_->setText(QStringLiteral("Looking up..."));
        lookup_button_->setEnabled(false);
    }
    completion_timer_->start();
}

void MainWindow::FinishLookup() {
    std::vector<goldendict::core::ArticleTabId> finished;
    for (const auto& [id, request] : requests_) {
        if (request->IsFinished())
            finished.push_back(id);
    }
    for (const auto id : finished) {
        auto request = std::move(requests_.at(id));
        requests_.erase(id);
        auto* view = ArticleViewForTab(id);
        if (view == nullptr)
            continue;
        const bool active = TabIdAt(article_tabs_->currentIndex()) == id;
        QString navigation_title;
        const auto tabs_state = facade_->GetArticleTabsState();
        const auto tab_state =
            std::find_if(tabs_state.tabs.begin(), tabs_state.tabs.end(),
                         [id](const auto& tab) { return tab.id == id; });
        if (tab_state != tabs_state.tabs.end()) {
            navigation_title =
                QString::fromStdString(tab_state->navigation.title);
        }
        try {
            const auto response = request->Await();
            if (!response.entries.empty()) {
                auto& results = lookup_results_[id];
                results.reserve(response.entries.size());
                for (const auto& entry : response.entries)
                    results.push_back(entry.dictionary);
                const auto article = facade_->ComposeLookupPage(response);
                if (article.sanitized_html.has_value()) {
                    view->setHtml(
                        QString::fromUtf8(article.sanitized_html->data(),
                                          static_cast<qsizetype>(
                                              article.sanitized_html->size())));
                } else {
                    view->setHtml(QStringLiteral("<!doctype "
                                                 "html><html><body><h1>%1</"
                                                 "h1><p>%2</p></body></html>")
                                      .arg(EscapeHtml(navigation_title),
                                           EscapeHtml(QString::fromStdString(
                                               article.plain_text))));
                }
                if (active)
                    status_->setText(tr("%1 result(s)")
                                         .arg(static_cast<qulonglong>(
                                             response.entries.size())));
            } else if (!response.errors.empty()) {
                view->setHtml(
                    QStringLiteral("<!doctype html><html><body><h1>Lookup "
                                   "failed</h1><p>%1</p></body></html>")
                        .arg(EscapeHtml(QString::fromStdString(
                            response.errors.front().message))));
                if (active)
                    status_->setText(QStringLiteral("Lookup failed"));
            } else {
                view->setHtml(QStringLiteral(
                                  "<!doctype html><html><body><h1>%1</h1><p>No "
                                  "result found.</p></body></html>")
                                  .arg(EscapeHtml(navigation_title)));
                if (active)
                    status_->setText(QStringLiteral("No result"));
            }
        } catch (const std::exception& error) {
            lookup_results_.erase(id);
            view->setHtml(
                QStringLiteral("<!doctype html><html><body><h1>Lookup "
                               "failed</h1><p>%1</p></body></html>")
                    .arg(EscapeHtml(QString::fromLocal8Bit(error.what()))));
            if (active)
                status_->setText(QStringLiteral("Lookup failed"));
        }
        if (active)
            RefreshResultsNavigation();
    }
    if (requests_.empty())
        completion_timer_->stop();
    const auto active_id = TabIdAt(article_tabs_->currentIndex());
    lookup_button_->setEnabled(requests_.find(active_id) == requests_.end());
}

void MainWindow::FindInArticle(bool backwards) {
    const QString text = article_search_->text();
    if (text.isEmpty()) {
        article_view_->findText(QString());
        article_search_status_->clear();
        return;
    }
    QWebEnginePage::FindFlags flags;
    if (backwards) {
        flags |= QWebEnginePage::FindBackward;
    }
    article_view_->findText(
        text, flags, [this](const QWebEngineFindTextResult& result) {
            if (result.numberOfMatches() == 0) {
                article_search_status_->setText(QStringLiteral("No matches"));
                return;
            }
            article_search_status_->setText(tr("%1 of %2")
                                                .arg(result.activeMatch())
                                                .arg(result.numberOfMatches()));
        });
}

void MainWindow::SaveArticleAsPdf() {
    const QString file_name = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Article as PDF"), QString(),
        QStringLiteral("PDF files (*.pdf)"));
    if (file_name.isEmpty()) {
        return;
    }
    const QString path =
        file_name.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)
            ? file_name
            : file_name + QStringLiteral(".pdf");
    status_->setText(QStringLiteral("Saving PDF..."));
    article_view_->printToPdf(path);
}

void MainWindow::PrintArticle() {
    if (article_view_ == nullptr || print_in_progress_)
        return;
    if (printer_ == nullptr)
        printer_ = std::make_unique<QPrinter>(QPrinter::HighResolution);
    const bool available =
        printer_available_ ? printer_available_() : printer_->isValid();
    if (!available) {
        status_->setText(QStringLiteral("No printer is available"));
        return;
    }
    const QPointer<ArticleView> captured_view(article_view_);
    const bool accepted =
        print_dialog_executor_
            ? print_dialog_executor_(printer_.get())
            : [this]() {
                  QPrintDialog dialog(printer_.get(), this);
                  dialog.setWindowTitle(QStringLiteral("Print Article"));
                  return dialog.exec() == QDialog::Accepted;
              }();
    if (!accepted)
        return;
    if (!captured_view.isNull())
        StartPrinterRender(captured_view.data(), printer_.get());
}

void MainWindow::PreviewArticle() {
    if (article_view_ == nullptr || print_in_progress_)
        return;
    if (printer_ == nullptr)
        printer_ = std::make_unique<QPrinter>(QPrinter::HighResolution);
    const bool available =
        printer_available_ ? printer_available_() : printer_->isValid();
    if (!available) {
        status_->setText(QStringLiteral("No printer is available"));
        return;
    }
    const QPointer<ArticleView> captured_view(article_view_);
    const auto paint = [this, captured_view]() {
        if (!captured_view.isNull())
            StartPrinterRender(captured_view.data(), printer_.get());
    };
    if (print_preview_executor_) {
        print_preview_executor_(printer_.get(), paint);
        return;
    }
    QPrintPreviewDialog dialog(printer_.get(), this);
    connect(&dialog, &QPrintPreviewDialog::paintRequested, this,
            [paint](QPrinter*) { paint(); });
    dialog.exec();
}

void MainWindow::StartPrinterRender(ArticleView* view, QPrinter* printer) {
    if (view == nullptr || printer == nullptr || print_in_progress_)
        return;
    print_in_progress_ = true;
    UpdateFileActions();
    status_->setText(QStringLiteral("Printing..."));
    if (print_dispatcher_) {
        print_dispatcher_(view, printer);
    } else {
        view->print(printer);
    }
}

void MainWindow::SaveArticle() {
    if (article_view_ == nullptr || save_in_progress_)
        return;
    const QString file_name =
        save_article_path_provider_
            ? save_article_path_provider_()
            : QFileDialog::getSaveFileName(
                  this, QStringLiteral("Save Article as HTML"), QString(),
                  QStringLiteral("HTML files (*.html *.htm)"));
    if (file_name.isEmpty()) {
        return;
    }
    const QString path = QFileInfo(file_name).suffix().isEmpty()
                             ? file_name + QStringLiteral(".html")
                             : file_name;
    save_in_progress_ = true;
    UpdateFileActions();
    article_view_->page()->toHtml([this, path](const QString& html) {
        const bool saved = article_save_writer_
                               ? article_save_writer_(path, html)
                               : [&path, &html]() {
                                     QSaveFile file(path);
                                     return file.open(QIODevice::WriteOnly) &&
                                            file.write(html.toUtf8()) != -1 &&
                                            file.commit();
                                 }();
        save_in_progress_ = false;
        UpdateFileActions();
        if (!saved) {
            status_->setText(QStringLiteral("HTML save failed"));
            return;
        }
        status_->setText(QStringLiteral("HTML saved"));
    });
}

void MainWindow::UpdateFileActions() {
    const bool has_article = facade_ != nullptr && article_view_ != nullptr;
    new_tab_action_->setEnabled(facade_ != nullptr);
    save_article_action_->setEnabled(has_article && !save_in_progress_);
    print_action_->setEnabled(has_article && !print_in_progress_);
    print_preview_action_->setEnabled(has_article && !print_in_progress_);
    quit_action_->setEnabled(true);
}

void MainWindow::UpdateNavigationActions() {
    bool can_back = false;
    bool can_forward = false;
    if (facade_ != nullptr) {
        const auto state = facade_->GetArticleTabsState();
        const auto found = std::find_if(
            state.tabs.begin(), state.tabs.end(),
            [&](const auto& tab) { return tab.id == state.active_tab_id; });
        if (found != state.tabs.end()) {
            can_back = found->can_go_back;
            can_forward = found->can_go_forward;
        }
    }
    back_action_->setEnabled(can_back);
    forward_action_->setEnabled(can_forward);
}

void MainWindow::ZoomArticle(double delta) {
    constexpr double kMinimumZoom = 0.25;
    constexpr double kMaximumZoom = 5.0;
    article_view_->setZoomFactor(std::clamp(article_view_->zoomFactor() + delta,
                                            kMinimumZoom, kMaximumZoom));
}

void MainWindow::ShowMessage(const QString& title, const QString& message) {
    article_view_->setHtml(
        QStringLiteral("<!doctype html><html><head><meta charset=\"utf-8\">"
                       "</head><body><h1>%1</h1><p>%2</p></body></html>")
            .arg(EscapeHtml(title), EscapeHtml(message)));
}
