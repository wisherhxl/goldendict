// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/user_state_upgrade.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include "user_state_upgrade_test_support.h"

namespace goldendict::core {
namespace {

using application::UserStateUpgradeDependencies;
using application::UserStateUpgradeOperation;

constexpr std::string_view kPendingHeader =
    "goldendict-user-state-upgrade-v1\n";
constexpr unsigned int kConfigurationBit = 1U;
constexpr unsigned int kHistoryBit = 2U;
constexpr unsigned int kFavoritesBit = 4U;

struct Publication {
    unsigned int bit;
    UserStateUpgradeOperation operation;
    std::filesystem::path destination;
    std::filesystem::path staging;
};

std::filesystem::file_status SafeStatus(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory ||
        error == std::errc::not_a_directory) {
        return {};
    }
    if (error) {
        throw std::runtime_error("Cannot inspect user-state path: " +
                                 error.message());
    }
    return status;
}

bool IsMissing(const std::filesystem::path& path) {
    return !std::filesystem::exists(SafeStatus(path));
}

void RequireRegularOrMissing(const std::filesystem::path& path,
                             const char* description) {
    const auto status = SafeStatus(path);
    if (!std::filesystem::exists(status)) {
        return;
    }
    if (!std::filesystem::is_regular_file(status)) {
        throw std::runtime_error(std::string(description) +
                                 " must be a regular file: " + path.string());
    }
}

void Inject(const UserStateUpgradeDependencies& dependencies,
            UserStateUpgradeOperation operation,
            const std::filesystem::path& path) {
    if (!dependencies.filesystem_failure) {
        return;
    }
    const auto error = dependencies.filesystem_failure(operation, path);
    if (error) {
        throw std::runtime_error("User-state upgrade persistence failed: " +
                                 error->message());
    }
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot read user-state upgrade file");
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 || static_cast<std::uintmax_t>(size) > 4U * 1024U * 1024U) {
        throw std::runtime_error("User-state upgrade file is too large");
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    input.seekg(0, std::ios::beg);
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input ||
        input.gcount() != static_cast<std::streamsize>(contents.size())) {
        throw std::runtime_error(
            "Cannot read complete user-state upgrade file");
    }
    return contents;
}

unsigned int ParsePendingMarker(const std::filesystem::path& path) {
    const std::string contents = ReadFile(path);
    if (contents.substr(0U, kPendingHeader.size()) != kPendingHeader) {
        throw std::runtime_error("User-state upgrade marker is malformed");
    }
    const std::string_view remainder(contents.data() + kPendingHeader.size(),
                                     contents.size() - kPendingHeader.size());
    constexpr std::string_view prefix = "mask=";
    if (remainder.empty() || remainder.substr(0U, prefix.size()) != prefix ||
        remainder.back() != '\n') {
        throw std::runtime_error("User-state upgrade marker is malformed");
    }
    const auto value =
        remainder.substr(prefix.size(), remainder.size() - prefix.size() - 1U);
    if (value.size() != 1U || value.front() < '1' || value.front() > '7') {
        throw std::runtime_error("User-state upgrade marker is malformed");
    }
    return static_cast<unsigned int>(value.front() - '0');
}

void RemoveRegularFile(const std::filesystem::path& path) {
    const auto status = SafeStatus(path);
    if (!std::filesystem::exists(status)) {
        return;
    }
    if (!std::filesystem::is_regular_file(status)) {
        throw std::runtime_error(
            "User-state upgrade artifact is not a regular file: " +
            path.string());
    }
    std::error_code error;
    if (!std::filesystem::remove(path, error) || error) {
        throw std::runtime_error("Cannot remove user-state upgrade artifact: " +
                                 error.message());
    }
}

void WritePendingMarker(const std::filesystem::path& marker, unsigned int mask,
                        const UserStateUpgradeDependencies& dependencies) {
    Inject(dependencies, UserStateUpgradeOperation::kWritePendingMarker,
           marker);
    const auto temporary = marker.string() + ".tmp";
    RemoveRegularFile(temporary);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        const std::string contents =
            std::string(kPendingHeader) + "mask=" + std::to_string(mask) + "\n";
        output.write(contents.data(),
                     static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output) {
            RemoveRegularFile(temporary);
            throw std::runtime_error("Cannot write user-state upgrade marker");
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, marker, error);
    if (error) {
        RemoveRegularFile(temporary);
        throw std::runtime_error("Cannot publish user-state upgrade marker: " +
                                 error.message());
    }
}

std::array<Publication, 3U> Publications(const UserStatePaths& paths) {
    const std::filesystem::path configuration(paths.configuration_path);
    const std::filesystem::path history(paths.history_path);
    const std::filesystem::path favorites(paths.favorites_path);
    return {
        {{kConfigurationBit, UserStateUpgradeOperation::kPublishConfiguration,
          configuration, configuration.string() + ".upgrade-v1.stage"},
         {kHistoryBit, UserStateUpgradeOperation::kPublishHistory, history,
          history.string() + ".upgrade-v1.stage"},
         {kFavoritesBit, UserStateUpgradeOperation::kPublishFavorites,
          favorites, favorites.string() + ".upgrade-v1.stage"}}};
}

