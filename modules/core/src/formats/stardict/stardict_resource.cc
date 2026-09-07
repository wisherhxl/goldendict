// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_resource.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <system_error>
#include <utility>

#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::stardict {
namespace {

constexpr std::uintmax_t kMaximumResourceSize = 64U * 1024U * 1024U;

dictionary::Error TranslateArchiveError(
    const foundation::ZipArchiveError& error) {
    return dictionary::Error(
        error.code() == foundation::ZipArchiveErrorCode::kUnavailable
            ? dictionary::ErrorCode::kUnavailable
            : dictionary::ErrorCode::kInvalidData,
        error.what());
}

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

ResourceProvider ResourceProvider::Open(
    const std::filesystem::path& info_path) {
    ResourceProvider provider;
    const auto directory = info_path.parent_path();
    provider.resource_root_ = directory / "res";
    const std::array<std::filesystem::path, 3U> candidates = {
        directory / "res.zip", directory / "RES.ZIP",
        directory / "res" / "res.zip"};
    for (const auto& candidate : candidates) {
        std::error_code filesystem_error;
        if (!std::filesystem::is_regular_file(candidate, filesystem_error) ||
            filesystem_error) {
            continue;
        }
        try {
            foundation::ZipArchiveLimits limits;
            limits.maximum_resource_size = kMaximumResourceSize;
            limits.maximum_compressed_size = kMaximumResourceSize;
            limits.normalize_resource_id = foundation::FoldSimpleCase;
            provider.archive_ = foundation::ZipArchive::Open(candidate, limits);
            provider.archive_path_ = provider.archive_->path();
            break;
        } catch (const foundation::ZipArchiveError& error) {
            throw TranslateArchiveError(error);
        } catch (const foundation::TextFoldingError& error) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    error.what());
        }
    }
    return provider;
}

std::optional<dictionary::Resource> ResourceProvider::Load(
    std::string_view resource_id,
    const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    const std::string normalized_id = NormalizeResourceId(resource_id);
    const auto load_archive = [&]() -> std::optional<dictionary::Resource> {
        if (!archive_.has_value()) {
            return std::nullopt;
        }
        try {
            auto data = archive_->Read(normalized_id, [&options]() {
                dictionary::CheckRequest(options);
            });
            if (!data.has_value()) {
                return std::nullopt;
            }
            return dictionary::Resource{
                normalized_id,
                dictionary::MediaTypeForResourceId(normalized_id),
                std::move(*data)};
        } catch (const foundation::ZipArchiveError& error) {
            throw TranslateArchiveError(error);
        } catch (const foundation::TextFoldingError& error) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    error.what());
        }
    };

    std::error_code filesystem_error;
    const auto canonical_root =
        std::filesystem::weakly_canonical(resource_root_, filesystem_error);
    if (filesystem_error) {
        return load_archive();
    }
    const auto candidate = std::filesystem::weakly_canonical(
        resource_root_ / std::filesystem::u8path(normalized_id),
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
        return load_archive();
    }
    if (!std::filesystem::is_regular_file(candidate, filesystem_error)) {
        if (filesystem_error) {
            throw dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                    "Cannot inspect StarDict resource");
        }
        return load_archive();
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
