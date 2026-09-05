// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsl_abbreviation.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <system_error>

namespace goldendict::core::formats::dsl {
namespace {

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

}  // namespace

std::optional<std::filesystem::path> FindAdjacentAbbreviation(
    const std::filesystem::path& dictionary_path) {
    const std::string filename = dictionary_path.filename().string();
    const std::string lowered = Lower(filename);
    const std::size_t suffix =
        lowered.size() >= 7U &&
                lowered.compare(lowered.size() - 7U, 7U, ".dsl.dz") == 0
            ? 7U
        : lowered.size() >= 4U &&
                lowered.compare(lowered.size() - 4U, 4U, ".dsl") == 0
            ? 4U
            : 0U;
    if (suffix == 0U)
        return std::nullopt;

    const std::string base = filename.substr(0U, filename.size() - suffix);
    constexpr std::array<std::string_view, 5> candidates{
        "_abrv.dsl", "_abrv.dsl.dz", "_ABRV.DSL", "_ABRV.DSL.DZ",
        "_ABRV.DSL.dz"};
    for (const auto candidate : candidates) {
        auto path = dictionary_path;
        path.replace_filename(base + std::string(candidate));
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error) && !error)
            return path;
    }
    return std::nullopt;
}

}  // namespace goldendict::core::formats::dsl
