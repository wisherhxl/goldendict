// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_STARDICT_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_STARDICT_FIXTURE_H_

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

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
    const std::vector<FixtureEntry>& entries) {
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
        "\nsametypesequence=m\n";
    WriteBinaryFile(info_path, info);
    WriteBinaryFile(directory / "fixture.idx", index);
    WriteBinaryFile(directory / "fixture.dict", dictionary);
    return info_path;
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_STARDICT_FIXTURE_H_
