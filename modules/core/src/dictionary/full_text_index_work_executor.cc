// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_index_work_executor.h"

#include <utility>

namespace goldendict::core::dictionary {

FullTextIndexWorkExecutor::FullTextIndexWorkExecutor(
    FullTextIndexLifecycleCoordinator& coordinator)
    : coordinator_(coordinator), worker_([this] { Run(); }) {}

FullTextIndexWorkExecutor::~FullTextIndexWorkExecutor() {
    Shutdown();
}

bool FullTextIndexWorkExecutor::Submit(
    const FullTextIndexExecutionBounds& bounds) {
    std::lock_guard lock(mutex_);
    if (shutting_down_)
        return false;
    if (!pending_bounds_.has_value())
        pending_bounds_.emplace(bounds);
    condition_.notify_one();
    return true;
}

void FullTextIndexWorkExecutor::Shutdown() noexcept {
    std::optional<FullTextIndexWorkIdentity> active_identity;
    {
        std::lock_guard lock(mutex_);
        if (!shutting_down_) {
            shutting_down_ = true;
            pending_bounds_.reset();
        }
        active_identity = active_identity_;
    }
    if (active_identity.has_value())
        coordinator_.Cancel({*active_identity});
    condition_.notify_one();
    if (worker_.joinable())
        worker_.join();
}

void FullTextIndexWorkExecutor::Run() {
    while (true) {
        std::optional<FullTextIndexExecutionBounds> bounds;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this] {
                return shutting_down_ || pending_bounds_.has_value();
            });
            if (shutting_down_)
                return;
            bounds.emplace(*pending_bounds_);
            pending_bounds_.reset();
        }

        const auto identities = coordinator_.DiscoverRequestedWork();
        for (const auto& identity : identities) {
            const auto request =
                coordinator_.ProjectBoundedWorkRequest(identity, *bounds);
            if (!request.has_value())
                continue;
            {
                std::lock_guard lock(mutex_);
                if (shutting_down_)
                    return;
                active_identity_ = identity;
            }
            coordinator_.ExecuteBoundedWork(*request);
            {
                std::lock_guard lock(mutex_);
                active_identity_.reset();
                if (shutting_down_)
                    return;
            }
        }
    }
}

}  // namespace goldendict::core::dictionary
