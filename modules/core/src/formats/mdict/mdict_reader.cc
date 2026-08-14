// SPDX-License-Identifier: GPL-3.0-or-later

#include "mdict_reader.h"

#include <expat.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>

#include "../../foundation/text_encoding.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::mdict {
namespace {

constexpr std::size_t kMaxFile = 512U * 1024U * 1024U;
constexpr std::size_t kMaxDecoded = 512U * 1024U * 1024U;
constexpr std::size_t kMaxHeader = 4U * 1024U * 1024U;
constexpr std::size_t kMaxRecord = 32U * 1024U * 1024U;
constexpr std::size_t kMaxEntries = 10U * 1000U * 1000U;
constexpr std::size_t kMaxBlocks = 1000U * 1000U;

struct Header {
    std::string title;
    std::string description;
    std::string encoding;
    std::string stylesheet;
    double version = 0.0;
    unsigned encrypted = 0;
    bool right_to_left = false;
};

struct RawEntry {
    std::string word;
    std::string value;
};

struct ParsedContainer {
    Header header;
    std::vector<RawEntry> entries;
};

struct BlockInfo {
    std::size_t compressed = 0;
    std::size_t decompressed = 0;
};

struct KeyEntry {
    std::size_t offset = 0;
    std::string word;
};

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::string ReadFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect MDict file");
    if (size > kMaxFile)
        Throw(ErrorCode::kInvalidDictionary, path,
              "MDict file exceeds size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        Throw(ErrorCode::kMissingFile, path, "Cannot open MDict file");
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size())))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete MDict file");
    return data;
}

class Cursor final {
   public:
    Cursor(std::string_view data, const std::filesystem::path& path)
        : data_(data), path_(path) {}

    std::size_t position() const noexcept { return position_; }

    std::size_t remaining() const noexcept { return data_.size() - position_; }

    std::string_view Read(std::size_t size, std::string_view field) {
        if (size > remaining())
            Throw(ErrorCode::kInvalidDictionary, path_,
                  "Truncated MDict " + std::string(field));
        const auto value = data_.substr(position_, size);
        position_ += size;
        return value;
    }

    std::uint64_t Big(std::size_t width, std::string_view field) {
        const auto bytes = Read(width, field);
        std::uint64_t value = 0;
        for (unsigned char byte : bytes)
            value = (value << 8U) | byte;
        return value;
    }

   private:
    std::string_view data_;
    const std::filesystem::path& path_;
    std::size_t position_ = 0;
};

std::uint32_t Little32(std::string_view value) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(value[0])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(value[1]))
            << 8U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(value[2]))
            << 16U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(value[3]))
            << 24U);
}

std::uint32_t Big32(std::string_view value) {
    std::uint32_t result = 0;
    for (unsigned char byte : value)
        result = (result << 8U) | byte;
    return result;
}

bool ValidAdler(std::string_view data, std::uint32_t expected) {
    uLong checksum = adler32(0L, Z_NULL, 0);
    checksum = adler32(checksum, reinterpret_cast<const Bytef*>(data.data()),
                       static_cast<uInt>(data.size()));
    return static_cast<std::uint32_t>(checksum) == expected;
}

std::string Decode(std::string_view data, std::string_view encoding,
                   const std::filesystem::path& path, std::string_view field,
                   std::size_t limit = kMaxRecord) {
    try {
        return foundation::DecodeToUtf8(data, encoding, limit);
    } catch (const foundation::TextEncodingError& error) {
        Throw(
            ErrorCode::kInvalidDictionary, path,
            "Cannot decode MDict " + std::string(field) + ": " + error.what());
    }
}

struct XmlAttributes {
    std::map<std::string, std::string> values;
    bool root_seen = false;
};

void XMLCALL StartHeader(void* context, const XML_Char*,
                         const XML_Char** attributes) {
    auto& state = *static_cast<XmlAttributes*>(context);
    if (state.root_seen)
        return;
    state.root_seen = true;
    for (std::size_t index = 0; attributes[index] != nullptr; index += 2U)
        state.values.emplace(attributes[index], attributes[index + 1]);
}

