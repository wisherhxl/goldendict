// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/xdxf/xdxf_dictionary.h"
#include "support/xdxf_fixture.h"

namespace goldendict::core::formats::xdxf {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class XdxfDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesHtmlIdentitySuggestionsAndResources();
    void RejectsCancellationAndUnsafeResources();
};

void XdxfDictionaryTest::ExposesHtmlIdentitySuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteXdxfFixture(
        root,
        {{{"example"}, "<def>definition <rref>images/pixel.png</rref></def>"}});
    test::WriteXdxfResource(path, "images/pixel.png", "png-data");
    const Dictionary dictionary = Dictionary::Open("xdxf-id", path);

    const auto articles = dictionary.LookupExact("EXAMPLE");
    const auto suggestions = dictionary.SuggestPrefix("exa");
    const auto resource = dictionary.GetResource("images/pixel.png");

    QCOMPARE(dictionary.identity().id, "xdxf-id");
    QCOMPARE(dictionary.identity().name, "Fixture XDXF");
    QCOMPARE(dictionary.identity().source_language, "eng");
    QCOMPARE(dictionary.identity().target_language, "deu");
    QCOMPARE(dictionary.identity().description, "Fixture description");
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(suggestions.front(), "example");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void XdxfDictionaryTest::RejectsCancellationAndUnsafeResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Dictionary dictionary = Dictionary::Open(
        "xdxf-id",
        test::WriteXdxfFixture(root, {{{"example"}, "<def>definition</def>"}}));
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
}  // namespace goldendict::core::formats::xdxf

using goldendict::core::formats::xdxf::XdxfDictionaryTest;
QTEST_APPLESS_MAIN(XdxfDictionaryTest)
#include "xdxf_dictionary_test.moc"
