// SPDX-License-Identifier: GPL-3.0-or-later
#include "zipsounds_reader.h"
#include <zlib.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <tuple>
#include <utility>
#include "../../audio/audio_file_types.h"
#include "../../foundation/text_encoding.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::zipsounds {
namespace {
constexpr std::size_t kMaximumFileSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumEntries = 100000U;
constexpr std::size_t kMaximumNameBytes = 64U * 1024U;
constexpr std::size_t kMaximumResourceBytes = 256U * 1024U * 1024U;
constexpr std::uint32_t kEocdSignature = 0x06054b50U;
constexpr std::uint32_t kCentralSignature = 0x02014b50U;
constexpr std::uint32_t kLocalSignature = 0x04034b50U;

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::uint16_t Le16(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 2U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated ZIP integer");
    return static_cast<std::uint16_t>(
        static_cast<unsigned char>(data[at]) |
        (static_cast<std::uint16_t>(static_cast<unsigned char>(data[at + 1U]))
         << 8U));
}

std::uint32_t Le32(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 4U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated ZIP integer");
    std::uint32_t value = 0U;
    for (unsigned i = 0; i < 4U; ++i)
        value |=
            static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + i]))
            << (i * 8U);
    return value;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect ZIP sound pack");
    if (size > kMaximumFileSize)
        Throw(ErrorCode::kInvalidDictionary, path,
              "ZIP sound pack exceeds the supported size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        Throw(ErrorCode::kMissingFile, path, "Cannot open ZIP sound pack");
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size())))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete ZIP sound pack");
    return data;
}

std::string DecodeName(std::string_view bytes, bool utf8,
                       const std::filesystem::path& path) {
    try {
        if (utf8)
            return foundation::DecodeToUtf8(bytes, "UTF-8", kMaximumNameBytes);
        try {
            return foundation::DecodeToUtf8(bytes, "UTF-8", kMaximumNameBytes);
        } catch (const foundation::TextEncodingError&) {
            return foundation::DecodeToUtf8(bytes, "IBM437", kMaximumNameBytes);
        }
    } catch (const foundation::TextEncodingError&) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid ZIP member name encoding");
    }
}

bool SafeResourceId(std::string_view value) {
    if (value.empty() || value.front() == '/' || value.back() == '/' ||
        value.find('\\') != std::string_view::npos ||
        value.find('\0') != std::string_view::npos)
        return false;
    if (std::any_of(value.begin(), value.end(),
                    [](unsigned char c) { return c < 0x20U || c == 0x7fU; }))
        return false;
    std::size_t start = 0U;
    while (start <= value.size()) {
        const auto end = value.find('/', start);
        const auto part = value.substr(start, end == std::string_view::npos
                                                  ? value.size() - start
                                                  : end - start);
        if (part.empty() || part == "." || part == "..")
            return false;
        if (end == std::string_view::npos)
            break;
        start = end + 1U;
    }
    return true;
}

std::string Headword(std::string_view resource_id) {
    std::size_t end = resource_id.size();
    while (end > 0U &&
           std::isspace(static_cast<unsigned char>(resource_id[end - 1U])) != 0)
        --end;
    const auto dot = resource_id.substr(0U, end).find_last_of('.');
    std::string word(resource_id.substr(0U, dot));
    while (!word.empty() &&
           std::isspace(static_cast<unsigned char>(word.back())) != 0)
        word.pop_back();
    return word;
}

std::string Escape(std::string_view value) {
    std::string result;
    for (const char c : value) {
        switch (c) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\'':
                result += "&#39;";
                break;
            default:
                result.push_back(c);
        }
    }
    return result;
}
}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(path.string() + ": " + std::move(message)),
      code_(code) {}

