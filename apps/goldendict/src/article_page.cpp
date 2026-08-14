// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_page.h"

#include <QApplication>
#include <QUrl>

#include "goldendict/core/desktop_facade.h"

ArticlePage::ArticlePage(QObject* parent) : QWebEnginePage(parent) {}

void ArticlePage::SetFacade(
    const goldendict::core::DesktopFacade* facade) noexcept {
    facade_ = facade;
}

void ArticlePage::SetOpenNewTabsInBackground(bool enabled) noexcept {
    open_new_tabs_in_background_ = enabled;
}

QWebEnginePage* ArticlePage::createWindow(WebWindowType type) {
    static_cast<void>(type);
    // Modified article-link clicks must still pass through this page's
    // navigation policy so they can become application tab commands.
    new_window_navigation_pending_ = true;
    return this;
}

bool ArticlePage::acceptNavigationRequest(const QUrl& url, NavigationType type,
                                          bool is_main_frame) {
    const bool requested_new_window = new_window_navigation_pending_;
    const bool requested_by_article_link =
        type == QWebEnginePage::NavigationTypeLinkClicked ||
        requested_new_window;
    new_window_navigation_pending_ = false;
    if (!requested_by_article_link) {
        return QWebEnginePage::acceptNavigationRequest(url, type,
                                                       is_main_frame);
    }
    if (facade_ == nullptr) {
        return false;
    }
    const auto resolved =
        facade_->ResolveArticleUrl(url.toEncoded().toStdString());
    if (resolved.has_value() &&
        resolved->kind == goldendict::core::ArticleUrlKind::kLookup) {
        const Qt::KeyboardModifiers modifiers =
            QApplication::keyboardModifiers();
        const bool middle_clicked =
            QApplication::mouseButtons().testFlag(Qt::MiddleButton);
        ArticleLinkDisposition disposition =
            requested_new_window
                ? (open_new_tabs_in_background_
                       ? ArticleLinkDisposition::kNewBackgroundTab
                       : ArticleLinkDisposition::kNewForegroundTab)
                : ArticleLinkDisposition::kCurrentTab;
        if (modifiers.testFlag(Qt::ShiftModifier)) {
            disposition = ArticleLinkDisposition::kNewForegroundTab;
        } else if (middle_clicked || modifiers.testFlag(Qt::ControlModifier)) {
            disposition = ArticleLinkDisposition::kNewBackgroundTab;
        }
        emit LookupRequested(QString::fromStdString(resolved->lookup_text),
                             QString::fromLatin1(url.toEncoded()), disposition);
    } else if (!resolved.has_value() &&
               (url.scheme() == QStringLiteral("http") ||
                url.scheme() == QStringLiteral("https") ||
                url.scheme() == QStringLiteral("mailto"))) {
        emit ExternalUrlRequested(url);
    }
    // Article links never navigate the embedded browser. Internal lookup links
    // become application commands, while explicitly allowed external schemes
    // are handed to the desktop integration layer. Everything else is denied.
    return false;
}
