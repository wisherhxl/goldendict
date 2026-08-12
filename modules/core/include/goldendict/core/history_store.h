// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_HISTORY_STORE_H_
#define GOLDENDICT_CORE_HISTORY_STORE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"

namespace goldendict::core {

struct HistoryEntry {
    std::uint32_t group_id = 0U;
    std::string word;

    bool operator==(const HistoryEntry& other) const noexcept {
        return group_id == other.group_id && word == other.word;
    }
};

GOLDENDICT_EXPORTS std::vector<HistoryEntry> LoadHistory(
    const std::string& history_path, std::size_t maximum_entries = 500U);
GOLDENDICT_EXPORTS void SaveHistory(const std::string& history_path,
                                    const std::vector<HistoryEntry>& entries);

// Imports a bounded UTF-8 text export with an optional BOM and one headword
// per line. Blank lines are ignored and surrounding ASCII whitespace is
// removed. The returned order matches the file order.
GOLDENDICT_EXPORTS std::vector<HistoryEntry> ImportHistoryText(
    const std::string& import_path, std::size_t maximum_entries = 500U,
    std::uint32_t group_id = 0U);

// Loads current history when present. Otherwise imports the bounded legacy
// line format, atomically saves the current format, and leaves legacy data
// untouched. Missing files produce an empty history.
GOLDENDICT_EXPORTS std::vector<HistoryEntry> LoadOrMigrateHistory(
    const std::string& history_path, const std::string& legacy_history_path,
    std::size_t maximum_entries = 500U);

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_HISTORY_STORE_H_
