// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_CONTENT_H_
#define GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_CONTENT_H_

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "hunspell_discovery.h"

namespace goldendict::core::morphology::hunspell {

inline constexpr std::size_t kMaximumAffixBytes = 16U * 1024U * 1024U;
inline constexpr std::size_t kMaximumDictionaryBytes = 256U * 1024U * 1024U;
inline constexpr std::size_t kMaximumContentBytes =
    kMaximumAffixBytes + kMaximumDictionaryBytes;
inline constexpr std::size_t kMaximumContentLineBytes = 1024U * 1024U;
inline constexpr std::size_t kMaximumDictionaryEntries = 2000000U;

enum class ContentErrorCode {
    kUnsafePath,
    kMissingFile,
    kResourceLimit,
    kInvalidAffix,
    kUnsupportedEncoding,
    kInvalidEncoding,
    kInvalidDictionary,
};

class ContentError final : public std::runtime_error {
   public:
    ContentError(ContentErrorCode code, std::filesystem::path path,
                 std::string message);

    ContentErrorCode code() const noexcept { return code_; }

    const std::filesystem::path& path() const noexcept { return path_; }

   private:
    ContentErrorCode code_;
    std::filesystem::path path_;
};

struct Content {
    DataFiles files;
    std::string encoding;
    std::string affix_bytes;
    std::string dictionary_bytes;
    std::size_t dictionary_entry_count = 0U;
};

Content LoadContent(const DataFiles& files);

}  // namespace goldendict::core::morphology::hunspell

#endif  // GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_CONTENT_H_
