// SPDX-License-Identifier: GPL-3.0-or-later

#include "help_window.h"

#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHelpContentWidget>
#include <QHelpEngine>
#include <QHelpIndexWidget>
#include <QHelpLink>
#include <QIcon>
#include <QLocale>
#include <QSplitter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

namespace goldendict::app {

namespace {

constexpr qreal kMinimumZoom = 0.2;
constexpr qreal kMaximumZoom = 5.0;

bool IsRussian(const QString& locale_name) {
    const QLocale locale(locale_name);
    return locale.language() == QLocale::Russian;
}

}  // namespace

QString HelpIdentifier(HelpIntent intent) {
    switch (intent) {
        case HelpIntent::kReference:
            return QStringLiteral("Content");
        case HelpIntent::kFullTextSearch:
            return QStringLiteral("Full-text search");
        case HelpIntent::kDictionaryHeadwords:
            return QStringLiteral("Dictionary headwords");
    }
    return {};
}

QString SelectHelpCollectionName(const QString& help_language,
                                 const QString& interface_language,
                                 const QString& system_locale) {
    const QString selected =
        !help_language.isEmpty()
            ? help_language
            : (!interface_language.isEmpty() ? interface_language
                                             : system_locale);
    return IsRussian(selected) ? QStringLiteral("gdhelp_ru.qch")
                               : QStringLiteral("gdhelp_en.qch");
}

QString InstalledHelpDirectory() {
    return QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../share/goldendict/help")));
}

bool IsExternalHelpUrl(const QUrl& url) {
    return url.scheme() == QStringLiteral("http") ||
           url.scheme() == QStringLiteral("https");
}

class HelpWindow::Browser final : public QTextBrowser {
   public:
    Browser(QHelpEngine* engine, QWidget* parent)
        : QTextBrowser(parent), engine_(engine) {
        setOpenLinks(false);
        connect(this, &QTextBrowser::anchorClicked, this,
                [this](const QUrl& url) {
                    if (IsExternalHelpUrl(url)) {
                        QDesktopServices::openUrl(url);
                    } else {
                        setSource(url);
                    }
                });
    }

   protected:
    QVariant loadResource(int type, const QUrl& name) override {
        if (type >= 4 || engine_ == nullptr)
            return {};
        const QUrl resolved =
            name.isRelative() ? source().resolved(name) : name;
        return engine_->fileData(resolved);
    }

