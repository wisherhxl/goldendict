// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/gls/gls_dictionary.h"
#include "support/gls_fixture.h"

namespace goldendict::core::formats::gls {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class GlsDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesHtmlIdentitySuggestionsAndResources();
    void RejectsCancellationAndUnsafeResources();
};

void GlsDictionaryTest::ExposesHtmlIdentitySuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteGlsFixture(
        root,
        {{{"example"}, "<b>definition</b> <img src=\"images/pixel.png\">"}});
    test::WriteGlsResource(path, "images/pixel.png", "png-data");
    const Dictionary dictionary = Dictionary::Open("gls-id", path);

    const auto articles = dictionary.LookupExact("EXAMPLE");
    const auto suggestions = dictionary.SuggestPrefix("exa");
    const auto resource = dictionary.GetResource("images/pixel.png");

    QCOMPARE(dictionary.identity().id, "gls-id");
    QCOMPARE(dictionary.identity().name, "Fixture GLS");
    QCOMPARE(dictionary.identity().source_language, "eng");
    QCOMPARE(dictionary.identity().target_language, "deu");
    QCOMPARE(dictionary.identity().description,
             "Author: GoldenDict tests\n\nFixture description");
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(suggestions.front(), "example");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void GlsDictionaryTest::RejectsCancellationAndUnsafeResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Dictionary dictionary = Dictionary::Open(
        "gls-id", test::WriteGlsFixture(root, {{{"example"}, "definition"}}));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("../outside.txt").has_value());

    const auto oversized = root / "oversized.bin";
    std::ofstream(oversized, std::ios::binary).put('\0');
    std::filesystem::resize_file(oversized, 16U * 1024U * 1024U + 1U);
    QVERIFY_EXCEPTION_THROWN(dictionary.GetResource("oversized.bin"),
                             dictionary::Error);
}

}  // namespace
}  // namespace goldendict::core::formats::gls

using goldendict::core::formats::gls::GlsDictionaryTest;
QTEST_APPLESS_MAIN(GlsDictionaryTest)
#include "gls_dictionary_test.moc"
