// SPDX-License-Identifier: GPL-3.0-or-later

#include "legacy_language_pair.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string_view>

#include <unicode/ucasemap.h>

namespace goldendict::core::foundation {
namespace {
using LegacyLanguageCode = std::pair<std::string_view, std::string_view>;
constexpr LegacyLanguageCode kLegacyLanguageCodes[] = {
#include "legacy_language_codes.inc"
};

std::string FoldAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return character >= 'A' && character <= 'Z'
                                  ? static_cast<char>(character - 'A' + 'a')
                                  : static_cast<char>(character);
                   });
    return value;
}

std::string FoldUnicodeCase(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() >
        static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return FoldAscii(std::string(value));
    }
    UErrorCode status = U_ZERO_ERROR;
    UCaseMap* case_map = ucasemap_open(nullptr, U_FOLD_CASE_DEFAULT, &status);
    if (U_FAILURE(status) || case_map == nullptr) {
        return FoldAscii(std::string(value));
    }
    status = U_ZERO_ERROR;
    const auto source_size = static_cast<std::int32_t>(value.size());
    const auto output_size = ucasemap_utf8FoldCase(
        case_map, nullptr, 0, value.data(), source_size, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        ucasemap_close(case_map);
        return FoldAscii(std::string(value));
    }
    status = U_ZERO_ERROR;
    std::string output(static_cast<std::size_t>(output_size), '\0');
    const auto written =
        ucasemap_utf8FoldCase(case_map, output.data(), output_size,
                              value.data(), source_size, &status);
    ucasemap_close(case_map);
    if (U_FAILURE(status)) {
        return FoldAscii(std::string(value));
    }
    output.resize(static_cast<std::size_t>(written));
    return output;
}

bool IsAsciiLetter(char character) noexcept {
    return character >= 'a' && character <= 'z';
}

std::string GuessLegacyLanguage(std::string_view token) {
    if (token.size() == 3U) {
        const auto found = std::find_if(
            std::begin(kLegacyLanguageCodes), std::end(kLegacyLanguageCodes),
            [token](const LegacyLanguageCode& entry) {
                return entry.first == token;
            });
        if (found != std::end(kLegacyLanguageCodes)) {
            return std::string(found->second);
        }
    }
    // Frozen Qt 5 accepts any captured token and falls back to its first two
    // letters when the exact three-letter mapping is unknown.
    return std::string(token.substr(0U, 2U));
}

}  // namespace

std::pair<std::string, std::string> InferLegacyLanguagePair(std::string name) {
    name = "|" + FoldUnicodeCase(name) + "|";
    for (std::size_t boundary = 0U; boundary < name.size(); ++boundary) {
        if (IsAsciiLetter(name[boundary])) {
            continue;
        }
        for (const std::size_t source_size : {2U, 3U}) {
            const auto source = boundary + 1U;
            const auto separator = source + source_size;
            if (separator >= name.size() || name[separator] != '-' ||
                !std::all_of(
                    name.begin() + static_cast<std::ptrdiff_t>(source),
                    name.begin() + static_cast<std::ptrdiff_t>(separator),
                    IsAsciiLetter)) {
                continue;
            }
            for (const std::size_t target_size : {2U, 3U}) {
                const auto target = separator + 1U;
                const auto end = target + target_size;
                if (end >= name.size() || IsAsciiLetter(name[end]) ||
                    !std::all_of(
                        name.begin() + static_cast<std::ptrdiff_t>(target),
                        name.begin() + static_cast<std::ptrdiff_t>(end),
                        IsAsciiLetter)) {
                    continue;
                }
                auto languages =
                    std::pair{GuessLegacyLanguage(std::string_view(name).substr(
                                  source, source_size)),
                              GuessLegacyLanguage(std::string_view(name).substr(
                                  target, target_size))};
                if (!languages.first.empty() && !languages.second.empty()) {
                    return languages;
                }
            }
        }
    }
    return {};
}

}  // namespace goldendict::core::foundation
