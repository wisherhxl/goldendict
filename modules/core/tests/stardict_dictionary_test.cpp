// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "../src/formats/stardict/stardict_article_decoder.h"
#include "../src/formats/stardict/stardict_dictionary.h"
#include "support/stardict_fixture.h"

namespace goldendict::core::formats::stardict {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class StardictDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesIdentityAndBoundedArticles();
    void ExposesLegacyPrimaryAndHeadwordCounts();
    void ReturnsBoundedPrefixArticles();
    void ReturnsBoundedHeadwordSuggestions();
    void EnumeratesUniqueHeadwordsInLegacyOrder();
    void PreservesFormattedArticleData();
    void DecodesSameTypeSequenceFieldsInOrder();
    void DecodesPerRecordFieldsHandledByQt5();
    void PreservesLegacyXdxfVisualAndResourceSemantics();
    void PreservesNestedPowerWordAndPangoPresentation();
    void UsesFirstStrongDirectionForPreformattedLines();
    void ChecksCancellationDuringArticleDecoding();
    void PreservesPartialArticlesForMalformedFieldTails();
    void PreservesLegacySameSequenceBlobDispatch();
    void RejectsOversizedArticleConversion();
    void BuildsPrimaryOnlyFullTextIndexWithStableProvenance();
    void IndexesDecodedMultiFieldArticleText();
    void ReusesAndRebuildsFullTextIndex();
    void RebuildsFullTextIndexForCurrentStardictSemantics();
    void SkipsLegacyEmptyHeadwordsInFullTextIndex();
    void SearchesCompressedDictionaryText();
    void ContainsFullTextStorageFailures();
    void HonorsCancellationAndDeadline();
    void TranslatesReaderFailures();
    void LoadsTypedResourcesAndLegacyDelimiters();
    void ReturnsMissingResourceWithoutAnError();
    void RejectsUnsafeResourcePaths();
    void RejectsResourceSymlinkEscapes();
    void RejectsOversizedResources();
};

std::filesystem::path TemporaryPath(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
}

std::string LegacyArticle(std::string_view headword, std::string_view body) {
    return "<h3 class=\"sdct_headwords\">" + std::string(headword) + "</h3>" +
           std::string(body);
}

void StardictDictionaryTest::ExposesIdentityAndBoundedArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory),
        {{"example", "first"}, {"example", "second"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    dictionary::RequestOptions options;
    options.result_limit = 1;

    const auto articles = dictionary.LookupExact("example", options);
    auto preferred_info_path = info_path;
    preferred_info_path.make_preferred();

    QCOMPARE(dictionary.identity().id, "fixture-id");
    QCOMPARE(dictionary.identity().name, "Generated Test Dictionary en-en");
    QCOMPARE(dictionary.identity().source, preferred_info_path.string());
    QCOMPARE(dictionary.identity().article_count, std::size_t{2});
    QCOMPARE(dictionary.identity().headword_count, std::size_t{2});
    QCOMPARE(dictionary.identity().description,
             "Author: Fixture Author\n\nFixture description");
    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().headword, "example");
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(articles.front().data,
             LegacyArticle("example",
                           "<div class=\"sdct_m\"><div>first</div></div>"));
}

void StardictDictionaryTest::ExposesLegacyPrimaryAndHeadwordCounts() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"first", "one"}, {"second", "two"}});
    test::WriteStardictSynonyms(
        info_path, {{"alias", 0U}, {"/discard$", 0U}, {"discard/$", 1U}});

    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    QCOMPARE(dictionary.identity().article_count, std::size_t{2});
    QCOMPARE(dictionary.identity().headword_count, std::size_t{5});
    QCOMPARE(
        dictionary.LookupExact("alias").front().data,
        LegacyArticle("first", "<div class=\"sdct_m\"><div>one</div></div>"));
    QVERIFY(dictionary.LookupExact("/discard$").empty());
    QVERIFY(dictionary.LookupExact("discard/$").empty());
}

