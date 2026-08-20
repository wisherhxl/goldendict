// SPDX-License-Identifier: GPL-3.0-or-later

#include "configuration_transaction_persistence.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace goldendict::core {
namespace {

class PersistenceFailure final : public std::runtime_error {
   public:
    PersistenceFailure(PendingFailureDestination destination,
                       PendingFailureCategory category, std::string identifier,
                       std::string message)
        : std::runtime_error(std::move(message)),
          identity_{PendingFailureOperation::kPersistDesired, destination,
                    category, std::move(identifier)} {}

    const PendingFailureIdentity& identity() const noexcept {
        return identity_;
    }

   private:
    PendingFailureIdentity identity_;
};

PendingFailureCategory Category(const std::error_code& error) {
    if (error == std::errc::no_such_file_or_directory)
        return PendingFailureCategory::kNotFound;
    if (error == std::errc::permission_denied ||
        error == std::errc::operation_not_permitted ||
        error == std::errc::read_only_file_system)
        return PendingFailureCategory::kPermission;
    if (error == std::errc::no_space_on_device ||
        error == std::errc::file_too_large ||
        error == std::errc::not_enough_memory ||
        error == std::errc::too_many_files_open ||
        error == std::errc::too_many_files_open_in_system)
        return PendingFailureCategory::kResourceLimit;
    if (error == std::errc::file_exists || error == std::errc::is_a_directory ||
        error == std::errc::not_a_directory ||
        error == std::errc::too_many_symbolic_link_levels)
        return PendingFailureCategory::kRejected;
    return PendingFailureCategory::kIo;
}

void Inject(const ConfigurationPersistenceDependencies& dependencies,
            ConfigurationPersistenceOperation operation,
            const std::filesystem::path& path,
            PendingFailureDestination destination, const char* identifier) {
    if (!dependencies.filesystem_failure)
        return;
    try {
        const auto error = dependencies.filesystem_failure(operation, path);
        if (error) {
            throw PersistenceFailure(destination, Category(*error), identifier,
                                     error->message());
        }
    } catch (const PersistenceFailure&) {
        throw;
    } catch (...) {
        throw PersistenceFailure(destination, PendingFailureCategory::kUnknown,
                                 identifier, "Persistence injection failed");
    }
}

void Checkpoint(const ConfigurationPersistenceDependencies& dependencies,
                ConfigurationPersistenceCheckpoint checkpoint,
                const std::filesystem::path& path,
                PendingFailureDestination destination, const char* identifier) {
    if (!dependencies.checkpoint)
        return;
    try {
        dependencies.checkpoint(checkpoint, path);
    } catch (...) {
        throw PersistenceFailure(destination, PendingFailureCategory::kUnknown,
                                 identifier, "Persistence checkpoint failed");
    }
}

void RequireSafeExisting(
    const std::filesystem::path& path, bool missing_allowed,
    const ConfigurationPersistenceDependencies& dependencies,
    PendingFailureDestination destination) {
    Inject(dependencies, ConfigurationPersistenceOperation::kInspectPath, path,
           destination, "path_inspection_failed");
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) {
        if (missing_allowed)
            return;
        throw PersistenceFailure(destination, PendingFailureCategory::kNotFound,
                                 "path_missing", "Required path is missing");
    }
    if (error) {
        throw PersistenceFailure(destination, Category(error),
                                 "path_inspection_failed", error.message());
    }
    if (!std::filesystem::exists(status)) {
        if (missing_allowed)
            return;
        throw PersistenceFailure(destination, PendingFailureCategory::kNotFound,
                                 "path_missing", "Required path is missing");
    }
    if (!std::filesystem::is_regular_file(status)) {
        throw PersistenceFailure(destination, PendingFailureCategory::kRejected,
                                 "unsafe_path_type",
                                 "Persistence path is not a regular file");
    }
}

