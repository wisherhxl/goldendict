// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "main_window.h"

class ViewMenuTestAccess final {
   public:
    static goldendict::core::ApplicationPreferences Preferences(
        const MainWindow& window) {
        return window.preferences_;
    }
};
