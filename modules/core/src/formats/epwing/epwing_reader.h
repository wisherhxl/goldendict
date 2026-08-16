// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_SRC_FORMATS_EPWING_EPWING_READER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_EPWING_EPWING_READER_H_
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "../../dictionary/generated_index.h"
#include "../../dictionary/ordered_headword_index.h"

namespace goldendict::core::formats::epwing {
enum class ErrorCode { kMissingFile, kInvalidDictionary };

class Error final : public std::runtime_error {
   public:
    Error(ErrorCode code, std::filesystem::path path, std::string message);

    ErrorCode code() const noexcept { return code_; }

   private:
    ErrorCode code_;
};

struct Metadata {
    std::string name;
    std::string source_language = "ja";
    std::string description;
};

struct Article {
    std::string headword;
    std::string data;
};

struct PhysicalIdentity {
    std::size_t text_file_ordinal = 0;
    std::uint32_t page = 0;
    std::uint16_t offset = 0;

    friend bool operator==(const PhysicalIdentity& left,
                           const PhysicalIdentity& right) {
        return left.text_file_ordinal == right.text_file_ordinal &&
               left.page == right.page && left.offset == right.offset;
    }
};

struct IngestionRecord {
    std::size_t record_ordinal = 0;
    std::string headword;
    PhysicalIdentity physical;
    std::size_t article_ordinal = 0;
};

struct IngestionArticle {
    std::string headword;
    std::vector<std::string> aliases;
    std::string html;
    std::size_t first_record_ordinal = 0;
    std::size_t article_ordinal = 0;
    PhysicalIdentity physical;
};

struct IngestionView {
    std::vector<IngestionRecord> records;
    std::vector<IngestionArticle> articles;
    dictionary::SourceSnapshot source_snapshot;
};

class Reader final {
   public:
    static Reader Open(const std::filesystem::path& catalog_path,
                       const std::function<void()>& checkpoint = {});

    const Metadata& metadata() const noexcept { return metadata_; }

    std::size_t headword_count() const noexcept { return records_.size(); }

    std::size_t article_count() const noexcept {
        return ingestion_view_.articles.size();
    }

    const IngestionView& ingestion_view() const noexcept {
        return ingestion_view_;
    }

    const std::filesystem::path& catalog_path() const noexcept { return path_; }

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
        std::size_t article_ordinal = 0;
    };

    std::vector<const Record*> Ranked(
        std::string_view prefix, const std::function<void()>& checkpoint) const;
    std::filesystem::path path_;
    Metadata metadata_;
    std::vector<Record> records_;
    IngestionView ingestion_view_;
    dictionary::OrderedHeadwordIndex enumeration_index_;
    std::unordered_map<std::string, std::string> resources_;
};
}  // namespace goldendict::core::formats::epwing
#endif
