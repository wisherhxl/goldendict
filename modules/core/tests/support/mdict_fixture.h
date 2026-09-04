// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_MDICT_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_MDICT_FIXTURE_H_

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../src/formats/mdict/mdict_discovery.h"
#include "../../src/formats/mdict/mdict_key_info_crypto.h"

namespace goldendict::core::test {

enum class MdictContainerVersion { kVersion1_2, kVersion2_0 };

inline void AppendMdictBig16(std::uint16_t value, std::string* output) {
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>(value & 0xffU));
}

inline void AppendMdictBig32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>((value >> 24U) & 0xffU));
    output->push_back(static_cast<char>((value >> 16U) & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>(value & 0xffU));
}

inline void AppendMdictLittle32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>(value & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>((value >> 16U) & 0xffU));
    output->push_back(static_cast<char>((value >> 24U) & 0xffU));
}

inline void AppendMdictBig64(std::uint64_t value, std::string* output) {
    for (unsigned shift = 56U;; shift -= 8U) {
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
        if (shift == 0U)
            break;
    }
}

inline void AppendMdictNumber(std::uint64_t value, std::size_t width,
                              std::string* output) {
    if (width == 4U) {
        AppendMdictBig32(static_cast<std::uint32_t>(value), output);
    } else {
        AppendMdictBig64(value, output);
    }
}

inline std::uint32_t MdictAdler(std::string_view data) {
    uLong checksum = adler32(0L, Z_NULL, 0);
    checksum = adler32(checksum, reinterpret_cast<const Bytef*>(data.data()),
                       static_cast<uInt>(data.size()));
    return static_cast<std::uint32_t>(checksum);
}

inline std::string MdictUtf16Le(std::string_view ascii) {
    std::string result;
    result.reserve((ascii.size() + 1U) * 2U);
    for (const unsigned char byte : ascii) {
        result.push_back(static_cast<char>(byte));
        result.push_back('\0');
    }
    result.append(2U, '\0');
    return result;
}

inline std::string MdictUtf16LeText(std::string_view ascii) {
    std::string result;
    result.reserve(ascii.size() * 2U);
    for (const unsigned char byte : ascii) {
        result.push_back(static_cast<char>(byte));
        result.push_back('\0');
    }
    return result;
}

inline std::string MdictZlibBlock(std::string_view data) {
    uLongf compressed_size = compressBound(static_cast<uLong>(data.size()));
    std::string compressed(compressed_size, '\0');
    if (compress2(reinterpret_cast<Bytef*>(compressed.data()), &compressed_size,
                  reinterpret_cast<const Bytef*>(data.data()),
                  static_cast<uLong>(data.size()), Z_BEST_COMPRESSION) != Z_OK)
        throw std::runtime_error("cannot compress MDict fixture");
    compressed.resize(compressed_size);
    std::string block{"\x02\x00\x00\x00", 4U};
    AppendMdictBig32(MdictAdler(data), &block);
    block += compressed;
    return block;
}

inline std::string EncryptMdictKeyInfo(std::string block) {
    std::array<char, 8> seed{};
    std::copy_n(block.data() + 4U, 4U, seed.data());
    seed[4] = static_cast<char>(0x95U);
    seed[5] = static_cast<char>(0x36U);
    const auto key = formats::mdict::detail::Ripemd128(
        std::string_view(seed.data(), seed.size()));
    std::uint8_t previous = 0x36U;
    for (std::size_t index = 8U; index < block.size(); ++index) {
        const std::size_t offset = index - 8U;
        const auto plain = static_cast<std::uint8_t>(block[index]);
        const auto mixed = static_cast<std::uint8_t>(
            plain ^ previous ^ static_cast<std::uint8_t>(offset) ^
            key[offset % key.size()]);
        const auto encrypted =
            static_cast<std::uint8_t>((mixed >> 4U) | (mixed << 4U));
        block[index] = static_cast<char>(encrypted);
        previous = encrypted;
    }
    return block;
}

