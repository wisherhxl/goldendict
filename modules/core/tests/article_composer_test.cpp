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
    void CollapsesOnlyLargeResultsInMultiDictionaryPages();
    void KeepsSingleDictionaryPagesExpanded();
    void AppliesOptionalPartPolicyWithoutChangingPlainText();
    void RejectsOversizedComposedPages();
};

void ArticleComposerTest::CombinesEntriesAndEscapesDictionaryLabels() {
    LookupResponse response;
    DictionaryEntry first;
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
            "<details class=\"gd-collapsed-article\"><summary><h2>Optional") ==
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
