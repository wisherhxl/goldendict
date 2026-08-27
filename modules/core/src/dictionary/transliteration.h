// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_TRANSLITERATION_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_TRANSLITERATION_H_

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace goldendict::core::dictionary {

inline constexpr std::size_t kMaximumTransliterationInputBytes = 4096U;
inline constexpr std::size_t kMaximumTransliterationOutputBytes = 16384U;

class TransliterationError final : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

std::optional<std::string> TransliterateRussian(
    std::string_view text,
    std::size_t output_limit = kMaximumTransliterationOutputBytes);

std::optional<std::string> TransliterateGerman(
    std::string_view text,
    std::size_t output_limit = kMaximumTransliterationOutputBytes);

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_TRANSLITERATION_H_
