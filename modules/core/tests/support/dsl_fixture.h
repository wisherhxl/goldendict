// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_DSL_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_DSL_FIXTURE_H_

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#include <zlib.h>

#include "gzip_fixture.h"

namespace goldendict::core::test {

inline std::filesystem::path WriteDslTextFixture(
    const std::filesystem::path& directory, std::string_view text,
    std::string_view filename = "fixture.dsl") {
    std::filesystem::create_directories(directory);
    const auto path = directory / filename;
    std::ofstream output(path, std::ios::binary);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return path;
}

inline std::filesystem::path WriteDslFixture(
    const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    const std::string text =
        "#NAME \"Fixture DSL\"\n"
        "#INDEX_LANGUAGE \"English\"\n"
        "#CONTENTS_LANGUAGE \"German\"\n"
        "#SOURCE_CODE_PAGE \"UTF-8\"\n"
        "Caf(e)\n"
        "~ shop\n"
        "\t[b]drink[/b] [*]optional[/*] <<coffee>> [s]images/cup.png[/s]\n"
        "cafeteria\n"
        "\tplace\n";
    const auto path = WriteDslTextFixture(directory, text);
    std::ofstream annotation(directory / "fixture.ann", std::ios::binary);
    annotation << "Fixture DSL annotation";
    return path;
}

inline std::filesystem::path CompressDslFixture(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    input.close();
    auto compressed = path;
    compressed += ".dz";
    gzFile output = OpenGzipFixture(compressed, "wb9");
    if (output == nullptr ||
        gzwrite(output, data.data(), static_cast<unsigned int>(data.size())) !=
            static_cast<int>(data.size()) ||
        gzclose(output) != Z_OK) {
        throw std::runtime_error("Cannot create compressed DSL fixture");
    }
    std::filesystem::remove(path);
    return compressed;
}

inline std::filesystem::path WriteUtf16LeDslFixture(
    const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    constexpr std::string_view text =
        "#NAME \"UTF16 DSL\"\n"
        "#INDEX_LANGUAGE \"English\"\n"
        "#CONTENTS_LANGUAGE \"Russian\"\n"
        "example\n"
        "\tdefinition\n";
    std::string encoded{"\xff\xfe", 2U};
    for (const unsigned char character : text) {
        encoded.push_back(static_cast<char>(character));
        encoded.push_back('\0');
    }
    const auto path = directory / "utf16.dsl";
    std::ofstream output(path, std::ios::binary);
    output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    return path;
}

inline std::filesystem::path WriteDslResource(
    const std::filesystem::path& dictionary_path,
    const std::filesystem::path& relative_path, std::string_view data) {
    const auto root =
        std::filesystem::u8path(dictionary_path.string() + ".files");
    const auto path = root / relative_path;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return path;
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_DSL_FIXTURE_H_
