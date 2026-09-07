// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace goldendict::core::image_codec {

// An empty result means undecodable input. Length violations throw
// std::length_error; request cancellation propagates from checkpoint.
std::vector<std::byte> DecodeToBmp(
    const std::vector<std::byte>& input, std::size_t maximum_bytes,
    const std::function<void()>& checkpoint);

}  // namespace goldendict::core::image_codec
