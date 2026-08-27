// SPDX-License-Identifier: GPL-3.0-or-later

#include "transliteration.h"

#include <string_view>
#include <unordered_set>

#include "../foundation/text_folding.h"
#include "../foundation/utf8.h"

namespace goldendict::core::dictionary {
namespace {

struct Mapping {
    std::string_view source;
    std::string_view target;
};

// Pinned legacy RussianTranslit table. Matching is longest-first regardless of
// declaration order, as in TransliterationDictionary::getAlternateWritings().
constexpr Mapping kRussianMappings[]{
    {"a", "а"},  {"b", "б"},  {"v", "в"},  {"w", "в"},    {"g", "г"},
    {"d", "д"},  {"e", "е"},  {"yo", "ё"}, {"zh", "ж"},   {"z", "з"},
    {"i", "и"},  {"j", "й"},  {"k", "к"},  {"l", "л"},    {"m", "м"},
    {"n", "н"},  {"o", "о"},  {"p", "п"},  {"r", "р"},    {"s", "с"},
    {"t", "т"},  {"u", "у"},  {"f", "ф"},  {"h", "х"},    {"ts", "ц"},
    {"c", "ц"},  {"ch", "ч"}, {"sh", "ш"}, {"shch", "щ"}, {"\"", "ъ"},
    {"y", "ы"},  {"'", "ь"},  {"'e", "э"}, {"yu", "ю"},   {"ya", "я"},
    {"A", "А"},  {"B", "Б"},  {"V", "В"},  {"W", "В"},    {"G", "Г"},
    {"D", "Д"},  {"E", "Е"},  {"YO", "Ё"}, {"Yo", "Ё"},   {"ZH", "Ж"},
    {"Zh", "Ж"}, {"Z", "З"},  {"I", "И"},  {"J", "Й"},    {"K", "К"},
    {"L", "Л"},  {"M", "М"},  {"N", "Н"},  {"O", "О"},    {"P", "П"},
    {"R", "Р"},  {"S", "С"},  {"T", "Т"},  {"U", "У"},    {"F", "Ф"},
    {"H", "Х"},  {"TS", "Ц"}, {"Ts", "Ц"}, {"C", "Ц"},    {"CH", "Ч"},
    {"Ch", "Ч"}, {"SH", "Ш"}, {"Sh", "Ш"}, {"SHCH", "Щ"}, {"ShCh", "Щ"},
    {"Y", "Ы"},  {"'E", "Э"}, {"YU", "Ю"}, {"Yu", "Ю"},   {"YA", "Я"},
    {"Ya", "Я"},
};

// Pinned legacy GermanTranslit table. The reverse mappings remain disabled,
// matching the source baseline.
constexpr Mapping kGermanMappings[]{
    {"ue", "ü"}, {"ae", "ä"}, {"oe", "ö"}, {"ss", "ß"},
    {"UE", "Ü"}, {"AE", "Ä"}, {"OE", "Ö"}, {"SS", "ß"},
    {"Ue", "Ü"}, {"Ae", "Ä"}, {"Oe", "Ö"}, {"Ss", "ß"},
};

constexpr Mapping kGreekMappings[]{
#include "greek_transliteration_table.inc"
};

#include "belarusian_transliteration_table.inc"

std::size_t CodePointCount(std::string_view text) noexcept {
    std::size_t count = 0;
    for (const unsigned char byte : text) {
        count += (byte & 0xc0U) != 0x80U;
    }
    return count;
}

void AppendBounded(std::string_view value, std::size_t limit,
                   std::string* output) {
    if (value.size() > limit - output->size()) {
        throw TransliterationError("Transliterated text exceeds output limit");
    }
    output->append(value);
}

template <std::size_t MappingCount>
std::optional<std::string> Transliterate(
    std::string_view text, const Mapping (&mappings)[MappingCount],
    std::size_t output_limit, bool case_sensitive = true) {
    if (text.size() > kMaximumTransliterationInputBytes ||
        !foundation::IsValidUtf8(text) || text.find('\0') != text.npos) {
        throw TransliterationError(
            "Transliteration input is not valid bounded UTF-8");
    }

    std::string folded;
    if (!case_sensitive) {
        try {
            folded = foundation::FoldSimpleCase(text);
        } catch (const foundation::TextFoldingError& error) {
            throw TransliterationError(error.what());
        }
    }
    const std::string_view target = case_sensitive ? text : folded;
    std::string output;
    output.reserve(target.size());
    for (std::size_t position = 0; position < target.size();) {
        const Mapping* best = nullptr;
        std::size_t best_length = 0;
        for (const auto& mapping : mappings) {
            const std::size_t mapping_length = CodePointCount(mapping.source);
            if ((best == nullptr || mapping_length > best_length) &&
                target.substr(position, mapping.source.size()) ==
                    mapping.source) {
                best = &mapping;
                best_length = mapping_length;
            }
        }
        if (best != nullptr) {
            AppendBounded(best->target, output_limit, &output);
            position += best->source.size();
        } else {
            std::size_t byte_count = 1U;
            while (position + byte_count < target.size() &&
                   (static_cast<unsigned char>(target[position + byte_count]) &
                    0xc0U) == 0x80U) {
                ++byte_count;
            }
            AppendBounded(target.substr(position, byte_count), output_limit,
                          &output);
            position += byte_count;
        }
    }
    if (output == target) {
        return std::nullopt;
    }
    return output;
}

template <std::size_t MappingCount>
TransliterationMappingCounts MappingCounts(
    const Mapping (&mappings)[MappingCount]) {
    std::unordered_set<std::string_view> sources;
    for (const auto& mapping : mappings) {
        sources.insert(mapping.source);
    }
    return {MappingCount, sources.size()};
}

}  // namespace

std::optional<std::string> TransliterateRussian(std::string_view text,
                                                std::size_t output_limit) {
    return Transliterate(text, kRussianMappings, output_limit);
}

std::optional<std::string> TransliterateGerman(std::string_view text,
                                               std::size_t output_limit) {
    return Transliterate(text, kGermanMappings, output_limit);
}

std::optional<std::string> TransliterateGreek(std::string_view text,
                                              std::size_t output_limit) {
    return Transliterate(text, kGreekMappings, output_limit);
}

std::optional<std::string> TransliterateBelarusianLatinClassic(
    std::string_view text, std::size_t output_limit) {
    return Transliterate(text, kBelarusianLatinToClassicMappings, output_limit,
                         false);
}

std::optional<std::string> TransliterateBelarusianLatinSchool(
    std::string_view text, std::size_t output_limit) {
    return Transliterate(text, kBelarusianLatinToSchoolMappings, output_limit,
                         false);
}

std::optional<std::string> TransliterateBelarusianSchoolClassic(
    std::string_view text, std::size_t output_limit) {
    return Transliterate(text, kBelarusianSchoolToClassicMappings, output_limit,
                         false);
}

TransliterationMappingCounts BelarusianLatinClassicMappingCounts() {
    return MappingCounts(kBelarusianLatinToClassicMappings);
}

TransliterationMappingCounts BelarusianLatinSchoolMappingCounts() {
    return MappingCounts(kBelarusianLatinToSchoolMappings);
}

TransliterationMappingCounts BelarusianSchoolClassicMappingCounts() {
    return MappingCounts(kBelarusianSchoolToClassicMappings);
}

}  // namespace goldendict::core::dictionary
