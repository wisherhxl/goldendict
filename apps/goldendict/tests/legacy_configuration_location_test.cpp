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
#include "goldendict/core/favorites_store.h"
#include "goldendict/core/history_store.h"

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
    return {platform,
            "/home/user",
            "/config",
            "/opt/goldendict/bin",
            "/roaming",
            "/data",
            "/current/GoldenDict/GoldenDict"};
}

}  // namespace

class LegacyConfigurationLocationTest : public QObject {
    Q_OBJECT

   private slots:
    void platformTable_data();
    void platformTable();
    void portableIsExclusive();
    void oldDirectoryGateDoesNotFallThrough();
    void linuxProfileHistoryPrecedesDataHistory();
    void allCurrentFilesPrecedeProfileDiscovery();
    void eachCurrentFileAvoidsItsLegacyProbe();
    void pathProbeFailureIsPreserved();
    void rejectsUnsafeLegacyCandidate_data();
    void rejectsUnsafeLegacyCandidate();
    void migratesOnceWithoutChangingSource();
    void companionsMigrateIndependentlyWithoutChangingSources();
    void malformedSelectedCandidateDoesNotFallThrough();
};

void LegacyConfigurationLocationTest::platformTable_data() {
    QTest::addColumn<int>("platform");
    QTest::addColumn<QString>("selected_directory");
    QTest::addColumn<QString>("expected_legacy");
    QTest::addColumn<QString>("expected_history");
    QTest::addColumn<QString>("expected_favorites");

    QTest::newRow("linux-xdg")
        << static_cast<int>(DesktopPlatform::kLinuxUnix) << QString()
        << QStringLiteral("/config/goldendict/config")
        << QStringLiteral("/data/goldendict/history")
        << QStringLiteral("/config/goldendict/favorites");
    QTest::newRow("linux-old")
        << static_cast<int>(DesktopPlatform::kLinuxUnix)
        << QStringLiteral("/home/user/.goldendict")
        << QStringLiteral("/home/user/.goldendict/config")
        << QStringLiteral("/data/goldendict/history")
        << QStringLiteral("/home/user/.goldendict/favorites");
    QTest::newRow("windows-roaming")
        << static_cast<int>(DesktopPlatform::kWindows) << QString()
        << QStringLiteral("/roaming/GoldenDict/config")
        << QStringLiteral("/roaming/GoldenDict/history")
        << QStringLiteral("/roaming/GoldenDict/favorites");
    QTest::newRow("windows-old")
        << static_cast<int>(DesktopPlatform::kWindows)
        << QStringLiteral("/home/user/Application Data/GoldenDict")
        << QStringLiteral("/home/user/Application Data/GoldenDict/config")
        << QStringLiteral("/home/user/Application Data/GoldenDict/history")
        << QStringLiteral("/home/user/Application Data/GoldenDict/favorites");
    QTest::newRow("mac-home")
        << static_cast<int>(DesktopPlatform::kMacOS) << QString()
        << QStringLiteral("/home/user/.goldendict/config")
        << QStringLiteral("/home/user/.goldendict/history")
        << QStringLiteral("/home/user/.goldendict/favorites");
}

