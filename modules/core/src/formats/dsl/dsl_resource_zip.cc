// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsl_resource_zip.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

#include "../../foundation/text_encoding.h"

namespace goldendict::core::formats::dsl {
namespace {

constexpr std::uint32_t kLocalSignature = 0x04034b50U;
constexpr std::uint32_t kCentralSignature = 0x02014b50U;
constexpr std::uint32_t kEocdSignature = 0x06054b50U;
constexpr std::uint32_t kZip64EocdSignature = 0x06064b50U;
constexpr std::uint32_t kZip64LocatorSignature = 0x07064b50U;
constexpr std::size_t kMaximumEntries = 500000U;
constexpr std::uint64_t kMaximumResourceSize = 16U * 1024U * 1024U;
constexpr std::uint64_t kMaximumCompressedSize = 20U * 1024U * 1024U;
constexpr std::size_t kMaximumNameSize = 64U * 1024U;
constexpr std::uint64_t kMaximumCentralSize = 256U * 1024U * 1024U;

[[noreturn]] void Throw(ResourceZipErrorCode code,
                        const std::filesystem::path& path,
                        std::string message) {
    throw ResourceZipError(code, path.string() + ": " + std::move(message));
}

std::uint16_t Le16(std::string_view data, std::size_t offset,
                   const std::filesystem::path& path) {
    if (offset > data.size() || data.size() - offset < 2U)
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Truncated ZIP integer");
    return static_cast<std::uint16_t>(
        static_cast<unsigned char>(data[offset]) |
        (static_cast<std::uint16_t>(
             static_cast<unsigned char>(data[offset + 1U]))
         << 8U));
}

std::uint32_t Le32(std::string_view data, std::size_t offset,
                   const std::filesystem::path& path) {
    if (offset > data.size() || data.size() - offset < 4U)
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Truncated ZIP integer");
    std::uint32_t value = 0U;
    for (unsigned index = 0U; index < 4U; ++index)
        value |= static_cast<std::uint32_t>(
                     static_cast<unsigned char>(data[offset + index]))
                 << (index * 8U);
    return value;
}

std::uint64_t Le64(std::string_view data, std::size_t offset,
                   const std::filesystem::path& path) {
    if (offset > data.size() || data.size() - offset < 8U)
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Truncated ZIP64 integer");
    std::uint64_t value = 0U;
    for (unsigned index = 0U; index < 8U; ++index)
        value |= static_cast<std::uint64_t>(
                     static_cast<unsigned char>(data[offset + index]))
                 << (index * 8U);
    return value;
}

std::string ReadAt(std::ifstream& input, std::uint64_t offset, std::size_t size,
                   const std::filesystem::path& path) {
    if (offset >
        static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "ZIP offset exceeds the platform limit");
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    std::string result(size, '\0');
    if (!input ||
        (size != 0U &&
         !input.read(result.data(), static_cast<std::streamsize>(size))))
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Cannot read complete ZIP metadata");
    return result;
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool SafeResourceId(std::string_view value) {
    if (value.empty() || value.front() == '/' || value.back() == '/' ||
        value.find('\0') != std::string_view::npos)
        return false;
    if (std::any_of(value.begin(), value.end(), [](unsigned char character) {
            return character < 0x20U || character == 0x7fU;
        }))
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

std::string DecodeName(std::string_view bytes, bool utf8,
                       const std::filesystem::path& path) {
    try {
        if (utf8)
            return foundation::DecodeToUtf8(bytes, "UTF-8", kMaximumNameSize);
        try {
            return foundation::DecodeToUtf8(bytes, "UTF-8", kMaximumNameSize);
        } catch (const foundation::TextEncodingError&) {
            return foundation::DecodeToUtf8(bytes, "IBM437", kMaximumNameSize);
        }
    } catch (const foundation::TextEncodingError&) {
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Invalid ZIP member name encoding");
    }
}

std::vector<std::filesystem::path> ArchiveCandidates(
    const std::filesystem::path& dictionary_path) {
    const std::string filename = dictionary_path.filename().u8string();
    const std::string lowered = Lower(filename);
    const std::size_t suffix =
        lowered.size() >= 7U &&
                lowered.compare(lowered.size() - 7U, 7U, ".dsl.dz") == 0
            ? 7U
            : 4U;
    const auto base =
        dictionary_path.parent_path() /
        std::filesystem::u8path(filename.substr(0U, filename.size() - suffix));
    std::vector<std::filesystem::path> result;
    for (const std::string_view ending :
         {".dsl.files.zip", ".dsl.dz.files.zip", ".DSL.FILES.ZIP",
          ".DSL.DZ.FILES.ZIP"})
        result.push_back(
            std::filesystem::u8path(base.u8string() + std::string(ending)));
    return result;
}

struct DirectoryLocation {
    std::uint64_t entries = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    bool exact_entries = false;
};

DirectoryLocation LocateDirectory(std::ifstream& input, std::uint64_t file_size,
                                  const std::filesystem::path& path) {
    if (file_size < 22U)
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Truncated ZIP archive");
    const auto tail_size =
        static_cast<std::size_t>(std::min<std::uint64_t>(file_size, 65557U));
    const auto tail_offset = file_size - tail_size;
    const std::string tail = ReadAt(input, tail_offset, tail_size, path);
    std::size_t eocd = std::string::npos;
    for (std::size_t offset = tail.size() - 22U;; --offset) {
        if (Le32(tail, offset, path) == kEocdSignature &&
            offset + 22U + Le16(tail, offset + 20U, path) == tail.size()) {
            eocd = offset;
            break;
        }
        if (offset == 0U)
            break;
    }
    if (eocd == std::string::npos || Le16(tail, eocd + 4U, path) != 0U ||
        Le16(tail, eocd + 6U, path) != 0U)
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Missing or split ZIP central directory");
    const auto disk_entries = Le16(tail, eocd + 8U, path);
    DirectoryLocation location{Le16(tail, eocd + 10U, path),
                               Le32(tail, eocd + 16U, path),
                               Le32(tail, eocd + 12U, path)};
    if (disk_entries != location.entries &&
        !(disk_entries == 0xffffU && location.entries == 0xffffU))
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Split ZIP central directory is not supported");
    const bool zip64 = location.entries == 0xffffU ||
                       location.offset == 0xffffffffU ||
                       location.size == 0xffffffffU;
    if (zip64) {
        const std::uint64_t eocd_absolute = tail_offset + eocd;
        if (eocd_absolute < 20U)
            Throw(ResourceZipErrorCode::kInvalidData, path,
                  "Missing ZIP64 locator");
        const auto locator = ReadAt(input, eocd_absolute - 20U, 20U, path);
        if (Le32(locator, 0U, path) != kZip64LocatorSignature ||
            Le32(locator, 4U, path) != 0U || Le32(locator, 16U, path) != 1U)
            Throw(ResourceZipErrorCode::kInvalidData, path,
                  "Invalid ZIP64 locator");
        const auto record_offset = Le64(locator, 8U, path);
        const auto record = ReadAt(input, record_offset, 56U, path);
        if (Le32(record, 0U, path) != kZip64EocdSignature ||
            Le64(record, 4U, path) < 44U || Le32(record, 16U, path) != 0U ||
            Le32(record, 20U, path) != 0U ||
            Le64(record, 24U, path) != Le64(record, 32U, path))
            Throw(ResourceZipErrorCode::kInvalidData, path,
                  "Invalid ZIP64 central directory");
        location.entries = Le64(record, 32U, path);
        location.size = Le64(record, 40U, path);
        location.offset = Le64(record, 48U, path);
        location.exact_entries = true;
    }
    if (location.entries > kMaximumEntries ||
        location.size > kMaximumCentralSize || location.offset > file_size ||
        location.size > file_size - location.offset)
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Invalid ZIP central directory bounds");
    return location;
}

void ParseZip64Extra(std::string_view extra, bool need_uncompressed,
                     bool need_compressed, bool need_offset, bool need_disk,
                     std::uint64_t* uncompressed, std::uint64_t* compressed,
                     std::uint64_t* offset, const std::filesystem::path& path) {
    std::size_t cursor = 0U;
    while (cursor + 4U <= extra.size()) {
        const auto identifier = Le16(extra, cursor, path);
        const auto size = Le16(extra, cursor + 2U, path);
        cursor += 4U;
        if (size > extra.size() - cursor)
            Throw(ResourceZipErrorCode::kInvalidData, path,
                  "Invalid ZIP extra field");
        if (identifier == 0x0001U) {
            std::size_t field = cursor;
            const auto take64 = [&](bool needed, std::uint64_t* target) {
                if (!needed)
                    return;
                if (field + 8U > cursor + size)
                    Throw(ResourceZipErrorCode::kInvalidData, path,
                          "Truncated ZIP64 member field");
                *target = Le64(extra, field, path);
                field += 8U;
            };
            take64(need_uncompressed, uncompressed);
            take64(need_compressed, compressed);
            take64(need_offset, offset);
            if (need_disk) {
                if (field + 4U > cursor + size ||
                    Le32(extra, field, path) != 0U)
                    Throw(ResourceZipErrorCode::kInvalidData, path,
                          "Split ZIP members are not supported");
            }
            return;
        }
        cursor += size;
    }
    if (need_uncompressed || need_compressed || need_offset || need_disk)
        Throw(ResourceZipErrorCode::kInvalidData, path,
              "Missing ZIP64 member field");
}

}  // namespace

ResourceZipError::ResourceZipError(ResourceZipErrorCode code,
                                   std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

std::optional<ResourceZip> ResourceZip::OpenAdjacent(
    const std::filesystem::path& dictionary_path) {
    std::filesystem::path archive_path;
    for (const auto& candidate : ArchiveCandidates(dictionary_path)) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error) {
            archive_path = candidate;
            break;
        }
    }
    if (archive_path.empty())
        return std::nullopt;
    ResourceZip result;
    std::error_code error;
    result.path_ = std::filesystem::weakly_canonical(archive_path, error);
    if (error)
        Throw(ResourceZipErrorCode::kUnavailable, archive_path,
              "Cannot resolve DSL resource archive");
    result.source_size_ = std::filesystem::file_size(result.path_, error);
    if (error)
        Throw(ResourceZipErrorCode::kUnavailable, result.path_,
              "Cannot inspect DSL resource archive");
    result.source_write_time_ =
        std::filesystem::last_write_time(result.path_, error);
    if (error)
        Throw(ResourceZipErrorCode::kUnavailable, result.path_,
              "Cannot inspect DSL resource archive timestamp");
    std::ifstream input(result.path_, std::ios::binary);
    if (!input)
        Throw(ResourceZipErrorCode::kUnavailable, result.path_,
              "Cannot open DSL resource archive");
    const auto location =
        LocateDirectory(input, result.source_size_, result.path_);
    result.central_offset_ = location.offset;
    const std::string central =
        ReadAt(input, location.offset, static_cast<std::size_t>(location.size),
               result.path_);
    std::size_t cursor = 0U;
    std::uint64_t parsed_entries = 0U;
    result.entries_.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
        kMaximumEntries,
        std::max<std::uint64_t>(location.entries, central.size() / 100U))));
    while (cursor < central.size()) {
        if (parsed_entries == kMaximumEntries)
            Throw(ResourceZipErrorCode::kInvalidData, result.path_,
                  "ZIP central directory has too many entries");
        if (cursor > central.size() || central.size() - cursor < 46U)
            Throw(ResourceZipErrorCode::kInvalidData, result.path_,
                  "Truncated ZIP central directory entry");
        const std::string_view fixed(central.data() + cursor, 46U);
        if (Le32(fixed, 0U, result.path_) != kCentralSignature)
            Throw(ResourceZipErrorCode::kInvalidData, result.path_,
                  "Invalid ZIP central directory entry");
        const auto flags = Le16(fixed, 8U, result.path_);
        const auto method = Le16(fixed, 10U, result.path_);
        const auto name_size = Le16(fixed, 28U, result.path_);
        const auto extra_size = Le16(fixed, 30U, result.path_);
        const auto comment_size = Le16(fixed, 32U, result.path_);
        const auto disk = Le16(fixed, 34U, result.path_);
        if (name_size == 0U || name_size > kMaximumNameSize ||
            46U + name_size + extra_size + comment_size >
                central.size() - cursor)
            Throw(ResourceZipErrorCode::kInvalidData, result.path_,
                  "Invalid ZIP member metadata bounds");
        const std::string_view variable(central.data() + cursor + 46U,
                                        name_size + extra_size);
        cursor += 46U + name_size + extra_size + comment_size;
        ++parsed_entries;
        std::uint64_t compressed = Le32(fixed, 20U, result.path_);
        std::uint64_t uncompressed = Le32(fixed, 24U, result.path_);
        std::uint64_t local_offset = Le32(fixed, 42U, result.path_);
        const bool zip64_uncompressed = uncompressed == 0xffffffffU;
        const bool zip64_compressed = compressed == 0xffffffffU;
        const bool zip64_offset = local_offset == 0xffffffffU;
        const bool zip64_disk = disk == 0xffffU;
        ParseZip64Extra(variable.substr(name_size), zip64_uncompressed,
                        zip64_compressed, zip64_offset, zip64_disk,
                        &uncompressed, &compressed, &local_offset,
                        result.path_);
        if ((!zip64_disk && disk != 0U) || (flags & 1U) != 0U ||
            (method != 0U && method != 8U) ||
            uncompressed > kMaximumResourceSize ||
            compressed > kMaximumCompressedSize)
            continue;
        std::string resource_id =
            DecodeName(variable.substr(0U, name_size), (flags & 0x800U) != 0U,
                       result.path_);
        std::replace(resource_id.begin(), resource_id.end(), '\\', '/');
        if (!SafeResourceId(resource_id))
            continue;
        result.entries_.try_emplace(
            std::move(resource_id),
            Entry{flags, method, Le32(fixed, 16U, result.path_), compressed,
                  uncompressed, local_offset});
    }
    if ((location.exact_entries && parsed_entries != location.entries) ||
        (!location.exact_entries &&
         static_cast<std::uint16_t>(parsed_entries & 0xffffU) !=
             location.entries))
        Throw(ResourceZipErrorCode::kInvalidData, result.path_,
              "ZIP central directory count does not match its entries");
    if (std::filesystem::file_size(result.path_, error) !=
            result.source_size_ ||
        error ||
        std::filesystem::last_write_time(result.path_, error) !=
            result.source_write_time_ ||
        error)
        Throw(ResourceZipErrorCode::kUnavailable, result.path_,
              "DSL resource archive changed during discovery");
    return result;
}

