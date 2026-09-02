// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_FAVORITES_STORE_H_
#define GOLDENDICT_CORE_FAVORITES_STORE_H_

#include <cstddef>
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
using FavoritePath = std::vector<std::size_t>;

enum class FavoriteMoveStatus {
    kMoved,
    kNoOp,
    kInvalidSource,
    kInvalidDestination,
    kCycle,
    kDuplicate,
    kInvalidState,
};

struct FavoriteMoveResult {
    FavoriteMoveStatus status = FavoriteMoveStatus::kInvalidSource;
    Favorites favorites;
    FavoritePath moved_path;

    bool changed() const noexcept {
        return status == FavoriteMoveStatus::kMoved;
    }
};

GOLDENDICT_EXPORTS Favorites LoadFavorites(const std::string& favorites_path);
GOLDENDICT_EXPORTS void SaveFavorites(const std::string& favorites_path,
                                      const Favorites& favorites);

// Moves one headword or complete folder subtree to a folder (or the root for
// an empty destination path). The insertion index is a boundary in the
// destination before source removal. A changed candidate is atomically saved
// before it is returned; rejected and no-op moves never write.
GOLDENDICT_EXPORTS FavoriteMoveResult MoveFavorite(
    const std::string& favorites_path, const Favorites& favorites,
    const FavoritePath& source_path, const FavoritePath& destination_path,
    std::size_t destination_index,
    const std::vector<FavoritePath>& expanded_paths = {},
    bool replace_expansion_state = false);

// Imports/exports the bounded legacy-compatible UTF-8 XML tree. Export uses
// atomic replacement and import validates the complete document before
// returning any state.
GOLDENDICT_EXPORTS Favorites ImportFavoritesXml(const std::string& import_path);
GOLDENDICT_EXPORTS void ExportFavoritesXml(const std::string& export_path,
                                           const Favorites& favorites);
GOLDENDICT_EXPORTS void ExportFavoritesText(const std::string& export_path,
                                            const Favorites& favorites);

// Loads current favorites when present. Otherwise imports the bounded legacy
// XML tree, atomically saves the current format, and leaves legacy data
// untouched. Missing files produce an empty tree.
GOLDENDICT_EXPORTS Favorites
LoadOrMigrateFavorites(const std::string& favorites_path,
                       const std::string& legacy_favorites_path);

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_FAVORITES_STORE_H_