void WriteAndSyncTemporary(
    const std::filesystem::path& path, std::string_view bytes,
    const ConfigurationPersistenceDependencies& dependencies,
    PendingFailureDestination destination) {
    Inject(dependencies, ConfigurationPersistenceOperation::kCreateTemporary,
           path, destination, "temp_create_failed");
#ifdef _WIN32
    std::FILE* file = nullptr;
    const auto open_error = _wfopen_s(&file, path.c_str(), L"wbx");
    if (!file)
        errno = static_cast<int>(open_error);
#else
    std::FILE* file = std::fopen(path.string().c_str(), "wbx");
#endif
    if (!file) {
        throw PersistenceFailure(
            destination,
            errno == EEXIST
                ? PendingFailureCategory::kRejected
                : Category(std::error_code(errno, std::generic_category())),
            errno == EEXIST ? "temp_collision" : "temp_create_failed",
            std::strerror(errno));
    }
    const auto close_file = [&file]() {
        if (file) {
            std::fclose(file);
            file = nullptr;
        }
    };
    try {
        Inject(dependencies, ConfigurationPersistenceOperation::kWriteTemporary,
               path, destination, "temp_write_failed");
        if (std::fwrite(bytes.data(), 1U, bytes.size(), file) != bytes.size())
            throw PersistenceFailure(destination, PendingFailureCategory::kIo,
                                     "temp_write_failed",
                                     "Cannot write persistence temporary");
        Inject(dependencies, ConfigurationPersistenceOperation::kFlushTemporary,
               path, destination, "file_flush_failed");
        if (std::fflush(file) != 0)
            throw PersistenceFailure(destination, PendingFailureCategory::kIo,
                                     "file_flush_failed",
                                     "Cannot flush persistence temporary");
        Inject(dependencies, ConfigurationPersistenceOperation::kSyncTemporary,
               path, destination, "file_sync_failed");
#ifdef _WIN32
        if (_commit(_fileno(file)) != 0)
#elif defined(__APPLE__)
        if (::fcntl(fileno(file), F_FULLFSYNC) != 0)
#else
        if (::fsync(fileno(file)) != 0)
#endif
            throw PersistenceFailure(destination, PendingFailureCategory::kIo,
                                     "file_sync_failed",
                                     "Cannot sync persistence temporary");
        if (std::fclose(file) != 0) {
            file = nullptr;
            throw PersistenceFailure(destination, PendingFailureCategory::kIo,
                                     "file_close_failed",
                                     "Cannot close persistence temporary");
        }
        file = nullptr;
        Checkpoint(dependencies,
                   ConfigurationPersistenceCheckpoint::kAfterTemporarySynced,
                   path, destination, "after_file_sync");
    } catch (...) {
        close_file();
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw;
    }
}

void RemovePrivateTemporary(
    const std::filesystem::path& path,
    const ConfigurationPersistenceDependencies& dependencies,
    PendingFailureDestination destination) {
    Inject(dependencies, ConfigurationPersistenceOperation::kRemoveTemporary,
           path, destination, "temp_cleanup_failed");
    std::error_code error;
    if (!std::filesystem::remove(path, error) && error) {
        throw PersistenceFailure(destination, Category(error),
                                 "temp_cleanup_failed", error.message());
    }
}

void PublishNoReplace(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    const ConfigurationPersistenceDependencies& dependencies) {
    Inject(dependencies, ConfigurationPersistenceOperation::kPublishDecision,
           destination, PendingFailureDestination::kPendingRecord,
           "decision_publish_failed");
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_WRITE_THROUGH)) {
        const auto error = std::error_code(static_cast<int>(GetLastError()),
                                           std::system_category());
        const bool collision = error.value() == ERROR_ALREADY_EXISTS ||
                               error.value() == ERROR_FILE_EXISTS;
        throw PersistenceFailure(
            PendingFailureDestination::kPendingRecord,
            collision ? PendingFailureCategory::kRejected : Category(error),
            collision ? "pending_collision" : "decision_publish_failed",
            error.message());
    }
#else
    if (::linkat(AT_FDCWD, temporary.c_str(), AT_FDCWD, destination.c_str(),
                 0) != 0) {
        const auto error = std::error_code(errno, std::generic_category());
        throw PersistenceFailure(
            PendingFailureDestination::kPendingRecord, Category(error),
            errno == EEXIST ? "pending_collision" : "decision_publish_failed",
            error.message());
    }
#endif
}

void Replace(const std::filesystem::path& temporary,
             const std::filesystem::path& destination,
             const ConfigurationPersistenceDependencies& dependencies,
             PendingFailureDestination failure_destination) {
    Inject(dependencies, ConfigurationPersistenceOperation::kReplaceDestination,
           destination, failure_destination, "atomic_replace_failed");
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto error = std::error_code(static_cast<int>(GetLastError()),
                                           std::system_category());
        throw PersistenceFailure(failure_destination, Category(error),
                                 "atomic_replace_failed", error.message());
    }
#else
    if (::rename(temporary.c_str(), destination.c_str()) != 0) {
        const auto error = std::error_code(errno, std::generic_category());
        throw PersistenceFailure(failure_destination, Category(error),
                                 "atomic_replace_failed", error.message());
    }
