// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <chrono>
#include <filesystem>

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
    void HonorsCancellationAndDeadline();
    void TranslatesReaderFailures();
    void ReportsUnsupportedResources();
};

std::filesystem::path TemporaryPath(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
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

    QCOMPARE(dictionary.identity().id, "fixture-id");
    QCOMPARE(dictionary.identity().name, "Generated Test Dictionary");
    QCOMPARE(dictionary.identity().source, info_path.string());
    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().headword, "example");
    QCOMPARE(articles.front().format, "stardict/m");
    QCOMPARE(articles.front().data, "first");
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

void StardictDictionaryTest::ReportsUnsupportedResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", "article"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    try {
        static_cast<void>(dictionary.GetResource("missing.png"));
        QFAIL("GetResource should report that resources are not implemented");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kUnsupported);
    }
}

}  // namespace
}  // namespace goldendict::core::formats::stardict

using goldendict::core::formats::stardict::StardictDictionaryTest;

QTEST_APPLESS_MAIN(StardictDictionaryTest)

#include "stardict_dictionary_test.moc"
