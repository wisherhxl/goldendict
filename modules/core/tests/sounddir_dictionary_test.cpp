// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/sounddir/sounddir_dictionary.h"
#include <QtTest>
#include "support/sounddir_fixture.h"

namespace goldendict::core::formats::sounddir {
class SoundDirDictionaryTest final : public QObject {
    Q_OBJECT
   private slots:
    void ExposesConfiguredIdentityAndTypedAudio();
};

void SoundDirDictionaryTest::ExposesConfiguredIdentityAndTypedAudio() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = test::WriteSoundDirectoryFixture(
        std::filesystem::path(temporary.path().toStdString()));
    const auto dictionary =
        Dictionary::Open("sounds-fixture", root, "Fixture sounds");
    QCOMPARE(dictionary.identity().name, std::string("Fixture sounds"));
    const auto articles = dictionary.LookupPrefix("sec");
    QCOMPARE(articles.size(), 1U);
    QVERIFY(articles.front().data.find("audio/ogg") != std::string::npos);
    const auto resource = dictionary.GetResource("nested/second.ogg");
    QVERIFY(resource.has_value());
    QCOMPARE(resource->media_type, std::string("audio/ogg"));
    QCOMPARE(resource->data.size(), std::size_t{15U});
}
}  // namespace goldendict::core::formats::sounddir

using goldendict::core::formats::sounddir::SoundDirDictionaryTest;
QTEST_MAIN(SoundDirDictionaryTest)
#include "sounddir_dictionary_test.moc"
