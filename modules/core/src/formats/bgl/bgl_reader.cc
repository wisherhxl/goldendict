// SPDX-License-Identifier: GPL-3.0-or-later
#include "bgl_reader.h"
#include <zlib.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <set>
#include <unordered_set>
#include <utility>
#include "../../foundation/text_encoding.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::bgl {
namespace {
constexpr std::size_t kMaxFile = 512U * 1024U * 1024U;
constexpr std::size_t kMaxDecoded = 512U * 1024U * 1024U;
constexpr std::size_t kMaxArticle = 16U * 1024U * 1024U;
constexpr std::size_t kMaxResource = 16U * 1024U * 1024U;
constexpr std::size_t kMaxRecords = 10U * 1000U * 1000U;

struct Block {
    unsigned type = 0;
    std::string data;
};

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::string ReadFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect BGL file");
    if (size > kMaxFile)
        Throw(ErrorCode::kInvalidDictionary, path,
              "BGL file exceeds size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        Throw(ErrorCode::kMissingFile, path, "Cannot open BGL file");
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size())))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete BGL file");
    return data;
}

std::uint32_t Big(std::string_view data) {
    std::uint32_t value = 0;
    for (unsigned char byte : data)
        value = (value << 8U) | byte;
    return value;
}

std::string Inflate(std::string_view input, const std::filesystem::path& path) {
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot initialize BGL gzip stream");
    std::string output;
    std::array<char, 64U * 1024U> buffer{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = buffer.size() - stream.avail_out;
        if (produced > kMaxDecoded - output.size()) {
            inflateEnd(&stream);
            Throw(ErrorCode::kInvalidDictionary, path,
                  "BGL stream exceeds size limit");
        }
        output.append(buffer.data(), produced);
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.avail_in != 0U)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid BGL gzip stream");
    return output;
}

std::vector<Block> ParseBlocks(std::string_view stream,
                               const std::filesystem::path& path) {
    std::vector<Block> blocks;
    std::size_t pos = 0;
    std::size_t total = 0;
    while (pos < stream.size()) {
        const unsigned marker = static_cast<unsigned char>(stream[pos++]);
        const unsigned type = marker & 0x0fU;
        unsigned code = marker >> 4U;
        if (type == 4U)
            return blocks;
        std::size_t length = 0;
        if (code < 4U) {
            const std::size_t bytes = code + 1U;
            if (bytes > stream.size() - pos)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Truncated BGL block length");
            length = Big(stream.substr(pos, bytes));
            pos += bytes;
        } else
            length = code - 4U;
        if (length > stream.size() - pos || length > kMaxDecoded - total)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid BGL block range");
        blocks.push_back({type, std::string(stream.substr(pos, length))});
        pos += length;
        total += length;
        if (blocks.size() > kMaxRecords * 2U)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "BGL contains too many blocks");
    }
    Throw(ErrorCode::kInvalidDictionary, path, "BGL end marker is missing");
}

std::string Charset(unsigned value) {
    static constexpr std::array<std::string_view, 14> names = {
        "windows-1252", "windows-1252", "windows-1250", "windows-1251",
        "windows-31j",  "Big5",         "GB18030",      "windows-1257",
        "windows-1253", "EUC-KR",       "ISO-8859-9",   "windows-1255",
        "windows-1256", "windows-874"};
    if (value > 64U)
        value -= 65U;
    return std::string(names[value < names.size() ? value : 0U]);
}

std::string Language(unsigned value) {
    static constexpr std::array<std::string_view, 61> codes = {
        "en", "fr", "it", "es", "nl", "pt", "de", "ru", "ja", "zh", "zh", "el",
        "ko", "tr", "he", "ar", "th", "",   "zh", "zh", "",   "",   "ru", "ja",
        "",   "el", "ko", "tr", "th", "pl", "hu", "cs", "lt", "lv", "ca", "hr",
        "sr", "sk", "sq", "ur", "sl", "et", "bg", "da", "fi", "is", "no", "ro",
        "sv", "uk", "be", "fa", "eu", "mk", "af", "fo", "la", "eo", "",   "hy"};
    return value < codes.size() ? std::string(codes[value]) : std::string{};
}

std::string Decode(std::string_view value, std::string_view charset,
                   const std::filesystem::path& path, std::string_view field) {
    if (value.size() > kMaxArticle)
        Throw(ErrorCode::kInvalidDictionary, path,
              std::string(field) + " exceeds size limit");
    try {
        return foundation::DecodeToUtf8(
            value, charset.empty() ? "windows-1252" : charset, kMaxArticle);
    } catch (const foundation::TextEncodingError& error) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot decode BGL " + std::string(field) + ": " + error.what());
    }
}

