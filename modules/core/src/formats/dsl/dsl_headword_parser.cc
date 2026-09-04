// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsl_headword_parser.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

#include <unicode/uchar.h>
#include <unicode/utf8.h>

#include "../../foundation/utf8.h"

namespace goldendict::core::formats::dsl::headword {
namespace {

std::size_t NextCodePoint(std::string_view value, std::size_t index) {
    if (index >= value.size()) {
        return value.size();
    }
    ++index;
    while (index < value.size() &&
           (static_cast<unsigned char>(value[index]) & 0xc0U) == 0x80U) {
        ++index;
    }
    return index;
}

char32_t CodePointAt(std::string_view value, std::size_t index,
                     std::size_t* next) {
    auto position = static_cast<std::int32_t>(index);
    UChar32 code_point = 0;
    U8_NEXT(value.data(), position, static_cast<std::int32_t>(value.size()),
            code_point);
    *next = static_cast<std::size_t>(position);
    return static_cast<char32_t>(code_point);
}

bool IsLegacyWhitespace(char32_t code_point) {
    switch (code_point) {
        case U'\n':
        case U'\r':
        case U'\t':
        case U'\u0020':
        case U'\u00a0':
        case U'\u1680':
        case U'\u180e':
        case U'\u2000':
        case U'\u2001':
        case U'\u2002':
        case U'\u2003':
        case U'\u2004':
        case U'\u2005':
        case U'\u2006':
        case U'\u2007':
        case U'\u2008':
        case U'\u2009':
        case U'\u200a':
        case U'\u2028':
        case U'\u2029':
        case U'\u202f':
        case U'\u205f':
        case U'\u3000':
            return true;
        default:
            return false;
    }
}

std::string TrimLegacyWhitespace(std::string_view value) {
    std::size_t first = value.size();
    std::size_t last = 0U;
    for (std::size_t index = 0U; index < value.size();) {
        std::size_t next = index;
        const char32_t code_point = CodePointAt(value, index, &next);
        if (!IsLegacyWhitespace(code_point)) {
            if (first == value.size()) {
                first = index;
            }
            last = next;
        }
        index = next;
    }
    return first == value.size()
               ? std::string{}
               : std::string(value.substr(first, last - first));
}

std::string ToggleFirstCharacterCase(std::string value) {
    if (value.empty()) {
        return value;
    }
    std::size_t first_end = 0U;
    const auto first = static_cast<UChar32>(CodePointAt(value, 0U, &first_end));
    // Qt 5 tested upper-case state through QChar's 16-bit constructor even
    // though the stored legacy character was a full code point.
    const auto legacy_unit = static_cast<UChar32>(first & 0xffff);
    const UChar32 toggled =
        u_isupper(legacy_unit) ? u_tolower(first) : u_toupper(first);
    std::array<char, U8_MAX_LENGTH> encoded{};
    auto encoded_size = std::int32_t{0};
    U8_APPEND_UNSAFE(encoded.data(), encoded_size, toggled);
    value.replace(0U, first_end, encoded.data(),
                  static_cast<std::size_t>(encoded_size));
    return value;
}

std::string ProcessUnsortedParts(std::string value, bool strip) {
    int depth = 0;
    std::size_t start = 0U;
    for (std::size_t index = 0U; index < value.size();) {
        if (value[index] == '\\') {
            index = NextCodePoint(value, index + 1U);
            continue;
        }
        if (value[index] == '{') {
            ++depth;
            if (!strip) {
                value.erase(index, 1U);
                continue;
            }
            if (depth == 1) {
                start = index;
            }
        } else if (value[index] == '}') {
            if (--depth < 0) {
                depth = 0;
                value.erase(index, 1U);
                continue;
            }
            if (!strip) {
                value.erase(index, 1U);
                continue;
            }
            if (depth == 0) {
                value.erase(start, index - start + 1U);
                index = start;
                continue;
            }
        }
        index = NextCodePoint(value, index);
    }
    if (strip && depth != 0) {
        value.erase(start);
    }
    return value;
}

void MergeLegacyExpansions(std::vector<std::string>* destination,
                           std::vector<std::string> source) {
    // Qt 5 merged each source line without sorting or removing duplicates.
    std::vector<std::string> merged;
    merged.reserve(destination->size() + source.size());
    auto destination_item = destination->begin();
    auto source_item = source.begin();
    while (destination_item != destination->end() &&
           source_item != source.end()) {
        if (*source_item < *destination_item) {
            merged.push_back(std::move(*source_item++));
        } else {
            merged.push_back(std::move(*destination_item++));
        }
    }
    std::move(destination_item, destination->end(), std::back_inserter(merged));
    std::move(source_item, source.end(), std::back_inserter(merged));
    *destination = std::move(merged);
}

void ExpandOptional(std::string value, std::vector<std::string>* result,
                    std::size_t start = 0U, bool inside_recursion = false) {
    if (inside_recursion && result->size() >= 32U) {
        return;
    }
    // The legacy guard counts decoded characters rather than UTF-8 bytes.
    if (foundation::Utf8CodePointCount(value) > 500U) {
        result->push_back(std::move(value));
        return;
    }

    std::vector<std::string> expanded;
    auto* headwords = inside_recursion ? result : &expanded;
    for (std::size_t index = start; index < value.size();) {
        if (value[index] == '\\') {
            index = NextCodePoint(value, index + 1U);
            continue;
        }
        if (value[index] == '(') {
            int depth = 1;
            std::size_t end = index + 1U;
            for (; end < value.size();) {
                if (value[end] == '\\') {
                    end = NextCodePoint(value, end + 1U);
                    continue;
                }
                if (value[end] == '(') {
                    ++depth;
                } else if (value[end] == ')' && --depth == 0) {
                    if (end != index + 1U) {
                        std::string removed = value.substr(0U, index);
                        removed.append(value, end + 1U, std::string::npos);
                        ExpandOptional(std::move(removed), headwords, index,
                                       true);
                    }
                    break;
                }
                end = NextCodePoint(value, end);
            }
            if (depth != 0 && index != value.size() - 1U) {
                if (headwords->size() < 32U) {
                    headwords->push_back(value.substr(0U, index));
                } else {
                    if (!inside_recursion) {
                        MergeLegacyExpansions(result, std::move(expanded));
                    }
                    return;
                }
            }
            value.erase(index, 1U);
            continue;
        }
        if (value[index] == ')') {
            value.erase(index, 1U);
            continue;
        }
        index = NextCodePoint(value, index);
    }

    if (headwords->size() < 32U) {
        headwords->push_back(std::move(value));
    }
    if (!inside_recursion) {
        MergeLegacyExpansions(result, std::move(expanded));
    }
}

std::string Unescape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\\' && index + 1U < value.size()) {
            ++index;
        } else if (value[index] == '\\') {
            continue;
        }
        result.push_back(value[index]);
    }
    return result;
}

