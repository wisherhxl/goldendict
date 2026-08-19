// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_LIFECYCLE_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_LIFECYCLE_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core::dictionary {

struct FullTextIndexPolicy {
    bool enabled = true;
    std::uint32_t maximum_dictionary_articles = 0U;
    std::string disabled_format_types;

    friend bool operator==(const FullTextIndexPolicy& left,
                           const FullTextIndexPolicy& right) {
        return left.enabled == right.enabled &&
               left.maximum_dictionary_articles ==
                   right.maximum_dictionary_articles &&
               left.disabled_format_types == right.disabled_format_types;
    }

    friend bool operator!=(const FullTextIndexPolicy& left,
                           const FullTextIndexPolicy& right) {
        return !(left == right);
    }
};

FullTextIndexPolicy ProjectFullTextIndexPolicy(
    const ApplicationPreferences& preferences);

struct FullTextIndexRegistrationMetadata {
    std::string dictionary_id;
    std::string format_type;
    std::size_t article_count = 0U;

    friend bool operator==(const FullTextIndexRegistrationMetadata& left,
                           const FullTextIndexRegistrationMetadata& right) {
        return left.dictionary_id == right.dictionary_id &&
               left.format_type == right.format_type &&
               left.article_count == right.article_count;
    }
};

bool IsFullTextIndexPolicyEligible(
    const FullTextIndexRegistrationMetadata& metadata,
    const FullTextIndexPolicy& policy) noexcept;

struct FullTextIndexWorkIdentity {
    std::uint64_t generation = 0U;
    std::string dictionary_id;

    friend bool operator==(const FullTextIndexWorkIdentity& left,
                           const FullTextIndexWorkIdentity& right) {
        return left.generation == right.generation &&
               left.dictionary_id == right.dictionary_id;
    }

    friend bool operator!=(const FullTextIndexWorkIdentity& left,
                           const FullTextIndexWorkIdentity& right) {
        return !(left == right);
    }
};

struct FullTextIndexRebuildIntent {
    FullTextIndexWorkIdentity identity;
    FullTextIndexPolicy policy;

    friend bool operator==(const FullTextIndexRebuildIntent& left,
                           const FullTextIndexRebuildIntent& right) {
        return left.identity == right.identity && left.policy == right.policy;
    }
};

struct FullTextIndexCancelIntent {
    FullTextIndexWorkIdentity identity;

    friend bool operator==(const FullTextIndexCancelIntent& left,
                           const FullTextIndexCancelIntent& right) {
        return left.identity == right.identity;
    }
};

enum class FullTextIndexLifecycleState {
    kUnavailable,
    kNotIndexed,
    kPolicyExcluded,
    kWorkRequested,
    kWorking,
    kCurrent,
    kCancelled,
    kFailed,
};

class FullTextIndexLifecycleSnapshot final {
   public:
    FullTextIndexLifecycleSnapshot(FullTextIndexWorkIdentity identity,
                                   FullTextIndexLifecycleState state,
                                   bool format_capable,
                                   std::string source_revision)
        : identity_(std::move(identity)),
          state_(state),
          format_capable_(format_capable),
          source_revision_(std::move(source_revision)) {}

    const FullTextIndexWorkIdentity& identity() const noexcept {
        return identity_;
    }

    FullTextIndexLifecycleState state() const noexcept { return state_; }

    bool format_capable() const noexcept { return format_capable_; }

    const std::string& source_revision() const noexcept {
        return source_revision_;
    }

    friend bool operator==(const FullTextIndexLifecycleSnapshot& left,
                           const FullTextIndexLifecycleSnapshot& right) {
        return left.identity_ == right.identity_ &&
               left.state_ == right.state_ &&
               left.format_capable_ == right.format_capable_ &&
               left.source_revision_ == right.source_revision_;
    }

   private:
    FullTextIndexWorkIdentity identity_;
    FullTextIndexLifecycleState state_;
    bool format_capable_;
    std::string source_revision_;
};

struct FullTextIndexWorkRequest {
    FullTextIndexWorkIdentity identity;
    FullTextIndexPolicy policy;
    std::string source_revision;
    std::size_t maximum_documents = 0U;
    std::size_t maximum_document_bytes = 0U;
    std::size_t maximum_corpus_bytes = 0U;
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max();
    const CancellationToken* cancellation = nullptr;
};

enum class FullTextIndexWorkStatus { kCompleted, kCancelled, kFailed };

struct FullTextIndexWorkResult {
    FullTextIndexWorkStatus status = FullTextIndexWorkStatus::kFailed;
    std::string message;
};

class FullTextIndexFormatWorkPort {
   public:
    virtual ~FullTextIndexFormatWorkPort() = default;

    virtual bool IsFullTextIndexSupported() const noexcept = 0;
    virtual std::string FullTextIndexSourceRevision() const = 0;

    FullTextIndexWorkResult PerformFullTextIndexWork(
        const FullTextIndexWorkRequest& request) noexcept;

   private:
    virtual FullTextIndexWorkResult DoPerformFullTextIndexWork(
        const FullTextIndexWorkRequest& request) = 0;
};

class FullTextIndexLifecycleCoordinator final {
   public:
    FullTextIndexLifecycleCoordinator();
    ~FullTextIndexLifecycleCoordinator();

    FullTextIndexLifecycleCoordinator(
        const FullTextIndexLifecycleCoordinator&) = delete;
    FullTextIndexLifecycleCoordinator& operator=(
        const FullTextIndexLifecycleCoordinator&) = delete;

    bool RegisterDictionary(
        FullTextIndexRegistrationMetadata metadata,
        std::shared_ptr<FullTextIndexFormatWorkPort> format_work_port);
    bool SubmitRebuild(const FullTextIndexRebuildIntent& intent);
    bool ExecuteBoundedWork(FullTextIndexWorkRequest request);
    bool Cancel(const FullTextIndexCancelIntent& intent) noexcept;

    std::optional<FullTextIndexLifecycleSnapshot> Snapshot(
        const std::string& dictionary_id) const;

   private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_LIFECYCLE_H_
