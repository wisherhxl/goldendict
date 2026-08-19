// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_matcher.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <utility>

#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <unicode/utf16.h>
#include <unicode/utf8.h>

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

void RequireSuccess(UErrorCode status, const char* operation) {
    if (U_FAILURE(status))
        throw std::runtime_error(std::string(operation) + ": " +
                                 u_errorName(status));
}

bool IsMark(UChar32 value) {
    const auto category = static_cast<UCharCategory>(u_charType(value));
    return category == U_NON_SPACING_MARK ||
           category == U_COMBINING_SPACING_MARK || category == U_ENCLOSING_MARK;
}

bool IsAttachableBase(UChar32 value) {
    return !IsMark(value) && !u_isUWhiteSpace(value) && !u_ispunct(value);
}

struct Span {
    std::size_t begin = 0U;
    std::size_t end = 0U;
};

bool Intersects(Span left, Span right) {
    return left.begin < right.end && right.begin < left.end;
}

Span Union(Span left, Span right) {
    return {std::min(left.begin, right.begin), std::max(left.end, right.end)};
}

struct Token {
    UChar32 value = 0;
    Span origin;
};

std::vector<UChar> Utf16(UChar32 value) {
    if (value <= 0xffff)
        return {static_cast<UChar>(value)};
    return {U16_LEAD(value), U16_TRAIL(value)};
}

std::vector<UChar32> CodePoints(const UChar* value, std::int32_t length) {
    std::vector<UChar32> result;
    for (std::int32_t position = 0; position < length;) {
        UChar32 code_point = 0;
        U16_NEXT(value, position, length, code_point);
        result.push_back(code_point);
    }
    return result;
}

std::vector<UChar32> Decompose(const UNormalizer2* nfd, UChar32 value) {
    const auto input = Utf16(value);
    UErrorCode status = U_ZERO_ERROR;
    auto length = unorm2_normalize(nfd, input.data(),
                                   static_cast<std::int32_t>(input.size()),
                                   nullptr, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
        RequireSuccess(status, "Cannot size Unicode normalization");
    status = U_ZERO_ERROR;
    std::vector<UChar> output(static_cast<std::size_t>(length));
    length = unorm2_normalize(nfd, input.data(),
                              static_cast<std::int32_t>(input.size()),
                              output.data(), length, &status);
    RequireSuccess(status, "Cannot normalize Unicode text");
    return CodePoints(output.data(), length);
}

std::vector<UChar32> Fold(UChar32 value) {
    const auto input = Utf16(value);
    UErrorCode status = U_ZERO_ERROR;
    auto length = u_strFoldCase(nullptr, 0, input.data(),
                                static_cast<std::int32_t>(input.size()),
                                U_FOLD_CASE_DEFAULT, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
        RequireSuccess(status, "Cannot size Unicode case folding");
    status = U_ZERO_ERROR;
    std::vector<UChar> output(static_cast<std::size_t>(length));
    length = u_strFoldCase(output.data(), length, input.data(),
                           static_cast<std::int32_t>(input.size()),
                           U_FOLD_CASE_DEFAULT, &status);
    RequireSuccess(status, "Cannot fold Unicode case");
    return CodePoints(output.data(), length);
}

template <typename Transform>
std::vector<Token> Expand(const std::vector<Token>& input,
                          Transform transform) {
    std::vector<Token> output;
    for (const auto& token : input) {
        for (const auto value : transform(token.value))
            output.push_back({value, token.origin});
    }
    return output;
}

void CanonicallyOrder(std::vector<Token>* tokens) {
    std::size_t begin = 0U;
    for (std::size_t end = 0U; end <= tokens->size(); ++end) {
        if (end != tokens->size() &&
            u_getCombiningClass((*tokens)[end].value) != 0)
            continue;
        std::stable_sort(tokens->begin() + static_cast<std::ptrdiff_t>(begin),
                         tokens->begin() + static_cast<std::ptrdiff_t>(end),
                         [](const Token& left, const Token& right) {
                             return u_getCombiningClass(left.value) <
                                    u_getCombiningClass(right.value);
                         });
        begin = end + 1U;
    }
}

std::vector<Token> Compose(const UNormalizer2* nfc,
                           const std::vector<Token>& input) {
    std::vector<Token> output;
    std::optional<std::size_t> starter;
    std::uint8_t previous_class = 0U;
    for (const auto& token : input) {
        const auto combining_class = u_getCombiningClass(token.value);
        UChar32 composition = -1;
        if (starter.has_value() &&
            (previous_class == 0U || previous_class < combining_class)) {
            composition =
                unorm2_composePair(nfc, output[*starter].value, token.value);
        }
        if (composition >= 0) {
            output[*starter].value = composition;
            output[*starter].origin =
                Union(output[*starter].origin, token.origin);
            continue;
        }
        output.push_back(token);
        if (combining_class == 0U)
            starter = output.size() - 1U;
        previous_class = combining_class;
    }
    return output;
}

struct MappedText {
    std::string text;
    std::vector<Span> original;
    std::vector<std::size_t> boundaries;
};

MappedText NormalizeMapped(std::string_view value, bool match_case,
                           bool ignore_diacritics,
                           const CancellationToken* cancellation,
                           std::chrono::steady_clock::time_point deadline) {
    std::vector<Token> tokens;
    for (std::int32_t position = 0;
         position < static_cast<std::int32_t>(value.size());) {
        Check(cancellation, deadline);
        const auto begin = position;
        UChar32 code_point = 0;
        U8_NEXT(value.data(), position, static_cast<std::int32_t>(value.size()),
                code_point);
        tokens.push_back({code_point,
                          {static_cast<std::size_t>(begin),
                           static_cast<std::size_t>(position)}});
    }
    for (std::size_t index = 0U; index < tokens.size();) {
        if (!IsAttachableBase(tokens[index].value)) {
            ++index;
            continue;
        }
        std::size_t end = index + 1U;
        while (end < tokens.size() && IsMark(tokens[end].value))
            ++end;
        const Span cluster{tokens[index].origin.begin,
                           tokens[end - 1U].origin.end};
        for (std::size_t member = index; member < end; ++member)
            tokens[member].origin = cluster;
        index = end;
    }

    UErrorCode status = U_ZERO_ERROR;
    const auto* nfd = unorm2_getNFDInstance(&status);
    RequireSuccess(status, "Cannot initialize NFD normalization");
    status = U_ZERO_ERROR;
    const auto* nfc = unorm2_getNFCInstance(&status);
    RequireSuccess(status, "Cannot initialize NFC normalization");

    tokens =
        Expand(tokens, [nfd](UChar32 input) { return Decompose(nfd, input); });
    CanonicallyOrder(&tokens);
    if (!match_case)
        tokens = Expand(tokens, Fold);
    tokens =
        Expand(tokens, [nfd](UChar32 input) { return Decompose(nfd, input); });
    CanonicallyOrder(&tokens);
    if (ignore_diacritics) {
        tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
                                    [](const Token& token) {
                                        return IsMark(token.value);
                                    }),
                     tokens.end());
    }
    tokens = Compose(nfc, tokens);

    MappedText result;
    result.boundaries.push_back(0U);
    for (const auto& token : tokens) {
        char encoded[U8_MAX_LENGTH];
        std::int32_t length = 0;
        U8_APPEND_UNSAFE(encoded, length, token.value);
        result.text.append(encoded, static_cast<std::size_t>(length));
        result.original.insert(result.original.end(),
                               static_cast<std::size_t>(length), token.origin);
        result.boundaries.push_back(result.text.size());
    }
    return result;
}

