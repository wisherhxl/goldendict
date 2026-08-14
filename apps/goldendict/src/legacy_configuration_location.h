// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <functional>

namespace goldendict::app {

enum class DesktopPlatform { kLinuxUnix, kWindows, kMacOS };

enum class PathKind { kMissing, kRegularFile, kDirectory, kSymlink, kOther };

struct LegacyConfigurationEnvironment {
    DesktopPlatform platform = DesktopPlatform::kLinuxUnix;
    std::filesystem::path home_directory;
    std::filesystem::path standard_config_directory;
    std::filesystem::path application_directory;
    std::filesystem::path roaming_app_data_directory;
    std::filesystem::path generic_data_directory;
    std::filesystem::path current_config_directory;
};

struct ConfigurationLocations {
    std::filesystem::path current_configuration_path;
    std::filesystem::path legacy_configuration_path;
    std::filesystem::path current_history_path;
    std::filesystem::path legacy_history_path;
    std::filesystem::path current_favorites_path;
    std::filesystem::path legacy_favorites_path;
    bool portable = false;
};

using PathProbe = std::function<PathKind(const std::filesystem::path&)>;

// Reproduces the pinned legacy profile-directory choice without scanning.
// The injected probe makes every platform branch deterministic in tests.
ConfigurationLocations ResolveConfigurationLocations(
    const LegacyConfigurationEnvironment& environment, const PathProbe& probe);

// Each current file has independent precedence. Otherwise, validates its one
// selected legacy candidate without falling through to another profile.
void ValidateAutoDiscoveredLegacyConfiguration(
    const ConfigurationLocations& locations, const PathProbe& probe);
void ValidateAutoDiscoveredLegacyHistory(
    const ConfigurationLocations& locations, const PathProbe& probe);
void ValidateAutoDiscoveredLegacyFavorites(
    const ConfigurationLocations& locations, const PathProbe& probe);

PathKind ProbePath(const std::filesystem::path& path);

}  // namespace goldendict::app
