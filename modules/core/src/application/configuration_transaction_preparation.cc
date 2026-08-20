// SPDX-License-Identifier: GPL-3.0-or-later

#include "configuration_transaction_preparation.h"

#include <algorithm>
#include <fstream>
#include <random>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace goldendict::core {
namespace {

constexpr std::uintmax_t kMaximumPayloadBytes = 1024U * 1024U;

class PreparationFailure final : public std::runtime_error {
   public:
    PreparationFailure(ConfigurationPreparationErrorCode code,
                       std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}

    ConfigurationPreparationErrorCode code() const noexcept { return code_; }

   private:
    ConfigurationPreparationErrorCode code_;
};

std::string IdentityHex(const std::array<std::uint8_t, 16U>& identity) {
    constexpr char kHex[] = "0123456789abcdef";
    std::string result;
    result.reserve(identity.size() * 2U);
    for (const auto byte : identity) {
        result.push_back(kHex[byte >> 4U]);
        result.push_back(kHex[byte & 0x0fU]);
    }
    return result;
}

std::optional<std::array<std::uint8_t, 16U>> GenerateIdentity() {
    std::random_device random;
    std::array<std::uint8_t, 16U> identity{};
    for (auto& byte : identity)
        byte = static_cast<std::uint8_t>(random());
    return identity;
}

bool IsZero(const std::array<std::uint8_t, 16U>& identity) {
    return std::all_of(identity.begin(), identity.end(),
                       [](std::uint8_t byte) { return byte == 0U; });
}

void FailIfInjected(
    const ConfigurationTransactionPreparationDependencies& dependencies,
    PreparationFilesystemOperation operation, const std::filesystem::path& path,
    ConfigurationPreparationErrorCode code, const char* message) {
    if (!dependencies.filesystem_failure)
        return;
    try {
        const auto error = dependencies.filesystem_failure(operation, path);
        if (error)
            throw PreparationFailure(code, message);
    } catch (const PreparationFailure&) {
        throw;
    } catch (...) {
        throw PreparationFailure(code, message);
    }
}

std::string ReadBounded(
    const std::filesystem::path& path, ConfigurationPreparationErrorCode code,
    bool required,
    const ConfigurationTransactionPreparationDependencies& dependencies,
    PreparationFilesystemOperation inspect_operation,
    PreparationFilesystemOperation read_operation) {
    FailIfInjected(dependencies, inspect_operation, path, code,
                   "Injected transaction source inspection failure");
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error)
        throw PreparationFailure(code, "Cannot inspect transaction source");
    if (!exists) {
        if (required)
            throw PreparationFailure(code,
                                     "Required transaction source is missing");
        return {};
    }
    if (!std::filesystem::is_regular_file(path, error) || error)
        throw PreparationFailure(code,
                                 "Transaction source is not a regular file");
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > kMaximumPayloadBytes)
        throw PreparationFailure(code,
                                 "Transaction source exceeds the size limit");
    FailIfInjected(dependencies, read_operation, path, code,
                   "Injected transaction source read failure");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw PreparationFailure(code, "Cannot open transaction source");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size()))
        throw PreparationFailure(code,
                                 "Cannot read complete transaction source");
    return bytes;
}

std::optional<std::string> ReadOptionalBounded(
    const std::filesystem::path& path, ConfigurationPreparationErrorCode code,
    const ConfigurationTransactionPreparationDependencies& dependencies) {
    FailIfInjected(dependencies,
                   PreparationFilesystemOperation::kInspectHistory, path, code,
                   "Injected history inspection failure");
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error)
        throw PreparationFailure(code, "Cannot inspect transaction source");
    if (!exists)
        return std::nullopt;
    return ReadBounded(path, code, true, dependencies,
                       PreparationFilesystemOperation::kInspectHistory,
                       PreparationFilesystemOperation::kReadHistory);
}

void WriteFile(
    const std::filesystem::path& path, const std::string& bytes,
    const ConfigurationTransactionPreparationDependencies& dependencies) {
    FailIfInjected(dependencies,
                   PreparationFilesystemOperation::kWriteStagingArtifact, path,
                   ConfigurationPreparationErrorCode::kStagingIo,
                   "Injected transaction staging write failure");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output)
        throw PreparationFailure(ConfigurationPreparationErrorCode::kStagingIo,
                                 "Cannot write transaction staging artifact");
}

