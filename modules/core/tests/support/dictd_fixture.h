// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_DICTD_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_DICTD_FIXTURE_H_

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zlib.h>

#include "gzip_fixture.h"

namespace goldendict::core::test {

struct DictdFixtureEntry {
    DictdFixtureEntry(
        std::string headword_value, std::string article_value,
        std::string original_headword_value,
        std::optional<std::size_t> article_reference_value = std::nullopt)
        : headword(std::move(headword_value)),
          article(std::move(article_value)),
          original_headword(std::move(original_headword_value)),
          article_reference(article_reference_value) {}

    std::string headword;
    std::string article;
    std::string original_headword;
    std::optional<std::size_t> article_reference;
};

inline std::string EncodeDictdBase64(std::uint32_t value) {
    constexpr char digits[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (value == 0U) {
        return "A";
    }
    std::string result;
    while (value != 0U) {
        result.insert(result.begin(), digits[value % 64U]);
        value /= 64U;
    }
    return result;
}

inline std::filesystem::path WriteDictdFixture(
    const std::filesystem::path& directory,
    const std::vector<DictdFixtureEntry>& entries) {
    std::filesystem::create_directories(directory);
    std::string index;
    std::string data;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    ranges.reserve(entries.size());
    for (std::size_t ordinal = 0U; ordinal < entries.size(); ++ordinal) {
        const auto& entry = entries[ordinal];
        std::uint32_t offset = static_cast<std::uint32_t>(data.size());
        std::uint32_t size = static_cast<std::uint32_t>(entry.article.size());
        if (entry.article_reference.has_value()) {
            if (*entry.article_reference >= ordinal) {
                throw std::runtime_error("Invalid Dictd article reference");
            }
            std::tie(offset, size) = ranges[*entry.article_reference];
        } else {
            data += entry.article;
        }
        ranges.emplace_back(offset, size);
        index += entry.headword + '\t' + EncodeDictdBase64(offset) + '\t' +
                 EncodeDictdBase64(size);
        if (!entry.original_headword.empty()) {
            index += '\t' + entry.original_headword;
        }
        index.push_back('\n');
    }
    const auto index_path = directory / "fixture.index";
    std::ofstream index_output(index_path, std::ios::binary);
    index_output.write(index.data(),
                       static_cast<std::streamsize>(index.size()));
    std::ofstream data_output(directory / "fixture.dict", std::ios::binary);
    data_output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return index_path;
}

inline std::filesystem::path CompressDictdFixture(
    const std::filesystem::path& index_path) {
    auto data_path = index_path;
    data_path.replace_extension(".dict");
    std::ifstream input(data_path, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    auto compressed_path = data_path;
    compressed_path += ".dz";
    gzFile output = OpenGzipFixture(compressed_path, "wb9");
    if (output == nullptr) {
        throw std::runtime_error("Cannot create compressed Dictd fixture");
    }
    const int written =
        gzwrite(output, data.data(), static_cast<unsigned>(data.size()));
    const int closed = gzclose(output);
    if (written != static_cast<int>(data.size()) || closed != Z_OK) {
        throw std::runtime_error("Cannot finish compressed Dictd fixture");
    }
    return compressed_path;
}

// Generated test content only. Each table entry ends with Z_FULL_FLUSH so
// the frozen Dictd reader can inflate that chunk from an independent offset.
inline std::string EncodeDictzipFixture(std::string_view data,
                                        std::uint16_t chunk_length = 64U) {
    if (data.empty() || chunk_length == 0U) {
        throw std::runtime_error("Dictzip fixture needs data and a chunk size");
    }
    const auto chunk_count = (data.size() + chunk_length - 1U) / chunk_length;
    if (chunk_count > (65535U - 10U) / 2U) {
        throw std::runtime_error("Dictzip fixture RA table is too large");
    }
    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("Cannot initialize dictzip fixture");
    }

    struct EndDeflate {
        z_stream* stream;

        ~EndDeflate() { deflateEnd(stream); }
    } end_deflate{&stream};

    std::vector<std::uint16_t> lengths;
    std::string payload;
    std::vector<char> buffer(deflateBound(&stream, chunk_length) + 64U);
    for (std::size_t offset = 0U; offset < data.size();
         offset += chunk_length) {
        const auto size =
            std::min<std::size_t>(chunk_length, data.size() - offset);
        stream.next_in =
            reinterpret_cast<Bytef*>(const_cast<char*>(data.data() + offset));
        stream.avail_in = static_cast<uInt>(size);
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        const int status = deflate(&stream, Z_FULL_FLUSH);
        const auto written = buffer.size() - stream.avail_out;
        if (status != Z_OK || stream.avail_in != 0U || stream.avail_out == 0U ||
            written > 65535U) {
            throw std::runtime_error("Cannot encode dictzip fixture chunk");
        }
        lengths.push_back(static_cast<std::uint16_t>(written));
        payload.append(buffer.data(), written);
    }
    stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
    stream.avail_out = static_cast<uInt>(buffer.size());
    if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
        throw std::runtime_error("Cannot finish dictzip fixture stream");
    }
    payload.append(buffer.data(), buffer.size() - stream.avail_out);

    std::string result("\x1f\x8b\x08\x04\0\0\0\0\0\xff", 10U);
    const auto append16 = [&result](std::uint16_t value) {
        result.push_back(static_cast<char>(value & 0xffU));
        result.push_back(static_cast<char>(value >> 8U));
    };
    append16(static_cast<std::uint16_t>(10U + 2U * chunk_count));
    result += "RA";
    append16(static_cast<std::uint16_t>(6U + 2U * chunk_count));
    append16(1U);
    append16(chunk_length);
    append16(static_cast<std::uint16_t>(chunk_count));
    for (const auto length : lengths) {
        append16(length);
    }
    result += payload;
    const auto append32 = [&result](std::uint32_t value) {
        for (unsigned shift = 0U; shift < 32U; shift += 8U) {
            result.push_back(static_cast<char>((value >> shift) & 0xffU));
        }
    };
    append32(static_cast<std::uint32_t>(
        crc32(0U, reinterpret_cast<const Bytef*>(data.data()),
              static_cast<uInt>(data.size()))));
    append32(static_cast<std::uint32_t>(data.size()));
    return result;
}

inline std::string AddDictzipFixtureHeaderFields(
    std::string bytes, const std::optional<std::string>& filename,
    const std::optional<std::string>& comment, bool header_crc) {
    const auto extra_length = static_cast<unsigned char>(bytes.at(10U)) |
                              (static_cast<unsigned char>(bytes.at(11U)) << 8U);
    const auto header_size = 12U + extra_length;
    auto header = bytes.substr(0U, header_size);
    if (filename.has_value()) {
        header[3] |= 0x08;
        header += *filename;
        header.push_back('\0');
    }
    if (comment.has_value()) {
        header[3] |= 0x10;
        header += *comment;
        header.push_back('\0');
    }
    if (header_crc) {
        header[3] |= 0x02;
        const auto crc = crc32(0U, reinterpret_cast<const Bytef*>(header.data()),
                               static_cast<uInt>(header.size()));
        header.push_back(static_cast<char>(crc & 0xffU));
        header.push_back(static_cast<char>((crc >> 8U) & 0xffU));
    }
    return header + bytes.substr(header_size);
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_DICTD_FIXTURE_H_
