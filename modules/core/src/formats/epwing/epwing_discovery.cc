// SPDX-License-Identifier: GPL-3.0-or-later
#include "epwing_discovery.h"
#include <algorithm>
#include <cctype>

namespace goldendict::core::formats::epwing {
namespace {
bool IsCatalog(const std::filesystem::path& path) {
    std::string name = path.filename().string();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name == "catalogs";
}
}  // namespace

DiscoveryResult Discover(const std::vector<std::filesystem::path>& roots) {
    DiscoveryResult result;
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
            if (IsCatalog(root))
                result.catalog_files.push_back(root);
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
            if (it->is_regular_file(error) && !error && IsCatalog(path))
                result.catalog_files.push_back(path);
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
    std::sort(result.catalog_files.begin(), result.catalog_files.end());
    result.catalog_files.erase(
        std::unique(result.catalog_files.begin(), result.catalog_files.end()),
        result.catalog_files.end());
    return result;
}
}  // namespace goldendict::core::formats::epwing
