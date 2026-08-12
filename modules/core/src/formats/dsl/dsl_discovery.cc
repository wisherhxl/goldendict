// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsl_discovery.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace goldendict::core::formats::dsl {
namespace {

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool IsDictionaryFile(const std::filesystem::path& path) {
    const std::string filename = Lower(path.filename().string());
    const bool compressed =
        filename.size() >= 7U &&
        filename.compare(filename.size() - 7U, 7U, ".dsl.dz") == 0;
    const bool plain = filename.size() >= 4U &&
                       filename.compare(filename.size() - 4U, 4U, ".dsl") == 0;
    if (!plain && !compressed) {
        return false;
    }
    const std::size_t suffix = compressed ? 7U : 4U;
    return filename.size() < suffix + 5U ||
           filename.compare(filename.size() - suffix - 5U, 5U, "_abrv") != 0;
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
        while (!error && iterator != end) {
            const auto path = iterator->path();
            if (iterator->is_regular_file(error) && !error &&
                IsDictionaryFile(path)) {
                result.dictionary_files.push_back(path);
            } else if (error) {
                AddIssue(path, error, &result);
                error.clear();
            }
            iterator.increment(error);
            if (error) {
                AddIssue(path, error, &result);
                error.clear();
            }
        }
        if (error) {
            AddIssue(root, error, &result);
        }
    }
    std::sort(result.dictionary_files.begin(), result.dictionary_files.end());
    result.dictionary_files.erase(std::unique(result.dictionary_files.begin(),
                                              result.dictionary_files.end()),
                                  result.dictionary_files.end());
    return result;
}

}  // namespace goldendict::core::formats::dsl
