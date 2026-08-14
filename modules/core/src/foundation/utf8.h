// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FOUNDATION_UTF8_H_
#define GOLDENDICT_CORE_SRC_FOUNDATION_UTF8_H_

#include <cstddef>
#include <string_view>

namespace goldendict::core::foundation {

bool IsValidUtf8(std::string_view text) noexcept;
std::size_t Utf8CodePointCount(std::string_view text) noexcept;
bool ContainsControlCharacter(std::string_view text) noexcept;

}  // namespace goldendict::core::foundation

#endif  // GOLDENDICT_CORE_SRC_FOUNDATION_UTF8_H_