void StardictDictionaryTest::ReturnsBoundedPrefixArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory),
        {{"example", "exact"}, {"examples", "prefix"}, {"examine", "other"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    dictionary::RequestOptions options;
    options.result_limit = 2U;

    const auto articles = dictionary.LookupPrefix("EXAMPLE", options);

    QCOMPARE(articles.size(), std::size_t{2});
    QCOMPARE(articles[0].data,
             LegacyArticle("example",
                           "<div class=\"sdct_m\"><div>exact</div></div>"));
    QCOMPARE(articles[1].data,
             LegacyArticle("examples",
                           "<div class=\"sdct_m\"><div>prefix</div></div>"));
}

void StardictDictionaryTest::ReturnsBoundedHeadwordSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory),
        {{"example", "exact"}, {"examples", "prefix"}, {"examine", "other"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    dictionary::RequestOptions options;
    options.result_limit = 2U;

    const auto suggestions = dictionary.SuggestPrefix("EXAMPLE", options);

    QCOMPARE(suggestions.size(), std::size_t{2});
    QCOMPARE(suggestions[0], "example");
    QCOMPARE(suggestions[1], "examples");
}

void StardictDictionaryTest::EnumeratesUniqueHeadwordsInLegacyOrder() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory), {{"zebra", "one"},
                                   {"Apple", "two"},
                                   {"apple", "three"},
                                   {"Apple", "duplicate"},
                                   {"\xf0\x90\x80\x80", "non-bmp"},
                                   {"\xee\x80\x80", "bmp"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    dictionary::RequestOptions options;
    options.result_limit = 2U;

    const auto first = dictionary.EnumerateHeadwords(0U, options);
    const auto second = dictionary.EnumerateHeadwords(2U, options);
    const auto third = dictionary.EnumerateHeadwords(4U, options);

    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(first.headwords, (std::vector<std::string>{"Apple", "apple"}));
    QVERIFY(!first.complete);
    QCOMPARE(second.headwords,
             (std::vector<std::string>{"zebra", "\xf0\x90\x80\x80"}));
    QVERIFY(!second.complete);
    QCOMPARE(third.headwords, (std::vector<std::string>{"\xee\x80\x80"}));
    QVERIFY(third.complete);
}

void StardictDictionaryTest::PreservesFormattedArticleData() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto html =
        "<p><b>Example</b> <a href=\"bword://linked\">linked</a>"
        "<img src=\"images/pixel.png\"></p>";
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", html}}, "h");
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    const auto articles = dictionary.LookupExact("example");

    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(articles.front().data,
             LegacyArticle("example", std::string("<div class=\"sdct_h\">") +
                                          html + "</div>"));
}

void StardictDictionaryTest::DecodesSameTypeSequenceFieldsInOrder() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    std::string record = "meaning";
    record.push_back('\0');
    record += "transcription";
    record.push_back('\0');
    record += "kana";
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory), {{"entry", record}}, "mty");

    const auto article =
        Dictionary::Open("fixture-id", info_path).LookupExact("entry").front();

    QCOMPARE(article.format, "text/html");
    QCOMPARE(article.data,
             LegacyArticle("entry",
                           "<div class=\"sdct_m\"><div>meaning</div></div>"
                           "<div class=\"sdct_t\">transcription</div>"
                           "<div class=\"sdct_y\">kana</div>"));
}

