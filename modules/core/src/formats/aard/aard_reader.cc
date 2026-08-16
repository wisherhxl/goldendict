// SPDX-License-Identifier: GPL-3.0-or-later
#include "aard_reader.h"

#include <bzlib.h>
#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>

#include "../../foundation/text_folding.h"
#include "../../foundation/utf8.h"

namespace goldendict::core::formats::aard {
namespace {
constexpr std::size_t kHeaderSize = 86U;
constexpr std::size_t kMaxFileBytes = 512U * 1024U * 1024U;
constexpr std::size_t kMaxMetadataBytes = 4U * 1024U * 1024U;
constexpr std::size_t kMaxCompressedArticleBytes = 1024U * 1024U;
constexpr std::size_t kMaxArticleBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaxRecords = 10U * 1000U * 1000U;

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::uint16_t Be16(std::string_view data, std::size_t offset,
                   const std::filesystem::path& path) {
    if (offset > data.size() || data.size() - offset < 2U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated Aard integer");
    return (static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset]))
            << 8U) |
           static_cast<unsigned char>(data[offset + 1U]);
}

std::uint32_t Be32(std::string_view data, std::size_t offset,
                   const std::filesystem::path& path) {
    if (offset > data.size() || data.size() - offset < 4U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated Aard integer");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4U; ++i)
        value = (value << 8U) | static_cast<unsigned char>(data[offset + i]);
    return value;
}

std::uint64_t Be64(std::string_view data, std::size_t offset,
                   const std::filesystem::path& path) {
    if (offset > data.size() || data.size() - offset < 8U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated Aard integer");
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8U; ++i)
        value = (value << 8U) | static_cast<unsigned char>(data[offset + i]);
    return value;
}

std::string LoadFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect Aard file");
    if (size > kMaxFileBytes)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Aard file exceeds size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        Throw(ErrorCode::kMissingFile, path, "Cannot open Aard file");
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size())))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete Aard file");
    return data;
}

std::string TryBzip2(std::string_view input, std::size_t limit) {
    for (std::size_t size = std::min<std::size_t>(64U * 1024U, limit);
         size <= limit && size != 0U; size = std::min(limit, size * 2U)) {
        std::string output(size, '\0');
        unsigned int length = static_cast<unsigned int>(output.size());
        const int result = BZ2_bzBuffToBuffDecompress(
            output.data(), &length, const_cast<char*>(input.data()),
            static_cast<unsigned int>(input.size()), 0, 0);
        if (result == BZ_OK) {
            output.resize(length);
            return output;
        }
        if (result != BZ_OUTBUFF_FULL || size == limit)
            break;
    }
    return {};
}

std::string TryZlib(std::string_view input, std::size_t limit) {
    for (std::size_t size = std::min<std::size_t>(64U * 1024U, limit);
         size <= limit && size != 0U; size = std::min(limit, size * 2U)) {
        std::string output(size, '\0');
        uLongf length = output.size();
        const int result = uncompress(
            reinterpret_cast<Bytef*>(output.data()), &length,
            reinterpret_cast<const Bytef*>(input.data()), input.size());
        if (result == Z_OK) {
            output.resize(length);
            return output;
        }
        if (result != Z_BUF_ERROR || size == limit)
            break;
    }
    return {};
}

std::string Decompress(std::string_view input, std::size_t limit, bool raw_ok,
                       const std::filesystem::path& path,
                       std::string_view kind) {
    if (input.empty())
        Throw(ErrorCode::kInvalidDictionary, path,
              "Empty compressed Aard " + std::string(kind));
    if (auto output = TryBzip2(input, limit); !output.empty())
        return output;
    if (auto output = TryZlib(input, limit); !output.empty())
        return output;
    if (raw_ok && input.size() <= limit)
        return std::string(input);
    Throw(ErrorCode::kInvalidDictionary, path,
          "Cannot decompress Aard " + std::string(kind));
}

void AppendUtf8(std::uint32_t point, std::string* output) {
    if (point <= 0x7fU)
        output->push_back(static_cast<char>(point));
    else if (point <= 0x7ffU) {
        output->push_back(static_cast<char>(0xc0U | (point >> 6U)));
        output->push_back(static_cast<char>(0x80U | (point & 0x3fU)));
    } else if (point <= 0xffffU) {
        output->push_back(static_cast<char>(0xe0U | (point >> 12U)));
        output->push_back(static_cast<char>(0x80U | ((point >> 6U) & 0x3fU)));
        output->push_back(static_cast<char>(0x80U | (point & 0x3fU)));
    } else {
        output->push_back(static_cast<char>(0xf0U | (point >> 18U)));
        output->push_back(static_cast<char>(0x80U | ((point >> 12U) & 0x3fU)));
        output->push_back(static_cast<char>(0x80U | ((point >> 6U) & 0x3fU)));
        output->push_back(static_cast<char>(0x80U | (point & 0x3fU)));
    }
}

int Hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool JsonString(std::string_view json, std::size_t* position,
                std::string* output) {
    if (*position >= json.size() || json[*position] != '"')
        return false;
    ++*position;
    output->clear();
    while (*position < json.size()) {
        const unsigned char c = json[(*position)++];
        if (c == '"')
            return true;
        if (c < 0x20U)
            return false;
        if (c != '\\') {
            output->push_back(static_cast<char>(c));
            continue;
        }
        if (*position >= json.size())
            return false;
        const char escaped = json[(*position)++];
        if (escaped == '"' || escaped == '\\' || escaped == '/')
            output->push_back(escaped);
        else if (escaped == 'b')
            output->push_back('\b');
        else if (escaped == 'f')
            output->push_back('\f');
        else if (escaped == 'n')
            output->push_back('\n');
        else if (escaped == 'r')
            output->push_back('\r');
        else if (escaped == 't')
            output->push_back('\t');
        else if (escaped == 'u') {
            if (json.size() - *position < 4U)
                return false;
            std::uint32_t point = 0;
            for (int i = 0; i < 4; ++i) {
                const int digit = Hex(json[(*position)++]);
                if (digit < 0)
                    return false;
                point = (point << 4U) | static_cast<unsigned>(digit);
            }
            if (point >= 0xd800U && point <= 0xdbffU) {
                if (json.size() - *position < 6U || json[*position] != '\\' ||
                    json[*position + 1U] != 'u')
                    return false;
                *position += 2U;
                std::uint32_t low = 0;
                for (int i = 0; i < 4; ++i) {
                    const int digit = Hex(json[(*position)++]);
                    if (digit < 0)
                        return false;
                    low = (low << 4U) | static_cast<unsigned>(digit);
                }
                if (low < 0xdc00U || low > 0xdfffU)
                    return false;
                point = 0x10000U + ((point - 0xd800U) << 10U) + (low - 0xdc00U);
            } else if (point >= 0xdc00U && point <= 0xdfffU)
                return false;
            AppendUtf8(point, output);
        } else
            return false;
    }
    return false;
}

void SkipSpace(std::string_view json, std::size_t* position) {
    while (*position < json.size() &&
           std::isspace(static_cast<unsigned char>(json[*position])) != 0)
        ++*position;
}

std::map<std::string, std::string> MetadataStrings(
    std::string_view json, const std::filesystem::path& path) {
    std::map<std::string, std::string> values;
    std::size_t position = 0;
    SkipSpace(json, &position);
    if (position >= json.size() || json[position++] != '{')
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid Aard metadata JSON");
    while (position < json.size()) {
        SkipSpace(json, &position);
        if (position < json.size() && json[position] == '}')
            break;
        std::string key, value;
        if (!JsonString(json, &position, &key))
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid Aard metadata key");
        SkipSpace(json, &position);
        if (position >= json.size() || json[position++] != ':')
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid Aard metadata separator");
        SkipSpace(json, &position);
        if (position < json.size() && json[position] == '"') {
            if (!JsonString(json, &position, &value))
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid Aard metadata value");
            if (!foundation::IsValidUtf8(key) ||
                !foundation::IsValidUtf8(value))
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid UTF-8 in Aard metadata");
            values.emplace(std::move(key), std::move(value));
        } else {
            int depth = 0;
            bool quoted = false, escaped = false;
            while (position < json.size()) {
                const char c = json[position];
                if (quoted) {
                    escaped = !escaped && c == '\\';
                    if (!escaped && c == '"')
                        quoted = false;
                    else if (c != '\\')
                        escaped = false;
                } else if (c == '"')
                    quoted = true;
                else if (c == '{' || c == '[')
                    ++depth;
                else if (c == '}' || c == ']') {
                    if (!depth)
                        break;
                    --depth;
                } else if (c == ',' && !depth)
                    break;
                ++position;
            }
        }
        SkipSpace(json, &position);
        if (position < json.size() && json[position] == ',')
            ++position;
    }
    return values;
}

std::string Escape(std::string_view value) {
    std::string output;
    for (char c : value) {
        if (c == '&')
            output += "&amp;";
        else if (c == '<')
            output += "&lt;";
        else if (c == '>')
            output += "&gt;";
        else if (c == '"')
            output += "&quot;";
        else
            output.push_back(c);
    }
    return output;
}

