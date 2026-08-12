// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_READER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_READER_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace goldendict::core::formats::dictd {

enum class ErrorCode {
    kMissingFile,
    kInvalidIndex,
    kInvalidDictionary,
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

struct Article {
    std::string headword;
    std::string data;
};

class Reader final {
   public:
    static Reader Open(const std::filesystem::path& index_path);

    const std::string& name() const noexcept { return name_; }

    std::size_t headword_count() const noexcept { return headword_count_; }

    std::size_t article_count() const noexcept { return article_count_; }

    const std::filesystem::path& index_path() const noexcept {
        return index_path_;
    }

    std::vector<Article> LookupExact(
        std::string_view headword,
        std::size_t result_limit = std::numeric_limits<std::size_t>::max(),
        const std::function<void()>& checkpoint = {}) const;
    std::vector<Article> LookupPrefix(
        std::string_view prefix,
        std::size_t result_limit = std::numeric_limits<std::size_t>::max(),
        const std::function<void()>& checkpoint = {}) const;
    std::vector<std::string> SuggestPrefix(
        std::string_view prefix,
        std::size_t result_limit = std::numeric_limits<std::size_t>::max(),
        const std::function<void()>& checkpoint = {}) const;

   private:
    struct Record {
        std::string headword;
        std::string folded_headword;
        std::uint32_t article_offset = 0;
        std::uint32_t article_size = 0;
    };

    std::vector<const Record*> RankedPrefixMatches(
        std::string_view prefix, const std::function<void()>& checkpoint) const;
    Article LoadArticle(const Record& record) const;

    std::filesystem::path index_path_;
    std::string name_;
    std::vector<Record> records_;
    std::string dictionary_data_;
    std::size_t headword_count_ = 0;
    std::size_t article_count_ = 0;
};

}  // namespace goldendict::core::formats::dictd

#endif  // GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_READER_H_