std::size_t ScalarStart(const MappedText& mapped, std::size_t offset) {
    const auto found = std::upper_bound(mapped.boundaries.begin(),
                                        mapped.boundaries.end(), offset);
    return found == mapped.boundaries.begin() ? 0U : *(found - 1);
}

std::size_t ScalarEnd(const MappedText& mapped, std::size_t offset) {
    const auto found = std::lower_bound(mapped.boundaries.begin(),
                                        mapped.boundaries.end(), offset);
    return found == mapped.boundaries.end() ? mapped.text.size() : *found;
}

std::size_t NextScalar(const MappedText& mapped, std::size_t offset) {
    const auto found = std::upper_bound(mapped.boundaries.begin(),
                                        mapped.boundaries.end(), offset);
    return found == mapped.boundaries.end() ? mapped.text.size() : *found;
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

std::string NormalizeFullTextQuery(std::string_view text, bool match_case,
                                   bool ignore_diacritics) {
    return NormalizeMapped(text, match_case, ignore_diacritics, nullptr,
                           std::chrono::steady_clock::time_point::max())
        .text;
}

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
    const std::string needle = NormalizeFullTextQuery(
        options.query_text, options.match_case, options.ignore_diacritics);
    if (needle.empty())
        return {};
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
            cursor = NextScalar(mapped, matched->first);
            continue;
        }
        const auto normalized_offset = ScalarStart(mapped, matched->first);
        const auto normalized_end =
            ScalarEnd(mapped, matched->first + matched->second);
        Span original = mapped.original[normalized_offset];
        for (auto position = normalized_offset; position < normalized_end;
             ++position)
            original = Union(original, mapped.original[position]);
        cursor = normalized_end;
        while (cursor < mapped.original.size() &&
               Intersects(mapped.original[cursor], original))
            cursor = NextScalar(mapped, cursor);
        if (original.begin >= original.end || original.begin < accepted_end) {
            cursor = std::max(cursor, NextScalar(mapped, matched->first));
            continue;
        }
        result.push_back({original.begin, original.end - original.begin});
        accepted_end = original.end;
        if (match_limit.has_value() && result.size() >= *match_limit)
            break;
    }
    return result;
}

}  // namespace goldendict::core::dictionary
