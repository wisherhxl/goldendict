// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_READER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_READER_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "../../dictionary/generated_index.h"
#include "../../dictionary/ordered_headword_index.h"

namespace goldendict::core::formats::stardict {

enum class ErrorCode {
    kMissingFile,
    kInvalidInfo,
    kInvalidIndex,
    kInvalidDictionary,
    kIndexStorage,
    kUnsupportedFeature,
};

enum class IndexState {
    kSourceOnly,
    kCreated,
    kReused,
    kRebuiltStale,
    kRebuiltCorrupt,
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
    std::string source_language;
    std::string target_language;
    std::string description;
    std::uint64_t word_count = 0;
    std::uint64_t synonym_count = 0;
    std::uint64_t index_file_size = 0;
    std::string same_type_sequence;
};

struct Article {
    std::string headword;
    std::string data;
};

struct PrimaryArticle {
    std::size_t record_ordinal = 0U;
    std::string headword;
    std::uint32_t article_offset = 0U;
    std::uint32_t article_size = 0U;
    std::string data;
};

class Reader final {
   public:
    static Reader Open(const std::filesystem::path& info_path,
                       const std::optional<std::filesystem::path>&
                           generated_index_path = std::nullopt);

    const Metadata& metadata() const noexcept { return metadata_; }

    std::size_t headword_count() const noexcept {
        std::uint32_t count = static_cast<std::uint32_t>(metadata_.word_count);
        count += static_cast<std::uint32_t>(metadata_.synonym_count);
        return static_cast<std::size_t>(count);
    }

    std::size_t article_count() const noexcept {
        return static_cast<std::size_t>(metadata_.word_count);
    }

    IndexState index_state() const noexcept { return index_state_; }

    const dictionary::SourceSnapshot& source_snapshot() const noexcept {
        return source_snapshot_;
    }

    std::vector<PrimaryArticle> ReadPrimaryArticles(
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
    std::vector<std::string> FindHeadwordsForSynonym(
        std::string_view headword, std::size_t result_limit,
        const std::function<void()>& checkpoint = {}) const;
    std::pair<std::vector<std::string>, bool> EnumerateHeadwords(
        std::size_t offset, std::size_t result_limit, std::size_t byte_limit,
        const std::function<void()>& checkpoint = {}) const;

   private:
    struct IndexRecord {
        std::string headword;
        std::string primary_headword;
        std::string folded_headword;
        std::uint32_t article_offset = 0;
        std::uint32_t article_size = 0;
    };

    std::vector<const IndexRecord*> RankedPrefixMatches(
        std::string_view prefix, const std::function<void()>& checkpoint) const;

    Metadata metadata_;
    std::vector<IndexRecord> index_;
    std::size_t primary_record_count_ = 0U;
    std::string dictionary_data_;
    IndexState index_state_ = IndexState::kSourceOnly;
    dictionary::SourceSnapshot source_snapshot_;
    dictionary::OrderedHeadwordIndex enumeration_index_;
};

}  // namespace goldendict::core::formats::stardict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_READER_H_
