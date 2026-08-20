// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace goldendict::core {

enum class PendingTransactionVersion : std::uint8_t { kV1 = 1U };

enum class PendingTransactionPhase : std::uint8_t {
    // The complete candidate and previous-known-good state have been captured
    // durably, but no desired/previous convergence decision has started.
    kPrepared = 1U,
    kDesiredCommit = 2U,
    kDesiredPersistenceApplying = 3U,
    kDesiredPersistenceApplied = 4U,
    kDesiredPersistenceFailed = 5U,
    kDesiredRuntimeApplying = 6U,
    kDesiredRuntimeFailed = 7U,
    kPreviousPersistenceApplying = 8U,
    kPreviousPersistenceBlocked = 9U,
    kPreviousRuntimeApplying = 10U,
    kQuarantined = 11U,
};

enum class DesiredRecoveryAttempt : std::uint8_t {
    kNotAttempted = 0U,
    kAttempted = 1U,
};

enum class PendingHistoryIntent : std::uint8_t {
    kUnchanged = 0U,
    kReplace = 1U,
};

enum class PendingFailureOperation : std::uint8_t {
    kReadRecord = 1U,
    kValidateRecord = 2U,
    kPersistDesired = 3U,
    kReconstructDesired = 4U,
    kPersistPrevious = 5U,
    kReconstructPrevious = 6U,
    kQuarantine = 7U,
};

enum class PendingFailureDestination : std::uint8_t {
    kPendingRecord = 1U,
    kConfiguration = 2U,
    kHistory = 3U,
    kRuntimeFoundation = 4U,
    kRuntimeTransport = 5U,
    kRuntimePresentation = 6U,
};

enum class PendingFailureCategory : std::uint8_t {
    kNotFound = 1U,
    kInvalidData = 2U,
    kIo = 3U,
    kPermission = 4U,
    kResourceLimit = 5U,
    kUnavailable = 6U,
    kRejected = 7U,
    kCancelled = 8U,
    kTimeout = 9U,
    kInvariant = 10U,
    kUnknown = 11U,
};

struct PendingTransactionPayload {
    std::string bytes;
    std::uint64_t size = 0U;
    std::array<std::uint8_t, 32U> sha256{};

    bool operator==(const PendingTransactionPayload& other) const noexcept {
        return bytes == other.bytes && size == other.size &&
               sha256 == other.sha256;
    }
};

struct PendingPreviousDestination {
    bool existed = false;
    std::optional<PendingTransactionPayload> payload;

    bool operator==(const PendingPreviousDestination& other) const noexcept {
        return existed == other.existed && payload == other.payload;
    }
};

struct PendingFailureIdentity {
    PendingFailureOperation operation = PendingFailureOperation::kReadRecord;
    PendingFailureDestination destination =
        PendingFailureDestination::kPendingRecord;
    PendingFailureCategory category = PendingFailureCategory::kUnknown;
    std::string identifier;

    bool operator==(const PendingFailureIdentity& other) const noexcept {
        return operation == other.operation &&
               destination == other.destination && category == other.category &&
               identifier == other.identifier;
    }
};

struct PendingConfigurationTransactionRecord {
    PendingTransactionVersion version = PendingTransactionVersion::kV1;
    std::array<std::uint8_t, 16U> transaction_id{};
    PendingTransactionPhase phase = PendingTransactionPhase::kPrepared;
    DesiredRecoveryAttempt desired_recovery_attempt =
        DesiredRecoveryAttempt::kNotAttempted;
    PendingTransactionPayload desired_configuration;
    PendingHistoryIntent history_intent = PendingHistoryIntent::kUnchanged;
    std::optional<PendingTransactionPayload> desired_history;
    PendingPreviousDestination previous_configuration;
    PendingPreviousDestination previous_history;
    std::optional<PendingFailureIdentity> failure;

    bool operator==(
        const PendingConfigurationTransactionRecord& other) const noexcept {
        return version == other.version &&
               transaction_id == other.transaction_id && phase == other.phase &&
               desired_recovery_attempt == other.desired_recovery_attempt &&
               desired_configuration == other.desired_configuration &&
               history_intent == other.history_intent &&
               desired_history == other.desired_history &&
               previous_configuration == other.previous_configuration &&
               previous_history == other.previous_history &&
               failure == other.failure;
    }
};

PendingTransactionPayload MakePendingTransactionPayload(std::string bytes);
std::string SerializePendingConfigurationTransaction(
    const PendingConfigurationTransactionRecord& record);
PendingConfigurationTransactionRecord ParsePendingConfigurationTransaction(
    const std::string& bytes);

}  // namespace goldendict::core