std::map<std::string, std::string> ParseHeaderXml(
    std::string_view text, const std::filesystem::path& path) {
    XmlAttributes state;
    XML_Parser parser = XML_ParserCreate("UTF-8");
    if (parser == nullptr)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot initialize MDict header parser");
    XML_SetUserData(parser, &state);
    XML_SetStartElementHandler(parser, StartHeader);
    const bool ok =
        XML_Parse(parser, text.data(), static_cast<int>(text.size()),
                  XML_TRUE) == XML_STATUS_OK;
    const std::string message =
        ok ? std::string{} : XML_ErrorString(XML_GetErrorCode(parser));
    XML_ParserFree(parser);
    if (!ok || !state.root_seen)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid MDict header XML" +
                  (message.empty() ? std::string{} : ": " + message));
    return state.values;
}

std::string Attribute(const std::map<std::string, std::string>& attributes,
                      std::string_view name) {
    const auto found = attributes.find(std::string(name));
    return found == attributes.end() ? std::string{} : found->second;
}

std::string NormalizeEncoding(std::string encoding) {
    std::transform(encoding.begin(), encoding.end(), encoding.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::toupper(value));
                   });
    if (encoding.empty() || encoding == "UTF-16")
        return "UTF-16LE";
    if (encoding == "GBK" || encoding == "GB2312")
        return "GB18030";
    return encoding;
}

Header ReadHeader(Cursor* cursor, const std::filesystem::path& path) {
    const std::size_t size =
        static_cast<std::size_t>(cursor->Big(4U, "header length"));
    if (size == 0U || size > kMaxHeader)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid MDict header length");
    const auto raw = cursor->Read(size, "header");
    const auto checksum = cursor->Read(4U, "header checksum");
    if (!ValidAdler(raw, Little32(checksum)))
        Throw(ErrorCode::kInvalidDictionary, path,
              "MDict header checksum does not match");
    std::string xml = Decode(raw, "UTF-16LE", path, "header", kMaxHeader);
    while (!xml.empty() && xml.back() == '\0')
        xml.pop_back();
    const auto attributes = ParseHeaderXml(xml, path);
    Header header;
    header.title = Attribute(attributes, "Title");
    header.description = Attribute(attributes, "Description");
    header.encoding = NormalizeEncoding(Attribute(attributes, "Encoding"));
    header.stylesheet = Attribute(attributes, "StyleSheet");
    header.version = std::strtod(
        Attribute(attributes, "GeneratedByEngineVersion").c_str(), nullptr);
    const std::string encrypted = Attribute(attributes, "Encrypted");
    header.encrypted = encrypted == "Yes"
                           ? 1U
                           : static_cast<unsigned>(
                                 std::strtoul(encrypted.c_str(), nullptr, 10));
    header.right_to_left = Attribute(attributes, "Left2Right") != "Yes";
    if (header.version < 2.0 || header.version >= 3.0)
        Throw(ErrorCode::kUnsupported, path,
              "Only MDict 2.x files are supported by this slice");
    if (header.encrypted != 0U)
        Throw(ErrorCode::kUnsupported, path,
              "Encrypted MDict files are not supported by this slice");
    return header;
}

std::string DecompressBlock(std::string_view block, std::size_t expected,
                            const std::filesystem::path& path,
                            std::string_view field) {
    if (block.size() < 8U || expected > kMaxDecoded)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid MDict " + std::string(field) + " block");
    const auto type = Big32(block.substr(0U, 4U));
    const auto checksum = Big32(block.substr(4U, 4U));
    const auto payload = block.substr(8U);
    std::string output;
    if (type == 0x00000000U) {
        if (payload.size() != expected)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "MDict plain block size does not match");
        output.assign(payload);
    } else if (type == 0x02000000U) {
        output.resize(std::max<std::size_t>(expected, 1U));
        uLongf size = static_cast<uLongf>(output.size());
        const int status =
            uncompress(reinterpret_cast<Bytef*>(output.data()), &size,
                       reinterpret_cast<const Bytef*>(payload.data()),
                       static_cast<uLong>(payload.size()));
        if (status != Z_OK || size != expected)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Cannot decompress MDict zlib block");
        output.resize(expected);
    } else if (type == 0x01000000U) {
        Throw(ErrorCode::kUnsupported, path,
              "LZO-compressed MDict blocks are not supported by this slice");
    } else {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Unknown MDict compression type");
    }
    if (!ValidAdler(output, checksum))
        Throw(ErrorCode::kInvalidDictionary, path,
              "MDict block checksum does not match");
    return output;
}

