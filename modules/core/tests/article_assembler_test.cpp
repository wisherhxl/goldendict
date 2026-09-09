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
    void PreservesTextInsideSafeCustomMarkup();
    void PreservesOnlyInternalOptionalPartSemantics();
    void PreservesOnlyDslParagraphSemantics();
    void PreservesOnlyAllowlistedDslPresentationSemantics();
    void PreservesSafeMdictReferences();
    void PreservesOnlyDictdBodyLayoutClass();
    void RejectsUnsafeMdictPresentationReferences();
    void PreservesSafeStardictPresentationAndReferences();
    void PreservesSafePangoPresentation();
    void PreservesSafeAudioAndCollectsItsResource();
    void DeduplicatesResourceReferencesAcrossArticles();
    void RemovesActiveContentAndUnsafeAttributes();
    void FallsBackToInertTextForMalformedMarkup();
    void EnforcesDocumentSizeLimit();
    void BuildsAndParsesCanonicalInternalUrls();
    void RejectsMalformedAndUnsafeInternalUrls();
    void PreservesCanonicalLookupTargets_data();
    void PreservesCanonicalLookupTargets();
};

const dictionary::Identity kDictionary{"fixture id", "Fixture", "/fixture", "",
                                       "",           "",        0U,         0U};

void ArticleAssemblerTest::PreservesOnlyDictdBodyLayoutClass() {
    const auto document =
        Assemble(kDictionary,
                 {{"entry", "text/html",
                   "<div class=\"dictd_article\" dir=\"rtl\" onclick=\"bad()\">"
                   "<div dir=\"ltr\">&nbsp;&lt;literal&gt;&amp;amp;</div>"
                   "<span class=\"dictd_article\">span</span>"
                   "<div class=\"dictd_article other\">other</div>"
                   "<span class=\"dictd_phonetic\">phonetic</span>"
                   "<a class=\"dictd_inert_reference\">inert</a>"
                   "<span class=\"dictd_control\">control</span>"
                   "<div class=\"dictd_control\">wrong tag</div>"
                   "<span class=\"dictd_control other\">wrong class</span>"
                   "<div class=\"dictd_phonetic\">wrong tag</div>"
                   "<a class=\"dictd_inert_reference other\">wrong class</a>"
                   "</div>"}});
    QVERIFY(document.sanitized_html.find(
                "<div class=\"dictd_article\" dir=\"rtl\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<div dir=\"ltr\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("onclick") == std::string::npos);
    QVERIFY(document.sanitized_html.find("<span class=\"dictd_article\"") ==
            std::string::npos);
    QVERIFY(document.sanitized_html.find("dictd_article other") ==
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<span class=\"dictd_phonetic\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<a class=\"dictd_inert_reference\">") != std::string::npos);
    QVERIFY(document.sanitized_html.find("<div class=\"dictd_phonetic\">") ==
            std::string::npos);
    QVERIFY(document.sanitized_html.find("dictd_inert_reference other") ==
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<span class=\"dictd_control\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<div class=\"dictd_control\">") ==
            std::string::npos);
    QVERIFY(document.sanitized_html.find("dictd_control other") ==
            std::string::npos);
    QVERIFY(document.plain_text.find(u8"\u00a0<literal>&amp;") == 0U);
}

void ArticleAssemblerTest::EscapesPlainTextAndKeepsItStructured() {
    const Document document =
        Assemble(kDictionary, {{"example", "text/plain", "one < two & three"}});

    QCOMPARE(document.plain_text, "one < two & three");
    QVERIFY(document.sanitized_html.find("one &lt; two &amp; three") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("Content-Security-Policy") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("body{background:#fefdeb;") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("pre{font-size:12px}") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("style-src 'unsafe-inline'") !=
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

void ArticleAssemblerTest::PreservesTextInsideSafeCustomMarkup() {
    const Document document =
        Assemble(kDictionary,
                 {{"example", "text/html",
                   "<h-g eid=\"entry\"><pron e gs><xhtml:a href=\"d:word\">word"
                   "</xhtml:a><script>hidden</script><a "
                   "href=\"sound://spoken.mp3\">listen</a>"
                   "<img src=\"picture.png\"></pron-gs></h-g></b>"}});

    QCOMPARE(document.plain_text, "wordlisten");
    QVERIFY(document.sanitized_html.find("word") != std::string::npos);
    QVERIFY(document.sanitized_html.find("&lt;h-g") == std::string::npos);
    QVERIFY(document.sanitized_html.find("xhtml:a") == std::string::npos);
    QVERIFY(document.sanitized_html.find("<script") == std::string::npos);
    QVERIFY(document.sanitized_html.find("d:word") == std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "src=\"goldendict://resource/fixture%20id/picture.png\"") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "href=\"goldendict://resource/fixture%20id/spoken.mp3\"") !=
            std::string::npos);
    QCOMPARE(document.resources.size(), std::size_t{2});
    QCOMPARE(document.resources.front().resource_id, "spoken.mp3");
    QCOMPARE(document.resources.back().resource_id, "picture.png");
}

