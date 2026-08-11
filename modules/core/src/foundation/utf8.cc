// SPDX-License-Identifier: GPL-3.0-or-later

#include "utf8.h"

#include <cstddef>
#include <cstdint>

namespace goldendict::core::foundation {
namespace {

bool IsContinuation(std::uint8_t byte) noexcept {
    return (byte & 0xc0U) == 0x80U;
}

}  // namespace

bool IsValidUtf8(std::string_view text) noexcept {
    std::size_t position = 0;
    while (position < text.size()) {
        const auto first = static_cast<std::uint8_t>(text[position]);
        if (first <= 0x7fU) {
            ++position;
            continue;
        }

        std::size_t length = 0;
        std::uint32_t code_point = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xe0U) == 0xc0U) {
            length = 2;
            code_point = first & 0x1fU;
            minimum = 0x80U;
        } else if ((first & 0xf0U) == 0xe0U) {
            length = 3;
            code_point = first & 0x0fU;
            minimum = 0x800U;
        } else if ((first & 0xf8U) == 0xf0U) {
            length = 4;
            code_point = first & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }
        if (length > text.size() - position) {
            return false;
        }
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto continuation =
                static_cast<std::uint8_t>(text[position + offset]);
            if (!IsContinuation(continuation)) {
                return false;
            }
            code_point = (code_point << 6U) | (continuation & 0x3fU);
        }
        if (code_point < minimum || code_point > 0x10ffffU ||
            (code_point >= 0xd800U && code_point <= 0xdfffU)) {
            return false;
        }
        position += length;
    }
    return true;
}

}  // namespace goldendict::core::foundation
