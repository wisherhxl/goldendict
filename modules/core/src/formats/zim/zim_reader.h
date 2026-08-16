// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_SRC_FORMATS_ZIM_ZIM_READER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_ZIM_ZIM_READER_H_
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "../../dictionary/generated_index.h"
#include "../../dictionary/ordered_headword_index.h"
#include "zim_discovery.h"

namespace goldendict::core::formats::zim {
enum class ErrorCode { kMissingFile, kInvalidDictionary };

class Error final : public std::runtime_error {
   public:
    Error(ErrorCode code, std::filesystem::path path, std::string message);

    ErrorCode code() const noexcept { return code_; }

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
    std::string headword;
    std::string data;
    std::size_t first_record_ordinal = 0;
    std::size_t article_ordinal = 0;
    std::size_t target_entry_index = 0;
    std::uint32_t cluster_index = 0;
    std::uint32_t blob_index = 0;
};

class Reader final {
   public:
    static Reader Open(const Files& files);

    const Metadata& metadata() const noexcept { return metadata_; }

    std::size_t headword_count() const noexcept { return records_.size(); }

    std::size_t article_count() const noexcept { return articles_.size(); }

    const Files& files() const noexcept { return files_; }

    const dictionary::SourceSnapshot& source_snapshot() const noexcept {
        return source_snapshot_;
    }

    std::vector<FullTextArticle> ReadFullTextArticles() const;

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
    const std::string* Resource(std::string_view id) const;

   private:
    struct Record {
        std::string word;
        std::string folded;
        std::size_t article;
    };

    std::vector<const Record*> Ranked(
        std::string_view prefix, const std::function<void()>& checkpoint) const;
    Files files_;
    Metadata metadata_;
    std::vector<Record> records_;
    dictionary::OrderedHeadwordIndex enumeration_index_;
    std::vector<std::string> articles_;
    std::vector<FullTextArticle> full_text_articles_;
    std::unordered_map<std::string, std::string> resources_;
    dictionary::SourceSnapshot source_snapshot_;
};
}  // namespace goldendict::core::formats::zim
#endif