void ArticleAssemblerTest::PreservesOnlyInternalOptionalPartSemantics() {
    const Document document = Assemble(
        kDictionary,
        {{"example", "text/html",
          "before<gd-optional><b>optional</b></gd-optional>after"
          "<span class=\"gd-optional-part\" onclick=\"bad()\">plain</span>"}});

    QCOMPARE(document.plain_text, "beforeoptionalafterplain");
    QVERIFY(document.sanitized_html.find(
                "<span class=\"gd-optional-part\"><b>optional</b></span>") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("onclick") == std::string::npos);
    QVERIFY(document.sanitized_html.find("<span>plain</span>") !=
            std::string::npos);
}

void ArticleAssemblerTest::PreservesOnlyDslParagraphSemantics() {
    const Document document = Assemble(
        kDictionary, {{"example", "text/html",
                       "<span class=\"dsl_p\" title=\"North &amp; American\" "
                       "onclick=\"bad()\">US</span>"
                       "<span class=\"dsl_p extra\" title=\"bad\">mixed</span>"
                       "<span class=\"other\" title=\"bad\">plain</span>"}});

    QCOMPARE(document.plain_text, "USmixedplain");
    QVERIFY(document.sanitized_html.find(
                "<span class=\"dsl_p\" title=\"North &amp; American\">"
                "US</span>") != std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<span>mixed</span><span>plain</span>") != std::string::npos);
    QVERIFY(document.sanitized_html.find("onclick") == std::string::npos);
    QVERIFY(document.sanitized_html.find("title=\"bad\"") == std::string::npos);
}

void ArticleAssemblerTest::PreservesOnlyAllowlistedDslPresentationSemantics() {
    const Document document = Assemble(
        kDictionary,
        {{"example", "text/html",
          "<div class=\"dsl_article\"><div class=\"dsl_headwords\">head"
          "</div><div class=\"dsl_m3\"><span class=\"dsl_ex\">example"
          "</span><a class=\"dsl_ref\" href=\"bword://target\">link</a>"
          "<font color=\"c_default_color\">default</font>"
          "<font color=\"#1a2B3c\">hex</font><sub>2</sub><sup>3</sup>"
          "</div></div><div class=\"dsl_article extra\">mixed</div>"
          "<span class=\"dsl_unknown\">unknown</span>"
          "<font color=\"red;display:none\">unsafe</font>"}});

    QCOMPARE(document.plain_text,
             "head\nexamplelinkdefaulthex23\nmixed\nunknownunsafe");
    QVERIFY(document.sanitized_html.find("<div class=\"dsl_article\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<div class=\"dsl_headwords\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<div class=\"dsl_m3\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<span class=\"dsl_ex\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<a class=\"dsl_ref\" href=\"goldendict://lookup/target\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<font color=\"c_default_color\">default</font>") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<font color=\"#1a2B3c\">hex</font><sub>2</sub><sup>3</sup>") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("dsl_article extra") ==
            std::string::npos);
    QVERIFY(document.sanitized_html.find("dsl_unknown") == std::string::npos);
    QVERIFY(document.sanitized_html.find("red;display:none") ==
            std::string::npos);
}

