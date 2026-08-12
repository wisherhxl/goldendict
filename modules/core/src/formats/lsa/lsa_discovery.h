// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_SRC_FORMATS_LSA_LSA_DISCOVERY_H_
#define GOLDENDICT_CORE_SRC_FORMATS_LSA_LSA_DISCOVERY_H_
#include <filesystem>
#include <string>
#include <vector>

namespace goldendict::core::formats::lsa {
struct DiscoveryIssue {
    std::filesystem::path path;
    std::string message;
};

struct DiscoveryResult {
    std::vector<std::filesystem::path> dictionary_files;
    std::vector<DiscoveryIssue> issues;
};

DiscoveryResult Discover(const std::vector<std::filesystem::path>& roots);
}  // namespace goldendict::core::formats::lsa
#endif
