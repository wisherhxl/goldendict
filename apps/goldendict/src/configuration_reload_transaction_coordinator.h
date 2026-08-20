// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "../../../modules/core/src/application/configuration_transaction_persistence.h"
#include "../../../modules/core/src/application/desktop_facade_activation_owner.h"
#include "goldendict/network/network_runtime.h"

class MainWindow;

namespace goldendict::app {

enum class ConfigurationReloadOutcome {
    kRejectedBeforeDecision,
    kPublished,
    kPublishedWithForwardFailure,
};

enum class ConfigurationReloadBoundary {
    kPersistencePrepare,
    kNetworkPrepare,
    kCorePrepare,
    kWidgetsPrepare,
    kNetworkReserve,
    kCoreReserve,
    kWidgetsBegin,
    kPersistenceDecision,
    kDesiredRuntimeApplying,
    kNetworkPublish,
    kCorePublish,
    kWidgetsPublish,
    kNetworkPostWork,
    kWidgetsFinish,
    kCoreForwardWork,
    kRecordRuntimeFailure,
    kFinalizeTransaction,
};

struct ConfigurationReloadRequest {
    core::ConfigurationTransactionPreparationInput persistence;
    network::NetworkRuntime::Preparation network;
    std::vector<std::unique_ptr<core::RuntimeDictionarySource>> runtime_sources;
};

struct ConfigurationReloadDependencies {
    core::ConfigurationTransactionPreparationDependencies preparation;
    core::ConfigurationPersistenceDependencies persistence;
    std::function<bool(ConfigurationReloadBoundary)> inject_failure;
    std::function<void(ConfigurationReloadBoundary)> observe_boundary;
    std::function<void()> fail_stop;
};

struct ConfigurationReloadResult {
    ConfigurationReloadOutcome outcome =
        ConfigurationReloadOutcome::kRejectedBeforeDecision;
    std::optional<core::ConfigurationPersistenceError> error;
    std::optional<core::PendingTransactionPhase> durable_phase;
    bool network_published = false;
    bool core_published = false;
    bool widgets_published = false;
};

class ConfigurationReloadTransactionCoordinator final {
   public:
    ConfigurationReloadTransactionCoordinator(
        std::shared_ptr<network::NetworkRuntime> network_runtime,
        core::application::DesktopFacadeActivationOwner& core_owner,
        MainWindow& main_window) noexcept;

    ConfigurationReloadResult Execute(
        ConfigurationReloadRequest request,
        const ConfigurationReloadDependencies& dependencies = {});
    void Shutdown() noexcept;

   private:
    std::shared_ptr<network::NetworkRuntime> network_runtime_;
    core::application::DesktopFacadeActivationOwner* core_owner_ = nullptr;
    MainWindow* main_window_ = nullptr;
    bool executing_ = false;
    bool shutdown_ = false;
};

}  // namespace goldendict::app
