// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "../src/formats/dsl/dsl_discovery.h"
#include "support/dsl_fixture.h"

namespace goldendict::core::formats::dsl {

class DslDiscoveryTest : public QObject {
    Q_OBJECT
   private slots:
    void DiscoversDictionariesAndSkipsAbbreviations();
};

void DslDiscoveryTest::DiscoversDictionariesAndSkipsAbbreviations() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto plain = test::WriteDslFixture(root / "plain");
    const auto compressed =
        test::CompressDslFixture(test::WriteDslFixture(root / "compressed"));
    std::ofstream(root / "fixture_abrv.dsl") << "abbreviation";
    const auto result = Discover({root, plain});
    QVERIFY(result.issues.empty());
    QCOMPARE(result.dictionary_files.size(), std::size_t{2});
    QVERIFY(std::find(result.dictionary_files.begin(),
                      result.dictionary_files.end(),
                      plain) != result.dictionary_files.end());
    QVERIFY(std::find(result.dictionary_files.begin(),
                      result.dictionary_files.end(),
                      compressed) != result.dictionary_files.end());
}

}  // namespace goldendict::core::formats::dsl

using goldendict::core::formats::dsl::DslDiscoveryTest;
QTEST_APPLESS_MAIN(DslDiscoveryTest)
#include "dsl_discovery_test.moc"
