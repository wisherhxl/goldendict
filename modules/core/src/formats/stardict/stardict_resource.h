// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_RESOURCE_H_
#define GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_RESOURCE_H_

#include <filesystem>
#include <optional>
#include <string_view>

#include "../../dictionary/dictionary_backend.h"

namespace goldendict::core::formats::stardict {

std::optional<dictionary::Resource> LoadResource(
    const std::filesystem::path& resource_root, std::string_view resource_id,
    const dictionary::RequestOptions& options);

}  // namespace goldendict::core::formats::stardict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_RESOURCE_H_