Reader Reader::Open(const std::filesystem::path& path) {
    Reader reader;
    reader.path_ = path;
    reader.file_ = ReadFile(path);
    if (reader.file_.size() < 22U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated ZIP sound pack");
    const std::size_t search_start =
        reader.file_.size() > 65557U ? reader.file_.size() - 65557U : 0U;
    std::size_t eocd = std::string::npos;
    for (std::size_t at = reader.file_.size() - 22U;; --at) {
        if (Le32(reader.file_, at, path) == kEocdSignature &&
            at + 22U + Le16(reader.file_, at + 20U, path) ==
                reader.file_.size()) {
            eocd = at;
            break;
        }
        if (at == search_start)
            break;
    }
    if (eocd == std::string::npos)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Missing ZIP end-of-central-directory record");
    const auto disk = Le16(reader.file_, eocd + 4U, path);
    const auto central_disk = Le16(reader.file_, eocd + 6U, path);
    const auto disk_entries = Le16(reader.file_, eocd + 8U, path);
    const auto entry_count = Le16(reader.file_, eocd + 10U, path);
    const auto central_size = Le32(reader.file_, eocd + 12U, path);
    const auto central_offset = Le32(reader.file_, eocd + 16U, path);
    if (disk != 0U || central_disk != 0U || disk_entries != entry_count ||
        entry_count == 0xffffU || central_size == 0xffffffffU ||
        central_offset == 0xffffffffU)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Split and ZIP64 sound packs are not supported");
    if (entry_count == 0U || entry_count > kMaximumEntries ||
        central_offset > eocd || central_size > eocd - central_offset)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid ZIP central directory bounds");
    std::size_t cursor = central_offset;
    reader.records_.reserve(entry_count);
    for (std::uint32_t index = 0U; index < entry_count; ++index) {
        if (Le32(reader.file_, cursor, path) != kCentralSignature)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid ZIP central directory entry");
        const auto flags = Le16(reader.file_, cursor + 8U, path);
        const auto method = Le16(reader.file_, cursor + 10U, path);
        const auto crc = Le32(reader.file_, cursor + 16U, path);
        const auto compressed_size = Le32(reader.file_, cursor + 20U, path);
        const auto uncompressed_size = Le32(reader.file_, cursor + 24U, path);
        const auto name_size = Le16(reader.file_, cursor + 28U, path);
        const auto extra_size = Le16(reader.file_, cursor + 30U, path);
        const auto comment_size = Le16(reader.file_, cursor + 32U, path);
        const auto local_offset = Le32(reader.file_, cursor + 42U, path);
        if (name_size == 0U || name_size > kMaximumNameBytes || cursor > eocd ||
            eocd - cursor < 46U + name_size + extra_size + comment_size)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid ZIP member metadata bounds");
        const auto raw_name =
            std::string_view(reader.file_).substr(cursor + 46U, name_size);
        cursor += 46U + name_size + extra_size + comment_size;
        if ((flags & 1U) != 0U || (method != 0U && method != 8U) ||
            compressed_size == 0xffffffffU ||
            uncompressed_size == 0xffffffffU ||
            uncompressed_size > kMaximumResourceBytes)
            continue;
        std::string resource_id =
            DecodeName(raw_name, (flags & 0x800U) != 0U, path);
        std::replace(resource_id.begin(), resource_id.end(), '\\', '/');
        if (!SafeResourceId(resource_id) ||
            !audio::IsSupportedAudioFile(resource_id))
            continue;
        std::string word = Headword(resource_id);
        if (word.empty())
            continue;
        reader.records_.push_back({std::move(word),
                                   {},
                                   std::move(resource_id),
                                   {},
                                   method,
                                   crc,
                                   compressed_size,
                                   uncompressed_size,
                                   local_offset});
        auto& record = reader.records_.back();
        record.folded = foundation::FoldForLookup(record.word);
        record.media_type = audio::MediaTypeForAudioFile(record.resource_id);
    }
    if (cursor != central_offset + central_size || reader.records_.empty())
        Throw(ErrorCode::kInvalidDictionary, path,
              "ZIP sound pack contains no supported audio entries");
    reader.metadata_.name = path.stem().string();
    std::stable_sort(
        reader.records_.begin(), reader.records_.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.folded, left.word, left.resource_id) <
                   std::tie(right.folded, right.word, right.resource_id);
        });
    return reader;
}

