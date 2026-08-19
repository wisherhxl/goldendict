// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_index_lifecycle.h"

#include <atomic>
#include <exception>
#include <map>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>

namespace goldendict::core::dictionary {

FullTextIndexPolicy ProjectFullTextIndexPolicy(
    const ApplicationPreferences& preferences) {
    return {preferences.full_text_search_enabled,
            preferences.full_text_maximum_dictionary_articles,
            preferences.full_text_disabled_types};
}

FullTextIndexWorkResult FullTextIndexFormatWorkPort::PerformFullTextIndexWork(
    const FullTextIndexWorkRequest& request) noexcept {
    try {
        return DoPerformFullTextIndexWork(request);
    } catch (const std::bad_alloc&) {
        return {FullTextIndexWorkStatus::kFailed, "Resource limit exceeded"};
    } catch (const std::length_error&) {
        return {FullTextIndexWorkStatus::kFailed, "Resource limit exceeded"};
    } catch (const std::exception& error) {
        return {FullTextIndexWorkStatus::kFailed, error.what()};
    } catch (...) {
        return {FullTextIndexWorkStatus::kFailed,
                "Unknown full-text index work failure"};
    }
}

namespace {

class CoordinatorCancellation final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override {
        return cancelled_.load();
    }

    void Cancel() noexcept { cancelled_.store(true); }

   private:
    std::atomic<bool> cancelled_{false};
};

}  // namespace

class FullTextIndexLifecycleCoordinator::Implementation final {
   public:
    struct Generation final {
        FullTextIndexWorkIdentity identity;
        FullTextIndexPolicy policy;
        bool format_capable = false;
        std::string source_revision;
        FullTextIndexLifecycleState state =
            FullTextIndexLifecycleState::kNotIndexed;
        std::shared_ptr<CoordinatorCancellation> cancellation;
    };

    struct Entry final {
        std::shared_ptr<FullTextIndexFormatWorkPort> port;
        std::shared_ptr<Generation> current;
        bool has_accepted_generation = false;
    };

    mutable std::mutex mutex;
    std::map<std::string, Entry> entries;
};

FullTextIndexLifecycleCoordinator::FullTextIndexLifecycleCoordinator()
    : implementation_(std::make_unique<Implementation>()) {}

FullTextIndexLifecycleCoordinator::~FullTextIndexLifecycleCoordinator() =
    default;

bool FullTextIndexLifecycleCoordinator::RegisterDictionary(
    std::string dictionary_id,
    std::shared_ptr<FullTextIndexFormatWorkPort> format_work_port) {
    if (dictionary_id.empty() || format_work_port == nullptr)
        return false;

    const bool format_capable = format_work_port->IsFullTextIndexSupported();
    std::string source_revision;
    FullTextIndexLifecycleState state =
        format_capable ? FullTextIndexLifecycleState::kNotIndexed
                       : FullTextIndexLifecycleState::kUnavailable;
    if (format_capable) {
        try {
            source_revision = format_work_port->FullTextIndexSourceRevision();
        } catch (...) {
            state = FullTextIndexLifecycleState::kFailed;
        }
    }

    auto generation = std::make_shared<Implementation::Generation>();
    generation->identity.dictionary_id = dictionary_id;
    generation->format_capable = format_capable;
    generation->source_revision = std::move(source_revision);
    generation->state = state;

    std::lock_guard lock(implementation_->mutex);
    return implementation_->entries
        .emplace(std::move(dictionary_id),
                 Implementation::Entry{std::move(format_work_port),
                                       std::move(generation), false})
        .second;
}

