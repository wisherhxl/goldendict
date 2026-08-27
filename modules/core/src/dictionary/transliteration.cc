// SPDX-License-Identifier: GPL-3.0-or-later

#include "transliteration.h"

#include <string_view>

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

void AppendBounded(std::string_view value, std::size_t limit,
                   std::string* output) {
    if (value.size() > limit - output->size()) {
        throw TransliterationError("Transliterated text exceeds output limit");
    }
    output->append(value);
}

}  // namespace

std::optional<std::string> TransliterateRussian(std::string_view text,
                                                std::size_t output_limit) {
    if (text.size() > kMaximumTransliterationInputBytes ||
        !foundation::IsValidUtf8(text) || text.find('\0') != text.npos) {
        throw TransliterationError(
            "Transliteration input is not valid bounded UTF-8");
    }

    std::string output;
    output.reserve(text.size());
    for (std::size_t position = 0; position < text.size();) {
        const Mapping* best = nullptr;
        for (const auto& mapping : kRussianMappings) {
            if ((best == nullptr ||
                 mapping.source.size() > best->source.size()) &&
                text.substr(position, mapping.source.size()) ==
                    mapping.source) {
                best = &mapping;
            }
        }
        if (best != nullptr) {
            AppendBounded(best->target, output_limit, &output);
            position += best->source.size();
        } else {
            AppendBounded(text.substr(position, 1U), output_limit, &output);
            ++position;
        }
    }
    if (output == text) {
        return std::nullopt;
    }
    return output;
}

}  // namespace goldendict::core::dictionary
