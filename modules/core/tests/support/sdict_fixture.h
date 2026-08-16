// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_SDICT_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_SDICT_FIXTURE_H_

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <bzlib.h>
#include <zlib.h>

namespace goldendict::core::test {

struct SdictFixtureEntry {
    SdictFixtureEntry(std::string headword_value, std::string article_value,
                      std::optional<std::size_t> shared_index = std::nullopt)
        : headword(std::move(headword_value)),
          article(std::move(article_value)),
          shared_article_index(shared_index) {}

    std::string headword;
    std::string article;
    std::optional<std::size_t> shared_article_index;
};

inline void AppendLittle16(std::uint16_t value, std::string* output) {
    output->push_back(static_cast<char>(value & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
}

inline void AppendLittle32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>(value & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>((value >> 16U) & 0xffU));
    output->push_back(static_cast<char>((value >> 24U) & 0xffU));
}

inline void WriteLittle32(std::uint32_t value, std::size_t offset,
                          std::string* output) {
    for (std::size_t byte = 0; byte < 4U; ++byte) {
        (*output)[offset + byte] =
            static_cast<char>((value >> (byte * 8U)) & 0xffU);
    }
}

inline std::string CompressSdictField(std::string_view value,
                                      std::uint8_t compression) {
    if (compression == 0U) {
        return std::string(value);
    }
    if (compression == 1U) {
        uLongf size = compressBound(static_cast<uLong>(value.size()));
        std::string output(size, '\0');
        if (compress2(reinterpret_cast<Bytef*>(output.data()), &size,
                      reinterpret_cast<const Bytef*>(value.data()),
                      static_cast<uLong>(value.size()),
                      Z_BEST_COMPRESSION) != Z_OK) {
            throw std::runtime_error("Cannot create zlib SDict fixture");
        }
        output.resize(size);
        return output;
    }
    unsigned int size = static_cast<unsigned int>(value.size() * 2U + 600U);
    std::string output(size, '\0');
    if (BZ2_bzBuffToBuffCompress(
            output.data(), &size, const_cast<char*>(value.data()),
            static_cast<unsigned int>(value.size()), 9, 0, 30) != BZ_OK) {
        throw std::runtime_error("Cannot create bzip2 SDict fixture");
    }
    output.resize(size);
    return output;
}

inline void AppendSizedSdictField(std::string_view value,
                                  std::uint8_t compression,
                                  std::string* output) {
    const auto stored = CompressSdictField(value, compression);
    AppendLittle32(static_cast<std::uint32_t>(stored.size()), output);
    output->append(stored);
}

inline std::filesystem::path WriteSdictFixture(
    const std::filesystem::path& directory,
    const std::vector<SdictFixtureEntry>& entries,
    std::uint8_t compression = 0U) {
    std::filesystem::create_directories(directory);
    std::string file(43U, '\0');
    file.replace(0U, 4U, "sdct");
    file.replace(4U, 3U, "eng");
    file.replace(7U, 3U, "deu");
    file[10U] = static_cast<char>(compression);
    WriteLittle32(static_cast<std::uint32_t>(entries.size()), 11U, &file);
    WriteLittle32(43U, 19U, &file);
    AppendSizedSdictField("Fixture SDict", compression, &file);
    WriteLittle32(static_cast<std::uint32_t>(file.size()), 23U, &file);
    AppendSizedSdictField("Fixture copyright", compression, &file);
    WriteLittle32(static_cast<std::uint32_t>(file.size()), 27U, &file);
    AppendSizedSdictField("1.0", compression, &file);
    const auto full_index_offset = static_cast<std::uint32_t>(file.size());
    WriteLittle32(full_index_offset, 35U, &file);

    std::string articles;
    std::vector<std::uint32_t> article_offsets;
    article_offsets.reserve(entries.size());
    for (std::size_t ordinal = 0U; ordinal < entries.size(); ++ordinal) {
        const auto& entry = entries[ordinal];
        if (entry.shared_article_index.has_value()) {
            if (*entry.shared_article_index >= ordinal) {
                throw std::runtime_error(
                    "SDict fixture alias must reference an earlier article");
            }
            article_offsets.push_back(
                article_offsets[*entry.shared_article_index]);
            continue;
        }
        article_offsets.push_back(static_cast<std::uint32_t>(articles.size()));
        AppendSizedSdictField(entry.article, compression, &articles);
    }
    std::string index;
    for (std::size_t ordinal = 0U; ordinal < entries.size(); ++ordinal) {
        const auto& entry = entries[ordinal];
        const auto next =
            static_cast<std::uint16_t>(8U + entry.headword.size());
        AppendLittle16(next, &index);
        AppendLittle16(0U, &index);
        AppendLittle32(article_offsets[ordinal], &index);
        index += entry.headword;
    }
    file += index;
    WriteLittle32(static_cast<std::uint32_t>(file.size()), 39U, &file);
    file += articles;
    const auto path = directory / "fixture.dct";
    std::ofstream output(path, std::ios::binary);
    output.write(file.data(), static_cast<std::streamsize>(file.size()));
    return path;
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_SDICT_FIXTURE_H_
