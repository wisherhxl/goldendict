// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_SRC_FORMATS_SOUNDDIR_SOUNDDIR_READER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_SOUNDDIR_SOUNDDIR_READER_H_
#include <cstddef>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "../../dictionary/ordered_headword_index.h"

namespace goldendict::core::formats::sounddir {
enum class ErrorCode { kMissingDirectory, kInvalidDirectory };

class Error final : public std::runtime_error {
   public:
    Error(ErrorCode code, std::filesystem::path path, std::string message);

    ErrorCode code() const noexcept { return code_; }

   private:
    ErrorCode code_;
};

struct Metadata {
    std::string name;
};

struct Article {
    std::string headword;
    std::string data;
};

class Reader final {
   public:
    static Reader Open(const std::filesystem::path& path,
                       std::string display_name);

    const Metadata& metadata() const noexcept { return metadata_; }

    std::size_t headword_count() const noexcept { return records_.size(); }

    std::size_t article_count() const noexcept { return records_.size(); }

    const std::filesystem::path& directory_path() const noexcept {
        return root_;
    }

    std::vector<Article> LookupExact(
        std::string_view word,
        std::size_t limit = std::numeric_limits<std::size_t>::max(),
        const std::function<void()>& checkpoint = {}) const;
    std::vector<Article> LookupPrefix(
        std::string_view prefix,
        std::size_t limit = std::numeric_limits<std::size_t>::max(),
        const std::function<void()>& checkpoint = {}) const;
    std::vector<std::string> SuggestPrefix(
        std::string_view prefix,
        std::size_t limit = std::numeric_limits<std::size_t>::max(),
        const std::function<void()>& checkpoint = {}) const;
    std::pair<std::vector<std::string>, bool> EnumerateHeadwords(
        std::size_t offset, std::size_t result_limit, std::size_t byte_limit,
        const std::function<void()>& checkpoint = {}) const;
    std::string Resource(std::string_view id) const;

   private:
    struct Record {
        std::string word;
        std::string folded;
        std::string resource_id;
        std::string media_type;
    };

    std::vector<const Record*> Ranked(
        std::string_view prefix, const std::function<void()>& checkpoint) const;
    std::filesystem::path root_;
    Metadata metadata_;
    std::vector<Record> records_;
    dictionary::OrderedHeadwordIndex enumeration_index_;
};
}  // namespace goldendict::core::formats::sounddir
#endif
