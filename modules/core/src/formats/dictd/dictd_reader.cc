// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictd_reader.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

#include <zlib.h>

#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::dictd {
namespace {

constexpr std::uintmax_t kMaximumIndexSize = 256U * 1024U * 1024U;
constexpr std::size_t kMaximumLineSize = 16U * 1024U;
constexpr std::size_t kMaximumDictionarySize = 512U * 1024U * 1024U;
constexpr std::string_view kBase64Digits =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::string ReadFile(const std::filesystem::path& path,
                     std::size_t maximum_size) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect dictionary data");
    }
    if (size > maximum_size) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Dictionary data exceeds the supported size limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Throw(ErrorCode::kMissingFile, path, "Cannot open dictionary data");
    }
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete dictionary data");
    }
    return data;
}

std::string ReadCompressedFile(const std::filesystem::path& path) {
    gzFile input = gzopen(path.string().c_str(), "rb");
    if (input == nullptr) {
        Throw(ErrorCode::kMissingFile, path,
              "Cannot open compressed dictionary data");
    }
    std::string data;
    std::array<char, 64U * 1024U> buffer{};
    while (true) {
        const int count =
            gzread(input, buffer.data(), static_cast<unsigned>(buffer.size()));
        if (count > 0) {
            const auto size = static_cast<std::size_t>(count);
            if (size > kMaximumDictionarySize - data.size()) {
                gzclose(input);
                Throw(
                    ErrorCode::kInvalidDictionary, path,
                    "Decompressed dictionary exceeds the supported size limit");
            }
            data.append(buffer.data(), size);
            continue;
        }
        if (count == 0) {
            break;
        }
        int error_number = Z_OK;
        const char* message = gzerror(input, &error_number);
        const std::string detail = message == nullptr ? "zlib error" : message;
        gzclose(input);
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot decompress dictionary data: " + detail);
    }
    if (gzclose(input) != Z_OK) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Compressed dictionary checksum is invalid");
    }
    return data;
}

std::uint32_t DecodeBase64(std::string_view encoded,
                           const std::filesystem::path& path,
                           std::size_t line_number) {
    if (encoded.empty()) {
        Throw(ErrorCode::kInvalidIndex, path,
              "Empty base-64 field at line " + std::to_string(line_number));
    }
    std::uint64_t value = 0;
    for (const char character : encoded) {
        const auto position = kBase64Digits.find(character);
        if (position == std::string_view::npos) {
            Throw(
                ErrorCode::kInvalidIndex, path,
                "Invalid base-64 field at line " + std::to_string(line_number));
        }
        value = value * 64U + position;
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            Throw(ErrorCode::kInvalidIndex, path,
                  "Base-64 field overflows at line " +
                      std::to_string(line_number));
        }
    }
    return static_cast<std::uint32_t>(value);
}

std::vector<std::string_view> SplitFields(const std::string& line) {
    std::vector<std::string_view> fields;
    std::size_t begin = 0;
    while (true) {
        const auto tab = line.find('\t', begin);
        fields.emplace_back(
            line.data() + begin,
            (tab == std::string::npos ? line.size() : tab) - begin);
        if (tab == std::string::npos) {
            return fields;
        }
        begin = tab + 1U;
    }
}

bool HasPrefix(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() &&
           text.compare(0, prefix.size(), prefix) == 0;
}

std::size_t Utf8CodePointCount(std::string_view text) noexcept {
    return static_cast<std::size_t>(
        std::count_if(text.begin(), text.end(), [](char byte) {
            return (static_cast<unsigned char>(byte) & 0xc0U) != 0x80U;
        }));
}

