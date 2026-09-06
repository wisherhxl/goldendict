// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_STARDICT_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_STARDICT_FIXTURE_H_

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zlib.h>

#include "gzip_fixture.h"

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

inline std::string ReadBinaryFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot read StarDict fixture file");
    }
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

inline void AppendStardictInfoField(const std::filesystem::path& info_path,
                                    std::string_view key,
                                    std::string_view value) {
    auto info = ReadBinaryFile(info_path);
    info += std::string(key) + "=" + std::string(value) + "\n";
    WriteBinaryFile(info_path, info);
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
        "bookname=Generated Test Dictionary en-en\n"
        "lang_from=en\n"
        "lang_to=en\n"
        "author=Fixture Author\n"
        "description=Fixture description\n"
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

inline std::filesystem::path WriteStardictSynonymData(
    const std::filesystem::path& info_path,
    const std::vector<std::pair<std::string, std::uint32_t>>& synonyms) {
    std::string data;
    for (const auto& [headword, article_index] : synonyms) {
        data.append(headword);
        data.push_back('\0');
        AppendBigEndian32(article_index, &data);
    }
    auto synonym_path = info_path;
    synonym_path.replace_extension(".syn");
    WriteBinaryFile(synonym_path, data);
    return synonym_path;
}

inline void WriteStardictSynonyms(
    const std::filesystem::path& info_path,
    const std::vector<std::pair<std::string, std::uint32_t>>& synonyms) {
    static_cast<void>(WriteStardictSynonymData(info_path, synonyms));
    AppendStardictInfoField(info_path, "synwordcount",
                            std::to_string(synonyms.size()));
}

inline std::filesystem::path CompressStardictCompanion(
    const std::filesystem::path& source_path,
    const std::filesystem::path& compressed_path) {
    const auto contents = ReadBinaryFile(source_path);
    gzFile output = OpenGzipFixture(compressed_path, "wb9");
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

inline std::filesystem::path CompressStardictDictionary(
    const std::filesystem::path& info_path) {
    auto dictionary_path = info_path;
    dictionary_path.replace_extension(".dict");
    auto compressed_path = dictionary_path;
    compressed_path += ".dz";
    return CompressStardictCompanion(dictionary_path, compressed_path);
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_STARDICT_FIXTURE_H_
