// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_DSL_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_DSL_FIXTURE_H_

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

struct DslZipResource {
    std::string name;
    std::string data;
    bool deflated = false;
};

inline void AppendZip16(std::string& output, std::uint16_t value) {
    for (unsigned index = 0U; index < 2U; ++index)
        output.push_back(static_cast<char>((value >> (index * 8U)) & 0xffU));
}

inline void AppendZip32(std::string& output, std::uint32_t value) {
    for (unsigned index = 0U; index < 4U; ++index)
        output.push_back(static_cast<char>((value >> (index * 8U)) & 0xffU));
}

inline void AppendZip64(std::string& output, std::uint64_t value) {
    for (unsigned index = 0U; index < 8U; ++index)
        output.push_back(static_cast<char>((value >> (index * 8U)) & 0xffU));
}

inline std::string DeflateZipResource(std::string_view data) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("Cannot initialize ZIP fixture deflater");
    std::string output(compressBound(static_cast<uLong>(data.size())), '\0');
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    const int status = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    if (status != Z_STREAM_END)
        throw std::runtime_error("Cannot deflate ZIP fixture member");
    output.resize(static_cast<std::size_t>(stream.total_out));
    return output;
}

inline std::filesystem::path WriteDslResourceZip(
    const std::filesystem::path& dictionary_path,
    const std::vector<DslZipResource>& resources, bool zip64 = false) {
    struct CentralEntry {
        DslZipResource resource;
        std::string compressed;
        std::uint32_t crc = 0U;
        std::uint64_t offset = 0U;
    };

    std::string archive;
    std::vector<CentralEntry> central;
    central.reserve(resources.size());
    for (const auto& resource : resources) {
        CentralEntry entry;
        entry.resource = resource;
        entry.compressed = resource.deflated ? DeflateZipResource(resource.data)
                                             : resource.data;
        entry.crc =
            crc32(0U, reinterpret_cast<const Bytef*>(resource.data.data()),
                  static_cast<uInt>(resource.data.size()));
        entry.offset = archive.size();
        AppendZip32(archive, 0x04034b50U);
        AppendZip16(archive, resource.deflated ? 20U : 10U);
        AppendZip16(archive, 0x0800U);
        AppendZip16(archive, resource.deflated ? 8U : 0U);
        AppendZip16(archive, 0U);
        AppendZip16(archive, 0U);
        AppendZip32(archive, entry.crc);
        AppendZip32(archive,
                    static_cast<std::uint32_t>(entry.compressed.size()));
        AppendZip32(archive, static_cast<std::uint32_t>(resource.data.size()));
        AppendZip16(archive, static_cast<std::uint16_t>(resource.name.size()));
        AppendZip16(archive, 0U);
        archive += resource.name;
        archive += entry.compressed;
        central.push_back(std::move(entry));
    }
    const std::uint64_t central_offset = archive.size();
    for (const auto& entry : central) {
        std::string extra;
        if (zip64) {
            AppendZip16(extra, 0x0001U);
            AppendZip16(extra, 24U);
            AppendZip64(extra, entry.resource.data.size());
            AppendZip64(extra, entry.compressed.size());
            AppendZip64(extra, entry.offset);
        }
        AppendZip32(archive, 0x02014b50U);
        AppendZip16(archive, zip64 ? 45U : 20U);
        AppendZip16(archive, zip64 ? 45U : 20U);
        AppendZip16(archive, 0x0800U);
        AppendZip16(archive, entry.resource.deflated ? 8U : 0U);
        AppendZip16(archive, 0U);
        AppendZip16(archive, 0U);
        AppendZip32(archive, entry.crc);
        AppendZip32(archive, zip64 ? 0xffffffffU
                                   : static_cast<std::uint32_t>(
                                         entry.compressed.size()));
        AppendZip32(archive, zip64 ? 0xffffffffU
                                   : static_cast<std::uint32_t>(
                                         entry.resource.data.size()));
        AppendZip16(archive,
                    static_cast<std::uint16_t>(entry.resource.name.size()));
        AppendZip16(archive, static_cast<std::uint16_t>(extra.size()));
        AppendZip16(archive, 0U);
        AppendZip16(archive, 0U);
        AppendZip16(archive, 0U);
        AppendZip32(archive, 0U);
        AppendZip32(archive, zip64 ? 0xffffffffU
                                   : static_cast<std::uint32_t>(entry.offset));
        archive += entry.resource.name;
        archive += extra;
    }
    const std::uint64_t central_size = archive.size() - central_offset;
    if (zip64) {
        const std::uint64_t zip64_offset = archive.size();
        AppendZip32(archive, 0x06064b50U);
        AppendZip64(archive, 44U);
        AppendZip16(archive, 45U);
        AppendZip16(archive, 45U);
        AppendZip32(archive, 0U);
        AppendZip32(archive, 0U);
        AppendZip64(archive, central.size());
        AppendZip64(archive, central.size());
        AppendZip64(archive, central_size);
        AppendZip64(archive, central_offset);
        AppendZip32(archive, 0x07064b50U);
        AppendZip32(archive, 0U);
        AppendZip64(archive, zip64_offset);
        AppendZip32(archive, 1U);
    }
    AppendZip32(archive, 0x06054b50U);
    AppendZip16(archive, 0U);
    AppendZip16(archive, 0U);
    AppendZip16(archive,
                zip64 ? 0xffffU : static_cast<std::uint16_t>(central.size()));
    AppendZip16(archive,
                zip64 ? 0xffffU : static_cast<std::uint16_t>(central.size()));
    AppendZip32(archive, static_cast<std::uint32_t>(central_size));
    AppendZip32(archive, static_cast<std::uint32_t>(central_offset));
    AppendZip16(archive, 0U);
    auto archive_path = dictionary_path;
    archive_path += ".files.zip";
    std::ofstream output(archive_path, std::ios::binary | std::ios::trunc);
    output.write(archive.data(), static_cast<std::streamsize>(archive.size()));
    return archive_path;
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_DSL_FIXTURE_H_
