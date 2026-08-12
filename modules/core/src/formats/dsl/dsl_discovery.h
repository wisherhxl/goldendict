// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_DISCOVERY_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_DISCOVERY_H_

#include <filesystem>
#include <string>
#include <vector>

namespace goldendict::core::formats::dsl {

struct DiscoveryIssue {
    std::filesystem::path path;
    std::string message;
};

struct DiscoveryResult {
    std::vector<std::filesystem::path> dictionary_files;
    std::vector<DiscoveryIssue> issues;
};

DiscoveryResult Discover(
    const std::vector<std::filesystem::path>& dictionary_roots);

}  // namespace goldendict::core::formats::dsl

#endif  // GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_DISCOVERY_H_
