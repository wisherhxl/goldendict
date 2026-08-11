// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_APPLICATION_H_
#define GOLDENDICT_CORE_APPLICATION_H_

#include <memory>
#include <string>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"
#include "goldendict/core/desktop_facade.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core {

struct CoreConfiguration {
    std::vector<std::string> dictionary_paths;
    std::string index_directory;
};

// Missing files load as an empty clean-profile configuration. Malformed files
// and I/O failures throw std::runtime_error.
GOLDENDICT_EXPORTS CoreConfiguration
LoadConfiguration(const std::string& configuration_path);
GOLDENDICT_EXPORTS void SaveConfiguration(
    const std::string& configuration_path,
    const CoreConfiguration& configuration);

GOLDENDICT_EXPORTS std::unique_ptr<DictionaryService> CreateDictionaryService(
    const CoreConfiguration& configuration);
GOLDENDICT_EXPORTS std::unique_ptr<DesktopFacade> CreateDesktopFacade(
    const CoreConfiguration& configuration);

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_APPLICATION_H_
