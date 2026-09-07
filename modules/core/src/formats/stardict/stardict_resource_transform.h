// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_RESOURCE_TRANSFORM_H_
#define GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_RESOURCE_TRANSFORM_H_

#include <string_view>
#include <vector>

#include "../../dictionary/dictionary_backend.h"

namespace goldendict::core::formats::stardict {

dictionary::Resource TransformResource(
    std::string_view normalized_id, std::string_view dictionary_id,
    std::vector<std::byte> data,
    const dictionary::RequestOptions& options);

}  // namespace goldendict::core::formats::stardict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_RESOURCE_TRANSFORM_H_
