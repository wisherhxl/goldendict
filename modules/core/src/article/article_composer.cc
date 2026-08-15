// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_composer.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "article_document.h"

namespace goldendict::core::article {
namespace {

constexpr std::size_t kMaximumComposedDocumentBytes = 16U * 1024U * 1024U;

void AppendBounded(std::string_view value, std::string* output) {
    if (value.size() > kMaximumComposedDocumentBytes - output->size()) {
        throw std::length_error("Composed article exceeds the size limit");
    }
    output->append(value);
}

std::string Escape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped.push_back(character);
        }
    }
    return escaped;
}

std::size_t Utf8CodePointCount(std::string_view text) noexcept {
    return static_cast<std::size_t>(std::count_if(
        text.begin(), text.end(),
        [](unsigned char byte) { return (byte & 0xC0U) != 0x80U; }));
}

constexpr std::string_view kOptionalPartStart =
    "<span class=\"gd-optional-part\">";

std::size_t OptionalTextCodePointCount(std::string_view html) noexcept {
    std::size_t count = 0U;
    std::size_t position = 0U;
    std::size_t optional_depth = 0U;
    std::vector<bool> span_stack;
    while (position < html.size()) {
        if (html.compare(position, kOptionalPartStart.size(),
                         kOptionalPartStart) == 0) {
            span_stack.push_back(true);
            ++optional_depth;
            position += kOptionalPartStart.size();
            continue;
        }
        if (html.compare(position, 5U, "<span") == 0) {
            span_stack.push_back(false);
        } else if (html.compare(position, 7U, "</span>") == 0) {
            if (!span_stack.empty()) {
                if (span_stack.back())
                    --optional_depth;
                span_stack.pop_back();
            }
        }
        if (html[position] == '<') {
            const auto end = html.find('>', position + 1U);
            position = end == std::string_view::npos ? html.size() : end + 1U;
            continue;
        }
        if (optional_depth != 0U) {
            if (html[position] == '&') {
                const auto end = html.find(';', position + 1U);
                if (end != std::string_view::npos) {
                    ++count;
                    position = end + 1U;
                    continue;
                }
            }
            if ((static_cast<unsigned char>(html[position]) & 0xC0U) != 0x80U)
                ++count;
        }
        ++position;
    }
    return count;
}

std::string RenderOptionalControls(std::string_view body,
                                   std::size_t entry_index) {
    constexpr std::string_view kArticleStart = "<section class=\"gd-article\">";
    constexpr std::string_view kArticleEnd = "</section>";
    std::string rendered;
    std::size_t position = 0U;
    std::size_t article_index = 0U;
    while (position < body.size()) {
        const auto start = body.find(kArticleStart, position);
        if (start == std::string_view::npos) {
            rendered.append(body.substr(position));
            break;
        }
        const auto end = body.find(kArticleEnd, start + kArticleStart.size());
        if (end == std::string_view::npos) {
            rendered.append(body.substr(position));
            break;
        }
        rendered.append(
            body.substr(position, start + kArticleStart.size() - position));
        const std::string_view article = body.substr(
            start + kArticleStart.size(), end - start - kArticleStart.size());
        if (article.find(kOptionalPartStart) != std::string_view::npos) {
            const std::string toggle_id = "gd-optional-toggle-" +
                                          std::to_string(entry_index) + "-" +
                                          std::to_string(article_index);
            rendered +=
                "<input class=\"gd-optional-toggle\" type=\"checkbox\" id=\"" +
                toggle_id + "\"><label class=\"gd-optional-control\" for=\"" +
                toggle_id +
                "\" aria-label=\"Expand optional parts\"></label>"
                "<div class=\"gd-entry-body\">";
            rendered.append(article);
            rendered += "</div>";
        } else {
            rendered.append(article);
        }
        rendered.append(kArticleEnd);
        position = end + kArticleEnd.size();
        ++article_index;
    }
    return rendered;
}

}  // namespace

ArticleContent ComposeLookupPage(const LookupResponse& response,
                                 const ArticleCompositionOptions& options) {
    ArticleContent page;
    std::string html = NewDocument();
    const bool may_collapse =
        options.collapse_large_articles && response.entries.size() > 1U;
    std::size_t entry_index = 0U;
    for (const auto& entry : response.entries) {
        const std::string label = entry.dictionary.name.empty()
                                      ? entry.dictionary.id
                                      : entry.dictionary.name;
        const std::string_view body =
            entry.article.sanitized_html.has_value()
                ? ExtractDocumentBody(*entry.article.sanitized_html)
                : std::string_view{};
        const bool has_optional_parts =
            body.find(kOptionalPartStart) != std::string_view::npos;
        std::size_t visible_size = Utf8CodePointCount(entry.article.plain_text);
        if (!options.always_expand_optional_parts && has_optional_parts) {
            const std::size_t optional_size = OptionalTextCodePointCount(body);
            visible_size = optional_size > visible_size
                               ? 0U
                               : visible_size - optional_size;
        }
        const bool collapse =
            may_collapse && visible_size > options.article_size_limit;
        AppendBounded("<section class=\"gd-dictionary-result\">", &html);
        if (collapse) {
            AppendBounded("<details class=\"gd-collapsed-article\"><summary>",
                          &html);
        }
        AppendBounded("<h2>", &html);
        AppendBounded(Escape(label), &html);
        AppendBounded("</h2>", &html);
        if (collapse) {
            AppendBounded("</summary>", &html);
        }
        AppendBounded("<div class=\"gd-entry-body\">", &html);
        if (!body.empty()) {
            if (has_optional_parts && !options.always_expand_optional_parts) {
                AppendBounded(RenderOptionalControls(body, entry_index), &html);
            } else {
                AppendBounded(body, &html);
            }
        } else {
            AppendBounded("<p>", &html);
            AppendBounded(Escape(entry.article.plain_text), &html);
            AppendBounded("</p>", &html);
        }
        AppendBounded("</div>", &html);
        if (collapse) {
            AppendBounded("</details>", &html);
        }
        AppendBounded("</section>", &html);

        if (!page.plain_text.empty()) {
            page.plain_text += "\n\n";
        }
        page.plain_text += label;
        if (!entry.article.plain_text.empty()) {
            page.plain_text += "\n" + entry.article.plain_text;
        }
        ++entry_index;
    }
    FinishDocument(&html);
    if (html.size() > kMaximumComposedDocumentBytes) {
        throw std::length_error("Composed article exceeds the size limit");
    }
    page.sanitized_html = std::move(html);
    return page;
}

}  // namespace goldendict::core::article
