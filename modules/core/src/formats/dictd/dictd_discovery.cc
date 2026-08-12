// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictd_discovery.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace goldendict::core::formats::dictd {
namespace {

bool IsIndexFile(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension == ".index";
}

bool HasDictionaryData(const std::filesystem::path& index_path) {
    auto base = index_path;
    base.replace_extension();
    std::error_code error;
    const bool plain =
        std::filesystem::is_regular_file(base.string() + ".dict", error);
    if (plain && !error) {
        return true;
    }
    error.clear();
    return std::filesystem::is_regular_file(base.string() + ".dict.dz",
                                            error) &&
           !error;
}

void Consider(const std::filesystem::path& path, DiscoveryResult* result) {
    if (!IsIndexFile(path)) {
        return;
    }
    if (HasDictionaryData(path)) {
        result->index_files.push_back(path);
    } else {
        result->issues.push_back(
            {path, "Dictd index has no .dict or .dict.dz companion"});
    }
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
            Consider(root, &result);
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
            } else if (regular) {
                Consider(path, &result);
            }
            iterator.increment(error);
            if (error) {
                AddIssue(path, error, &result);
                error.clear();
            }
        }
    }
    std::sort(result.index_files.begin(), result.index_files.end());
    result.index_files.erase(
        std::unique(result.index_files.begin(), result.index_files.end()),
        result.index_files.end());
    return result;
}

}  // namespace goldendict::core::formats::dictd
