// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2007 Michael Niedermayer <michaelni@gmx.at>
// Copyright (C) 2013 James Almer <jamrial@gmail.com>
// Copyright (C) 2015 Zhe Wang <0x1998@gmail.com>

#include "mdict_key_info_crypto.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

namespace goldendict::core::formats::mdict::detail {
namespace {

constexpr std::array<std::uint32_t, 4> kLeftConstants{
    0x5a827999U, 0x6ed9eba1U, 0x8f1bbcdcU, 0xa953fd4eU};
constexpr std::array<std::uint32_t, 4> kRightConstants{
    0x50a28be6U, 0x5c4dd124U, 0x6d703ef3U, 0x7a6d76e9U};
constexpr std::array<unsigned, 64> kLeftRotations{
    11, 14, 15, 12, 5,  8,  7,  9,  11, 13, 14, 15, 6,  7,  9,  8,
    7,  6,  8,  13, 11, 9,  7,  15, 7,  12, 15, 9,  11, 7,  13, 12,
    11, 13, 6,  7,  14, 9,  13, 15, 14, 8,  13, 6,  5,  12, 7,  5,
    11, 12, 14, 15, 14, 15, 9,  8,  9,  14, 5,  6,  8,  6,  5,  12};
constexpr std::array<unsigned, 64> kRightRotations{
    8,  9,  9,  11, 13, 15, 15, 5,  7,  7,  8,  11, 14, 14, 12, 6,
    9,  13, 15, 7,  12, 8,  9,  11, 7,  7,  12, 7,  6,  15, 13, 11,
    9,  7,  15, 11, 8,  6,  6,  14, 12, 13, 5,  14, 13, 13, 7,  5,
    15, 5,  8,  11, 14, 14, 6,  14, 6,  9,  12, 9,  12, 5,  15, 8};
constexpr std::array<unsigned, 64> kLeftWords{
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    7,  4,  13, 1,  10, 6,  15, 3,  12, 0,  9,  5,  2,  14, 11, 8,
    3,  10, 14, 4,  9,  15, 8,  1,  2,  7,  0,  6,  13, 11, 5,  12,
    1,  9,  11, 10, 0,  8,  12, 4,  13, 3,  7,  15, 14, 5,  6,  2};
constexpr std::array<unsigned, 64> kRightWords{
    5,  14, 7,  0,  9,  2,  11, 4,  13, 6,  15, 8,  1,  10, 3,  12,
    6,  11, 3,  7,  0,  13, 5,  10, 14, 15, 8,  12, 4,  9,  1,  2,
    15, 5,  1,  3,  7,  14, 6,  9,  11, 8,  12, 2,  10, 0,  4,  13,
    8,  6,  4,  1,  3,  11, 15, 0,  5,  12, 2,  13, 9,  7,  10, 14};

std::uint32_t RotateLeft(std::uint32_t value, unsigned bits) {
    return (value << bits) | (value >> (32U - bits));
}

std::uint32_t ReadLittle32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8U) |
           (static_cast<std::uint32_t>(data[2]) << 16U) |
           (static_cast<std::uint32_t>(data[3]) << 24U);
}

std::uint32_t LeftFunction(unsigned round, std::uint32_t x, std::uint32_t y,
                           std::uint32_t z) {
    switch (round) {
        case 0:
            return x ^ y ^ z;
        case 1:
            return ((y ^ z) & x) ^ z;
        case 2:
            return (~y | x) ^ z;
        default:
            return ((x ^ y) & z) ^ y;
    }
}

std::uint32_t RightFunction(unsigned round, std::uint32_t x,
                            std::uint32_t y, std::uint32_t z) {
    switch (round) {
        case 0:
            return ((x ^ y) & z) ^ y;
        case 1:
            return (~y | x) ^ z;
        case 2:
            return ((y ^ z) & x) ^ z;
        default:
            return x ^ y ^ z;
    }
}

class Ripemd128State final {
   public:
    void Update(const std::uint8_t* data, std::size_t size) {
        const std::size_t buffered = count_ & 63U;
        count_ += size;
        std::size_t position = 0;
        if (buffered + size >= buffer_.size()) {
            const std::size_t prefix = buffer_.size() - buffered;
            std::memcpy(buffer_.data() + buffered, data, prefix);
            Transform(buffer_.data());
            position = prefix;
            while (position + buffer_.size() <= size) {
                Transform(data + position);
                position += buffer_.size();
            }
        }
        if (position < size) {
            const std::size_t destination = (buffered + position) & 63U;
            std::memcpy(buffer_.data() + destination, data + position,
                        size - position);
        }
    }

