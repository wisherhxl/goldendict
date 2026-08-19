// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_APPLICATION_FULL_TEXT_INDEX_LIFECYCLE_INSPECTION_H_
#define GOLDENDICT_CORE_SRC_APPLICATION_FULL_TEXT_INDEX_LIFECYCLE_INSPECTION_H_

#include <optional>
#include <string>

#include "../dictionary/full_text_index_lifecycle.h"
#include "goldendict/base/goldendict_def.tp.h"

namespace goldendict::core {

class DictionaryService;

namespace application {

GOLDENDICT_EXPORTS
std::optional<dictionary::FullTextIndexLifecycleSnapshot>
FullTextIndexLifecycleSnapshot(const DictionaryService& service,
                               const std::string& dictionary_id);

}  // namespace application
}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_SRC_APPLICATION_FULL_TEXT_INDEX_LIFECYCLE_INSPECTION_H_
