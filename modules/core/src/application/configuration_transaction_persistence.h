// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <system_error>

#include "configuration_transaction_preparation.h"

namespace goldendict::core {

struct ConfigurationRecoveryRequest;

enum class ConfigurationPersistenceOperation : std::uint8_t {
    kInspectPath,
    kCreateTemporary,
    kWriteTemporary,
    kFlushTemporary,
    kSyncTemporary,
    kPublishDecision,
    kReplaceDestination,
    kSyncDirectory,
    kRemoveTemporary,
    kRemoveDestination,
    kReadVerification,
    kVerifyPayload,
    kVerifyAbsence,
    kValidatePendingIdentity,
    kVerifyPendingRecord,
};

enum class ConfigurationPersistenceCheckpoint : std::uint8_t {
    kAfterTemporarySynced,
    kAfterDecisionPublished,
    kAfterRecordReplaced,
    kAfterDirectorySynced,
    kAfterConfigurationReplaced,
    kAfterConfigurationVerified,
    kAfterHistoryReplaced,
    kAfterHistoryVerified,
    kAfterHistoryRemoved,
    kAfterHistoryAbsenceVerified,
    kAfterPendingRecordVerified,
};

struct ConfigurationPersistenceDependencies {
    std::function<std::optional<std::error_code>(
        ConfigurationPersistenceOperation, const std::filesystem::path&)>
        filesystem_failure;
    std::function<void(ConfigurationPersistenceCheckpoint,
                       const std::filesystem::path&)>
        checkpoint;
};

enum class ConfigurationPersistenceOutcome : std::uint8_t {
    kPreDecisionFailure,
    kDesiredPersistenceApplied,
    kPostDecisionFailure,
};

struct ConfigurationPersistenceError {
    PendingFailureIdentity identity;
    std::string message;
};

struct ConfigurationPersistenceResult {
    ConfigurationPersistenceOutcome outcome =
        ConfigurationPersistenceOutcome::kPreDecisionFailure;
    bool decision_path_published = false;
    std::optional<PendingTransactionPhase> namespace_published_phase;
    std::optional<PendingTransactionPhase> confirmed_durable_phase;
    std::optional<ConfigurationPersistenceError> error;
    bool failure_evidence_namespace_published = false;
    bool failure_evidence_durable = false;
    std::optional<ConfigurationPersistenceError> failure_evidence_error;
    std::optional<PreparedConfigurationTransaction> abortable_prepared;
};

std::filesystem::path PendingConfigurationTransactionPath(
    const std::filesystem::path& configuration_path);

ConfigurationPersistenceResult PersistDesiredConfiguration(
    PreparedConfigurationTransaction prepared,
    const ConfigurationPersistenceDependencies& dependencies = {});

enum class PreviousPersistenceOutcome : std::uint8_t {
    kPreviousPersistenceApplied,
    kPreviousPersistenceFailed,
};

struct PreviousPersistenceRequest {
    std::filesystem::path configuration_path;
    std::filesystem::path history_path;
    std::array<std::uint8_t, 16U> transaction_id{};
};

struct PreviousPersistenceResult {
    PreviousPersistenceOutcome outcome =
        PreviousPersistenceOutcome::kPreviousPersistenceFailed;
    std::optional<PendingTransactionPhase> namespace_published_phase;
    std::optional<PendingTransactionPhase> confirmed_durable_phase;
    std::optional<ConfigurationPersistenceError> error;
    bool failure_evidence_namespace_published = false;
    bool failure_evidence_durable = false;
    std::optional<ConfigurationPersistenceError> failure_evidence_error;
};

PreviousPersistenceResult PersistPreviousConfiguration(
    const PreviousPersistenceRequest& request,
    const ConfigurationPersistenceDependencies& dependencies = {});

enum class RuntimeTransitionOutcome : std::uint8_t {
    kRejectedBeforePublication,
    kApplied,
    kPostPublicationFailure,
};

struct RuntimeTransitionResult {
    RuntimeTransitionOutcome outcome =
        RuntimeTransitionOutcome::kRejectedBeforePublication;
    std::optional<PendingTransactionPhase> namespace_published_phase;
    std::optional<PendingTransactionPhase> confirmed_durable_phase;
    bool pending_record_removed = false;
    bool removal_confirmed_durable = false;
    std::optional<ConfigurationPersistenceError> error;
};

RuntimeTransitionResult BeginDesiredRuntimePublication(
    const ConfigurationRecoveryRequest& request,
    const ConfigurationPersistenceDependencies& dependencies = {});
RuntimeTransitionResult RecordDesiredRuntimeFailure(
    const ConfigurationRecoveryRequest& request,
    PendingFailureDestination destination, PendingFailureCategory category,
    std::string identifier,
    const ConfigurationPersistenceDependencies& dependencies = {});
RuntimeTransitionResult FinishDesiredConfigurationTransaction(
    const ConfigurationRecoveryRequest& request,
    const std::filesystem::path& history_path,
    const ConfigurationPersistenceDependencies& dependencies = {});

enum class ConfigurationRecoveryDisposition : std::uint8_t {
    kNoActionDeferToRuntime,
    kAutomaticDesiredRecoveryAuthorized,
    kPreviousFallbackSelected,
    kPreviousFallbackReplaySelected,
    kQuarantinedTerminal,
    kRejectedOrFailed,
};

struct ConfigurationRecoveryRequest {
    std::filesystem::path configuration_path;
    std::array<std::uint8_t, 16U> transaction_id{};
};

struct ConfigurationRecoverySnapshot {
    PendingTransactionPhase phase = PendingTransactionPhase::kPrepared;
    DesiredRecoveryAttempt desired_recovery_attempt =
        DesiredRecoveryAttempt::kNotAttempted;

