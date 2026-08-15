// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_document.h"

namespace goldendict::core::article {
namespace {

constexpr std::string_view kDocumentPrefix =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src "
    "'none'; img-src goldendict:; media-src goldendict:; style-src "
    "'unsafe-inline'\"><style>"
    ":root{color-scheme:light dark}"
    "body{box-sizing:border-box;margin:0 auto;max-width:72rem;padding:1rem;"
    "font:1rem/1.55 system-ui,sans-serif;overflow-wrap:anywhere}"
    "a{color:#1769aa;text-decoration-thickness:.08em}"
    "img,video{height:auto;max-width:100%}"
    "audio{max-width:100%}"
    "table{border-collapse:collapse;display:block;max-width:100%;overflow:auto}"
    "td,th{border:1px solid #999;padding:.25rem .45rem}"
    "pre{overflow:auto;white-space:pre-wrap}"
    ".gd-dictionary-result{border-top:1px solid #aaa;margin-top:1.25rem;"
    "padding-top:.5rem}"
    ".gd-dictionary-result:first-child{border-top:0;margin-top:0}"
    ".gd-dictionary-result h2{font-size:1rem;margin:.25rem 0 .75rem}"
    ".gd-collapsed-article>summary{cursor:pointer;list-style-position:outside}"
    ".gd-collapsed-article>summary>h2{display:inline}"
    ".gd-optional-toggle{position:absolute;clip:rect(0 0 0 0);"
    "clip-path:inset(50%);height:1px;width:1px;overflow:hidden}"
    ".gd-optional-control{color:#1769aa;cursor:pointer;margin-left:.35rem}"
    ".gd-optional-control:after{content:'[+]'}"
    ".gd-optional-toggle:checked+.gd-optional-control:after{content:'[-]'}"
    ".gd-optional-toggle:not(:checked)~.gd-entry-body .gd-optional-part{"
    "display:none}"
    ".gd-article{margin-bottom:.75rem}"
    "@media "
    "print{.gd-collapsed-article:not([open])>:not(summary){display:block!"
    "important}}"
    "@media(prefers-color-scheme:dark){a{color:#8bc4ff}}"
    "</style></head><body>";
constexpr std::string_view kDocumentSuffix = "</body></html>";

}  // namespace

std::string NewDocument() {
    return std::string(kDocumentPrefix);
}

void FinishDocument(std::string* document) {
    document->append(kDocumentSuffix);
}

std::string_view ExtractDocumentBody(std::string_view document) {
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

}  // namespace goldendict::core::article