    std::array<std::uint8_t, 16> Finish() {
        const std::uint64_t bits = count_ << 3U;
        const std::uint8_t marker = 0x80U;
        const std::uint8_t zero = 0U;
        Update(&marker, 1U);
        while ((count_ & 63U) != 56U)
            Update(&zero, 1U);
        std::array<std::uint8_t, 8> length{};
        for (std::size_t index = 0; index < length.size(); ++index)
            length[index] = static_cast<std::uint8_t>(bits >> (index * 8U));
        Update(length.data(), length.size());
        std::array<std::uint8_t, 16> digest{};
        for (std::size_t word = 0; word < state_.size(); ++word) {
            for (std::size_t byte = 0; byte < 4U; ++byte) {
                digest[word * 4U + byte] = static_cast<std::uint8_t>(
                    state_[word] >> (byte * 8U));
            }
        }
        return digest;
    }

   private:
    void Transform(const std::uint8_t* data) {
        std::array<std::uint32_t, 16> words{};
        for (std::size_t index = 0; index < words.size(); ++index)
            words[index] = ReadLittle32(data + index * 4U);
        std::uint32_t left_a = state_[0];
        std::uint32_t left_b = state_[1];
        std::uint32_t left_c = state_[2];
        std::uint32_t left_d = state_[3];
        std::uint32_t right_a = left_a;
        std::uint32_t right_b = left_b;
        std::uint32_t right_c = left_c;
        std::uint32_t right_d = left_d;
        for (unsigned index = 0; index < 64U; ++index) {
            const unsigned round = index / 16U;
            const std::uint32_t next_left = RotateLeft(
                left_a + LeftFunction(round, left_b, left_c, left_d) +
                    words[kLeftWords[index]] +
                    (round == 0U ? 0U : kLeftConstants[round - 1U]),
                kLeftRotations[index]);
            left_a = left_d;
            left_d = left_c;
            left_c = left_b;
            left_b = next_left;

            const std::uint32_t next_right = RotateLeft(
                right_a + RightFunction(round, right_b, right_c, right_d) +
                    words[kRightWords[index]] +
                    (round == 3U ? 0U : kRightConstants[round]),
                kRightRotations[index]);
            right_a = right_d;
            right_d = right_c;
            right_c = right_b;
            right_b = next_right;
        }
        const std::uint32_t merged = state_[1] + left_c + right_d;
        state_[1] = state_[2] + left_d + right_a;
        state_[2] = state_[3] + left_a + right_b;
        state_[3] = state_[0] + left_b + right_c;
        state_[0] = merged;
    }

    std::uint64_t count_ = 0;
    std::array<std::uint8_t, 64> buffer_{};
    std::array<std::uint32_t, 4> state_{0x67452301U, 0xefcdab89U,
                                        0x98badcfeU, 0x10325476U};
};

}  // namespace

std::array<std::uint8_t, 16> Ripemd128(std::string_view data) {
    Ripemd128State state;
    state.Update(reinterpret_cast<const std::uint8_t*>(data.data()),
                 data.size());
    return state.Finish();
}

bool DecryptKeyInfo(std::string* block) {
    if (block == nullptr || block->size() < 8U)
        return false;
    std::array<char, 8> seed{};
    std::copy_n(block->data() + 4U, 4U, seed.data());
    seed[4] = static_cast<char>(0x95U);
    seed[5] = static_cast<char>(0x36U);
    const auto key = Ripemd128(std::string_view(seed.data(), seed.size()));
    std::uint8_t previous = 0x36U;
    for (std::size_t index = 8U; index < block->size(); ++index) {
        const auto encrypted = static_cast<std::uint8_t>((*block)[index]);
        const auto swapped = static_cast<std::uint8_t>(
            (encrypted >> 4U) | (encrypted << 4U));
        const std::size_t offset = index - 8U;
        (*block)[index] = static_cast<char>(
            swapped ^ previous ^ static_cast<std::uint8_t>(offset) ^
            key[offset % key.size()]);
        previous = encrypted;
    }
    return true;
}

}  // namespace goldendict::core::formats::mdict::detail
