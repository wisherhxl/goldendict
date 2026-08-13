// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.h"

#include <algorithm>
#include <exception>

#include <QAction>
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
#include <QPixmap>
#include <QPushButton>
#include <QSaveFile>
#include <QShortcut>
#include <QTimer>
#include <QToolBar>
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
    lookup_button_ = new QPushButton(QStringLiteral("Lookup"), central);
    controls->addWidget(directory_button);
    controls->addWidget(group_selector_);
    controls->addWidget(edit_groups_button_);
    controls->addWidget(query_, 1);
    controls->addWidget(lookup_button_);
    layout->addLayout(controls);

    status_ = new QLabel(QStringLiteral("No dictionary configured"), central);
    layout->addWidget(status_);
    article_view_ = new QWebEngineView(central);
    article_page_ = new ArticlePage(article_view_);
    article_view_->setPage(article_page_);
    layout->addWidget(article_view_, 1);

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
    connect(lookup_button_, &QPushButton::clicked, this,
            &MainWindow::StartLookup);
    connect(query_, &QLineEdit::returnPressed, this, &MainWindow::StartLookup);
    connect(completion_timer_, &QTimer::timeout, this,
            &MainWindow::FinishLookup);
    connect(article_page_, &ArticlePage::LookupRequested, this,
            [this](const QString& text) {
                query_->setText(text);
                StartLookup();
            });
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
    connect(article_page_, &ArticlePage::ExternalUrlRequested, this,
            [](const QUrl& url) { QDesktopServices::openUrl(url); });
    connect(back_action_, &QAction::triggered, article_view_,
            &QWebEngineView::back);
    connect(forward_action_, &QAction::triggered, article_view_,
            &QWebEngineView::forward);
    connect(reload_action, &QAction::triggered, article_view_,
            &QWebEngineView::reload);
    connect(article_view_, &QWebEngineView::urlChanged, this,
            &MainWindow::UpdateNavigationActions);
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
    connect(copy_action, &QAction::triggered, article_page_,
            [this]() { article_page_->triggerAction(QWebEnginePage::Copy); });
    connect(save_action, &QAction::triggered, this, &MainWindow::SaveArticle);
    connect(print_action, &QAction::triggered, this, &MainWindow::PrintArticle);
    connect(article_view_, &QWebEngineView::pdfPrintingFinished, this,
            [this](const QString&, bool success) {
                status_->setText(success ? QStringLiteral("PDF saved")
                                         : QStringLiteral("PDF save failed"));
            });
    auto* find_action = new QAction(this);
    find_action->setShortcut(QKeySequence::Find);
    find_action->setShortcutContext(Qt::WindowShortcut);
    addAction(find_action);
    connect(find_action, &QAction::triggered, article_search_,
            qOverload<>(&QLineEdit::setFocus));
    UpdateNavigationActions();
    ShowMessage(QStringLiteral("GoldenDict"),
                QStringLiteral("Choose a dictionary folder to begin."));
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
    QWebEngineProfile::defaultProfile()->removeUrlSchemeHandler(
        scheme_handler_);
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
    request_.reset();
    facade_ = facade;
    article_page_->SetFacade(facade);
    scheme_handler_->SetFacade(facade);
    if (dictionary_browser_ != nullptr) {
        dictionary_browser_->SetFacade(facade);
    }
    const auto count = facade == nullptr
                           ? std::size_t{0}
                           : facade->GetDictionaryService().GetCatalog().size();
    status_->setText(
        tr("%1 dictionary loaded").arg(static_cast<qulonglong>(count)));
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
    if (facade_ == nullptr || query_->text().trimmed().isEmpty()) {
        return;
    }
    completion_timer_->stop();
    request_.reset();
    goldendict::core::LookupQuery query;
    const QString word = query_->text().trimmed();
    query.text = word.toStdString();
    query.group_id = selected_group_id_;
    goldendict::core::TabNavigationState navigation;
    navigation.kind = goldendict::core::TabNavigationKind::kLookup;
    navigation.query = query.text;
    navigation.group_id = query.group_id;
    navigation.title = query.text;
    const auto tab_result = facade_->OpenArticleTab(
        navigation, goldendict::core::TabOpenPolicy::kCurrentTab,
        goldendict::core::TabActivationPolicy::kActivate);
    if (!tab_result) {
        status_->setText(QStringLiteral("Unable to update article state"));
        return;
    }
    emit LookupSubmitted(word, selected_group_id_);
    request_ = facade_->GetDictionaryService().StartLookup(std::move(query));
    status_->setText(QStringLiteral("Looking up..."));
    lookup_button_->setEnabled(false);
    completion_timer_->start();
}

void MainWindow::FinishLookup() {
    if (request_ == nullptr || !request_->IsFinished()) {
        return;
    }
    completion_timer_->stop();
    lookup_button_->setEnabled(true);
    try {
        const auto response = request_->Await();
        request_.reset();
        if (!response.entries.empty()) {
            const auto article = facade_->ComposeLookupPage(response);
            if (article.sanitized_html.has_value()) {
                article_view_->setHtml(QString::fromUtf8(
                    article.sanitized_html->data(),
                    static_cast<qsizetype>(article.sanitized_html->size())));
            } else {
                ShowMessage(query_->text(),
                            QString::fromStdString(article.plain_text));
            }
            status_->setText(
                tr("%1 result(s)")
                    .arg(static_cast<qulonglong>(response.entries.size())));
        } else if (!response.errors.empty()) {
            ShowMessage(
                QStringLiteral("Lookup failed"),
                QString::fromStdString(response.errors.front().message));
            status_->setText(QStringLiteral("Lookup failed"));
        } else {
            ShowMessage(query_->text(), QStringLiteral("No result found."));
            status_->setText(QStringLiteral("No result"));
        }
    } catch (const std::exception& error) {
        request_.reset();
        ShowMessage(QStringLiteral("Lookup failed"),
                    QString::fromLocal8Bit(error.what()));
        status_->setText(QStringLiteral("Lookup failed"));
    }
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
    back_action_->setEnabled(article_view_->history()->canGoBack());
    forward_action_->setEnabled(article_view_->history()->canGoForward());
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
