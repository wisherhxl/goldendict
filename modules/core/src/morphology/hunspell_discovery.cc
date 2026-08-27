// SPDX-License-Identifier: GPL-3.0-or-later

#include "hunspell_discovery.h"

#include <algorithm>
#include <set>
#include <system_error>
#include <utility>

namespace goldendict::core::morphology::hunspell {
namespace {

bool IsAffixFile(const std::filesystem::path& path) {
    const auto extension = path.extension().string();
    return extension == ".aff" || extension == ".AFF";
}

void AddIssue(const std::filesystem::path& path, std::string message,
              DiscoveryResult* result) {
    result->issues.push_back({path, std::move(message)});
}

std::filesystem::path AbsoluteNormalized(const std::filesystem::path& path,
                                         std::error_code* error) {
    return std::filesystem::absolute(path, *error).lexically_normal();
}

bool IsRegularFileOrMissing(const std::filesystem::path& path,
                            std::error_code* error) {
    const auto status = std::filesystem::status(path, *error);
    if (*error == std::errc::no_such_file_or_directory) {
        error->clear();
        return false;
    }
    return !*error && std::filesystem::is_regular_file(status);
}

}  // namespace

DiscoveryResult Discover(const std::filesystem::path& directory) {
    DiscoveryResult result;
    if (directory.empty()) {
        return result;
    }

    std::error_code error;
    const auto status = std::filesystem::status(directory, error);
    if (error) {
        AddIssue(directory, error.message(), &result);
        return result;
    }
    if (!std::filesystem::exists(status)) {
        AddIssue(directory, "Hunspell dictionary directory does not exist",
                 &result);
        return result;
    }
    if (!std::filesystem::is_directory(status)) {
        AddIssue(directory, "Hunspell dictionary root is not a directory",
                 &result);
        return result;
    }

    std::vector<std::filesystem::path> affix_files;
    std::filesystem::directory_iterator iterator(
        directory, std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::directory_iterator end;
    if (error) {
        AddIssue(directory, error.message(), &result);
        return result;
    }
    std::size_t entry_count = 0U;
    while (iterator != end) {
        const auto path = iterator->path();
        ++entry_count;
        if (entry_count > kMaximumDirectoryEntries) {
            AddIssue(directory,
                     "Hunspell dictionary directory entry limit exceeded",
                     &result);
            return result;
        }
        const bool regular = iterator->is_regular_file(error);
        if (error) {
            AddIssue(path, error.message(), &result);
            error.clear();
        } else if (regular && IsAffixFile(path)) {
            affix_files.push_back(path);
        }
        iterator.increment(error);
        if (error) {
            AddIssue(path, error.message(), &result);
            error.clear();
        }
    }
    std::sort(affix_files.begin(), affix_files.end());

    std::set<std::string> dictionary_ids;
    for (const auto& affix_file : affix_files) {
        const std::string dictionary_id = affix_file.stem().string();
        auto dictionary_file =
            affix_file.parent_path() / (dictionary_id + ".dic");
        bool has_dictionary = IsRegularFileOrMissing(dictionary_file, &error);
        if (error) {
            AddIssue(dictionary_file, error.message(), &result);
            error.clear();
            continue;
        }
        if (!has_dictionary) {
            dictionary_file =
                affix_file.parent_path() / (dictionary_id + ".DIC");
            has_dictionary = IsRegularFileOrMissing(dictionary_file, &error);
            if (error) {
                AddIssue(dictionary_file, error.message(), &result);
                error.clear();
                continue;
            }
        }
        if (!has_dictionary) {
            AddIssue(affix_file,
                     "Hunspell affix file has no .dic or .DIC companion",
                     &result);
            continue;
        }
        if (!dictionary_ids.insert(dictionary_id).second) {
            AddIssue(affix_file, "Duplicate Hunspell dictionary id", &result);
            continue;
        }

        auto absolute_affix = AbsoluteNormalized(affix_file, &error);
        if (error) {
            AddIssue(affix_file, error.message(), &result);
            error.clear();
            continue;
        }
        auto absolute_dictionary = AbsoluteNormalized(dictionary_file, &error);
        if (error) {
            AddIssue(dictionary_file, error.message(), &result);
            error.clear();
            continue;
        }
        result.dictionaries.push_back({std::move(absolute_affix),
                                       std::move(absolute_dictionary),
                                       dictionary_id});
    }
    return result;
}

}  // namespace goldendict::core::morphology::hunspell