    bool operator==(const ConfigurationRecoverySnapshot& other) const noexcept {
        return phase == other.phase &&
               desired_recovery_attempt == other.desired_recovery_attempt;
    }
};

struct ConfigurationRecoveryResult {
    ConfigurationRecoveryDisposition disposition =
        ConfigurationRecoveryDisposition::kRejectedOrFailed;
    std::optional<ConfigurationRecoverySnapshot> namespace_visible_snapshot;
    std::optional<ConfigurationRecoverySnapshot>
        directory_sync_confirmed_snapshot;
    bool exact_verification_succeeded = false;
    std::optional<ConfigurationPersistenceError> primary_error;
    std::optional<ConfigurationPersistenceError> secondary_error;
};

struct PendingConfigurationInspectionResult {
    bool present = false;
    std::optional<PendingConfigurationTransactionRecord> record;
    std::optional<ConfigurationPersistenceError> error;
};

PendingConfigurationInspectionResult InspectPendingConfigurationTransaction(
    const std::filesystem::path& configuration_path,
    const ConfigurationPersistenceDependencies& dependencies = {});

ConfigurationPersistenceResult ReplayDesiredConfiguration(
    const ConfigurationRecoveryRequest& request,
    const std::filesystem::path& history_path,
    const ConfigurationPersistenceDependencies& dependencies = {});

RuntimeTransitionResult QuarantineConfigurationTransaction(
    const ConfigurationRecoveryRequest& request,
    PendingFailureOperation operation, PendingFailureDestination destination,
    PendingFailureCategory category, std::string identifier,
    const ConfigurationPersistenceDependencies& dependencies = {});

RuntimeTransitionResult FinishRecoveredConfigurationTransaction(
    const ConfigurationRecoveryRequest& request,
    const std::filesystem::path& history_path, bool desired,
    const ConfigurationPersistenceDependencies& dependencies = {});

RuntimeTransitionResult DiscardPreparedConfigurationTransaction(
    const ConfigurationRecoveryRequest& request,
    const std::filesystem::path& history_path,
    const ConfigurationPersistenceDependencies& dependencies = {});

ConfigurationRecoveryResult EvaluateConfigurationRecovery(
    const ConfigurationRecoveryRequest& request,
    const ConfigurationPersistenceDependencies& dependencies = {});

}  // namespace goldendict::core
