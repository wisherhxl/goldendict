// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_APPLICATION_H_
#define GOLDENDICT_CORE_APPLICATION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"
#include "goldendict/core/desktop_facade.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core {

struct SoundDirectoryConfiguration {
    std::string path;
    std::string name;

    bool operator==(const SoundDirectoryConfiguration& other) const noexcept {
        return path == other.path && name == other.name;
    }

    bool operator!=(const SoundDirectoryConfiguration& other) const noexcept {
        return !(*this == other);
    }
};

struct DictionaryGroupConfiguration {
    std::uint32_t id = 0U;
    std::string name;
    std::string icon;
    std::vector<std::string> dictionary_ids;

    bool operator==(const DictionaryGroupConfiguration& other) const noexcept {
        return id == other.id && name == other.name && icon == other.icon &&
               dictionary_ids == other.dictionary_ids;
    }

    bool operator!=(const DictionaryGroupConfiguration& other) const noexcept {
        return !(*this == other);
    }
};

struct CoreConfiguration {
    std::vector<std::string> dictionary_paths;
    std::string index_directory;
    std::vector<SoundDirectoryConfiguration> sound_directories;
    std::vector<DictionaryGroupConfiguration> dictionary_groups;
};

// Missing files load as an empty clean-profile configuration. Malformed files
// and I/O failures throw std::runtime_error.
GOLDENDICT_EXPORTS CoreConfiguration
LoadConfiguration(const std::string& configuration_path);
GOLDENDICT_EXPORTS void SaveConfiguration(
    const std::string& configuration_path,
    const CoreConfiguration& configuration);

// Loads the current configuration when present. Otherwise, imports dictionary
// and sound-directory paths from a legacy GoldenDict XML configuration, saves
// the new configuration atomically, and leaves the legacy file untouched.
// Missing current and legacy files return an empty configuration with the
// supplied index directory. Malformed files and I/O failures throw.
GOLDENDICT_EXPORTS CoreConfiguration
LoadOrMigrateConfiguration(const std::string& configuration_path,
                           const std::string& legacy_configuration_path,
                           const std::string& index_directory);

GOLDENDICT_EXPORTS std::unique_ptr<DictionaryService> CreateDictionaryService(
    const CoreConfiguration& configuration);
GOLDENDICT_EXPORTS std::unique_ptr<DesktopFacade> CreateDesktopFacade(
    const CoreConfiguration& configuration);

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_APPLICATION_H_