void ReplaceAll(std::string* value, std::string_view from,
                std::string_view to) {
    for (std::size_t position = 0;
         (position = value->find(from, position)) != std::string::npos;
         position += to.size())
        value->replace(position, from.size(), to);
}

std::string ParseArticle(std::string_view json,
                         const std::filesystem::path& path) {
    std::size_t position = 0;
    SkipSpace(json, &position);
    if (position >= json.size() || json[position++] != '[')
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid Aard article JSON");
    SkipSpace(json, &position);
    std::string article;
    if (position < json.size() && json[position] == '"') {
        if (!JsonString(json, &position, &article))
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid Aard article text");
    }
    if (article.empty()) {
        std::size_t search = position;
        while ((search = json.find("\"r\"", search)) !=
               std::string_view::npos) {
            search += 3U;
            SkipSpace(json, &search);
            if (search >= json.size() || json[search++] != ':')
                continue;
            SkipSpace(json, &search);
            std::string target;
            if (JsonString(json, &search, &target) && !target.empty()) {
                article = "<a href=\"bword://" + Escape(target) + "\">" +
                          Escape(target) + "</a>";
                break;
            }
        }
    }
    if (article.empty())
        Throw(ErrorCode::kInvalidDictionary, path, "Empty Aard article");
    if (!foundation::IsValidUtf8(article))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid UTF-8 in Aard article");
    ReplaceAll(&article, "href=\"w:", "href=\"bword://");
    ReplaceAll(&article, "href=\"s:", "href=\"bword://");
    ReplaceAll(&article, "href='w:", "href='bword://");
    ReplaceAll(&article, "href='s:", "href='bword://");
    return article;
}

bool Prefix(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

std::size_t Points(std::string_view value) {
    return static_cast<std::size_t>(
        std::count_if(value.begin(), value.end(),
                      [](unsigned char c) { return (c & 0xc0U) != 0x80U; }));
}
}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message) + ": " + path.string()),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const std::filesystem::path& path) {
    Reader reader;
    reader.path_ = path;
    const std::string file = LoadFile(path);
    if (file.size() < kHeaderSize || file.compare(0, 4U, "aard") != 0)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid Aard signature");
    const bool index64 = file.compare(78U, 3U, ">LQ") == 0 && file[81U] == '\0';
    const bool index32 = file.compare(78U, 3U, ">LL") == 0 && file[81U] == '\0';
    if (!index64 && !index32)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Unsupported Aard index format");
    if (file.compare(82U, 2U, ">H") != 0 || file.compare(84U, 2U, ">L") != 0)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Unsupported Aard length format");
    const std::size_t metadata_size = Be32(file, 66U, path);
    const std::size_t word_count = Be32(file, 70U, path);
    const std::size_t articles_base = Be32(file, 74U, path);
    if (!metadata_size || metadata_size > kMaxMetadataBytes ||
        word_count > kMaxRecords)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid Aard header limits");
    if (kHeaderSize > file.size() || metadata_size > file.size() - kHeaderSize)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated Aard metadata");
    const std::string metadata_json =
        Decompress(std::string_view(file).substr(kHeaderSize, metadata_size),
                   kMaxMetadataBytes, false, path, "metadata");
    const auto metadata = MetadataStrings(metadata_json, path);
    const auto find = [&metadata](std::string_view key) {
        const auto value = metadata.find(std::string(key));
        return value == metadata.end() ? std::string{} : value->second;
    };
    reader.metadata_.name = find("title");
    reader.metadata_.source_language = find("index_language");
    reader.metadata_.target_language = find("article_language");
    reader.metadata_.description = find("description");
    if (reader.metadata_.name.empty())
        reader.metadata_.name = path.stem().string();

    const std::size_t index_size = index64 ? 12U : 8U;
    const std::size_t index_base = kHeaderSize + metadata_size;
    if (word_count > (file.size() - index_base) / index_size)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated Aard index");
    const std::size_t words_base = index_base + word_count * index_size;
    if (articles_base < words_base || articles_base > file.size())
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid Aard article offset");
    std::map<std::size_t, std::size_t> article_indices;
    for (std::size_t i = 0; i < word_count; ++i) {
        const std::size_t item = index_base + i * index_size;
        const std::size_t word_offset = Be32(file, item, path);
        const std::uint64_t relative_article =
            index64 ? Be64(file, item + 4U, path) : Be32(file, item + 4U, path);
        if (word_offset > articles_base - words_base)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid Aard word offset");
        const std::size_t word_position = words_base + word_offset;
        const std::size_t word_size = Be16(file, word_position, path);
        if (word_position + 2U > articles_base ||
            word_size > articles_base - word_position - 2U)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Truncated Aard headword");
        const std::string word = file.substr(word_position + 2U, word_size);
        if (word.empty() || !foundation::IsValidUtf8(word))
            Throw(ErrorCode::kInvalidDictionary, path, "Invalid Aard headword");
        if (relative_article > file.size() ||
            articles_base > file.size() - relative_article)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid Aard article address");
        const std::size_t article_position = articles_base + relative_article;
        auto [it, inserted] =
            article_indices.emplace(article_position, reader.articles_.size());
        if (inserted) {
            const std::size_t compressed_size =
                Be32(file, article_position, path);
            if (!compressed_size ||
                compressed_size > kMaxCompressedArticleBytes ||
                article_position + 4U > file.size() ||
                compressed_size > file.size() - article_position - 4U)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid Aard article length");
            const std::string json =
                Decompress(std::string_view(file).substr(article_position + 4U,
                                                         compressed_size),
                           kMaxArticleBytes, true, path, "article");
            reader.articles_.push_back(ParseArticle(json, path));
        }
        try {
            reader.records_.push_back(
                {word, foundation::FoldForLookup(word), it->second});
        } catch (const foundation::TextFoldingError& error) {
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid Aard headword: " + std::string(error.what()));
        }
    }
    if (reader.records_.empty())
        Throw(ErrorCode::kInvalidDictionary, path, "Aard contains no entries");
    reader.metadata_.article_count = reader.articles_.size();
    try {
        reader.source_snapshot_ = dictionary::CaptureSourceSnapshot({path});
    } catch (const dictionary::GeneratedIndexError& error) {
        Throw(ErrorCode::kMissingFile, path, error.what());
    }
    return reader;
}