std::string DecodeSizedWord(Cursor* cursor, std::size_t characters,
                            std::string_view encoding,
                            const std::filesystem::path& path,
                            std::string_view field) {
    const bool utf16 = encoding == "UTF-16LE" || encoding == "UTF-16BE";
    const std::size_t unit = utf16 ? 2U : 1U;
    if (characters > kMaxRecord / unit)
        Throw(ErrorCode::kInvalidDictionary, path,
              "MDict key text exceeds size limit");
    const auto raw = cursor->Read(characters * unit, field);
    const auto terminator = cursor->Read(unit, "key terminator");
    if (std::any_of(terminator.begin(), terminator.end(),
                    [](char byte) { return byte != '\0'; }))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid MDict key terminator");
    return Decode(raw, encoding, path, field);
}

std::vector<BlockInfo> ParseKeyInfo(std::string_view data,
                                    std::size_t number_width,
                                    std::size_t block_count,
                                    std::size_t entry_count,
                                    std::string_view encoding,
                                    const std::filesystem::path& path) {
    Cursor cursor(data, path);
    std::vector<BlockInfo> blocks;
    std::size_t entries = 0;
    for (std::size_t index = 0; index < block_count; ++index) {
        const auto count = cursor.Big(number_width, "key block entry count");
        if (count > kMaxEntries - entries)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "MDict key count exceeds size limit");
        entries += static_cast<std::size_t>(count);
        DecodeSizedWord(&cursor, cursor.Big(2U, "first key length"), encoding,
                        path, "first key");
        DecodeSizedWord(&cursor, cursor.Big(2U, "last key length"), encoding,
                        path, "last key");
        const auto compressed = cursor.Big(number_width, "key block size");
        const auto decompressed =
            cursor.Big(number_width, "decoded key block size");
        if (compressed < 8U || compressed > kMaxFile ||
            decompressed > kMaxDecoded)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid MDict key block size");
        blocks.push_back({static_cast<std::size_t>(compressed),
                          static_cast<std::size_t>(decompressed)});
    }
    if (cursor.remaining() != 0U || entries != entry_count)
        Throw(ErrorCode::kInvalidDictionary, path,
              "MDict key block metadata does not match");
    return blocks;
}

std::vector<KeyEntry> ParseKeyBlock(std::string_view data,
                                    std::size_t number_width,
                                    std::string_view encoding,
                                    const std::filesystem::path& path) {
    Cursor cursor(data, path);
    std::vector<KeyEntry> entries;
    const bool utf16 = encoding == "UTF-16LE" || encoding == "UTF-16BE";
    const std::size_t unit = utf16 ? 2U : 1U;
    while (cursor.remaining() != 0U) {
        const auto offset = cursor.Big(number_width, "record offset");
        std::string raw;
        for (;;) {
            const auto bytes = cursor.Read(unit, "key text");
            bool end = true;
            for (char byte : bytes)
                end = end && byte == '\0';
            if (end)
                break;
            raw.append(bytes);
            if (raw.size() > kMaxRecord)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "MDict key exceeds size limit");
        }
        entries.push_back({static_cast<std::size_t>(offset),
                           Decode(raw, encoding, path, "key")});
    }
    return entries;
}

