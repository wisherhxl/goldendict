// SPDX-License-Identifier: GPL-3.0-or-later

#include "internal_url.h"

#include <cctype>
#include <stdexcept>

namespace goldendict::core::article {
namespace {

constexpr std::string_view kPrefix = "goldendict://";

bool IsUnreserved(unsigned char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '-' || value == '.' ||
           value == '_' || value == '~';
}

std::string Encode(std::string_view value) {
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (const unsigned char byte : value) {
        if (IsUnreserved(byte)) {
            encoded.push_back(static_cast<char>(byte));
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[byte >> 4U]);
            encoded.push_back(kHex[byte & 0x0fU]);
        }
    }
    return encoded;
}

int HexValue(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

std::optional<std::string> Decode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        unsigned char byte = static_cast<unsigned char>(value[index]);
        if (byte == '%') {
            if (index + 2U >= value.size()) {
                return std::nullopt;
            }
            const int high = HexValue(value[index + 1U]);
            const int low = HexValue(value[index + 2U]);
            if (high < 0 || low < 0) {
                return std::nullopt;
            }
            byte = static_cast<unsigned char>((high << 4U) | low);
            index += 2U;
        }
        if (byte == 0U || byte < 0x20U || byte == 0x7fU) {
            return std::nullopt;
        }
        decoded.push_back(static_cast<char>(byte));
    }
    if (decoded.empty()) {
        return std::nullopt;
    }
    return decoded;
}

void RequireValue(std::string_view value, std::string_view name) {
    if (!Decode(Encode(value)).has_value()) {
        throw std::invalid_argument(std::string(name) + " is empty or invalid");
    }
}

void RequireResourceId(std::string_view resource_id) {
    RequireValue(resource_id, "resource_id");
    if (resource_id.front() == '/' || resource_id.front() == '\\') {
        throw std::invalid_argument("resource_id must be relative");
    }
    std::size_t start = 0;
    while (start <= resource_id.size()) {
        const auto end = resource_id.find_first_of("/\\", start);
        const auto part = resource_id.substr(
            start, end == std::string_view::npos ? resource_id.size() - start
                                                 : end - start);
        if (part.empty() || part == "." || part == "..") {
            throw std::invalid_argument("resource_id contains an unsafe path");
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
    }
}

}  // namespace

std::string MakeLookupUrl(std::string_view headword) {
    RequireValue(headword, "headword");
    return std::string(kPrefix) + "lookup/" + Encode(headword);
}

std::string MakeResourceUrl(std::string_view dictionary_id,
                            std::string_view resource_id) {
    RequireValue(dictionary_id, "dictionary_id");
    RequireResourceId(resource_id);
    return std::string(kPrefix) + "resource/" + Encode(dictionary_id) + "/" +
           Encode(resource_id);
}

std::optional<InternalUrl> ParseInternalUrl(std::string_view url) {
    if (url.substr(0, kPrefix.size()) != kPrefix) {
        return std::nullopt;
    }
    const auto body = url.substr(kPrefix.size());
    const auto first_slash = body.find('/');
    if (first_slash == std::string_view::npos) {
        return std::nullopt;
    }
    const auto kind = body.substr(0, first_slash);
    const auto payload = body.substr(first_slash + 1U);
    if (kind == "lookup" && payload.find('/') == std::string_view::npos) {
        const auto target = Decode(payload);
        if (target.has_value() && Encode(*target) == payload) {
            return InternalUrl{InternalUrlKind::kLookup, {}, *target};
        }
        return std::nullopt;
    }
    if (kind != "resource") {
        return std::nullopt;
    }
    const auto separator = payload.find('/');
    if (separator == std::string_view::npos ||
        payload.find('/', separator + 1U) != std::string_view::npos) {
        return std::nullopt;
    }
    const auto dictionary_id = Decode(payload.substr(0, separator));
    const auto resource_id = Decode(payload.substr(separator + 1U));
    if (!dictionary_id.has_value() || !resource_id.has_value() ||
        Encode(*dictionary_id) != payload.substr(0, separator) ||
        Encode(*resource_id) != payload.substr(separator + 1U)) {
        return std::nullopt;
    }
    try {
        RequireResourceId(*resource_id);
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
    return InternalUrl{InternalUrlKind::kResource, *dictionary_id,
                       *resource_id};
}

}  // namespace goldendict::core::article