std::vector<const Reader::Record*> Reader::Ranked(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(prefix);
    std::vector<const Record*> result;
    for (const auto& record : records_) {
        if (checkpoint)
            checkpoint();
        if (record.folded.rfind(folded, 0U) == 0U)
            result.push_back(&record);
    }
    return result;
}

std::vector<Article> Reader::LookupExact(
    std::string_view word, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    const auto folded = foundation::FoldForLookup(word);
    std::vector<Article> result;
    for (const auto* record : Ranked(word, checkpoint)) {
        if (record->folded != folded)
            continue;
        result.push_back(
            {record->word,
             "<div class=\"zipsounds_article\"><audio controls=\"controls\">"
             "<source src=\"" +
                 Escape(record->resource_id) + "\" type=\"" +
                 record->media_type + "\"></audio><span>" +
                 Escape(record->resource_id) + "</span></div>"});
        if (result.size() == limit)
            break;
    }
    return result;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        result.push_back(
            {record->word,
             "<div class=\"zipsounds_article\"><audio controls=\"controls\">"
             "<source src=\"" +
                 Escape(record->resource_id) + "\" type=\"" +
                 record->media_type + "\"></audio><span>" +
                 Escape(record->resource_id) + "</span></div>"});
        if (result.size() == limit)
            break;
    }
    return result;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        if (seen.insert(record->folded).second)
            result.push_back(record->word);
        if (result.size() == limit)
            break;
    }
    return result;
}

std::string Reader::Resource(std::string_view id) const {
    const auto found = std::find_if(
        records_.begin(), records_.end(),
        [id](const auto& record) { return record.resource_id == id; });
    if (found == records_.end())
        return {};
    const auto local = static_cast<std::size_t>(found->local_offset);
    if (Le32(file_, local, path_) != kLocalSignature)
        Throw(ErrorCode::kInvalidDictionary, path_,
              "Invalid ZIP local member header");
    const auto local_flags = Le16(file_, local + 6U, path_);
    const auto local_method = Le16(file_, local + 8U, path_);
    const auto name_size = Le16(file_, local + 26U, path_);
    const auto extra_size = Le16(file_, local + 28U, path_);
    if ((local_flags & 1U) != 0U || local_method != found->method ||
        local > file_.size() ||
        file_.size() - local < 30U + name_size + extra_size)
        Throw(ErrorCode::kInvalidDictionary, path_,
              "Invalid ZIP local member metadata");
    const auto data_offset = local + 30U + name_size + extra_size;
    if (data_offset > file_.size() ||
        found->compressed_size > file_.size() - data_offset)
        Throw(ErrorCode::kInvalidDictionary, path_,
              "ZIP audio member is out of bounds");
    const auto compressed =
        std::string_view(file_).substr(data_offset, found->compressed_size);
    std::string output;
    if (found->method == 0U) {
        if (found->compressed_size != found->uncompressed_size)
            Throw(ErrorCode::kInvalidDictionary, path_,
                  "Stored ZIP audio sizes disagree");
        output.assign(compressed);
    } else {
        output.resize(found->uncompressed_size);
        z_stream stream{};
        stream.next_in =
            reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
        stream.avail_in = static_cast<uInt>(compressed.size());
        stream.next_out = reinterpret_cast<Bytef*>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
            Throw(ErrorCode::kInvalidDictionary, path_,
                  "Cannot initialize ZIP inflater");
        const int status = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);
        if (status != Z_STREAM_END || stream.total_out != output.size() ||
            stream.total_in != compressed.size())
            Throw(ErrorCode::kInvalidDictionary, path_,
                  "Invalid deflated ZIP audio member");
    }
    const auto crc = crc32(0U, reinterpret_cast<const Bytef*>(output.data()),
                           static_cast<uInt>(output.size()));
    if (crc != found->crc)
        Throw(ErrorCode::kInvalidDictionary, path_,
              "ZIP audio member checksum mismatch");
    return output;
}
}  // namespace goldendict::core::formats::zipsounds
