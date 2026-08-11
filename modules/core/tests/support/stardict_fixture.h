// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_STARDICT_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_STARDICT_FIXTURE_H_

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

namespace goldendict::core::test {

struct FixtureEntry {
    std::string headword;
    std::string article;
};

inline void AppendBigEndian32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>((value >> 24U) & 0xffU));
    output->push_back(static_cast<char>((value >> 16U) & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>(value & 0xffU));
}

inline void WriteBinaryFile(const std::filesystem::path& path,
                            const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    output.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
}

inline std::filesystem::path WriteStardictFixture(
    const std::filesystem::path& directory,
    const std::vector<FixtureEntry>& entries,
    std::string same_type_sequence = "m") {
    std::string index;
    std::string dictionary;
    for (const auto& entry : entries) {
        index.append(entry.headword);
        index.push_back('\0');
        AppendBigEndian32(static_cast<std::uint32_t>(dictionary.size()),
                          &index);
        AppendBigEndian32(static_cast<std::uint32_t>(entry.article.size()),
                          &index);
        dictionary.append(entry.article);
    }

    const auto info_path = directory / "fixture.ifo";
    const std::string info =
        "StarDict's dict ifo file\n"
        "version=2.4.2\n"
        "bookname=Generated Test Dictionary\n"
        "wordcount=" +
        std::to_string(entries.size()) +
        "\nidxfilesize=" + std::to_string(index.size()) +
        "\nsametypesequence=" + same_type_sequence + "\n";
    WriteBinaryFile(info_path, info);
    WriteBinaryFile(directory / "fixture.idx", index);
    WriteBinaryFile(directory / "fixture.dict", dictionary);
    return info_path;
}

inline std::filesystem::path WriteStardictResource(
    const std::filesystem::path& dictionary_directory,
    const std::filesystem::path& resource_id, const std::string& contents) {
    const auto path = dictionary_directory / "res" / resource_id;
    std::filesystem::create_directories(path.parent_path());
    WriteBinaryFile(path, contents);
    return path;
}

inline std::filesystem::path CompressStardictDictionary(
    const std::filesystem::path& info_path) {
    auto dictionary_path = info_path;
    dictionary_path.replace_extension(".dict");
    std::ifstream input(dictionary_path, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    const auto compressed_path = dictionary_path.string() + ".dz";
    gzFile output = gzopen(compressed_path.c_str(), "wb9");
    if (output == nullptr) {
        throw std::runtime_error("Cannot open compressed StarDict fixture");
    }
    const int written = gzwrite(output, contents.data(),
                                static_cast<unsigned>(contents.size()));
    const int close_result = gzclose(output);
    if (written != static_cast<int>(contents.size()) || close_result != Z_OK) {
        throw std::runtime_error("Cannot write compressed StarDict fixture");
    }
    return compressed_path;
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_STARDICT_FIXTURE_H_
