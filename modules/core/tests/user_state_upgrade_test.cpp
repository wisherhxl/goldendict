// SPDX-License-Identifier: GPL-3.0-or-later

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#include "../src/application/user_state_upgrade_test_support.h"
#include "goldendict/core/application.h"
#include "goldendict/core/user_state_upgrade.h"

namespace {

using goldendict::core::UserStatePaths;
using goldendict::core::application::UserStateUpgradeDependencies;
using goldendict::core::application::UserStateUpgradeOperation;

void WriteFile(const std::filesystem::path& path, const QByteArray& contents) {
    std::filesystem::create_directories(path.parent_path());
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(contents) != contents.size()) {
        throw std::runtime_error("Cannot write user-state test file");
    }
}

std::string ReadFile(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Cannot read user-state test file");
    }
    return file.readAll().toStdString();
}

UserStatePaths Paths(const std::filesystem::path& root) {
    const auto current = root / "current";
    const auto legacy = root / "legacy";
    return {
        (current / "core.conf").string(),     (legacy / "config").string(),
        (current / "history-v1").string(),    (legacy / "history").string(),
        (current / "favorites-v1").string(),  (legacy / "favorites").string(),
        (root / "cache" / "indexes").string()};
}

QByteArray LegacyConfiguration() {
    return QByteArray(
        "<config><paths><path recursive=\"1\">C:/Dictionaries</path>"
        "</paths><groups nextId=\"2\"><group id=\"1\" name=\"Primary\" "
        "icon=\"\" favoritesFolder=\"Saved\" shortcut=\"Ctrl+1\" "
        "iconData=\"\"><dictionary>dictionary-id</dictionary>"
        "<mutedDictionaries><mutedDictionary>muted-id</mutedDictionary>"
        "</mutedDictionaries></group></groups><websites>"
        "<website id=\"legacy-website\" name=\"Legacy Website\" "
        "url=\"https://example.test/?q=%GD1251%\" enabled=\"0\" "
        "icon=\"\"/></websites>"
        "<forvo><enable>0</enable><languageCodes></languageCodes></forvo>"
        "<futureLegacySection><value>retained in the source</value>"
        "</futureLegacySection></config>");
}

QByteArray LegacyHistory() {
    return QByteArray("1 alpha\n1 caf\xC3\xA9\n");
}

QByteArray LegacyFavorites() {
    return QByteArray(
        "<root><folder name=\"Saved\" expanded=\"1\">"
        "<headword>alpha</headword></folder></root>");
}

void WriteLegacyProfile(const UserStatePaths& paths) {
    WriteFile(paths.legacy_configuration_path, LegacyConfiguration());
    WriteFile(paths.legacy_history_path, LegacyHistory());
    WriteFile(paths.legacy_favorites_path, LegacyFavorites());
}

std::filesystem::path PendingPath(const UserStatePaths& paths) {
    return paths.configuration_path + ".upgrade-v1.pending";
}

std::filesystem::path StagePath(const std::string& path) {
    return path + ".upgrade-v1.stage";
}

}  // namespace

class UserStateUpgradeTest : public QObject {
    Q_OBJECT

   private slots:
    void migratesCompleteProfileAndLeavesLegacyUntouched();
    void malformedCompanionPublishesNothing();
    void publicationFailureIsRecoveredOnRestart();
    void preservesUnknownCurrentConfigurationFields();
    void rejectsMalformedUnknownCurrentConfigurationFields();
    void upgradesDisposableQt5ProfileFromEnvironment();
};

void UserStateUpgradeTest::migratesCompleteProfileAndLeavesLegacyUntouched() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto paths = Paths(temporary.path().toStdString());
    WriteLegacyProfile(paths);

    const auto state = goldendict::core::LoadOrMigrateUserState(paths);
    QCOMPARE(state.configuration.dictionary_paths,
             std::vector<std::string>{"C:/Dictionaries"});
    QCOMPARE(state.configuration.dictionary_groups.size(), std::size_t{1});
    QCOMPARE(state.configuration.website_sources.size(), std::size_t{1});
    QVERIFY(state.configuration.website_sources.front().inside_iframe);
    QCOMPARE(state.configuration.website_sources.front().url_template,
             std::string("https://example.test/?q=%GD1251%"));
    QCOMPARE(state.configuration.forvo_sources.size(), std::size_t{1});
    QVERIFY(!state.configuration.forvo_sources.front().enabled);
    QVERIFY(state.configuration.forvo_sources.front().language_codes.empty());
    QCOMPARE(state.history.size(), std::size_t{2});
    QCOMPARE(state.favorites.front().text, std::string("Saved"));

    QCOMPARE(ReadFile(paths.legacy_configuration_path),
             LegacyConfiguration().toStdString());
    QCOMPARE(ReadFile(paths.legacy_history_path),
             LegacyHistory().toStdString());
    QCOMPARE(ReadFile(paths.legacy_favorites_path),
             LegacyFavorites().toStdString());
    QVERIFY(!std::filesystem::exists(PendingPath(paths)));
    QVERIFY(!std::filesystem::exists(StagePath(paths.configuration_path)));
    QVERIFY(!std::filesystem::exists(StagePath(paths.history_path)));
    QVERIFY(!std::filesystem::exists(StagePath(paths.favorites_path)));

    WriteFile(paths.legacy_configuration_path, "<not-config>");
    const auto restarted = goldendict::core::LoadOrMigrateUserState(paths);
    QCOMPARE(restarted.configuration.dictionary_paths,
             state.configuration.dictionary_paths);
    QCOMPARE(restarted.history, state.history);
    QCOMPARE(restarted.favorites, state.favorites);
}