void ArticleAssemblerTest::PreservesSafeMdictReferences() {
    const Document document = Assemble(
        kDictionary,
        {{"example", "text/html",
          "<div class=\"mdict\"><link rel=\"StyleSheet\" "
          "href=\"dictionary.css\"><script src=\"dictionary.js\">"
          "active()</script><a href=\"ENTRY://linked word#anchor\">linked</a>"
          "</div>"}});

    QCOMPARE(document.plain_text, "linked");
    QVERIFY(document.sanitized_html.find("<div class=\"mdict\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<link rel=\"stylesheet\" href=\"goldendict://resource/"
                "fixture%20id/dictionary.css\">") != std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "type=\"application/x-goldendict-inert\"") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "src=\"goldendict://resource/fixture%20id/dictionary.js\"") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("active()") == std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "href=\"goldendict://lookup/linked%20word\"") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("script-src 'none'") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "style-src 'unsafe-inline' goldendict:") != std::string::npos);
    QCOMPARE(document.resources.size(), std::size_t{2});
    QCOMPARE(document.resources[0].resource_id, "dictionary.css");
    QCOMPARE(document.resources[1].resource_id, "dictionary.js");
}

void ArticleAssemblerTest::RejectsUnsafeMdictPresentationReferences() {
    const Document document = Assemble(
        kDictionary,
        {{"example", "text/html",
          "<div class=\"mdict\"><link rel=\"stylesheet\" "
          "href=\"../secret.css\"><link rel=\"stylesheet\" "
          "href=\"https://example.test/remote.css\"><link rel=\"preload\" "
          "href=\"safe.css\"><script src=\"../secret.js\">nested()"
          "</script><script src=\"/absolute.js\">absolute()</script>"
          "<script>inline()</script>safe</div>"}});

    QCOMPARE(document.plain_text, "safe");
    QVERIFY(document.sanitized_html.find("<link") == std::string::npos);
    QVERIFY(document.sanitized_html.find("<script") == std::string::npos);
    QVERIFY(document.sanitized_html.find("secret") == std::string::npos);
    QVERIFY(document.sanitized_html.find("example.test") == std::string::npos);
    QVERIFY(document.sanitized_html.find("nested()") == std::string::npos);
    QVERIFY(document.sanitized_html.find("absolute()") == std::string::npos);
    QVERIFY(document.sanitized_html.find("inline()") == std::string::npos);
    QVERIFY(document.resources.empty());
}

void ArticleAssemblerTest::PreservesSafeStardictPresentationAndReferences() {
    const Document document = Assemble(
        kDictionary,
        {{"example", "text/html",
          "<h3 class=\"sdct_headwords\">example</h3>"
          "<div class=\"sdct_x\"><span class=\"xdxf_k\">entry</span>"
          "<span class=\"xdxf_ex_source\">Writer, Corpus</span>"
          "<a class=\"xdxf_kref\" href=\"linked word#anchor\">linked</a>"
          "<a href=\"https://example.test/reference\">remote</a>"
          "<a href=\"http://example.test/reference\">http</a>"
          "<a href=\"mailto:editor@example.test\">mail</a>"
          "<a href=\"javascript:bad()\">unsafe</a>"
          "<a href=\"data:text/plain,bad\">data</a>"
          "<img src=\"pixel.png\" losrc=\"pixel-small.png\" "
          "hisrc=\"pixel-large.png\"><span class=\"xdxf_rref\">sprite.png"
          "</span></div><div class=\"sdct_m\">"
          "<div dir=\"rtl\">&nbsp;meaning</div></div>"
          "<span style=\"color:#123456;\">colored</span>"
          "<audio src=\"spoken.wav\">"
          "listen</audio>"}});

    QCOMPARE(document.plain_text,
             "example\nentryWriter, Corpuslinkedremotehttpmailunsafedata"
             "sprite.png\n"
             "\xc2\xa0meaning\ncoloredlisten");
    QVERIFY(document.sanitized_html.find(
                "<h3 class=\"sdct_headwords\">example</h3>") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<div class=\"sdct_x\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<span class=\"xdxf_k\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<span class=\"xdxf_ex_source\">Writer, Corpus</span>") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "class=\"xdxf_kref\" href=\"goldendict://lookup/"
                "linked%20word\"") != std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "href=\"https://example.test/reference\"") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "href=\"http://example.test/reference\"") != std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "href=\"mailto:editor@example.test\"") != std::string::npos);
    QVERIFY(document.sanitized_html.find("javascript:") == std::string::npos);
    QVERIFY(document.sanitized_html.find("data:text") == std::string::npos);
    QVERIFY(document.sanitized_html.find("<div dir=\"rtl\">") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "src=\"goldendict://resource/fixture%20id/pixel.png\"") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "losrc=\"goldendict://resource/fixture%20id/"
                "pixel-small.png\"") != std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "hisrc=\"goldendict://resource/fixture%20id/"
                "pixel-large.png\"") != std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<span class=\"xdxf_rref\">sprite.png</span>") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find("sprite.png") != std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<span style=\"color:#123456;\">colored</span>") !=
            std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "<audio src=\"goldendict://resource/fixture%20id/"
                "spoken.wav\" controls=\"controls\">") != std::string::npos);
    QCOMPARE(document.resources.size(), std::size_t{4});
    QCOMPARE(document.resources[0].resource_id, "pixel.png");
    QCOMPARE(document.resources[1].resource_id, "pixel-small.png");
    QCOMPARE(document.resources[2].resource_id, "pixel-large.png");
    QCOMPARE(document.resources[3].resource_id, "spoken.wav");
}

