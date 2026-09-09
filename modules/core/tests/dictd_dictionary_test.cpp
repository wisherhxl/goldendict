// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include "../src/article/article_assembler.h"
#include "../src/formats/dictd/dictd_article_renderer.h"
#include "../src/formats/dictd/dictd_dictionary.h"
#include "../src/foundation/legacy_language_pair.h"
#include "support/dictd_fixture.h"

namespace goldendict::core::formats::dictd {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class FullTextCancelledToken final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class SlowFullTextToken final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return false;
    }
};

class DictdDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ReusesFullTextWithPhysicalHeadwordCounts();
    void ReusesCorrectedTitles_data();
    void ReusesCorrectedTitles();
    void RebuildsFormerTitleFullTextIndex();
    void ExposesLaidOutArticlesAndSuggestions();
    void RendersFrozenLayout_data();
    void RendersFrozenLayout();
    void UsesOnlyFilenameDirection();
    void BoundsAndCancelsRendering();
    void RebuildsFormerPlainTextLayoutIndex();
    void HonorsCancellationAndHasNoResources();
    void BuildsRangeDeduplicatedFullTextIndex();
    void ReusesAndRebuildsFullTextIndexForBothSources();
    void SearchesGzipAndContainsFullTextFailures();
    void ReusesRealDictzipCompanions_data();
    void ReusesRealDictzipCompanions();
    void RebuildsPreContentDetectionFullTextIndex();
    void ReusesRecoveredIndexRows();
    void RejectsAcceptedCorruptionAfterSkippedRow();
    void RejectsRaHeaderBeforeReusingFullText_data();
    void RejectsRaHeaderBeforeReusingFullText();
};

void DictdDictionaryTest::RejectsRaHeaderBeforeReusingFullText_data() {
    QTest::addColumn<QString>("suffix");
    QTest::newRow("dict") << ".dict";
    QTest::newRow("dz") << ".dict.dz";
}

void DictdDictionaryTest::RejectsRaHeaderBeforeReusingFullText() {
    QFETCH(QString, suffix);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"entry", "searchable article", {}}});
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    const auto selected = root / ("fixture" + suffix.toStdString());
    auto bytes = test::EncodeDictzipFixture("searchable article");
    bytes[16U] = 2;
    std::ofstream(selected, std::ios::binary)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const auto original_sources =
        dictionary::CaptureSourceSnapshot({index, selected});
    auto sources = original_sources;
    sources.push_back({"goldendict:dictd-content-detection-v1", 0U, 0});
    sources.push_back({"goldendict:dictd-title-metadata-v1", 0U, 0});
    dictionary::FullTextDocument document;
    document.dictionary.id = "dictd-id";
    document.dictionary.name = "fixture";
    document.headword = "entry";
    document.document_id = "dictd-index:0:0:18";
    document.plain_text = "searchable article";
    const auto full_text_path = root / "fixture.gdfts";
    // This is the actual former decoded document, bound to the now-rejected
    // source's unchanged stamps and current private semantic keys.
    const auto seeded = dictionary::FullTextIndex::OpenOrBuild(
        full_text_path, sources, {document});
    const auto reused = dictionary::FullTextIndex::OpenOrBuild(
        full_text_path, sources, {document});
    QCOMPARE(reused.state(), dictionary::FullTextIndexState::kReused);
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(seeded.Search(query).results.size(), 1U);
    const auto read_artifact = [&]() {
        std::ifstream input(full_text_path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>());
    };
    const auto before_bytes = read_artifact();
    const auto before_time = std::filesystem::last_write_time(full_text_path);
    try {
        static_cast<void>(Dictionary::Open("dictd-id", index, full_text_path));
        QFAIL("Valid former artifact must not bypass RA header admission");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
    QCOMPARE(read_artifact(), before_bytes);
    QCOMPARE(std::filesystem::last_write_time(full_text_path), before_time);
    QCOMPARE(dictionary::CaptureSourceSnapshot({index, selected}),
             original_sources);
}

