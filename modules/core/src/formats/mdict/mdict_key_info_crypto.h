// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_KEY_INFO_CRYPTO_H_
#define GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_KEY_INFO_CRYPTO_H_

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace goldendict::core::formats::mdict::detail {

std::array<std::uint8_t, 16> Ripemd128(std::string_view data);

bool DecryptKeyInfo(std::string* block);

}  // namespace goldendict::core::formats::mdict::detail

#endif  // GOLDENDICT_CORE_SRC_FORMATS_MDICT_MDICT_KEY_INFO_CRYPTO_H_