void ArticleAssemblerTest::PreservesSafePangoPresentation() {
    const Document document = Assemble(
        kDictionary,
        {{"example", "text/html",
          "<span style=\"font-family:Noto,Sans;font-size:2.000pt;"
          "font-style:italic;font-weight:bold;font-variant:small-caps;"
          "font-stretch:condensed;background-color:#112233;"
          "text-decoration-color:blue;text-decoration-line:none;"
          "text-decoration-style:dotted;vertical-align:1.000pt;"
          "letter-spacing:0.500pt;\">safe</span>"
          "<span "
          "style=\"font-weight:bold;position:absolute;\">unsafe</span>"}});

    QCOMPARE(document.plain_text, "safeunsafe");
    QVERIFY(document.sanitized_html.find(
                "style=\"font-family:Noto,Sans;font-size:2.000pt;"
                "font-style:italic;font-weight:bold;font-variant:small-caps;"
                "font-stretch:condensed;background-color:#112233;"
                "text-decoration-color:blue;text-decoration-line:none;"
                "text-decoration-style:dotted;vertical-align:1.000pt;"
                "letter-spacing:0.500pt;\"") != std::string::npos);
    QVERIFY(document.sanitized_html.find(
                "style=\"font-weight:bold;position:absolute;\"") ==
            std::string::npos);
    QVERIFY(document.sanitized_html.find("<span>unsafe</span>") !=
            std::string::npos);
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
    QVERIFY(document.sanitized_html.find("media-src goldendict:") !=
            std::string::npos);
    QCOMPARE(document.resources.size(), std::size_t{1});
    QCOMPARE(document.resources.front().resource_id, "spoken.wav");

    const Document ogg = Assemble(
        kDictionary, {{"example", "text/html",
                       "<audio controls=\"controls\"><source "
                       "src=\"spoken.ogg\" type=\"audio/ogg\"></audio>"}});
    QVERIFY(ogg.sanitized_html.find("type=\"audio/ogg\"") != std::string::npos);
}

void ArticleAssemblerTest::DeduplicatesResourceReferencesAcrossArticles() {
    const Document document = Assemble(
        kDictionary, {{"first", "text/html", "<img src=\"shared.png\">"},
                      {"second", "text/html", "<img src=\"shared.png\">"}});

    QCOMPARE(document.resources.size(), std::size_t{1});
    QCOMPARE(document.resources.front().resource_id, "shared.png");
}

