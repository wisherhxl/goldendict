// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_matcher.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <utility>

#include "../foundation/text_folding.h"

namespace goldendict::core::dictionary {
namespace {

void Check(const CancellationToken* cancellation,
           std::chrono::steady_clock::time_point deadline) {
    if (cancellation != nullptr && cancellation->IsCancellationRequested()) {
        throw FullTextMatcherError(FullTextMatcherErrorCode::kCancelled,
                                   "Full-text operation cancelled");
    }
    if (std::chrono::steady_clock::now() >= deadline) {
        throw FullTextMatcherError(FullTextMatcherErrorCode::kDeadlineExceeded,
                                   "Full-text operation deadline exceeded");
    }
}

std::string WildcardRegex(std::string_view pattern) {
    std::string result;
    for (char ch : pattern) {
        if (ch == '*')
            result += ".*";
        else if (ch == '?')
            result += '.';
        else {
            if (std::string_view(".^$|()[]{}+\\").find(ch) !=
                std::string_view::npos)
                result += '\\';
            result += ch;
        }
    }
    return result;
}

std::size_t Utf8CharacterBytes(unsigned char byte) {
    if ((byte & 0x80U) == 0U)
        return 1U;
    if ((byte & 0xe0U) == 0xc0U)
        return 2U;
    if ((byte & 0xf0U) == 0xe0U)
        return 3U;
    return 4U;
}

struct MappedText {
    std::string text;
    std::vector<std::pair<std::size_t, std::size_t>> original;
};

MappedText NormalizeMapped(std::string_view value, bool match_case,
                           bool ignore_diacritics,
                           const CancellationToken* cancellation,
                           std::chrono::steady_clock::time_point deadline) {
    MappedText result;
    for (std::size_t begin = 0U; begin < value.size();) {
        Check(cancellation, deadline);
        const auto end = std::min(
            value.size(), begin + Utf8CharacterBytes(static_cast<unsigned char>(
                                      value[begin])));
        const auto normalized =
            match_case
                ? std::string(value.substr(begin, end - begin))
                : foundation::NormalizeForExactLookup(
                      value.substr(begin, end - begin), ignore_diacritics);
        result.text += normalized;
        for (std::size_t i = 0U; i < normalized.size(); ++i)
            result.original.push_back({begin, end});
        begin = end;
    }
    return result;
}

struct Word {
    std::string_view text;
    std::size_t offset;
};

std::vector<Word> Words(std::string_view text) {
    std::vector<Word> words;
    for (std::size_t begin = 0U; begin < text.size();) {
        while (begin < text.size() &&
               std::isspace(static_cast<unsigned char>(text[begin])))
            ++begin;
        if (begin == text.size())
            break;
        std::size_t end = begin;
        while (end < text.size() &&
               !std::isspace(static_cast<unsigned char>(text[end])))
            ++end;
        words.push_back({text.substr(begin, end - begin), begin});
        begin = end;
    }
    return words;
}

std::optional<std::pair<std::size_t, std::size_t>> MatchWords(
    std::string_view haystack, std::string_view query, bool whole_words,
    bool ignore_order, std::optional<std::uint32_t> maximum_distance,
    const CancellationToken* cancellation,
    std::chrono::steady_clock::time_point deadline) {
    const auto source = Words(haystack);
    const auto wanted = Words(query);
    if (wanted.empty())
        return std::nullopt;
    const auto matches = [whole_words](std::string_view left,
                                       std::string_view right) {
        return whole_words ? left == right
                           : left.find(right) != std::string_view::npos;
    };
    for (std::size_t start = 0U; start < source.size(); ++start) {
        Check(cancellation, deadline);
        std::vector<std::size_t> selected;
        std::size_t cursor = start;
        bool found = true;
        for (const auto& term : wanted) {
            Check(cancellation, deadline);
            std::size_t position = source.size();
            const std::size_t begin = ignore_order ? start : cursor;
            for (std::size_t i = begin; i < source.size(); ++i) {
                if ((!ignore_order ||
                     std::find(selected.begin(), selected.end(), i) ==
                         selected.end()) &&
                    matches(source[i].text, term.text)) {
                    position = i;
                    cursor = i + 1U;
                    break;
                }
            }
            if (position == source.size()) {
                found = false;
                break;
            }
            selected.push_back(position);
        }
        if (!found)
            continue;
        std::sort(selected.begin(), selected.end());
        if (maximum_distance.has_value()) {
            for (std::size_t i = 1U; i < selected.size(); ++i) {
                if (selected[i] - selected[i - 1U] - 1U > *maximum_distance)
                    found = false;
            }
        }
        if (!found)
            continue;
        const auto first = source[selected.front()].offset;
        const auto last = source[selected.back()];
        return std::pair{first, last.offset + last.text.size() - first};
    }
    return std::nullopt;
}

}  // namespace

FullTextMatcherError::FullTextMatcherError(FullTextMatcherErrorCode code,
                                           std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

std::vector<FullTextMatcherRange> MatchFullText(
    std::string_view text, const FullTextMatcherOptions& options,
    const CancellationToken* cancellation,
    std::chrono::steady_clock::time_point deadline,
    std::optional<std::size_t> match_limit) {
    Check(cancellation, deadline);
    const auto mapped =
        NormalizeMapped(text, options.match_case, options.ignore_diacritics,
                        cancellation, deadline);
    Check(cancellation, deadline);
    const std::string needle =
        options.match_case ? std::string(options.query_text)
                           : foundation::NormalizeForExactLookup(
                                 options.query_text, options.ignore_diacritics);
    std::optional<std::regex> expression;
    try {
        if (options.mode == FullTextQueryMode::kWildcard)
            expression.emplace(WildcardRegex(needle));
        else if (options.mode == FullTextQueryMode::kRegularExpression)
            expression.emplace(needle);
    } catch (const std::regex_error&) {
        throw FullTextMatcherError(FullTextMatcherErrorCode::kMalformedPattern,
                                   "Malformed full-text pattern");
    }

    std::vector<FullTextMatcherRange> result;
    std::size_t cursor = 0U;
    std::size_t accepted_end = 0U;
    while (cursor < mapped.text.size()) {
        Check(cancellation, deadline);
        std::optional<std::pair<std::size_t, std::size_t>> matched;
        if (expression.has_value()) {
            std::match_results<std::string::const_iterator> match;
            const auto begin =
                mapped.text.cbegin() + static_cast<std::ptrdiff_t>(cursor);
            if (std::regex_search(begin, mapped.text.cend(), match,
                                  *expression)) {
                matched = std::pair{
                    cursor + static_cast<std::size_t>(match.position()),
                    static_cast<std::size_t>(match.length())};
            }
        } else {
            const auto word_match = MatchWords(
                std::string_view(mapped.text).substr(cursor), needle,
                options.mode == FullTextQueryMode::kWholeWords,
                options.ignore_word_order, options.maximum_word_distance,
                cancellation, deadline);
            if (word_match.has_value())
                matched =
                    std::pair{cursor + word_match->first, word_match->second};
        }
        Check(cancellation, deadline);
        if (!matched.has_value())
            break;
        if (matched->second == 0U) {
            const auto zero_offset = matched->first;
            cursor = zero_offset +
                     Utf8CharacterBytes(
                         static_cast<unsigned char>(mapped.text[zero_offset]));
            continue;
        }
        const auto normalized_offset = matched->first;
        const auto normalized_end = normalized_offset + matched->second;
        const auto original_offset = mapped.original[normalized_offset].first;
        const auto original_end = mapped.original[normalized_end - 1U].second;
        cursor = normalized_end;
        while (cursor < mapped.original.size() &&
               mapped.original[cursor].first < original_end) {
            ++cursor;
        }
        if (original_offset < accepted_end)
            continue;
        result.push_back({original_offset, original_end - original_offset});
        accepted_end = original_end;
        if (match_limit.has_value() && result.size() >= *match_limit)
            break;
    }
    return result;
}

}  // namespace goldendict::core::dictionary
