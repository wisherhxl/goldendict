// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_BGL_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_BGL_FIXTURE_H_
#include <zlib.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace goldendict::core::test {
inline void AppendBglBig(std::size_t value, std::size_t width,
                         std::string* data) {
    for (std::size_t shift = width; shift > 0U; --shift) {
        data->push_back(
            static_cast<char>((value >> ((shift - 1U) * 8U)) & 0xffU));
    }
}

inline void AppendBglBlock(unsigned type, std::string_view data,
                           std::string* stream) {
    if (data.size() <= 11U)
        stream->push_back(static_cast<char>(((data.size() + 4U) << 4U) | type));
    else {
        if (data.size() > 255U)
            throw std::runtime_error("fixture block too large");
        stream->push_back(static_cast<char>(type));
        stream->push_back(static_cast<char>(data.size()));
    }
    stream->append(data);
}

inline void AppendBglEntry(unsigned type, std::string_view primary,
                           std::string_view definition,
                           const std::vector<std::string_view>& alternates,
                           std::string* stream) {
    std::string entry;
    if (type == 11U) {
        entry.push_back('\0');
        AppendBglBig(primary.size(), 4U, &entry);
        entry.append(primary);
        AppendBglBig(alternates.size(), 4U, &entry);
        for (const auto alternate : alternates) {
            AppendBglBig(alternate.size(), 4U, &entry);
            entry.append(alternate);
        }
        AppendBglBig(definition.size(), 4U, &entry);
        entry.append(definition);
    } else {
        entry.push_back(static_cast<char>(primary.size()));
        entry.append(primary);
        AppendBglBig(definition.size(), 2U, &entry);
        entry.append(definition);
        for (const auto alternate : alternates) {
            entry.push_back(static_cast<char>(alternate.size()));
            entry.append(alternate);
        }
    }
    AppendBglBlock(type, entry, stream);
}

inline std::filesystem::path WriteBglStream(
    const std::filesystem::path& directory, std::string_view stream,
    std::string_view filename) {
    std::filesystem::create_directories(directory);
    const auto path = directory / filename;
    std::ofstream output(path, std::ios::binary);
    output.write("\x12\x34\0\1\0\6", 6);
    output.close();
#ifdef _WIN32
    gzFile gzip = gzopen_w(path.c_str(), "ab9");
#else
    gzFile gzip = gzopen(path.c_str(), "ab9");
#endif
    if (!gzip ||
        gzwrite(gzip, stream.data(), static_cast<unsigned>(stream.size())) !=
            static_cast<int>(stream.size()) ||
        gzclose(gzip) != Z_OK)
        throw std::runtime_error("cannot write BGL gzip stream");
    return path;
}

inline std::filesystem::path WriteBglFixture(
    const std::filesystem::path& directory) {
    std::string stream;
    AppendBglBlock(3U, std::string("\0\1Fixture BGL", 13U), &stream);
    AppendBglBlock(3U, std::string("\0\2Fixture Author", 16U), &stream);
    AppendBglBlock(3U, std::string("\0\4Fixture Copyright", 19U), &stream);
    AppendBglBlock(3U, std::string("\0\11Fixture description", 21U), &stream);
    AppendBglBlock(3U, std::string("\0\7\0\0\0\0", 6U), &stream);
    AppendBglBlock(3U, std::string("\0\10\0\0\0\6", 6U), &stream);
    AppendBglBlock(3U, std::string("\0\21\0\0\200", 5U), &stream);
    const std::string definition = "<b>definition</b><img src=\"pixel.png\">";
    std::string entry;
    entry.push_back(7);
    entry += "example";
    entry.push_back(0);
    entry.push_back(static_cast<char>(definition.size()));
    entry += definition;
    entry.push_back(5);
    entry += "alias";
    AppendBglBlock(1U, entry, &stream);
    std::string resource;
    resource.push_back(9);
    resource += "pixel.png";
    resource += "png-data";
    AppendBglBlock(2U, resource, &stream);
    stream.push_back(4);
    return WriteBglStream(directory, stream, "fixture.bgl");
}

inline std::filesystem::path WriteWindows1251BglFixture(
    const std::filesystem::path& directory) {
    std::string stream;
    AppendBglBlock(0U, std::string("\x08\x00\x03", 3U), &stream);
    constexpr std::string_view headword{"\xef\xf0\xe8\xec\xe5\xf0", 6U};
    constexpr std::string_view definition{"\xf2\xe5\xf1\xf2", 4U};
    std::string entry;
    entry.push_back(static_cast<char>(headword.size()));
    entry.append(headword);
    entry.push_back('\0');
    entry.push_back(static_cast<char>(definition.size()));
    entry.append(definition);
    AppendBglBlock(1U, entry, &stream);
    stream.push_back(4);
    return WriteBglStream(directory, stream, "cp1251.bgl");
}

inline std::filesystem::path WriteBglFullTextFixture(
    const std::filesystem::path& directory,
    std::string_view first_definition =
        "<b>visible searchable definition</b>"
        "<a href=\"link-target-secret\">safe label</a>"
        "<img src=\"resource-name-secret.png\">",
    std::string_view resource_data = "resource-bytes-secret") {
    std::string stream;
    AppendBglBlock(3U, std::string("\0\1Metadata Name Secret", 22U), &stream);
    AppendBglBlock(3U, std::string("\0\21\0\0\200", 5U), &stream);
    AppendBglEntry(1U, "", first_definition, {"first-owner"}, &stream);
    AppendBglEntry(7U, "empty-article", "", {"empty-alias"}, &stream);
    AppendBglEntry(10U, "third-owner", "third layout searchable", {}, &stream);
    AppendBglEntry(11U, "fourth-owner", "fourth layout searchable",
                   {"fourth-alias"}, &stream);
    AppendBglEntry(1U, "", "unreferenced article secret", {""}, &stream);
    std::string resource;
    constexpr std::string_view resource_name = "resource-name-secret.png";
    resource.push_back(static_cast<char>(resource_name.size()));
    resource.append(resource_name);
    resource.append(resource_data);
    AppendBglBlock(2U, resource, &stream);
    stream.push_back(4);
    return WriteBglStream(directory, stream, "full-text.bgl");
}
}  // namespace goldendict::core::test
#endif
