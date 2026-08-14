// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_composer.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

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

}  // namespace

ArticleContent ComposeLookupPage(const LookupResponse& response,
                                 const ArticleCompositionOptions& options) {
    ArticleContent page;
    std::string html = NewDocument();
    const bool may_collapse =
        options.collapse_large_articles && response.entries.size() > 1U;
    for (const auto& entry : response.entries) {
        const std::string label = entry.dictionary.name.empty()
                                      ? entry.dictionary.id
                                      : entry.dictionary.name;
        const bool collapse =
            may_collapse && Utf8CodePointCount(entry.article.plain_text) >
                                options.article_size_limit;
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
        const std::string_view body =
            entry.article.sanitized_html.has_value()
                ? ExtractDocumentBody(*entry.article.sanitized_html)
                : std::string_view{};
        if (!body.empty()) {
            AppendBounded(body, &html);
        } else {
            AppendBounded("<p>", &html);
            AppendBounded(Escape(entry.article.plain_text), &html);
            AppendBounded("</p>", &html);
        }
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
    }
    FinishDocument(&html);
    if (html.size() > kMaximumComposedDocumentBytes) {
        throw std::length_error("Composed article exceeds the size limit");
    }
    page.sanitized_html = std::move(html);
    return page;
}

}  // namespace goldendict::core::article
