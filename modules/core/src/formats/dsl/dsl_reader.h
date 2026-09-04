// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_READER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_READER_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "../../dictionary/generated_index.h"
#include "../../dictionary/ordered_headword_index.h"

namespace goldendict::core::formats::dsl {

inline constexpr std::size_t kMaximumDictionaryBytes =
    1024U * 1024U * 1024U;

enum class ErrorCode { kMissingFile, kInvalidDictionary };

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
    std::string name;
    std::string source_language;
    std::string target_language;
    std::string description;
};

struct Article {
    std::string headword;
    std::string data;
};

struct FullTextArticle {
    std::size_t record_ordinal = 0U;
    std::string headword;
    std::size_t article_ordinal = 0U;
    std::string data;
};

class Reader final {
   public:
    static Reader Open(const std::filesystem::path& dictionary_path,
                       std::string_view preferred_language = {});

    const Metadata& metadata() const noexcept { return metadata_; }

    std::size_t headword_count() const noexcept { return headword_count_; }

    std::size_t article_count() const noexcept { return articles_.size(); }

    const std::filesystem::path& dictionary_path() const noexcept {
        return dictionary_path_;
    }

    const dictionary::SourceSnapshot& source_snapshot() const noexcept {
        return source_snapshot_;
    }

    std::vector<FullTextArticle> ReadFullTextArticles(
        const std::function<void()>& checkpoint = {}) const;

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
    std::pair<std::vector<std::string>, bool> EnumerateHeadwords(
        std::size_t offset, std::size_t result_limit, std::size_t byte_limit,
        const std::function<void()>& checkpoint = {}) const;

   private:
    struct Record {
        std::string headword;
        std::string folded_headword;
        std::size_t article = 0;
    };

    struct FullTextSource {
        std::size_t first_record_ordinal = 0U;
        std::string canonical_headword;
        std::size_t article = 0U;
    };

    std::vector<const Record*> RankedPrefixMatches(
        std::string_view prefix, const std::function<void()>& checkpoint) const;

    std::filesystem::path dictionary_path_;
    Metadata metadata_;
    std::vector<Record> records_;
    std::vector<FullTextSource> full_text_sources_;
    std::size_t headword_count_ = 0U;
    dictionary::OrderedHeadwordIndex enumeration_index_;
    std::vector<std::string> articles_;
    dictionary::SourceSnapshot source_snapshot_;
};

}  // namespace goldendict::core::formats::dsl

#endif  // GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_READER_H_
