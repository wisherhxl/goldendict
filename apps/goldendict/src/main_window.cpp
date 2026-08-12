// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.h"

#include <algorithm>
#include <exception>

#include <QAction>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSaveFile>
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
    query_ = new QLineEdit(central);
    query_->setPlaceholderText(QStringLiteral("Enter a word"));
    lookup_button_ = new QPushButton(QStringLiteral("Lookup"), central);
    controls->addWidget(directory_button);
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
    history_layout->addWidget(history_filter_);
    history_layout->addWidget(history_list_, 1);
    history_layout->addWidget(clear_history_button_);
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
                query_->setText(item->text());
                StartLookup();
            });
    connect(history_filter_, &QLineEdit::textChanged, this,
            &MainWindow::RefreshHistoryList);
    connect(clear_history_button_, &QPushButton::clicked, this,
            &MainWindow::ClearHistoryRequested);
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
            emit AddFavoriteRequested(word);
        }
    });
    connect(favorites_tree_, &QTreeWidget::itemSelectionChanged, this,
            [this]() {
                remove_favorite_action_->setEnabled(
                    favorites_tree_->currentItem() != nullptr);
            });
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
    connect(
        this, &MainWindow::LookupSubmitted, this,
        [this, expected,
         completion = std::move(completion)](const QString& submitted) mutable {
            completion(submitted == expected && history_list_->count() > 0 &&
                       history_list_->item(0)->text() == expected);
        },
        Qt::SingleShotConnection);
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
            completion(filtered && history_words_.isEmpty() &&
                       history_list_->count() == 0);
        },
        Qt::SingleShotConnection);
    clear_history_button_->click();
}

void MainWindow::RunFavoritesSmokeCheck(std::function<void(bool)> completion) {
    const QString expected = QStringLiteral("favorites-smoke-entry");
    const int initial_count = favorites_tree_->topLevelItemCount();
    connect(
        this, &MainWindow::AddFavoriteRequested, this,
        [this, expected, initial_count,
         completion = std::move(completion)](const QString& submitted) mutable {
            auto* item = favorites_tree_->topLevelItem(
                favorites_tree_->topLevelItemCount() - 1);
            const bool added = submitted == expected && item != nullptr &&
                               item->text(0) == expected;
            connect(
                this, &MainWindow::RemoveFavoriteRequested, this,
                [this, added, initial_count,
                 completion =
                     std::move(completion)](const QList<int>& path) mutable {
                    completion(added && path == QList<int>{initial_count} &&
                               favorites_tree_->topLevelItemCount() ==
                                   initial_count);
                },
                Qt::SingleShotConnection);
            favorites_tree_->setCurrentItem(item);
            remove_favorite_action_->trigger();
        },
        Qt::SingleShotConnection);
    query_->setText(expected);
    add_favorite_action_->trigger();
}

void MainWindow::RunDictionaryBrowserSmokeCheck(
    std::function<void(bool)> completion) {
    auto lookup_passed = std::make_shared<bool>(false);
    connect(
        this, &MainWindow::LookupSubmitted, this,
        [lookup_passed](const QString& submitted) {
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

MainWindow::~MainWindow() {
    QWebEngineProfile::defaultProfile()->removeUrlSchemeHandler(
        scheme_handler_);
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
    history_words_ = words;
    RefreshHistoryList();
}

void MainWindow::RefreshHistoryList() {
    history_list_->clear();
    const QString filter = history_filter_->text().trimmed();
    for (const QString& word : history_words_) {
        if (filter.isEmpty() || word.contains(filter, Qt::CaseInsensitive)) {
            history_list_->addItem(word);
        }
    }
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
    emit LookupSubmitted(word);
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
