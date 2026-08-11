// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_resource.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <system_error>

namespace goldendict::core::formats::stardict {
namespace {

constexpr std::uintmax_t kMaximumResourceSize = 64U * 1024U * 1024U;

std::string NormalizeResourceId(std::string_view resource_id) {
    if (!resource_id.empty() && resource_id.front() == '\x1e') {
        resource_id.remove_prefix(1U);
    }
    if (!resource_id.empty() && resource_id.back() == '\x1f') {
        resource_id.remove_suffix(1U);
    }
    if (resource_id.empty() ||
        resource_id.find('\0') != std::string_view::npos) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Invalid empty StarDict resource identifier");
    }

    std::string normalized(resource_id);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const auto path = std::filesystem::u8path(normalized);
    if (path.is_absolute() || path.has_root_name() ||
        path.has_root_directory()) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Absolute StarDict resource path is forbidden");
    }
    std::filesystem::path safe_path;
    for (const auto& component : path) {
        if (component == "." || component.empty()) {
            continue;
        }
        if (component == "..") {
            throw dictionary::Error(
                dictionary::ErrorCode::kInvalidData,
                "StarDict resource path traversal is forbidden");
        }
        safe_path /= component;
    }
    if (safe_path.empty()) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Invalid StarDict resource identifier");
    }
    return safe_path.generic_string();
}

bool IsWithin(const std::filesystem::path& root,
              const std::filesystem::path& candidate) {
    auto root_iterator = root.begin();
    auto candidate_iterator = candidate.begin();
    while (root_iterator != root.end()) {
        if (candidate_iterator == candidate.end() ||
            *root_iterator != *candidate_iterator) {
            return false;
        }
        ++root_iterator;
        ++candidate_iterator;
    }
    return true;
}

}  // namespace

std::optional<dictionary::Resource> LoadResource(
    const std::filesystem::path& resource_root, std::string_view resource_id,
    const dictionary::RequestOptions& options) {
    dictionary::CheckRequest(options);
    const std::string normalized_id = NormalizeResourceId(resource_id);

    std::error_code filesystem_error;
    const auto canonical_root =
        std::filesystem::weakly_canonical(resource_root, filesystem_error);
    if (filesystem_error) {
        throw dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                "Cannot resolve StarDict resource directory");
    }
    const auto candidate = std::filesystem::weakly_canonical(
        resource_root / std::filesystem::u8path(normalized_id),
        filesystem_error);
    if (filesystem_error) {
        throw dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                "Cannot resolve StarDict resource path");
    }
    if (!IsWithin(canonical_root, candidate) || candidate == canonical_root) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "StarDict resource escapes its resource root");
    }
    if (!std::filesystem::exists(candidate, filesystem_error)) {
        if (filesystem_error) {
            throw dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                    "Cannot inspect StarDict resource");
        }
        return std::nullopt;
    }
    if (!std::filesystem::is_regular_file(candidate, filesystem_error) ||
        filesystem_error) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "StarDict resource is not a regular file");
    }
    const auto size = std::filesystem::file_size(candidate, filesystem_error);
    if (filesystem_error) {
        throw dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                "Cannot inspect StarDict resource size");
    }
    if (size > kMaximumResourceSize) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "StarDict resource exceeds the size limit");
    }

    std::ifstream input(candidate, std::ios::binary);
    if (!input) {
        throw dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                "Cannot open StarDict resource");
    }
    dictionary::Resource resource;
    resource.id = normalized_id;
    resource.media_type = dictionary::MediaTypeForResourceId(normalized_id);
    resource.data.reserve(static_cast<std::size_t>(size));
    std::array<char, 64U * 1024U> buffer{};
    while (input) {
        dictionary::CheckRequest(options);
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        const auto* begin = reinterpret_cast<const std::byte*>(buffer.data());
        resource.data.insert(resource.data.end(), begin, begin + count);
    }
    if (!input.eof()) {
        throw dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                "Cannot read complete StarDict resource");
    }
    dictionary::CheckRequest(options);
    return resource;
}

}  // namespace goldendict::core::formats::stardict
