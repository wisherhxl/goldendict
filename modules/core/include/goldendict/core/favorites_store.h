// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_FAVORITES_STORE_H_
#define GOLDENDICT_CORE_FAVORITES_STORE_H_

#include <string>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"

namespace goldendict::core {

enum class FavoriteItemKind { kFolder, kHeadword };

struct FavoriteItem {
    FavoriteItemKind kind = FavoriteItemKind::kHeadword;
    std::string text;
    bool expanded = false;
    std::vector<FavoriteItem> children;

    bool operator==(const FavoriteItem& other) const noexcept {
        return kind == other.kind && text == other.text &&
               expanded == other.expanded && children == other.children;
    }
};

using Favorites = std::vector<FavoriteItem>;

GOLDENDICT_EXPORTS Favorites LoadFavorites(
    const std::string& favorites_path);
GOLDENDICT_EXPORTS void SaveFavorites(const std::string& favorites_path,
                                      const Favorites& favorites);

// Loads current favorites when present. Otherwise imports the bounded legacy
// XML tree, atomically saves the current format, and leaves legacy data
// untouched. Missing files produce an empty tree.
GOLDENDICT_EXPORTS Favorites LoadOrMigrateFavorites(
    const std::string& favorites_path,
    const std::string& legacy_favorites_path);

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_FAVORITES_STORE_H_
