// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "../src/article/article_assembler.h"
#include "../src/article/internal_url.h"

namespace goldendict::core::article {
namespace {

class ArticleAssemblerTest : public QObject {
    Q_OBJECT

   private slots:
    void EscapesPlainTextAndKeepsItStructured();
    void SanitizesMarkupAndRewritesTypedLinks();
    void PreservesSafeAudioAndCollectsItsResource();
    void DeduplicatesResourceReferencesAcrossArticles();
    void RemovesActiveContentAndUnsafeAttributes();
    void FallsBackToInertTextForMalformedMarkup();
    void EnforcesDocumentSizeLimit();
    void BuildsAndParsesCanonicalInternalUrls();
    void RejectsMalformedAndUnsafeInternalUrls();
};

const dictionary::Identity kDictionary{"fixture id", "Fixture", "/fixture", "",
                                       ""};

void ArticleAssemblerTest::EscapesPlainTextAndKeepsItStructured() {
    const Document document =
        Assemble(kDictionary, {{"example", "text/plain", "one < two & three"}});

    QCOMPARE(document.plain_text, "one < two & three");
    QVERIFY(document.sanitized_html.find("one &lt; two &amp; three") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("Content-Security-Policy") !=
            std::string::npos);
    QVERIFY(document.resources.empty());
}

void ArticleAssemblerTest::SanitizesMarkupAndRewritesTypedLinks() {
    const Document document = Assemble(
        kDictionary,
        {{"example", "text/html",
          "<p><b>Example</b> <a href=\"bword://linked word\">linked</a>"
          "<img src=\"images\\pixel.png\" alt=\"pixel\"></p>"}});

    QCOMPARE(document.plain_text, "Example linked");
    QVERIFY(document.sanitized_html.find(
                "href=\"goldendict://lookup/linked%20word\"") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "src=\"goldendict://resource/fixture%20id/"
                "images%2Fpixel.png\"") != std::string::npos);
    QCOMPARE(document.resources.size(), std::size_t{1});
    QCOMPARE(document.resources.front().dictionary_id, "fixture id");
    QCOMPARE(document.resources.front().resource_id, "images/pixel.png");
}

void ArticleAssemblerTest::PreservesSafeAudioAndCollectsItsResource() {
    const Document document = Assemble(
        kDictionary, {{"example", "text/html",
                       "<audio controls=\"yes\"><source src=\"spoken.wav\" "
                       "type=\"audio/wav\"></audio>spoken"}});
    QVERIFY(document.sanitized_html.find("<audio controls=\"controls\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "src=\"goldendict://resource/fixture%20id/spoken.wav\"") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("type=\"audio/wav\"") !=
            std::string::npos);
    QCOMPARE(document.resources.size(), std::size_t{1});
    QCOMPARE(document.resources.front().resource_id, "spoken.wav");
}

void ArticleAssemblerTest::DeduplicatesResourceReferencesAcrossArticles() {
    const Document document = Assemble(
        kDictionary, {{"first", "text/html", "<img src=\"shared.png\">"},
                      {"second", "text/html", "<img src=\"shared.png\">"}});

    QCOMPARE(document.resources.size(), std::size_t{1});
    QCOMPARE(document.resources.front().resource_id, "shared.png");
}

void ArticleAssemblerTest::RemovesActiveContentAndUnsafeAttributes() {
    const Document document = Assemble(
        kDictionary, {{"example", "text/html",
                       "<p onclick=\"steal()\">safe<script>alert(1)</script>"
                       "<a href=\"javascript:steal()\">link</a>"
                       "<img src=\"../secret\" onerror=\"steal()\"></p>"}});

    QCOMPARE(document.plain_text, "safelink");
    QVERIFY(document.sanitized_html.find("script") == std::string::npos);
    QVERIFY(document.sanitized_html.find("onclick") == std::string::npos);
    QVERIFY(document.sanitized_html.find("onerror") == std::string::npos);
    QVERIFY(document.sanitized_html.find("javascript:") == std::string::npos);
    QVERIFY(document.sanitized_html.find("secret") == std::string::npos);
    QVERIFY(document.resources.empty());
}

void ArticleAssemblerTest::FallsBackToInertTextForMalformedMarkup() {
    const Document document = Assemble(
        kDictionary, {{"example", "text/html", "<b>unterminated<script>"}});

    QCOMPARE(document.plain_text, "<b>unterminated<script>");
    QVERIFY(document.sanitized_html.find(
                "&lt;b&gt;unterminated&lt;script&gt;") != std::string::npos);
}

void ArticleAssemblerTest::EnforcesDocumentSizeLimit() {
    const std::string oversized(16U * 1024U * 1024U + 1U, 'x');

    try {
        static_cast<void>(
            Assemble(kDictionary, {{"example", "text/plain", oversized}}));
        QFAIL("Assemble should reject an oversized document");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

void ArticleAssemblerTest::BuildsAndParsesCanonicalInternalUrls() {
    const std::string lookup = MakeLookupUrl("你好 world");
    const auto parsed_lookup = ParseInternalUrl(lookup);
    QVERIFY(parsed_lookup.has_value());
    QCOMPARE(parsed_lookup->kind, InternalUrlKind::kLookup);
    QCOMPARE(parsed_lookup->target, "你好 world");

    const std::string resource =
        MakeResourceUrl("fixture/id", "images/pixel one.png");
    const auto parsed_resource = ParseInternalUrl(resource);
    QVERIFY(parsed_resource.has_value());
    QCOMPARE(parsed_resource->kind, InternalUrlKind::kResource);
    QCOMPARE(parsed_resource->dictionary_id, "fixture/id");
    QCOMPARE(parsed_resource->target, "images/pixel one.png");
}

void ArticleAssemblerTest::RejectsMalformedAndUnsafeInternalUrls() {
    const std::vector<std::string> invalid = {
        "https://example.test",
        "goldendict://lookup/",
        "goldendict://lookup/%",
        "goldendict://lookup/a/b",
        "goldendict://resource/id/../secret",
        "goldendict://resource/id/%2E%2E%2Fsecret",
        "goldendict://resource/id/path/extra"};
    for (const auto& url : invalid) {
        QVERIFY2(!ParseInternalUrl(url).has_value(), url.c_str());
    }
}

}  // namespace
}  // namespace goldendict::core::article

using goldendict::core::article::ArticleAssemblerTest;

QTEST_APPLESS_MAIN(ArticleAssemblerTest)

#include "article_assembler_test.moc"
