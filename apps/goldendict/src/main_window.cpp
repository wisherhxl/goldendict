// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.h"

#include <algorithm>
#include <exception>

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSaveFile>
#include <QShortcut>
#include <QSignalBlocker>
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
#include "dictionary_browser.h"
#include "goldendict/core/desktop_facade.h"
#include "group_editor.h"

namespace {

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
        new QPushButton(QStringLiteral("Dictionary Folder..."), central);
    group_selector_ = new QComboBox(central);
    group_selector_->setObjectName(QStringLiteral("groupSelector"));
    group_selector_->setToolTip(
        QStringLiteral("Choose a dictionary group (Alt+G)"));
    group_selector_->setMaxVisibleItems(30);
    edit_groups_button_ =
        new QPushButton(QStringLiteral("Edit Groups..."), central);
    edit_groups_button_->setObjectName(QStringLiteral("editGroupsButton"));
    query_ = new QLineEdit(central);
    query_->setPlaceholderText(QStringLiteral("Enter a word"));
    lookup_button_ = new QToolButton(central);
    lookup_button_->setText(QStringLiteral("Lookup"));
    lookup_button_->setPopupMode(QToolButton::MenuButtonPopup);
    auto* lookup_menu = new QMenu(lookup_button_);
    auto* lookup_new_tab =
        lookup_menu->addAction(QStringLiteral("Lookup in New Tab"));
    auto* lookup_background_tab =
        lookup_menu->addAction(QStringLiteral("Lookup in Background Tab"));
    lookup_button_->setMenu(lookup_menu);
    controls->addWidget(directory_button);
    controls->addWidget(group_selector_);
    controls->addWidget(edit_groups_button_);
    controls->addWidget(query_, 1);
    controls->addWidget(lookup_button_);
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
    auto* add_foreground_tab =
        add_tab_menu->addAction(QStringLiteral("New Tab"));
    auto* add_background_tab =
        add_tab_menu->addAction(QStringLiteral("New Background Tab"));
    add_tab_button->setMenu(add_tab_menu);
    add_tab_button->setPopupMode(QToolButton::MenuButtonPopup);
    article_tabs_->setCornerWidget(add_tab_button, Qt::TopLeftCorner);
    layout->addWidget(article_tabs_, 1);

    auto* history_dock = new QDockWidget(QStringLiteral("History"), this);
    history_dock->setObjectName(QStringLiteral("historyDock"));
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
    addDockWidget(Qt::LeftDockWidgetArea, history_dock);

    auto* favorites_dock = new QDockWidget(QStringLiteral("Favorites"), this);
    favorites_dock->setObjectName(QStringLiteral("favoritesDock"));
    favorites_tree_ = new QTreeWidget(favorites_dock);
    favorites_tree_->setObjectName(QStringLiteral("favoritesTree"));
    favorites_tree_->setHeaderHidden(true);
    favorites_dock->setWidget(favorites_tree_);
    addDockWidget(Qt::LeftDockWidgetArea, favorites_dock);
    tabifyDockWidget(history_dock, favorites_dock);
    history_dock->raise();

    auto* article_toolbar = addToolBar(QStringLiteral("Article"));
    article_toolbar->setObjectName(QStringLiteral("articleToolbar"));
    back_action_ = article_toolbar->addAction(QStringLiteral("Back"));
    back_action_->setShortcut(QKeySequence::Back);
    forward_action_ = article_toolbar->addAction(QStringLiteral("Forward"));
    forward_action_->setShortcut(QKeySequence::Forward);
    auto* reload_action = article_toolbar->addAction(QStringLiteral("Reload"));
    reload_action->setShortcut(QKeySequence::Refresh);
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
    auto* save_action = article_toolbar->addAction(QStringLiteral("Save HTML"));
    save_action->setShortcut(QKeySequence::Save);
    auto* print_action =
        article_toolbar->addAction(QStringLiteral("Print PDF"));
    print_action->setShortcut(QKeySequence::Print);
    setCentralWidget(central);

