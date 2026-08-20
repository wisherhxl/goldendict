// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_APPLICATION_CORE_FACADE_ACTIVATION_TEST_ACCESS_H_
#define GOLDENDICT_CORE_SRC_APPLICATION_CORE_FACADE_ACTIVATION_TEST_ACCESS_H_

#include "desktop_facade_activation_owner.h"

namespace goldendict::core::application {

enum class CoreFacadeActivationEvent {
    kPublished,
    kOldExecutorStopped,
    kNewExecutorSubmitted,
};

class GOLDENDICT_EXPORTS CoreFacadeActivationTestAccess final {
   public:
    using Observer = void (*)(void*, CoreFacadeActivationEvent) noexcept;

    static void Observe(PreparedCoreFacadeCandidate& candidate,
                        Observer observer, void* context) noexcept;
    static std::shared_ptr<DesktopFacade> Facade(
        const PreparedCoreFacadeCandidate& candidate) noexcept;
};

}  // namespace goldendict::core::application

#endif  // GOLDENDICT_CORE_SRC_APPLICATION_CORE_FACADE_ACTIVATION_TEST_ACCESS_H_
