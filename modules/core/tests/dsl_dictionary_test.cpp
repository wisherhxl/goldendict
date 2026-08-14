// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/formats/dsl/dsl_dictionary.h"
#include "support/dsl_fixture.h"

namespace goldendict::core::formats::dsl {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class DslDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlSuggestionsAndResources();
    void RejectsCancellationAndUnsafeResources();
};

void DslDictionaryTest::ExposesIdentityHtmlSuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteDslFixture(root);
    test::WriteDslResource(path, "images/cup.png", "png-data");
    std::ofstream(root / "fixture.ann")
        << "#LANGUAGE \"en\"\nEnglish annotation\n"
           "#LANGUAGE \"de\"\nGerman annotation";
    const Dictionary dictionary = Dictionary::Open("dsl-id", path, "de");
    QCOMPARE(dictionary.identity().name, "Fixture DSL");
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.identity().target_language, "de");
    QCOMPARE(dictionary.identity().description, "German annotation");
    QCOMPARE(dictionary.LookupExact("CAFE").front().format, "text/html");
    QVERIFY(!dictionary.SuggestPrefix("caf").empty());
    const auto resource = dictionary.GetResource("images/cup.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
}

void DslDictionaryTest::RejectsCancellationAndUnsafeResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "dsl-id", test::WriteDslFixture(
                      std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;
    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("cafe", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("../outside.txt").has_value());
}

}  // namespace
}  // namespace goldendict::core::formats::dsl

using goldendict::core::formats::dsl::DslDictionaryTest;
QTEST_APPLESS_MAIN(DslDictionaryTest)
#include "dsl_dictionary_test.moc"