bool FullTextIndexLifecycleCoordinator::SubmitRebuild(
    const FullTextIndexRebuildIntent& intent) {
    std::shared_ptr<FullTextIndexFormatWorkPort> port;
    {
        std::lock_guard lock(implementation_->mutex);
        const auto found =
            implementation_->entries.find(intent.identity.dictionary_id);
        if (found == implementation_->entries.end() ||
            (found->second.has_accepted_generation &&
             intent.identity.generation <=
                 found->second.current->identity.generation)) {
            return false;
        }
        port = found->second.port;
    }

    const bool format_capable = port->IsFullTextIndexSupported();
    std::string source_revision;
    FullTextIndexLifecycleState state =
        format_capable ? FullTextIndexLifecycleState::kWorkRequested
                       : FullTextIndexLifecycleState::kUnavailable;
    if (format_capable) {
        try {
            source_revision = port->FullTextIndexSourceRevision();
        } catch (...) {
            state = FullTextIndexLifecycleState::kFailed;
        }
    }

    auto replacement = std::make_shared<Implementation::Generation>();
    replacement->identity = intent.identity;
    replacement->policy = intent.policy;
    replacement->format_capable = format_capable;
    replacement->source_revision = std::move(source_revision);
    replacement->state = state;
    replacement->cancellation = std::make_shared<CoordinatorCancellation>();

    std::lock_guard lock(implementation_->mutex);
    const auto found =
        implementation_->entries.find(intent.identity.dictionary_id);
    if (found == implementation_->entries.end() || found->second.port != port ||
        (found->second.has_accepted_generation &&
         intent.identity.generation <=
             found->second.current->identity.generation)) {
        return false;
    }
    if (found->second.current->cancellation != nullptr)
        found->second.current->cancellation->Cancel();
    found->second.current = std::move(replacement);
    found->second.has_accepted_generation = true;
    return true;
}

bool FullTextIndexLifecycleCoordinator::ExecuteBoundedWork(
    FullTextIndexWorkRequest request) {
    std::shared_ptr<FullTextIndexFormatWorkPort> port;
    std::shared_ptr<Implementation::Generation> generation;
    {
        std::lock_guard lock(implementation_->mutex);
        const auto found =
            implementation_->entries.find(request.identity.dictionary_id);
        if (found == implementation_->entries.end() ||
            found->second.current->identity != request.identity ||
            found->second.current->state !=
                FullTextIndexLifecycleState::kWorkRequested) {
            return false;
        }
        generation = found->second.current;
        generation->state = FullTextIndexLifecycleState::kWorking;
        port = found->second.port;
        request.policy = generation->policy;
        request.source_revision = generation->source_revision;
        request.cancellation = generation->cancellation.get();
    }

    const auto result = port->PerformFullTextIndexWork(request);

    std::lock_guard lock(implementation_->mutex);
    const auto found =
        implementation_->entries.find(request.identity.dictionary_id);
    if (found == implementation_->entries.end() ||
        found->second.current != generation) {
        return true;
    }
    if (generation->cancellation->IsCancellationRequested()) {
        generation->state = FullTextIndexLifecycleState::kCancelled;
    } else {
        switch (result.status) {
            case FullTextIndexWorkStatus::kCompleted:
                generation->state = FullTextIndexLifecycleState::kCurrent;
                break;
            case FullTextIndexWorkStatus::kCancelled:
                generation->state = FullTextIndexLifecycleState::kCancelled;
                break;
            case FullTextIndexWorkStatus::kFailed:
                generation->state = FullTextIndexLifecycleState::kFailed;
                break;
        }
    }
    return true;
}

bool FullTextIndexLifecycleCoordinator::Cancel(
    const FullTextIndexCancelIntent& intent) noexcept {
    std::lock_guard lock(implementation_->mutex);
    const auto found =
        implementation_->entries.find(intent.identity.dictionary_id);
    if (found == implementation_->entries.end() ||
        !found->second.has_accepted_generation ||
        found->second.current->identity != intent.identity) {
        return false;
    }
    auto& generation = *found->second.current;
    if (generation.cancellation != nullptr)
        generation.cancellation->Cancel();
    if (generation.state == FullTextIndexLifecycleState::kWorkRequested)
        generation.state = FullTextIndexLifecycleState::kCancelled;
    return true;
}

std::optional<FullTextIndexLifecycleSnapshot>
FullTextIndexLifecycleCoordinator::Snapshot(
    const std::string& dictionary_id) const {
    std::lock_guard lock(implementation_->mutex);
    const auto found = implementation_->entries.find(dictionary_id);
    if (found == implementation_->entries.end())
        return std::nullopt;
    const auto& generation = *found->second.current;
    return FullTextIndexLifecycleSnapshot(generation.identity, generation.state,
                                          generation.format_capable,
                                          generation.source_revision);
}

}  // namespace goldendict::core::dictionary
