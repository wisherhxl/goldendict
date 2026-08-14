// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_

#include <QUrl>
#include <QWebEngineView>

#include "article_page.h"

namespace goldendict::core {
class DesktopFacade;
}

enum class ArticleContextAction {
    kOpenLink,
    kOpenLinkInNewTab,
    kOpenExternalLink,
    kCopyLink,
    kLookupSelection,
    kLookupSelectionInNewTab,
    kSendSelectionToInput,
    kCopy,
    kCopyAsText,
    kCopyImage,
    kSelectAll,
};

struct ArticleContext {
    QString selected_text;
    QUrl link_url;
    bool has_image_content = false;
};

class ArticleView final : public QWebEngineView {
    Q_OBJECT

   public:
    explicit ArticleView(QWidget* parent = nullptr);

    void SetFacade(const goldendict::core::DesktopFacade* facade) noexcept;
    QList<ArticleContextAction> AvailableContextActions(
        const ArticleContext& context) const;
    void TriggerContextActionForTest(ArticleContextAction action,
                                     const ArticleContext& context);

   signals:
    void LinkRequested(const QUrl& url, ArticleLinkDisposition disposition);
    void SelectionLookupRequested(const QString& text,
                                  ArticleLinkDisposition disposition);
    void SelectionToInputRequested(const QString& text);
    void ExternalUrlRequested(const QUrl& url);

   protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

   private:
    enum class LinkKind { kNone, kInternalLookup, kExternal };

    LinkKind ClassifyLink(const QUrl& url) const;
    void TriggerContextAction(ArticleContextAction action,
                              const ArticleContext& context);

    const goldendict::core::DesktopFacade* facade_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_
