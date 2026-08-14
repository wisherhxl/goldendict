// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>

#include "../src/formats/sdict/sdict_dictionary.h"
#include "support/sdict_fixture.h"

namespace goldendict::core::formats::sdict {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class SdictDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesHtmlArticlesIdentityAndSuggestions();
    void HonorsCancellationAndHasNoResources();
};

void SdictDictionaryTest::ExposesHtmlArticlesIdentityAndSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path =
        test::WriteSdictFixture(root, {{"example", "<b>definition</b>"}});
    const Dictionary dictionary = Dictionary::Open("sdict-id", path);

    const auto articles = dictionary.LookupExact("EXAMPLE");
    const auto suggestions = dictionary.SuggestPrefix("EXA");

    QCOMPARE(dictionary.identity().id, "sdict-id");
    QCOMPARE(dictionary.identity().name, "Fixture SDict");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"example"}));
    QCOMPARE(dictionary.identity().source_language, "eng");
    QCOMPARE(dictionary.identity().target_language, "deu");
    QVERIFY(dictionary.identity().description.find("Fixture copyright") !=
            std::string::npos);
    QCOMPARE(articles.front().format, "text/html");
    QCOMPARE(articles.front().data, "<b>definition</b>");
    QCOMPARE(suggestions.front(), "example");
}

void SdictDictionaryTest::HonorsCancellationAndHasNoResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const Dictionary dictionary = Dictionary::Open(
        "sdict-id", test::WriteSdictFixture(root, {{"example", "definition"}}));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;

    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("missing").has_value());
}

}  // namespace
}  // namespace goldendict::core::formats::sdict

using goldendict::core::formats::sdict::SdictDictionaryTest;
QTEST_APPLESS_MAIN(SdictDictionaryTest)
#include "sdict_dictionary_test.moc"
