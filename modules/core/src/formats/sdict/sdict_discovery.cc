// SPDX-License-Identifier: GPL-3.0-or-later

#include "sdict_discovery.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace goldendict::core::formats::sdict {
namespace {

bool IsDictionaryFile(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension == ".dct";
}

void AddIssue(const std::filesystem::path& path, const std::error_code& error,
              DiscoveryResult* result) {
    result->issues.push_back({path, error.message()});
}

}  // namespace

DiscoveryResult Discover(
    const std::vector<std::filesystem::path>& dictionary_roots) {
    DiscoveryResult result;
    for (const auto& root : dictionary_roots) {
        std::error_code error;
        const auto status = std::filesystem::status(root, error);
        if (error) {
            AddIssue(root, error, &result);
            continue;
        }
        if (!std::filesystem::exists(status)) {
            continue;
        }
        if (std::filesystem::is_regular_file(status)) {
            if (IsDictionaryFile(root)) {
                result.dictionary_files.push_back(root);
            }
            continue;
        }
        if (!std::filesystem::is_directory(status)) {
            continue;
        }
        std::filesystem::recursive_directory_iterator iterator(
            root, std::filesystem::directory_options::skip_permission_denied,
            error);
        const std::filesystem::recursive_directory_iterator end;
        if (error) {
            AddIssue(root, error, &result);
            continue;
        }
        while (iterator != end) {
            const auto path = iterator->path();
            const bool regular = iterator->is_regular_file(error);
            if (error) {
                AddIssue(path, error, &result);
                error.clear();
            } else if (regular && IsDictionaryFile(path)) {
                result.dictionary_files.push_back(path);
            }
            iterator.increment(error);
            if (error) {
                AddIssue(path, error, &result);
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

}  // namespace goldendict::core::formats::sdict
