// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_PROVIDER_H_
#define GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_PROVIDER_H_

#include <memory>
#include <optional>
#include <string_view>

#include "../dictionary/dictionary_backend.h"
#include "hunspell_discovery.h"

namespace goldendict::core::morphology::hunspell {

namespace detail {
std::optional<std::string_view> ExtractStem(std::string_view record);
}  // namespace detail

std::unique_ptr<dictionary::Backend> OpenProvider(const DataFiles& files);

}  // namespace goldendict::core::morphology::hunspell

#endif  // GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_PROVIDER_H_
