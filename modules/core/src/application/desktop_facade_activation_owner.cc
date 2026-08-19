// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop_facade_activation_owner.h"

#include <utility>

namespace goldendict::core::application {

DesktopFacadeActivationOwner::~DesktopFacadeActivationOwner() {
    // As with every C++ destructor, callers must ensure that no operation is
    // concurrently using this object once destruction begins.
    static_cast<void>(Shutdown());
}

bool DesktopFacadeActivationOwner::BuildAndInstallCandidate(
    const CoreConfiguration& configuration) {
    return InstallCandidate(
        CreateDesktopFacadeActivationCandidate(configuration));
}

bool DesktopFacadeActivationOwner::BuildAndInstallCandidate(
    const CoreConfiguration& configuration,
    std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources) {
    return InstallCandidate(CreateDesktopFacadeActivationCandidate(
        configuration, std::move(runtime_sources)));
}

bool DesktopFacadeActivationOwner::InstallCandidate(
    DesktopFacadeActivationCandidate candidate) {
    std::lock_guard lock(mutex_);
    if (state_ != State::kOpen || candidate_.has_value())
        return false;
    candidate_.emplace(std::move(candidate));
    return true;
}

bool DesktopFacadeActivationOwner::Activate() {
    DesktopFacadeActivationCandidate candidate;
    std::optional<ServiceStateActivationHandle> old_activation;
    {
        std::lock_guard lock(mutex_);
        if (state_ != State::kOpen || !candidate_.has_value())
            return false;
        state_ = State::kHandoffReserved;
        candidate = std::move(*candidate_);
        candidate_.reset();
        if (current_.has_value() && current_->activation.has_value()) {
            old_activation.emplace(std::move(*current_->activation));
            current_->activation.reset();
        }
    }

    if (old_activation.has_value())
        old_activation->ShutdownAndJoin();
    const bool submitted = candidate.activation.SubmitOnceWithDefaults();

    {
        std::lock_guard lock(mutex_);
        if (submitted) {
            current_.emplace(PublishedComposition{
                std::move(candidate.facade),
                std::optional<ServiceStateActivationHandle>(
                    std::move(candidate.activation))});
        }
        state_ = State::kOpen;
    }
    return submitted;
}

bool DesktopFacadeActivationOwner::Shutdown() noexcept {
    std::optional<ServiceStateActivationHandle> current_activation;
    std::optional<DesktopFacadeActivationCandidate> candidate;
    {
        std::lock_guard lock(mutex_);
        if (state_ == State::kShutdown)
            return true;
        if (state_ == State::kHandoffReserved)
            return false;
        state_ = State::kShutdown;
        if (current_.has_value() && current_->activation.has_value()) {
            current_activation.emplace(std::move(*current_->activation));
        }
        current_.reset();
        if (candidate_.has_value())
            candidate.emplace(std::move(*candidate_));
        candidate_.reset();
    }
    if (current_activation.has_value())
        current_activation->ShutdownAndJoin();
    candidate.reset();
    return true;
}

std::shared_ptr<DesktopFacade> DesktopFacadeActivationOwner::CurrentSnapshot()
    const {
    std::lock_guard lock(mutex_);
    return current_.has_value() ? current_->facade : nullptr;
}

}  // namespace goldendict::core::application
