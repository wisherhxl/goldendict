// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_discovery.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace goldendict::core::formats::stardict {
namespace {

bool IsInfoFile(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension == ".ifo";
}

void AddIssue(const std::filesystem::path& path, const std::error_code& error,
              std::vector<DiscoveryIssue>* issues) {
    issues->push_back({path, error.message()});
}

}  // namespace

DiscoveryResult Discover(
    const std::vector<std::filesystem::path>& dictionary_roots) {
    DiscoveryResult result;
    for (const auto& root : dictionary_roots) {
        std::error_code filesystem_error;
        const auto status = std::filesystem::status(root, filesystem_error);
        if (filesystem_error) {
            AddIssue(root, filesystem_error, &result.issues);
            continue;
        }
        if (!std::filesystem::exists(status)) {
            result.issues.push_back({root, "Dictionary root does not exist"});
            continue;
        }
        if (std::filesystem::is_regular_file(status)) {
            if (IsInfoFile(root)) {
                result.info_files.push_back(root);
            }
            continue;
        }
        if (!std::filesystem::is_directory(status)) {
            result.issues.push_back(
                {root, "Dictionary root is not a file or directory"});
            continue;
        }

        std::filesystem::recursive_directory_iterator iterator(
            root, std::filesystem::directory_options::skip_permission_denied,
            filesystem_error);
        const std::filesystem::recursive_directory_iterator end;
        if (filesystem_error) {
            AddIssue(root, filesystem_error, &result.issues);
            continue;
        }
        while (iterator != end) {
            const auto entry_path = iterator->path();
            const bool is_regular = iterator->is_regular_file(filesystem_error);
            if (filesystem_error) {
                AddIssue(entry_path, filesystem_error, &result.issues);
                filesystem_error.clear();
            } else if (is_regular && IsInfoFile(entry_path)) {
                result.info_files.push_back(entry_path);
            }
            iterator.increment(filesystem_error);
            if (filesystem_error) {
                AddIssue(entry_path, filesystem_error, &result.issues);
                filesystem_error.clear();
            }
        }
    }

    std::sort(result.info_files.begin(), result.info_files.end());
    result.info_files.erase(
        std::unique(result.info_files.begin(), result.info_files.end()),
        result.info_files.end());
    return result;
}

}  // namespace goldendict::core::formats::stardict
