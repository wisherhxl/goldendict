// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_RESOURCE_ZIP_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_RESOURCE_ZIP_H_

#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "../../foundation/zip_archive.h"

namespace goldendict::core::formats::dsl {

using ResourceZipErrorCode = foundation::ZipArchiveErrorCode;
using ResourceZipError = foundation::ZipArchiveError;

class ResourceZip final {
   public:
    static std::optional<ResourceZip> OpenAdjacent(
        const std::filesystem::path& dictionary_path);

    std::optional<std::vector<std::byte>> Read(
        std::string_view resource_id) const {
        return archive_.Read(resource_id);
    }

    const std::filesystem::path& path() const noexcept {
        return archive_.path();
    }

   private:
    explicit ResourceZip(foundation::ZipArchive archive)
        : archive_(std::move(archive)) {}

    foundation::ZipArchive archive_;
};

}  // namespace goldendict::core::formats::dsl

#endif  // GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_RESOURCE_ZIP_H_
