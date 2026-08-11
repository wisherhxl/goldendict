// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FOUNDATION_TEXT_ENCODING_H_
#define GOLDENDICT_CORE_SRC_FOUNDATION_TEXT_ENCODING_H_

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace goldendict::core::foundation {

class TextEncodingError final : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

std::string DecodeToUtf8(std::string_view input, std::string_view encoding,
                         std::size_t maximum_output_bytes);
std::string EncodeFromUtf8(std::string_view input, std::string_view encoding,
                           std::size_t maximum_output_bytes);

}  // namespace goldendict::core::foundation

#endif  // GOLDENDICT_CORE_SRC_FOUNDATION_TEXT_ENCODING_H_
