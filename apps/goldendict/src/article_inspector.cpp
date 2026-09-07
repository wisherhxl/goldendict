// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_inspector.h"

#include <QVBoxLayout>
#include <QWebEngineProfile>
#include <QWebEngineView>

ArticleInspector::ArticleInspector(QWebEnginePage* inspected_page)
    : inspected_page_(inspected_page) {
    setObjectName(QStringLiteral("articleInspector"));
    setWindowTitle(tr("Web Inspector"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    profile_ = new QWebEngineProfile(this);
    view_ = new QWebEngineView(this);
    view_->setObjectName(QStringLiteral("articleInspectorContent"));
    view_->setPage(new QWebEnginePage(profile_, view_));
    layout->addWidget(view_);
    resize(sizeHint());
    connect(view_->page(), &QWebEnginePage::titleChanged, this,
            &QWidget::setWindowTitle);
    connect(view_->page(), &QWebEnginePage::windowCloseRequested, this,
            &QWidget::close);
    connect(inspected_page, &QObject::destroyed, this, [this]() {
        inspected_page_.clear();
        hide();
    });
    inspected_page->setDevToolsPage(view_->page());
}

ArticleInspector::~ArticleInspector() {
    if (inspected_page_ && inspected_page_->devToolsPage() == view_->page()) {
        inspected_page_->setDevToolsPage(nullptr);
    }
    // A WebEngine profile must outlive every page that uses it.
    delete view_;
    delete profile_;
}

QWebEnginePage* ArticleInspector::inspectedPage() const {
    return inspected_page_;
}

void ArticleInspector::Inspect(bool context_target) {
    if (!inspected_page_)
        return;
    if (isMinimized())
        showNormal();
    else
        show();
    raise();
    activateWindow();
    view_->setFocus();
    if (context_target)
        inspected_page_->triggerAction(QWebEnginePage::InspectElement);
}
