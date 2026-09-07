// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_inspector.h"

#include <QScopedValueRollback>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWebEngineProfile>
#include <QWebEngineView>

#include <algorithm>

void ArticleInspectorState::SetInitialGeometry(const QByteArray& geometry) {
    saved_ = geometry;
}

QByteArray ArticleInspectorState::geometry() const {
    return saved_;
}

void ArticleInspectorState::CheckpointForExit() {
    if (latest_adjusted_)
        saved_ = *latest_adjusted_;
    exiting_ = true;
}

void ArticleInspectorState::Restore(ArticleInspector* inspector) {
    if (retained_.empty()) {
        const QRect fallback = inspector->geometry();
        if (!saved_.isEmpty() && !inspector->restoreGeometry(saved_))
            inspector->setGeometry(fallback);
    } else {
        inspector->setGeometry(retained_.front()->geometry());
    }
    if (std::find(retained_.begin(), retained_.end(), inspector) ==
        retained_.end())
        retained_.push_back(inspector);
}

void ArticleInspectorState::Forget(ArticleInspector* inspector) {
    retained_.erase(std::remove(retained_.begin(), retained_.end(), inspector),
                    retained_.end());
}

void ArticleInspectorState::Adjusted(ArticleInspector* inspector) {
    if (!exiting_)
        latest_adjusted_ = inspector->saveGeometry();
}

void ArticleInspectorState::Closed(ArticleInspector* inspector) {
    if (exiting_)
        return;
    saved_ = inspector->saveGeometry();
    emit GeometryCaptured();
}

ArticleInspector::ArticleInspector(QWebEnginePage* inspected_page,
                                   std::shared_ptr<ArticleInspectorState> state)
    : inspected_page_(inspected_page),
      state_(state ? std::move(state)
                   : std::make_shared<ArticleInspectorState>()) {
    setObjectName(QStringLiteral("articleInspector"));
    setWindowTitle(tr("Web Inspector"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    profile_ = new QWebEngineProfile(this);
    view_ = new QWebEngineView(this);
    view_->setObjectName(QStringLiteral("articleInspectorContent"));
    view_->setPage(new QWebEnginePage(profile_, view_));
    layout->addWidget(view_);
    // Frozen QWebInspector's initial logical size, independent of the
    // WebEngine view's not-yet-loaded size hint.
    resize(450, 300);
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
    state_->Forget(this);
    if (inspected_page_ && inspected_page_->devToolsPage() == view_->page()) {
        inspected_page_->setDevToolsPage(nullptr);
    }
    // A WebEngine profile must outlive every page that uses it.
    delete view_;
    delete profile_;
}

void ArticleInspector::showEvent(QShowEvent* event) {
    const QScopedValueRollback<bool> restoring(restoring_, true);
    if (!event->spontaneous())
        state_->Restore(this);
    QWidget::showEvent(event);
}

void ArticleInspector::closeEvent(QCloseEvent* event) {
    QWidget::closeEvent(event);
    state_->Closed(this);
}

void ArticleInspector::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    if (isVisible() && !restoring_)
        state_->Adjusted(this);
}

void ArticleInspector::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (isVisible() && !restoring_)
        state_->Adjusted(this);
}

void ArticleInspector::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isVisible() &&
        !restoring_)
        state_->Adjusted(this);
}

QWebEnginePage* ArticleInspector::inspectedPage() const {
    return inspected_page_;
}

void ArticleInspector::Inspect(bool context_target) {
    if (!inspected_page_)
        return;
    const QScopedValueRollback<bool> restoring(restoring_, true);
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
