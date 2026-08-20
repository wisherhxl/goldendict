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
    kReadVerification,
    kVerifyPayload,
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

}  // namespace goldendict::core
