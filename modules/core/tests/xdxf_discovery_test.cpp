// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>

#include "../src/formats/xdxf/xdxf_discovery.h"
#include "support/xdxf_fixture.h"

namespace goldendict::core::formats::xdxf {

class XdxfDiscoveryTest : public QObject {
    Q_OBJECT

   private slots:
    void DiscoversPlainAndCompressedFilesWithoutDuplicates();
};

void XdxfDiscoveryTest::DiscoversPlainAndCompressedFilesWithoutDuplicates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto plain = test::WriteXdxfFixture(
        root / "plain", {{{"plain"}, "<def>definition</def>"}});
    const auto compressed = test::CompressXdxfFixture(test::WriteXdxfFixture(
        root / "compressed", {{{"compressed"}, "<def>definition</def>"}}));
    std::ofstream(root / "ignored.xml") << "<xdxf/>";

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

}  // namespace goldendict::core::formats::xdxf

using goldendict::core::formats::xdxf::XdxfDiscoveryTest;
QTEST_APPLESS_MAIN(XdxfDiscoveryTest)
#include "xdxf_discovery_test.moc"