void Publish(const Publication& publication,
             const UserStateUpgradeDependencies& dependencies) {
    RequireRegularOrMissing(publication.destination,
                            "Current user-state destination");
    RequireRegularOrMissing(publication.staging,
                            "User-state upgrade staging path");
    if (!IsMissing(publication.destination)) {
        if (!IsMissing(publication.staging)) {
            if (ReadFile(publication.destination) !=
                ReadFile(publication.staging)) {
                throw std::runtime_error(
                    "User-state upgrade has conflicting published and staged "
                    "data");
            }
            RemoveRegularFile(publication.staging);
        }
        return;
    }
    if (IsMissing(publication.staging)) {
        throw std::runtime_error("User-state upgrade staging file is missing");
    }
    Inject(dependencies, publication.operation, publication.destination);
    std::error_code error;
    std::filesystem::rename(publication.staging, publication.destination,
                            error);
    if (error) {
        throw std::runtime_error("Cannot publish user-state upgrade file: " +
                                 error.message());
    }
}

UserStateSnapshot LoadCurrentState(const UserStatePaths& paths) {
    UserStateSnapshot state;
    state.configuration = LoadOrMigrateConfiguration(paths.configuration_path,
                                                     {}, paths.index_directory);
    state.history = LoadOrMigrateHistory(
        paths.history_path, {},
        state.configuration.preferences.maximum_history_entries);
    state.favorites = LoadOrMigrateFavorites(paths.favorites_path, {});
    return state;
}

UserStateSnapshot LoadOrMigrateImpl(
    const UserStatePaths& paths,
    const UserStateUpgradeDependencies& dependencies) {
    const std::filesystem::path configuration(paths.configuration_path);
    if (configuration.empty() || paths.history_path.empty() ||
        paths.favorites_path.empty() || configuration.parent_path().empty() ||
        std::filesystem::path(paths.history_path).parent_path() !=
            configuration.parent_path() ||
        std::filesystem::path(paths.favorites_path).parent_path() !=
            configuration.parent_path()) {
        throw std::invalid_argument(
            "Current user-state paths must share one nonempty directory");
    }
    std::filesystem::create_directories(configuration.parent_path());
    const auto publications = Publications(paths);
    const std::filesystem::path marker =
        configuration.string() + ".upgrade-v1.pending";
    RequireRegularOrMissing(marker, "User-state upgrade marker");

    if (!IsMissing(marker)) {
        const unsigned int mask = ParsePendingMarker(marker);
        for (const auto& publication : publications) {
            if ((mask & publication.bit) != 0U) {
                Publish(publication, dependencies);
            }
        }
        UserStateSnapshot recovered = LoadCurrentState(paths);
        Inject(dependencies, UserStateUpgradeOperation::kRemovePendingMarker,
               marker);
        RemoveRegularFile(marker);
        return recovered;
    }

    RemoveRegularFile(marker.string() + ".tmp");
    for (const auto& publication : publications) {
        RemoveRegularFile(publication.staging);
        RemoveRegularFile(publication.staging.string() + ".tmp");
    }

    unsigned int mask = 0U;
    UserStateSnapshot prepared;
    try {
        if (IsMissing(configuration) &&
            !paths.legacy_configuration_path.empty() &&
            !IsMissing(paths.legacy_configuration_path)) {
            mask |= kConfigurationBit;
            prepared.configuration = LoadOrMigrateConfiguration(
                publications[0].staging.string(),
                paths.legacy_configuration_path, paths.index_directory);
        } else {
            prepared.configuration = LoadOrMigrateConfiguration(
                paths.configuration_path, {}, paths.index_directory);
        }

        const auto history_limit =
            prepared.configuration.preferences.maximum_history_entries;
        if (IsMissing(paths.history_path) &&
            !paths.legacy_history_path.empty() &&
            !IsMissing(paths.legacy_history_path)) {
            mask |= kHistoryBit;
            prepared.history =
                LoadOrMigrateHistory(publications[1].staging.string(),
                                     paths.legacy_history_path, history_limit);
        } else {
            prepared.history =
                LoadOrMigrateHistory(paths.history_path, {}, history_limit);
        }

        if (IsMissing(paths.favorites_path) &&
            !paths.legacy_favorites_path.empty() &&
            !IsMissing(paths.legacy_favorites_path)) {
            mask |= kFavoritesBit;
            prepared.favorites = LoadOrMigrateFavorites(
                publications[2].staging.string(), paths.legacy_favorites_path);
        } else {
            prepared.favorites =
                LoadOrMigrateFavorites(paths.favorites_path, {});
        }
    } catch (...) {
        for (const auto& publication : publications) {
            RemoveRegularFile(publication.staging);
            RemoveRegularFile(publication.staging.string() + ".tmp");
        }
        throw;
    }

    if (mask == 0U) {
        return prepared;
    }
    try {
        WritePendingMarker(marker, mask, dependencies);
    } catch (...) {
        for (const auto& publication : publications) {
            RemoveRegularFile(publication.staging);
        }
        throw;
    }
    for (const auto& publication : publications) {
        if ((mask & publication.bit) != 0U) {
            Publish(publication, dependencies);
        }
    }
    UserStateSnapshot published = LoadCurrentState(paths);
    Inject(dependencies, UserStateUpgradeOperation::kRemovePendingMarker,
           marker);
    RemoveRegularFile(marker);
    return published;
}

}  // namespace

UserStateSnapshot LoadOrMigrateUserState(const UserStatePaths& paths) {
    return LoadOrMigrateImpl(paths, {});
}

namespace application {

UserStateSnapshot LoadOrMigrateUserStateForTesting(
    const UserStatePaths& paths,
    const UserStateUpgradeDependencies& dependencies) {
    return LoadOrMigrateImpl(paths, dependencies);
}

}  // namespace application
}  // namespace goldendict::core