void StardictDictionaryTest::DecodesPerRecordFieldsHandledByQt5() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    std::string record;
    const auto append_text = [&record](char type, std::string_view value) {
        record.push_back(type);
        record.append(value);
        record.push_back('\0');
    };
    const auto append_blob = [&record](char type, std::string_view value) {
        record.push_back(type);
        test::AppendBigEndian32(static_cast<std::uint32_t>(value.size()),
                                &record);
        record.append(value);
    };
    append_text('x',
                "<k>key</k><tr>phon</tr><ex author=\"Writer\" "
                "source=\"Corpus\"><ex_orig>usage</ex_orig></ex>"
                "<kref>linked</kref><iref "
                "href=\"https://example.test/reference\">remote</iref>"
                "<rref>pixel.png</rref><rref>spoken.wav</rref>");
    append_text('h',
                "<b>html</b><a href=\"bare link\">linked</a>"
                "<audio src=\"spoken.wav\">listen</audio>");
    append_text('m', "  plain\nnext");
    append_text('l', "local");
    append_text('g',
                "<span weight=\"bold\" foreground=\"#123456\">pango</span>\n"
                "next");
    append_text('t', "<phonetic>");
    append_text('y', "kana & tone");
    append_text('k', "<root><v>&amp;b{bold}</v><v>plain</v></root>");
    append_text('w', "wiki <raw>");
    append_text('n', "wordnet & raw");
    append_text('r', "resource.png");
    append_blob('W', "wave bytes");
    append_blob('P', "picture bytes");
    append_text('q', "unknown <text>");
    append_blob('Z', "unknown blob");
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"entry", record}}, "");

    const auto article =
        Dictionary::Open("fixture-id", info_path).LookupExact("entry").front();

    QCOMPARE(
        article.data,
        LegacyArticle(
            "entry",
            "<div class=\"sdct_x\"> <span class=\"xdxf_k\">key</span>"
            " <span class=\"xdxf_tr_old\">phon</span> <span "
            "class=\"xdxf_ex_old\"><span class=\"xdxf_ex_orig\">usage</span>"
            " <span class=\"xdxf_ex_source\">Writer, Corpus</span></span>"
            " <a class=\"xdxf_kref\" "
            "href=\"bword://linked\">linked</a>"
            " <a href=\"https://example.test/reference\">remote</a>"
            " <img src=\"pixel.png\" alt=\"pixel.png\"> <a class=\"xdxf_wav\" "
            "href=\"sound://spoken.wav\"></a></div>"
            "<div class=\"sdct_h\"><b>html</b><a href=\"bare link\">linked"
            "</a><span class=\"sdict_h_wav\"><a "
            "href=\"sound://spoken.wav\">listen </a></span></div>"
            "<div class=\"sdct_m\"><div>&nbsp;&nbsp;plain</div>"
            "<div>next</div></div><div class=\"sdct_l\"><div>local</div></div>"
            "<div class=\"sdct_g\"><span style=\"font-weight:bold;"
            "color:#123456;\">pango</span><br>next</div>"
            "<div class=\"sdct_t\">&lt;phonetic&gt;</div>"
            "<div class=\"sdct_y\">kana &amp; tone</div>"
            "<div class=\"sdct_k\"><b>bold</b><br>plain<br></div>"
            "<div class=\"sdct_w\">wiki &lt;raw&gt;</div>"
            "<div class=\"sdct_n\">wordnet &amp; raw</div>"
            "<div class=\"sdct_r\">resource.png</div>"
            "<div class=\"sdct_W\">(an embedded .wav file)</div>"
            "<div class=\"sdct_P\">(an embedded picture file)</div>"
            "<b>Unknown textual entry type q:</b> unknown &lt;text&gt;<br>"
            "<b>Unknown blob entry type Z</b><br>"));
}

void StardictDictionaryTest::PreservesLegacyXdxfVisualAndResourceSemantics() {
    const std::string decoded = DecodeArticleFields(
        "<k>first\n  second</k>"
        "<img src=\"normal.png\" losrc=\"small.png\" "
        "hisrc=\"large.png\" alt=\"variants\"/>"
        "<rref start=\"4\">sprite.png</rref>"
        "<iref href=\"\">https://example.test/fallback</iref>"
        "<rref>picture.jpe</rref><rref>picture.tga</rref>"
        "<rref>picture.pcx</rref><rref>audio.au</rref>"
        "<rref>audio.voc</rref><rref>audio.kar</rref>"
        "<rref>audio.mpc</rref><rref>audio.wma</rref>"
        "<rref>audio.wv</rref><rref>audio.ape</rref>"
        "<rref>audio.spx</rref><rref>audio.mpa</rref>"
        "<rref>audio.mp2</rref>",
        "x", "en");

    QVERIFY(decoded.find("first<br>&nbsp;&nbsp;second") != std::string::npos);
    QVERIFY(decoded.find("<img src=\"normal.png\" hisrc=\"large.png\" "
                         "losrc=\"small.png\" alt=\"variants\">") !=
            std::string::npos);
    QVERIFY(decoded.find("<span class=\"xdxf_rref\">sprite.png</span>") !=
            std::string::npos);
    QVERIFY(decoded.find("<a href=\"https://example.test/fallback\">") !=
            std::string::npos);
    for (const std::string_view picture :
         {"picture.jpe", "picture.tga", "picture.pcx"}) {
        QVERIFY2(decoded.find("<img src=\"" + std::string(picture)) !=
                     std::string::npos,
                 picture.data());
    }
    for (const std::string_view sound :
         {"audio.au", "audio.voc", "audio.kar", "audio.mpc", "audio.wma",
          "audio.wv", "audio.ape", "audio.spx", "audio.mpa", "audio.mp2"}) {
        QVERIFY2(decoded.find("href=\"sound://" + std::string(sound)) !=
                     std::string::npos,
                 sound.data());
    }
}

