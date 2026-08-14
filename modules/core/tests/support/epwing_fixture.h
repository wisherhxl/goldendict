// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_EPWING_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_EPWING_FIXTURE_H_
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace goldendict::core::test {
inline void EpwingBe16(std::uint16_t value, std::string* data, std::size_t at) {
    (*data)[at] = static_cast<char>(value >> 8U);
    (*data)[at + 1U] = static_cast<char>(value);
}

inline void EpwingBe32(std::uint32_t value, std::string* data, std::size_t at) {
    for (unsigned i = 0; i < 4U; ++i)
        (*data)[at + i] = static_cast<char>(value >> ((3U - i) * 8U));
}

inline void EpwingWrite(const std::filesystem::path& path,
                        std::string_view data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!output)
        throw std::runtime_error("cannot write EPWING fixture");
}

inline std::filesystem::path WriteEpwingFixture(
    const std::filesystem::path& root, bool latin = true) {
    std::string catalog(16U + 164U, '\0');
    EpwingBe16(1U, &catalog, 0U);
    EpwingBe16(1U, &catalog, 2U);
    if (latin)
        catalog.replace(18U, 14U, "Fixture EPWING");
    else
        catalog.replace(18U, 4U, "\x46\x7c\x4b\x5c", 4U);
    catalog.replace(98U, 7U, "FIXTURE");
    EpwingBe16(1U, &catalog, 110U);
    EpwingWrite(root / "CATALOGS", catalog);
    if (latin) {
        std::string language(16U, '\0');
        EpwingBe16(1U, &language, 0U);
        EpwingWrite(root / "LANGUAGE", language);
    }

    std::string honmon(4U * 2048U, '\0');
    honmon[1] = latin ? 2 : 1;
    honmon[16] = static_cast<char>(0x92);
    EpwingBe32(2U, &honmon, 18U);
    EpwingBe32(1U, &honmon, 22U);
    if (latin) {
        honmon[32] = static_cast<char>(0x02);
        EpwingBe32(4U, &honmon, 34U);
    }
    honmon[2048U] = static_cast<char>(0xe0);
    EpwingBe16(2U, &honmon, 2050U);
    std::size_t cursor = 2052U;
    const auto add_index = [&](std::string_view word, std::uint16_t offset,
                               std::string* file, std::size_t* position) {
        (*file)[(*position)++] = static_cast<char>(word.size());
        file->replace(*position, word.size(), word);
        *position += word.size();
        EpwingBe32(3U, file, *position);
        EpwingBe16(offset, file, *position + 4U);
        EpwingBe32(3U, file, *position + 6U);
        EpwingBe16(offset, file, *position + 10U);
        *position += 12U;
    };
    if (latin) {
        add_index("example", 0U, &honmon, &cursor);
        add_index("second", 64U, &honmon, &cursor);
        std::string first("\x1f\x02", 2U);
        first += "definition ";
        first.append("\x1f\x42\x00\x00next", 8U);
        first.append("\x1f\x62\x00\x00\x00\x03\x00\x64", 8U);
        first.append("\x1f\x03", 2U);
        honmon.replace(4096U, first.size(), first);
        const std::string second("\x1f\x02second article\x1f\x03", 18U);
        honmon.replace(4096U + 64U, second.size(), second);
        std::string copyright("\x1f\x02", 2U);
        copyright += "Fixture copyright";
        copyright.append("\x1f\x03", 2U);
        honmon.replace(6144U, copyright.size(), copyright);
    } else {
        honmon[16] = static_cast<char>(0x91);
        EpwingBe16(1U, &honmon, 2050U);
        add_index("\x46\x7c\x4b\x5c", 0U, &honmon, &cursor);
        const std::string article("\x1f\x02\x44\x6a\x35\x41\x1f\x03", 8U);
        honmon.replace(4096U, article.size(), article);
    }
    EpwingWrite(root / "FIXTURE" / "DATA" / "HONMON", honmon);
    EpwingWrite(root / "FIXTURE" / "GAIJI" / "pixel.png", "png-data");
    return root / "CATALOGS";
}
}  // namespace goldendict::core::test
#endif
