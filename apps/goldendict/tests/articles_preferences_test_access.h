// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QTabWidget>
#include "main_window.h"

// Preserves the existing synchronous interaction seam; never applies
// preferences.
class ArticlesPreferencesTestAccess final {
   public:
    // Observe the currently published tabs after Preferences replaces widgets.
    // No pointer or mutable state escapes this boundary.
    static bool ArticleTabsVisible(const MainWindow& window) {
        return window.article_tabs_ != nullptr &&
               window.article_tabs_->isVisible();
    }

    static void SetDialogExecutor(
        MainWindow& window, std::function<int(PreferencesDialog&)> executor) {
        window.preferences_dialog_executor_ = std::move(executor);
    }
};