void StardictDictionaryTest::PreservesNestedPowerWordAndPangoPresentation() {
    QCOMPARE(DecodeArticleFields("<root><v>&amp;b{&amp;i{nested}}</v></root>",
                                 "k", "en"),
             std::string("<div class=\"sdct_k\"><i>nested</i><br></div>"));

    const std::string pango = DecodeArticleFields(
        "<span font_desc=\"Noto Sans Bold 12pt\" background=\"#112233\" "
        "underline_color=\"blue\" underline=\"single\" "
        "strikethrough=\"true\" rise=\"1024\" letter_spacing=\"512\">"
        "styled</span>",
        "g", "en");
    QVERIFY(pango.find("font-family:Noto,Sans;") != std::string::npos);
    QVERIFY(pango.find("font-weight:bold;") != std::string::npos);
    QVERIFY(pango.find("font-size:12pt;") != std::string::npos);
    QVERIFY(pango.find("background-color:#112233;") != std::string::npos);
    QVERIFY(pango.find("text-decoration-color:blue;") != std::string::npos);
    QVERIFY(pango.find("text-decoration-line:none;") != std::string::npos);
    QVERIFY(pango.find("vertical-align:1.000pt;") != std::string::npos);
    QVERIFY(pango.find("letter-spacing:0.500pt;") != std::string::npos);
}

void StardictDictionaryTest::UsesFirstStrongDirectionForPreformattedLines() {
    const std::string value = u8"Latin العربية\nالعربية Latin";

    QCOMPARE(DecodeArticleFields(value, "m", "en"),
             std::string(u8"<div class=\"sdct_m\"><div>Latin العربية</div>"
                         u8"<div dir=\"rtl\">العربية Latin</div></div>"));
    QCOMPARE(DecodeArticleFields(value, "m", "ar"),
             std::string(u8"<div class=\"sdct_m\"><div dir=\"ltr\">Latin "
                         u8"العربية</div><div>العربية Latin</div></div>"));

    const std::string article =
        DecodeArticle(u8"العربية", "Latin", "m", "sd", "ug");
    QVERIFY(article.find("class=\"sdct_headwords\" dir=\"rtl\"") !=
            std::string::npos);
    QVERIFY(article.find("<div dir=\"rtl\"><div class=\"sdct_m\">"
                         "<div dir=\"ltr\">Latin</div></div></div>") !=
            std::string::npos);
}

void StardictDictionaryTest::ChecksCancellationDuringArticleDecoding() {
    const auto expect_cancelled = [](std::string record,
                                     std::string_view sequence,
                                     std::size_t cancel_at) {
        std::size_t checkpoints = 0U;
        try {
            static_cast<void>(DecodeArticleFields(
                record, sequence, "en", [&checkpoints, cancel_at]() {
                    ++checkpoints;
                    if (checkpoints == cancel_at) {
                        throw dictionary::Error(
                            dictionary::ErrorCode::kCancelled,
                            "cancelled during decoding");
                    }
                }));
            QFAIL("Article decoding should propagate cancellation checkpoints");
        } catch (const dictionary::Error& error) {
            QCOMPARE(error.code(), dictionary::ErrorCode::kCancelled);
            QCOMPARE(checkpoints, cancel_at);
        }
    };

    expect_cancelled(std::string(512U * 1024U, 'x'), "m", 3U);
    expect_cancelled(std::string(512U * 1024U, 'x'), "h", 3U);
    expect_cancelled(std::string(512U * 1024U, 'x'), "l", 1U);

    std::string per_record = "m";
    per_record.append(512U * 1024U, 'x');
    per_record.push_back('\0');
    per_record += "ttail";
    expect_cancelled(std::move(per_record), "", 3U);
}