std::map<int, std::pair<std::string, std::string>> ParseStyles(
    std::string_view stylesheet) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= stylesheet.size()) {
        const auto end = stylesheet.find_first_of("\r\n", start);
        lines.emplace_back(stylesheet.substr(
            start, end == std::string_view::npos ? stylesheet.size() - start
                                                 : end - start));
        if (end == std::string_view::npos)
            break;
        start = end + 1U;
        if (stylesheet[end] == '\r' && start < stylesheet.size() &&
            stylesheet[start] == '\n')
            ++start;
    }
    std::map<int, std::pair<std::string, std::string>> styles;
    for (std::size_t index = 0; index + 2U < lines.size(); index += 3U) {
        char* end = nullptr;
        const long id = std::strtol(lines[index].c_str(), &end, 10);
        if (end != lines[index].c_str() && *end == '\0')
            styles.emplace(
                static_cast<int>(id),
                std::make_pair(lines[index + 1U], lines[index + 2U]));
    }
    return styles;
}

std::string ApplyStyles(
    std::string_view article,
    const std::map<int, std::pair<std::string, std::string>>& styles) {
    std::string result;
    std::string closing;
    std::size_t position = 0;
    while (position < article.size()) {
        if (article[position] != '`') {
            result.push_back(article[position++]);
            continue;
        }
        const std::size_t end = article.find('`', position + 1U);
        if (end == std::string_view::npos) {
            result.append(article.substr(position));
            break;
        }
        const std::string number(
            article.substr(position + 1U, end - position - 1U));
        char* number_end = nullptr;
        const long id = std::strtol(number.c_str(), &number_end, 10);
        if (number.empty() || *number_end != '\0') {
            result.append(article.substr(position, end - position + 1U));
            position = end + 1U;
            continue;
        }
        result += closing;
        const auto style = styles.find(static_cast<int>(id));
        if (style == styles.end())
            closing.clear();
        else {
            result += style->second.first;
            closing = style->second.second;
        }
        position = end + 1U;
    }
    result += closing;
    return result;
}

