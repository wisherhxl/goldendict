// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>

#include "../src/morphology/hunspell_discovery.h"

namespace goldendict::core::morphology::hunspell {

class HunspellDiscoveryTest : public QObject {
    Q_OBJECT

   private slots:
    void PreservesPinnedCompanionAndIdentityRules();
    void RejectsInvalidRootsAndBoundsDirectoryScan();
};

void HunspellDiscoveryTest::PreservesPinnedCompanionAndIdentityRules() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    std::filesystem::create_directory(root / "nested");
    std::ofstream(root / "en_US.aff") << "SET UTF-8\n";
    std::ofstream(root / "en_US.dic") << "0\n";
    std::ofstream(root / "de_DE.AFF") << "SET UTF-8\n";
    std::ofstream(root / "de_DE.DIC") << "0\n";
    std::ofstream(root / "alias.AFF") << "SET UTF-8\n";
    std::ofstream(root / "alias.aff") << "SET UTF-8\n";
    std::ofstream(root / "alias.dic") << "0\n";
    std::ofstream(root / "missing.aff") << "SET UTF-8\n";
    std::ofstream(root / "ignored.AfF") << "SET UTF-8\n";
    std::ofstream(root / "nested" / "fr_FR.aff") << "SET UTF-8\n";
    std::ofstream(root / "nested" / "fr_FR.dic") << "0\n";

    const auto result = Discover(root);

    QCOMPARE(result.dictionaries.size(), std::size_t{3});
    QCOMPARE(result.dictionaries[0].dictionary_id, std::string("alias"));
    QCOMPARE(result.dictionaries[0].affix_file, root / "alias.AFF");
    QCOMPARE(result.dictionaries[0].dictionary_file, root / "alias.dic");
    QCOMPARE(result.dictionaries[1].dictionary_id, std::string("de_DE"));
    QCOMPARE(result.dictionaries[1].affix_file, root / "de_DE.AFF");
    QCOMPARE(result.dictionaries[1].dictionary_file, root / "de_DE.DIC");
    QCOMPARE(result.dictionaries[2].dictionary_id, std::string("en_US"));
    QCOMPARE(result.dictionaries[2].affix_file, root / "en_US.aff");
    QCOMPARE(result.dictionaries[2].dictionary_file, root / "en_US.dic");
    QCOMPARE(result.issues.size(), std::size_t{2});
    QCOMPARE(result.issues[0].path, root / "alias.aff");
    QCOMPARE(result.issues[1].path, root / "missing.aff");
}

void HunspellDiscoveryTest::RejectsInvalidRootsAndBoundsDirectoryScan() {
    QCOMPARE(Discover({}).dictionaries.size(), std::size_t{0});

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto plain_file = root / "plain";
    std::ofstream(plain_file) << "not a directory";
    const auto file_result = Discover(plain_file);
    QVERIFY(file_result.dictionaries.empty());
    QCOMPARE(file_result.issues.size(), std::size_t{1});

    const auto missing_result = Discover(root / "missing");
    QVERIFY(missing_result.dictionaries.empty());
    QCOMPARE(missing_result.issues.size(), std::size_t{1});

    const auto crowded = root / "crowded";
    std::filesystem::create_directory(crowded);
    for (std::size_t index = 0; index <= kMaximumDirectoryEntries; ++index) {
        std::ofstream(crowded / ("entry-" + std::to_string(index))) << index;
    }
    const auto crowded_result = Discover(crowded);
    QVERIFY(crowded_result.dictionaries.empty());
    QCOMPARE(crowded_result.issues.size(), std::size_t{1});
    QVERIFY(QString::fromStdString(crowded_result.issues.front().message)
                .contains(QStringLiteral("limit")));
}

}  // namespace goldendict::core::morphology::hunspell

using goldendict::core::morphology::hunspell::HunspellDiscoveryTest;
QTEST_APPLESS_MAIN(HunspellDiscoveryTest)
#include "hunspell_discovery_test.moc"