void ArticleAssemblerTest::RemovesActiveContentAndUnsafeAttributes() {
    const Document document =
        Assemble(kDictionary,
                 {{"example", "text/html",
                   "<p style=\"display:none\" onclick=\"steal()\">safe"
                   "<style>body{display:none}</style><script>alert(1)</script>"
                   "<a href=\"javascript:steal()\">link</a>"
                   "<a href=\"sound://../secret\">audio</a>"
                   "<img src=\"../secret\" onerror=\"steal()\"></p>"}});

    QCOMPARE(document.plain_text, "safelinkaudio");
    QVERIFY(document.sanitized_html.find("<script") == std::string::npos);
    QVERIFY(document.sanitized_html.find("onclick") == std::string::npos);
    QVERIFY(document.sanitized_html.find("<p style=") == std::string::npos);
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

    const std::string expanding_markup(3U * 1024U * 1024U, '"');
    try {
        static_cast<void>(Assemble(
            kDictionary, {{"example", "text/html", expanding_markup}}));
        QFAIL("Assemble should reject an expanded rendered document");
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
        "goldendict://lookup/a#b",
        "goldendict://lookup/a?b",
        "goldendict://lookup/%61",
        "goldendict://lookup/%7f",
        "goldendict://lookup/%GG",
        "goldendict://lookup/%C3%28",
        "goldendict://lookup/%FF",
        "goldendict://lookup/%00",
        "goldendict://lookup/%09",
        "goldendict://lookup/%0A",
        "goldendict://lookup/%0B",
        "goldendict://lookup/%0C",
        "goldendict://lookup/%0D",
        "goldendict://lookup:80/word",
        "goldendict://user@lookup/word",
        "goldendict://resource/id/%01",
        "goldendict://resource/%7F/name",
        "goldendict://resource/id/../secret",
        "goldendict://resource/id/%2E%2E%2Fsecret",
        "goldendict://resource/id/path/extra"};
    for (const auto& url : invalid) {
        QVERIFY2(!ParseInternalUrl(url).has_value(), url.c_str());
        if (url.rfind("goldendict:", 0U) == 0U) {
            const auto document = Assemble(
                kDictionary,
                {{"entry", "text/html", "<a href=\"" + url + "\">inert</a>"}});
            QVERIFY(document.sanitized_html.find("<a href=") ==
                    std::string::npos);
        }
    }
    for (const std::string value :
         {std::string{}, std::string(1, '\0'), std::string("\t"),
          std::string("\n"), std::string("\v"), std::string("\f"),
          std::string("\r"), std::string("\xff")}) {
        QVERIFY_EXCEPTION_THROWN(MakeLookupUrl(value), std::invalid_argument);
    }
    const auto resource =
        Assemble(kDictionary,
                 {{"entry", "text/html",
                   "<a href=\"goldendict://resource/id/safe.wav\">inert</a>"}});
    QVERIFY(resource.sanitized_html.find("<a href=") == std::string::npos);
}

void ArticleAssemblerTest::PreservesCanonicalLookupTargets_data() {
    QTest::addColumn<QByteArray>("target");
    for (int byte = 1; byte <= 0x7f; ++byte) {
        if ((byte < 0x20 && (byte < 9 || byte > 13)) || byte == 0x7f) {
            QTest::newRow(qPrintable(QString::number(byte, 16)))
                << QByteArray(1, static_cast<char>(byte));
        }
    }
    QTest::newRow("printable") << QByteArray("ordinary target");
    QTest::newRow("literal-percent-hash-slash-unicode")
        << QByteArray(u8"%01#part/你好");
    QTest::newRow("embedded-controls") << QByteArray(
        "a\x01"
        "b\x7f"
        "c");
}

void ArticleAssemblerTest::PreservesCanonicalLookupTargets() {
    QFETCH(QByteArray, target);
    const auto url = MakeLookupUrl(target.toStdString());
    const auto parsed = ParseInternalUrl(url);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->kind, InternalUrlKind::kLookup);
    QCOMPARE(parsed->target, target.toStdString());
    const auto document = Assemble(
        kDictionary,
        {{"entry", "text/html", "<a href=\"" + url + "\">target</a>"}});
    QVERIFY(document.sanitized_html.find("<a href=\"" + url +
                                         "\">target</a>") != std::string::npos);
    QCOMPARE(document.plain_text, "target");
    QVERIFY(document.resources.empty());
    if (target.size() == 1) {
        QVERIFY_EXCEPTION_THROWN(MakeResourceUrl("id", target.toStdString()),
                                 std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(MakeResourceUrl(target.toStdString(), "name"),
                                 std::invalid_argument);
    }
}

}  // namespace
}  // namespace goldendict::core::article

using goldendict::core::article::ArticleAssemblerTest;

QTEST_APPLESS_MAIN(ArticleAssemblerTest)

#include "article_assembler_test.moc"