void DictdDictionaryTest::ReusesFullTextWithPhysicalHeadwordCounts() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"entry", "searchable article", {}}});
    std::ofstream(index, std::ios::binary | std::ios::app)
        << "entry\tA\tS\t\r\nentry\tA\tS\tentry";
    const auto sources =
        dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"});
    const auto full_text_path = root / "fixture.gdfts";
    std::string cold_bytes;
    std::filesystem::file_time_type cold_time;
    FullTextQuery query;
    query.text = "searchable";
    for (const bool warm : {false, true}) {
        const auto dictionary =
            Dictionary::Open("dictd-id", index, full_text_path);
        QCOMPARE(dictionary.identity().headword_count, 5U);
        QCOMPARE(dictionary.identity().article_count, 3U);
        QCOMPARE(dictionary.identity().id, "dictd-id");
        QCOMPARE(dictionary.identity().name, "fixture");
        QCOMPARE(dictionary.identity().source,
                 std::filesystem::weakly_canonical(index).string());
        QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
                 (std::vector<std::string>{"entry"}));
        const auto articles = dictionary.LookupExact("entry", {});
        QCOMPARE(articles.size(), 1U);
        QCOMPARE(article::Assemble(dictionary.identity(), articles).plain_text,
                 "searchable article");
        QCOMPARE(
            dictionary.full_text_index_state(),
            std::optional(warm ? dictionary::FullTextIndexState::kReused
                               : dictionary::FullTextIndexState::kCreated));
        const auto response = dictionary.SearchFullText(query);
        QVERIFY(response.errors.empty());
        QVERIFY(!response.partial);
        QCOMPARE(response.results.size(), 1U);
        QCOMPARE(response.results.front().dictionary.id, "dictd-id");
        QCOMPARE(response.results.front().dictionary.name, "fixture");
        QCOMPARE(response.results.front().headword, "entry");
        QCOMPARE(response.results.front().document_id, "dictd-index:0:0:18");
        QCOMPARE(
            dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"}),
            sources);
        std::ifstream input(full_text_path, std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(input), {}};
        const auto timestamp = std::filesystem::last_write_time(full_text_path);
        if (warm) {
            QCOMPARE(bytes, cold_bytes);
            QCOMPARE(timestamp, cold_time);
        } else {
            cold_bytes = bytes;
            cold_time = timestamp;
        }
    }
}

void DictdDictionaryTest::ReusesCorrectedTitles_data() {
    QTest::addColumn<QString>("companion");
    QTest::addColumn<QByteArray>("title");
    for (const QString companion : {"plain", "ra-dict", "ra-dz"}) {
        QTest::newRow((companion + "-title").toLatin1().constData())
            << companion << QByteArray("Last \r");
        QTest::newRow((companion + "-empty").toLatin1().constData())
            << companion << QByteArray("");
    }
}

void DictdDictionaryTest::ReusesCorrectedTitles() {
    QFETCH(QString, companion);
    QFETCH(QByteArray, title);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root,
        {{"00databaseshort", "First", {}},
         {"00-database-short", "00-database-short\n" + title.toStdString(), {}},
         {"entry", "searchable article", "original"},
         {"00databaseinfo", "Description", {}}});
    auto selected = root / "fixture.dict";
    if (companion != "plain") {
        std::ifstream input(selected, std::ios::binary);
        const std::string data{std::istreambuf_iterator<char>(input), {}};
        input.close();
        const auto bytes = test::EncodeDictzipFixture(data);
        if (companion == "ra-dz") {
            QVERIFY(std::filesystem::remove(selected));
            selected += ".dz";
        }
        std::ofstream(selected, std::ios::binary | std::ios::trunc)
            .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    const auto sources = dictionary::CaptureSourceSnapshot({index, selected});
    const auto full_text_path = root / "fixture.gdfts";
    FullTextQuery query;
    query.text = "searchable";
    for (const bool warm : {false, true}) {
        const auto dictionary =
            Dictionary::Open("dictd-id", index, full_text_path);
        QCOMPARE(dictionary.identity().name, title.toStdString());
        QCOMPARE(dictionary.identity().id, "dictd-id");
        QCOMPARE(dictionary.identity().description, "Description");
        QCOMPARE(dictionary.identity().article_count, 4U);
        QCOMPARE(dictionary.identity().headword_count, 5U);
        QCOMPARE(dictionary.identity().source,
                 std::filesystem::weakly_canonical(index).string());
        QCOMPARE(article::Assemble(dictionary.identity(),
                                   dictionary.LookupExact("original", {}))
                     .plain_text,
                 "searchable article");
        QCOMPARE(
            dictionary.full_text_index_state(),
            std::optional(warm ? dictionary::FullTextIndexState::kReused
                               : dictionary::FullTextIndexState::kCreated));
        const auto response = dictionary.SearchFullText(query);
        QVERIFY(response.errors.empty());
        QVERIFY(!response.partial);
        QCOMPARE(response.results.size(), 1U);
        QCOMPARE(response.results.front().dictionary.id, "dictd-id");
        QCOMPARE(response.results.front().dictionary.name, title.toStdString());
        QCOMPARE(response.results.front().headword, "entry");
        QCOMPARE(response.results.front().document_id,
                 "dictd-index:2:" + std::to_string(23U + title.size()) + ":18");
        QCOMPARE(dictionary::CaptureSourceSnapshot({index, selected}), sources);
    }
}

