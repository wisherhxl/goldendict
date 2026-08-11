// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_READER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_READER_H_

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace goldendict::core::formats::stardict {

enum class ErrorCode {
    kMissingFile,
    kInvalidInfo,
    kInvalidIndex,
    kInvalidDictionary,
    kUnsupportedFeature,
};

class Error final : public std::runtime_error {
   public:
    Error(ErrorCode code, std::filesystem::path path, std::string message);

    ErrorCode code() const noexcept { return code_; }

    const std::filesystem::path& path() const noexcept { return path_; }

   private:
    ErrorCode code_;
    std::filesystem::path path_;
};

struct Metadata {
    std::string book_name;
    std::uint64_t word_count = 0;
    std::uint64_t index_file_size = 0;
    std::string same_type_sequence;
};

struct Article {
    std::string headword;
    std::string data;
};

class Reader final {
   public:
    static Reader Open(const std::filesystem::path& info_path);

    const Metadata& metadata() const noexcept { return metadata_; }

    std::vector<Article> LookupExact(std::string_view headword) const;

   private:
    struct IndexRecord {
        std::string headword;
        std::uint32_t article_offset = 0;
        std::uint32_t article_size = 0;
    };

    Metadata metadata_;
    std::vector<IndexRecord> index_;
    std::string dictionary_data_;
};

}  // namespace goldendict::core::formats::stardict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_READER_H_
