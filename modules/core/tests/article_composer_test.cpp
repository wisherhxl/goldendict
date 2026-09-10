// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "../src/article/article_composer.h"
#include "../src/article/article_document.h"

namespace goldendict::core::article {

class ArticleComposerTest : public QObject {
    Q_OBJECT

   private slots:
    void CombinesEntriesAndEscapesDictionaryLabels();
    void FallsBackToEscapedPlainTextForUntrustedMarkup();
    void KeepsCurrentPrefixAndRejectsForeignPrefixes();
    void CollapsesOnlyLargeResultsInMultiDictionaryPages();
    void KeepsSingleDictionaryPagesExpanded();
    void AppliesOptionalPartPolicyWithoutChangingPlainText();
    void RejectsOversizedComposedPages();
};

void ArticleComposerTest::CombinesEntriesAndEscapesDictionaryLabels() {
    LookupResponse response;
    DictionaryEntry first;
    first.dictionary.id = "first\"<&'";
    first.dictionary.name = "First <Dictionary>";
    first.article.plain_text = "first article";
    first.article.sanitized_html = NewDocument();
    first.article.sanitized_html->append(
        "<section class=\"gd-article\"><p>first article</p></section>");
    FinishDocument(&*first.article.sanitized_html);
    DictionaryEntry second;
    second.dictionary.name = "Second";
    second.article.plain_text = "second article";
    second.article.sanitized_html = NewDocument();
    second.article.sanitized_html->append(
        "<section class=\"gd-article\"><b>second article</b></section>");
    FinishDocument(&*second.article.sanitized_html);
    response.entries = {std::move(first), std::move(second)};

    const ArticleContent page = ComposeLookupPage(response);

    QVERIFY(page.sanitized_html.has_value());
    QVERIFY(page.sanitized_html->find(
                "data-gd-dictionary-id=\"first&quot;&lt;&amp;&#39;\"") !=
            std::string::npos);
    QVERIFY(page.sanitized_html->find("First &lt;Dictionary&gt;") !=
            std::string::npos);
    QVERIFY(page.sanitized_html->find("first article") != std::string::npos);
    QVERIFY(page.sanitized_html->find("second article") != std::string::npos);
    QCOMPARE(page.plain_text,
             std::string("First <Dictionary>\nfirst article\n\nSecond\nsecond "
                         "article"));
}

void ArticleComposerTest::FallsBackToEscapedPlainTextForUntrustedMarkup() {
    LookupResponse response;
    DictionaryEntry entry;
    entry.dictionary.id = "fallback";
    entry.article.plain_text = "one < two & three";
    entry.article.sanitized_html = "<script>alert(1)</script>";
    response.entries.push_back(std::move(entry));

    const ArticleContent page = ComposeLookupPage(response);

    QVERIFY(page.sanitized_html->find("<script>") == std::string::npos);
    QVERIFY(page.sanitized_html->find("one &lt; two &amp; three") !=
            std::string::npos);
}

void ArticleComposerTest::KeepsCurrentPrefixAndRejectsForeignPrefixes() {
    LookupResponse response;
    DictionaryEntry entry;
    entry.dictionary.name = "Prefix compatibility";
    entry.article.plain_text = "safe <fallback>";
    const std::string body = "<pre>preserved  spaces\nand lines</pre>";
    const std::string prefix = NewDocument();
    std::string current = prefix + body;
    FinishDocument(&current);
    QCOMPARE(ExtractDocumentBody(current), std::string_view(body));
    entry.article.sanitized_html = current;
    response.entries = {entry};
    QVERIFY(ComposeLookupPage(response).sanitized_html->find(body) !=
            std::string::npos);

    // An old or foreign envelope is not proof that its body is trusted.
    const std::string old_body_style =
        "body{box-sizing:border-box;margin:0 auto;max-width:72rem;padding:1rem;"
        "font:1rem/1.55 system-ui,sans-serif;overflow-wrap:anywhere}";
    std::string old = current;
    const auto start = old.find("body{");
    QVERIFY(start != std::string::npos);
    old.replace(start, old.find('}', start) - start + 1U, old_body_style);
    old.insert(start, ":root{color-scheme:light dark}");
    const auto pre = old.find("pre{font-size:12px}");
    QVERIFY(pre != std::string::npos);
    old.replace(pre, std::string("pre{font-size:12px}").size(),
                "pre{overflow:auto;white-space:pre-wrap}");
    std::string previous_heading_style = current;
    const auto heading_start =
        previous_heading_style.find("body>section.gd-dictionary-result{");
    const auto heading_end =
        previous_heading_style.find(".gd-optional-toggle{");
    QVERIFY(heading_start != std::string::npos);
    QVERIFY(heading_end > heading_start);
    previous_heading_style.replace(
        heading_start, heading_end - heading_start,
        ".gd-dictionary-result{border-top:1px solid #aaa;margin-top:1.25rem;"
        "padding-top:.5rem}"
        ".gd-dictionary-result:first-child{border-top:0;margin-top:0}"
        ".gd-dictionary-result h2{font-size:1rem;margin:.25rem 0 .75rem}"
        ".gd-collapsed-article>summary{cursor:pointer;list-style-position:"
        "outside}"
        ".gd-collapsed-article>summary>h2{display:inline}");
    for (const auto& foreign :
         {old, previous_heading_style,
          std::string("<!doctype html><html><body>") + body + "</body></html>",
          current + "extra", current.substr(1)}) {
        QVERIFY(ExtractDocumentBody(foreign).empty());
        response.entries.front().article.sanitized_html = foreign;
        const auto page = ComposeLookupPage(response);
        QVERIFY(page.sanitized_html->find(body) == std::string::npos);
        QVERIFY(page.sanitized_html->find("safe &lt;fallback&gt;") !=
                std::string::npos);
        QCOMPARE(page.plain_text,
                 std::string("Prefix compatibility\nsafe <fallback>"));
    }
}

void ArticleComposerTest::CollapsesOnlyLargeResultsInMultiDictionaryPages() {
    LookupResponse response;
    DictionaryEntry large;
    large.dictionary.name = "Large";
    large.article.plain_text = "12345";
    DictionaryEntry equal;
    equal.dictionary.name = "Equal";
    equal.article.plain_text = "1234";
    response.entries = {std::move(large), std::move(equal)};

    const ArticleContent page =
        ComposeLookupPage(response, {false, true, std::uint32_t{4}});

    QVERIFY(page.sanitized_html.has_value());
    QCOMPARE(
        page.sanitized_html->find(
            "<details class=\"gd-collapsed-article\"><summary><h2>Large") !=
            std::string::npos,
        true);
    QVERIFY(page.sanitized_html->find(
                "</h2></summary><div class=\"gd-entry-body\"><p>12345</p>") !=
            std::string::npos);
    QCOMPARE(
        page.sanitized_html->find(
            "<details class=\"gd-collapsed-article\"><summary><h2>Equal") ==
            std::string::npos,
        true);
    QVERIFY(page.sanitized_html->find("@media print") != std::string::npos);
}

void ArticleComposerTest::KeepsSingleDictionaryPagesExpanded() {
    LookupResponse response;
    DictionaryEntry entry;
    entry.dictionary.name = "Only";
    entry.article.plain_text = "long article";
    response.entries.push_back(std::move(entry));

    const ArticleContent page =
        ComposeLookupPage(response, {false, true, std::uint32_t{1}});

    QVERIFY(page.sanitized_html.has_value());
    QVERIFY(
        page.sanitized_html->find("<details class=\"gd-collapsed-article\"") ==
        std::string::npos);
}

void ArticleComposerTest::AppliesOptionalPartPolicyWithoutChangingPlainText() {
    LookupResponse response;
    DictionaryEntry optional;
    optional.dictionary.name = "Optional";
    optional.article.plain_text = "aboptionalcdmore";
    optional.article.sanitized_html = NewDocument();
    optional.article.sanitized_html->append(
        "<section class=\"gd-article\"><span>ab<span "
        "class=\"gd-optional-part\">optional</span></span></section><section "
        "class=\"gd-article\">cd<span "
        "class=\"gd-optional-part\">more</span></section>");
    FinishDocument(&*optional.article.sanitized_html);
    DictionaryEntry other;
    other.dictionary.name = "Other";
    other.article.plain_text = "x";
    response.entries = {optional, other};

    const ArticleContent hidden =
        ComposeLookupPage(response, {false, true, std::uint32_t{4}});
    QVERIFY(hidden.sanitized_html->find("gd-optional-toggle-0-0") !=
            std::string::npos);
    QVERIFY(hidden.sanitized_html->find("gd-optional-toggle-0-1") !=
            std::string::npos);
    QVERIFY(hidden.sanitized_html->find("gd-optional-part") !=
            std::string::npos);
    QVERIFY(
        hidden.sanitized_html->find(
            "<details class=\"gd-collapsed-article\"><summary><h2>Optional") !=
        std::string::npos);
    QCOMPARE(hidden.plain_text,
             std::string("Optional\naboptionalcdmore\n\nOther\nx"));

    const ArticleContent expanded =
        ComposeLookupPage(response, {true, true, std::uint32_t{4}});
    QVERIFY(expanded.sanitized_html->find("gd-optional-toggle-0-0") ==
            std::string::npos);
    QVERIFY(
        expanded.sanitized_html->find(
            "<details class=\"gd-collapsed-article\"><summary><h2>Optional") !=
        std::string::npos);
    QCOMPARE(expanded.plain_text, hidden.plain_text);

    for (const bool expand : {false, true}) {
        for (const std::uint32_t limit : {15U, 16U}) {
            const auto page =
                ComposeLookupPage(response, {expand, true, limit});
            QCOMPARE(page.sanitized_html->find(
                         "<details class=\"gd-collapsed-article\">") !=
                         std::string::npos,
                     limit == 15U);
            QCOMPARE(page.plain_text, hidden.plain_text);
        }
        const auto disabled = ComposeLookupPage(response, {expand, false, 1U});
        QVERIFY(disabled.sanitized_html->find(
                    "<details class=\"gd-collapsed-article\">") ==
                std::string::npos);
    }
    response.entries.resize(1U);
    for (const bool expand : {false, true}) {
        const auto sole = ComposeLookupPage(response, {expand, true, 1U});
        QVERIFY(sole.sanitized_html->find(
                    "<details class=\"gd-collapsed-article\">") ==
                std::string::npos);
    }
}

void ArticleComposerTest::RejectsOversizedComposedPages() {
    LookupResponse response;
    DictionaryEntry entry;
    entry.dictionary.name = "Large";
    entry.article.plain_text.assign(16U * 1024U * 1024U, 'x');
    response.entries.push_back(std::move(entry));

    QVERIFY_EXCEPTION_THROWN(ComposeLookupPage(response), std::length_error);
}

}  // namespace goldendict::core::article

using goldendict::core::article::ArticleComposerTest;

QTEST_APPLESS_MAIN(ArticleComposerTest)

#include "article_composer_test.moc"