std::string TrimTitle(std::string article) {
    if (article.rfind("00databaseshort", 0) == 0U ||
        article.rfind("00-database-short", 0) == 0U) {
        const auto newline = article.find('\n');
        article = newline == std::string::npos ? std::string{}
                                               : article.substr(newline + 1U);
    }
    const auto first = article.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto end = article.find_first_of("\r\n", first);
    return article.substr(first, end == std::string::npos ? end : end - first);
}

}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message)),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const std::filesystem::path& index_path) {
    std::error_code filesystem_error;
    const auto index_size =
        std::filesystem::file_size(index_path, filesystem_error);
    if (filesystem_error) {
        Throw(ErrorCode::kMissingFile, index_path,
              "Cannot inspect Dictd index");
    }
    if (index_size > kMaximumIndexSize) {
        Throw(ErrorCode::kInvalidIndex, index_path,
              "Dictd index exceeds the supported size limit");
    }
    auto base = index_path;
    base.replace_extension();
    const auto plain_path = std::filesystem::path(base.string() + ".dict");
    const auto compressed_path =
        std::filesystem::path(base.string() + ".dict.dz");
    Reader reader;
    reader.index_path_ = index_path;
    reader.name_ = base.filename().string();
    if (std::filesystem::is_regular_file(plain_path, filesystem_error) &&
        !filesystem_error) {
        reader.dictionary_data_ = ReadFile(plain_path, kMaximumDictionarySize);
    } else {
        filesystem_error.clear();
        if (!std::filesystem::is_regular_file(compressed_path,
                                              filesystem_error) ||
            filesystem_error) {
            Throw(ErrorCode::kMissingFile, index_path,
                  "Dictd dictionary data companion is missing");
        }
        reader.dictionary_data_ = ReadCompressedFile(compressed_path);
    }

    std::ifstream input(index_path, std::ios::binary);
    if (!input) {
        Throw(ErrorCode::kMissingFile, index_path, "Cannot open Dictd index");
    }
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.size() > kMaximumLineSize) {
            Throw(ErrorCode::kInvalidIndex, index_path,
                  "Dictd index line exceeds the supported size limit");
        }
        const auto fields = SplitFields(line);
        if (fields.size() != 3U && fields.size() != 4U) {
            Throw(ErrorCode::kInvalidIndex, index_path,
                  "Malformed Dictd index line " + std::to_string(line_number));
        }
        const auto offset = DecodeBase64(fields[1], index_path, line_number);
        const auto size = DecodeBase64(fields[2], index_path, line_number);
        const auto end = static_cast<std::uint64_t>(offset) + size;
        if (end > reader.dictionary_data_.size()) {
            Throw(ErrorCode::kInvalidDictionary, index_path,
                  "Dictd article range exceeds dictionary data at line " +
                      std::to_string(line_number));
        }
        const auto add_record = [&](std::string_view headword) {
            if (headword.empty()) {
                Throw(ErrorCode::kInvalidIndex, index_path,
                      "Empty Dictd headword at line " +
                          std::to_string(line_number));
            }
            Record record;
            record.headword = std::string(headword);
            try {
                record.folded_headword =
                    foundation::FoldForLookup(record.headword);
            } catch (const foundation::TextFoldingError& error) {
                Throw(ErrorCode::kInvalidIndex, index_path,
                      "Invalid UTF-8 Dictd headword at line " +
                          std::to_string(line_number) + ": " + error.what());
            }
            record.article_offset = offset;
            record.article_size = size;
            reader.records_.push_back(std::move(record));
            ++reader.headword_count_;
        };
        add_record(fields[0]);
        if (fields.size() == 4U && !fields[3].empty() &&
            fields[3] != fields[0]) {
            add_record(fields[3]);
        }
        ++reader.article_count_;
    }
    if (!input.eof()) {
        Throw(ErrorCode::kInvalidIndex, index_path,
              "Cannot read complete Dictd index");
    }

    for (const auto& record : reader.records_) {
        if (record.headword == "00databaseshort" ||
            record.headword == "00-database-short") {
            const auto title = TrimTitle(reader.LoadArticle(record).data);
            if (!title.empty()) {
                reader.name_ = title;
            }
            break;
        }
    }
    for (const auto& record : reader.records_) {
        if (record.headword == "00databaseinfo" ||
            record.headword == "00-database-info") {
            reader.description_ = reader.LoadArticle(record).data;
            break;
        }
    }
    return reader;
}

Article Reader::LoadArticle(const Record& record) const {
    return {record.headword, dictionary_data_.substr(record.article_offset,
                                                     record.article_size)};
}

std::vector<Article> Reader::LookupExact(
    std::string_view headword, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> articles;
    const std::string folded = foundation::FoldForLookup(headword);
    if (folded.empty() || result_limit == 0U) {
        return articles;
    }
    std::set<std::pair<std::uint32_t, std::uint32_t>> seen;
    std::size_t record_number = 0;
    for (const auto& record : records_) {
        if (checkpoint && (record_number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (record.folded_headword == folded &&
            seen.insert({record.article_offset, record.article_size}).second) {
            articles.push_back(LoadArticle(record));
            if (articles.size() == result_limit) {
                break;
            }
        }
    }
    return articles;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> articles;
    if (result_limit == 0U) {
        return articles;
    }
    const auto matches = RankedPrefixMatches(prefix, checkpoint);
    std::set<std::pair<std::uint32_t, std::uint32_t>> seen;
    for (const auto* record : matches) {
        if (seen.insert({record->article_offset, record->article_size})
                .second) {
            articles.push_back(LoadArticle(*record));
            if (articles.size() == result_limit) {
                break;
            }
        }
    }
    return articles;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> suggestions;
    if (result_limit == 0U) {
        return suggestions;
    }
    const auto matches = RankedPrefixMatches(prefix, checkpoint);
    std::unordered_set<std::string> seen;
    for (const auto* record : matches) {
        if (seen.insert(record->headword).second) {
            suggestions.push_back(record->headword);
            if (suggestions.size() == result_limit) {
                break;
            }
        }
    }
    return suggestions;
}

std::vector<const Reader::Record*> Reader::RankedPrefixMatches(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(prefix);
    if (folded.empty()) {
        return {};
    }
    std::vector<const Record*> matches;
    std::size_t record_number = 0;
    for (const auto& record : records_) {
        if (checkpoint && (record_number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (HasPrefix(record.folded_headword, folded)) {
            matches.push_back(&record);
        }
    }
    std::stable_sort(
        matches.begin(), matches.end(),
        [&folded](const auto* left, const auto* right) {
            const bool left_exact = left->folded_headword == folded;
            const bool right_exact = right->folded_headword == folded;
            if (left_exact != right_exact) {
                return left_exact;
            }
            const auto left_length = Utf8CodePointCount(left->folded_headword);
            const auto right_length =
                Utf8CodePointCount(right->folded_headword);
            if (left_length != right_length) {
                return left_length < right_length;
            }
            if (left->folded_headword != right->folded_headword) {
                return left->folded_headword < right->folded_headword;
            }
            return left->headword < right->headword;
        });
    return matches;
}

}  // namespace goldendict::core::formats::dictd
