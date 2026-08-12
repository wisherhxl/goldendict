// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/history_store.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

#include "../foundation/utf8.h"

namespace goldendict::core {
namespace {

constexpr std::string_view kHeader = "goldendict-history-v1\n";
constexpr std::size_t kMaximumHistoryBytes = 1024U * 1024U;
constexpr std::size_t kMaximumWordBytes = 4096U;
constexpr std::size_t kMaximumEntryLimit = 10000U;

bool Exists(const std::string& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw std::runtime_error("Cannot inspect history path: " +
                                 error.message());
    }
    return exists;
}

std::string ReadBounded(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open history file");
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 || static_cast<std::uintmax_t>(size) > kMaximumHistoryBytes) {
        throw std::runtime_error("Cannot read bounded history file");
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    input.seekg(0, std::ios::beg);
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input ||
        input.gcount() != static_cast<std::streamsize>(contents.size())) {
        throw std::runtime_error("Cannot read complete history file");
    }
    return contents;
}

void ValidateLimit(std::size_t maximum_entries) {
    if (maximum_entries == 0U || maximum_entries > kMaximumEntryLimit) {
        throw std::runtime_error("History entry limit is invalid");
    }
}

void ValidateEntry(const HistoryEntry& entry) {
    if (entry.word.empty() || entry.word.size() > kMaximumWordBytes ||
        entry.word.find_first_of("\r\n\0", 0U, 3U) != std::string::npos ||
        !foundation::IsValidUtf8(entry.word)) {
        throw std::runtime_error("History entry is invalid");
    }
}

std::vector<HistoryEntry> ParseLines(std::string_view contents,
                                     std::size_t maximum_entries) {
    ValidateLimit(maximum_entries);
    std::vector<HistoryEntry> entries;
    std::size_t position = 0U;
    while (position < contents.size() && entries.size() < maximum_entries) {
        const auto end = contents.find('\n', position);
        if (end == std::string_view::npos) {
            throw std::runtime_error("History line is not terminated");
        }
        const std::string_view line = contents.substr(position, end - position);
        const auto separator = line.find(' ');
        if (separator == std::string_view::npos || separator == 0U ||
            separator + 1U == line.size()) {
            throw std::runtime_error("History line is malformed");
        }
        HistoryEntry entry;
        const auto conversion = std::from_chars(
            line.data(), line.data() + separator, entry.group_id, 10);
        if (conversion.ec != std::errc() ||
            conversion.ptr != line.data() + separator) {
            throw std::runtime_error("History group ID is invalid");
        }
        entry.word = std::string(line.substr(separator + 1U));
        ValidateEntry(entry);
        entries.push_back(std::move(entry));
        position = end + 1U;
    }
    return entries;
}

std::vector<HistoryEntry> LoadCurrent(const std::string& path,
                                      std::size_t maximum_entries) {
    const std::string contents = ReadBounded(path);
    if (contents.substr(0U, kHeader.size()) != kHeader) {
        throw std::runtime_error("Unsupported history format");
    }
    return ParseLines(std::string_view(contents).substr(kHeader.size()),
                      maximum_entries);
}

std::vector<HistoryEntry> LoadLegacy(const std::string& path,
                                     std::size_t maximum_entries) {
    return ParseLines(ReadBounded(path), maximum_entries);
}

std::string_view TrimAsciiWhitespace(std::string_view text) {
    constexpr std::string_view kWhitespace = " \t\r";
    const auto first = text.find_first_not_of(kWhitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(kWhitespace);
    return text.substr(first, last - first + 1U);
}

}  // namespace

std::vector<HistoryEntry> LoadHistory(const std::string& history_path,
                                      std::size_t maximum_entries) {
    ValidateLimit(maximum_entries);
    if (!Exists(history_path)) {
        return {};
    }
    return LoadCurrent(history_path, maximum_entries);
}

void SaveHistory(const std::string& history_path,
                 const std::vector<HistoryEntry>& entries) {
    if (entries.size() > kMaximumEntryLimit) {
        throw std::runtime_error("History has too many entries");
    }
    std::string contents(kHeader);
    for (const auto& entry : entries) {
        ValidateEntry(entry);
        contents += std::to_string(entry.group_id) + " " + entry.word + "\n";
        if (contents.size() > kMaximumHistoryBytes) {
            throw std::runtime_error("History exceeds the size limit");
        }
    }
    const std::filesystem::path destination(history_path);
    if (!destination.parent_path().empty()) {
        std::filesystem::create_directories(destination.parent_path());
    }
    const std::string temporary = destination.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(contents.data(),
                     static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("Cannot write history file");
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Cannot replace history file: " +
                                 error.message());
    }
}

std::vector<HistoryEntry> ImportHistoryText(const std::string& import_path,
                                            std::size_t maximum_entries,
                                            std::uint32_t group_id) {
    ValidateLimit(maximum_entries);
    const std::string contents = ReadBounded(import_path);
    std::string_view text(contents);
    constexpr std::string_view kUtf8Bom("\xEF\xBB\xBF", 3U);
    if (text.substr(0U, kUtf8Bom.size()) == kUtf8Bom) {
        text.remove_prefix(kUtf8Bom.size());
    }
    if (!foundation::IsValidUtf8(text)) {
        throw std::runtime_error("History import is not valid UTF-8");
    }

    std::vector<HistoryEntry> entries;
    std::size_t position = 0U;
    while (position <= text.size() && entries.size() < maximum_entries) {
        const auto end = text.find('\n', position);
        const auto length = end == std::string_view::npos
                                ? text.size() - position
                                : end - position;
        const std::string_view word =
            TrimAsciiWhitespace(text.substr(position, length));
        if (!word.empty()) {
            HistoryEntry entry{group_id, std::string(word)};
            ValidateEntry(entry);
            entries.push_back(std::move(entry));
        }
        if (end == std::string_view::npos) {
            break;
        }
        position = end + 1U;
    }
    return entries;
}

std::vector<HistoryEntry> LoadOrMigrateHistory(
    const std::string& history_path, const std::string& legacy_history_path,
    std::size_t maximum_entries) {
    ValidateLimit(maximum_entries);
    if (Exists(history_path)) {
        return LoadCurrent(history_path, maximum_entries);
    }
    if (!Exists(legacy_history_path)) {
        return {};
    }
    std::vector<HistoryEntry> entries =
        LoadLegacy(legacy_history_path, maximum_entries);
    SaveHistory(history_path, entries);
    return entries;
}

}  // namespace goldendict::core