   private:
    QHelpEngine* engine_;
};

HelpWindow::HelpWindow(const QString& help_directory,
                       const QString& help_language,
                       const QString& interface_language,
                       const QString& system_locale, QWidget* parent)
    : QDialog(parent),
      collection_directory_(std::make_unique<QTemporaryDir>()) {
    resize(600, 450);
    setWindowTitle(tr("GoldenDict help"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    collection_path_ =
        QDir(help_directory)
            .filePath(SelectHelpCollectionName(
                help_language, interface_language, system_locale));
    if (!collection_directory_->isValid() ||
        !QFileInfo(collection_path_).isFile()) {
        return;
    }

    const QString qhc_path = QDir(collection_directory_->path())
                                 .filePath(QStringLiteral("gdhelp.qhc"));
    help_engine_ = new QHelpEngine(qhc_path, this);
    help_engine_->setReadOnly(false);
    if (!help_engine_->setupData()) {
        qWarning().noquote() << "GoldenDict help collection setup failed:"
                             << help_engine_->error();
        delete help_engine_;
        help_engine_ = nullptr;
        return;
    }
    if (!help_engine_->registerDocumentation(collection_path_)) {
        qWarning().noquote()
            << "GoldenDict help documentation registration failed:"
            << help_engine_->error();
        delete help_engine_;
        help_engine_ = nullptr;
        return;
    }

    auto* layout = new QVBoxLayout(this);
    auto* toolbar = new QToolBar(this);
    auto* home = toolbar->addAction(tr("Home"));
    home->setObjectName(QStringLiteral("helpHomeAction"));
    auto* back = toolbar->addAction(tr("Back"));
    back->setObjectName(QStringLiteral("helpBackAction"));
    auto* forward = toolbar->addAction(tr("Forward"));
    forward->setObjectName(QStringLiteral("helpForwardAction"));
    toolbar->addSeparator();
    auto* zoom_in = toolbar->addAction(tr("Zoom In"));
    zoom_in->setObjectName(QStringLiteral("helpZoomInAction"));
    auto* zoom_out = toolbar->addAction(tr("Zoom Out"));
    zoom_out->setObjectName(QStringLiteral("helpZoomOutAction"));
    auto* normal_size = toolbar->addAction(tr("Normal Size"));
    normal_size->setObjectName(QStringLiteral("helpNormalSizeAction"));
    layout->addWidget(toolbar);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(help_engine_->contentWidget(), tr("Content"));
    tabs->addTab(help_engine_->indexWidget(), tr("Index"));
    browser_ = new Browser(help_engine_, this);
    browser_->setObjectName(QStringLiteral("helpBrowser"));
    auto* splitter = new QSplitter(this);
    splitter->addWidget(tabs);
    splitter->addWidget(browser_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    layout->addWidget(splitter);

    back->setEnabled(false);
    forward->setEnabled(false);
    connect(home, &QAction::triggered, browser_, &QTextBrowser::home);
    connect(back, &QAction::triggered, browser_, &QTextBrowser::backward);
    connect(forward, &QAction::triggered, browser_, &QTextBrowser::forward);
    connect(browser_, &QTextBrowser::backwardAvailable, back,
            &QAction::setEnabled);
    connect(browser_, &QTextBrowser::forwardAvailable, forward,
            &QAction::setEnabled);
    connect(help_engine_->contentWidget(), &QHelpContentWidget::linkActivated,
            browser_, [this](const QUrl& url) { browser_->setSource(url); });
    connect(help_engine_->indexWidget(), &QHelpIndexWidget::documentActivated,
            browser_, [this](const QHelpLink& document, const QString&) {
                browser_->setSource(document.url);
            });

    auto apply_zoom = [browser = browser_, zoom_in, zoom_out,
                       normal_size](qreal factor) {
        browser->setProperty("helpZoomFactor", factor);
        QFont font = browser->font();
        const qreal base = browser->property("helpBasePointSize").toReal();
        font.setPointSizeF(base * factor);
        browser->setFont(font);
        zoom_in->setEnabled(factor < kMaximumZoom);
        zoom_out->setEnabled(factor > kMinimumZoom);
        normal_size->setEnabled(factor != 1.0);
    };
    qreal base = browser_->font().pointSizeF();
    if (base < 10.0)
        base = 10.0;
    browser_->setProperty("helpBasePointSize", base);
    apply_zoom(1.0);
    connect(zoom_in, &QAction::triggered, this,
            [apply_zoom, browser = browser_]() {
                apply_zoom(
                    qMin(kMaximumZoom,
                         browser->property("helpZoomFactor").toReal() + 0.2));
            });
    connect(zoom_out, &QAction::triggered, this,
            [apply_zoom, browser = browser_]() {
                apply_zoom(
                    qMax(kMinimumZoom,
                         browser->property("helpZoomFactor").toReal() - 0.2));
            });
    connect(normal_size, &QAction::triggered, this,
            [apply_zoom]() { apply_zoom(1.0); });
}

HelpWindow::~HelpWindow() {
    delete help_engine_;
    help_engine_ = nullptr;
}

bool HelpWindow::IsReady() const noexcept {
    return browser_ != nullptr;
}

QString HelpWindow::CollectionPath() const {
    return collection_path_;
}

bool HelpWindow::ShowIdentifier(const QString& identifier) {
    if (!IsReady())
        return false;
    const auto links = help_engine_->documentsForIdentifier(identifier);
    if (links.isEmpty())
        return false;
    browser_->setSource(links.constFirst().url);
    return true;
}

}  // namespace goldendict::app
