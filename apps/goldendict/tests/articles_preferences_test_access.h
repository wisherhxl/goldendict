// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "main_window.h"

// Preserves the existing synchronous interaction seam; never applies
// preferences.
class ArticlesPreferencesTestAccess final {
   public:
    static void SetDialogExecutor(
        MainWindow& window, std::function<int(PreferencesDialog&)> executor) {
        window.preferences_dialog_executor_ = std::move(executor);
    }
};