void DictdDictionaryTest::RebuildsFormerTitleFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"00databaseshort", "First", {}},
                                       {"00-database-short", "Last", {}},
                                       {"entry", "searchable article", {}}});
    const auto sources =
        dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"});
    auto former_sources = sources;
    former_sources.push_back({"goldendict:dictd-content-detection-v1", 0U, 0});
    dictionary::FullTextDocument former;
    former.dictionary.id = "dictd-id";
    former.dictionary.name = "First";
    former.headword = "entry";
    former.document_id = "dictd-index:2:9:18";
    former.plain_text = "searchable article";
    const auto full_text_path = root / "fixture.gdfts";
    const auto old_index = dictionary::FullTextIndex::OpenOrBuild(
        full_text_path, former_sources, {former});
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(old_index.Search(query).results.front().dictionary.name, "First");
    // Reopening the former source key really reuses its persisted wrong name.
    const auto old_reopened = dictionary::FullTextIndex::OpenOrBuild(
        full_text_path, former_sources, {former});
    QCOMPARE(old_reopened.state(), dictionary::FullTextIndexState::kReused);
    QCOMPARE(old_reopened.Search(query).results.front().dictionary.name,
             "First");
    for (const bool warm : {false, true}) {
        const auto dictionary =
            Dictionary::Open("dictd-id", index, full_text_path);
        QCOMPARE(dictionary.full_text_index_state(),
                 std::optional(
                     warm ? dictionary::FullTextIndexState::kReused
                          : dictionary::FullTextIndexState::kRebuiltStale));
        QCOMPARE(dictionary.identity().name, "Last");
        const auto response = dictionary.SearchFullText(query);
        QVERIFY(response.errors.empty());
        QCOMPARE(response.results.size(), 1U);
        QCOMPARE(response.results.front().dictionary.name, "Last");
        QCOMPARE(response.results.front().document_id, former.document_id);
        QCOMPARE(
            dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"}),
            sources);
    }
}

void DictdDictionaryTest::ReusesRecoveredIndexRows() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"first", "first searchable", "original"},
               {"second", "second searchable", {}}});
    std::ifstream input(index, std::ios::binary);
    const std::string original{std::istreambuf_iterator<char>(input), {}};
    input.close();
    std::ofstream(index, std::ios::binary | std::ios::trunc)
        << "ignored\t!\t!\talias\textra\n"
        << original << "ignored\n";
    const auto sources =
        dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"});
    const auto full_text_path = root / "fixture.gdfts";
    FullTextQuery query;
    query.text = "searchable";
    for (const bool warm : {false, true}) {
        const auto dictionary =
            Dictionary::Open("dictd-id", index, full_text_path);
        QCOMPARE(dictionary.identity().id, "dictd-id");
        QCOMPARE(dictionary.identity().source,
                 std::filesystem::weakly_canonical(index).string());
        QCOMPARE(dictionary.identity().article_count, 2U);
        QCOMPARE(dictionary.identity().headword_count, 3U);
        QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
                 (std::vector<std::string>{"first", "original", "second"}));
        QCOMPARE(article::Assemble(dictionary.identity(),
                                   dictionary.LookupExact("original", {}))
                     .plain_text,
                 "first searchable");
        QVERIFY(dictionary.LookupExact("ignored", {}).empty());
        QCOMPARE(
            dictionary.full_text_index_state(),
            std::optional(warm ? dictionary::FullTextIndexState::kReused
                               : dictionary::FullTextIndexState::kCreated));
        const auto response = dictionary.SearchFullText(query);
        QVERIFY(response.errors.empty());
        QVERIFY(!response.partial);
        QCOMPARE(response.results.size(), 2U);
        QCOMPARE(response.results[0].headword, "first");
        QCOMPARE(response.results[0].document_id, "dictd-index:1:0:16");
        QCOMPARE(response.results[1].headword, "second");
        QCOMPARE(response.results[1].document_id, "dictd-index:2:16:17");
        QCOMPARE(
            dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"}),
            sources);
    }
}

void DictdDictionaryTest::RejectsAcceptedCorruptionAfterSkippedRow() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(root, {{"entry", "data", {}}});
    std::ofstream(index, std::ios::binary | std::ios::trunc)
        << "ignored\nentry\t!\tB\n";
    const auto full_text_path = root / "fixture.gdfts";
    try {
        static_cast<void>(Dictionary::Open("dictd-id", index, full_text_path));
        QFAIL("Accepted corrupt rows must still report invalid data");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
        QVERIFY(std::string(error.what()).find("line 2") != std::string::npos);
    }
    QVERIFY(!std::filesystem::exists(full_text_path));
}

