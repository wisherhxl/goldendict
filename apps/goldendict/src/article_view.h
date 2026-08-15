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
    void SetClickPreferences(bool double_click_translates,
                             bool select_word_by_single_click) noexcept;
    QList<ArticleContextAction> AvailableContextActions(
        const ArticleContext& context) const;
    void TriggerContextActionForTest(ArticleContextAction action,
                                     const ArticleContext& context);
    void TriggerWordQueryForTest(const QPointF& position, bool translate);

   signals:
    void LinkRequested(const QUrl& url, ArticleLinkDisposition disposition);
    void SelectionLookupRequested(const QString& text,
                                  ArticleLinkDisposition disposition);
    void SelectionToInputRequested(const QString& text);
    void ExternalUrlRequested(const QUrl& url);

   protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

   private:
    enum class LinkKind { kNone, kInternalLookup, kExternal };

    LinkKind ClassifyLink(const QUrl& url) const;
    void TriggerContextAction(ArticleContextAction action,
                              const ArticleContext& context);
    void QueryWordAt(const QPointF& position, bool translate);

    const goldendict::core::DesktopFacade* facade_ = nullptr;
    bool double_click_translates_ = true;
    bool select_word_by_single_click_ = false;
    quint64 document_generation_ = 0U;
    quint64 pointer_generation_ = 0U;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_
