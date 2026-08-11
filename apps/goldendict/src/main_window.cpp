// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.h"

#include <exception>

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
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
    ShowMessage(QStringLiteral("GoldenDict"),
                QStringLiteral("Choose a dictionary folder to begin."));
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

void MainWindow::ShowMessage(const QString& title, const QString& message) {
    article_view_->setHtml(
        QStringLiteral("<!doctype html><html><head><meta charset=\"utf-8\">"
                       "</head><body><h1>%1</h1><p>%2</p></body></html>")
            .arg(EscapeHtml(title), EscapeHtml(message)));
}
