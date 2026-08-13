// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_RUNTIME_COMPOSITION_H_
#define GOLDENDICT_NETWORK_RUNTIME_COMPOSITION_H_

#include <memory>
#include <vector>

#include "goldendict/core/application.h"
#include "goldendict/core/runtime_dictionary_source.h"

namespace goldendict::network {

std::vector<std::unique_ptr<goldendict::core::RuntimeDictionarySource>>
ComposeConfiguredRuntimeSources(
    const goldendict::core::CoreConfiguration& configuration);

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_RUNTIME_COMPOSITION_H_
