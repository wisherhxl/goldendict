// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.h"

#include <algorithm>
#include <exception>

#include <QAction>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWebEngineFindTextResult>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWidget>

#include "article_page.h"
#include "article_scheme_handler.h"
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

    auto* article_toolbar = addToolBar(QStringLiteral("Article"));
    article_toolbar->setObjectName(QStringLiteral("articleToolbar"));
    back_action_ = article_toolbar->addAction(QStringLiteral("Back"));
    back_action_->setShortcut(QKeySequence::Back);
    forward_action_ = article_toolbar->addAction(QStringLiteral("Forward"));
    forward_action_->setShortcut(QKeySequence::Forward);
    auto* reload_action = article_toolbar->addAction(QStringLiteral("Reload"));
    reload_action->setShortcut(QKeySequence::Refresh);
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
                    const bool passed = result.numberOfMatches() == 2 &&
                                        article_view_->zoomFactor() == 1.25;
                    article_view_->findText(QString());
                    completion(passed);
                });
        },
        Qt::SingleShotConnection);
    article_view_->setHtml(QStringLiteral(
        "<!doctype html><html><body><p>needle</p><p>needle</p></body></html>"));
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
    const auto count = facade == nullptr
                           ? std::size_t{0}
                           : facade->GetDictionaryService().GetCatalog().size();
    status_->setText(
        tr("%1 dictionary loaded").arg(static_cast<qulonglong>(count)));
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
    query.text = query_->text().trimmed().toStdString();
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
            const auto& article = response.entries.front().article;
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
