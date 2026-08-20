// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "goldendict/core/application.h"
#include "goldendict/core/history_store.h"
#include "pending_configuration_transaction.h"

namespace goldendict::core {

enum class PreparationCheckpoint : std::uint8_t {
    kAfterPreviousConfigurationRead,
    kAfterPreviousHistoryRead,
    kAfterDesiredConfigurationStaged,
    kAfterDesiredHistoryStaged,
    kAfterPendingRecordStaged,
};

enum class PreparationFilesystemOperation : std::uint8_t {
    kInspectConfiguration,
    kReadConfiguration,
    kInspectHistory,
    kReadHistory,
    kCreateStagingDirectory,
    kWriteStagingArtifact,
    kReadStagingArtifact,
    kRemoveStagingArtifact,
    kRemoveStagingDirectory,
};

enum class ConfigurationPreparationErrorCode : std::uint8_t {
    kInvalidInput,
    kIdentityGeneration,
    kConfigurationRead,
    kHistoryRead,
    kStagingCollision,
    kStagingIo,
    kVerification,
    kCleanup,
};

struct ConfigurationPreparationError {
    ConfigurationPreparationErrorCode code =
        ConfigurationPreparationErrorCode::kInvalidInput;
    std::string message;
    std::filesystem::path residual_staging_directory;
};

struct ConfigurationTransactionPreparationInput {
    std::filesystem::path configuration_path;
    std::filesystem::path history_path;
    CoreConfiguration desired_configuration;
    PendingHistoryIntent history_intent = PendingHistoryIntent::kUnchanged;
    std::vector<HistoryEntry> desired_history;
};

struct ConfigurationTransactionPreparationDependencies {
    std::function<std::optional<std::array<std::uint8_t, 16U>>()>
        generate_transaction_id;
    std::function<void(PreparationCheckpoint, const std::filesystem::path&)>
        checkpoint;
    std::function<std::optional<std::error_code>(PreparationFilesystemOperation,
                                                 const std::filesystem::path&)>
        filesystem_failure;
};

struct ConfigurationTransactionPreparationResult;

class PreparedConfigurationTransaction final {
   public:
    PreparedConfigurationTransaction(
        PreparedConfigurationTransaction&& other) noexcept;
    PreparedConfigurationTransaction& operator=(
        PreparedConfigurationTransaction&& other) = delete;
    PreparedConfigurationTransaction(const PreparedConfigurationTransaction&) =
        delete;
    PreparedConfigurationTransaction& operator=(
        const PreparedConfigurationTransaction&) = delete;
    ~PreparedConfigurationTransaction();

    const PendingConfigurationTransactionRecord& record() const noexcept {
        return record_;
    }

    const std::string& serialized_record() const noexcept {
        return serialized_record_;
    }

    const std::filesystem::path& staged_record_path() const noexcept {
        return staged_record_path_;
    }

    const std::filesystem::path& configuration_path() const noexcept {
        return configuration_path_;
    }

    const std::filesystem::path& history_path() const noexcept {
        return history_path_;
    }

    std::optional<ConfigurationPreparationError> Abort();

    void ReleaseForDecision() noexcept { owns_staging_ = false; }

   private:
    friend struct ConfigurationTransactionPreparationResult;
    friend ConfigurationTransactionPreparationResult
    PrepareConfigurationTransaction(
        const ConfigurationTransactionPreparationInput&,
        const ConfigurationTransactionPreparationDependencies&);

    PreparedConfigurationTransaction() = default;
    void CleanupNoThrow() noexcept;

    PendingConfigurationTransactionRecord record_;
    std::string serialized_record_;
    std::filesystem::path staged_record_path_;
    std::filesystem::path staging_directory_;
    std::filesystem::path configuration_path_;
    std::filesystem::path history_path_;
    std::function<std::optional<std::error_code>(PreparationFilesystemOperation,
                                                 const std::filesystem::path&)>
        filesystem_failure_;
    bool owns_staging_ = false;
};

struct ConfigurationTransactionPreparationResult {
    std::optional<PreparedConfigurationTransaction> prepared;
    std::optional<ConfigurationPreparationError> error;

    explicit operator bool() const noexcept { return prepared.has_value(); }
};

ConfigurationTransactionPreparationResult PrepareConfigurationTransaction(
    const ConfigurationTransactionPreparationInput& input,
    const ConfigurationTransactionPreparationDependencies& dependencies = {});

}  // namespace goldendict::core