void UserStateUpgradeTest::malformedCompanionPublishesNothing() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    enum class Companion { kConfiguration, kHistory, kFavorites };
    constexpr std::array companions{Companion::kConfiguration,
                                    Companion::kHistory, Companion::kFavorites};
    for (std::size_t index = 0U; index < companions.size(); ++index) {
        const auto root =
            std::filesystem::path(temporary.path().toStdString()) /
            std::to_string(index);
        const auto paths = Paths(root);
        WriteLegacyProfile(paths);
        std::string malformed_path;
        switch (companions[index]) {
            case Companion::kConfiguration:
                malformed_path = paths.legacy_configuration_path;
                break;
            case Companion::kHistory:
                malformed_path = paths.legacy_history_path;
                break;
            case Companion::kFavorites:
                malformed_path = paths.legacy_favorites_path;
                break;
        }
        WriteFile(malformed_path, "malformed user state");

        QVERIFY_EXCEPTION_THROWN(
            goldendict::core::LoadOrMigrateUserState(paths),
            std::runtime_error);
        QVERIFY(!std::filesystem::exists(paths.configuration_path));
        QVERIFY(!std::filesystem::exists(paths.history_path));
        QVERIFY(!std::filesystem::exists(paths.favorites_path));
        QVERIFY(!std::filesystem::exists(PendingPath(paths)));
        QVERIFY(!std::filesystem::exists(StagePath(paths.configuration_path)));
        QVERIFY(!std::filesystem::exists(StagePath(paths.history_path)));
        QVERIFY(!std::filesystem::exists(StagePath(paths.favorites_path)));
        QCOMPARE(ReadFile(malformed_path), std::string("malformed user state"));
    }
}

void UserStateUpgradeTest::publicationFailureIsRecoveredOnRestart() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    struct FailureCase {
        UserStateUpgradeOperation operation;
        std::size_t expected_publications;
        bool marker_exists;
    };

    constexpr std::array cases{
        FailureCase{UserStateUpgradeOperation::kWritePendingMarker, 0U, false},
        FailureCase{UserStateUpgradeOperation::kPublishConfiguration, 0U, true},
        FailureCase{UserStateUpgradeOperation::kPublishHistory, 1U, true},
        FailureCase{UserStateUpgradeOperation::kPublishFavorites, 2U, true},
        FailureCase{UserStateUpgradeOperation::kRemovePendingMarker, 3U, true},
    };
    for (std::size_t index = 0U; index < cases.size(); ++index) {
        const auto root =
            std::filesystem::path(temporary.path().toStdString()) /
            std::to_string(index);
        const auto paths = Paths(root);
        WriteLegacyProfile(paths);
        UserStateUpgradeDependencies failure;
        failure.filesystem_failure = [failed_operation =
                                          cases[index].operation](
                                         UserStateUpgradeOperation operation,
                                         const std::filesystem::path&)
            -> std::optional<std::error_code> {
            if (operation == failed_operation) {
                return std::make_error_code(std::errc::permission_denied);
            }
            return std::nullopt;
        };

        QVERIFY_EXCEPTION_THROWN(
            goldendict::core::application::LoadOrMigrateUserStateForTesting(
                paths, failure),
            std::runtime_error);
        const std::array destinations{paths.configuration_path,
                                      paths.history_path, paths.favorites_path};
        std::size_t publication_count = 0U;
        for (const auto& destination : destinations) {
            publication_count += std::filesystem::exists(destination) ? 1U : 0U;
        }
        QCOMPARE(publication_count, cases[index].expected_publications);
        QCOMPARE(std::filesystem::exists(PendingPath(paths)),
                 cases[index].marker_exists);

        const auto recovered = goldendict::core::LoadOrMigrateUserState(paths);
        QCOMPARE(recovered.history.size(), std::size_t{2});
        QCOMPARE(recovered.favorites.front().text, std::string("Saved"));
        QVERIFY(!std::filesystem::exists(PendingPath(paths)));
        QVERIFY(!std::filesystem::exists(StagePath(paths.configuration_path)));
        QVERIFY(!std::filesystem::exists(StagePath(paths.history_path)));
        QVERIFY(!std::filesystem::exists(StagePath(paths.favorites_path)));
        QCOMPARE(ReadFile(paths.legacy_configuration_path),
                 LegacyConfiguration().toStdString());
    }
}