void DictdDictionaryTest::ExposesLaidOutArticlesAndSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"example", "definition", "Example Original"},
               {"examples", "plural", {}},
               {"00databaseinfo", "Fixture description", {}}});
    const Dictionary dictionary = Dictionary::Open("dictd-id", index);
    dictionary::RequestOptions options;
    options.result_limit = 1U;

    const auto articles = dictionary.LookupPrefix("EXAMPLE", options);
    const auto suggestions = dictionary.SuggestPrefix("EXAMPLE", options);

    QCOMPARE(dictionary.identity().id, "dictd-id");
    QCOMPARE(dictionary.identity().name, "fixture");
    QCOMPARE(dictionary.identity().article_count, std::size_t{3});
    QCOMPARE(dictionary.identity().headword_count, std::size_t{4});
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"00databaseinfo", "Example Original",
                                       "example", "examples"}));
    QCOMPARE(dictionary.identity().description, "Fixture description");
    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(articles.front().data,
             "<div class=\"dictd_article\"><div>definition</div></div>");
    QCOMPARE(suggestions.size(), std::size_t{1});
    QCOMPARE(suggestions.front(), "example");
}

void DictdDictionaryTest::RendersFrozenLayout_data() {
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QByteArray>("language");
    QTest::addColumn<QByteArray>("expected");
    const auto row = [](const char* name, QByteArray body, QByteArray expected,
                        QByteArray language = {}) {
        QTest::newRow(name) << body << language << expected;
    };
    row("empty", {}, "<div class=\"dictd_article\"></div>");
    row("indent-crlf-blank-final",
        "  indented\n\tTabbed\r\ninterior  spaces\tstay\n\nlast\n",
        "<div class=\"dictd_article\"><div>&nbsp;&nbsp;indented</div>"
        "<div>&nbsp;&nbsp;&nbsp;&nbsp;Tabbed</div><div>interior  "
        "spaces\tstay</div>"
        "<div></div><div>last</div></div>");
    row("escaped", "<script>alert(\"x\")</script>&amp;\n< > \" &",
        "<div "
        "class=\"dictd_article\"><div>&lt;script&gt;alert(&quot;x&quot;)&lt;/"
        "script&gt;&amp;amp;</div>"
        "<div>&lt; &gt; &quot; &amp;</div></div>");
    row("nul", QByteArray("before\0hidden\nsecret", 20),
        "<div class=\"dictd_article\"><div>before</div></div>");
    row("cr-leading", " \r\ttext\r\n\r\n ",
        "<div "
        "class=\"dictd_article\"><div>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;text</div>"
        "<div></div><div>&nbsp;</div></div>");
    row("malformed",
        QByteArray::fromHex(
            "41c0af42e28243eda08044f4908080458046f09f47ff48e282"),
        QString::fromUtf8(u8"<div "
                          u8"class=\"dictd_article\"><div>"
                          u8"A��B��C���D����E�F��G�H��</div></div>")
            .toUtf8());
    row("cr-before-utf8", QByteArray::fromHex("c30da9"),
        QString::fromUtf8(u8"<div class=\"dictd_article\"><div>é</div></div>")
            .toUtf8());
    row("bom-retained", QByteArray::fromHex("efbbbf410aefbbbf42"),
        QByteArray("<div class=\"dictd_article\"><div>") +
            QByteArray::fromHex("efbbbf") + "A</div><div>" +
            QByteArray::fromHex("efbbbf") + "B</div></div>");
    row("bidi",
        u8"123 שלום\n123 English\n\n123\n𐤀 ancient\n\u2067שלום\u2069 English",
        u8"<div class=\"dictd_article\"><div dir=\"rtl\">123 "
        u8"שלום</div><div>123 English</div>"
        u8"<div></div><div>123</div><div dir=\"rtl\">𐤀 "
        u8"ancient</div><div>\u2067שלום\u2069 English</div></div>");
    row("bidi-base-rtl", u8"שלום\nEnglish\n\n123",
        u8"<div class=\"dictd_article\" dir=\"rtl\"><div>שלום</div><div "
        u8"dir=\"ltr\">English</div>"
        "<div dir=\"ltr\"></div><div dir=\"ltr\">123</div></div>",
        "ar");
    row("literal-entities-direction", u8"&nbsp;שלום\n<שלום>\n\"שלום",
        u8"<div class=\"dictd_article\"><div>&amp;nbsp;שלום</div><div "
        u8"dir=\"rtl\">&lt;שלום&gt;</div>"
        u8"<div dir=\"rtl\">&quot;שלום</div></div>");
    row("isolate-nesting-and-unmatched",
        u8"\u2068שלום\u2066English\u2069\u2069שלום\n\u2067שלום\n\u2069שלום\n"
        u8"\u202bEnglish\u202cשלום",
        u8"<div class=\"dictd_article\"><div "
        u8"dir=\"rtl\">\u2068שלום\u2066English\u2069\u2069שלום</div>"
        u8"<div>\u2067שלום</div><div "
        u8"dir=\"rtl\">\u2069שלום</div><div>\u202bEnglish\u202cשלום</div></"
        u8"div>");
    row("inline-markers-remain", "\\phonetic\\ {reference}",
        "<div class=\"dictd_article\"><div>\\phonetic\\ "
        "{reference}</div></div>");
}

