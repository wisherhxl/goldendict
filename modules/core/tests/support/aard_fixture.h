// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_AARD_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_AARD_FIXTURE_H_
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
inline void AardBe16(std::uint16_t value, std::string* output) {
    output->push_back(static_cast<char>(value >> 8U));
    output->push_back(static_cast<char>(value));
}

inline void AardBe32(std::uint32_t value, std::string* output) {
    for (int shift = 24; shift >= 0; shift -= 8)
        output->push_back(static_cast<char>(value >> shift));
}

inline void AardBe64(std::uint64_t value, std::string* output) {
    for (int shift = 56; shift >= 0; shift -= 8)
        output->push_back(static_cast<char>(value >> shift));
}

inline std::string AardZlib(std::string_view input) {
    std::string output(compressBound(input.size()), '\0');
    uLongf size = output.size();
    if (compress2(reinterpret_cast<Bytef*>(output.data()), &size,
                  reinterpret_cast<const Bytef*>(input.data()), input.size(),
                  Z_BEST_COMPRESSION) != Z_OK)
        throw std::runtime_error("cannot compress Aard fixture");
    output.resize(size);
    return output;
}

inline std::string AardBzip2(std::string_view input) {
    std::string output(input.size() + input.size() / 100U + 601U, '\0');
    unsigned int size = output.size();
    if (BZ2_bzBuffToBuffCompress(output.data(), &size,
                                 const_cast<char*>(input.data()), input.size(),
                                 9, 0, 30) != BZ_OK)
        throw std::runtime_error("cannot compress Aard fixture with bzip2");
    output.resize(size);
    return output;
}

inline std::filesystem::path WriteAardFixture(
    const std::filesystem::path& directory,
    std::string_view filename = "fixture.aar", bool bzip2 = false,
    bool index64 = false, bool raw_articles = false) {
    std::filesystem::create_directories(directory);
    const auto compress = [bzip2](std::string_view value) {
        return bzip2 ? AardBzip2(value) : AardZlib(value);
    };
    const std::string metadata = compress(
        "{\"title\":\"Fixture Aard\",\"index_language\":\"en\","
        "\"article_language\":\"de\",\"description\":\"fixture\"}");
    const std::vector<std::string> words = {"example", "alias", "redirect"};
    const std::vector<std::size_t> article_ids = {0U, 0U, 1U};
    const auto encode_article = [&compress,
                                 raw_articles](std::string_view value) {
        return raw_articles ? std::string(value) : compress(value);
    };
    const std::vector<std::string> articles = {
        encode_article(
            "[\"<b>definition</b><a href=\\\"w:alias\\\">alias</a>\"]"),
        encode_article("[\"\",{\"r\":\"example\"}]")};

    std::string word_data;
    std::vector<std::uint32_t> word_offsets;
    for (const auto& word : words) {
        word_offsets.push_back(word_data.size());
        AardBe16(static_cast<std::uint16_t>(word.size()), &word_data);
        word_data += word;
    }
    std::string article_data;
    std::vector<std::uint32_t> article_offsets;
    for (const auto& article : articles) {
        article_offsets.push_back(article_data.size());
        AardBe32(article.size(), &article_data);
        article_data += article;
    }
    const std::uint32_t article_base = 86U + metadata.size() +
                                       words.size() * (index64 ? 12U : 8U) +
                                       word_data.size();
    std::string header(4U + 40U, '\0');
    header.replace(0, 4U, "aard");
    AardBe16(1U, &header);
    header.append(16U, '\0');
    AardBe16(1U, &header);
    AardBe16(1U, &header);
    AardBe32(metadata.size(), &header);
    AardBe32(words.size(), &header);
    AardBe32(article_base, &header);
    header.append(index64 ? ">LQ" : ">LL", 3U);
    header.push_back('\0');
    header += ">H>L";
    if (header.size() != 86U)
        throw std::runtime_error("invalid Aard header");
    std::string index;
    for (std::size_t i = 0; i < words.size(); ++i) {
        AardBe32(word_offsets[i], &index);
        if (index64)
            AardBe64(article_offsets[article_ids[i]], &index);
        else
            AardBe32(article_offsets[article_ids[i]], &index);
    }
    const auto path = directory / filename;
    std::ofstream output(path, std::ios::binary);
    output.write(header.data(), header.size());
    output.write(metadata.data(), metadata.size());
    output.write(index.data(), index.size());
    output.write(word_data.data(), word_data.size());
    output.write(article_data.data(), article_data.size());
    if (!output)
        throw std::runtime_error("cannot write Aard fixture");
    return path;
}
}  // namespace goldendict::core::test
#endif