#endif
}

void SyncDirectory(const std::filesystem::path& directory,
                   const ConfigurationPersistenceDependencies& dependencies,
                   PendingFailureDestination destination,
                   const std::function<void()>& after_sync = {}) {
    Inject(dependencies, ConfigurationPersistenceOperation::kSyncDirectory,
           directory, destination, "directory_sync_failed");
#ifndef _WIN32
    int flags = O_RDONLY;
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
    const int descriptor = ::open(directory.c_str(), flags);
    if (descriptor < 0) {
        const auto error = std::error_code(errno, std::generic_category());
        throw PersistenceFailure(destination, Category(error),
                                 "directory_sync_failed", error.message());
    }
    const int result = ::fsync(descriptor);
    const int saved_errno = errno;
    ::close(descriptor);
    if (result != 0) {
        const auto error =
            std::error_code(saved_errno, std::generic_category());
        throw PersistenceFailure(destination, Category(error),
                                 "directory_sync_failed", error.message());
    }
#endif
    if (after_sync)
        after_sync();
    Checkpoint(dependencies,
               ConfigurationPersistenceCheckpoint::kAfterDirectorySynced,
               directory, destination, "after_directory_sync");
}

std::string ReadFile(const std::filesystem::path& path,
                     const ConfigurationPersistenceDependencies& dependencies,
                     PendingFailureDestination destination) {
    Inject(dependencies, ConfigurationPersistenceOperation::kReadVerification,
           path, destination, "verification_read_failed");
    RequireSafeExisting(path, false, dependencies, destination);
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw PersistenceFailure(destination, PendingFailureCategory::kIo,
                                 "verification_read_failed",
                                 "Cannot open persisted destination");
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void VerifyPayload(const std::filesystem::path& path,
                   const PendingTransactionPayload& payload,
                   const ConfigurationPersistenceDependencies& dependencies,
                   PendingFailureDestination destination) {
    Inject(dependencies, ConfigurationPersistenceOperation::kVerifyPayload,
           path, destination, "verification_failed");
    const auto bytes = ReadFile(path, dependencies, destination);
    bool matches = bytes.size() == payload.size && bytes == payload.bytes;
    if (matches) {
        try {
            matches =
                MakePendingTransactionPayload(bytes).sha256 == payload.sha256;
        } catch (...) {
            matches = false;
        }
    }
    if (!matches) {
        throw PersistenceFailure(
            destination, PendingFailureCategory::kInvalidData,
            "verification_failed", "Persisted destination verification failed");
    }
}

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

std::filesystem::path TemporaryPath(
    const std::filesystem::path& destination,
    const std::array<std::uint8_t, 16U>& identity, std::string_view purpose) {
    return destination.parent_path() /
           ("." + destination.filename().string() + ".goldendict-" +
            IdentityHex(identity) + "-" + std::string(purpose) + ".tmp");
}

ConfigurationPersistenceError Error(const PersistenceFailure& failure) {
    return {failure.identity(), failure.what()};
}

void RequirePendingIdentity(
    const std::filesystem::path& pending_path,
    const std::array<std::uint8_t, 16U>& identity,
    const ConfigurationPersistenceDependencies& dependencies) {
    try {
        const auto record = ParsePendingConfigurationTransaction(
            ReadFile(pending_path, dependencies,
                     PendingFailureDestination::kPendingRecord));
        if (record.transaction_id != identity) {
            throw PersistenceFailure(PendingFailureDestination::kPendingRecord,
                                     PendingFailureCategory::kRejected,
                                     "pending_identity_mismatch",
                                     "Pending transaction identity changed");
        }
    } catch (const PersistenceFailure&) {
        throw;
    } catch (...) {
        throw PersistenceFailure(PendingFailureDestination::kPendingRecord,
                                 PendingFailureCategory::kInvalidData,
                                 "pending_record_invalid",
                                 "Pending transaction record is invalid");
    }
}

void PublishRecordUpdate(
    const PendingConfigurationTransactionRecord& record,
    const std::filesystem::path& pending_path, std::string_view purpose,
    const ConfigurationPersistenceDependencies& dependencies,
    const std::function<void()>& after_namespace_publication,
    const std::function<void()>& after_durability_confirmation) {
    RequirePendingIdentity(pending_path, record.transaction_id, dependencies);
    const auto temporary =
        TemporaryPath(pending_path, record.transaction_id, purpose);
    bool temporary_ready = false;
    try {
        WriteAndSyncTemporary(
            temporary, SerializePendingConfigurationTransaction(record),
            dependencies, PendingFailureDestination::kPendingRecord);
        temporary_ready = true;
        RequirePendingIdentity(pending_path, record.transaction_id,
                               dependencies);
        Replace(temporary, pending_path, dependencies,
                PendingFailureDestination::kPendingRecord);
    } catch (...) {
        if (temporary_ready) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        throw;
    }
    after_namespace_publication();
    Checkpoint(dependencies,
               ConfigurationPersistenceCheckpoint::kAfterRecordReplaced,
               pending_path, PendingFailureDestination::kPendingRecord,
               "after_record_replace");
    SyncDirectory(pending_path.parent_path(), dependencies,
                  PendingFailureDestination::kPendingRecord,
                  after_durability_confirmation);
}

void PublishPayload(const PendingTransactionPayload& payload,
                    const std::filesystem::path& destination,
                    std::string_view purpose,
                    const std::array<std::uint8_t, 16U>& identity,
                    const ConfigurationPersistenceDependencies& dependencies,
                    PendingFailureDestination failure_destination,
                    ConfigurationPersistenceCheckpoint replaced_checkpoint,
                    ConfigurationPersistenceCheckpoint verified_checkpoint) {
    RequireSafeExisting(destination, true, dependencies, failure_destination);
    const auto temporary = TemporaryPath(destination, identity, purpose);
    bool temporary_ready = false;
    try {
        WriteAndSyncTemporary(temporary, payload.bytes, dependencies,
                              failure_destination);
        temporary_ready = true;
        RequireSafeExisting(destination, true, dependencies,
                            failure_destination);
        Replace(temporary, destination, dependencies, failure_destination);
    } catch (...) {
        if (temporary_ready) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        throw;
    }
    Checkpoint(dependencies, replaced_checkpoint, destination,
               failure_destination, "after_destination_replace");
    SyncDirectory(destination.parent_path(), dependencies, failure_destination);
    VerifyPayload(destination, payload, dependencies, failure_destination);
    Checkpoint(dependencies, verified_checkpoint, destination,
               failure_destination, "after_destination_verification");
}

}  // namespace

