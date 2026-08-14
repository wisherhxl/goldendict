// SPDX-License-Identifier: GPL-3.0-or-later
#include "slob_reader.h"
#include <bzlib.h>
#include <zlib.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>
#include "../../foundation/text_encoding.h"
#include "../../foundation/text_folding.h"
#include "../../foundation/utf8.h"

namespace goldendict::core::formats::slob {
namespace {
constexpr std::string_view kMagic("\x21\x2d\x31SLOB\x1f", 8U);
constexpr std::size_t kMaxFile = 512U * 1024U * 1024U;
constexpr std::size_t kMaxItem = 64U * 1024U * 1024U;
constexpr std::size_t kMaxEntries = 10U * 1000U * 1000U;

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::uint16_t Be16(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 2U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated SLOB integer");
    return (static_cast<std::uint16_t>(static_cast<unsigned char>(data[at]))
            << 8U) |
           static_cast<unsigned char>(data[at + 1U]);
}

std::uint32_t Be32(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 4U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated SLOB integer");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4U; ++i)
        value = (value << 8U) | static_cast<unsigned char>(data[at + i]);
    return value;
}

std::uint64_t Be64(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 8U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated SLOB integer");
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8U; ++i)
        value = (value << 8U) | static_cast<unsigned char>(data[at + i]);
    return value;
}

std::string Load(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect SLOB file");
    if (size > kMaxFile)
        Throw(ErrorCode::kInvalidDictionary, path, "SLOB exceeds size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        Throw(ErrorCode::kMissingFile, path, "Cannot open SLOB file");
    std::string data(size, '\0');
    if (size && !input.read(data.data(), static_cast<std::streamsize>(size)))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete SLOB file");
    return data;
}

std::string Decode(std::string_view value, std::string_view encoding,
                   const std::filesystem::path& path) {
    try {
        return foundation::DecodeToUtf8(value, encoding, kMaxItem);
    } catch (const foundation::TextEncodingError& error) {
        Throw(ErrorCode::kInvalidDictionary, path,
              std::string("Cannot decode SLOB text: ") + error.what());
    }
}

std::string Text(std::string_view data, std::size_t* at, std::size_t width,
                 std::string_view encoding, const std::filesystem::path& path) {
    std::size_t length;
    if (width == 1U) {
        if (*at >= data.size())
            Throw(ErrorCode::kInvalidDictionary, path, "Truncated SLOB text");
        length = static_cast<unsigned char>(data[(*at)++]);
    } else {
        length = width == 2U ? Be16(data, *at, path) : Be32(data, *at, path);
        *at += width;
    }
    if (*at > data.size() || length > data.size() - *at)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated SLOB text");
    auto value = Decode(data.substr(*at, length), encoding, path);
    *at += length;
    const auto zero = value.find('\0');
    if (zero != std::string::npos)
        value.resize(zero);
    return value;
}

std::string Inflate(std::string_view input, bool bzip) {
    for (std::size_t size = 64U * 1024U; size <= kMaxItem;
         size = std::min(kMaxItem, size * 2U)) {
        std::string output(size, '\0');
        if (bzip) {
            unsigned int length = output.size();
            const int result = BZ2_bzBuffToBuffDecompress(
                output.data(), &length, const_cast<char*>(input.data()),
                input.size(), 0, 0);
            if (result == BZ_OK) {
                output.resize(length);
                return output;
            }
            if (result != BZ_OUTBUFF_FULL || size == kMaxItem)
                break;
        } else {
            uLongf length = output.size();
            const int result = uncompress(
                reinterpret_cast<Bytef*>(output.data()), &length,
                reinterpret_cast<const Bytef*>(input.data()), input.size());
            if (result == Z_OK) {
                output.resize(length);
                return output;
            }
            if (result != Z_BUF_ERROR || size == kMaxItem)
                break;
        }
    }
    return {};
}

bool Prefix(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           value.substr(0U, prefix.size()) == prefix;
}

std::size_t Points(std::string_view value) {
    return std::count_if(value.begin(), value.end(),
                         [](unsigned char c) { return (c & 0xc0U) != 0x80U; });
}

std::string EscapeHtml(std::string_view value) {
    std::string output;
    for (const char c : value) {
        if (c == '&')
            output += "&amp;";
        else if (c == '<')
            output += "&lt;";
        else if (c == '>')
            output += "&gt;";
        else
            output.push_back(c);
    }
    return output;
}

std::string LowerAscii(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

struct Reference {
    std::string key;
    std::uint32_t item;
    std::uint16_t bin;
};
}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message) + ": " + path.string()),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const std::filesystem::path& path) {
    Reader reader;
    reader.path_ = path;
    const std::string file = Load(path);
    if (file.size() < 24U || std::string_view(file).substr(0U, 8U) != kMagic)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid SLOB signature");
    std::size_t at = 24U;
    const std::string encoding = Text(file, &at, 1U, "UTF-8", path);
    const std::string compression =
        LowerAscii(Text(file, &at, 1U, "UTF-8", path));
    if (compression != "none" && !compression.empty() &&
        compression != "zlib" && compression != "bz2")
        Throw(ErrorCode::kInvalidDictionary, path,
              "Unsupported SLOB compression");
    if (at >= file.size())
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated SLOB tags");
    const auto tags = static_cast<unsigned char>(file[at++]);
    for (unsigned i = 0; i < tags; ++i) {
        const auto key = LowerAscii(Text(file, &at, 1U, encoding, path));
        const auto value = Text(file, &at, 1U, encoding, path);
        if (key == "label" || key == "name")
            reader.metadata_.name = value;
        else if (key == "description")
            reader.metadata_.description = value;
        else if (key == "source_language")
            reader.metadata_.source_language = value;
        else if (key == "target_language")
            reader.metadata_.target_language = value;
    }
    if (at >= file.size())
        Throw(ErrorCode::kInvalidDictionary, path,
              "Truncated SLOB content types");
    const auto type_count = static_cast<unsigned char>(file[at++]);
    std::vector<std::string> types;
    for (unsigned i = 0; i < type_count; ++i)
        types.push_back(LowerAscii(Text(file, &at, 2U, encoding, path)));
    static_cast<void>(Be32(file, at, path));
    at += 4U;
    const auto store = Be64(file, at, path);
    at += 8U;
    const auto declared_size = Be64(file, at, path);
    at += 8U;
    const std::size_t reference_count = Be32(file, at, path);
    at += 4U;
    if (!reference_count || reference_count > kMaxEntries ||
        declared_size > file.size() || store >= file.size() ||
        reference_count > (file.size() - at) / 8U)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid SLOB bounds");
    const auto reference_base = at + reference_count * 8U;
    std::vector<Reference> references;
    for (std::size_t i = 0; i < reference_count; ++i) {
        const auto offset = Be64(file, at + i * 8U, path);
        if (offset > file.size() - reference_base)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid SLOB reference offset");
        std::size_t position = reference_base + offset;
        Reference reference;
        reference.key = Text(file, &position, 2U, encoding, path);
        reference.item = Be32(file, position, path);
        position += 4U;
        reference.bin = Be16(file, position, path);
        position += 2U;
        static_cast<void>(Text(file, &position, 1U, encoding, path));
        if (reference.key.empty())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Empty SLOB reference key");
        references.push_back(std::move(reference));
    }
    const std::size_t item_count = Be32(file, store, path);
    const std::size_t item_offsets = store + 4U;
    const std::size_t item_base = item_offsets + item_count * 8U;
    if (!item_count || item_count > kMaxEntries || item_base > file.size())
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid SLOB item table");
    std::map<std::size_t, std::vector<std::string>> item_cache;
    const auto bins = [&](std::size_t item) -> const std::vector<std::string>& {
        auto found = item_cache.find(item);
        if (found != item_cache.end())
            return found->second;
        if (item >= item_count)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid SLOB item number");
        const auto relative = Be64(file, item_offsets + item * 8U, path);
        if (relative > file.size() - item_base)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid SLOB item offset");
        std::size_t position = item_base + relative;
        const std::size_t count = Be32(file, position, path);
        position += 4U;
        if (!count || count > kMaxEntries || count > file.size() - position)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid SLOB bin count");
        position += count;
        const std::size_t length = Be32(file, position, path);
        position += 4U;
        if (length > file.size() - position)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Truncated SLOB item data");
        std::string data;
        const auto input = std::string_view(file).substr(position, length);
        if (compression.empty() || compression == "none")
            data = std::string(input);
        else
            data = Inflate(input, compression == "bz2");
        if (data.empty() || count > data.size() / 4U)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Cannot decode SLOB item data");
        std::vector<std::string> result;
        for (std::size_t i = 0; i < count; ++i) {
            const auto offset = Be32(data, i * 4U, path);
            const auto value_at = count * 4U + offset;
            const auto value_size = Be32(data, value_at, path);
            if (value_at + 4U > data.size() ||
                value_size > data.size() - value_at - 4U)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid SLOB bin bounds");
            result.emplace_back(data.substr(value_at + 4U, value_size));
        }
        return item_cache.emplace(item, std::move(result)).first->second;
    };
    std::map<std::pair<std::uint32_t, std::uint16_t>, std::size_t> articles;
    for (const auto& reference : references) {
        const auto& values = bins(reference.item);
        if (reference.bin >= values.size())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid SLOB bin number");
        std::size_t position =
            item_base + Be64(file, item_offsets + reference.item * 8U, path) +
            4U;
        const auto type =
            static_cast<unsigned char>(file[position + reference.bin]);
        if (type >= types.size())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid SLOB content type");
        const auto& value = values[reference.bin];
        if (types[type].rfind("text/html", 0U) == 0U ||
            types[type].rfind("text/plain", 0U) == 0U) {
            const auto decoded = Decode(value, encoding, path);
            const auto key = std::make_pair(reference.item, reference.bin);
            auto [entry, inserted] =
                articles.emplace(key, reader.articles_.size());
            if (inserted)
                reader.articles_.push_back(
                    types[type].rfind("text/plain", 0U) == 0U
                        ? "<pre>" + EscapeHtml(decoded) + "</pre>"
                        : decoded);
            reader.records_.push_back({reference.key,
                                       foundation::FoldForLookup(reference.key),
                                       entry->second});
        } else {
            reader.resources_.emplace(reference.key, value);
        }
    }
    if (reader.records_.empty())
        Throw(ErrorCode::kInvalidDictionary, path, "SLOB contains no articles");
    if (reader.metadata_.name.empty())
        reader.metadata_.name = path.stem().string();
    return reader;
}

