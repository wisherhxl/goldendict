// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_

#include <QUrl>
#include <QWebEnginePage>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "article_page.h"

namespace goldendict::core {
class DesktopFacade;
}

class ArticleWebView;
class QPrinter;

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

enum class ArticleHighlightNavigationDirection { kPrevious, kNext };

struct ArticleHighlightNavigationSnapshot {
    bool accepted = false;
    QString token;
    int current_position = -1;
    int ordered_count = 0;
    bool can_previous = false;
    bool can_next = false;
};

class ArticleView final : public QWidget {
    Q_OBJECT

   public:
    explicit ArticleView(QWidget* parent = nullptr);

    QWebEnginePage* page() const;
    void setPage(QWebEnginePage* page);
    void setHtml(const QString& html, const QUrl& base_url = QUrl());
    quint64 ReserveHtmlNavigation();
    void SetHtmlNavigation(quint64 navigation_token, const QString& html,
                           const QUrl& base_url = QUrl());
    void reload();
    void findText(const QString& text, QWebEnginePage::FindFlags options = {},
                  const std::function<void(const QWebEngineFindTextResult&)>&
                      callback = {});
    void setZoomFactor(qreal factor);
    qreal zoomFactor() const;
    void print(QPrinter* printer);
    void printToPdf(
        const std::function<void(const QByteArray&)>& result_callback) const;
    void printToPdf(const QString& file_path) const;
    void setFocus(Qt::FocusReason reason = Qt::OtherFocusReason);
    bool hasFocus() const;

    void SetFacade(const goldendict::core::DesktopFacade* facade) noexcept;
    void SetClickPreferences(bool double_click_translates,
                             bool select_word_by_single_click) noexcept;
    QList<ArticleContextAction> AvailableContextActions(
        const ArticleContext& context) const;
    void TriggerContextActionForTest(ArticleContextAction action,
                                     const ArticleContext& context);
    void TriggerWordQueryForTest(const QPointF& position, bool translate,
                                 std::function<void()> completion = {});
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
    void NavigateFullTextHighlight(
        const QString& expected_token,
        ArticleHighlightNavigationDirection direction,
        std::function<void(ArticleHighlightNavigationSnapshot)> completion);

   signals:
    void loadStarted();
    void loadFinished(bool success);
    void HtmlNavigationFinished(quint64 navigation_token, bool success);
    void PageReplaced();
    void urlChanged(const QUrl& url);
    void pdfPrintingFinished(const QString& file_path, bool success);
    void printFinished(bool success);
    void FullTextNavigationRequested(
        ArticleHighlightNavigationDirection direction);
    void LinkRequested(const QUrl& url, ArticleLinkDisposition disposition);
    void SelectionLookupRequested(const QString& text,
                                  ArticleLinkDisposition disposition);
    void SelectionToInputRequested(const QString& text);
    void ExternalUrlRequested(const QUrl& url);
    void DictionaryResultRequested(const QString& dictionary_id,
                                   int first_result_index,
                                   quint64 presentation_generation);
    void DictionaryResultsPaneRequested(quint64 presentation_generation);

   private:
    friend class MainWindow;
    friend class ArticleWebView;

    enum class LinkKind { kNone, kInternalLookup, kExternal };

    LinkKind ClassifyLink(const QUrl& url) const;
    void TriggerContextAction(ArticleContextAction action,
                              const ArticleContext& context);
    void QueryWordAt(const QPointF& position, bool translate,
                     std::function<void()> completion = {});
    bool IsCurrentDictionarySnapshot(
        const ArticleDictionaryContextSnapshot& snapshot) const noexcept;
    void TriggerDictionaryContextAction(
        const ArticleDictionaryContextSnapshot& snapshot, int entry_index);
    void TriggerDictionaryContextOverflow(
        const ArticleDictionaryContextSnapshot& snapshot);
    void HandleContextMenuEvent(QContextMenuEvent* event);
    void HandleMousePressEvent(QMouseEvent* event);
    void HandleMouseDoubleClickEvent(QMouseEvent* event);
    void PublishFullTextNavigationSnapshot(
        const ArticleHighlightNavigationSnapshot& snapshot);
    void ClearFullTextNavigation(const QString& expected_token = {},
                                 bool force = false);

    ArticleWebView* web_view_ = nullptr;
    QWidget* full_text_navigation_row_ = nullptr;
    class QPushButton* full_text_previous_ = nullptr;
    class QPushButton* full_text_next_ = nullptr;
    class QLabel* full_text_status_ = nullptr;
    QString full_text_navigation_token_;

    const goldendict::core::DesktopFacade* facade_ = nullptr;
    bool double_click_translates_ = true;
    bool select_word_by_single_click_ = false;
    quint64 document_generation_ = 0U;
    quint64 pointer_generation_ = 0U;
    quint64 next_html_navigation_token_ = 0U;
    QMetaObject::Connection page_loading_connection_;
    QList<ArticleDictionaryContextEntry> dictionary_context_entries_;
    bool dictionary_context_overflow_ = false;
    quint64 dictionary_context_generation_ = 0U;
    quint64 dictionary_context_revision_ = 0U;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_VIEW_H_