std::filesystem::path PendingConfigurationTransactionPath(
    const std::filesystem::path& configuration_path) {
    auto parent = configuration_path.parent_path();
    if (parent.empty())
        parent = std::filesystem::current_path();
    return parent / ".goldendict-pending-configuration-transaction";
}

ConfigurationPersistenceResult PersistDesiredConfiguration(
    PreparedConfigurationTransaction prepared,
    const ConfigurationPersistenceDependencies& dependencies) {
    ConfigurationPersistenceResult result;
    const auto pending_path =
        PendingConfigurationTransactionPath(prepared.configuration_path());
    auto record = prepared.record();
    const auto identity = record.transaction_id;

    try {
        RequireSafeExisting(pending_path, true, dependencies,
                            PendingFailureDestination::kPendingRecord);
        std::error_code existing_error;
        if (std::filesystem::exists(std::filesystem::symlink_status(
                pending_path, existing_error))) {
            throw PersistenceFailure(PendingFailureDestination::kPendingRecord,
                                     PendingFailureCategory::kRejected,
                                     "pending_collision",
                                     "Pending transaction already exists");
        }
        if (existing_error &&
            existing_error != std::errc::no_such_file_or_directory) {
            throw PersistenceFailure(PendingFailureDestination::kPendingRecord,
                                     Category(existing_error),
                                     "path_inspection_failed",
                                     existing_error.message());
        }

        record.phase = PendingTransactionPhase::kDesiredCommit;
        record.failure.reset();
        const auto temporary =
            TemporaryPath(pending_path, identity, "decision");
        bool temporary_ready = false;
        try {
            WriteAndSyncTemporary(
                temporary, SerializePendingConfigurationTransaction(record),
                dependencies, PendingFailureDestination::kPendingRecord);
            temporary_ready = true;
            PublishNoReplace(temporary, pending_path, dependencies);
        } catch (...) {
            if (temporary_ready) {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
            }
            throw;
        }

        // Namespace publication is the irreversible boundary. Nothing fallible
        // may occur before staging ownership is released.
        prepared.ReleaseForDecision();
        result.decision_path_published = true;
        result.namespace_published_phase =
            PendingTransactionPhase::kDesiredCommit;
        Checkpoint(dependencies,
                   ConfigurationPersistenceCheckpoint::kAfterDecisionPublished,
                   pending_path, PendingFailureDestination::kPendingRecord,
                   "after_decision_publish");
#ifndef _WIN32
        RemovePrivateTemporary(temporary, dependencies,
                               PendingFailureDestination::kPendingRecord);
#endif
        SyncDirectory(pending_path.parent_path(), dependencies,
                      PendingFailureDestination::kPendingRecord, [&result] {
                          result.confirmed_durable_phase =
                              PendingTransactionPhase::kDesiredCommit;
                      });

        record.phase = PendingTransactionPhase::kDesiredPersistenceApplying;
        PublishRecordUpdate(
            record, pending_path, "phase-applying", dependencies,
            [&result, &record] {
                result.namespace_published_phase = record.phase;
            },
            [&result, &record] {
                result.confirmed_durable_phase = record.phase;
            });

        PublishPayload(
            record.desired_configuration, prepared.configuration_path(),
            "configuration", identity, dependencies,
            PendingFailureDestination::kConfiguration,
            ConfigurationPersistenceCheckpoint::kAfterConfigurationReplaced,
            ConfigurationPersistenceCheckpoint::kAfterConfigurationVerified);
        if (record.history_intent == PendingHistoryIntent::kReplace) {
            PublishPayload(
                *record.desired_history, prepared.history_path(), "history",
                identity, dependencies, PendingFailureDestination::kHistory,
                ConfigurationPersistenceCheckpoint::kAfterHistoryReplaced,
                ConfigurationPersistenceCheckpoint::kAfterHistoryVerified);
        }

        record.phase = PendingTransactionPhase::kDesiredPersistenceApplied;
        PublishRecordUpdate(
            record, pending_path, "phase-applied", dependencies,
            [&result, &record] {
                result.namespace_published_phase = record.phase;
            },
            [&result, &record] {
                result.confirmed_durable_phase = record.phase;
            });
        result.outcome =
            ConfigurationPersistenceOutcome::kDesiredPersistenceApplied;
        return result;
    } catch (const PersistenceFailure& failure) {
        result.error = Error(failure);
    } catch (const std::bad_alloc&) {
        result.error = ConfigurationPersistenceError{
            {PendingFailureOperation::kPersistDesired,
             PendingFailureDestination::kPendingRecord,
             PendingFailureCategory::kResourceLimit, "allocation_failed"},
            "Persistence allocation failed"};
    } catch (const std::exception& failure) {
        result.error = ConfigurationPersistenceError{
            {PendingFailureOperation::kPersistDesired,
             PendingFailureDestination::kPendingRecord,
             PendingFailureCategory::kInvalidData, "codec_failed"},
            failure.what()};
    } catch (...) {
        result.error = ConfigurationPersistenceError{
            {PendingFailureOperation::kPersistDesired,
             PendingFailureDestination::kPendingRecord,
             PendingFailureCategory::kUnknown, "unknown_failure"},
            "Unknown desired persistence failure"};
    }

    if (!result.decision_path_published) {
        result.outcome = ConfigurationPersistenceOutcome::kPreDecisionFailure;
        result.abortable_prepared.emplace(std::move(prepared));
        return result;
    }

    result.outcome = ConfigurationPersistenceOutcome::kPostDecisionFailure;
    record.phase = PendingTransactionPhase::kDesiredPersistenceFailed;
    record.failure = result.error->identity;
    try {
        PublishRecordUpdate(
            record, pending_path, "phase-failed", dependencies,
            [&result, &record] {
                result.namespace_published_phase = record.phase;
                result.failure_evidence_namespace_published = true;
            },
            [&result, &record] {
                result.confirmed_durable_phase = record.phase;
                result.failure_evidence_durable = true;
            });
    } catch (const PersistenceFailure& failure) {
        result.failure_evidence_error = Error(failure);
    } catch (const std::exception& failure) {
        result.failure_evidence_error = ConfigurationPersistenceError{
            {PendingFailureOperation::kPersistDesired,
             PendingFailureDestination::kPendingRecord,
             PendingFailureCategory::kInvalidData,
             "failure_record_codec_failed"},
            failure.what()};
    } catch (...) {
        result.failure_evidence_error = ConfigurationPersistenceError{
            {PendingFailureOperation::kPersistDesired,
             PendingFailureDestination::kPendingRecord,
             PendingFailureCategory::kUnknown, "failure_record_publish_failed"},
            "Unknown failure-evidence publication failure"};
    }
    return result;
}

}  // namespace goldendict::core
