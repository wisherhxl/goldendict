// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_READER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_READER_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "../../dictionary/generated_index.h"
#include "../../dictionary/ordered_headword_index.h"

#include "mdict_discovery.h"

namespace goldendict::core::formats::mdict {

namespace detail {
class ResourceStore;
}

enum class ErrorCode { kMissingFile, kInvalidDictionary, kUnsupported };

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
    std::string description;
    bool right_to_left = false;
};

struct Article {
    std::string headword;
    std::string data;
};

enum class ResolutionOutcome { kTerminal, kMissingTarget, kCycle };

struct TerminalIdentity {
    std::size_t terminal_key_ordinal = 0;
    std::size_t record_offset = 0;
    std::size_t record_size = 0;

    friend bool operator==(const TerminalIdentity& left,
                           const TerminalIdentity& right) {
        return left.terminal_key_ordinal == right.terminal_key_ordinal &&
               left.record_offset == right.record_offset &&
               left.record_size == right.record_size;
    }
};

struct IngestionRecord {
    std::size_t record_ordinal = 0;
    std::string headword;
    ResolutionOutcome outcome = ResolutionOutcome::kMissingTarget;
    std::optional<TerminalIdentity> terminal;
};

struct IngestionArticle {
    std::string headword;
    std::vector<std::string> aliases;
    std::string html;
    std::size_t first_record_ordinal = 0;
    std::size_t article_ordinal = 0;
    TerminalIdentity terminal;
};

struct IngestionView {
    std::vector<IngestionRecord> records;
    std::vector<IngestionArticle> articles;
    dictionary::SourceSnapshot source_snapshot;
};

class Reader final {
   public:
    static Reader Open(const DictionaryFiles& files);

    const Metadata& metadata() const noexcept { return metadata_; }

    std::size_t headword_count() const noexcept { return records_.size(); }

    std::size_t article_count() const noexcept { return articles_.size(); }

    const std::filesystem::path& dictionary_path() const noexcept {
        return path_;
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
    std::optional<std::string> Resource(std::string_view id) const;
    IngestionView ReadIngestionView(
        const std::function<void()>& checkpoint = {}) const;

   private:
    struct Record {
        std::string word;
        std::string folded;
        std::size_t article = 0;
        std::size_t record_offset = 0;
        std::size_t record_size = 0;
    };

    struct SearchKey {
        std::string folded_suffix;
        std::size_t record = 0;
    };

    std::vector<const Record*> Ranked(
        std::string_view prefix, const std::function<void()>& checkpoint) const;
    const std::string& ResolveArticle(std::size_t article) const;

    std::filesystem::path path_;
    Metadata metadata_;
    std::vector<Record> records_;
    std::vector<SearchKey> search_index_;
    dictionary::OrderedHeadwordIndex enumeration_index_;
    std::vector<std::string> articles_;
    std::unordered_map<std::string, std::size_t> article_by_folded_word_;
    std::vector<std::shared_ptr<const detail::ResourceStore>> resources_;
    dictionary::SourceSnapshot source_snapshot_;
};

}  // namespace goldendict::core::formats::mdict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_READER_H_
