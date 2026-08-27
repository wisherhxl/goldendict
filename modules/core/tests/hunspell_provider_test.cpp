// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <QTemporaryDir>
#include <QtTest>

#include "../src/morphology/hunspell_provider.h"
#include "support/hunspell_fixture.h"

namespace goldendict::core::morphology::hunspell {

// These minimal dictionaries are generated entirely by this test from
// project-authored GPL-3.0-or-later data. They contain no third-party wordlist
// content and are redistributable with the test source.

class HunspellProviderTest : public QObject {
    Q_OBJECT

   private slots:
    void PreservesExactUtf8AndAffixLookup();
    void PreservesLegacyEncodedLookup();
    void RejectsInvalidContentAndBoundsQueries();
};

void HunspellProviderTest::PreservesExactUtf8AndAffixLookup() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "fixture",
        "SET UTF-8\nSFX S Y 1\nSFX S 0 s .\n", "2\ncat/S\ncaf\xC3\xA9\n");
    auto provider = OpenProvider(files);

    QCOMPARE(provider->identity().id, std::string("fixture"));
    QCOMPARE(provider->identity().headword_count, 2U);
    QCOMPARE(provider->LookupExact("cat").size(), 1U);
    QCOMPARE(provider->LookupExact("cats").size(), 1U);
    QCOMPARE(provider->LookupExact("caf\xC3\xA9").front().headword,
             std::string("caf\xC3\xA9"));
    QVERIFY(provider->LookupExact("dog").empty());
    QVERIFY(provider->LookupPrefix("ca").empty());
    QVERIFY(provider->SuggestPrefix("ca").empty());
}

void HunspellProviderTest::PreservesLegacyEncodedLookup() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto files = test::WriteHunspellFixture(
        std::filesystem::path(temporary.path().toStdString()), "latin1",
        "SET ISO8859-1\n", std::string("1\nma") + "\xF1" + "ana\n");
    auto provider = OpenProvider(files);

    const std::string manana = std::string("ma") + "\xC3\xB1" + "ana";
    QCOMPARE(provider->LookupExact(manana).size(), 1U);
    QVERIFY(provider->LookupExact("manana").empty());
}

void HunspellProviderTest::RejectsInvalidContentAndBoundsQueries() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    const auto invalid =
        test::WriteHunspellFixture(root / "invalid", "x", "TRY abc\n", "0\n");
    try {
        OpenProvider(invalid);
        QFAIL("invalid content was accepted");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }

    const auto valid = test::WriteHunspellFixture(root / "valid", "x",
                                                  "SET UTF-8\n", "1\nword\n");
    auto provider = OpenProvider(valid);
    QVERIFY(provider->LookupExact("two words").empty());
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact(std::string(4097U, 'a')),
                             dictionary::Error);
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact(std::string("bad\xFF", 4U)),
                             dictionary::Error);
    dictionary::RequestOptions options;
    options.result_limit = 0U;
    QVERIFY(provider->LookupExact("word", options).empty());
    options.result_limit = 1U;
    options.deadline =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    QVERIFY_EXCEPTION_THROWN(provider->LookupExact("word", options),
                             dictionary::Error);
}

}  // namespace goldendict::core::morphology::hunspell

using goldendict::core::morphology::hunspell::HunspellProviderTest;
QTEST_APPLESS_MAIN(HunspellProviderTest)
#include "hunspell_provider_test.moc"
