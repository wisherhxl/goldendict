// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <chrono>
#include <filesystem>

#include "../src/formats/dictd/dictd_dictionary.h"
#include "support/dictd_fixture.h"

namespace goldendict::core::formats::dictd {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class DictdDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesPlainArticlesAndSuggestions();
    void HonorsCancellationAndHasNoResources();
};

void DictdDictionaryTest::ExposesPlainArticlesAndSuggestions() {
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
    QCOMPARE(articles.front().format, "text/plain");
    QCOMPARE(articles.front().data, "definition");
    QCOMPARE(suggestions.size(), std::size_t{1});
    QCOMPARE(suggestions.front(), "example");
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
