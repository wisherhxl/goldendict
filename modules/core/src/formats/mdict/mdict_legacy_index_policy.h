// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_LEGACY_INDEX_POLICY_H_
#define GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_LEGACY_INDEX_POLICY_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <unicode/utf8.h>

#include "../../foundation/legacy_folding_policy.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::mdict::detail {

// QString::trimmed() runs before the frozen index builder. Qt 5 additionally
// treats vertical tab and form feed as space, but no longer treats U+180E as
// one. The second legacy-index trim below still handles U+180E.
inline constexpr std::array<UChar32, 25> kQt5StringWhitespace{
    0x9U,    0xAU,    0xBU,    0xCU,    0xDU,    0x20U,   0x85U,
    0xA0U,   0x1680U, 0x2000U, 0x2001U, 0x2002U, 0x2003U, 0x2004U,
    0x2005U, 0x2006U, 0x2007U, 0x2008U, 0x2009U, 0x200AU, 0x2028U,
    0x2029U, 0x202FU, 0x205FU, 0x3000U,
};

inline bool IsLegacyWhitespace(UChar32 code_point) noexcept {
    return foundation::legacy::IsWhitespace(code_point);
}

inline bool IsLegacyPunctuation(UChar32 code_point) noexcept {
    return foundation::legacy::IsPunctuation(code_point);
}

template <typename Predicate>
std::string_view TrimWhitespace(std::string_view word,
                                Predicate is_whitespace) noexcept {
    std::int32_t begin = 0;
    const auto length = static_cast<std::int32_t>(word.size());
    while (begin < length) {
        std::int32_t next = begin;
        UChar32 code_point = 0;
        U8_NEXT(word.data(), next, length, code_point);
        if (code_point < 0 || !is_whitespace(code_point))
            break;
        begin = next;
    }

    std::int32_t end = length;
    while (end > begin) {
        std::int32_t previous = end;
        UChar32 code_point = 0;
        U8_PREV(word.data(), 0, previous, code_point);
        if (code_point < 0 || !is_whitespace(code_point))
            break;
        end = previous;
    }
    return word.substr(static_cast<std::size_t>(begin),
                       static_cast<std::size_t>(end - begin));
}

inline std::string_view TrimQt5StringWhitespace(
    std::string_view word) noexcept {
    return TrimWhitespace(word, [](UChar32 code_point) {
        return std::binary_search(kQt5StringWhitespace.begin(),
                                  kQt5StringWhitespace.end(), code_point);
    });
}

inline std::string_view TrimLegacyWhitespace(std::string_view word) noexcept {
    return TrimWhitespace(word, foundation::legacy::IsWhitespace);
}

inline std::string StripLegacyWhitespace(std::string_view word) {
    std::string result;
    result.reserve(word.size());
    std::int32_t position = 0;
    const auto length = static_cast<std::int32_t>(word.size());
    while (position < length) {
        const std::int32_t begin = position;
        UChar32 code_point = 0;
        U8_NEXT(word.data(), position, length, code_point);
        if (code_point >= 0 && !foundation::legacy::IsWhitespace(code_point)) {
            result.append(
                word.substr(static_cast<std::size_t>(begin),
                            static_cast<std::size_t>(position - begin)));
        }
    }
    return result;
}

inline std::string FoldForLegacyMdictLookup(std::string_view word) {
    std::string folded = foundation::FoldForLookupWithSeparatorPolicy(
        word, foundation::legacy::IsSeparator);
    return folded.empty() ? StripLegacyWhitespace(word) : folded;
}

}  // namespace goldendict::core::formats::mdict::detail

#endif  // GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_LEGACY_INDEX_POLICY_H_
