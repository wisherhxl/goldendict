// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FOUNDATION_TEXT_FOLDING_H_
#define GOLDENDICT_CORE_SRC_FOUNDATION_TEXT_FOLDING_H_

#include <stdexcept>
#include <string>
#include <string_view>

namespace goldendict::core::foundation {

class TextFoldingError final : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

using LookupSeparatorPredicate = bool (*)(char32_t code_point);

struct LegacyPrefixRankingForms {
    std::string simple_case;
    std::string full_case;
    std::string without_diacritics;
    std::string without_punctuation;
    std::string without_whitespace;
};

std::string FoldForLookup(std::string_view text);
// Retains the shared normalization, case, and mark rules while allowing a
// format compatibility boundary to provide frozen separator membership.
std::string FoldForLookupWithSeparatorPolicy(
    std::string_view text, LookupSeparatorPredicate is_separator);
std::string FoldSimpleCase(std::string_view text);
std::string NormalizeForExactLookup(std::string_view text,
                                    bool ignore_diacritics);
LegacyPrefixRankingForms FoldForLegacyPrefixRanking(std::string_view text);

}  // namespace goldendict::core::foundation

#endif  // GOLDENDICT_CORE_SRC_FOUNDATION_TEXT_FOLDING_H_
