// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_view.h"

#include "article_content_origin.h"

#include <utility>

#include <cmath>
#include <limits>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineLoadingInfo>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineView>

#include "goldendict/core/desktop_facade.h"

namespace {

constexpr auto kFullTextHighlightName = "goldendict-full-text-match";

QString DisplaySelection(QString text) {
    text = text.trimmed();
    if (text.isRightToLeft()) {
        text.prepend(QChar(0x202e));
        text.append(QChar(0x202c));
    }
    return text;
}

bool ReadJavaScriptInteger(const QVariantMap& result, const QString& key,
                           int* output) {
    if (!result.contains(key))
        return false;
    const QVariant value = result.value(key);
    bool converted = false;
    const double number = value.toDouble(&converted);
    if (!converted || value.metaType().id() == QMetaType::Bool ||
        value.metaType().id() == QMetaType::QString || !std::isfinite(number) ||
        std::trunc(number) != number ||
        number < static_cast<double>(std::numeric_limits<int>::min()) ||
        number > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    *output = static_cast<int>(number);
    return true;
}

}  // namespace

class ArticleWebView final : public QWebEngineView {
   public:
    explicit ArticleWebView(ArticleView* owner)
        : QWebEngineView(owner), owner_(owner) {}

   protected:
    void contextMenuEvent(QContextMenuEvent* event) override {
        owner_->HandleContextMenuEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        QWebEngineView::mousePressEvent(event);
        owner_->HandleMousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        QWebEngineView::mouseDoubleClickEvent(event);
        owner_->HandleMouseDoubleClickEvent(event);
    }