ParsedContainer ParseContainer(const std::filesystem::path& path,
                               bool resources) {
    const std::string file = ReadFile(path);
    Cursor cursor(file, path);
    ParsedContainer parsed;
    parsed.header = ReadHeader(&cursor, path);
    constexpr std::size_t number_width = 8U;
    const auto key_header = cursor.Read(number_width * 5U, "key header");
    Cursor key_cursor(key_header, path);
    const auto block_count = key_cursor.Big(number_width, "key block count");
    const auto entry_count = key_cursor.Big(number_width, "entry count");
    const auto decoded_info_size =
        key_cursor.Big(number_width, "decoded key info size");
    const auto info_size = key_cursor.Big(number_width, "key info size");
    const auto key_blocks_size =
        key_cursor.Big(number_width, "key blocks size");
    const auto key_header_checksum = cursor.Read(4U, "key header checksum");
    if (!ValidAdler(key_header, Big32(key_header_checksum)) ||
        block_count == 0U || block_count > kMaxBlocks || entry_count == 0U ||
        entry_count > kMaxEntries || decoded_info_size > kMaxDecoded ||
        info_size > kMaxFile || key_blocks_size > kMaxFile)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid MDict key header");
    const std::string info = DecompressBlock(
        cursor.Read(static_cast<std::size_t>(info_size), "key block info"),
        static_cast<std::size_t>(decoded_info_size), path, "key info");
    const auto blocks = ParseKeyInfo(
        info, number_width, static_cast<std::size_t>(block_count),
        static_cast<std::size_t>(entry_count), parsed.header.encoding, path);
    std::vector<KeyEntry> keys;
    std::size_t compressed_total = 0;
    for (const auto& block : blocks) {
        if (block.compressed >
            static_cast<std::size_t>(key_blocks_size) - compressed_total)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "MDict key blocks exceed declared size");
        const std::string decoded =
            DecompressBlock(cursor.Read(block.compressed, "key block"),
                            block.decompressed, path, "key");
        auto entries =
            ParseKeyBlock(decoded, number_width, parsed.header.encoding, path);
        keys.insert(keys.end(), std::make_move_iterator(entries.begin()),
                    std::make_move_iterator(entries.end()));
        compressed_total += block.compressed;
    }
    if (compressed_total != key_blocks_size || keys.size() != entry_count)
        Throw(ErrorCode::kInvalidDictionary, path,
              "MDict key data does not match metadata");

    const auto record_block_count =
        cursor.Big(number_width, "record block count");
    const auto record_count = cursor.Big(number_width, "record count");
    const auto record_info_size = cursor.Big(number_width, "record info size");
    const auto record_blocks_size =
        cursor.Big(number_width, "record blocks size");
    if (record_block_count == 0U || record_block_count > kMaxBlocks ||
        record_count != entry_count ||
        record_info_size != record_block_count * number_width * 2U ||
        record_blocks_size > kMaxFile)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid MDict record header");
    std::vector<BlockInfo> record_blocks;
    std::size_t record_decoded_total = 0;
    std::size_t record_compressed_total = 0;
    for (std::size_t index = 0; index < record_block_count; ++index) {
        const auto compressed = cursor.Big(number_width, "record block size");
        const auto decompressed =
            cursor.Big(number_width, "decoded record block size");
        if (compressed < 8U || compressed > kMaxFile ||
            decompressed > kMaxDecoded - record_decoded_total ||
            compressed > record_blocks_size - record_compressed_total)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid MDict record block size");
        record_blocks.push_back({static_cast<std::size_t>(compressed),
                                 static_cast<std::size_t>(decompressed)});
        record_decoded_total += static_cast<std::size_t>(decompressed);
        record_compressed_total += static_cast<std::size_t>(compressed);
    }
    if (record_compressed_total != record_blocks_size)
        Throw(ErrorCode::kInvalidDictionary, path,
              "MDict record sizes do not match");
    std::string records;
    records.reserve(record_decoded_total);
    for (const auto& block : record_blocks)
        records +=
            DecompressBlock(cursor.Read(block.compressed, "record block"),
                            block.decompressed, path, "record");
    if (cursor.remaining() != 0U)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Trailing data after MDict record blocks");
    const auto styles = ParseStyles(parsed.header.stylesheet);
    parsed.entries.reserve(keys.size());
    for (std::size_t index = 0; index < keys.size(); ++index) {
        const std::size_t start = keys[index].offset;
        const std::size_t end =
            index + 1U < keys.size() ? keys[index + 1U].offset : records.size();
        if (start > end || end > records.size() || end - start > kMaxRecord)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid MDict record range");
        std::string value(records.substr(start, end - start));
        if (!resources)
            value = ApplyStyles(
                Decode(value, parsed.header.encoding, path, "record"), styles);
        parsed.entries.push_back(
            {std::move(keys[index].word), std::move(value)});
    }
    return parsed;
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

std::string NormalizeResourceId(std::string id) {
    std::replace(id.begin(), id.end(), '\\', '/');
    while (!id.empty() && id.front() == '/')
        id.erase(id.begin());
    if (id.empty() || id.find('\0') != std::string::npos)
        return {};
    std::filesystem::path path(id);
    for (const auto& part : path)
        if (part == "..")
            return {};
    return path.lexically_normal().generic_string();
}

