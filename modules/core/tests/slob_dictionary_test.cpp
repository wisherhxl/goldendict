// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/slob/slob_dictionary.h"
#include <QtTest>
#include <filesystem>
#include "support/slob_fixture.h"

namespace goldendict::core::formats::slob {
namespace {
class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class SlobDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityArticlesSuggestionsAndResources();
    void RejectsCancellation();
};

void SlobDictionaryTest::ExposesIdentityArticlesSuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "slob-id", test::WriteSlobFixture(
                       std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(dictionary.identity().name, "Fixture SLOB");
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.identity().target_language, "de");
    QCOMPARE(dictionary.LookupExact("alias").front().format, "text/html");
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "example");
    const auto resource = dictionary.GetResource("pixel.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void SlobDictionaryTest::RejectsCancellation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "slob-id", test::WriteSlobFixture(
                       std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;
    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
}
}  // namespace
}  // namespace goldendict::core::formats::slob

using goldendict::core::formats::slob::SlobDictionaryTest;
QTEST_APPLESS_MAIN(SlobDictionaryTest)
#include "slob_dictionary_test.moc"
