// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/zipsounds/zipsounds_dictionary.h"
#include <QtTest>
#include "support/zipsounds_fixture.h"

namespace goldendict::core::formats::zipsounds {
class ZipSoundsDictionaryTest final : public QObject {
    Q_OBJECT
   private slots:
    void ExposesAudioArticlesSuggestionsAndResources();
};

void ZipSoundsDictionaryTest::ExposesAudioArticlesSuggestionsAndResources() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = test::WriteZipSoundsFixture(
        std::filesystem::path(temporary.path().toStdString()));
    const auto dictionary = Dictionary::Open("zips-fixture", path);
    QCOMPARE(dictionary.identity().name, std::string("fixture"));
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    dictionary::RequestOptions enumeration_options;
    enumeration_options.result_limit = 3U;
    const auto first = dictionary.EnumerateHeadwords(0U, enumeration_options);
    const auto second = dictionary.EnumerateHeadwords(3U, enumeration_options);
    QCOMPARE(first.headwords,
             (std::vector<std::string>{" spaced", "Apple", "apple"}));
    QVERIFY(!first.complete);
    QCOMPARE(second.headwords, (std::vector<std::string>{"duplicate", "example",
                                                         "nested/second"}));
    QVERIFY(second.complete);
    const auto articles = dictionary.LookupPrefix("nested/s");
    QCOMPARE(articles.size(), 1U);
    QVERIFY(articles.front().data.find("audio/ogg") != std::string::npos);
    QCOMPARE(dictionary.SuggestPrefix("ex").front(), std::string("example"));
    const auto resource = dictionary.GetResource("nested/second.ogg");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, std::string("audio/ogg"));
    QCOMPARE(resource->data.size(), std::size_t(15U));
}
}  // namespace goldendict::core::formats::zipsounds

using goldendict::core::formats::zipsounds::ZipSoundsDictionaryTest;
QTEST_MAIN(ZipSoundsDictionaryTest)
#include "zipsounds_dictionary_test.moc"