void LegacyConfigurationLocationTest::platformTable() {
    QFETCH(int, platform);
    QFETCH(QString, selected_directory);
    QFETCH(QString, expected_legacy);
    QFETCH(QString, expected_history);
    QFETCH(QString, expected_favorites);
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
    QCOMPARE(
        QString::fromStdString(locations.current_history_path.generic_string()),
        QStringLiteral("/current/GoldenDict/GoldenDict/history-v1"));
    QCOMPARE(
        QString::fromStdString(locations.legacy_history_path.generic_string()),
        expected_history);
    QCOMPARE(QString::fromStdString(
                 locations.current_favorites_path.generic_string()),
             QStringLiteral("/current/GoldenDict/GoldenDict/favorites-v1"));
    QCOMPARE(QString::fromStdString(
                 locations.legacy_favorites_path.generic_string()),
             expected_favorites);
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
    QCOMPARE(locations.current_history_path.generic_string(),
             std::string("/opt/goldendict/bin/portable/history-v1"));
    QCOMPARE(locations.legacy_history_path.generic_string(),
             std::string("/opt/goldendict/bin/portable/history"));
    QCOMPARE(locations.current_favorites_path.generic_string(),
             std::string("/opt/goldendict/bin/portable/favorites-v1"));
    QCOMPARE(locations.legacy_favorites_path.generic_string(),
             std::string("/opt/goldendict/bin/portable/favorites"));
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

void LegacyConfigurationLocationTest::linuxProfileHistoryPrecedesDataHistory() {
    FakePaths paths;
    paths.kinds["/home/user/.goldendict"] = PathKind::kDirectory;
    paths.kinds["/home/user/.goldendict/history"] = PathKind::kRegularFile;
    paths.kinds["/data/goldendict/history"] = PathKind::kRegularFile;
    const auto locations = goldendict::app::ResolveConfigurationLocations(
        Environment(DesktopPlatform::kLinuxUnix),
        [&paths](const auto& path) { return paths.Probe(path); });
    QCOMPARE(locations.legacy_history_path.generic_string(),
             std::string("/home/user/.goldendict/history"));
    QCOMPARE(paths.probes["/data/goldendict/history"], 0);
}

void LegacyConfigurationLocationTest::allCurrentFilesPrecedeProfileDiscovery() {
    FakePaths paths;
    paths.kinds["/current/GoldenDict/GoldenDict/core.conf"] =
        PathKind::kRegularFile;
    paths.kinds["/current/GoldenDict/GoldenDict/history-v1"] =
        PathKind::kRegularFile;
    paths.kinds["/current/GoldenDict/GoldenDict/favorites-v1"] =
        PathKind::kRegularFile;
    paths.kinds["/home/user/.goldendict"] = PathKind::kSymlink;
    const auto locations = goldendict::app::ResolveConfigurationLocations(
        Environment(DesktopPlatform::kLinuxUnix),
        [&paths](const auto& path) { return paths.Probe(path); });
    QVERIFY(locations.legacy_configuration_path.empty());
    QVERIFY(locations.legacy_history_path.empty());
    QVERIFY(locations.legacy_favorites_path.empty());
    QCOMPARE(paths.probes["/home/user/.goldendict"], 0);
}

void LegacyConfigurationLocationTest::eachCurrentFileAvoidsItsLegacyProbe() {
    FakePaths paths;
    const ConfigurationLocations locations{"/current/core.conf",
                                           "/legacy/config",
                                           "/current/history-v1",
                                           "/legacy/history",
                                           "/current/favorites-v1",
                                           "/legacy/favorites",
                                           false};
    paths.kinds["/current/core.conf"] = PathKind::kRegularFile;
    paths.kinds["/current/history-v1"] = PathKind::kRegularFile;
    paths.kinds["/current/favorites-v1"] = PathKind::kRegularFile;
    paths.kinds["/legacy/config"] = PathKind::kSymlink;
    paths.kinds["/legacy/history"] = PathKind::kDirectory;
    paths.kinds["/legacy/favorites"] = PathKind::kOther;
    goldendict::app::ValidateAutoDiscoveredLegacyConfiguration(
        locations, [&paths](const auto& path) { return paths.Probe(path); });
    goldendict::app::ValidateAutoDiscoveredLegacyHistory(
        locations, [&paths](const auto& path) { return paths.Probe(path); });
    goldendict::app::ValidateAutoDiscoveredLegacyFavorites(
        locations, [&paths](const auto& path) { return paths.Probe(path); });
    QCOMPARE(paths.probes["/legacy/config"], 0);
    QCOMPARE(paths.probes["/legacy/history"], 0);
    QCOMPARE(paths.probes["/legacy/favorites"], 0);
}

void LegacyConfigurationLocationTest::pathProbeFailureIsPreserved() {
    const ConfigurationLocations locations{
        "/current/core.conf", "/legacy/config", {}, {}, {}, {}, false};
    QVERIFY_EXCEPTION_THROWN(
        goldendict::app::ValidateAutoDiscoveredLegacyConfiguration(
            locations,
            [](const auto&) -> PathKind {
                throw std::runtime_error("permission denied");
            }),
        std::runtime_error);
}

void LegacyConfigurationLocationTest::rejectsUnsafeLegacyCandidate_data() {
    QTest::addColumn<QString>("current");
    QTest::addColumn<QString>("legacy");
    QTest::addColumn<int>("companion");
    QTest::addColumn<int>("kind");
    const auto add_rows = [](const char* companion, int index) {
        QTest::newRow((std::string(companion) + "-directory").c_str())
            << QString("/current/%1").arg(companion)
            << QString("/legacy/%1").arg(companion) << index
            << static_cast<int>(PathKind::kDirectory);
        QTest::newRow((std::string(companion) + "-symlink").c_str())
            << QString("/current/%1").arg(companion)
            << QString("/legacy/%1").arg(companion) << index
            << static_cast<int>(PathKind::kSymlink);
        QTest::newRow((std::string(companion) + "-special").c_str())
            << QString("/current/%1").arg(companion)
            << QString("/legacy/%1").arg(companion) << index
            << static_cast<int>(PathKind::kOther);
    };
    add_rows("config", 0);
    add_rows("history", 1);
    add_rows("favorites", 2);
}

void LegacyConfigurationLocationTest::rejectsUnsafeLegacyCandidate() {
    QFETCH(QString, current);
    QFETCH(QString, legacy);
    QFETCH(int, companion);
    QFETCH(int, kind);
    FakePaths paths;
    ConfigurationLocations locations;
    if (companion == 0) {
        locations.current_configuration_path = current.toStdString();
        locations.legacy_configuration_path = legacy.toStdString();
    } else if (companion == 1) {
        locations.current_history_path = current.toStdString();
        locations.legacy_history_path = legacy.toStdString();
    } else {
        locations.current_favorites_path = current.toStdString();
        locations.legacy_favorites_path = legacy.toStdString();
    }
    paths.kinds[legacy.toStdString()] = static_cast<PathKind>(kind);
    const auto validate = [&]() {
        const auto probe = [&paths](const auto& path) {
            return paths.Probe(path);
        };
        if (companion == 0) {
            goldendict::app::ValidateAutoDiscoveredLegacyConfiguration(
                locations, probe);
        } else if (companion == 1) {
            goldendict::app::ValidateAutoDiscoveredLegacyHistory(locations,
                                                                 probe);
        } else {
            goldendict::app::ValidateAutoDiscoveredLegacyFavorites(locations,
                                                                   probe);
        }
    };
    QVERIFY_EXCEPTION_THROWN(validate(), std::runtime_error);
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
        root / "data",
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
    companionsMigrateIndependentlyWithoutChangingSources() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    const auto legacy_directory = root / "home" / ".goldendict";
    const QByteArray history("7 caf\xC3\xA9\n9 \xE8\xBE\x9E\xE6\x9B\xB8\n");
    const QByteArray favorites(
        "<root><folder name=\"Languages\" expanded=\"1\">"
        "<headword>caf\xC3\xA9</headword></folder></root>");
    WriteFile(legacy_directory / "config", "<config/>");
    WriteFile(legacy_directory / "history", history);
    WriteFile(legacy_directory / "favorites", favorites);
    const LegacyConfigurationEnvironment environment{
        DesktopPlatform::kLinuxUnix,
        root / "home",
        root / "xdg",
        root / "bin",
        {},
        root / "data",
        root / "current"};
    const auto locations = goldendict::app::ResolveConfigurationLocations(
        environment, goldendict::app::ProbePath);
    goldendict::app::ValidateAutoDiscoveredLegacyHistory(
        locations, goldendict::app::ProbePath);
    goldendict::app::ValidateAutoDiscoveredLegacyFavorites(
        locations, goldendict::app::ProbePath);

    const auto migrated_history = goldendict::core::LoadOrMigrateHistory(
        locations.current_history_path.string(),
        locations.legacy_history_path.string());
    QCOMPARE(migrated_history,
             (std::vector<goldendict::core::HistoryEntry>{
                 {7U, "caf\xC3\xA9"}, {9U, "\xE8\xBE\x9E\xE6\x9B\xB8"}}));
    WriteFile(legacy_directory / "favorites", "<root><folder></root>");
    QVERIFY_EXCEPTION_THROWN(goldendict::core::LoadOrMigrateFavorites(
                                 locations.current_favorites_path.string(),
                                 locations.legacy_favorites_path.string()),
                             std::runtime_error);
    QVERIFY(!std::filesystem::exists(locations.current_favorites_path));
    QVERIFY(!std::filesystem::exists(locations.current_favorites_path.string() +
                                     ".tmp"));
    QCOMPARE(ReadFile(legacy_directory / "history"), history.toStdString());

    WriteFile(legacy_directory / "favorites", favorites);
    const auto migrated_favorites = goldendict::core::LoadOrMigrateFavorites(
        locations.current_favorites_path.string(),
        locations.legacy_favorites_path.string());
    QCOMPARE(migrated_favorites.front().text, std::string("Languages"));
    QCOMPARE(ReadFile(legacy_directory / "favorites"), favorites.toStdString());
    QCOMPARE(goldendict::core::LoadOrMigrateHistory(
                 locations.current_history_path.string(),
                 locations.legacy_history_path.string()),
             migrated_history);
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
        root / "data",
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
