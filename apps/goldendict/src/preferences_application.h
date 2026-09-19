// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "configuration_reload_transaction_coordinator.h"
#include "goldendict/core/history_store.h"
#include "goldendict/network/runtime_composition.h"
#include "main_window.h"

namespace goldendict::app {

struct PreparedProductionFacade {
    goldendict::core::application::PreparedCoreFacadeCandidate candidate;
    std::shared_ptr<goldendict::core::DesktopFacade> facade;
    std::vector<goldendict::network::RuntimeCompositionDiagnostic> diagnostics;
    bool session_restored = true;
};

PreparedProductionFacade PrepareProductionFacade(
    const core::CoreConfiguration& configuration,
    const network::ForvoCredentialMap& forvo_credentials,
    const std::shared_ptr<network::NetworkRuntime>& network_runtime,
    core::application::DesktopFacadeActivationOwner& owner,
    bool require_session_restoration = true);
void ReportRuntimeCompositionDiagnostics(
    const std::vector<network::RuntimeCompositionDiagnostic>& diagnostics);

// Non-owning application state, valid for every synchronous callback
// invocation. The installer copies the path and notification values, not
// application state.
struct PreferencesApplicationBindings {
    core::CoreConfiguration& configuration;
    std::vector<core::HistoryEntry>& history;
    std::shared_ptr<core::DesktopFacade>& facade;
    core::application::DesktopFacadeActivationOwner& facade_owner;
    const std::shared_ptr<network::NetworkRuntime>& network_runtime;
    ConfigurationReloadTransactionCoordinator& coordinator;
    const network::ForvoCredentialMap& forvo_credentials;
    std::vector<network::RuntimeCompositionDiagnostic>& composition_diagnostics;
    QString configuration_path;
    QString history_path;
    std::string network_cache_root;
    std::function<void()> refresh_history;
};

// Replaces a single callback, with no connection or scheduling. Borrowed owners
// must outlive invocations; replacing/destroying this callable invokes no work.
MainWindow::PreferencesApplyCallback InstallPreferencesApplication(
    MainWindow& window, PreferencesApplicationBindings bindings,
    std::function<bool(ConfigurationReloadBoundary)> inject_failure = {});

}  // namespace goldendict::app
