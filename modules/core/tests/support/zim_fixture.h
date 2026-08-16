// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_ZIM_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_ZIM_FIXTURE_H_
#include <bzlib.h>
#include <zlib.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace goldendict::core::test {
inline void ZimSet16(std::uint16_t value, std::size_t at, std::string* data) {
    for (unsigned i = 0; i < 2U; ++i)
        (*data)[at + i] = static_cast<char>(value >> (i * 8U));
}

inline void ZimSet32(std::uint32_t value, std::size_t at, std::string* data) {
    for (unsigned i = 0; i < 4U; ++i)
        (*data)[at + i] = static_cast<char>(value >> (i * 8U));
}

inline void ZimSet64(std::uint64_t value, std::size_t at, std::string* data) {
    for (unsigned i = 0; i < 8U; ++i)
        (*data)[at + i] = static_cast<char>(value >> (i * 8U));
}

inline void ZimAppend32(std::uint32_t value, std::string* data) {
    const auto at = data->size();
    data->resize(at + 4U);
    ZimSet32(value, at, data);
}

inline std::string ZimCompress(std::string_view input, unsigned compression) {
    if (compression == 2U) {
        std::string output(compressBound(input.size()), '\0');
        uLongf size = output.size();
        if (compress2(reinterpret_cast<Bytef*>(output.data()), &size,
                      reinterpret_cast<const Bytef*>(input.data()),
                      input.size(), Z_BEST_COMPRESSION) != Z_OK)
            throw std::runtime_error("cannot zlib-compress ZIM fixture");
        output.resize(size);
        return output;
    }
    if (compression == 3U) {
        std::string output(input.size() + input.size() / 100U + 601U, '\0');
        unsigned int size = output.size();
        if (BZ2_bzBuffToBuffCompress(output.data(), &size,
                                     const_cast<char*>(input.data()),
                                     input.size(), 9, 0, 30) != BZ_OK)
            throw std::runtime_error("cannot bzip2-compress ZIM fixture");
        output.resize(size);
        return output;
    }
    return std::string(input);
}

inline std::filesystem::path WriteZimFixture(
    const std::filesystem::path& directory,
    std::string_view filename = "fixture.zim", unsigned compression = 1U,
    bool offsets64 = false,
    std::string_view html = "<b>definition</b><img src=\"I/pixel.png\">",
    std::string_view resource = "png-data") {
    std::filesystem::create_directories(directory);

    struct Item {
        std::uint16_t mime;
        char name_space;
        std::string url;
        std::string title;
        std::uint32_t blob;
        bool redirect = false;
        std::uint32_t target = 0;
    };

    const std::vector<std::string> blobs = {
        std::string(html), std::string(resource), "Fixture ZIM", "en",
        "plain <unsafe>"};
    const std::vector<Item> items = {
        {0U, 'A', "example", "Example", 0U},
        {0xffffU, 'A', "alias", "Alias", 0U, true, 0U},
        {1U, 'I', "pixel.png", "", 1U},
        {2U, 'M', "Title", "", 2U},
        {2U, 'M', "Language", "", 3U},
        {2U, 'A', "plain", "Plain", 4U}};
    const std::size_t width = offsets64 ? 8U : 4U;
    std::string cluster_data((blobs.size() + 1U) * width, '\0');
    std::size_t cursor = cluster_data.size();
    for (std::size_t i = 0; i < blobs.size(); ++i) {
        offsets64 ? ZimSet64(cursor, i * width, &cluster_data)
                  : ZimSet32(cursor, i * width, &cluster_data);
        cluster_data += blobs[i];
        cursor += blobs[i].size();
    }
    offsets64 ? ZimSet64(cursor, blobs.size() * width, &cluster_data)
              : ZimSet32(cursor, blobs.size() * width, &cluster_data);
    std::string cluster(
        1U, static_cast<char>(compression | (offsets64 ? 0x10U : 0U)));
    cluster += ZimCompress(cluster_data, compression);

    std::string file(80U, '\0');
    ZimSet32(0x044d495aU, 0U, &file);
    ZimSet16(6U, 4U, &file);
    ZimSet16(1U, 6U, &file);
    ZimSet32(items.size(), 24U, &file);
    ZimSet32(1U, 28U, &file);
    ZimSet64(80U, 56U, &file);
    file.append("text/html\0image/png\0text/plain\0\0", 32U);
    const auto url_table = file.size();
    file.resize(file.size() + items.size() * 8U);
    const auto cluster_table = file.size();
    file.resize(file.size() + 8U);
    for (std::size_t i = 0; i < items.size(); ++i) {
        ZimSet64(file.size(), url_table + i * 8U, &file);
        const auto& item = items[i];
        const auto start = file.size();
        file.resize(start + (item.redirect ? 12U : 16U), '\0');
        ZimSet16(item.mime, start, &file);
        file[start + 3U] = item.name_space;
        if (item.redirect)
            ZimSet32(item.target, start + 8U, &file);
        else {
            ZimSet32(0U, start + 8U, &file);
            ZimSet32(item.blob, start + 12U, &file);
        }
        file += item.url;
        file.push_back('\0');
        file += item.title;
        file.push_back('\0');
    }
    ZimSet64(file.size(), cluster_table, &file);
    file += cluster;
    ZimSet64(url_table, 32U, &file);
    ZimSet64(url_table, 40U, &file);
    ZimSet64(cluster_table, 48U, &file);
    ZimSet64(file.size(), 72U, &file);
    const auto path = directory / filename;
    std::ofstream output(path, std::ios::binary);
    output.write(file.data(), static_cast<std::streamsize>(file.size()));
    if (!output)
        throw std::runtime_error("cannot write ZIM fixture");
    return path;
}
}  // namespace goldendict::core::test
#endif
