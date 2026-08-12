// SPDX-License-Identifier: GPL-3.0-or-later

#include "mdict_discovery.h"

#include <algorithm>
#include <cctype>

namespace goldendict::core::formats::mdict {
namespace {

bool HasExtension(const std::filesystem::path& path, std::string extension) {
    std::string actual = path.extension().string();
    std::transform(actual.begin(), actual.end(), actual.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return actual == extension;
}

std::vector<std::filesystem::path> CompanionResources(
    const std::filesystem::path& mdx) {
    std::vector<std::filesystem::path> resources;
    std::error_code error;
    auto base = mdx;
    base.replace_extension();
    auto candidate = base;
    candidate += ".mdd";
    if (std::filesystem::is_regular_file(candidate, error) && !error) {
        resources.push_back(candidate);
        for (std::size_t volume = 1;; ++volume) {
            candidate = base;
            candidate += "." + std::to_string(volume) + ".mdd";
            if (!std::filesystem::is_regular_file(candidate, error) || error)
                break;
            resources.push_back(candidate);
        }
    }
    return resources;
}

}  // namespace

DiscoveryResult Discover(const std::vector<std::filesystem::path>& roots) {
    DiscoveryResult result;
    std::vector<std::filesystem::path> mdx_files;
    for (const auto& root : roots) {
        std::error_code error;
        const auto status = std::filesystem::status(root, error);
        if (error) {
            result.issues.push_back({root, error.message()});
            continue;
        }
        if (!std::filesystem::exists(status))
            continue;
        if (std::filesystem::is_regular_file(status)) {
            if (HasExtension(root, ".mdx"))
                mdx_files.push_back(root);
            continue;
        }
        if (!std::filesystem::is_directory(status))
            continue;
        std::filesystem::recursive_directory_iterator it(
            root, std::filesystem::directory_options::skip_permission_denied,
            error),
            end;
        while (!error && it != end) {
            const auto path = it->path();
            if (it->is_regular_file(error) && !error &&
                HasExtension(path, ".mdx"))
                mdx_files.push_back(path);
            else if (error) {
                result.issues.push_back({path, error.message()});
                error.clear();
            }
            it.increment(error);
            if (error) {
                result.issues.push_back({path, error.message()});
                error.clear();
            }
        }
    }
    std::sort(mdx_files.begin(), mdx_files.end());
    mdx_files.erase(std::unique(mdx_files.begin(), mdx_files.end()),
                    mdx_files.end());
    for (const auto& mdx : mdx_files)
        result.dictionaries.push_back({mdx, CompanionResources(mdx)});
    return result;
}

}  // namespace goldendict::core::formats::mdict