std::string Trim(std::string value) {
    const auto whitespace = [](unsigned char byte) {
        return std::isspace(byte) != 0;
    };
    value.erase(value.begin(),
                std::find_if_not(value.begin(), value.end(), whitespace));
    value.erase(
        std::find_if_not(value.rbegin(), value.rend(), whitespace).base(),
        value.end());
    return value;
}

}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message)),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const DictionaryFiles& files) {
    const ParsedContainer mdx = ParseContainer(files.mdx, false);
    Reader reader;
    reader.path_ = files.mdx;
    reader.metadata_.name =
        mdx.header.title.empty() ? files.mdx.stem().string() : mdx.header.title;
    reader.metadata_.description = mdx.header.description;
    reader.metadata_.right_to_left = mdx.header.right_to_left;
    reader.articles_.reserve(mdx.entries.size());
    reader.records_.reserve(mdx.entries.size());
    for (const auto& entry : mdx.entries) {
        if (entry.word.empty())
            continue;
        std::string folded;
        try {
            folded = foundation::FoldForLookup(entry.word);
        } catch (const foundation::TextFoldingError& error) {
            Throw(ErrorCode::kInvalidDictionary, files.mdx,
                  "Invalid MDict headword: " + std::string(error.what()));
        }
        const std::size_t article = reader.articles_.size();
        reader.articles_.push_back(entry.value);
        reader.records_.push_back({entry.word, folded, article});
        reader.article_by_folded_word_.emplace(std::move(folded), article);
    }
    if (reader.records_.empty())
        Throw(ErrorCode::kInvalidDictionary, files.mdx,
              "MDict file contains no entries");
    for (const auto& mdd_path : files.mdd) {
        const ParsedContainer mdd = ParseContainer(mdd_path, true);
        for (const auto& entry : mdd.entries) {
            std::string id = NormalizeResourceId(entry.word);
            if (!id.empty())
                reader.resources_.emplace(std::move(id), entry.value);
        }
    }
    return reader;
}

std::vector<const Reader::Record*> Reader::Ranked(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(prefix);
    std::vector<const Record*> matches;
    std::size_t checked = 0;
    for (const auto& record : records_) {
        if (checkpoint && checked++ % 1024U == 0U)
            checkpoint();
        if (Prefix(record.folded, folded))
            matches.push_back(&record);
    }
    std::stable_sort(matches.begin(), matches.end(),
                     [&folded](const Record* left, const Record* right) {
                         const bool left_exact = left->folded == folded;
                         const bool right_exact = right->folded == folded;
                         if (left_exact != right_exact)
                             return left_exact;
                         const auto left_size = Points(left->folded);
                         const auto right_size = Points(right->folded);
                         if (left_size != right_size)
                             return left_size < right_size;
                         return left->folded == right->folded
                                    ? left->word < right->word
                                    : left->folded < right->folded;
                     });
    return matches;
}

const std::string& Reader::ResolveArticle(std::size_t article) const {
    std::set<std::size_t> seen;
    while (article < articles_.size() && seen.insert(article).second &&
           articles_[article].compare(0U, 8U, "@@@LINK=") == 0) {
        const std::string target = Trim(articles_[article].substr(8U));
        const auto found =
            article_by_folded_word_.find(foundation::FoldForLookup(target));
        if (found == article_by_folded_word_.end())
            break;
        article = found->second;
    }
    return articles_[article];
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
    std::vector<Article> result;
    if (!limit)
        return result;
    const std::string folded = foundation::FoldForLookup(word);
    std::unordered_set<std::size_t> seen;
    std::size_t checked = 0;
    for (const auto& record : records_) {
        if (checkpoint && checked++ % 1024U == 0U)
            checkpoint();
        if (record.folded == folded && seen.insert(record.article).second) {
            result.push_back({record.word, ResolveArticle(record.article)});
            if (result.size() == limit)
                break;
        }
    }
    return result;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    if (!limit)
        return result;
    std::unordered_set<std::size_t> seen;
    for (const auto* record : Ranked(prefix, checkpoint))
        if (seen.insert(record->article).second) {
            result.push_back({record->word, ResolveArticle(record->article)});
            if (result.size() == limit)
                break;
        }
    return result;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> result;
    if (!limit)
        return result;
    std::unordered_set<std::string> seen;
    for (const auto* record : Ranked(prefix, checkpoint))
        if (seen.insert(record->folded).second) {
            result.push_back(record->word);
            if (result.size() == limit)
                break;
        }
    return result;
}

const std::string* Reader::Resource(std::string_view id) const {
    const std::string normalized = NormalizeResourceId(std::string(id));
    if (normalized.empty())
        return nullptr;
    const auto found = resources_.find(normalized);
    return found == resources_.end() ? nullptr : &found->second;
}

}  // namespace goldendict::core::formats::mdict