void StardictDictionaryTest::PreservesPartialArticlesForMalformedFieldTails() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    std::string per_record = "mfirst";
    per_record.push_back('\0');
    per_record += "htruncated";
    const auto root = TemporaryPath(directory);
    const auto per_record_info =
        test::WriteStardictFixture(root, {{"per-record", per_record}}, "");
    const auto per_record_article =
        Dictionary::Open("per-record", per_record_info)
            .LookupExact("per-record")
            .front();
    QCOMPARE(per_record_article.data,
             LegacyArticle("per-record",
                           "<div class=\"sdct_m\"><div>first</div></div>"));

    QTemporaryDir same_sequence_directory;
    QVERIFY(same_sequence_directory.isValid());
    std::string same_sequence = "first";
    same_sequence.push_back('\0');
    same_sequence += "unterminated";
    const auto same_sequence_info =
        test::WriteStardictFixture(TemporaryPath(same_sequence_directory),
                                   {{"same", same_sequence}}, "mtm");
    const auto same_sequence_article =
        Dictionary::Open("same", same_sequence_info)
            .LookupExact("same")
            .front();
    QCOMPARE(
        same_sequence_article.data,
        LegacyArticle("same", "<div class=\"sdct_m\"><div>first</div></div>"));
}

void StardictDictionaryTest::PreservesLegacySameSequenceBlobDispatch() {
    QTemporaryDir accepted_directory;
    QVERIFY(accepted_directory.isValid());
    const auto accepted_info =
        test::WriteStardictFixture(TemporaryPath(accepted_directory),
                                   {{"accepted", "Accepted bytes"}}, "W");
    QCOMPARE(
        Dictionary::Open("accepted", accepted_info)
            .LookupExact("accepted")
            .front()
            .data,
        LegacyArticle("accepted",
                      "<div class=\"sdct_W\">(an embedded .wav file)</div>"));

    QTemporaryDir rejected_directory;
    QVERIFY(rejected_directory.isValid());
    const auto rejected_info =
        test::WriteStardictFixture(TemporaryPath(rejected_directory),
                                   {{"rejected", "lowercase bytes"}}, "W");
    QCOMPARE(Dictionary::Open("rejected", rejected_info)
                 .LookupExact("rejected")
                 .front()
                 .data,
             LegacyArticle("rejected", ""));
}

void StardictDictionaryTest::RejectsOversizedArticleConversion() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory),
        {{"entry", std::string(16U * 1024U * 1024U + 1U, 'x')}}, "h");

    try {
        static_cast<void>(
            Dictionary::Open("fixture-id", info_path).LookupExact("entry"));
        QFAIL("LookupExact should reject oversized article conversion");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

void StardictDictionaryTest::
    BuildsPrimaryOnlyFullTextIndexWithStableProvenance() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root,
        {{"plain", "primary searchable text"},
         {"html", "<script>hidden term</script><b>HTML searchable</b>"}},
        "h");
    test::WriteStardictSynonyms(info_path, {{"alias", 0U}});
    const auto full_text_path = root / "fixture.gdfts";
    const Dictionary dictionary = Dictionary::Open(
        "fixture-id", info_path, root / "fixture.gdidx", full_text_path);

    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);

    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(response.results.size(), 2U);
    QCOMPARE(response.results[0].dictionary.id, std::string("fixture-id"));
    QCOMPARE(response.results[0].headword, std::string("html"));
    QVERIFY(response.results[0].document_id.rfind("stardict-idx:1:", 0U) == 0U);
    QCOMPARE(response.results[1].headword, std::string("plain"));
    QVERIFY(response.results[1].document_id.rfind("stardict-idx:0:", 0U) == 0U);
    query.text = "hidden";
    QVERIFY(dictionary.SearchFullText(query).results.empty());
    query.text = "primary";
    const auto primary = dictionary.SearchFullText(query);
    QCOMPARE(primary.results.size(), 1U);
    QCOMPARE(primary.results.front().headword, std::string("plain"));
}