void Checkpoint(
    const ConfigurationTransactionPreparationDependencies& dependencies,
    PreparationCheckpoint checkpoint, const std::filesystem::path& path) {
    if (dependencies.checkpoint)
        dependencies.checkpoint(checkpoint, path);
}

void RemoveArtifact(
    const std::filesystem::path& path,
    const ConfigurationTransactionPreparationDependencies& dependencies,
    const char* message) {
    FailIfInjected(dependencies,
                   PreparationFilesystemOperation::kRemoveStagingArtifact, path,
                   ConfigurationPreparationErrorCode::kCleanup, message);
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error)
        throw PreparationFailure(ConfigurationPreparationErrorCode::kCleanup,
                                 message);
}

ConfigurationTransactionPreparationResult Failure(
    ConfigurationPreparationErrorCode code, std::string message,
    const std::filesystem::path& staging_directory,
    const ConfigurationTransactionPreparationDependencies& dependencies) {
    ConfigurationTransactionPreparationResult result;
    result.error = ConfigurationPreparationError{code, std::move(message), {}};
    if (!staging_directory.empty()) {
        std::error_code cleanup_error;
        if (dependencies.filesystem_failure) {
            try {
                const auto injected = dependencies.filesystem_failure(
                    PreparationFilesystemOperation::kRemoveStagingDirectory,
                    staging_directory);
                if (injected)
                    cleanup_error = *injected;
            } catch (...) {
                cleanup_error = std::make_error_code(std::errc::io_error);
            }
        }
        if (!cleanup_error)
            std::filesystem::remove_all(staging_directory, cleanup_error);
        if (cleanup_error) {
            result.error->code = ConfigurationPreparationErrorCode::kCleanup;
            result.error->message = "Cannot clean transaction staging";
            result.error->residual_staging_directory = staging_directory;
        }
    }
    return result;
}

}  // namespace

PreparedConfigurationTransaction::PreparedConfigurationTransaction(
    PreparedConfigurationTransaction&& other) noexcept
    : record_(std::move(other.record_)),
      serialized_record_(std::move(other.serialized_record_)),
      staged_record_path_(std::move(other.staged_record_path_)),
      staging_directory_(std::move(other.staging_directory_)),
      configuration_path_(std::move(other.configuration_path_)),
      history_path_(std::move(other.history_path_)),
      filesystem_failure_(std::move(other.filesystem_failure_)),
      owns_staging_(std::exchange(other.owns_staging_, false)) {}

PreparedConfigurationTransaction::~PreparedConfigurationTransaction() {
    CleanupNoThrow();
}

void PreparedConfigurationTransaction::CleanupNoThrow() noexcept {
    if (!owns_staging_)
        return;
    std::error_code error;
    try {
        if (filesystem_failure_) {
            const auto injected = filesystem_failure_(
                PreparationFilesystemOperation::kRemoveStagingDirectory,
                staging_directory_);
            if (injected)
                error = *injected;
        }
        if (!error)
            std::filesystem::remove_all(staging_directory_, error);
    } catch (...) {
        error = std::make_error_code(std::errc::io_error);
    }
    if (!error)
        owns_staging_ = false;
}

std::optional<ConfigurationPreparationError>
PreparedConfigurationTransaction::Abort() {
    if (!owns_staging_)
        return std::nullopt;
    std::error_code error;
    try {
        if (filesystem_failure_) {
            const auto injected = filesystem_failure_(
                PreparationFilesystemOperation::kRemoveStagingDirectory,
                staging_directory_);
            if (injected)
                error = *injected;
        }
        if (!error)
            std::filesystem::remove_all(staging_directory_, error);
    } catch (...) {
        error = std::make_error_code(std::errc::io_error);
    }
    if (error) {
        return ConfigurationPreparationError{
            ConfigurationPreparationErrorCode::kCleanup,
            "Cannot clean transaction staging", staging_directory_};
    }
    owns_staging_ = false;
    return std::nullopt;
}