std::vector<FullTextArticle> Reader::ReadFullTextArticles(
    const std::function<void()>& checkpoint) const {
    std::vector<FullTextArticle> result;
    result.reserve(articles_.size());
    std::vector<bool> seen(articles_.size(), false);
    for (std::size_t ordinal = 0U; ordinal < records_.size(); ++ordinal) {
        if (checkpoint)
            checkpoint();
        const auto& record = records_[ordinal];
        if (seen[record.article])
            continue;
        seen[record.article] = true;
        result.push_back(
            {ordinal, record.word, record.article, articles_[record.article]});
    }
    return result;
}

std::vector<const Reader::Record*> Reader::Ranked(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(prefix);
    std::vector<const Record*> matches;
    std::size_t n = 0;
    for (const auto& record : records_) {
        if (checkpoint && n++ % 1024U == 0U)
            checkpoint();
        if (Prefix(record.folded, folded))
            matches.push_back(&record);
    }
    std::stable_sort(
        matches.begin(), matches.end(),
        [&folded](const Record* a, const Record* b) {
            const bool ae = a->folded == folded, be = b->folded == folded;
            if (ae != be)
                return ae;
            const auto as = Points(a->folded), bs = Points(b->folded);
            if (as != bs)
                return as < bs;
            return a->folded == b->folded ? a->word < b->word
                                          : a->folded < b->folded;
        });
    return matches;
}

std::pair<std::vector<std::string>, bool> Reader::EnumerateHeadwords(
    std::size_t offset, std::size_t result_limit, std::size_t byte_limit,
    const std::function<void()>& checkpoint) const {
    return enumeration_index_.Page(
        records_.size(),
        [this](std::uint32_t ordinal) -> std::string_view {
            return records_[ordinal].word;
        },
        offset, result_limit, byte_limit, checkpoint);
}

std::vector<Article> Reader::LookupExact(
    std::string_view word, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> output;
    if (!limit)
        return output;
    const std::string folded = foundation::FoldForLookup(word);
    std::set<std::size_t> seen;
    std::size_t n = 0;
    for (const auto& record : records_) {
        if (checkpoint && n++ % 1024U == 0U)
            checkpoint();
        if (record.folded == folded && seen.insert(record.article).second) {
            output.push_back({record.word, articles_[record.article]});
            if (output.size() == limit)
                break;
        }
    }
    return output;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> output;
    if (!limit)
        return output;
    std::set<std::size_t> seen;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        if (seen.insert(record->article).second) {
            output.push_back({record->word, articles_[record->article]});
            if (output.size() == limit)
                break;
        }
    }
    return output;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> output;
    if (!limit)
        return output;
    std::unordered_set<std::string> seen;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        if (seen.insert(record->folded).second) {
            output.push_back(record->word);
            if (output.size() == limit)
                break;
        }
    }
    return output;
}
}  // namespace goldendict::core::formats::aard
