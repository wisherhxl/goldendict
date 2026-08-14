// SPDX-License-Identifier: GPL-3.0-or-later

#include "sdict_reader.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <regex>
#include <set>
#include <unordered_set>
#include <utility>

#include <bzlib.h>
#include <zlib.h>

#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::sdict {
namespace {

constexpr std::size_t kHeaderSize = 43U;
constexpr std::size_t kIndexElementSize = 8U;
constexpr std::size_t kMaximumFileSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumHeadwordSize = 16U * 1024U;
constexpr std::size_t kMaximumMetadataSize = 1024U * 1024U;
constexpr std::size_t kMaximumArticleSize = 16U * 1024U * 1024U;
constexpr std::uint32_t kMaximumWordCount = 10U * 1000U * 1000U;

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::uint16_t ReadU16(std::string_view data, std::size_t offset,
                      const std::filesystem::path& path) {
    if (offset > data.size() || data.size() - offset < 2U) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Truncated SDict integer field");
    }
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(data.data() + offset);
    return static_cast<std::uint16_t>(bytes[0]) |
           (static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t ReadU32(std::string_view data, std::size_t offset,
                      const std::filesystem::path& path) {
    if (offset > data.size() || data.size() - offset < 4U) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Truncated SDict integer field");
    }
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(data.data() + offset);
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::string ReadFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect SDict file");
    }
    if (size > kMaximumFileSize) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "SDict file exceeds the supported size limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Throw(ErrorCode::kMissingFile, path, "Cannot open SDict file");
    }
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete SDict file");
    }
    return data;
}

std::string Inflate(std::string_view compressed, std::size_t limit,
                    const std::filesystem::path& path) {
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<char*>(compressed.data()));  // zlib's API predates const.
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit(&stream) != Z_OK) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot initialize SDict zlib decompression");
    }
    std::string output;
    std::array<char, 64U * 1024U> buffer{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = buffer.size() - stream.avail_out;
        if (produced > limit - output.size()) {
            inflateEnd(&stream);
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Decompressed SDict field exceeds the supported size limit");
        }
        output.append(buffer.data(), produced);
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.avail_in != 0U) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid SDict zlib-compressed field");
    }
    return output;
}

std::string Bunzip(std::string_view compressed, std::size_t limit,
                   const std::filesystem::path& path) {
    std::size_t capacity = std::min<std::size_t>(64U * 1024U, limit);
    while (capacity <= limit) {
        std::string output(capacity, '\0');
        unsigned int output_size = static_cast<unsigned int>(output.size());
        const int status = BZ2_bzBuffToBuffDecompress(
            output.data(), &output_size, const_cast<char*>(compressed.data()),
            static_cast<unsigned int>(compressed.size()), 0, 0);
        if (status == BZ_OK) {
            output.resize(output_size);
            return output;
        }
        if (status != BZ_OUTBUFF_FULL || capacity == limit) {
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid SDict bzip2-compressed field");
        }
        capacity = std::min(limit, capacity * 2U);
    }
    Throw(ErrorCode::kInvalidDictionary, path,
          "Decompressed SDict field exceeds the supported size limit");
}

std::string Decompress(std::string_view data, std::uint8_t compression,
                       std::size_t limit, const std::filesystem::path& path) {
    if (compression == 0U) {
        if (data.size() > limit) {
            Throw(ErrorCode::kInvalidDictionary, path,
                  "SDict field exceeds the supported size limit");
        }
        return std::string(data);
    }
    if (compression == 1U) {
        return Inflate(data, limit, path);
    }
    if (compression == 2U) {
        return Bunzip(data, limit, path);
    }
    Throw(ErrorCode::kUnsupportedFeature, path,
          "Unsupported SDict compression method");
}

std::string ReadSizedField(std::string_view data, std::uint32_t offset,
                           std::uint8_t compression, std::size_t limit,
                           const std::filesystem::path& path) {
    const std::uint32_t size = ReadU32(data, offset, path);
    const std::uint64_t begin = static_cast<std::uint64_t>(offset) + 4U;
    const std::uint64_t end = begin + size;
    if (end > data.size()) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "SDict length-prefixed field exceeds the file");
    }
    return Decompress(data.substr(static_cast<std::size_t>(begin), size),
                      compression, limit, path);
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