std::vector<const Reader::Record*> Reader::Ranked(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const auto folded = foundation::FoldForLookup(prefix);
    std::vector<const Record*> result;
    std::size_t n = 0;
    for (const auto& record : records_) {
        if (checkpoint && n++ % 1024U == 0U)
            checkpoint();
        if (Prefix(record.folded, folded))
            result.push_back(&record);
    }
    std::stable_sort(result.begin(), result.end(),
                     [&folded](const Record* a, const Record* b) {
                         const bool exact_a = a->folded == folded,
                                    exact_b = b->folded == folded;
                         if (exact_a != exact_b)
                             return exact_a;
                         const auto points_a = Points(a->folded),
                                    points_b = Points(b->folded);
                         if (points_a != points_b)
                             return points_a < points_b;
                         return a->folded == b->folded ? a->word < b->word
                                                       : a->folded < b->folded;
                     });
    return result;
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
    const auto folded = foundation::FoldForLookup(word);
    std::set<std::size_t> seen;
    std::size_t n = 0;
    for (const auto& record : records_) {
        if (checkpoint && n++ % 1024U == 0U)
            checkpoint();
        if (result.size() == limit)
            break;
        if (record.folded == folded && seen.insert(record.article).second)
            result.push_back({record.word, articles_[record.article]});
    }
    return result;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    std::set<std::size_t> seen;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        if (result.size() == limit)
            break;
        if (seen.insert(record->article).second)
            result.push_back({record->word, articles_[record->article]});
    }
    return result;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        if (result.size() == limit)
            break;
        if (seen.insert(record->folded).second)
            result.push_back(record->word);
    }
    return result;
}

const std::string* Reader::Resource(std::string_view id) const {
    const auto found = resources_.find(std::string(id));
    return found == resources_.end() ? nullptr : &found->second;
}
}  // namespace goldendict::core::formats::slob
