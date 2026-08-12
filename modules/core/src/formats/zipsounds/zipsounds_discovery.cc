// SPDX-License-Identifier: GPL-3.0-or-later
#include "zipsounds_discovery.h"
#include <algorithm>
#include <cctype>

namespace goldendict::core::formats::zipsounds {
namespace {
bool IsDictionary(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".zips";
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
            if (IsDictionary(root))
                result.dictionary_files.push_back(root);
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
            if (it->is_regular_file(error) && !error && IsDictionary(path))
                result.dictionary_files.push_back(path);
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
    std::sort(result.dictionary_files.begin(), result.dictionary_files.end());
    result.dictionary_files.erase(std::unique(result.dictionary_files.begin(),
                                              result.dictionary_files.end()),
                                  result.dictionary_files.end());
    return result;
}
}  // namespace goldendict::core::formats::zipsounds
