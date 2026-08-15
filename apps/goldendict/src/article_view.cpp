// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_view.h"

#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QHash>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QWebEngineContextMenuRequest>
#include <QWebEnginePage>
#include <QWebEngineScript>

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

ArticleView::ArticleView(QWidget* parent) : QWebEngineView(parent) {
    connect(this, &QWebEngineView::loadStarted, this, [this]() {
        ++document_generation_;
        ++pointer_generation_;
        dictionary_context_entries_.clear();
        dictionary_context_overflow_ = false;
    });
}

void ArticleView::SetFacade(
    const goldendict::core::DesktopFacade* facade) noexcept {
    facade_ = facade;
}

void ArticleView::SetClickPreferences(
    bool double_click_translates, bool select_word_by_single_click) noexcept {
    if (double_click_translates_ == double_click_translates &&
        select_word_by_single_click_ == select_word_by_single_click) {
        return;
    }
    double_click_translates_ = double_click_translates;
    select_word_by_single_click_ = select_word_by_single_click;
    ++pointer_generation_;
}

void ArticleView::QueryWordAt(const QPointF& position, bool translate) {
    if (translate ? !double_click_translates_ : !select_word_by_single_click_) {
        return;
    }
    const quint64 pointer_generation = ++pointer_generation_;
    const quint64 document_generation = document_generation_;
    const QString script = QStringLiteral(R"JS(
(() => {
  const x = %1;
  const y = %2;
  if (!Number.isFinite(x) || !Number.isFinite(y)) return null;
  const target = document.elementFromPoint(x, y);
  if (!target || target.closest(
      'a[href],input,textarea,select,option,button,[contenteditable]:not([contenteditable="false"])')) {
    return null;
  }
  let position = null;
  if (document.caretPositionFromPoint) {
    position = document.caretPositionFromPoint(x, y);
  } else if (document.caretRangeFromPoint) {
    const range = document.caretRangeFromPoint(x, y);
    if (range) position = {offsetNode: range.startContainer,
                           offset: range.startOffset};
  }
  if (!position || !position.offsetNode ||
      position.offsetNode.nodeType !== Node.TEXT_NODE) return null;
  const selection = window.getSelection();
  if (!selection) return null;
  selection.removeAllRanges();
  selection.collapse(position.offsetNode, position.offset);
  selection.modify('move', 'backward', 'word');
  selection.modify('extend', 'forward', 'word');
  const word = selection.toString();
  if (!word || word.trim().length === 0 || word.length >= 60) {
    selection.removeAllRanges();
    return null;
  }
  return word;
})()
)JS")
                               .arg(position.x(), 0, 'f', 3)
                               .arg(position.y(), 0, 'f', 3);
    page()->runJavaScript(
        script, QWebEngineScript::ApplicationWorld,
        [guard = QPointer<ArticleView>(this), pointer_generation,
         document_generation, translate](const QVariant& result) {
            if (guard.isNull() ||
                pointer_generation != guard->pointer_generation_ ||
                document_generation != guard->document_generation_) {
                return;
            }
            if (translate ? !guard->double_click_translates_
                          : !guard->select_word_by_single_click_) {
                return;
            }
            const QString word = result.toString();
            if (word.trimmed().isEmpty() || word.size() >= 60)
                return;
            if (translate) {
                emit guard->SelectionLookupRequested(
                    word, ArticleLinkDisposition::kCurrentTab);
            }
        });
}

void ArticleView::mousePressEvent(QMouseEvent* event) {
    QWebEngineView::mousePressEvent(event);
    if (select_word_by_single_click_ && event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::NoModifier) {
        QueryWordAt(event->position(), false);
    }
}

void ArticleView::mouseDoubleClickEvent(QMouseEvent* event) {
    QWebEngineView::mouseDoubleClickEvent(event);
    if (double_click_translates_ && event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::NoModifier) {
        QueryWordAt(event->position(), true);
    }
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

void ArticleView::TriggerWordQueryForTest(const QPointF& position,
                                          bool translate) {
    QueryWordAt(position, translate);
}

void ArticleView::SetDictionaryContextEntries(
    QList<ArticleDictionaryContextEntry> entries, bool overflow,
    quint64 presentation_generation) {
    dictionary_context_entries_ = std::move(entries);
    dictionary_context_overflow_ = overflow;
    dictionary_context_generation_ = presentation_generation;
    ++dictionary_context_revision_;
}

ArticleDictionaryContextSnapshot ArticleView::DictionaryContextSnapshot()
    const {
    return {dictionary_context_entries_, dictionary_context_overflow_,
            dictionary_context_generation_, document_generation_,
            dictionary_context_revision_};
}

bool ArticleView::IsCurrentDictionarySnapshot(
    const ArticleDictionaryContextSnapshot& snapshot) const noexcept {
    return snapshot.document_generation == document_generation_ &&
           snapshot.presentation_generation == dictionary_context_generation_ &&
           snapshot.snapshot_revision == dictionary_context_revision_;
}

void ArticleView::TriggerDictionaryContextAction(
    const ArticleDictionaryContextSnapshot& snapshot, int entry_index) {
    if (!IsCurrentDictionarySnapshot(snapshot) || entry_index < 0 ||
        entry_index >= snapshot.entries.size()) {
        return;
    }
    const auto& entry = snapshot.entries[entry_index];
    emit DictionaryResultRequested(entry.dictionary_id,
                                   entry.first_result_index,
                                   snapshot.presentation_generation);
}

void ArticleView::TriggerDictionaryContextOverflow(
    const ArticleDictionaryContextSnapshot& snapshot) {
    if (IsCurrentDictionarySnapshot(snapshot) && snapshot.overflow)
        emit DictionaryResultsPaneRequested(snapshot.presentation_generation);
}

void ArticleView::TriggerDictionaryContextActionForTest(
    const ArticleDictionaryContextSnapshot& snapshot, int entry_index) {
    TriggerDictionaryContextAction(snapshot, entry_index);
}

void ArticleView::TriggerDictionaryContextOverflowForTest(
    const ArticleDictionaryContextSnapshot& snapshot) {
    TriggerDictionaryContextOverflow(snapshot);
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
    const auto dictionary_snapshot = DictionaryContextSnapshot();
    QMenu menu(this);
    QHash<QAction*, ArticleContextAction> action_map;
    QHash<QAction*, int> dictionary_action_map;
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
    if (!dictionary_snapshot.entries.isEmpty()) {
        if (!menu.isEmpty())
            menu.addSeparator();
        for (int index = 0; index < dictionary_snapshot.entries.size();
             ++index) {
            dictionary_action_map.insert(
                menu.addAction(dictionary_snapshot.entries[index].display_name),
                index);
        }
    }
    QAction* overflow_action = nullptr;
    if (dictionary_snapshot.overflow)
        overflow_action = menu.addAction(QStringLiteral("........."));
    QAction* selected = menu.exec(event->globalPos());
    if (selected == overflow_action) {
        TriggerDictionaryContextOverflow(dictionary_snapshot);
    } else if (dictionary_action_map.contains(selected)) {
        TriggerDictionaryContextAction(dictionary_snapshot,
                                       dictionary_action_map.value(selected));
    } else if (action_map.contains(selected)) {
        TriggerContextAction(action_map.value(selected), context);
    }
}
