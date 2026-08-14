// SPDX-License-Identifier: GPL-3.0-or-later

#include "legacy_configuration_location.h"

#include <stdexcept>
#include <string>

namespace goldendict::app {
namespace {

void RequireBasePath(const std::filesystem::path& path,
                     const char* description) {
    if (path.empty()) {
        throw std::runtime_error(std::string(description) + " is unavailable");
    }
}

bool IsSelectedDirectory(const std::filesystem::path& path,
                         const PathProbe& probe) {
    if (path.empty()) {
        return false;
    }
    switch (probe(path)) {
        case PathKind::kMissing:
            return false;
        case PathKind::kDirectory:
            return true;
        case PathKind::kSymlink:
            throw std::runtime_error(
                "Legacy configuration directory must not be a symlink: " +
                path.string());
        case PathKind::kRegularFile:
        case PathKind::kOther:
            return false;
    }
    return false;
}

std::filesystem::path NonPortableLegacyProfileDirectory(
    const LegacyConfigurationEnvironment& environment, const PathProbe& probe) {
    switch (environment.platform) {
        case DesktopPlatform::kLinuxUnix: {
            const auto old_directory =
                environment.home_directory / ".goldendict";
            if (IsSelectedDirectory(old_directory, probe)) {
                return old_directory;
            }
            RequireBasePath(environment.standard_config_directory,
                            "Standard configuration directory");
            return environment.standard_config_directory / "goldendict";
        }
        case DesktopPlatform::kWindows: {
            const auto old_directory =
                environment.home_directory / "Application Data" / "GoldenDict";
            if (IsSelectedDirectory(old_directory, probe)) {
                return old_directory;
            }
            RequireBasePath(environment.roaming_app_data_directory,
                            "Roaming application-data directory");
            return environment.roaming_app_data_directory / "GoldenDict";
        }
        case DesktopPlatform::kMacOS:
            return environment.home_directory / ".goldendict";
    }
    throw std::runtime_error("Unsupported desktop platform");
}

bool IsMissing(const std::filesystem::path& path, const PathProbe& probe) {
    return probe(path) == PathKind::kMissing;
}

void ValidateSelectedFile(const std::filesystem::path& current_path,
                          const std::filesystem::path& legacy_path,
                          const char* description, const PathProbe& probe) {
    if (legacy_path.empty() || !IsMissing(current_path, probe)) {
        return;
    }
    switch (probe(legacy_path)) {
        case PathKind::kMissing:
        case PathKind::kRegularFile:
            return;
        case PathKind::kSymlink:
            throw std::runtime_error(
                std::string("Legacy ") + description +
                " file must not be a symlink: " + legacy_path.string());
        case PathKind::kDirectory:
        case PathKind::kOther:
            throw std::runtime_error(
                std::string("Legacy ") + description +
                " path is not a regular file: " + legacy_path.string());
    }
}

}  // namespace

ConfigurationLocations ResolveConfigurationLocations(
    const LegacyConfigurationEnvironment& environment, const PathProbe& probe) {
    if (!probe) {
        throw std::invalid_argument(
            "Legacy configuration path probe is missing");
    }
    RequireBasePath(environment.application_directory, "Application directory");
    const auto portable_directory =
        environment.application_directory / "portable";
    const bool portable = IsSelectedDirectory(portable_directory, probe);
    if (!portable) {
        RequireBasePath(environment.current_config_directory,
                        "Current configuration directory");
        RequireBasePath(environment.home_directory, "Home directory");
    }
    const auto current_directory =
        portable ? portable_directory : environment.current_config_directory;
    ConfigurationLocations locations;
    locations.portable = portable;
    locations.current_configuration_path = current_directory / "core.conf";
    locations.current_history_path = current_directory / "history-v1";
    locations.current_favorites_path = current_directory / "favorites-v1";

    const bool needs_configuration =
        IsMissing(locations.current_configuration_path, probe);
    const bool needs_history = IsMissing(locations.current_history_path, probe);
    const bool needs_favorites =
        IsMissing(locations.current_favorites_path, probe);
    if (!needs_configuration && !needs_history && !needs_favorites) {
        return locations;
    }

    const auto legacy_directory =
        portable ? portable_directory
                 : NonPortableLegacyProfileDirectory(environment, probe);
    if (needs_configuration) {
        locations.legacy_configuration_path = legacy_directory / "config";
    }
    if (needs_favorites) {
        locations.legacy_favorites_path = legacy_directory / "favorites";
    }
    if (needs_history) {
        const auto profile_history = legacy_directory / "history";
        if (portable || environment.platform != DesktopPlatform::kLinuxUnix ||
            !IsMissing(profile_history, probe)) {
            locations.legacy_history_path = profile_history;
        } else {
            RequireBasePath(environment.generic_data_directory,
                            "Generic data directory");
            locations.legacy_history_path =
                environment.generic_data_directory / "goldendict" / "history";
        }
    }
    return locations;
}

static void RequireProbe(const PathProbe& probe) {
    if (!probe) {
        throw std::invalid_argument(
            "Legacy configuration path probe is missing");
    }
}

void ValidateAutoDiscoveredLegacyConfiguration(
    const ConfigurationLocations& locations, const PathProbe& probe) {
    RequireProbe(probe);
    ValidateSelectedFile(locations.current_configuration_path,
                         locations.legacy_configuration_path, "configuration",
                         probe);
}

void ValidateAutoDiscoveredLegacyHistory(
    const ConfigurationLocations& locations, const PathProbe& probe) {
    RequireProbe(probe);
    ValidateSelectedFile(locations.current_history_path,
                         locations.legacy_history_path, "history", probe);
}

void ValidateAutoDiscoveredLegacyFavorites(
    const ConfigurationLocations& locations, const PathProbe& probe) {
    RequireProbe(probe);
    ValidateSelectedFile(locations.current_favorites_path,
                         locations.legacy_favorites_path, "favorites", probe);
}

PathKind ProbePath(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory ||
        error == std::errc::not_a_directory) {
        return PathKind::kMissing;
    }
    if (error) {
        throw std::runtime_error("Cannot inspect configuration path: " +
                                 error.message());
    }
    if (!std::filesystem::exists(status)) {
        return PathKind::kMissing;
    }
    if (std::filesystem::is_symlink(status)) {
        return PathKind::kSymlink;
    }
    if (std::filesystem::is_regular_file(status)) {
        return PathKind::kRegularFile;
    }
    if (std::filesystem::is_directory(status)) {
        return PathKind::kDirectory;
    }
    return PathKind::kOther;
}

}  // namespace goldendict::app
