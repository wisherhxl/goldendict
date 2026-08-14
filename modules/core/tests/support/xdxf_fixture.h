// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_XDXF_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_XDXF_FIXTURE_H_

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

namespace goldendict::core::test {

struct XdxfFixtureEntry {
    std::vector<std::string> headwords;
    std::string article;
};

inline std::string EscapeXdxf(std::string_view value) {
    std::string result;
    for (const char character : value) {
        switch (character) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            default:
                result.push_back(character);
        }
    }
    return result;
}

inline std::filesystem::path WriteXdxfFixture(
    const std::filesystem::path& directory,
    const std::vector<XdxfFixtureEntry>& entries) {
    std::filesystem::create_directories(directory);
    std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<xdxf lang_from=\"eng\" lang_to=\"deu\" format=\"logical\" "
        "revision=\"34\"><full_name>Fixture XDXF</full_name>"
        "<description>Fixture description</description>";
    for (const auto& entry : entries) {
        xml += "<ar>";
        for (const auto& headword : entry.headwords) {
            xml += "<k>" + EscapeXdxf(headword) + "</k>";
        }
        xml += entry.article + "</ar>";
    }
    xml += "</xdxf>";
    const auto path = directory / "fixture.xdxf";
    std::ofstream output(path, std::ios::binary);
    output.write(xml.data(), static_cast<std::streamsize>(xml.size()));
    return path;
}

inline std::filesystem::path CompressXdxfFixture(
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
        throw std::runtime_error("Cannot create compressed XDXF fixture");
    }
    std::filesystem::remove(path);
    return compressed;
}

inline std::filesystem::path WriteXdxfResource(
    const std::filesystem::path& dictionary_path,
    const std::filesystem::path& relative_path, std::string_view data) {
    const auto path = dictionary_path.parent_path() / relative_path;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return path;
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_XDXF_FIXTURE_H_
