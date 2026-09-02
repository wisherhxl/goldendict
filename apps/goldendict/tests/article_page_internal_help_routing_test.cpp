// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QByteArray>
#include <QUrl>
#include <QWebEngineUrlScheme>

#include "article_page.h"

class ArticlePageInternalHelpRoutingTestAccess {
   public:
    static bool Request(ArticlePage& page, const QUrl& url) {
        return page.acceptNavigationRequest(
            url, QWebEnginePage::NavigationTypeLinkClicked, true);
    }

    static void RequestNewWindow(ArticlePage& page) {
        page.createWindow(QWebEnginePage::WebBrowserTab);
    }
};

namespace {

void RegisterArticleScheme() {
    QWebEngineUrlScheme scheme(QByteArrayLiteral("goldendict"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(0);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                    QWebEngineUrlScheme::LocalScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
}

}  // namespace

int main(int argc, char* argv[]) {
    RegisterArticleScheme();
    QApplication application(argc, argv);

    ArticlePage page;
    int request_count = 0;
    QUrl requested_url;
    ArticleLinkDisposition requested_disposition =
        ArticleLinkDisposition::kNewBackgroundTab;
    QObject::connect(
        &page, &ArticlePage::InternalHelpRequested, &page,
        [&](const QUrl& url, ArticleLinkDisposition disposition) {
            ++request_count;
            requested_url = url;
            requested_disposition = disposition;
        });

    const QUrl help_url(
        QStringLiteral("goldendict://help/working-with-popup"));
    if (ArticlePageInternalHelpRoutingTestAccess::Request(page, help_url) ||
        request_count != 1 || requested_url != help_url ||
        requested_disposition != ArticleLinkDisposition::kCurrentTab) {
        return 1;
    }

    const QUrl unsupported_url(
        QStringLiteral("goldendict://help/working-with-popup?unexpected=1"));
    if (ArticlePageInternalHelpRoutingTestAccess::Request(page,
                                                         unsupported_url) ||
        request_count != 1) {
        return 1;
    }

    page.SetOpenNewTabsInBackground(false);
    ArticlePageInternalHelpRoutingTestAccess::RequestNewWindow(page);
    if (ArticlePageInternalHelpRoutingTestAccess::Request(page, help_url) ||
        request_count != 2 ||
        requested_disposition != ArticleLinkDisposition::kNewForegroundTab) {
        return 1;
    }
    return 0;
}
