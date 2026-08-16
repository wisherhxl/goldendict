// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_GLS_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_GLS_FIXTURE_H_

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zlib.h>

namespace goldendict::core::test {

struct GlsFixtureEntry {
    std::vector<std::string> headwords;
    std::string article;
};

inline std::filesystem::path WriteGlsFixture(
    const std::filesystem::path& directory,
    const std::vector<GlsFixtureEntry>& entries) {
    std::filesystem::create_directories(directory);
    std::string text =
        "### Glossary title: Fixture GLS\n"
        "### Author: GoldenDict tests\n"
        "### Description: Fixture description\n"
        "### Source language: eng\n"
        "### Target language: deu\n"
        "### Glossary section:\n\n";
    for (const auto& entry : entries) {
        for (std::size_t index = 0; index < entry.headwords.size(); ++index) {
            if (index != 0U) {
                text.push_back('|');
            }
            text += entry.headwords[index];
        }
        text += "\n" + entry.article + "\n\n";
    }
    const auto path = directory / "fixture.gls";
    std::ofstream output(path, std::ios::binary);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return path;
}

inline std::filesystem::path WriteUtf16LeGlsFixture(
    const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    constexpr std::string_view text =
        "### Glossary title: UTF16 GLS\n"
        "### Source language: eng\n"
        "### Target language: deu\n"
        "### Glossary section:\n\n"
        "example\n"
        "<b>definition</b>\n";
    std::string encoded{"\xff\xfe", 2U};
    encoded.reserve(2U + text.size() * 2U);
    for (const unsigned char character : text) {
        encoded.push_back(static_cast<char>(character));
        encoded.push_back('\0');
    }
    const auto path = directory / "utf16.gls";
    std::ofstream output(path, std::ios::binary);
    output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    return path;
}

inline std::filesystem::path WriteUtf16BeGlsFixture(
    const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    constexpr std::string_view text =
        "### Glossary title: UTF16 BE GLS\n"
        "### Source language: eng\n"
        "### Target language: deu\n"
        "### Glossary section:\n\n"
        "example\n"
        "<b>definition</b>\n";
    std::string encoded{"\xfe\xff", 2U};
    encoded.reserve(2U + text.size() * 2U);
    for (const unsigned char character : text) {
        encoded.push_back('\0');
        encoded.push_back(static_cast<char>(character));
    }
    const auto path = directory / "utf16be.gls";
    std::ofstream output(path, std::ios::binary);
    output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    return path;
}

inline std::filesystem::path CompressGlsFixture(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    const auto compressed = path.string() + ".dz";
    gzFile output = gzopen(compressed.c_str(), "wb9");
    if (output == nullptr ||
        gzwrite(output, data.data(), static_cast<unsigned int>(data.size())) !=
            static_cast<int>(data.size()) ||
        gzclose(output) != Z_OK) {
        throw std::runtime_error("Cannot create compressed GLS fixture");
    }
    std::filesystem::remove(path);
    return compressed;
}

inline std::filesystem::path WriteGlsResource(
    const std::filesystem::path& dictionary_path,
    const std::filesystem::path& relative_path, std::string_view data) {
    const auto path = dictionary_path.parent_path() / relative_path;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return path;
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_GLS_FIXTURE_H_