void DictdDictionaryTest::RendersFrozenLayout() {
    QFETCH(QByteArray, body);
    QFETCH(QByteArray, language);
    QFETCH(QByteArray, expected);
    QCOMPARE(RenderArticleBody(body.toStdString(), language.toStdString()),
             expected.toStdString());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto index =
        test::WriteDictdFixture(root, {{"entry", body.toStdString(), {}}});
    if (!language.isEmpty()) {
        const auto base = root / ("en-" + language.toStdString());
        std::filesystem::rename(index, base.string() + ".index");
        std::filesystem::rename(root / "fixture.dict", base.string() + ".dict");
        index = base.string() + ".index";
    }
    const auto dictionary = Dictionary::Open("dictd-id", index);
    for (const auto articles :
         {dictionary.LookupExact("entry"), dictionary.LookupPrefix("ent")}) {
        QCOMPARE(articles.size(), 1U);
        QCOMPARE(articles.front().format, "text/html");
        QCOMPARE(articles.front().data, expected.toStdString());
        const auto document =
            article::Assemble(dictionary.identity(), articles);
        QVERIFY(document.sanitized_html.find("class=\"dictd_article\"") !=
                std::string::npos);
        QVERIFY(document.sanitized_html.find("<pre>") == std::string::npos);
        QVERIFY(document.sanitized_html.find("<script>") == std::string::npos);
        QVERIFY(document.resources.empty());
    }
}

void DictdDictionaryTest::UsesOnlyFilenameDirection() {
    for (const auto& [filename, right_to_left] :
         std::vector<std::pair<std::string, bool>>{{"neutral", false},
                                                   {"EN-AR", true},
                                                   {"en-ara", true},
                                                   {"en-arm", false},
                                                   {"en-heb", true},
                                                   {"en-per", true},
                                                   {"en-xxx", false},
                                                   {"en-arg", false},
                                                   {"en-arx", true},
                                                   {"longen-ar", false},
                                                   {"en-arabic", false}}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root =
            std::filesystem::path(directory.path().toStdString()) / "en-ar";
        const auto old_index = test::WriteDictdFixture(
            root, {{"00databaseshort", "x\nen-ar metadata title\n", {}},
                   {"entry", "123", {}}});
        const auto base = root / filename;
        std::filesystem::rename(old_index, base.string() + ".index");
        std::filesystem::rename(root / "fixture.dict", base.string() + ".dict");
        const auto dictionary =
            Dictionary::Open("dictd-id", base.string() + ".index");
        const auto body = dictionary.LookupExact("entry").front().data;
        QCOMPARE(body,
                 right_to_left
                     ? "<div class=\"dictd_article\" dir=\"rtl\"><div "
                       "dir=\"ltr\">123</div></div>"
                     : "<div class=\"dictd_article\"><div>123</div></div>");
        QVERIFY(dictionary.identity().source_language.empty());
        QVERIFY(dictionary.identity().target_language.empty());
    }
    // Every frozen three-letter mapping moved intact to the shared primitive.
    const std::pair<std::string_view, std::string_view> codes[] = {
#include "../src/foundation/legacy_language_codes.inc"
    };
    for (const auto& [code, expected] : codes) {
        QCOMPARE(foundation::InferLegacyLanguagePair("en-" + std::string(code))
                     .second,
                 std::string(expected));
    }
}

