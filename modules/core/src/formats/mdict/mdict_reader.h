// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_READER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_READER_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mdict_discovery.h"

namespace goldendict::core::formats::mdict {

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
    const std::string* Resource(std::string_view id) const;

   private:
    struct Record {
        std::string word;
        std::string folded;
        std::size_t article = 0;
    };

    std::vector<const Record*> Ranked(
        std::string_view prefix, const std::function<void()>& checkpoint) const;
    const std::string& ResolveArticle(std::size_t article) const;

    std::filesystem::path path_;
    Metadata metadata_;
    std::vector<Record> records_;
    std::vector<std::string> articles_;
    std::unordered_map<std::string, std::size_t> article_by_folded_word_;
    std::unordered_map<std::string, std::string> resources_;
};

}  // namespace goldendict::core::formats::mdict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_READER_H_
