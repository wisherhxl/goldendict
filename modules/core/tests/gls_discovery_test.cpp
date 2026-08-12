// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "../src/formats/gls/gls_discovery.h"
#include "support/gls_fixture.h"

namespace goldendict::core::formats::gls {

class GlsDiscoveryTest : public QObject {
    Q_OBJECT

   private slots:
    void DiscoversPlainAndCompressedFilesWithoutDuplicates();
};

void GlsDiscoveryTest::DiscoversPlainAndCompressedFilesWithoutDuplicates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto plain =
        test::WriteGlsFixture(root / "plain", {{{"plain"}, "definition"}});
    const auto compressed = test::CompressGlsFixture(test::WriteGlsFixture(
        root / "compressed", {{{"compressed"}, "definition"}}));
    std::ofstream(root / "ignored.txt") << "not a glossary";

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

}  // namespace goldendict::core::formats::gls

using goldendict::core::formats::gls::GlsDiscoveryTest;
QTEST_APPLESS_MAIN(GlsDiscoveryTest)
#include "gls_discovery_test.moc"
