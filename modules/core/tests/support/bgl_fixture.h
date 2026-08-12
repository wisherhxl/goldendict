// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_BGL_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_BGL_FIXTURE_H_
#include <zlib.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace goldendict::core::test {
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

inline std::filesystem::path WriteBglStream(
    const std::filesystem::path& directory, std::string_view stream,
    std::string_view filename) {
    std::filesystem::create_directories(directory);
    const auto path = directory / filename;
    std::ofstream output(path, std::ios::binary);
    output.write("\x12\x34\0\1\0\6", 6);
    output.close();
    gzFile gzip = gzopen(path.c_str(), "ab9");
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
}  // namespace goldendict::core::test
#endif