    scheme_handler_ = new ArticleSchemeHandler(this);
    QWebEngineProfile::defaultProfile()->installUrlSchemeHandler(
        QByteArrayLiteral("goldendict"), scheme_handler_);
    completion_timer_ = new QTimer(this);
    completion_timer_->setInterval(15);

    connect(directory_button, &QPushButton::clicked, this,
            &MainWindow::ChooseDictionaryDirectory);
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
    connect(add_tab_button, &QToolButton::clicked, this, [this]() {
        CreateEmptyArticleTab(!preferences_.open_new_tabs_in_background);
    });
    connect(add_foreground_tab, &QAction::triggered, this,
            [this]() { CreateEmptyArticleTab(true); });
    connect(add_background_tab, &QAction::triggered, this,
            [this]() { CreateEmptyArticleTab(false); });
    connect(article_tabs_, &QTabWidget::currentChanged, this,
            &MainWindow::ActivateArticleTab);
    connect(article_tabs_, &QTabWidget::tabCloseRequested, this,
            &MainWindow::CloseArticleTab);
    connect(article_tabs_->tabBar(), &QWidget::customContextMenuRequested, this,
            &MainWindow::ShowTabContextMenu);
    connect(query_, &QLineEdit::returnPressed, this, &MainWindow::StartLookup);
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
    connect(clear_history_button_, &QPushButton::clicked, this,
            &MainWindow::ClearHistoryRequested);
    connect(export_history_button_, &QPushButton::clicked, this,
            &MainWindow::ExportHistory);
    connect(import_history_button_, &QPushButton::clicked, this,
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
    connect(save_action, &QAction::triggered, this, &MainWindow::SaveArticle);
    connect(print_action, &QAction::triggered, this, &MainWindow::PrintArticle);
    auto* find_action = new QAction(this);
    find_action->setShortcut(QKeySequence::Find);
    find_action->setShortcutContext(Qt::WindowShortcut);
    addAction(find_action);
    connect(find_action, &QAction::triggered, article_search_,
            qOverload<>(&QLineEdit::setFocus));
    UpdateNavigationActions();
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
    const bool restored_geometry =
        RestoreMainWindowGeometry(saved_geometry) && size() == default_size;
    const QRect before_rejected = geometry();
    const bool rejected_geometry =
        !RestoreMainWindowGeometry("not-qt-geometry") &&
        geometry() == before_rejected;
    bool passed = article_tabs_->count() == 1 &&
                  facade_->GetArticleTabsState().tabs.size() == 1U &&
                  restored_geometry && rejected_geometry;
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
        bool ok = passed && state.tabs.size() == 3U &&
                  article_tabs_->count() == 3 &&
                  state.tabs[0].navigation.query == "application" &&
                  state.tabs[1].navigation.query == "apple" &&
                  state.tabs[2].navigation.query == "application" &&
                  state.active_tab_id == state.tabs[1].id;
        auto* first = ArticleView(state.tabs[0].id);
        auto* second = ArticleView(state.tabs[1].id);
        auto* third = ArticleView(state.tabs[2].id);
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
                                   selected_group_id_ == 0U;
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
    if (prepare) {
        const auto restored = facade_->RestoreArticleTabSession(expected);
        if (!restored) {
            completion(false);
            return;
        }
        RebuildArticleTabs();
        emit ArticleTabSessionMutated();
        completion(true);
        return;
    }
    bool passed = facade_->ExportArticleTabSession() == expected &&
                  article_tabs_->count() == 2 && TabIdAt(0) == 7U &&
                  TabIdAt(1) == 42U &&
                  TabIdAt(article_tabs_->currentIndex()) == 42U;
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [this, passed, completion = std::move(completion), poll]() mutable {
        FinishLookup();
        if (!requests_.empty()) {
            QTimer::singleShot(10, this, *poll);
            return;
        }
        auto* first = ArticleView(7U);
        auto* second = ArticleView(42U);
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
    SetHistoryWords(
        {QStringLiteral("Alpha"), QStringLiteral("line\nbreak\rentry")});
    const bool exported = ExportHistoryToFile(path);
    QFile file(path);
    const bool opened = file.open(QIODevice::ReadOnly);
    const QByteArray expected =
        QByteArray::fromHex("efbbbf") + "Alpha\nline break entry\n";
    completion(exported && opened && file.readAll() == expected);
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
        QByteArray::fromHex("efbbbf") + "apple\napplication\n",
        std::move(completion));
}

MainWindow::~MainWindow() {
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

QWebEngineView* MainWindow::ArticleView(
    goldendict::core::ArticleTabId tab_id) const {
    for (int index = 0; index < article_tabs_->count(); ++index) {
        if (TabIdAt(index) == tab_id) {
            return qobject_cast<QWebEngineView*>(article_tabs_->widget(index));
        }
    }
    return nullptr;
}

QWebEngineView* MainWindow::CreateArticleView(
    goldendict::core::ArticleTabId tab_id) {
    auto* view = new QWebEngineView(article_tabs_);
    view->setProperty("articleTabId", QVariant::fromValue<qulonglong>(tab_id));
    auto* page = new ArticlePage(view);
    page->SetFacade(facade_);
    page->SetOpenNewTabsInBackground(preferences_.open_new_tabs_in_background);
    view->setPage(page);
    connect(
        page, &ArticlePage::LookupRequested, this,
        [this, tab_id](const QString& text, const QString& internal_url,
                       ArticleLinkDisposition disposition) {
            if (facade_ == nullptr)
                return;
            std::uint32_t group_id = 0U;
            const auto state = facade_->GetArticleTabsState();
            const auto found = std::find_if(
                state.tabs.begin(), state.tabs.end(),
                [tab_id](const auto& tab) { return tab.id == tab_id; });
            if (found != state.tabs.end())
                group_id = found->navigation.group_id;
            goldendict::core::TabNavigationState navigation;
            navigation.kind =
                goldendict::core::TabNavigationKind::kInternalLink;
            navigation.query = text.toStdString();
            navigation.group_id = group_id;
            navigation.title = navigation.query;
            navigation.internal_url = internal_url.toStdString();
            const bool foreground =
                disposition == ArticleLinkDisposition::kNewForegroundTab;
            const bool background =
                disposition == ArticleLinkDisposition::kNewBackgroundTab;
            const auto result = facade_->OpenArticleTab(
                navigation,
                foreground || background
                    ? goldendict::core::TabOpenPolicy::kNewTab
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
        });
    connect(page, &ArticlePage::ExternalUrlRequested, this,
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
    view->setHtml(QStringLiteral(
        "<!doctype html><html><body><h1>GoldenDict</h1>"
        "<p>Choose a dictionary folder to begin.</p></body></html>"));
    return view;
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
            QWidget* widget = article_tabs_->widget(index);
            article_tabs_->removeTab(index);
            widget->deleteLater();
        }
    }
    for (std::size_t desired = 0; desired < state.tabs.size(); ++desired) {
        const auto& tab = state.tabs[desired];
        auto* view = ArticleView(tab.id);
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
    auto* active_view = ArticleView(active->id);
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
        if (auto* view = ArticleView(id); view != nullptr) {
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

void MainWindow::SetFacade(goldendict::core::DesktopFacade* facade) {
    completion_timer_->stop();
    for (auto& [id, request] : requests_) {
        static_cast<void>(id);
        request->Cancel();
    }
    requests_.clear();
    while (article_tabs_->count() > 0) {
        QWidget* widget = article_tabs_->widget(0);
        article_tabs_->removeTab(0);
        delete widget;
    }
    article_view_ = nullptr;
    article_page_ = nullptr;
    facade_ = facade;
    scheme_handler_->SetFacade(facade);
    if (dictionary_browser_ != nullptr) {
        dictionary_browser_->SetFacade(facade);
    }
    const auto count = facade == nullptr
                           ? std::size_t{0}
                           : facade->GetDictionaryService().GetCatalog().size();
    status_->setText(
        tr("%1 dictionary loaded").arg(static_cast<qulonglong>(count)));
    if (facade_ != nullptr)
        RebuildArticleTabs();
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
}

void MainWindow::SetHistoryItems(const std::vector<HistoryViewItem>& items) {
    history_items_ = items;
    RefreshHistoryList();
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

void MainWindow::ExportHistory() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export history to file"), QString(),
        QStringLiteral("Text files (*.txt);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    status_->setText(ExportHistoryToFile(path)
                         ? QStringLiteral("History export complete")
                         : QStringLiteral("History export failed"));
}

void MainWindow::ImportHistory() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import history from file"), QString(),
        QStringLiteral("Text files (*.txt);;All files (*.*)"));
    if (!path.isEmpty()) {
        emit ImportHistoryRequested(path, selected_group_id_);
    }
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

bool MainWindow::ExportHistoryToFile(const QString& path) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(QByteArray::fromHex("efbbbf")) != 3) {
        return false;
    }
    for (const auto& entry : history_items_) {
        QByteArray line = entry.word.toUtf8();
        line.replace('\n', ' ');
        line.replace('\r', ' ');
        line.push_back('\n');
        if (file.write(line) != line.size()) {
            return false;
        }
    }
    return file.commit();
}

void MainWindow::SetFavoriteItems(const std::vector<FavoriteViewItem>& items) {
    const auto append = [&](const auto& self, QTreeWidgetItem* parent,
                            const FavoriteViewItem& item) -> void {
        auto* tree_item = parent == nullptr
                              ? new QTreeWidgetItem(favorites_tree_)
                              : new QTreeWidgetItem(parent);
        tree_item->setText(0, item.text);
        tree_item->setData(0, Qt::UserRole, item.folder);
        tree_item->setData(0, Qt::UserRole + 1, QVariant::fromValue(item.path));
        for (const auto& child : item.children) {
            self(self, tree_item, child);
        }
        tree_item->setExpanded(item.expanded);
    };
    favorites_tree_->clear();
    for (const auto& item : items) {
        append(append, nullptr, item);
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

void MainWindow::ChooseDictionaryDirectory() {
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose Dictionary Folder"));
    if (!directory.isEmpty()) {
        emit DictionaryDirectorySelected(directory);
    }
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
    if (auto existing = requests_.find(tab_id); existing != requests_.end()) {
        existing->second->Cancel();
        requests_.erase(existing);
    }
    goldendict::core::LookupQuery query;
    query.text = navigation.query;
    query.group_id = navigation.group_id;
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
        auto* view = ArticleView(id);
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
            view->setHtml(
                QStringLiteral("<!doctype html><html><body><h1>Lookup "
                               "failed</h1><p>%1</p></body></html>")
                    .arg(EscapeHtml(QString::fromLocal8Bit(error.what()))));
            if (active)
                status_->setText(QStringLiteral("Lookup failed"));
        }
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

void MainWindow::PrintArticle() {
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

void MainWindow::SaveArticle() {
    const QString file_name = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Article as HTML"), QString(),
        QStringLiteral("HTML files (*.html *.htm)"));
    if (file_name.isEmpty()) {
        return;
    }
    const QString path = QFileInfo(file_name).suffix().isEmpty()
                             ? file_name + QStringLiteral(".html")
                             : file_name;
    article_view_->page()->toHtml([this, path](const QString& html) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(html.toUtf8()) == -1 || !file.commit()) {
            status_->setText(QStringLiteral("HTML save failed"));
            return;
        }
        status_->setText(QStringLiteral("HTML saved"));
    });
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