void UserStateUpgradeTest::preservesUnknownCurrentConfigurationFields() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = std::filesystem::path(temporary.path().toStdString());
    const auto path = root / "core.conf";
    WriteFile(path,
              "goldendict-core-config-v1\n"
              "index_directory=C%3A%2Findexes\n"
              "future_record=alpha%20beta\n"
              "preference=future_preference|42\n");

    auto configuration = goldendict::core::LoadConfiguration(path.string());
    QCOMPARE(configuration.opaque_fields,
             (std::vector<std::string>{"future_record=alpha%20beta",
                                       "preference=future_preference|42"}));
    configuration.preferences.maximum_history_entries = 321U;
    goldendict::core::SaveConfiguration(path.string(), configuration);
    const auto saved = ReadFile(path);
    QVERIFY(saved.find("future_record=alpha%20beta\n") != std::string::npos);
    QVERIFY(saved.find("preference=future_preference|42\n") !=
            std::string::npos);
    QCOMPARE(goldendict::core::LoadConfiguration(path.string()).opaque_fields,
             configuration.opaque_fields);
}

void UserStateUpgradeTest::rejectsMalformedUnknownCurrentConfigurationFields() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path =
        std::filesystem::path(temporary.path().toStdString()) / "core.conf";
    WriteFile(path, "goldendict-core-config-v1\n=missing-key\n");
    QVERIFY_EXCEPTION_THROWN(goldendict::core::LoadConfiguration(path.string()),
                             std::runtime_error);
    goldendict::core::CoreConfiguration configuration;
    configuration.opaque_fields = {"index_directory=shadow"};
    QVERIFY_EXCEPTION_THROWN(
        goldendict::core::SaveConfiguration(path.string(), configuration),
        std::runtime_error);
}

void UserStateUpgradeTest::upgradesDisposableQt5ProfileFromEnvironment() {
    const QByteArray source_value = qgetenv("GOLDENDICT_TEST_QT5_PROFILE");
    if (source_value.isEmpty()) {
        QSKIP("GOLDENDICT_TEST_QT5_PROFILE is not configured");
    }
    const std::filesystem::path source(source_value.toStdString());
    const auto source_config = source / "config";
    if (!std::filesystem::is_regular_file(source_config)) {
        QFAIL("The configured Qt 5 profile has no regular config file");
    }
    const std::string original_config = ReadFile(source_config);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto paths = Paths(temporary.path().toStdString());
    WriteFile(paths.legacy_configuration_path,
              QByteArray::fromStdString(original_config));
    const auto copy_or_default = [](const std::filesystem::path& source_path,
                                    const std::string& destination,
                                    const QByteArray& fallback) {
        WriteFile(destination,
                  std::filesystem::is_regular_file(source_path)
                      ? QByteArray::fromStdString(ReadFile(source_path))
                      : fallback);
    };
    copy_or_default(source / "history", paths.legacy_history_path,
                    LegacyHistory());
    copy_or_default(source / "favorites", paths.legacy_favorites_path,
                    LegacyFavorites());

    std::optional<goldendict::core::UserStateSnapshot> migrated;
    try {
        migrated = goldendict::core::LoadOrMigrateUserState(paths);
    } catch (const std::exception& error) {
        QFAIL(qPrintable(QStringLiteral("Qt 5 profile upgrade failed: %1")
                             .arg(QString::fromUtf8(error.what()))));
    }
    QVERIFY(migrated.has_value());
    QVERIFY(!migrated->configuration.mediawiki_sources.empty());
    QVERIFY(!migrated->configuration.website_sources.empty());
    QVERIFY(std::filesystem::exists(paths.configuration_path));
    QCOMPARE(ReadFile(paths.legacy_configuration_path), original_config);
}

QTEST_APPLESS_MAIN(UserStateUpgradeTest)

#include "user_state_upgrade_test.moc"
