// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FOUNDATION_ZIP_ARCHIVE_H_
#define GOLDENDICT_CORE_SRC_FOUNDATION_ZIP_ARCHIVE_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace goldendict::core::foundation {

struct ZipArchiveLimits {
    std::size_t maximum_entries = 500000U;
    std::uint64_t maximum_resource_size = 16U * 1024U * 1024U;
    std::uint64_t maximum_compressed_size = 20U * 1024U * 1024U;
    std::size_t maximum_name_size = 64U * 1024U;
    std::uint64_t maximum_central_size = 256U * 1024U * 1024U;
    std::function<std::string(std::string_view)> normalize_resource_id;
};

enum class ZipArchiveErrorCode { kUnavailable, kInvalidData };

class ZipArchiveError final : public std::runtime_error {
   public:
    ZipArchiveError(ZipArchiveErrorCode code, std::string message);

    ZipArchiveErrorCode code() const noexcept { return code_; }

   private:
    ZipArchiveErrorCode code_;
};

class ZipArchive final {
   public:
    static ZipArchive Open(const std::filesystem::path& archive_path,
                           ZipArchiveLimits limits = {},
                           const std::function<void()>& checkpoint = {});

    std::optional<std::vector<std::byte>> Read(
        std::string_view resource_id,
        const std::function<void()>& checkpoint = {}) const;

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
    ZipArchiveLimits limits_;
    std::uintmax_t source_size_ = 0;
    std::filesystem::file_time_type source_write_time_;
    std::uint64_t central_offset_ = 0;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace goldendict::core::foundation

#endif  // GOLDENDICT_CORE_SRC_FOUNDATION_ZIP_ARCHIVE_H_
