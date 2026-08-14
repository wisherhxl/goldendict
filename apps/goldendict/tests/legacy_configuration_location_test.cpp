// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>

#include "../src/legacy_configuration_location.h"
#include "goldendict/core/application.h"

namespace {

using goldendict::app::ConfigurationLocations;
using goldendict::app::DesktopPlatform;
using goldendict::app::LegacyConfigurationEnvironment;
using goldendict::app::PathKind;

std::string ReadFile(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Cannot read test file");
    }
    return file.readAll().toStdString();
}

void WriteFile(const std::filesystem::path& path, const QByteArray& contents) {
    std::filesystem::create_directories(path.parent_path());
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(contents) != contents.size()) {
        throw std::runtime_error("Cannot write test file");
    }
}

class FakePaths {
   public:
    PathKind Probe(const std::filesystem::path& path) {
        ++probes[path.generic_string()];
        const auto found = kinds.find(path.generic_string());
        return found == kinds.end() ? PathKind::kMissing : found->second;
    }

    std::map<std::string, PathKind> kinds;
    std::map<std::string, int> probes;
};

LegacyConfigurationEnvironment Environment(DesktopPlatform platform) {
    return {platform,   "/home/user",
            "/config",  "/opt/goldendict/bin",
            "/roaming", "/current/GoldenDict/GoldenDict"};
}

}  // namespace

class LegacyConfigurationLocationTest : public QObject {
    Q_OBJECT

   private slots:
    void platformTable_data();
    void platformTable();
    void portableIsExclusive();
    void oldDirectoryGateDoesNotFallThrough();
    void currentConfigurationPrecedesProfileDiscovery();
    void currentConfigurationAvoidsLegacyProbe();
    void pathProbeFailureIsPreserved();
    void rejectsUnsafeLegacyCandidate_data();
    void rejectsUnsafeLegacyCandidate();
    void migratesOnceWithoutChangingSource();
    void malformedSelectedCandidateDoesNotFallThrough();
};

void LegacyConfigurationLocationTest::platformTable_data() {
    QTest::addColumn<int>("platform");
    QTest::addColumn<QString>("selected_directory");
    QTest::addColumn<QString>("expected_legacy");

    QTest::newRow("linux-xdg")
        << static_cast<int>(DesktopPlatform::kLinuxUnix) << QString()
        << QStringLiteral("/config/goldendict/config");
    QTest::newRow("linux-old")
        << static_cast<int>(DesktopPlatform::kLinuxUnix)
        << QStringLiteral("/home/user/.goldendict")
        << QStringLiteral("/home/user/.goldendict/config");
    QTest::newRow("windows-roaming")
        << static_cast<int>(DesktopPlatform::kWindows) << QString()
        << QStringLiteral("/roaming/GoldenDict/config");
    QTest::newRow("windows-old")
        << static_cast<int>(DesktopPlatform::kWindows)
        << QStringLiteral("/home/user/Application Data/GoldenDict")
        << QStringLiteral("/home/user/Application Data/GoldenDict/config");
    QTest::newRow("mac-home")
        << static_cast<int>(DesktopPlatform::kMacOS) << QString()
        << QStringLiteral("/home/user/.goldendict/config");
}

void LegacyConfigurationLocationTest::platformTable() {
    QFETCH(int, platform);
    QFETCH(QString, selected_directory);
    QFETCH(QString, expected_legacy);
    FakePaths paths;
    if (!selected_directory.isEmpty()) {
        paths.kinds[selected_directory.toStdString()] = PathKind::kDirectory;
    }
    const auto locations = goldendict::app::ResolveConfigurationLocations(
        Environment(static_cast<DesktopPlatform>(platform)),
        [&paths](const auto& path) { return paths.Probe(path); });
    QCOMPARE(QString::fromStdString(
                 locations.current_configuration_path.generic_string()),
             QStringLiteral("/current/GoldenDict/GoldenDict/core.conf"));
    QCOMPARE(QString::fromStdString(
                 locations.legacy_configuration_path.generic_string()),
             expected_legacy);
    QVERIFY(!locations.portable);
}

void LegacyConfigurationLocationTest::portableIsExclusive() {
    FakePaths paths;
    paths.kinds["/opt/goldendict/bin/portable"] = PathKind::kDirectory;
    paths.kinds["/home/user/.goldendict"] = PathKind::kDirectory;
    const auto locations = goldendict::app::ResolveConfigurationLocations(
        Environment(DesktopPlatform::kLinuxUnix),
        [&paths](const auto& path) { return paths.Probe(path); });
    QCOMPARE(locations.current_configuration_path.generic_string(),
             std::string("/opt/goldendict/bin/portable/core.conf"));
    QCOMPARE(locations.legacy_configuration_path.generic_string(),
             std::string("/opt/goldendict/bin/portable/config"));
    QVERIFY(locations.portable);
    QCOMPARE(paths.probes["/home/user/.goldendict"], 0);
}

void LegacyConfigurationLocationTest::oldDirectoryGateDoesNotFallThrough() {
    FakePaths paths;
    paths.kinds["/home/user/.goldendict"] = PathKind::kDirectory;
    const auto locations = goldendict::app::ResolveConfigurationLocations(
        Environment(DesktopPlatform::kLinuxUnix),
        [&paths](const auto& path) { return paths.Probe(path); });
    QCOMPARE(locations.legacy_configuration_path.generic_string(),
             std::string("/home/user/.goldendict/config"));
    QCOMPARE(paths.probes["/config/goldendict"], 0);
}