void StardictDictionaryTest::IndexesDecodedMultiFieldArticleText() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    std::string record = "first searchable";
    record.push_back('\0');
    record += "<b>second searchable</b>";
    const auto info_path = test::WriteStardictFixture(
        root, {{"entry", record}, {"other", "unrelated\0ignored"}}, "mh");
    const Dictionary dictionary =
        Dictionary::Open("fixture-id", info_path, root / "fixture.gdidx",
                         root / "fixture.gdfts");

    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);

    QCOMPARE(response.results.size(), 1U);
    QCOMPARE(response.results.front().headword, std::string("entry"));
    QVERIFY(response.results.front().excerpt.find("first searchable") !=
            std::string::npos);
    QVERIFY(response.results.front().excerpt.find("second searchable") !=
            std::string::npos);
}

void StardictDictionaryTest::ReusesAndRebuildsFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"entry", "first text"}});
    const auto generated_path = root / "fixture.gdidx";
    const auto full_text_path = root / "fixture.gdfts";
    QCOMPARE(Dictionary::Open("fixture-id", info_path, generated_path,
                              full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(Dictionary::Open("fixture-id", info_path, generated_path,
                              full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));

    test::WriteStardictFixture(root, {{"entry", "second text"}});
    QCOMPARE(Dictionary::Open("fixture-id", info_path, generated_path,
                              full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    test::WriteBinaryFile(full_text_path, "corrupt");
    QCOMPARE(Dictionary::Open("fixture-id", info_path, generated_path,
                              full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));
}

void StardictDictionaryTest::
    RebuildsFullTextIndexForCurrentStardictSemantics() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"entry", "searchable text"}});
    const auto full_text_path = root / "fixture.gdfts";
    const Reader reader = Reader::Open(info_path);
    dictionary::FullTextDocument old_document;
    old_document.dictionary.id = "fixture-id";
    old_document.headword = "obsolete";
    old_document.document_id = "obsolete:0";
    old_document.plain_text = "obsolete text";
    QCOMPARE(dictionary::FullTextIndex::OpenOrBuild(
                 full_text_path, reader.source_snapshot(), {old_document})
                 .state(),
             dictionary::FullTextIndexState::kCreated);

    const Dictionary dictionary = Dictionary::Open(
        "fixture-id", info_path, root / "fixture.gdidx", full_text_path);

    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(dictionary.SearchFullText(query).results.size(), 1U);
    query.text = "obsolete";
    QVERIFY(dictionary.SearchFullText(query).results.empty());
}

void StardictDictionaryTest::SkipsLegacyEmptyHeadwordsInFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root, {{"broken&#65;", "hidden text"}, {"visible", "searchable text"}});
    const Dictionary dictionary =
        Dictionary::Open("fixture-id", info_path, root / "fixture.gdidx",
                         root / "fixture.gdfts");

    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QCOMPARE(response.results.size(), 1U);
    QCOMPARE(response.results.front().headword, std::string("visible"));
}

void StardictDictionaryTest::SearchesCompressedDictionaryText() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path = test::WriteStardictFixture(
        root, {{"compressed", "compressed searchable text"}});
    test::CompressStardictDictionary(info_path);
    std::filesystem::remove(root / "fixture.dict");
    const Dictionary dictionary =
        Dictionary::Open("fixture-id", info_path, root / "fixture.gdidx",
                         root / "fixture.gdfts");
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(dictionary.SearchFullText(query).results.size(), 1U);
}

