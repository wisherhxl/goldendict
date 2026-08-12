// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_SLOB_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_SLOB_FIXTURE_H_
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
inline void SlobBe16(std::uint16_t value, std::string* output) {
    output->push_back(static_cast<char>(value >> 8U));
    output->push_back(static_cast<char>(value));
}

inline void SlobBe32(std::uint32_t value, std::string* output) {
    for (int shift = 24; shift >= 0; shift -= 8)
        output->push_back(static_cast<char>(value >> shift));
}

inline void SlobBe64(std::uint64_t value, std::string* output) {
    for (int shift = 56; shift >= 0; shift -= 8)
        output->push_back(static_cast<char>(value >> shift));
}

inline void SlobSet64(std::uint64_t value, std::size_t at,
                      std::string* output) {
    for (unsigned i = 0; i < 8U; ++i)
        (*output)[at + i] = static_cast<char>(value >> ((7U - i) * 8U));
}

inline void SlobTiny(std::string_view value, std::string* output) {
    output->push_back(static_cast<char>(value.size()));
    output->append(value);
}

inline void SlobText(std::string_view value, std::string* output) {
    SlobBe16(static_cast<std::uint16_t>(value.size()), output);
    output->append(value);
}

inline std::string SlobCompress(std::string_view input,
                                std::string_view compression) {
    if (compression == "zlib") {
        std::string output(compressBound(input.size()), '\0');
        uLongf size = output.size();
        if (compress2(reinterpret_cast<Bytef*>(output.data()), &size,
                      reinterpret_cast<const Bytef*>(input.data()),
                      input.size(), Z_BEST_COMPRESSION) != Z_OK)
            throw std::runtime_error("cannot compress SLOB fixture");
        output.resize(size);
        return output;
    }
    if (compression == "bz2") {
        std::string output(input.size() + input.size() / 100U + 601U, '\0');
        unsigned int size = output.size();
        if (BZ2_bzBuffToBuffCompress(output.data(), &size,
                                     const_cast<char*>(input.data()),
                                     input.size(), 9, 0, 30) != BZ_OK)
            throw std::runtime_error("cannot bzip2-compress SLOB fixture");
        output.resize(size);
        return output;
    }
    return std::string(input);
}

inline std::filesystem::path WriteSlobFixture(
    const std::filesystem::path& directory,
    std::string_view compression = "none") {
    std::filesystem::create_directories(directory);
    std::string file("\x21\x2d\x31SLOB\x1f", 8U);
    file.append(16U, '\0');
    SlobTiny("UTF-8", &file);
    SlobTiny(compression, &file);
    file.push_back(4);
    SlobTiny("name", &file);
    SlobTiny("Fixture SLOB", &file);
    SlobTiny("description", &file);
    SlobTiny("fixture", &file);
    SlobTiny("source_language", &file);
    SlobTiny("en", &file);
    SlobTiny("target_language", &file);
    SlobTiny("de", &file);
    file.push_back(2);
    SlobText("text/html", &file);
    SlobText("image/png", &file);
    SlobBe32(2U, &file);
    const auto store_field = file.size();
    SlobBe64(0U, &file);
    const auto size_field = file.size();
    SlobBe64(0U, &file);
    SlobBe32(3U, &file);
    const auto ref_table = file.size();
    file.resize(file.size() + 24U, '\0');
    const auto ref_base = file.size();

    struct Ref {
        std::string key;
        std::uint16_t bin;
    };

    const std::vector<Ref> refs = {
        {"example", 0U}, {"alias", 0U}, {"pixel.png", 1U}};
    for (std::size_t i = 0; i < refs.size(); ++i) {
        SlobSet64(file.size() - ref_base, ref_table + i * 8U, &file);
        SlobText(refs[i].key, &file);
        SlobBe32(0U, &file);
        SlobBe16(refs[i].bin, &file);
        SlobTiny("", &file);
    }
    const auto store = file.size();
    SlobBe32(1U, &file);
    SlobBe64(0U, &file);
    std::string data;
    const std::string article = "<b>definition</b><img src=\"pixel.png\">";
    SlobBe32(0U, &data);
    SlobBe32(4U + article.size(), &data);
    SlobBe32(article.size(), &data);
    data += article;
    SlobBe32(8U, &data);
    data += "png-data";
    const auto stored = SlobCompress(data, compression);
    SlobBe32(2U, &file);
    file.push_back(0);
    file.push_back(1);
    SlobBe32(stored.size(), &file);
    file += stored;
    SlobSet64(store, store_field, &file);
    SlobSet64(file.size(), size_field, &file);
    const auto path = directory / "fixture.slob";
    std::ofstream output(path, std::ios::binary);
    output.write(file.data(), static_cast<std::streamsize>(file.size()));
    if (!output)
        throw std::runtime_error("cannot write SLOB fixture");
    return path;
}
}  // namespace goldendict::core::test
#endif