std::string EscapeAttribute(std::string_view value) {
    std::string escaped;
    for (const char character : value) {
        switch (character) {
            case '&':
                escaped += "&amp;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            default:
                escaped.push_back(character);
        }
    }
    return escaped;
}

std::string ConvertMarkup(std::string article) {
    static const std::regex reference(R"(<\s*r\s*>([^<]*)<\s*/\s*r\s*>)",
                                      std::regex_constants::icase);
    std::string linked;
    std::size_t position = 0;
    for (std::sregex_iterator
             iterator(article.begin(), article.end(), reference),
         end;
         iterator != end; ++iterator) {
        const auto& match = *iterator;
        linked.append(article, position,
                      static_cast<std::size_t>(match.position()) - position);
        const std::string label = match[1].str();
        const std::string escaped = EscapeAttribute(label);
        linked += "<a href=\"bword://" + escaped + "\">" + escaped + "</a>";
        position = static_cast<std::size_t>(match.position() + match.length());
    }
    linked.append(article, position, std::string::npos);
    const std::array<std::pair<std::regex, std::string>, 8> replacements = {{
        {std::regex(R"(<\s*(p|br)\s*>)", std::regex_constants::icase), "<br>"},
        {std::regex(R"(<\s*/\s*p\s*>)", std::regex_constants::icase), ""},
        {std::regex(R"(<\s*t\s*>)", std::regex_constants::icase), "<span>"},
        {std::regex(R"(<\s*f\s*>)", std::regex_constants::icase), "<span>"},
        {std::regex(R"(<\s*/\s*(t|f)\s*>)", std::regex_constants::icase),
         "</span>"},
        {std::regex(R"(<\s*l\s*>)", std::regex_constants::icase), "<ul>"},
        {std::regex(R"(<\s*/\s*l\s*>)", std::regex_constants::icase), "</ul>"},
        {std::regex("\\n"), "<br>"},
    }};
    for (const auto& replacement : replacements) {
        linked =
            std::regex_replace(linked, replacement.first, replacement.second);
    }
    return linked;
}