void DictdDictionaryTest::BoundsAndCancelsRendering() {
    constexpr std::size_t limit = 16U * 1024U * 1024U;
    for (const auto& body : {std::string(limit / 6U + 1U, ' '),
                             std::string(limit / 3U + 1U, '\xff'),
                             std::string(limit / 11U + 1U, '\n')}) {
        QVERIFY_EXCEPTION_THROWN(RenderArticleBody(body, ""),
                                 dictionary::Error);
    }
    const std::string many_cr(65536U, '\r');
    const std::string long_line(65536U, 'x');
    for (const auto& body : {many_cr, long_line}) {
        std::size_t calls = 0U;
        static_cast<void>(RenderArticleBody(body, "", [&]() { ++calls; }));
        QVERIFY(calls > 8U);
        for (const auto stop : std::array<std::size_t, 4U>{
                 2U, calls / 2U, calls * 3U / 4U, calls - 1U}) {
            std::size_t current = 0U;
            try {
                RenderArticleBody(body, "", [&]() {
                    if (++current == stop) {
                        throw dictionary::Error(
                            dictionary::ErrorCode::kCancelled, "cancelled");
                    }
                });
                QFAIL("Renderer ignored cancellation");
            } catch (const dictionary::Error& error) {
                QCOMPARE(error.code(), dictionary::ErrorCode::kCancelled);
                QCOMPARE(current, stop);
            }
        }
    }
}

void DictdDictionaryTest::RebuildsFormerPlainTextLayoutIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    std::string body =
        u8"  "
        u8"visible\r\n<b>literal</b>&amp;\n\u00a0literalnbsp\n&nbsp;entityword";
    body.append("\0hidden", 7U);
    const auto index = test::WriteDictdFixture(root, {{"entry", body, {}}});
    const auto source_snapshot =
        dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"});
    auto old_sources = source_snapshot;
    old_sources.push_back({"goldendict:dictd-content-detection-v1", 0U, 0});
    old_sources.push_back({"goldendict:dictd-title-metadata-v1", 0U, 0});
    dictionary::FullTextDocument former;
    former.dictionary.id = "dictd-id";
    former.dictionary.name = "fixture";
    former.headword = "entry";
    former.document_id = "dictd-index:0:0:" + std::to_string(body.size());
    former.plain_text = body;
    const auto cache = root / "fixture.gdfts";
    static_cast<void>(
        dictionary::FullTextIndex::OpenOrBuild(cache, old_sources, {former}));
    const auto old =
        dictionary::FullTextIndex::OpenOrBuild(cache, old_sources, {former});
    QCOMPARE(old.state(), dictionary::FullTextIndexState::kReused);
    for (const bool warm : {false, true}) {
        const auto dictionary = Dictionary::Open("dictd-id", index, cache);
        QCOMPARE(dictionary.full_text_index_state(),
                 std::optional(
                     warm ? dictionary::FullTextIndexState::kReused
                          : dictionary::FullTextIndexState::kRebuiltStale));
        const auto assembled = article::Assemble(
            dictionary.identity(), dictionary.LookupExact("entry"));
        QCOMPARE(assembled.plain_text,
                 u8"\u00a0\u00a0visible\n<b>literal</"
                 u8"b>&amp;\n\u00a0literalnbsp\n&nbsp;entityword");
        const auto resolved =
            dictionary.ResolveFullTextDocument(former.document_id);
        QVERIFY(resolved.has_value());
        FullTextQuery query;
        query.text = "hidden";
        QVERIFY(dictionary.SearchFullText(query).results.empty());
        query.text = "visible";
        const auto response = dictionary.SearchFullText(query);
        QVERIFY(response.errors.empty());
        QCOMPARE(response.results.size(), 1U);
        QCOMPARE(response.results.front().document_id, former.document_id);
        QCOMPARE(
            response.results.front().excerpt,
            "  visible\n<b>literal</b>&amp;\n literalnbsp\n&nbsp;entityword");
        query.text = "literalnbsp";
        QCOMPARE(dictionary.SearchFullText(query).results.size(), 1U);
        QCOMPARE(
            dictionary::CaptureSourceSnapshot({index, root / "fixture.dict"}),
            source_snapshot);
    }
}

