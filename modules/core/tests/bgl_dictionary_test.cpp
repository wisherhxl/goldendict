// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>

#include "../src/formats/bgl/bgl_dictionary.h"
#include "support/bgl_fixture.h"

namespace goldendict::core::formats::bgl {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class BglDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlSuggestionsAndEmbeddedResources();
    void RejectsCancellationAndUnknownResources();
};

void BglDictionaryTest::ExposesIdentityHtmlSuggestionsAndEmbeddedResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "bgl-id", test::WriteBglFixture(
                      std::filesystem::path(directory.path().toStdString())));

    QCOMPARE(dictionary.identity().name, "Fixture BGL");
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.identity().target_language, "de");
    QVERIFY(dictionary.identity().description.find("Fixture description") !=
            std::string::npos);
    QCOMPARE(dictionary.LookupExact("EXAMPLE").front().format, "text/html");
    QCOMPARE(dictionary.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "example");
    const auto resource = dictionary.GetResource("pixel.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void BglDictionaryTest::RejectsCancellationAndUnknownResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "bgl-id", test::WriteBglFixture(
                      std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("missing.png").has_value());
    QVERIFY(!dictionary.GetResource(std::string("bad\0id", 6U)).has_value());
}

}  // namespace
}  // namespace goldendict::core::formats::bgl

using goldendict::core::formats::bgl::BglDictionaryTest;
QTEST_APPLESS_MAIN(BglDictionaryTest)
#include "bgl_dictionary_test.moc"
