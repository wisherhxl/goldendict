// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_USER_STATE_UPGRADE_H_
#define GOLDENDICT_CORE_USER_STATE_UPGRADE_H_

#include <string>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"
#include "goldendict/core/application.h"
#include "goldendict/core/favorites_store.h"
#include "goldendict/core/history_store.h"

namespace goldendict::core {

struct UserStatePaths {
    std::string configuration_path;
    std::string legacy_configuration_path;
    std::string history_path;
    std::string legacy_history_path;
    std::string favorites_path;
    std::string legacy_favorites_path;
    std::string index_directory;
};

struct UserStateSnapshot {
    CoreConfiguration configuration;
    std::vector<HistoryEntry> history;
    Favorites favorites;
};

// Loads current user state or upgrades every selected legacy companion as one
// recoverable publication. Legacy inputs are never modified. An interrupted
// publication is completed before any state is returned to the caller.
GOLDENDICT_EXPORTS UserStateSnapshot
LoadOrMigrateUserState(const UserStatePaths& paths);

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_USER_STATE_UPGRADE_H_
