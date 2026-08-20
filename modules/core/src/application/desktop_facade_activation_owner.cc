// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop_facade_activation_owner.h"

#include "core_facade_activation_test_access.h"

#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <utility>

namespace goldendict::core::application {

class DesktopFacadeActivationOwner::Impl final {
   public:
    enum class State { kOpen, kHandoffReserved, kShutdown };

    struct PublishedComposition {
        std::shared_ptr<DesktopFacade> facade;
        std::optional<ServiceStateActivationHandle> activation;
    };

    mutable std::mutex mutex;
    State state = State::kOpen;
    std::uint64_t generation = 1U;
    std::optional<PublishedComposition> current;
};

class PreparedCoreFacadeCandidate::Impl final {
   public:
    std::weak_ptr<DesktopFacadeActivationOwner::Impl> owner;
    std::uint64_t generation = 0U;
    std::shared_ptr<DesktopFacade> facade;
    ServiceStateActivationHandle activation;
    bool ready = false;
    bool consumed = false;
    CoreFacadeActivationTestAccess::Observer observer = nullptr;
    void* observer_context = nullptr;
};

PreparedCoreFacadeCandidate::PreparedCoreFacadeCandidate() noexcept = default;

PreparedCoreFacadeCandidate::PreparedCoreFacadeCandidate(
    std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

PreparedCoreFacadeCandidate::~PreparedCoreFacadeCandidate() = default;

PreparedCoreFacadeCandidate::PreparedCoreFacadeCandidate(
    PreparedCoreFacadeCandidate&&) noexcept = default;

PreparedCoreFacadeCandidate& PreparedCoreFacadeCandidate::operator=(
    PreparedCoreFacadeCandidate&&) noexcept = default;

PreparedCoreFacadeCandidate::operator bool() const noexcept {
    return impl_ != nullptr && impl_->ready && !impl_->consumed;
}

void PreparedCoreFacadeCandidate::Abandon() noexcept {
    impl_.reset();
}

DesktopFacadeActivationOwner::DesktopFacadeActivationOwner()
    : impl_(std::make_shared<Impl>()) {}

DesktopFacadeActivationOwner::~DesktopFacadeActivationOwner() {
    static_cast<void>(Shutdown());
}

PreparedCoreFacadeCandidate DesktopFacadeActivationOwner::PrepareCandidate(
    const CoreConfiguration& configuration) {
    return PrepareCandidate(configuration, {});
}

PreparedCoreFacadeCandidate DesktopFacadeActivationOwner::PrepareCandidate(
    const CoreConfiguration& configuration,
    std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources) {
    std::uint64_t generation = 0U;
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->state != Impl::State::kOpen)
            return {};
        generation = impl_->generation;
    }

    auto composition = CreateDesktopFacadeActivationCandidate(
        configuration, std::move(runtime_sources));
    auto candidate = std::make_unique<PreparedCoreFacadeCandidate::Impl>();
    candidate->owner = impl_;
    candidate->generation = generation;
    candidate->facade = std::move(composition.facade);
    candidate->activation = std::move(composition.activation);

    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->state != Impl::State::kOpen ||
            impl_->generation != generation) {
            return {};
        }
        candidate->ready = true;
    }
    return PreparedCoreFacadeCandidate(std::move(candidate));
}

bool DesktopFacadeActivationOwner::Activate(
    PreparedCoreFacadeCandidate& candidate) noexcept {
    if (!candidate.impl_)
        return false;
    const auto candidate_owner = candidate.impl_->owner.lock();
    if (candidate_owner != impl_)
        return false;

    std::optional<Impl::PublishedComposition> old;
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->state != Impl::State::kOpen || !candidate.impl_->ready ||
            candidate.impl_->consumed ||
            candidate.impl_->generation != impl_->generation) {
            return false;
        }

        impl_->state = Impl::State::kHandoffReserved;
        candidate.impl_->consumed = true;
        candidate.impl_->ready = false;
        if (impl_->current.has_value()) {
            old.emplace(std::move(*impl_->current));
            impl_->current.reset();
        }
        // Irreversible publication is deliberately limited to nothrow moves
        // into already-owned storage and the generation update below.
        impl_->current.emplace(Impl::PublishedComposition{
            std::move(candidate.impl_->facade),
            std::optional<ServiceStateActivationHandle>(
                std::move(candidate.impl_->activation))});
        ++impl_->generation;
        if (impl_->generation == 0U)
            impl_->generation = 1U;
    }

    if (candidate.impl_->observer != nullptr) {
        candidate.impl_->observer(candidate.impl_->observer_context,
                                  CoreFacadeActivationEvent::kPublished);
    }

    if (old.has_value() && old->activation.has_value()) {
        old->activation->ShutdownAndJoin();
        if (candidate.impl_->observer != nullptr) {
            candidate.impl_->observer(
                candidate.impl_->observer_context,
                CoreFacadeActivationEvent::kOldExecutorStopped);
        }
    }

    const bool submitted = impl_->current.has_value() &&
                           impl_->current->activation.has_value() &&
                           impl_->current->activation->SubmitOnceWithDefaults();
    if (!submitted)
        std::terminate();
    if (candidate.impl_->observer != nullptr) {
        candidate.impl_->observer(
            candidate.impl_->observer_context,
            CoreFacadeActivationEvent::kNewExecutorSubmitted);
    }
    {
        std::lock_guard lock(impl_->mutex);
        impl_->state = Impl::State::kOpen;
    }
    return true;
}

bool DesktopFacadeActivationOwner::Shutdown() noexcept {
    std::optional<Impl::PublishedComposition> current;
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->state == Impl::State::kShutdown)
            return true;
        if (impl_->state == Impl::State::kHandoffReserved)
            return false;
        impl_->state = Impl::State::kShutdown;
        ++impl_->generation;
        if (impl_->current.has_value()) {
            current.emplace(std::move(*impl_->current));
            impl_->current.reset();
        }
    }
    if (current.has_value() && current->activation.has_value())
        current->activation->ShutdownAndJoin();
    return true;
}

std::shared_ptr<DesktopFacade> DesktopFacadeActivationOwner::CurrentSnapshot()
    const {
    std::lock_guard lock(impl_->mutex);
    return impl_->current.has_value() ? impl_->current->facade : nullptr;
}

void CoreFacadeActivationTestAccess::Observe(
    PreparedCoreFacadeCandidate& candidate, Observer observer,
    void* context) noexcept {
    if (!candidate.impl_)
        return;
    candidate.impl_->observer = observer;
    candidate.impl_->observer_context = context;
}

std::shared_ptr<DesktopFacade> CoreFacadeActivationTestAccess::Facade(
    const PreparedCoreFacadeCandidate& candidate) noexcept {
    return candidate.impl_ ? candidate.impl_->facade : nullptr;
}

}  // namespace goldendict::core::application