void DictdDictionaryTest::BuildsRangeDeduplicatedFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root,
        {{"00databaseshort", "a", ""},
         {"00-database-short", "b", ""},
         {"00databaseinfo", "c", ""},
         {"00-database-info", "d", ""},
         {"canonical", "visible searchable <b>literal</b>", "originalalias"},
         {"lateralias", "alias-only-secret", "", 4U},
         {"second", "other searchable", ""}});
    const Dictionary dictionary =
        Dictionary::Open("dictd-id", index, root / "fixture.gdfts");

    FullTextQuery query;
    query.text = "searchable";
    const auto response = dictionary.SearchFullText(query);
    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(response.results.size(), std::size_t{2});
    QCOMPARE(response.results[0].headword, std::string("canonical"));
    QCOMPARE(response.results[0].document_id,
             std::string("dictd-index:4:4:33"));
    QCOMPARE(response.results[1].headword, std::string("second"));
    for (const std::string excluded :
         {"originalalias", "lateralias", "alias-only-secret"}) {
        query.text = excluded;
        QVERIFY2(dictionary.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
}

void DictdDictionaryTest::ReusesAndRebuildsFullTextIndexForBothSources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto index =
        test::WriteDictdFixture(root, {{"entry", "first searchable text", ""}});
    const auto full_text_path = root / "fixture.gdfts";
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kReused));

    index = test::WriteDictdFixture(
        root, {{"entry", "second searchable text expanded", ""}});
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    const auto compressed = test::CompressDictdFixture(index);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ifstream compressed_input(compressed, std::ios::binary);
    const std::string compressed_data{
        std::istreambuf_iterator<char>(compressed_input),
        std::istreambuf_iterator<char>()};
    compressed_input.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::ofstream(compressed, std::ios::binary | std::ios::trunc)
        .write(compressed_data.data(),
               static_cast<std::streamsize>(compressed_data.size()));
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    std::ofstream(full_text_path, std::ios::binary | std::ios::trunc)
        << "corrupt";
    QCOMPARE(Dictionary::Open("dictd-id", index, full_text_path)
                 .full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));

    const Dictionary disabled = Dictionary::Open("dictd-id", index);
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void DictdDictionaryTest::SearchesGzipAndContainsFullTextFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = test::WriteDictdFixture(
        root, {{"entry", "compressed searchable text", ""}});
    test::CompressDictdFixture(index);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    const Dictionary compressed =
        Dictionary::Open("dictd-id", index, root / "fixture.gdfts");
    FullTextQuery query;
    query.text = "searchable";
    QCOMPARE(compressed.SearchFullText(query).results.size(), std::size_t{1});
    FullTextCancelledToken cancelled;
    QCOMPARE(compressed.SearchFullText(query, &cancelled).errors.front().code,
             FullTextErrorCode::kCancelled);
    SlowFullTextToken slow;
    query.timeout = std::chrono::milliseconds(1);
    QCOMPARE(compressed.SearchFullText(query, &slow).errors.front().code,
             FullTextErrorCode::kDeadlineExceeded);

    const auto storage_root = root / "storage";
    const auto storage_index =
        test::WriteDictdFixture(storage_root, {{"entry", "searchable", ""}});
    const auto storage_path = storage_root / "fixture.gdfts";
    QVERIFY(std::filesystem::create_directory(storage_path));
    const Dictionary storage =
        Dictionary::Open("storage-id", storage_index, storage_path);
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);

    const auto oversized_root = root / "oversized";
    const auto oversized_index = test::WriteDictdFixture(
        oversized_root,
        {{"oversized", std::string(16U * 1024U * 1024U + 1U, 'x'), ""}});
    const Dictionary oversized = Dictionary::Open(
        "oversized-id", oversized_index, oversized_root / "fixture.gdfts");
    query.timeout = std::chrono::seconds(5);
    QCOMPARE(oversized.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kResourceLimit);
}

void DictdDictionaryTest::ReusesRealDictzipCompanions_data() {
    QTest::addColumn<QString>("suffix");
    QTest::newRow("ra-dict") << ".dict";
    QTest::newRow("ra-dz") << ".dict.dz";
}

