// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/epwing/epwing_dictionary.h"
#include <QtTest>
#include <filesystem>
#include "support/epwing_fixture.h"

namespace goldendict::core::formats::epwing {
class EpwingDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesBackendContract();
};

void EpwingDictionaryTest::ExposesBackendContract() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto dictionary =
        Dictionary::Open("epwing-fixture", test::WriteEpwingFixture(root));
    QCOMPARE(dictionary.identity().name, "Fixture EPWING");
    QCOMPARE(dictionary.LookupExact("example").size(), std::size_t{1});
    QCOMPARE(dictionary.SuggestPrefix("sec").front(), "second");
    QVERIFY(dictionary.GetResource("FIXTURE/GAIJI/pixel.png").has_value());
}
}  // namespace goldendict::core::formats::epwing

using goldendict::core::formats::epwing::EpwingDictionaryTest;
QTEST_APPLESS_MAIN(EpwingDictionaryTest)
#include "epwing_dictionary_test.moc"