void LegacyConfigurationLocationTest::
    currentConfigurationPrecedesProfileDiscovery() {
    FakePaths paths;
    paths.kinds["/current/GoldenDict/GoldenDict/core.conf"] =
        PathKind::kRegularFile;
    paths.kinds["/home/user/.goldendict"] = PathKind::kSymlink;
    const auto locations = goldendict::app::ResolveConfigurationLocations(
        Environment(DesktopPlatform::kLinuxUnix),
        [&paths](const auto& path) { return paths.Probe(path); });
    QVERIFY(locations.legacy_configuration_path.empty());
    QCOMPARE(paths.probes["/home/user/.goldendict"], 0);
}

void LegacyConfigurationLocationTest::currentConfigurationAvoidsLegacyProbe() {
    FakePaths paths;
    const ConfigurationLocations locations{"/current/core.conf",
                                           "/legacy/config", false};
    paths.kinds["/current/core.conf"] = PathKind::kRegularFile;
    paths.kinds["/legacy/config"] = PathKind::kSymlink;
    goldendict::app::ValidateAutoDiscoveredLegacyConfiguration(
        locations, [&paths](const auto& path) { return paths.Probe(path); });
    QCOMPARE(paths.probes["/legacy/config"], 0);
}

void LegacyConfigurationLocationTest::pathProbeFailureIsPreserved() {
    const ConfigurationLocations locations{"/current/core.conf",
                                           "/legacy/config", false};
    QVERIFY_EXCEPTION_THROWN(
        goldendict::app::ValidateAutoDiscoveredLegacyConfiguration(
            locations,
            [](const auto&) -> PathKind {
                throw std::runtime_error("permission denied");
            }),
        std::runtime_error);
}

void LegacyConfigurationLocationTest::rejectsUnsafeLegacyCandidate_data() {
    QTest::addColumn<int>("kind");
    QTest::newRow("directory") << static_cast<int>(PathKind::kDirectory);
    QTest::newRow("symlink") << static_cast<int>(PathKind::kSymlink);
    QTest::newRow("special") << static_cast<int>(PathKind::kOther);
}

void LegacyConfigurationLocationTest::rejectsUnsafeLegacyCandidate() {
    QFETCH(int, kind);
    FakePaths paths;
    const ConfigurationLocations locations{"/current/core.conf",
                                           "/legacy/config", false};
    paths.kinds["/legacy/config"] = static_cast<PathKind>(kind);
    QVERIFY_EXCEPTION_THROWN(
        goldendict::app::ValidateAutoDiscoveredLegacyConfiguration(
            locations,
            [&paths](const auto& path) { return paths.Probe(path); }),
        std::runtime_error);
}

void LegacyConfigurationLocationTest::migratesOnceWithoutChangingSource() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    const auto legacy_directory = root / "home" / ".goldendict";
    const QByteArray legacy(
        "<config><paths><path recursive=\"1\">/dicts</path>"
        "</paths></config>");
    WriteFile(legacy_directory / "config", legacy);

    const LegacyConfigurationEnvironment environment{
        DesktopPlatform::kLinuxUnix,
        root / "home",
        root / "xdg",
        root / "bin",
        {},
        root / "current"};
    const auto locations = goldendict::app::ResolveConfigurationLocations(
        environment, goldendict::app::ProbePath);
    goldendict::app::ValidateAutoDiscoveredLegacyConfiguration(
        locations, goldendict::app::ProbePath);
    const auto migrated = goldendict::core::LoadOrMigrateConfiguration(
        locations.current_configuration_path.string(),
        locations.legacy_configuration_path.string(), "/indexes");
    QCOMPARE(migrated.dictionary_paths, std::vector<std::string>{"/dicts"});
    QCOMPARE(ReadFile(legacy_directory / "config"), legacy.toStdString());

    WriteFile(legacy_directory / "config", "<not-config>");
    const auto loaded = goldendict::core::LoadOrMigrateConfiguration(
        locations.current_configuration_path.string(),
        locations.legacy_configuration_path.string(), "/other-indexes");
    QCOMPARE(loaded.dictionary_paths, migrated.dictionary_paths);
    QCOMPARE(loaded.index_directory, migrated.index_directory);
    QCOMPARE(ReadFile(legacy_directory / "config"),
             std::string("<not-config>"));
}

void LegacyConfigurationLocationTest::
    malformedSelectedCandidateDoesNotFallThrough() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    WriteFile(root / "home" / ".goldendict" / "config", "<not-config>");
    WriteFile(root / "xdg" / "goldendict" / "config", "<config/>");
    const LegacyConfigurationEnvironment environment{
        DesktopPlatform::kLinuxUnix,
        root / "home",
        root / "xdg",
        root / "bin",
        {},
        root / "current"};
    const auto locations = goldendict::app::ResolveConfigurationLocations(
        environment, goldendict::app::ProbePath);
    QVERIFY_EXCEPTION_THROWN(
        goldendict::core::LoadOrMigrateConfiguration(
            locations.current_configuration_path.string(),
            locations.legacy_configuration_path.string(), "/indexes"),
        std::runtime_error);
    QVERIFY(!std::filesystem::exists(locations.current_configuration_path));
    QVERIFY(!std::filesystem::exists(
        locations.current_configuration_path.string() + ".tmp"));
    QCOMPARE(ReadFile(root / "xdg" / "goldendict" / "config"),
             std::string("<config/>"));
}

QTEST_APPLESS_MAIN(LegacyConfigurationLocationTest)

#include "legacy_configuration_location_test.moc"
