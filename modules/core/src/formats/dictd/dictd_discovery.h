// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_DISCOVERY_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_DISCOVERY_H_

#include <filesystem>
#include <string>
#include <vector>

namespace goldendict::core::formats::dictd {

struct DiscoveryIssue {
    std::filesystem::path path;
    std::string message;
};

struct DiscoveryResult {
    std::vector<std::filesystem::path> index_files;
    std::vector<DiscoveryIssue> issues;
};

DiscoveryResult Discover(
    const std::vector<std::filesystem::path>& dictionary_roots);

}  // namespace goldendict::core::formats::dictd

#endif  // GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_DISCOVERY_H_
