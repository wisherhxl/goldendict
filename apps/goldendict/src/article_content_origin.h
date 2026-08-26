// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_CONTENT_ORIGIN_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_CONTENT_ORIGIN_H_

#include <QUrl>

namespace goldendict::app {

inline QUrl ArticleContentBaseUrl() {
    // An empty setHtml() base has an opaque origin and cannot load resources
    // from the local goldendict scheme. The assembled article CSP still limits
    // resource URLs to the explicitly allowed internal types.
    return QUrl::fromLocalFile(QStringLiteral("/goldendict/article/"));
}

}  // namespace goldendict::app

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_CONTENT_ORIGIN_H_
