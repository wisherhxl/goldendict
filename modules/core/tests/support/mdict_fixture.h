// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_MDICT_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_MDICT_FIXTURE_H_

#include <zlib.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../src/formats/mdict/mdict_discovery.h"

namespace goldendict::core::test {

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

inline std::filesystem::path WriteMdictContainer(
    const std::filesystem::path& path, std::string_view title,
    const std::vector<std::pair<std::string, std::string>>& entries,
    bool encrypted = false, bool utf16 = false) {
    if (entries.empty())
        throw std::runtime_error("MDict fixture needs entries");
    std::filesystem::create_directories(path.parent_path());
    const std::string header_text =
        "<Dictionary GeneratedByEngineVersion=\"2.0\" Encoding=\"" +
        std::string(utf16 ? "UTF-16" : "UTF-8") +
        "\" "
        "Encrypted=\"" +
        std::string(encrypted ? "2" : "0") + "\" Left2Right=\"Yes\" Title=\"" +
        std::string(title) +
        "\" Description=\"Fixture description\" "
        "StyleSheet=\"1&#10;&lt;b&gt;&#10;&lt;/b&gt;\"/>";
    const std::string header = MdictUtf16Le(header_text);

    std::string records;
    std::string keys;
    for (const auto& entry : entries) {
        AppendMdictBig64(records.size(), &keys);
        keys += utf16 ? MdictUtf16LeText(entry.first) : entry.first;
        keys.append(utf16 ? 2U : 1U, '\0');
        records += utf16 ? MdictUtf16LeText(entry.second) : entry.second;
    }
    const std::string key_block = MdictZlibBlock(keys);
    std::string key_info;
    AppendMdictBig64(entries.size(), &key_info);
    AppendMdictBig16(static_cast<std::uint16_t>(entries.front().first.size()),
                     &key_info);
    key_info +=
        utf16 ? MdictUtf16LeText(entries.front().first) : entries.front().first;
    key_info.append(utf16 ? 2U : 1U, '\0');
    AppendMdictBig16(static_cast<std::uint16_t>(entries.back().first.size()),
                     &key_info);
    key_info +=
        utf16 ? MdictUtf16LeText(entries.back().first) : entries.back().first;
    key_info.append(utf16 ? 2U : 1U, '\0');
    AppendMdictBig64(key_block.size(), &key_info);
    AppendMdictBig64(keys.size(), &key_info);
    const std::string compressed_key_info = MdictZlibBlock(key_info);

    std::string file;
    AppendMdictBig32(static_cast<std::uint32_t>(header.size()), &file);
    file += header;
    AppendMdictLittle32(MdictAdler(header), &file);
    std::string key_header;
    AppendMdictBig64(1U, &key_header);
    AppendMdictBig64(entries.size(), &key_header);
    AppendMdictBig64(key_info.size(), &key_header);
    AppendMdictBig64(compressed_key_info.size(), &key_header);
    AppendMdictBig64(key_block.size(), &key_header);
    file += key_header;
    AppendMdictBig32(MdictAdler(key_header), &file);
    file += compressed_key_info;
    file += key_block;

    const std::string record_block = MdictZlibBlock(records);
    AppendMdictBig64(1U, &file);
    AppendMdictBig64(entries.size(), &file);
    AppendMdictBig64(16U, &file);
    AppendMdictBig64(record_block.size(), &file);
    AppendMdictBig64(record_block.size(), &file);
    AppendMdictBig64(records.size(), &file);
    file += record_block;

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
