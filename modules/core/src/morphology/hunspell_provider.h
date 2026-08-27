// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_PROVIDER_H_
#define GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_PROVIDER_H_

#include <memory>

#include "../dictionary/dictionary_backend.h"
#include "hunspell_discovery.h"

namespace goldendict::core::morphology::hunspell {

std::unique_ptr<dictionary::Backend> OpenProvider(const DataFiles& files);

}  // namespace goldendict::core::morphology::hunspell

#endif  // GOLDENDICT_CORE_SRC_MORPHOLOGY_HUNSPELL_PROVIDER_H_
