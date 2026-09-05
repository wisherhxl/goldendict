// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_APPLICATION_LEGACY_SUGGESTION_RANKING_H_
#define GOLDENDICT_CORE_SRC_APPLICATION_LEGACY_SUGGESTION_RANKING_H_

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include <unicode/utf8.h>

#include "../foundation/legacy_folding_policy.h"
#include "../foundation/text_folding.h"
#include "../foundation/utf8.h"

namespace goldendict::core::application {

enum class LegacySuggestionCategory : std::uint8_t {
    kExact,
    kExactWithoutFullCase,
    kExactWithoutDiacritics,
    kExactWithoutPunctuation,
    kExactWithoutWhitespace,
    kExactInside,
    kExactWithoutDiacriticsInside,
    kExactWithoutPunctuationInside,
    kPrefix,
    kPrefixWithoutDiacritics,
    kPrefixWithoutPunctuation,
    kPrefixWithoutWhitespace,
    kWorst,
};

struct LegacySuggestionRank {
    LegacySuggestionCategory category = LegacySuggestionCategory::kWorst;
    std::uint8_t detail = 0U;
};

inline std::uint8_t SaturatedLegacySuggestionDetail(
    std::size_t value) noexcept {
    return static_cast<std::uint8_t>(std::min<std::size_t>(value, 255U));
}

inline std::optional<std::uint8_t> LegacySurroundedMatchPosition(
    std::string_view haystack, std::string_view needle) noexcept {
    if (needle.empty() || haystack.size() < needle.size()) {
        return std::nullopt;
    }
    std::size_t search_from = 0U;
    while (search_from <= haystack.size() - needle.size()) {
        const auto position = haystack.find(needle, search_from);
        if (position == std::string_view::npos) {
            return std::nullopt;
        }
        bool left_boundary = position == 0U;
        if (!left_boundary) {
            auto previous = static_cast<std::int32_t>(position);
            UChar32 code_point = 0;
            U8_PREV(haystack.data(), 0, previous, code_point);
            left_boundary = code_point >= 0 &&
                            foundation::legacy::IsSeparator(code_point);
        }
        const auto match_end = position + needle.size();
        bool right_boundary = match_end == haystack.size();
        if (!right_boundary) {
            auto next = static_cast<std::int32_t>(match_end);
            const auto length = static_cast<std::int32_t>(haystack.size());
            UChar32 code_point = 0;
            U8_NEXT(haystack.data(), next, length, code_point);
            right_boundary = code_point >= 0 &&
                             foundation::legacy::IsSeparator(code_point);
        }
        if (left_boundary && right_boundary) {
            return SaturatedLegacySuggestionDetail(
                foundation::Utf8CodePointCount(haystack.substr(0U, position)));
        }
        search_from = position + 1U;
    }
    return std::nullopt;
}

inline bool IsStrictLegacyPrefix(std::string_view candidate,
                                 std::string_view query) noexcept {
    return candidate.size() > query.size() &&
           candidate.compare(0U, query.size(), query) == 0;
}

inline LegacySuggestionRank RankLegacySuggestion(
    std::string_view headword,
    const foundation::LegacyPrefixRankingForms& query) {
    const auto candidate = foundation::FoldForLegacyPrefixRanking(headword);
    if (candidate.simple_case == query.simple_case) {
        return {LegacySuggestionCategory::kExact, 0U};
    }
    if (candidate.full_case == query.full_case) {
        return {LegacySuggestionCategory::kExactWithoutFullCase, 0U};
    }
    if (candidate.without_diacritics == query.without_diacritics) {
        return {LegacySuggestionCategory::kExactWithoutDiacritics, 0U};
    }
    if (candidate.without_punctuation == query.without_punctuation) {
        return {LegacySuggestionCategory::kExactWithoutPunctuation, 0U};
    }
    if (candidate.without_whitespace == query.without_whitespace) {
        return {LegacySuggestionCategory::kExactWithoutWhitespace, 0U};
    }
    if (const auto position =
            LegacySurroundedMatchPosition(candidate.simple_case,
                                          query.simple_case)) {
        return {LegacySuggestionCategory::kExactInside, *position};
    }
    if (const auto position = LegacySurroundedMatchPosition(
            candidate.without_diacritics, query.without_diacritics)) {
        return {LegacySuggestionCategory::kExactWithoutDiacriticsInside,
                *position};
    }
    if (const auto position = LegacySurroundedMatchPosition(
            candidate.without_punctuation, query.without_punctuation)) {
        return {LegacySuggestionCategory::kExactWithoutPunctuationInside,
                *position};
    }
    const auto length = SaturatedLegacySuggestionDetail(
        foundation::Utf8CodePointCount(candidate.simple_case));
    if (IsStrictLegacyPrefix(candidate.simple_case, query.simple_case)) {
        return {LegacySuggestionCategory::kPrefix, length};
    }
    if (IsStrictLegacyPrefix(candidate.without_diacritics,
                             query.without_diacritics)) {
        return {LegacySuggestionCategory::kPrefixWithoutDiacritics, length};
    }
    if (IsStrictLegacyPrefix(candidate.without_punctuation,
                             query.without_punctuation)) {
        return {LegacySuggestionCategory::kPrefixWithoutPunctuation, length};
    }
    if (IsStrictLegacyPrefix(candidate.without_whitespace,
                             query.without_whitespace)) {
        return {LegacySuggestionCategory::kPrefixWithoutWhitespace, length};
    }
    return {LegacySuggestionCategory::kWorst, 0U};
}

inline bool LegacySuggestionLess(const LegacySuggestionRank& left_rank,
                                 std::string_view left_headword,
                                 const LegacySuggestionRank& right_rank,
                                 std::string_view right_headword) noexcept {
    if (left_rank.category != right_rank.category) {
        return left_rank.category < right_rank.category;
    }
    if (left_rank.detail != right_rank.detail) {
        return left_rank.detail < right_rank.detail;
    }
    return left_headword < right_headword;
}

}  // namespace goldendict::core::application

#endif  // GOLDENDICT_CORE_SRC_APPLICATION_LEGACY_SUGGESTION_RANKING_H_
