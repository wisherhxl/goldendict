// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_

#include <QUrl>
#include <QWebEngineView>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

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

struct ArticleDictionaryContextEntry {
    QString dictionary_id;
    QString display_name;
    int first_result_index = 0;
};

struct ArticleDictionaryContextSnapshot {
    QList<ArticleDictionaryContextEntry> entries;
    bool overflow = false;
    quint64 presentation_generation = 0U;
    quint64 document_generation = 0U;
    quint64 snapshot_revision = 0U;
};

struct ArticleContext {
    QString selected_text;
    QUrl link_url;
    bool has_image_content = false;
};

struct ArticleHighlightRange {
    std::size_t byte_offset = 0U;
    std::size_t byte_length = 0U;
    QString literal;
};

struct ArticleHighlightResult {
    QString token;
    bool applied = false;
    int occurrence_count = 0;
    int ordered_count = 0;
    int current_position = -1;
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
    void SetDictionaryContextEntries(
        QList<ArticleDictionaryContextEntry> entries, bool overflow,
        quint64 presentation_generation);
    ArticleDictionaryContextSnapshot DictionaryContextSnapshot() const;
    void TriggerDictionaryContextActionForTest(
        const ArticleDictionaryContextSnapshot& snapshot, int entry_index);
    void TriggerDictionaryContextOverflowForTest(
        const ArticleDictionaryContextSnapshot& snapshot);
    void ApplyFullTextHighlights(
        const QString& token, const QString& rendered_text,
        const std::vector<ArticleHighlightRange>& ordered_ranges,
        bool match_case,
        std::function<void(ArticleHighlightResult)> completion);
    void ClearFullTextHighlights(const QString& expected_token,
                                 bool clear_current_owner = false);

   signals:
    void LinkRequested(const QUrl& url, ArticleLinkDisposition disposition);
    void SelectionLookupRequested(const QString& text,
                                  ArticleLinkDisposition disposition);
    void SelectionToInputRequested(const QString& text);
    void ExternalUrlRequested(const QUrl& url);
    void DictionaryResultRequested(const QString& dictionary_id,
                                   int first_result_index,
                                   quint64 presentation_generation);
    void DictionaryResultsPaneRequested(quint64 presentation_generation);

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
    bool IsCurrentDictionarySnapshot(
        const ArticleDictionaryContextSnapshot& snapshot) const noexcept;
    void TriggerDictionaryContextAction(
        const ArticleDictionaryContextSnapshot& snapshot, int entry_index);
    void TriggerDictionaryContextOverflow(
        const ArticleDictionaryContextSnapshot& snapshot);

    const goldendict::core::DesktopFacade* facade_ = nullptr;
    bool double_click_translates_ = true;
    bool select_word_by_single_click_ = false;
    quint64 document_generation_ = 0U;
    quint64 pointer_generation_ = 0U;
    QList<ArticleDictionaryContextEntry> dictionary_context_entries_;
    bool dictionary_context_overflow_ = false;
    quint64 dictionary_context_generation_ = 0U;
    quint64 dictionary_context_revision_ = 0U;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_
