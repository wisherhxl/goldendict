// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/zim/zim_dictionary.h"
#include <QtTest>
#include <filesystem>
#include "support/zim_fixture.h"

namespace goldendict::core::formats::zim {
namespace {
class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class ZimDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityArticlesSuggestionsAndResources();
    void RejectsCancellation();
};

void ZimDictionaryTest::ExposesIdentityArticlesSuggestionsAndResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = test::WriteZimFixture(
        std::filesystem::path(directory.path().toStdString()));
    const Dictionary dictionary = Dictionary::Open("zim-id", {path, {path}});
    QCOMPARE(dictionary.identity().name, "Fixture ZIM");
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.LookupExact("alias").front().format, "text/html");
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "Example");
    const auto resource = dictionary.GetResource("I/pixel.png");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, "image/png");
    QCOMPARE(resource->data.size(), std::size_t{8});
}

void ZimDictionaryTest::RejectsCancellation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = test::WriteZimFixture(
        std::filesystem::path(directory.path().toStdString()));
    const Dictionary dictionary = Dictionary::Open("zim-id", {path, {path}});
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;
    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
}
}  // namespace
}  // namespace goldendict::core::formats::zim

using goldendict::core::formats::zim::ZimDictionaryTest;
QTEST_APPLESS_MAIN(ZimDictionaryTest)
#include "zim_dictionary_test.moc"
