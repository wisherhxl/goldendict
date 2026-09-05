// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_ABBREVIATION_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_ABBREVIATION_H_

#include <filesystem>
#include <optional>

namespace goldendict::core::formats::dsl {

std::optional<std::filesystem::path> FindAdjacentAbbreviation(
    const std::filesystem::path& dictionary_path);

}  // namespace goldendict::core::formats::dsl

#endif  // GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_ABBREVIATION_H_
