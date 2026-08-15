// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_RUNTIME_COMPOSITION_H_
#define GOLDENDICT_NETWORK_RUNTIME_COMPOSITION_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "goldendict/core/application.h"
#include "goldendict/core/runtime_dictionary_source.h"

namespace goldendict::network {

class NetworkRuntime;

using ForvoCredentialMap = std::map<std::string, std::string>;

enum class RuntimeCompositionDiagnosticCode {
    kMissingForvoCredential,
};

struct RuntimeCompositionDiagnostic {
    RuntimeCompositionDiagnosticCode code;
    std::string source_id;
};

struct RuntimeCompositionResult {
    std::vector<std::unique_ptr<goldendict::core::RuntimeDictionarySource>>
        sources;
    std::vector<RuntimeCompositionDiagnostic> diagnostics;
};

struct ApplicationCompositionResult {
    std::unique_ptr<goldendict::core::DesktopFacade> facade;
    std::vector<RuntimeCompositionDiagnostic> diagnostics;
};

RuntimeCompositionResult ComposeConfiguredRuntimeSources(
    const goldendict::core::CoreConfiguration& configuration,
    const ForvoCredentialMap& forvo_credentials = {});
RuntimeCompositionResult ComposeConfiguredRuntimeSources(
    const goldendict::core::CoreConfiguration& configuration,
    const ForvoCredentialMap& forvo_credentials,
    const std::shared_ptr<NetworkRuntime>& network_runtime);

ApplicationCompositionResult ComposeConfiguredApplication(
    const goldendict::core::CoreConfiguration& configuration,
    const ForvoCredentialMap& forvo_credentials = {});
ApplicationCompositionResult ComposeConfiguredApplication(
    const goldendict::core::CoreConfiguration& configuration,
    const ForvoCredentialMap& forvo_credentials,
    const std::shared_ptr<NetworkRuntime>& network_runtime);

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_RUNTIME_COMPOSITION_H_
