// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_CHINESE_CONVERSION_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_CHINESE_CONVERSION_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "transliteration.h"

namespace goldendict::core::dictionary {

enum class ChineseConversionVariant {
    kSimplifiedToTaiwan,
    kSimplifiedToHongKong,
    kTraditionalToSimplified,
};

std::optional<std::string> ConvertChinese(
    std::string_view text, ChineseConversionVariant variant,
    std::string_view configuration_directory,
    std::size_t output_limit = kMaximumTransliterationOutputBytes);

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_CHINESE_CONVERSION_H_
