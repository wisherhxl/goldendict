// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/aard/aard_dictionary.h"
#include <QtTest>
#include <filesystem>
#include "support/aard_fixture.h"

namespace goldendict::core::formats::aard {
namespace {
class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class AardDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlAndSuggestions();
    void RejectsCancellationAndHasNoResources();
};

void AardDictionaryTest::ExposesIdentityHtmlAndSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "aard-id", test::WriteAardFixture(
                       std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(dictionary.identity().name, "Fixture Aard");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"alias", "example", "redirect"}));
    QCOMPARE(dictionary.identity().description, "fixture");
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.identity().target_language, "de");
    QCOMPARE(dictionary.LookupExact("example").front().format, "text/html");
    QCOMPARE(dictionary.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "example");
}

void AardDictionaryTest::RejectsCancellationAndHasNoResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "aard-id", test::WriteAardFixture(
                       std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;
    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("missing.png").has_value());
}
}  // namespace
}  // namespace goldendict::core::formats::aard

using goldendict::core::formats::aard::AardDictionaryTest;
QTEST_APPLESS_MAIN(AardDictionaryTest)
#include "aard_dictionary_test.moc"
