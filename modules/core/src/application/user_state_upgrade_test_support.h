// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <system_error>

#include "goldendict/base/goldendict_def.tp.h"
#include "goldendict/core/user_state_upgrade.h"

namespace goldendict::core::application {

enum class UserStateUpgradeOperation : std::uint8_t {
    kWritePendingMarker,
    kPublishConfiguration,
    kPublishHistory,
    kPublishFavorites,
    kRemovePendingMarker,
};

struct UserStateUpgradeDependencies {
    std::function<std::optional<std::error_code>(UserStateUpgradeOperation,
                                                 const std::filesystem::path&)>
        filesystem_failure;
};

GOLDENDICT_EXPORTS UserStateSnapshot LoadOrMigrateUserStateForTesting(
    const UserStatePaths& paths,
    const UserStateUpgradeDependencies& dependencies);

}  // namespace goldendict::core::application