   private:
    ArticleView* owner_;
};

void ArticleView::ApplyFullTextHighlights(
    const QString& token, const QString& rendered_text,
    const std::vector<ArticleHighlightRange>& ordered_ranges, bool match_case,
    std::function<void(ArticleHighlightResult)> completion) {
    if (page() == nullptr || token.isEmpty()) {
        completion({token});
        return;
    }
    QJsonArray ranges;
    for (const auto& range : ordered_ranges) {
        ranges.append(QJsonObject{
            {QStringLiteral("offset"),
             QString::number(static_cast<qulonglong>(range.byte_offset))},
            {QStringLiteral("length"),
             QString::number(static_cast<qulonglong>(range.byte_length))},
            {QStringLiteral("literal"), range.literal}});
    }
    const QJsonObject payload{{QStringLiteral("token"), token},
                              {QStringLiteral("text"), rendered_text},
                              {QStringLiteral("matchCase"), match_case},
                              {QStringLiteral("ranges"), ranges}};
    const QString encoded = QString::fromLatin1(
        QJsonDocument(payload).toJson(QJsonDocument::Compact).toBase64());
    const QString script =
        QStringLiteral(R"JS(
(() => {
  const payload = JSON.parse(new TextDecoder().decode(
      Uint8Array.from(atob('%1'), c => c.charCodeAt(0))));
  const token = payload.token;
  const name = '%2';
  const stateKey = '__goldendictFullTextHighlightState';
  const prior = globalThis[stateKey] || {published: null};
  globalThis[stateKey] = prior;
  const fail = () => ({token, applied: false, occurrenceCount: 0,
                        orderedCount: 0, currentPosition: -1});
  try {
    if (!globalThis.CSS || !CSS.highlights ||
        typeof globalThis.Highlight !== 'function' ||
        typeof globalThis.Range !== 'function' ||
        typeof globalThis.TextEncoder !== 'function' ||
        typeof globalThis.NodeFilter === 'undefined' ||
        !document.createTreeWalker || !window.getSelection ||
        typeof globalThis.CSSStyleSheet !== 'function' ||
        !('adoptedStyleSheets' in document)) return fail();

    const nodes = [];
    let domText = '';
    const walker = document.createTreeWalker(
        document.body || document.documentElement, NodeFilter.SHOW_TEXT);
    for (let node = walker.nextNode(); node; node = walker.nextNode()) {
      nodes.push({node, start: domText.length, end: domText.length + node.data.length});
      domText += node.data;
    }
    const encoder = new TextEncoder();
    const byteToCodeUnit = byteText => {
      const wanted = BigInt(byteText);
      if (wanted < 0n) return null;
      let bytes = 0n;
      for (let index = 0; index <= payload.text.length; ++index) {
        if (bytes === wanted) return index;
        if (index === payload.text.length) break;
        const code = payload.text.codePointAt(index);
        const width = code > 0xffff ? 2 : 1;
        bytes += BigInt(encoder.encode(payload.text.slice(index, index + width)).length);
        index += width - 1;
      }
      return null;
    };
    const boundary = offset => {
      if (offset === domText.length && nodes.length)
        return [nodes[nodes.length - 1].node, nodes[nodes.length - 1].node.data.length];
      const found = nodes.find(item => offset >= item.start && offset < item.end);
      return found ? [found.node, offset - found.start] : null;
    };
    const makeRange = (start, end) => {
      const first = boundary(start);
      const last = boundary(end);
      if (!first || !last) return null;
      const range = new Range();
      range.setStart(first[0], first[1]);
      range.setEnd(last[0], last[1]);
      return range;
    };

    const ordered = [];
    for (const supplied of payload.ranges) {
      const start = byteToCodeUnit(supplied.offset);
      const length = BigInt(supplied.length);
      const end = start === null ? null : byteToCodeUnit(
          (BigInt(supplied.offset) + length).toString());
      if (start === null || end === null || end <= start ||
          payload.text.slice(start, end) !== supplied.literal) return fail();
      const renderedHaystack = payload.matchCase ? payload.text : payload.text.toLowerCase();
      const domHaystack = payload.matchCase ? domText : domText.toLowerCase();
      const needle = payload.matchCase ? supplied.literal : supplied.literal.toLowerCase();
      let ordinal = 0;
      for (let found = renderedHaystack.indexOf(needle); found !== -1 && found < start;
           found = renderedHaystack.indexOf(needle, found + Math.max(needle.length, 1))) {
        ++ordinal;
      }
      let domStart = -1;
      for (let index = 0; index <= ordinal; ++index) {
        domStart = domHaystack.indexOf(needle, domStart + 1);
        if (domStart === -1) return fail();
      }
      const range = makeRange(domStart, domStart + supplied.literal.length);
      const mapped = range ? range.toString() : '';
      if (!range || (payload.matchCase ? mapped !== supplied.literal
                                      : mapped.toLowerCase() !== needle)) return fail();
      ordered.push(range);
    }

    if (!ordered.length) {
      if (prior.published && prior.published.token === token) {
        if (CSS.highlights.get(name) === prior.published.highlight)
          CSS.highlights.delete(name);
        document.adoptedStyleSheets = document.adoptedStyleSheets.filter(
            sheet => sheet !== prior.published.sheet);
        prior.published = null;
      }
      return {token, applied: true, occurrenceCount: 0,
              orderedCount: 0, currentPosition: -1};
    }

    const literals = [];
    const keys = new Set();
    for (const supplied of payload.ranges) {
      const key = payload.matchCase ? supplied.literal : supplied.literal.toLowerCase();
      if (!keys.has(key)) { keys.add(key); literals.push(supplied.literal); }
    }
    const occurrenceRanges = [];
    const haystack = payload.matchCase ? domText : domText.toLowerCase();
    for (const literal of literals) {
      const needle = payload.matchCase ? literal : literal.toLowerCase();
      if (!needle.length) return fail();
      for (let start = 0; (start = haystack.indexOf(needle, start)) !== -1;
           start += Math.max(needle.length, 1)) {
        const range = makeRange(start, start + literal.length);
        if (!range) return fail();
        occurrenceRanges.push(range);
      }
    }
    const highlight = new Highlight(...occurrenceRanges);
    const sheet = new CSSStyleSheet();
    sheet.replaceSync('::highlight(' + name +
        ') { background-color: Highlight; color: HighlightText; }');
    const selection = window.getSelection();
    if (!selection) return fail();

    const published = {token, highlight, sheet, ordered, position: 0};
    const old = prior.published;
    CSS.highlights.set(name, highlight);
    document.adoptedStyleSheets = document.adoptedStyleSheets
        .filter(candidate => !old || candidate !== old.sheet).concat(sheet);
    selection.removeAllRanges();
    selection.addRange(ordered[0]);
    const rect = ordered[0].getBoundingClientRect();
    window.scrollTo({top: window.scrollY + rect.top,
                     left: window.scrollX + rect.left, behavior: 'instant'});
    prior.published = published;
    return {token, applied: true, occurrenceCount: occurrenceRanges.length,
            orderedCount: ordered.length, currentPosition: 0};
  } catch (_) {
    return fail();
  }
})()
)JS")
            .arg(encoded, QString::fromLatin1(kFullTextHighlightName));
    page()->runJavaScript(
        script, QWebEngineScript::ApplicationWorld,
        [token,
         completion = std::move(completion)](const QVariant& value) mutable {
            const QVariantMap result = value.toMap();
            ArticleHighlightResult applied;
            applied.token = result.value(QStringLiteral("token")).toString();
            applied.applied = result.value(QStringLiteral("applied")).toBool();
            applied.occurrence_count =
                result.value(QStringLiteral("occurrenceCount"), -1).toInt();
            applied.ordered_count =
                result.value(QStringLiteral("orderedCount"), -1).toInt();
            applied.current_position =
                result.value(QStringLiteral("currentPosition"), -1).toInt();
            if (applied.token.isEmpty())
                applied.token = token;
            completion(std::move(applied));
        });
}

void ArticleView::ClearFullTextHighlights(const QString& expected_token,
                                          bool clear_current_owner) {
    if (page() == nullptr)
        return;
    const QJsonObject payload{{QStringLiteral("token"), expected_token},
                              {QStringLiteral("force"), clear_current_owner}};
    const QString encoded = QString::fromLatin1(
        QJsonDocument(payload).toJson(QJsonDocument::Compact).toBase64());
    const QString script =
        QStringLiteral(R"JS(
(() => {
  const request = JSON.parse(new TextDecoder().decode(
      Uint8Array.from(atob('%1'), c => c.charCodeAt(0))));
  const state = globalThis.__goldendictFullTextHighlightState;
  const published = state && state.published;
  if (!published || (!request.force && published.token !== request.token)) return false;
  if (CSS.highlights.get('%2') === published.highlight)
    CSS.highlights.delete('%2');
  document.adoptedStyleSheets = document.adoptedStyleSheets.filter(
      sheet => sheet !== published.sheet);
  const selection = window.getSelection();
  if (selection && selection.rangeCount && published.ordered.some(range =>
      range.compareBoundaryPoints(Range.START_TO_START, selection.getRangeAt(0)) === 0 &&
      range.compareBoundaryPoints(Range.END_TO_END, selection.getRangeAt(0)) === 0)) {
    selection.removeAllRanges();
  }
  state.published = null;
  return true;
})()
)JS")
            .arg(encoded, QString::fromLatin1(kFullTextHighlightName));
    page()->runJavaScript(script, QWebEngineScript::ApplicationWorld);
}

void ArticleView::NavigateFullTextHighlight(
    const QString& expected_token,
    ArticleHighlightNavigationDirection direction,
    std::function<void(ArticleHighlightNavigationSnapshot)> completion) {
    ArticleHighlightNavigationSnapshot rejected;
    rejected.token = expected_token;
    if (page() == nullptr || expected_token.isEmpty()) {
        completion(std::move(rejected));
        return;
    }
    const QJsonObject payload{
        {QStringLiteral("token"), expected_token},
        {QStringLiteral("delta"),
         direction == ArticleHighlightNavigationDirection::kPrevious ? -1 : 1}};
    const QString encoded = QString::fromLatin1(
        QJsonDocument(payload).toJson(QJsonDocument::Compact).toBase64());
    const QString script =
        QStringLiteral(R"JS(
(() => {
  const request = JSON.parse(new TextDecoder().decode(
      Uint8Array.from(atob('%1'), c => c.charCodeAt(0))));
  const reject = () => ({accepted: false, token: request.token,
      currentPosition: -1, orderedCount: 0,
      canPrevious: false, canNext: false});
  try {
    const state = globalThis.__goldendictFullTextHighlightState;
    const published = state && state.published;
    if ((request.delta !== -1 && request.delta !== 1) ||
        !published || published.token !== request.token ||
        !Array.isArray(published.ordered) || !published.ordered.length ||
        !Number.isInteger(published.position) || published.position < 0 ||
        published.position >= published.ordered.length ||
        !published.ordered.every(range => range instanceof Range) ||
        !globalThis.CSS || !CSS.highlights ||
        CSS.highlights.get('%2') !== published.highlight ||
        !('adoptedStyleSheets' in document) ||
        !Array.from(document.adoptedStyleSheets).includes(published.sheet) ||
        !window.getSelection || typeof window.scrollTo !== 'function') return reject();
    const count = published.ordered.length;
    const snapshot = () => ({accepted: true, token: published.token,
        currentPosition: published.position, orderedCount: count,
        canPrevious: published.position > 0,
        canNext: published.position + 1 < count});
    const targetPosition = published.position + request.delta;
    if (targetPosition < 0 || targetPosition >= count) return snapshot();
    const target = published.ordered[targetPosition];
    const rect = target.getBoundingClientRect();
    if (!rect || !Number.isFinite(rect.top) || !Number.isFinite(rect.left))
      return reject();
    const selection = window.getSelection();
    if (!selection || typeof selection.removeAllRanges !== 'function' ||
        typeof selection.addRange !== 'function') return reject();
    const oldRanges = [];
    for (let index = 0; index < selection.rangeCount; ++index)
      oldRanges.push(selection.getRangeAt(index).cloneRange());
    const oldX = window.scrollX;
    const oldY = window.scrollY;
    try {
      selection.removeAllRanges();
      selection.addRange(target);
      window.scrollTo({top: oldY + rect.top, left: oldX + rect.left,
                       behavior: 'instant'});
    } catch (_) {
      try {
        selection.removeAllRanges();
        for (const range of oldRanges) selection.addRange(range);
        window.scrollTo({top: oldY, left: oldX, behavior: 'instant'});
      } catch (_) {}
      return reject();
    }
    published.position = targetPosition;
    return snapshot();
  } catch (_) {
    return reject();
  }
})()
)JS")
            .arg(encoded, QString::fromLatin1(kFullTextHighlightName));
    page()->runJavaScript(
        script, QWebEngineScript::ApplicationWorld,
        [expected_token,
         completion = std::move(completion)](const QVariant& value) mutable {
            ArticleHighlightNavigationSnapshot snapshot;
            snapshot.token = expected_token;
            if (value.metaType().id() != QMetaType::QVariantMap) {
                completion(std::move(snapshot));
                return;
            }
            const QVariantMap result = value.toMap();
            const QVariant accepted = result.value(QStringLiteral("accepted"));
            const QVariant token = result.value(QStringLiteral("token"));
            const QVariant can_previous =
                result.value(QStringLiteral("canPrevious"));
            const QVariant can_next = result.value(QStringLiteral("canNext"));
            int position = -1;
            int count = 0;
            if (accepted.metaType().id() != QMetaType::Bool ||
                token.metaType().id() != QMetaType::QString ||
                can_previous.metaType().id() != QMetaType::Bool ||
                can_next.metaType().id() != QMetaType::Bool ||
                !ReadJavaScriptInteger(
                    result, QStringLiteral("currentPosition"), &position) ||
                !ReadJavaScriptInteger(result, QStringLiteral("orderedCount"),
                                       &count)) {
                completion(std::move(snapshot));
                return;
            }
            if (!accepted.toBool()) {
                if (token.toString() == expected_token && position == -1 &&
                    count == 0 && !can_previous.toBool() && !can_next.toBool())
                    snapshot.token = token.toString();
                completion(std::move(snapshot));
                return;
            }
            if (token.toString() != expected_token || count <= 0 ||
                position < 0 || position >= count ||
                can_previous.toBool() != (position > 0) ||
                can_next.toBool() != (position + 1 < count)) {
                completion(std::move(snapshot));
                return;
            }
            snapshot.accepted = true;
            snapshot.token = token.toString();
            snapshot.current_position = position;
            snapshot.ordered_count = count;
            snapshot.can_previous = can_previous.toBool();
            snapshot.can_next = can_next.toBool();
            completion(std::move(snapshot));
        });
}

ArticleView::ArticleView(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    web_view_ = new ArticleWebView(this);
    web_view_->setObjectName(QStringLiteral("articleWebContent"));
    layout->addWidget(web_view_, 1);

    full_text_navigation_row_ = new QWidget(this);
    full_text_navigation_row_->setObjectName(
        QStringLiteral("fullTextNavigationRow"));
    auto* navigation_layout = new QHBoxLayout(full_text_navigation_row_);
    full_text_previous_ =
        new QPushButton(tr("&Previous"), full_text_navigation_row_);
    full_text_previous_->setObjectName(QStringLiteral("fullTextPrevious"));
    full_text_next_ = new QPushButton(tr("&Next"), full_text_navigation_row_);
    full_text_next_->setObjectName(QStringLiteral("fullTextNext"));
    full_text_status_ = new QLabel(full_text_navigation_row_);
    full_text_status_->setObjectName(
        QStringLiteral("fullTextNavigationStatus"));
    navigation_layout->addWidget(full_text_previous_);
    navigation_layout->addWidget(full_text_next_);
    navigation_layout->addWidget(full_text_status_);
    navigation_layout->addStretch(1);
    layout->addWidget(full_text_navigation_row_);
    full_text_navigation_row_->hide();
    full_text_previous_->setEnabled(false);
    full_text_next_->setEnabled(false);
    setFocusProxy(web_view_);

    connect(web_view_, &QWebEngineView::loadStarted, this, [this]() {
        ++document_generation_;
        ++pointer_generation_;
        dictionary_context_entries_.clear();
        dictionary_context_overflow_ = false;
        ClearFullTextNavigation({}, true);
        emit loadStarted();
    });
    connect(web_view_, &QWebEngineView::loadFinished, this,
            &ArticleView::loadFinished);
    connect(web_view_, &QWebEngineView::urlChanged, this,
            &ArticleView::urlChanged);
    connect(web_view_, &QWebEngineView::pdfPrintingFinished, this,
            &ArticleView::pdfPrintingFinished);
    connect(web_view_, &QWebEngineView::printFinished, this,
            &ArticleView::printFinished);
    connect(full_text_previous_, &QPushButton::clicked, this, [this]() {
        if (!full_text_navigation_token_.isEmpty())
            emit FullTextNavigationRequested(
                ArticleHighlightNavigationDirection::kPrevious);
    });
    connect(full_text_next_, &QPushButton::clicked, this, [this]() {
        if (!full_text_navigation_token_.isEmpty())
            emit FullTextNavigationRequested(
                ArticleHighlightNavigationDirection::kNext);
    });
}

QWebEnginePage* ArticleView::page() const {
    return web_view_->page();
}

void ArticleView::setPage(QWebEnginePage* page) {
    disconnect(page_loading_connection_);
    ++document_generation_;
    ++pointer_generation_;
    web_view_->setPage(page);
    page_loading_connection_ = connect(
        page, &QWebEnginePage::loadingChanged, this,
        [this](const QWebEngineLoadingInfo& info) {
            if (info.status() != QWebEngineLoadingInfo::LoadSucceededStatus &&
                info.status() != QWebEngineLoadingInfo::LoadFailedStatus &&
                info.status() != QWebEngineLoadingInfo::LoadStoppedStatus) {
                return;
            }
            const QUrlQuery query(info.url());
            bool valid = false;
            const quint64 token =
                query.queryItemValue(QStringLiteral("gd-navigation-token"))
                    .toULongLong(&valid);
            if (valid && token != 0U) {
                emit HtmlNavigationFinished(
                    token, info.status() ==
                               QWebEngineLoadingInfo::LoadSucceededStatus);
            }
        });
    emit PageReplaced();
}

void ArticleView::setHtml(const QString& html, const QUrl& base_url) {
    SetHtmlNavigation(ReserveHtmlNavigation(), html, base_url);
}

quint64 ArticleView::ReserveHtmlNavigation() {
    ++next_html_navigation_token_;
    if (next_html_navigation_token_ == 0U)
        ++next_html_navigation_token_;
    return next_html_navigation_token_;
}

void ArticleView::SetHtmlNavigation(quint64 navigation_token,
                                    const QString& html, const QUrl& base_url) {
    Q_ASSERT(navigation_token != 0U &&
             navigation_token == next_html_navigation_token_);
    QUrl navigation_url = base_url.isEmpty()
                              ? goldendict::app::ArticleContentBaseUrl()
                              : base_url;
    QUrlQuery query(navigation_url);
    query.removeAllQueryItems(QStringLiteral("gd-navigation-token"));
    query.addQueryItem(QStringLiteral("gd-navigation-token"),
                       QString::number(navigation_token));
    navigation_url.setQuery(query);
    web_view_->setHtml(html, navigation_url);
}

void ArticleView::reload() {
    web_view_->reload();
}

void ArticleView::findText(
    const QString& text, QWebEnginePage::FindFlags options,
    const std::function<void(const QWebEngineFindTextResult&)>& callback) {
    web_view_->findText(text, options, callback);
}

void ArticleView::setZoomFactor(qreal factor) {
    web_view_->setZoomFactor(factor);
}

qreal ArticleView::zoomFactor() const {
    return web_view_->zoomFactor();
}

void ArticleView::print(QPrinter* printer) {
    web_view_->print(printer);
}

void ArticleView::printToPdf(
    const std::function<void(const QByteArray&)>& result_callback) const {
    web_view_->printToPdf(result_callback);
}

void ArticleView::printToPdf(const QString& file_path) const {
    web_view_->printToPdf(file_path);
}

void ArticleView::setFocus(Qt::FocusReason reason) {
    web_view_->setFocus(reason);
}

bool ArticleView::hasFocus() const {
    return web_view_->hasFocus();
}

void ArticleView::PublishFullTextNavigationSnapshot(
    const ArticleHighlightNavigationSnapshot& snapshot) {
    if (!snapshot.accepted || snapshot.token.isEmpty() ||
        snapshot.ordered_count <= 0 || snapshot.current_position < 0 ||
        snapshot.current_position >= snapshot.ordered_count) {
        return;
    }
    full_text_navigation_token_ = snapshot.token;
    full_text_previous_->setEnabled(snapshot.can_previous);
    full_text_next_->setEnabled(snapshot.can_next);
    full_text_status_->setText(tr("%1 of %2 matches")
                                   .arg(snapshot.current_position + 1)
                                   .arg(snapshot.ordered_count));
    full_text_navigation_row_->show();
}

void ArticleView::ClearFullTextNavigation(const QString& expected_token,
                                          bool force) {
    if (!force && (expected_token.isEmpty() ||
                   expected_token != full_text_navigation_token_)) {
        return;
    }
    full_text_navigation_token_.clear();
    full_text_previous_->setEnabled(false);
    full_text_next_->setEnabled(false);
    full_text_status_->clear();
    full_text_navigation_row_->hide();
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

void ArticleView::HandleMousePressEvent(QMouseEvent* event) {
    if (select_word_by_single_click_ && event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::NoModifier) {
        QueryWordAt(event->position(), false);
    }
}

void ArticleView::HandleMouseDoubleClickEvent(QMouseEvent* event) {
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

void ArticleView::HandleContextMenuEvent(QContextMenuEvent* event) {
    auto* request = web_view_->lastContextMenuRequest();
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