std::string CleanDefinition(std::string_view raw) {
    std::string clean;
    clean.reserve(raw.size());
    for (unsigned char byte : raw) {
        if (byte == 0x1eU || byte == 0x1fU)
            continue;
        if (byte == '\n')
            clean += "<br>";
        else if (byte < 0x20U)
            clean.push_back(' ');
        else
            clean.push_back(static_cast<char>(byte));
    }
    return clean;
}

bool Prefix(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           value.compare(0, prefix.size(), prefix) == 0;
}

std::size_t Points(std::string_view value) {
    return std::count_if(value.begin(), value.end(), [](char byte) {
        return (static_cast<unsigned char>(byte) & 0xc0U) != 0x80U;
    });
}
}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message)),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const std::filesystem::path& path) {
    const std::string file = ReadFile(path);
    if (file.size() < 6U || static_cast<unsigned char>(file[0]) != 0x12U ||
        static_cast<unsigned char>(file[1]) != 0x34U || file[2] != '\0' ||
        file[3] == '\0' || static_cast<unsigned char>(file[3]) > 2U)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid BGL signature");
    const std::size_t offset = Big(std::string_view(file).substr(4U, 2U));
    if (offset < 6U || offset >= file.size())
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid BGL gzip offset");
    const auto blocks =
        ParseBlocks(Inflate(std::string_view(file).substr(offset), path), path);
    std::string default_charset = "windows-1252", source_charset,
                target_charset;
    bool utf8 = false;
    Reader reader;
    std::string author, email, copyright, description;
    reader.path_ = path;
    for (const auto& block : blocks) {
        if (block.type == 0U && block.data.size() >= 3U &&
            static_cast<unsigned char>(block.data[0]) == 8U)
            default_charset =
                Charset(static_cast<unsigned char>(block.data[2]));
        if (block.type != 3U || block.data.size() < 2U)
            continue;
        const unsigned subtype = static_cast<unsigned char>(block.data[1]);
        if (subtype == 1U)
            reader.metadata_.name = block.data.substr(2U);
        else if (subtype == 2U)
            author = block.data.substr(2U);
        else if (subtype == 3U)
            email = block.data.substr(2U);
        else if (subtype == 4U)
            copyright = block.data.substr(2U);
        else if (subtype == 7U && block.data.size() >= 6U)
            reader.metadata_.source_language =
                Language(static_cast<unsigned char>(block.data[5]));
        else if (subtype == 8U && block.data.size() >= 6U)
            reader.metadata_.target_language =
                Language(static_cast<unsigned char>(block.data[5]));
        else if (subtype == 9U)
            description = block.data.substr(2U);
        else if (subtype == 17U && block.data.size() >= 5U &&
                 (static_cast<unsigned char>(block.data[4]) & 0x80U) != 0U)
            utf8 = true;
        else if (subtype == 26U && block.data.size() >= 3U)
            source_charset = Charset(static_cast<unsigned char>(block.data[2]));
        else if (subtype == 27U && block.data.size() >= 3U)
            target_charset = Charset(static_cast<unsigned char>(block.data[2]));
    }
    if (utf8)
        default_charset = source_charset = target_charset = "UTF-8";
    if (source_charset.empty())
        source_charset = default_charset;
    if (target_charset.empty())
        target_charset = default_charset;
    if (!reader.metadata_.name.empty())
        reader.metadata_.name =
            Decode(reader.metadata_.name, target_charset, path, "title");
    const auto append = [&reader, &path, &target_charset](
                            std::string_view label, std::string value) {
        if (value.empty())
            return;
        try {
            value = Decode(value, target_charset, path, "metadata");
        } catch (const Error&) {
            return;
        }
        value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
        if (!reader.metadata_.description.empty())
            reader.metadata_.description += "\n\n";
        reader.metadata_.description += std::string(label) + value;
    };
    append("Copyright: ", std::move(copyright));
    append("Author: ", std::move(author));
    append("E-mail: ", std::move(email));
    append("", std::move(description));
    std::size_t article_bytes = 0, resource_bytes = 0;
    for (const auto& block : blocks) {
        if (block.type == 2U) {
            if (block.data.empty())
                continue;
            const std::size_t length =
                static_cast<unsigned char>(block.data[0]);
            if (length > block.data.size() - 1U ||
                block.data.size() - 1U - length > kMaxResource)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid BGL resource block");
            std::string name =
                Decode(std::string_view(block.data).substr(1U, length),
                       target_charset, path, "resource name");
            if (name.empty() || name.find('\0') != std::string::npos ||
                resource_bytes >
                    kMaxDecoded - (block.data.size() - 1U - length))
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid BGL resource");
            resource_bytes += block.data.size() - 1U - length;
            reader.resources_.emplace(std::move(name),
                                      block.data.substr(1U + length));
            continue;
        }
        if (block.type != 1U && block.type != 7U && block.type != 10U &&
            block.type != 11U)
            continue;
        std::size_t pos = block.type == 11U ? 1U : 0U;
        const std::size_t width = block.type == 11U ? 4U : 1U;
        if (width > block.data.size() - pos)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Truncated BGL headword length");
        const std::size_t head_length =
            Big(std::string_view(block.data).substr(pos, width));
        pos += width;
        if (head_length > block.data.size() - pos)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid BGL headword range");
        std::vector<std::string> words{
            Decode(std::string_view(block.data).substr(pos, head_length),
                   source_charset, path, "headword")};
        pos += head_length;
        if (block.type == 11U) {
            if (4U > block.data.size() - pos)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Truncated BGL alternate count");
            const std::size_t count =
                Big(std::string_view(block.data).substr(pos, 4U));
            pos += 4U;
            if (count > 1024U)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "BGL has too many alternates");
            for (std::size_t i = 0; i < count; ++i) {
                if (4U > block.data.size() - pos)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated BGL alternate");
                const std::size_t length =
                    Big(std::string_view(block.data).substr(pos, 4U));
                pos += 4U;
                if (length > block.data.size() - pos)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Invalid BGL alternate range");
                words.push_back(
                    Decode(std::string_view(block.data).substr(pos, length),
                           source_charset, path, "alternate"));
                pos += length;
            }
        }
        const std::size_t def_width = block.type == 11U ? 4U : 2U;
        if (def_width > block.data.size() - pos)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Truncated BGL definition length");
        const std::size_t def_length =
            Big(std::string_view(block.data).substr(pos, def_width));
        pos += def_width;
        if (def_length > block.data.size() - pos)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid BGL definition range");
        const std::string article =
            Decode(CleanDefinition(
                       std::string_view(block.data).substr(pos, def_length)),
                   target_charset, path, "definition");
        pos += def_length;
        if (article_bytes > kMaxDecoded - article.size())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "BGL articles exceed size limit");
        article_bytes += article.size();
        const std::size_t article_index = reader.articles_.size();
        reader.articles_.push_back(article);
        if (block.type != 11U)
            while (pos < block.data.size()) {
                const std::size_t length =
                    static_cast<unsigned char>(block.data[pos++]);
                if (length > block.data.size() - pos)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Invalid BGL alternate range");
                words.push_back(
                    Decode(std::string_view(block.data).substr(pos, length),
                           source_charset, path, "alternate"));
                pos += length;
            }
        for (auto& word : words) {
            if (word.empty())
                continue;
            if (reader.records_.size() == kMaxRecords)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "BGL contains too many headwords");
            try {
                reader.records_.push_back(
                    {word, foundation::FoldForLookup(word), article_index});
            } catch (const foundation::TextFoldingError& error) {
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid BGL headword: " + std::string(error.what()));
            }
        }
    }
    if (reader.records_.empty())
        Throw(ErrorCode::kInvalidDictionary, path, "BGL contains no entries");
    if (reader.metadata_.name.empty())
        reader.metadata_.name = path.stem().string();
    return reader;
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
    std::vector<Article> out;
    if (!limit)
        return out;
    const std::string folded = foundation::FoldForLookup(word);
    std::set<std::size_t> seen;
    std::size_t n = 0;
    for (const auto& record : records_) {
        if (checkpoint && n++ % 1024U == 0U)
            checkpoint();
        if (record.folded == folded && seen.insert(record.article).second) {
            out.push_back({record.word, articles_[record.article]});
            if (out.size() == limit)
                break;
        }
    }
    return out;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> out;
    if (!limit)
        return out;
    std::set<std::size_t> seen;
    for (const auto* record : Ranked(prefix, checkpoint))
        if (seen.insert(record->article).second) {
            out.push_back({record->word, articles_[record->article]});
            if (out.size() == limit)
                break;
        }
    return out;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> out;
    if (!limit)
        return out;
    std::unordered_set<std::string> seen;
    for (const auto* record : Ranked(prefix, checkpoint))
        if (seen.insert(record->folded).second) {
            out.push_back(record->word);
            if (out.size() == limit)
                break;
        }
    return out;
}

const std::string* Reader::Resource(std::string_view id) const {
    const auto found = resources_.find(std::string(id));
    return found == resources_.end() ? nullptr : &found->second;
}
}  // namespace goldendict::core::formats::bgl