inline std::filesystem::path WriteMdictContainer(
    const std::filesystem::path& path, std::string_view title,
    const std::vector<std::pair<std::string, std::string>>& entries,
    unsigned encryption = 0U, bool utf16 = false,
    MdictContainerVersion version = MdictContainerVersion::kVersion2_0,
    const std::vector<std::size_t>& record_block_sizes = {}) {
    if (entries.empty())
        throw std::runtime_error("MDict fixture needs entries");
    std::filesystem::create_directories(path.parent_path());
    const bool version_two = version == MdictContainerVersion::kVersion2_0;
    const std::size_t number_width = version_two ? 8U : 4U;
    const std::string header_text =
        "<Dictionary GeneratedByEngineVersion=\"" +
        std::string(version_two ? "2.0" : "1.2") + "\" Encoding=\"" +
        std::string(utf16 ? "UTF-16" : "UTF-8") +
        "\" "
        "Encrypted=\"" +
        std::to_string(encryption) + "\" Left2Right=\"Yes\" Title=\"" +
        std::string(title) +
        "\" Description=\"Fixture description\" "
        "StyleSheet=\"1&#10;&lt;b&gt;&#10;&lt;/b&gt;\"/>";
    const std::string header = MdictUtf16Le(header_text);

    std::string records;
    std::string keys;
    for (const auto& entry : entries) {
        AppendMdictNumber(records.size(), number_width, &keys);
        keys += utf16 ? MdictUtf16LeText(entry.first) : entry.first;
        keys.append(utf16 ? 2U : 1U, '\0');
        records += utf16 ? MdictUtf16LeText(entry.second) : entry.second;
    }
    const std::string key_block = MdictZlibBlock(keys);
    std::string key_info;
    AppendMdictNumber(entries.size(), number_width, &key_info);
    if (version_two) {
        AppendMdictBig16(
            static_cast<std::uint16_t>(entries.front().first.size()),
            &key_info);
    } else {
        key_info.push_back(static_cast<char>(entries.front().first.size()));
    }
    key_info +=
        utf16 ? MdictUtf16LeText(entries.front().first) : entries.front().first;
    if (version_two)
        key_info.append(utf16 ? 2U : 1U, '\0');
    if (version_two) {
        AppendMdictBig16(
            static_cast<std::uint16_t>(entries.back().first.size()), &key_info);
    } else {
        key_info.push_back(static_cast<char>(entries.back().first.size()));
    }
    key_info +=
        utf16 ? MdictUtf16LeText(entries.back().first) : entries.back().first;
    if (version_two)
        key_info.append(utf16 ? 2U : 1U, '\0');
    AppendMdictNumber(key_block.size(), number_width, &key_info);
    AppendMdictNumber(keys.size(), number_width, &key_info);
    std::string stored_key_info =
        version_two ? MdictZlibBlock(key_info) : key_info;
    if (encryption == 2U)
        stored_key_info = EncryptMdictKeyInfo(std::move(stored_key_info));

    std::string file;
    AppendMdictBig32(static_cast<std::uint32_t>(header.size()), &file);
    file += header;
    AppendMdictLittle32(MdictAdler(header), &file);
    std::string key_header;
    AppendMdictNumber(1U, number_width, &key_header);
    AppendMdictNumber(entries.size(), number_width, &key_header);
    if (version_two)
        AppendMdictNumber(key_info.size(), number_width, &key_header);
    AppendMdictNumber(stored_key_info.size(), number_width, &key_header);
    AppendMdictNumber(key_block.size(), number_width, &key_header);
    file += key_header;
    if (version_two)
        AppendMdictBig32(MdictAdler(key_header), &file);
    file += stored_key_info;
    file += key_block;

    std::vector<std::string> record_blocks;
    if (record_block_sizes.empty()) {
        record_blocks.push_back(MdictZlibBlock(records));
    } else {
        std::size_t offset = 0;
        for (const std::size_t size : record_block_sizes) {
            if (size == 0U || size > records.size() - offset)
                throw std::runtime_error("invalid MDict record block split");
            record_blocks.push_back(
                MdictZlibBlock(std::string_view(records).substr(offset, size)));
            offset += size;
        }
        if (offset != records.size())
            throw std::runtime_error("incomplete MDict record block split");
    }
    std::size_t stored_record_size = 0;
    for (const auto& block : record_blocks)
        stored_record_size += block.size();
    AppendMdictNumber(record_blocks.size(), number_width, &file);
    AppendMdictNumber(entries.size(), number_width, &file);
    AppendMdictNumber(record_blocks.size() * number_width * 2U, number_width,
                      &file);
    AppendMdictNumber(stored_record_size, number_width, &file);
    for (std::size_t index = 0; index < record_blocks.size(); ++index) {
        AppendMdictNumber(record_blocks[index].size(), number_width, &file);
        const std::size_t decoded_size = record_block_sizes.empty()
                                             ? records.size()
                                             : record_block_sizes[index];
        AppendMdictNumber(decoded_size, number_width, &file);
    }
    for (const auto& block : record_blocks)
        file += block;

    std::ofstream output(path, std::ios::binary);
    output.write(file.data(), static_cast<std::streamsize>(file.size()));
    if (!output)
        throw std::runtime_error("cannot write MDict fixture");
    return path;
}

inline formats::mdict::DictionaryFiles WriteMdictFixture(
    const std::filesystem::path& directory) {
    const auto mdx = WriteMdictContainer(
        directory / "fixture.mdx", "Fixture MDict",
        {{"alias", "@@@LINK=example"},
         {"example", "`1`definition`0`<img src=\"pixel.png\">"}});
    const auto mdd =
        WriteMdictContainer(directory / "fixture.mdd", "Fixture resources",
                            {{"\\pixel.png", "mdict-png"}});
    return {mdx, {mdd}};
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_MDICT_FIXTURE_H_
