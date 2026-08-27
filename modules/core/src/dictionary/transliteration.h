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

std::optional<std::string> TransliterateGreek(
    std::string_view text,
    std::size_t output_limit = kMaximumTransliterationOutputBytes);

std::optional<std::string> TransliterateBelarusianLatinClassic(
    std::string_view text,
    std::size_t output_limit = kMaximumTransliterationOutputBytes);

std::optional<std::string> TransliterateBelarusianLatinSchool(
    std::string_view text,
    std::size_t output_limit = kMaximumTransliterationOutputBytes);

std::optional<std::string> TransliterateBelarusianSchoolClassic(
    std::string_view text,
    std::size_t output_limit = kMaximumTransliterationOutputBytes);

std::optional<std::string> TransliterateHepburnHiragana(
    std::string_view text,
    std::size_t output_limit = kMaximumTransliterationOutputBytes);

std::optional<std::string> TransliterateHepburnKatakana(
    std::string_view text,
    std::size_t output_limit = kMaximumTransliterationOutputBytes);

struct TransliterationMappingCounts {
    std::size_t declarations;
    std::size_t effective;
    std::size_t maximum_source_code_points;
};

TransliterationMappingCounts BelarusianLatinClassicMappingCounts();
TransliterationMappingCounts BelarusianLatinSchoolMappingCounts();
TransliterationMappingCounts BelarusianSchoolClassicMappingCounts();
TransliterationMappingCounts HepburnHiraganaMappingCounts();
TransliterationMappingCounts HepburnKatakanaMappingCounts();

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_TRANSLITERATION_H_
