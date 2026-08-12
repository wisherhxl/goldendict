// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_DISCOVERY_H_
#define GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_DISCOVERY_H_

#include <filesystem>
#include <string>
#include <vector>

namespace goldendict::core::formats::mdict {

struct DictionaryFiles {
    std::filesystem::path mdx;
    std::vector<std::filesystem::path> mdd;
};

struct DiscoveryIssue {
    std::filesystem::path path;
    std::string message;
};

struct DiscoveryResult {
    std::vector<DictionaryFiles> dictionaries;
    std::vector<DiscoveryIssue> issues;
};

DiscoveryResult Discover(const std::vector<std::filesystem::path>& roots);

}  // namespace goldendict::core::formats::mdict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_DISCOVERY_H_
