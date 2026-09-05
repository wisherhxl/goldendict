// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_RESOURCE_ZIP_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_RESOURCE_ZIP_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace goldendict::core::formats::dsl {

enum class ResourceZipErrorCode { kUnavailable, kInvalidData };

class ResourceZipError final : public std::runtime_error {
   public:
    ResourceZipError(ResourceZipErrorCode code, std::string message);

    ResourceZipErrorCode code() const noexcept { return code_; }

   private:
    ResourceZipErrorCode code_;
};

class ResourceZip final {
   public:
    static std::optional<ResourceZip> OpenAdjacent(
        const std::filesystem::path& dictionary_path);

    std::optional<std::vector<std::byte>> Read(
        std::string_view resource_id) const;

    const std::filesystem::path& path() const noexcept { return path_; }

   private:
    struct Entry {
        std::uint16_t flags = 0;
        std::uint16_t method = 0;
        std::uint32_t crc = 0;
        std::uint64_t compressed_size = 0;
        std::uint64_t uncompressed_size = 0;
        std::uint64_t local_offset = 0;
    };

    std::filesystem::path path_;
    std::uintmax_t source_size_ = 0;
    std::filesystem::file_time_type source_write_time_;
    std::uint64_t central_offset_ = 0;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace goldendict::core::formats::dsl

#endif  // GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_RESOURCE_ZIP_H_