void DictdDictionaryTest::ReusesRealDictzipCompanions() {
    QFETCH(QString, suffix);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const std::string title = "00databaseshort\nRA title\n";
    const std::string info = "RA description";
    const std::string article =
        "searchable " + std::string(140U, 'x') + " ending";
    const auto index =
        test::WriteDictdFixture(root, {{"00databaseshort", title, {}},
                                       {"00databaseinfo", info, {}},
                                       {"entry", article, "alias"}});
    const auto selected = root / ("fixture" + suffix.toStdString());
    const auto bytes = test::AddDictzipFixtureHeaderFields(
        test::EncodeDictzipFixture(title + info + article), "original.dict",
        "generated multi-chunk content", true);
    QVERIFY(std::filesystem::remove(root / "fixture.dict"));
    std::ofstream(selected, std::ios::binary)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const auto original_sources =
        dictionary::CaptureSourceSnapshot({index, selected});
    const auto full_text_path = root / "fixture.gdfts";
    FullTextQuery query;
    query.text = "searchable";
    for (const bool warm : {false, true}) {
        const auto dictionary =
            Dictionary::Open("dictd-id", index, full_text_path);
        QCOMPARE(dictionary.identity().name, "RA title");
        QCOMPARE(dictionary.identity().description, info);
        QCOMPARE(dictionary.identity().article_count, 3U);
        QCOMPARE(dictionary.identity().headword_count, 4U);
        QCOMPARE(dictionary.identity().id, "dictd-id");
        QCOMPARE(dictionary.identity().source,
                 std::filesystem::weakly_canonical(index).string());
        QCOMPARE(article::Assemble(dictionary.identity(),
                                   dictionary.LookupExact("entry", {}))
                     .plain_text,
                 article);
        QCOMPARE(article::Assemble(dictionary.identity(),
                                   dictionary.LookupExact("alias", {}))
                     .plain_text,
                 article);
        QCOMPARE(
            dictionary.full_text_index_state(),
            std::optional(warm ? dictionary::FullTextIndexState::kReused
                               : dictionary::FullTextIndexState::kCreated));
        const auto response = dictionary.SearchFullText(query);
        QVERIFY(response.errors.empty());
        QVERIFY(!response.partial);
        QCOMPARE(response.results.size(), 1U);
        QCOMPARE(response.results.front().headword, "entry");
        QCOMPARE(response.results.front().dictionary.name, "RA title");
        QCOMPARE(response.results.front().document_id,
                 "dictd-index:2:" + std::to_string(title.size() + info.size()) +
                     ":" + std::to_string(article.size()));
        QCOMPARE(dictionary::CaptureSourceSnapshot({index, selected}),
                 original_sources);
    }
    auto damaged = bytes;
    damaged[damaged.size() - 8U] ^= 1;
    std::ofstream(selected, std::ios::binary | std::ios::trunc)
        .write(damaged.data(), static_cast<std::streamsize>(damaged.size()));
    try {
        static_cast<void>(Dictionary::Open("dictd-id", index, full_text_path));
        QFAIL("Damaged selected dictzip must report invalid data");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

void DictdDictionaryTest::RebuildsPreContentDetectionFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index = root / "fixture.index";
    const auto data = root / "fixture.dict";
    const auto full_text_path = root / "fixture.gdfts";
    std::ofstream(index, std::ios::binary) << "entry\tM\tC\n";
    const auto bytes = test::EncodeDictzipFixture(std::string(12U, 'x') + "OK");
    std::ofstream(data, std::ios::binary)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    // The former raw .dict path loaded the valid UTF-8 RA header at this range.
    // Seed precisely that old document against the unchanged original sources.
    QCOMPARE(bytes.substr(12U, 2U), "RA");
    dictionary::FullTextDocument old_document;
    old_document.dictionary.id = "dictd-id";
    old_document.dictionary.name = "fixture";
    old_document.headword = "entry";
    old_document.document_id = "dictd-index:0:12:2";
    old_document.plain_text = bytes.substr(12U, 2U);
    const auto sources = dictionary::CaptureSourceSnapshot({index, data});
    const auto old_index = dictionary::FullTextIndex::OpenOrBuild(
        full_text_path, sources, {old_document});
    FullTextQuery query;
    query.text = "RA";
    QCOMPARE(old_index.Search(query).results.size(), 1U);
    for (const bool warm : {false, true}) {
        const auto dictionary =
            Dictionary::Open("dictd-id", index, full_text_path);
        QCOMPARE(dictionary.full_text_index_state(),
                 std::optional(
                     warm ? dictionary::FullTextIndexState::kReused
                          : dictionary::FullTextIndexState::kRebuiltStale));
        query.text = "RA";
        QVERIFY(dictionary.SearchFullText(query).results.empty());
        query.text = "OK";
        QCOMPARE(dictionary.SearchFullText(query).results.size(), 1U);
        QCOMPARE(article::Assemble(dictionary.identity(),
                                   dictionary.LookupExact("entry", {}))
                     .plain_text,
                 "OK");
        QCOMPARE(dictionary::CaptureSourceSnapshot({index, data}), sources);
    }
}

void DictdDictionaryTest::HonorsCancellationAndHasNoResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto index =
        test::WriteDictdFixture(root, {{"example", "definition", {}}});
    const Dictionary dictionary = Dictionary::Open("dictd-id", index);
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    dictionary::RequestOptions active;
    QVERIFY(!dictionary.GetResource("missing", active).has_value());
}

}  // namespace
}  // namespace goldendict::core::formats::dictd

using goldendict::core::formats::dictd::DictdDictionaryTest;
QTEST_APPLESS_MAIN(DictdDictionaryTest)
#include "dictd_dictionary_test.moc"
