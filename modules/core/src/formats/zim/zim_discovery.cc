// SPDX-License-Identifier: GPL-3.0-or-later
#include "zim_discovery.h"
#include <algorithm>
#include <cctype>

namespace goldendict::core::formats::zim {
namespace {
std::string Lower(const std::filesystem::path& path) {
    std::string name = path.filename().string();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name;
}

bool IsPrimary(const std::filesystem::path& path) {
    const std::string name = Lower(path);
    return (name.size() >= 4U &&
            name.compare(name.size() - 4U, 4U, ".zim") == 0) ||
           (name.size() >= 6U &&
            name.compare(name.size() - 6U, 6U, ".zimaa") == 0);
}

std::vector<std::filesystem::path> Parts(const std::filesystem::path& primary) {
    std::vector<std::filesystem::path> parts{primary};
    const std::string name = Lower(primary);
    if (name.size() < 6U || name.compare(name.size() - 6U, 6U, ".zimaa") != 0)
        return parts;
    auto candidate = primary;
    std::string filename = candidate.filename().string();
    for (unsigned first = 0; first < 26U; ++first) {
        for (unsigned second = 0; second < 26U; ++second) {
            if (first == 0U && second == 0U)
                continue;
            filename[filename.size() - 2U] = static_cast<char>('a' + first);
            filename[filename.size() - 1U] = static_cast<char>('a' + second);
            candidate = primary.parent_path() / filename;
            std::error_code error;
            if (!std::filesystem::is_regular_file(candidate, error))
                return parts;
            parts.push_back(candidate);
        }
    }
    return parts;
}
}  // namespace

DiscoveryResult Discover(const std::vector<std::filesystem::path>& roots) {
    DiscoveryResult result;
    std::vector<std::filesystem::path> primary_files;
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
            if (IsPrimary(root))
                primary_files.push_back(root);
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
            if (it->is_regular_file(error) && !error && IsPrimary(path))
                primary_files.push_back(path);
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
    std::sort(primary_files.begin(), primary_files.end());
    primary_files.erase(std::unique(primary_files.begin(), primary_files.end()),
                        primary_files.end());
    for (const auto& primary : primary_files)
        result.dictionaries.push_back({primary, Parts(primary)});
    return result;
}
}  // namespace goldendict::core::formats::zim
