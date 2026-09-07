// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_RESOURCE_H_
#define GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_RESOURCE_H_

#include <filesystem>
#include <optional>
#include <string_view>

#include "../../dictionary/dictionary_backend.h"
#include "../../foundation/zip_archive.h"

namespace goldendict::core::formats::stardict {

class ResourceProvider final {
   public:
    static ResourceProvider Open(const std::filesystem::path& info_path);

    std::optional<dictionary::Resource> Load(
        std::string_view resource_id,
        const dictionary::RequestOptions& options) const;

    const std::optional<std::filesystem::path>& archive_path() const noexcept {
        return archive_path_;
    }

   private:
    std::filesystem::path resource_root_;
    std::optional<std::filesystem::path> archive_path_;
    std::optional<foundation::ZipArchive> archive_;
};

}  // namespace goldendict::core::formats::stardict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_RESOURCE_H_
