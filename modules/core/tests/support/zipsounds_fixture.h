// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_ZIPSOUNDS_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_ZIPSOUNDS_FIXTURE_H_
#include <zlib.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace goldendict::core::formats::zipsounds::test {
namespace detail {
inline void Le16(std::string* output, std::uint16_t value) {
    output->push_back(static_cast<char>(value & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
}

inline void Le32(std::string* output, std::uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i)
        output->push_back(static_cast<char>((value >> (i * 8U)) & 0xffU));
}

inline std::string Deflate(std::string_view input) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("cannot initialize fixture deflater");
    std::string output(compressBound(input.size()), '\0');
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    const int status = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    if (status != Z_STREAM_END)
        throw std::runtime_error("cannot deflate fixture entry");
    output.resize(stream.total_out);
    return output;
}

struct Entry {
    std::string name;
    std::string content;
    std::string compressed;
    std::uint16_t method = 0;
    std::uint32_t crc = 0;
    std::uint32_t local_offset = 0;
};
}  // namespace detail

inline std::filesystem::path WriteZipSoundsFixture(
    const std::filesystem::path& directory,
    std::string_view filename = "fixture.zips") {
    std::filesystem::create_directories(directory);
    std::vector<detail::Entry> entries = {
        {"example.wav", "RIFFfixture-wave", {}, 0U},
        {"nested/second.ogg", "OggSfixture-ogg", {}, 8U},
        {"ignored.txt", "not audio", {}, 0U}};
    std::string archive;
    for (auto& entry : entries) {
        entry.compressed =
            entry.method == 8U ? detail::Deflate(entry.content) : entry.content;
        entry.crc =
            crc32(0U, reinterpret_cast<const Bytef*>(entry.content.data()),
                  static_cast<uInt>(entry.content.size()));
        entry.local_offset = static_cast<std::uint32_t>(archive.size());
        detail::Le32(&archive, 0x04034b50U);
        detail::Le16(&archive, 20U);
        detail::Le16(&archive, 0x0800U);
        detail::Le16(&archive, entry.method);
        detail::Le16(&archive, 0U);
        detail::Le16(&archive, 0U);
        detail::Le32(&archive, entry.crc);
        detail::Le32(&archive,
                     static_cast<std::uint32_t>(entry.compressed.size()));
        detail::Le32(&archive,
                     static_cast<std::uint32_t>(entry.content.size()));
        detail::Le16(&archive, static_cast<std::uint16_t>(entry.name.size()));
        detail::Le16(&archive, 0U);
        archive += entry.name;
        archive += entry.compressed;
    }
    const auto central_offset = static_cast<std::uint32_t>(archive.size());
    for (const auto& entry : entries) {
        detail::Le32(&archive, 0x02014b50U);
        detail::Le16(&archive, 20U);
        detail::Le16(&archive, 20U);
        detail::Le16(&archive, 0x0800U);
        detail::Le16(&archive, entry.method);
        detail::Le16(&archive, 0U);
        detail::Le16(&archive, 0U);
        detail::Le32(&archive, entry.crc);
        detail::Le32(&archive,
                     static_cast<std::uint32_t>(entry.compressed.size()));
        detail::Le32(&archive,
                     static_cast<std::uint32_t>(entry.content.size()));
        detail::Le16(&archive, static_cast<std::uint16_t>(entry.name.size()));
        detail::Le16(&archive, 0U);
        detail::Le16(&archive, 0U);
        detail::Le16(&archive, 0U);
        detail::Le16(&archive, 0U);
        detail::Le32(&archive, 0U);
        detail::Le32(&archive, entry.local_offset);
        archive += entry.name;
    }
    const auto central_size =
        static_cast<std::uint32_t>(archive.size()) - central_offset;
    detail::Le32(&archive, 0x06054b50U);
    detail::Le16(&archive, 0U);
    detail::Le16(&archive, 0U);
    detail::Le16(&archive, static_cast<std::uint16_t>(entries.size()));
    detail::Le16(&archive, static_cast<std::uint16_t>(entries.size()));
    detail::Le32(&archive, central_size);
    detail::Le32(&archive, central_offset);
    detail::Le16(&archive, 0U);
    const auto path = directory / filename;
    std::ofstream output(path, std::ios::binary);
    output.write(archive.data(), static_cast<std::streamsize>(archive.size()));
    if (!output)
        throw std::runtime_error("cannot write ZIP sound fixture");
    return path;
}
}  // namespace goldendict::core::formats::zipsounds::test
#endif
