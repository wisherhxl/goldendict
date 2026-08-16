// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_DICTD_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_DICTD_FIXTURE_H_

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

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
    const auto compressed_path = data_path.string() + ".dz";
    gzFile output = gzopen(compressed_path.c_str(), "wb9");
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

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_DICTD_FIXTURE_H_