void StardictDictionaryTest::ContainsFullTextStorageFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"entry", "searchable text"}});
    const auto full_text_path = root / "fixture.gdfts";
    QVERIFY(std::filesystem::create_directory(full_text_path));

    const Dictionary dictionary = Dictionary::Open(
        "fixture-id", info_path, root / "fixture.gdidx", full_text_path);
    QCOMPARE(dictionary.LookupExact("entry").size(), 1U);
    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QVERIFY(response.results.empty());
    QCOMPARE(response.errors.size(), 1U);
    QCOMPARE(response.errors.front().code, FullTextErrorCode::kInternal);
    QCOMPARE(response.errors.front().dictionary_id, std::string("fixture-id"));
}

void StardictDictionaryTest::HonorsCancellationAndDeadline() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", "article"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    CancelledSignal cancellation;
    dictionary::RequestOptions cancelled;
    cancelled.cancellation = &cancellation;

    try {
        static_cast<void>(dictionary.LookupExact("example", cancelled));
        QFAIL("LookupExact should honor cancellation");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kCancelled);
    }
    try {
        static_cast<void>(dictionary.GetResource("resource.bin", cancelled));
        QFAIL("GetResource should honor cancellation");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kCancelled);
    }

    dictionary::RequestOptions expired;
    expired.deadline = std::chrono::steady_clock::time_point::min();
    try {
        static_cast<void>(dictionary.LookupExact("example", expired));
        QFAIL("LookupExact should honor an expired deadline");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kDeadlineExceeded);
    }
}

void StardictDictionaryTest::TranslatesReaderFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto missing = TemporaryPath(directory) / "missing.ifo";

    try {
        static_cast<void>(Dictionary::Open("missing", missing));
        QFAIL("Dictionary::Open should translate missing input");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kUnavailable);
    }
}

void StardictDictionaryTest::LoadsTypedResourcesAndLegacyDelimiters() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const std::string image_data("\x89PNG\r\n\x1a\nfixture", 15);
    test::WriteStardictResource(root, "images/pixel.png", image_data);
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    const auto resource = dictionary.GetResource("\x1eimages/pixel.png\x1f");

    QVERIFY(resource.has_value());
    QCOMPARE(resource->id, "images/pixel.png");
    QCOMPARE(resource->media_type, "image/png");
    const std::string loaded(
        reinterpret_cast<const char*>(resource->data.data()),
        resource->data.size());
    QCOMPARE(loaded, image_data);
}

void StardictDictionaryTest::ReturnsMissingResourceWithoutAnError() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", "article"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    QVERIFY(!dictionary.GetResource("images/missing.png").has_value());
}

void StardictDictionaryTest::RejectsUnsafeResourcePaths() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    test::WriteStardictResource(root, "safe.txt", "safe");
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    const std::vector<std::string> unsafe_paths = {
        "../fixture.dict", "images/../../fixture.dict", "/etc/passwd",
        "..\\fixture.dict"};
    for (const auto& path : unsafe_paths) {
        try {
            static_cast<void>(dictionary.GetResource(path));
            QFAIL("GetResource should reject an unsafe path");
        } catch (const dictionary::Error& error) {
            QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
        }
    }
}

void StardictDictionaryTest::RejectsResourceSymlinkEscapes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto outside = root / "outside.txt";
    test::WriteBinaryFile(outside, "outside");
    const auto resource_root = root / "res";
    QVERIFY(std::filesystem::create_directory(resource_root));
    std::error_code symlink_error;
    std::filesystem::create_symlink(outside, resource_root / "escape.txt",
                                    symlink_error);
    if (symlink_error)
        QSKIP("File symlink creation is unavailable");
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    try {
        static_cast<void>(dictionary.GetResource("escape.txt"));
        QFAIL("GetResource should reject a symlink escape");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

void StardictDictionaryTest::RejectsOversizedResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto resource = test::WriteStardictResource(root, "large.bin", "");
    std::filesystem::resize_file(resource, 64U * 1024U * 1024U + 1U);
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    try {
        static_cast<void>(dictionary.GetResource("large.bin"));
        QFAIL("GetResource should reject an oversized resource");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

}  // namespace
}  // namespace goldendict::core::formats::stardict

using goldendict::core::formats::stardict::StardictDictionaryTest;

QTEST_APPLESS_MAIN(StardictDictionaryTest)

#include "stardict_dictionary_test.moc"
