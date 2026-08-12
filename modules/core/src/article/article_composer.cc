// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_composer.h"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace goldendict::core::article {
namespace {

constexpr std::string_view kDocumentPrefix =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src "
    "'none'; img-src goldendict:; media-src goldendict:; style-src "
    "'none'\"></head><body>";
constexpr std::string_view kDocumentSuffix = "</body></html>";
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

std::string_view BodyOf(std::string_view document) {
    if (document.size() < kDocumentPrefix.size() + kDocumentSuffix.size() ||
        document.compare(0, kDocumentPrefix.size(), kDocumentPrefix) != 0 ||
        document.compare(document.size() - kDocumentSuffix.size(),
                         kDocumentSuffix.size(), kDocumentSuffix) != 0) {
        return {};
    }
    document.remove_prefix(kDocumentPrefix.size());
    document.remove_suffix(kDocumentSuffix.size());
    return document;
}

}  // namespace

ArticleContent ComposeLookupPage(const LookupResponse& response) {
    ArticleContent page;
    std::string html(kDocumentPrefix);
    for (const auto& entry : response.entries) {
        const std::string label = entry.dictionary.name.empty()
                                      ? entry.dictionary.id
                                      : entry.dictionary.name;
        AppendBounded("<section class=\"gd-dictionary-result\"><h2>", &html);
        AppendBounded(Escape(label), &html);
        AppendBounded("</h2>", &html);
        const std::string_view body =
            entry.article.sanitized_html.has_value()
                ? BodyOf(*entry.article.sanitized_html)
                : std::string_view{};
        if (!body.empty()) {
            AppendBounded(body, &html);
        } else {
            AppendBounded("<p>", &html);
            AppendBounded(Escape(entry.article.plain_text), &html);
            AppendBounded("</p>", &html);
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
    AppendBounded(kDocumentSuffix, &html);
    page.sanitized_html = std::move(html);
    return page;
}

}  // namespace goldendict::core::article
