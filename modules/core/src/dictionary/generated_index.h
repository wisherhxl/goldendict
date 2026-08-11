// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_GENERATED_INDEX_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_GENERATED_INDEX_H_

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace goldendict::core::dictionary {

struct SourceStamp {
    std::string path;
    std::uint64_t size = 0;
    std::int64_t modified = 0;

    friend bool operator==(const SourceStamp& left, const SourceStamp& right) {
        return left.path == right.path && left.size == right.size &&
               left.modified == right.modified;
    }
};

using SourceSnapshot = std::vector<SourceStamp>;

enum class GeneratedIndexState {
    kMissing,
    kCurrent,
    kStale,
    kCorrupt,
};

struct GeneratedIndexLoadResult {
    GeneratedIndexState state = GeneratedIndexState::kMissing;
    std::string payload;
};

class GeneratedIndexError final : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

SourceSnapshot CaptureSourceSnapshot(
    const std::vector<std::filesystem::path>& source_files);

GeneratedIndexLoadResult LoadGeneratedIndex(
    const std::filesystem::path& index_path, std::string_view format,
    const SourceSnapshot& expected_sources);

void StoreGeneratedIndex(const std::filesystem::path& index_path,
                         std::string_view format, const SourceSnapshot& sources,
                         std::string_view payload);

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_GENERATED_INDEX_H_
