// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "../src/article/article_composer.h"

namespace goldendict::core::article {

class ArticleComposerTest : public QObject {
    Q_OBJECT

   private slots:
    void CombinesEntriesAndEscapesDictionaryLabels();
    void FallsBackToEscapedPlainTextForUntrustedMarkup();
    void RejectsOversizedComposedPages();
};

void ArticleComposerTest::CombinesEntriesAndEscapesDictionaryLabels() {
    LookupResponse response;
    DictionaryEntry first;
    first.dictionary.name = "First <Dictionary>";
    first.article.plain_text = "first article";
    first.article.sanitized_html =
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src "
        "'none'; img-src goldendict:; media-src goldendict:; style-src "
        "'none'\"></head><body><section class=\"gd-article\"><p>first "
        "article</p></section></body></html>";
    DictionaryEntry second;
    second.dictionary.name = "Second";
    second.article.plain_text = "second article";
    second.article.sanitized_html =
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src "
        "'none'; img-src goldendict:; media-src goldendict:; style-src "
        "'none'\"></head><body><section class=\"gd-article\"><b>second "
        "article</b></section></body></html>";
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