std::string Normalize(std::string value) {
    value.erase(std::unique(value.begin(), value.end(),
                            [](char left, char right) {
                                return left == ' ' && right == ' ';
                            }),
                value.end());
    if (!value.empty() && value.back() == ' ') {
        value.pop_back();
    }
    if (!value.empty() && value.front() == ' ') {
        value.erase(value.begin());
    }
    return value;
}

}  // namespace

std::string ReplaceTildes(std::string value, std::string_view primary) {
    const std::string replacement = TrimLegacyWhitespace(primary);
    for (std::size_t index = 0U; index < value.size();) {
        if (value[index] == '\\') {
            index = NextCodePoint(value, index + 1U);
        } else if (value[index] == '~') {
            if (index > 0U && value[index - 1U] == '^' &&
                (index < 2U || value[index - 2U] != '\\')) {
                const std::string toggled =
                    ToggleFirstCharacterCase(replacement);
                value.replace(index - 1U, 2U, toggled);
                index = index - 1U + toggled.size();
            } else {
                value.replace(index, 1U, replacement);
                index += replacement.size();
            }
        } else {
            index = NextCodePoint(value, index);
        }
    }
    return value;
}

Expansion Parse(const std::vector<std::string>& source_lines) {
    Expansion expansion;
    if (source_lines.empty()) {
        return expansion;
    }
    std::string article_tilde =
        ProcessUnsortedParts(source_lines.front(), false);
    std::vector<std::string> article_tilde_expansions;
    ExpandOptional(article_tilde, &article_tilde_expansions);
    expansion.article_tilde = article_tilde_expansions.empty()
                                  ? std::move(article_tilde)
                                  : article_tilde_expansions.front();

    ExpandOptional(ProcessUnsortedParts(source_lines.front(), true),
                   &expansion.records);
    if (expansion.records.empty()) {
        return expansion;
    }
    for (const auto& record : expansion.records) {
        std::string candidate = Normalize(Unescape(record));
        if (!candidate.empty()) {
            expansion.primary = std::move(candidate);
            break;
        }
    }
    for (std::size_t index = 1U; index < source_lines.size(); ++index) {
        std::string alternate = ProcessUnsortedParts(source_lines[index], true);
        alternate =
            ReplaceTildes(std::move(alternate), expansion.records.front());
        ExpandOptional(std::move(alternate), &expansion.records);
    }
    for (auto& record : expansion.records) {
        record = Normalize(Unescape(record));
    }
    return expansion;
}

}  // namespace goldendict::core::formats::dsl::headword
