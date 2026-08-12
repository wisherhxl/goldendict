// SPDX-License-Identifier: GPL-3.0-or-later

#include "xdxf_discovery.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace goldendict::core::formats::xdxf {
namespace {

bool IsDictionaryFile(const std::filesystem::path& path) {
    std::string filename = path.filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return filename.size() >= 5U &&
           (filename.compare(filename.size() - 5U, 5U, ".xdxf") == 0 ||
            (filename.size() >= 8U &&
             filename.compare(filename.size() - 8U, 8U, ".xdxf.dz") == 0));
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

}  // namespace goldendict::core::formats::xdxf
