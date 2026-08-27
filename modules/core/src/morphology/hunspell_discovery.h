// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_DISCOVERY_H_
#define GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_DISCOVERY_H_

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace goldendict::core::morphology::hunspell {

inline constexpr std::size_t kMaximumDirectoryEntries = 4096U;

struct DataFiles {
    std::filesystem::path affix_file;
    std::filesystem::path dictionary_file;
    std::string dictionary_id;
};

struct DiscoveryIssue {
    std::filesystem::path path;
    std::string message;
};

struct DiscoveryResult {
    std::vector<DataFiles> dictionaries;
    std::vector<DiscoveryIssue> issues;
};

DiscoveryResult Discover(const std::filesystem::path& directory);

}  // namespace goldendict::core::morphology::hunspell

#endif  // GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_DISCOVERY_H_
