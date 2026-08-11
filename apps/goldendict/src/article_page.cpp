// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_page.h"

#include <QUrl>

#include "goldendict/core/desktop_facade.h"

ArticlePage::ArticlePage(QObject* parent) : QWebEnginePage(parent) {}

void ArticlePage::SetFacade(
    const goldendict::core::DesktopFacade* facade) noexcept {
    facade_ = facade;
}

bool ArticlePage::acceptNavigationRequest(const QUrl& url, NavigationType type,
                                          bool is_main_frame) {
    if (type != QWebEnginePage::NavigationTypeLinkClicked) {
        return QWebEnginePage::acceptNavigationRequest(url, type,
                                                       is_main_frame);
    }
    if (facade_ == nullptr) {
        return false;
    }
    const auto resolved =
        facade_->ResolveArticleUrl(url.toString().toStdString());
    if (resolved.has_value() &&
        resolved->kind == goldendict::core::ArticleUrlKind::kLookup) {
        emit LookupRequested(QString::fromStdString(resolved->lookup_text));
    }
    // Article links never navigate the embedded browser. Internal lookup links
    // become application commands; external links are denied by policy.
    return false;
}
