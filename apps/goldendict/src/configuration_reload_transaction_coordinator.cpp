// SPDX-License-Identifier: GPL-3.0-or-later

#include "configuration_reload_transaction_coordinator.h"

#include <exception>
#include <string>
#include <utility>

#include <QThread>

#include "../../../modules/network/src/network_runtime_transaction.h"
#include "main_window.h"

namespace goldendict::app {
namespace {

void Observe(const ConfigurationReloadDependencies& dependencies,
             ConfigurationReloadBoundary boundary) {
    if (dependencies.observe_boundary)
        dependencies.observe_boundary(boundary);
}

bool Inject(const ConfigurationReloadDependencies& dependencies,
            ConfigurationReloadBoundary boundary) {
    Observe(dependencies, boundary);
    return dependencies.inject_failure && dependencies.inject_failure(boundary);
}

void FailStop(const ConfigurationReloadDependencies& dependencies) {
    if (dependencies.fail_stop)
        dependencies.fail_stop();
    std::terminate();
}

core::ConfigurationPersistenceError BoundaryError(
    ConfigurationReloadBoundary boundary, const char* message) {
    return {{core::PendingFailureOperation::kReconstructDesired,
             core::PendingFailureDestination::kRuntimeFoundation,
             core::PendingFailureCategory::kInvariant,
             std::to_string(static_cast<std::uint32_t>(boundary))},
            message};
}

}  // namespace

ConfigurationReloadTransactionCoordinator::
    ConfigurationReloadTransactionCoordinator(
        std::shared_ptr<network::NetworkRuntime> network_runtime,
        core::application::DesktopFacadeActivationOwner& core_owner,
        MainWindow& main_window) noexcept
    : network_runtime_(std::move(network_runtime)),
      core_owner_(&core_owner),
      main_window_(&main_window) {}

void ConfigurationReloadTransactionCoordinator::Shutdown() noexcept {
    if (executing_)
        std::terminate();
    shutdown_ = true;
}

ConfigurationReloadResult ConfigurationReloadTransactionCoordinator::Execute(
    ConfigurationReloadRequest request,
    const ConfigurationReloadDependencies& dependencies) {
    ConfigurationReloadResult result;
    if (shutdown_ || executing_ || !network_runtime_ ||
        core_owner_ == nullptr || main_window_ == nullptr ||
        QThread::currentThread() != main_window_->thread()) {
        return result;
    }
    executing_ = true;

    struct ExecutionGuard {
        bool& flag;

        ~ExecutionGuard() { flag = false; }
    } guard{executing_};

    if (Inject(dependencies, ConfigurationReloadBoundary::kPersistencePrepare))
        return result;
    auto persistence = core::PrepareConfigurationTransaction(
        request.persistence, dependencies.preparation);
    if (!persistence) {
        if (persistence.error) {
            result.error =
                BoundaryError(ConfigurationReloadBoundary::kPersistencePrepare,
                              persistence.error->message.c_str());
        }
        return result;
    }
    const auto transaction_id = persistence.prepared->record().transaction_id;

    if (Inject(dependencies, ConfigurationReloadBoundary::kNetworkPrepare))
        return result;
    auto network_candidate =
        network_runtime_->PrepareCandidate(std::move(request.network));
    if (!network_candidate)
        return result;

    if (Inject(dependencies, ConfigurationReloadBoundary::kCorePrepare))
        return result;
    core::application::PreparedCoreFacadeCandidate core_candidate;
    std::shared_ptr<core::DesktopFacade> candidate_facade;
    try {
        if (request.prepare_core) {
            auto prepared = request.prepare_core();
            if (!prepared)
                return result;
            core_candidate = std::move(prepared->candidate);
            candidate_facade = std::move(prepared->facade);
        } else {
            core_candidate = core_owner_->PrepareCandidate(
                request.persistence.desired_configuration,
                std::move(request.runtime_sources));
            if (core_candidate) {
                candidate_facade =
                    core_owner_->PreparedFacadeSnapshot(core_candidate);
            }
        }
    } catch (const std::exception& error) {
        result.error = BoundaryError(ConfigurationReloadBoundary::kCorePrepare,
                                     error.what());
        return result;
    }
    if (!core_candidate || !candidate_facade ||
        core_owner_->PreparedFacadeSnapshot(core_candidate) !=
            candidate_facade) {
        return result;
    }

    if (Inject(dependencies, ConfigurationReloadBoundary::kWidgetsPrepare))
        return result;
    auto widgets_candidate = main_window_->PrepareFacadeCandidate(
        candidate_facade, request.persistence.desired_configuration.preferences,
        request.persistence.desired_configuration.dictionary_groups);
    if (!widgets_candidate)
        return result;

    if (Inject(dependencies, ConfigurationReloadBoundary::kNetworkReserve))
        return result;
    auto network_reservation = network_runtime_->Reserve(network_candidate);
    if (!network_reservation)
        return result;

    if (Inject(dependencies, ConfigurationReloadBoundary::kCoreReserve)) {
        network_runtime_->Abort(network_reservation);
        return result;
    }
    auto core_reservation = core_owner_->Reserve(core_candidate);
    if (!core_reservation) {
        network_runtime_->Abort(network_reservation);
        return result;
    }

    if (Inject(dependencies, ConfigurationReloadBoundary::kWidgetsBegin)) {
        core_reservation.Abort();
        network_runtime_->Abort(network_reservation);
        return result;
    }
    auto widgets_begin =
        main_window_->BeginFacadeCandidateMaintenance(widgets_candidate);
    if (widgets_begin.outcome != WidgetsCommitOutcome::kMaintainedAbortable) {
        core_reservation.Abort();
        network_runtime_->Abort(network_reservation);
        return result;
    }

    if (Inject(dependencies,
               ConfigurationReloadBoundary::kPersistenceDecision)) {
        main_window_->AbortMaintainedFacadeCommit(
            std::move(widgets_begin.maintained));
        core_reservation.Abort();
        network_runtime_->Abort(network_reservation);
        return result;
    }
    auto persisted = core::PersistDesiredConfiguration(
        std::move(*persistence.prepared), dependencies.persistence);
    if (persisted.outcome ==
        core::ConfigurationPersistenceOutcome::kPreDecisionFailure) {
        main_window_->AbortMaintainedFacadeCommit(
            std::move(widgets_begin.maintained));
        core_reservation.Abort();
        network_runtime_->Abort(network_reservation);
        result.error = persisted.error;
        return result;
    }

    const core::ConfigurationRecoveryRequest recovery{
        request.persistence.configuration_path, transaction_id};
    result.outcome = ConfigurationReloadOutcome::kPublishedWithForwardFailure;
    result.error = persisted.error;
    result.durable_phase = persisted.confirmed_durable_phase;

    const bool applying_injected = Inject(
        dependencies, ConfigurationReloadBoundary::kDesiredRuntimeApplying);
    auto applying = applying_injected ? core::RuntimeTransitionResult{}
                                      : core::BeginDesiredRuntimePublication(
                                            recovery, dependencies.persistence);
    if (applying.outcome != core::RuntimeTransitionOutcome::kApplied ||
        applying.confirmed_durable_phase !=
            core::PendingTransactionPhase::kDesiredRuntimeApplying) {
        result.error = applying.error;
        FailStop(dependencies);
        return result;
    }
    result.durable_phase = applying.confirmed_durable_phase;

    auto record_failure = [&](core::PendingFailureDestination destination,
                              ConfigurationReloadBoundary boundary,
                              const char* identifier) {
        Observe(dependencies,
                ConfigurationReloadBoundary::kRecordRuntimeFailure);
        auto failure = core::RecordDesiredRuntimeFailure(
            recovery, destination, core::PendingFailureCategory::kInvariant,
            identifier, dependencies.persistence);
        if (failure.error)
            result.error = failure.error;
        else
            result.error = BoundaryError(boundary, identifier);
        result.durable_phase = failure.confirmed_durable_phase;
    };

    if (Inject(dependencies, ConfigurationReloadBoundary::kNetworkPublish)) {
        record_failure(core::PendingFailureDestination::kRuntimeTransport,
                       ConfigurationReloadBoundary::kNetworkPublish,
                       "network_publication");
        FailStop(dependencies);
        return result;
    }
    auto published_network = network::NetworkRuntimeTransaction::Publish(
        *network_runtime_, network_reservation);
    result.network_published = true;

    if (Inject(dependencies, ConfigurationReloadBoundary::kCorePublish)) {
        record_failure(core::PendingFailureDestination::kRuntimeFoundation,
                       ConfigurationReloadBoundary::kCorePublish,
                       "core_publication");
        FailStop(dependencies);
        return result;
    }
    auto published_core = core_owner_->PublishReservedOnly(core_reservation);
    result.core_published = true;

    if (Inject(dependencies, ConfigurationReloadBoundary::kWidgetsPublish)) {
        record_failure(core::PendingFailureDestination::kRuntimePresentation,
                       ConfigurationReloadBoundary::kWidgetsPublish,
                       "widgets_publication");
        FailStop(dependencies);
        return result;
    }
    auto published_widgets = main_window_->PublishMaintainedFacadeCommit(
        std::move(widgets_begin.maintained));
    result.widgets_published = true;

    bool forward_failure = false;
    const bool network_failure_injected =
        Inject(dependencies, ConfigurationReloadBoundary::kNetworkPostWork);
    auto network_result = network::NetworkRuntimeTransaction::Finish(
        *network_runtime_, published_network);
    if (network_failure_injected)
        network_result = network::NetworkRuntime::CommitResult::
            kPublishedWithPostWorkFailure;
    if (network_result ==
        network::NetworkRuntime::CommitResult::kPublishedWithPostWorkFailure) {
        forward_failure = true;
        record_failure(core::PendingFailureDestination::kRuntimeTransport,
                       ConfigurationReloadBoundary::kNetworkPostWork,
                       "network_post_work");
    }

    const bool widgets_failure_injected =
        Inject(dependencies, ConfigurationReloadBoundary::kWidgetsFinish);
    auto widgets_result =
        main_window_->FinishPublishedFacadeCommit(std::move(published_widgets));
    if (widgets_failure_injected)
        widgets_result = WidgetsCommitOutcome::kPublishedWithCleanupFailure;
    if (widgets_result == WidgetsCommitOutcome::kPublishedWithCleanupFailure) {
        forward_failure = true;
        record_failure(core::PendingFailureDestination::kRuntimePresentation,
                       ConfigurationReloadBoundary::kWidgetsFinish,
                       "widgets_forward_maintenance");
    }

    if (Inject(dependencies, ConfigurationReloadBoundary::kCoreForwardWork)) {
        record_failure(core::PendingFailureDestination::kRuntimeFoundation,
                       ConfigurationReloadBoundary::kCoreForwardWork,
                       "core_forward_maintenance");
        FailStop(dependencies);
        return result;
    }
    core_owner_->FinishPublished(published_core);

    if (forward_failure)
        return result;
    if (Inject(dependencies, ConfigurationReloadBoundary::kFinalizeTransaction))
        return result;
    auto finalized = core::FinishDesiredConfigurationTransaction(
        recovery, request.persistence.history_path, dependencies.persistence);
    result.error = finalized.error;
    result.durable_phase = finalized.confirmed_durable_phase;
    if (finalized.outcome != core::RuntimeTransitionOutcome::kApplied)
        return result;

    result.outcome = ConfigurationReloadOutcome::kPublished;
    return result;
}

}  // namespace goldendict::app