std::string Language(std::string_view field) {
    const auto end = field.find('\0');
    return std::string(field.substr(0, end));
}

}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message)),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const std::filesystem::path& dictionary_path) {
    Reader reader;
    reader.dictionary_path_ = dictionary_path;
    reader.file_data_ = ReadFile(dictionary_path);
    if (reader.file_data_.size() < kHeaderSize ||
        std::memcmp(reader.file_data_.data(), "sdct", 4U) != 0) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "Not an SDict dictionary file");
    }
    const std::string_view data = reader.file_data_;
    reader.metadata_.source_language = Language(data.substr(4U, 3U));
    reader.metadata_.target_language = Language(data.substr(7U, 3U));
    reader.compression_ = static_cast<std::uint8_t>(data[10U]) & 0x0fU;
    if (reader.compression_ > 2U) {
        Throw(ErrorCode::kUnsupportedFeature, dictionary_path,
              "Unsupported SDict compression method");
    }
    const auto word_count = ReadU32(data, 11U, dictionary_path);
    const auto title_offset = ReadU32(data, 19U, dictionary_path);
    const auto copyright_offset = ReadU32(data, 23U, dictionary_path);
    const auto version_offset = ReadU32(data, 27U, dictionary_path);
    const auto full_index_offset = ReadU32(data, 35U, dictionary_path);
    const auto articles_offset = ReadU32(data, 39U, dictionary_path);
    if (word_count > kMaximumWordCount || full_index_offset > data.size() ||
        articles_offset > data.size()) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "Invalid SDict header range or word count");
    }
    reader.metadata_.name =
        ReadSizedField(data, title_offset, reader.compression_,
                       kMaximumMetadataSize, dictionary_path);
    if (reader.metadata_.name.empty()) {
        reader.metadata_.name = dictionary_path.stem().string();
    }
    reader.metadata_.description = "Title: " + reader.metadata_.name;
    const auto append_metadata = [&](std::string_view label,
                                     std::uint32_t offset) {
        if (offset == 0U)
            return;
        try {
            const std::string value =
                ReadSizedField(data, offset, reader.compression_,
                               kMaximumMetadataSize, dictionary_path);
            if (!value.empty())
                reader.metadata_.description +=
                    "\n\n" + std::string(label) + value;
        } catch (const Error&) {}
    };
    append_metadata("Copyright: ", copyright_offset);
    append_metadata("Version: ", version_offset);

    std::uint64_t position = full_index_offset;
    reader.records_.reserve(word_count);
    for (std::uint32_t number = 0; number < word_count; ++number) {
        if (position > data.size() ||
            data.size() - static_cast<std::size_t>(position) <
                kIndexElementSize) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "Truncated SDict full index");
        }
        const auto next_word =
            ReadU16(data, static_cast<std::size_t>(position), dictionary_path);
        const auto relative_article = ReadU32(
            data, static_cast<std::size_t>(position) + 4U, dictionary_path);
        if (next_word < kIndexElementSize ||
            next_word - kIndexElementSize > kMaximumHeadwordSize ||
            position + next_word > data.size()) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "Invalid SDict full-index entry size");
        }
        Record record;
        record.headword = std::string(
            data.substr(static_cast<std::size_t>(position) + kIndexElementSize,
                        next_word - kIndexElementSize));
        if (record.headword.empty()) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "Empty SDict headword");
        }
        try {
            record.folded_headword = foundation::FoldForLookup(record.headword);
        } catch (const foundation::TextFoldingError& error) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "Invalid UTF-8 SDict headword: " + std::string(error.what()));
        }
        const std::uint64_t article_offset =
            static_cast<std::uint64_t>(articles_offset) + relative_article;
        if (article_offset > data.size() || data.size() - article_offset < 4U ||
            article_offset > std::numeric_limits<std::uint32_t>::max()) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "SDict article offset exceeds the file");
        }
        const auto stored_size = ReadU32(
            data, static_cast<std::size_t>(article_offset), dictionary_path);
        if (static_cast<std::uint64_t>(stored_size) + article_offset + 4U >
            data.size()) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "SDict article data exceeds the file");
        }
        record.article_offset = static_cast<std::uint32_t>(article_offset);
        reader.records_.push_back(std::move(record));
        position += next_word;
    }
    return reader;
}

Article Reader::LoadArticle(const Record& record) const {
    auto article =
        ReadSizedField(file_data_, record.article_offset, compression_,
                       kMaximumArticleSize, dictionary_path_);
    return {record.headword, ConvertMarkup(std::move(article))};
}

std::pair<std::vector<std::string>, bool> Reader::EnumerateHeadwords(
    std::size_t offset, std::size_t result_limit, std::size_t byte_limit,
    const std::function<void()>& checkpoint) const {
    return enumeration_index_.Page(
        records_.size(),
        [this](std::uint32_t ordinal) -> std::string_view {
            return records_[ordinal].headword;
        },
        offset, result_limit, byte_limit, checkpoint);
}

std::vector<Article> Reader::LookupExact(
    std::string_view headword, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> articles;
    const auto folded = foundation::FoldForLookup(headword);
    if (folded.empty() || result_limit == 0U) {
        return articles;
    }
    std::set<std::uint32_t> seen;
    std::size_t number = 0;
    for (const auto& record : records_) {
        if (checkpoint && (number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (record.folded_headword == folded &&
            seen.insert(record.article_offset).second) {
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
    std::set<std::uint32_t> seen;
    for (const auto* record : matches) {
        if (seen.insert(record->article_offset).second) {
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
    const auto folded = foundation::FoldForLookup(prefix);
    if (folded.empty()) {
        return {};
    }
    std::vector<const Record*> matches;
    std::size_t number = 0;
    for (const auto& record : records_) {
        if (checkpoint && (number++ % 1024U) == 0U) {
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

}  // namespace goldendict::core::formats::sdict