ConfigurationTransactionPreparationResult PrepareConfigurationTransaction(
    const ConfigurationTransactionPreparationInput& input,
    const ConfigurationTransactionPreparationDependencies& dependencies) {
    std::filesystem::path staging_directory;
    bool owns_staging_directory = false;
    try {
        if (input.configuration_path.empty() ||
            input.configuration_path.filename().empty() ||
            (input.history_intent != PendingHistoryIntent::kUnchanged &&
             input.history_intent != PendingHistoryIntent::kReplace) ||
            (input.history_intent == PendingHistoryIntent::kUnchanged &&
             !input.desired_history.empty()) ||
            (input.history_intent == PendingHistoryIntent::kReplace &&
             (input.history_path.empty() ||
              input.history_path.filename().empty()))) {
            throw PreparationFailure(
                ConfigurationPreparationErrorCode::kInvalidInput,
                "Configuration transaction input is invalid");
        }

        std::optional<std::array<std::uint8_t, 16U>> identity;
        try {
            identity = dependencies.generate_transaction_id
                           ? dependencies.generate_transaction_id()
                           : GenerateIdentity();
        } catch (...) {
            throw PreparationFailure(
                ConfigurationPreparationErrorCode::kIdentityGeneration,
                "Cannot generate transaction identity");
        }
        if (!identity || IsZero(*identity)) {
            throw PreparationFailure(
                ConfigurationPreparationErrorCode::kIdentityGeneration,
                "Cannot generate transaction identity");
        }

        const auto previous_configuration = ReadBounded(
            input.configuration_path,
            ConfigurationPreparationErrorCode::kConfigurationRead, true,
            dependencies, PreparationFilesystemOperation::kInspectConfiguration,
            PreparationFilesystemOperation::kReadConfiguration);
        Checkpoint(dependencies,
                   PreparationCheckpoint::kAfterPreviousConfigurationRead,
                   input.configuration_path);

        std::optional<std::string> previous_history;
        if (input.history_intent == PendingHistoryIntent::kReplace) {
            previous_history = ReadOptionalBounded(
                input.history_path,
                ConfigurationPreparationErrorCode::kHistoryRead, dependencies);
            Checkpoint(dependencies,
                       PreparationCheckpoint::kAfterPreviousHistoryRead,
                       input.history_path);
        }

        try {
            ValidateConfiguration(input.desired_configuration);
        } catch (const std::exception& error) {
            throw PreparationFailure(
                ConfigurationPreparationErrorCode::kInvalidInput, error.what());
        }

        const auto parent = input.configuration_path.parent_path().empty()
                                ? std::filesystem::current_path()
                                : input.configuration_path.parent_path();
        staging_directory =
            parent / (".goldendict-transaction-" + IdentityHex(*identity));
        FailIfInjected(dependencies,
                       PreparationFilesystemOperation::kCreateStagingDirectory,
                       staging_directory,
                       ConfigurationPreparationErrorCode::kStagingIo,
                       "Injected transaction staging creation failure");
        std::error_code create_error;
        if (!std::filesystem::create_directory(staging_directory,
                                               create_error)) {
            throw PreparationFailure(
                create_error
                    ? ConfigurationPreparationErrorCode::kStagingIo
                    : ConfigurationPreparationErrorCode::kStagingCollision,
                create_error ? "Cannot create transaction staging"
                             : "Transaction staging collision");
        }
        owns_staging_directory = true;

        const auto previous_configuration_path =
            staging_directory / "previous-configuration";
        WriteFile(previous_configuration_path, previous_configuration,
                  dependencies);
        try {
            static_cast<void>(
                LoadConfiguration(previous_configuration_path.string()));
        } catch (const std::exception& error) {
            throw PreparationFailure(
                ConfigurationPreparationErrorCode::kInvalidInput, error.what());
        }

        const auto desired_configuration_path =
            staging_directory / "desired-configuration";
        FailIfInjected(dependencies,
                       PreparationFilesystemOperation::kWriteStagingArtifact,
                       desired_configuration_path,
                       ConfigurationPreparationErrorCode::kStagingIo,
                       "Injected transaction staging write failure");
        SaveConfiguration(desired_configuration_path.string(),
                          input.desired_configuration);
        Checkpoint(dependencies,
                   PreparationCheckpoint::kAfterDesiredConfigurationStaged,
                   desired_configuration_path);
        const auto desired_configuration = ReadBounded(
            desired_configuration_path,
            ConfigurationPreparationErrorCode::kStagingIo, true, dependencies,
            PreparationFilesystemOperation::kReadStagingArtifact,
            PreparationFilesystemOperation::kReadStagingArtifact);

        std::optional<std::string> desired_history;
        const auto desired_history_path = staging_directory / "desired-history";
        if (input.history_intent == PendingHistoryIntent::kReplace) {
            FailIfInjected(
                dependencies,
                PreparationFilesystemOperation::kWriteStagingArtifact,
                desired_history_path,
                ConfigurationPreparationErrorCode::kStagingIo,
                "Injected transaction staging write failure");
            SaveHistory(desired_history_path.string(), input.desired_history);
            Checkpoint(dependencies,
                       PreparationCheckpoint::kAfterDesiredHistoryStaged,
                       desired_history_path);
            desired_history = ReadBounded(
                desired_history_path,
                ConfigurationPreparationErrorCode::kStagingIo, true,
                dependencies,
                PreparationFilesystemOperation::kReadStagingArtifact,
                PreparationFilesystemOperation::kReadStagingArtifact);
        }

        PendingConfigurationTransactionRecord record;
        record.transaction_id = *identity;
        record.desired_configuration =
            MakePendingTransactionPayload(desired_configuration);
        record.history_intent = input.history_intent;
        if (desired_history)
            record.desired_history =
                MakePendingTransactionPayload(std::move(*desired_history));
        record.previous_configuration = {
            true, MakePendingTransactionPayload(previous_configuration)};
        if (previous_history) {
            record.previous_history = {true, MakePendingTransactionPayload(
                                                 std::move(*previous_history))};
        }

        const auto serialized =
            SerializePendingConfigurationTransaction(record);
        const auto staged_record_path = staging_directory / "pending-record";
        WriteFile(staged_record_path, serialized, dependencies);
        Checkpoint(dependencies,
                   PreparationCheckpoint::kAfterPendingRecordStaged,
                   staged_record_path);
        const auto verified_bytes = ReadBounded(
            staged_record_path, ConfigurationPreparationErrorCode::kStagingIo,
            true, dependencies,
            PreparationFilesystemOperation::kReadStagingArtifact,
            PreparationFilesystemOperation::kReadStagingArtifact);
        bool verified = verified_bytes == serialized;
        if (verified) {
            try {
                verified = ParsePendingConfigurationTransaction(
                               verified_bytes) == record;
            } catch (const std::exception&) {
                verified = false;
            }
        }
        if (!verified) {
            throw PreparationFailure(
                ConfigurationPreparationErrorCode::kVerification,
                "Transaction staging verification failed");
        }

        RemoveArtifact(
            previous_configuration_path, dependencies,
            "Cannot remove temporary previous configuration staging");
        RemoveArtifact(desired_configuration_path, dependencies,
                       "Cannot remove temporary configuration staging");
        if (input.history_intent == PendingHistoryIntent::kReplace) {
            RemoveArtifact(desired_history_path, dependencies,
                           "Cannot remove temporary history staging");
        }

        PreparedConfigurationTransaction prepared;
        prepared.record_ = std::move(record);
        prepared.serialized_record_ = serialized;
        prepared.staged_record_path_ = staged_record_path;
        prepared.staging_directory_ = staging_directory;
        prepared.configuration_path_ = input.configuration_path;
        prepared.history_path_ = input.history_path;
        prepared.filesystem_failure_ = dependencies.filesystem_failure;
        prepared.owns_staging_ = true;
        ConfigurationTransactionPreparationResult result;
        result.prepared.emplace(std::move(prepared));
        return result;
    } catch (const PreparationFailure& error) {
        return Failure(error.code(), error.what(),
                       owns_staging_directory ? staging_directory
                                              : std::filesystem::path{},
                       dependencies);
    } catch (const std::exception& error) {
        return Failure(ConfigurationPreparationErrorCode::kInvalidInput,
                       error.what(),
                       owns_staging_directory ? staging_directory
                                              : std::filesystem::path{},
                       dependencies);
    } catch (...) {
        return Failure(ConfigurationPreparationErrorCode::kInvalidInput,
                       "Unknown configuration preparation failure",
                       owns_staging_directory ? staging_directory
                                              : std::filesystem::path{},
                       dependencies);
    }
}

}  // namespace goldendict::core
