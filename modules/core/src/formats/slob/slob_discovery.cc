// SPDX-License-Identifier: GPL-3.0-or-later
#include "slob_discovery.h"
#include <algorithm>
#include <cctype>

namespace goldendict::core::formats::slob {
namespace {
bool IsSlob(const std::filesystem::path& path) {
    std::string name = path.filename().string();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name.size() >= 5U &&
           name.compare(name.size() - 5U, 5U, ".slob") == 0;
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
            if (IsSlob(root))
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
            if (it->is_regular_file(error) && !error && IsSlob(path))
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
}  // namespace goldendict::core::formats::slob
