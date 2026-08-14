// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/lsa/lsa_dictionary.h"
#include <QtTest>
#include "support/lsa_fixture.h"

namespace goldendict::core::formats::lsa {
class LsaDictionaryTest final : public QObject {
    Q_OBJECT
   private slots:
    void ExposesAudioArticlesSuggestionsAndWavResources();
};

void LsaDictionaryTest::ExposesAudioArticlesSuggestionsAndWavResources() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = test::WriteLsaFixture(
        std::filesystem::path(temporary.path().toStdString()));
    const auto dictionary = Dictionary::Open("lsa-fixture", path);
    QCOMPARE(dictionary.identity().name, std::string("fixture"));
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    dictionary::RequestOptions enumeration_options;
    enumeration_options.result_limit = 3U;
    const auto first = dictionary.EnumerateHeadwords(0U, enumeration_options);
    const auto second = dictionary.EnumerateHeadwords(3U, enumeration_options);
    QCOMPARE(first.headwords,
             (std::vector<std::string>{"Apple", "apple", "duplicate"}));
    QVERIFY(!first.complete);
    QCOMPARE(second.headwords, (std::vector<std::string>{"example", "second"}));
    QVERIFY(second.complete);
    const auto articles = dictionary.LookupPrefix("ex");
    QCOMPARE(articles.size(), 1U);
    QVERIFY(articles.front().data.find("audio/wav") != std::string::npos);
    QCOMPARE(dictionary.SuggestPrefix("s").front(), std::string("second"));
    const auto resource = dictionary.GetResource("second.wav");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, std::string("audio/wav"));
    QCOMPARE(resource->data.size(), 44U + 16U * 2U);
}
}  // namespace goldendict::core::formats::lsa

using goldendict::core::formats::lsa::LsaDictionaryTest;
QTEST_MAIN(LsaDictionaryTest)
#include "lsa_dictionary_test.moc"