std::optional<std::vector<std::byte>> ResourceZip::Read(
    std::string_view resource_id) const {
    if (!SafeResourceId(resource_id))
        return std::nullopt;
    const auto found = entries_.find(std::string(resource_id));
    if (found == entries_.end())
        return std::nullopt;
    std::error_code error;
    if (std::filesystem::file_size(path_, error) != source_size_ || error ||
        std::filesystem::last_write_time(path_, error) != source_write_time_ ||
        error)
        Throw(ResourceZipErrorCode::kUnavailable, path_,
              "DSL resource archive changed after discovery");
    std::ifstream input(path_, std::ios::binary);
    if (!input)
        Throw(ResourceZipErrorCode::kUnavailable, path_,
              "Cannot reopen DSL resource archive");
    const Entry& entry = found->second;
    if (entry.local_offset > central_offset_ ||
        central_offset_ - entry.local_offset < 30U)
        Throw(ResourceZipErrorCode::kInvalidData, path_,
              "ZIP resource local header is outside the data region");
    const auto local = ReadAt(input, entry.local_offset, 30U, path_);
    if (Le32(local, 0U, path_) != kLocalSignature ||
        Le16(local, 6U, path_) != entry.flags ||
        Le16(local, 8U, path_) != entry.method)
        Throw(ResourceZipErrorCode::kInvalidData, path_,
              "Invalid ZIP local member header");
    const auto name_size = Le16(local, 26U, path_);
    const auto extra_size = Le16(local, 28U, path_);
    const std::uint64_t data_offset =
        entry.local_offset + 30U + name_size + extra_size;
    if (data_offset > central_offset_ ||
        entry.compressed_size > central_offset_ - data_offset)
        Throw(ResourceZipErrorCode::kInvalidData, path_,
              "ZIP resource member is outside the data region");
    const auto compressed =
        ReadAt(input, data_offset,
               static_cast<std::size_t>(entry.compressed_size), path_);
    std::vector<std::byte> output(
        static_cast<std::size_t>(entry.uncompressed_size));
    if (entry.method == 0U) {
        if (entry.compressed_size != entry.uncompressed_size)
            Throw(ResourceZipErrorCode::kInvalidData, path_,
                  "Stored ZIP resource sizes disagree");
        std::transform(compressed.begin(), compressed.end(), output.begin(),
                       [](char value) {
                           return static_cast<std::byte>(
                               static_cast<unsigned char>(value));
                       });
    } else {
        z_stream stream{};
        stream.next_in =
            reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
        stream.avail_in = static_cast<uInt>(compressed.size());
        std::array<Bytef, 1U> empty_output{};
        stream.next_out = output.empty()
                              ? empty_output.data()
                              : reinterpret_cast<Bytef*>(output.data());
        stream.avail_out = static_cast<uInt>(
            output.empty() ? empty_output.size() : output.size());
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
            Throw(ResourceZipErrorCode::kInvalidData, path_,
                  "Cannot initialize ZIP resource inflater");
        const int status = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);
        if (status != Z_STREAM_END || stream.total_in != compressed.size() ||
            stream.total_out != output.size())
            Throw(ResourceZipErrorCode::kInvalidData, path_,
                  "Invalid deflated ZIP resource member");
    }
    const auto checksum =
        crc32(0U, reinterpret_cast<const Bytef*>(output.data()),
              static_cast<uInt>(output.size()));
    if (checksum != entry.crc)
        Throw(ResourceZipErrorCode::kInvalidData, path_,
              "ZIP resource checksum mismatch");
    if (std::filesystem::file_size(path_, error) != source_size_ || error ||
        std::filesystem::last_write_time(path_, error) != source_write_time_ ||
        error)
        Throw(ResourceZipErrorCode::kUnavailable, path_,
              "DSL resource archive changed during reading");
    return output;
}

}  // namespace goldendict::core::formats::dsl
