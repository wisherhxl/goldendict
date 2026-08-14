// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_view.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QHash>
#include <QMenu>
#include <QWebEngineContextMenuRequest>
#include <QWebEnginePage>

#include "goldendict/core/desktop_facade.h"

namespace {

QString DisplaySelection(QString text) {
    text = text.trimmed();
    if (text.isRightToLeft()) {
        text.prepend(QChar(0x202e));
        text.append(QChar(0x202c));
    }
    return text;
}

}  // namespace

ArticleView::ArticleView(QWidget* parent) : QWebEngineView(parent) {}

void ArticleView::SetFacade(
    const goldendict::core::DesktopFacade* facade) noexcept {
    facade_ = facade;
}

ArticleView::LinkKind ArticleView::ClassifyLink(const QUrl& url) const {
    if (url.isEmpty() || facade_ == nullptr)
        return LinkKind::kNone;
    const auto resolved =
        facade_->ResolveArticleUrl(url.toEncoded().toStdString());
    if (resolved.has_value() &&
        resolved->kind == goldendict::core::ArticleUrlKind::kLookup) {
        return LinkKind::kInternalLookup;
    }
    if (!resolved.has_value() && url.userInfo().isEmpty() &&
        (url.scheme() == QStringLiteral("http") ||
         url.scheme() == QStringLiteral("https") ||
         url.scheme() == QStringLiteral("mailto"))) {
        return LinkKind::kExternal;
    }
    return LinkKind::kNone;
}

QList<ArticleContextAction> ArticleView::AvailableContextActions(
    const ArticleContext& context) const {
    QList<ArticleContextAction> actions;
    switch (ClassifyLink(context.link_url)) {
        case LinkKind::kInternalLookup:
            actions << ArticleContextAction::kOpenLink
                    << ArticleContextAction::kOpenLinkInNewTab;
            break;
        case LinkKind::kExternal:
            actions << ArticleContextAction::kOpenExternalLink
                    << ArticleContextAction::kCopyLink;
            break;
        case LinkKind::kNone:
            break;
    }

    const QString trimmed = context.selected_text.trimmed();
    if (!trimmed.isEmpty() && trimmed.size() < 60) {
        actions << ArticleContextAction::kLookupSelection
                << ArticleContextAction::kLookupSelectionInNewTab
                << ArticleContextAction::kSendSelectionToInput;
    }
    if (!context.selected_text.isEmpty()) {
        actions << ArticleContextAction::kCopy
                << ArticleContextAction::kCopyAsText;
    } else {
        actions << ArticleContextAction::kSelectAll;
    }
    if (context.has_image_content)
        actions << ArticleContextAction::kCopyImage;
    return actions;
}

void ArticleView::TriggerContextAction(ArticleContextAction action,
                                       const ArticleContext& context) {
    switch (action) {
        case ArticleContextAction::kOpenLink:
            emit LinkRequested(context.link_url,
                               ArticleLinkDisposition::kCurrentTab);
            break;
        case ArticleContextAction::kOpenLinkInNewTab:
            emit LinkRequested(context.link_url,
                               ArticleLinkDisposition::kNewForegroundTab);
            break;
        case ArticleContextAction::kOpenExternalLink:
            emit ExternalUrlRequested(context.link_url);
            break;
        case ArticleContextAction::kCopyLink:
            QApplication::clipboard()->setText(context.link_url.toString());
            break;
        case ArticleContextAction::kLookupSelection:
            emit SelectionLookupRequested(context.selected_text,
                                          ArticleLinkDisposition::kCurrentTab);
            break;
        case ArticleContextAction::kLookupSelectionInNewTab:
            emit SelectionLookupRequested(
                context.selected_text,
                ArticleLinkDisposition::kNewForegroundTab);
            break;
        case ArticleContextAction::kSendSelectionToInput:
            emit SelectionToInputRequested(context.selected_text);
            break;
        case ArticleContextAction::kCopy:
            page()->triggerAction(QWebEnginePage::Copy);
            break;
        case ArticleContextAction::kCopyAsText:
            QApplication::clipboard()->setText(context.selected_text);
            break;
        case ArticleContextAction::kCopyImage:
            page()->triggerAction(QWebEnginePage::CopyImageToClipboard);
            break;
        case ArticleContextAction::kSelectAll:
            page()->triggerAction(QWebEnginePage::SelectAll);
            break;
    }
}

void ArticleView::TriggerContextActionForTest(ArticleContextAction action,
                                              const ArticleContext& context) {
    if (AvailableContextActions(context).contains(action))
        TriggerContextAction(action, context);
}

void ArticleView::contextMenuEvent(QContextMenuEvent* event) {
    auto* request = lastContextMenuRequest();
    if (request == nullptr)
        return;
    request->setAccepted(true);
    const ArticleContext context{
        request->selectedText(), request->linkUrl(),
        request->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage &&
            !request->mediaUrl().isEmpty()};
    const auto available = AvailableContextActions(context);
    QMenu menu(this);
    QHash<QAction*, ArticleContextAction> action_map;
    const QString display_selection = DisplaySelection(context.selected_text);
    for (const auto action : available) {
        QString label;
        switch (action) {
            case ArticleContextAction::kOpenLink:
                label = tr("&Open Link");
                break;
            case ArticleContextAction::kOpenLinkInNewTab:
                label = tr("Open Link in New &Tab");
                break;
            case ArticleContextAction::kOpenExternalLink:
                label = tr("Open Link in &External Browser");
                break;
            case ArticleContextAction::kCopyLink:
                label = tr("Copy Link");
                break;
            case ArticleContextAction::kLookupSelection:
                label = tr("&Look up \"%1\"").arg(display_selection);
                break;
            case ArticleContextAction::kLookupSelectionInNewTab:
                label = tr("Look up \"%1\" in &New Tab").arg(display_selection);
                break;
            case ArticleContextAction::kSendSelectionToInput:
                label = tr("Send \"%1\" to input line").arg(display_selection);
                break;
            case ArticleContextAction::kCopy:
                label = tr("Copy");
                break;
            case ArticleContextAction::kCopyAsText:
                label = tr("Copy as Text");
                break;
            case ArticleContextAction::kCopyImage:
                label = tr("Copy Image");
                break;
            case ArticleContextAction::kSelectAll:
                label = tr("Select All");
                break;
        }
        action_map.insert(menu.addAction(label), action);
    }
    QAction* selected = menu.exec(event->globalPos());
    if (selected != nullptr)
        TriggerContextAction(action_map.value(selected), context);
}
