// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>

#include "../src/formats/mdict/mdict_dictionary.h"
#include "support/mdict_fixture.h"

namespace goldendict::core::formats::mdict {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class MdictDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlSuggestionsAndResources();
    void HonorsCancellationAndUnknownResources();
};

void MdictDictionaryTest::ExposesIdentityHtmlSuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto files = test::WriteMdictFixture(
        std::filesystem::path(directory.path().toStdString()));
    const Dictionary dictionary = Dictionary::Open("mdict-id", files);

    QCOMPARE(dictionary.identity().name, "Fixture MDict");
    QCOMPARE(dictionary.identity().description, "Fixture description");
    QCOMPARE(dictionary.LookupExact("example").front().format, "text/html");
    QCOMPARE(dictionary.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "example");
    const auto resource = dictionary.GetResource("pixel.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{9});
}

void MdictDictionaryTest::HonorsCancellationAndUnknownResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "mdict-id", test::WriteMdictFixture(
                        std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("missing.png").has_value());
    QVERIFY(!dictionary.GetResource("../pixel.png").has_value());
}

}  // namespace
}  // namespace goldendict::core::formats::mdict

using goldendict::core::formats::mdict::MdictDictionaryTest;
QTEST_APPLESS_MAIN(MdictDictionaryTest)
#include "mdict_dictionary_test.moc"
