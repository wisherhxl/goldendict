// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "main_window.h"
#include "full_text_query_composer.h"

// Only the real scope operation, group selection and request-count observation
// cross this source-private boundary. Scenario state belongs to the test targets.
class DictionaryScopeTestAccess {
   public:
    static goldendict::core::FullTextQuery Compose(
        const MainWindow& window,
        const goldendict::app::FullTextQueryComposer& composer) {
        return window.ComposeFullTextQuery(composer);
    }

    static void SelectGroup(MainWindow& window, std::uint32_t id,
                            bool refresh = false) {
        window.SelectGroup(id);
        if (refresh)
            window.RefreshDictionaryBar();
    }

    static std::size_t RequestCount(const MainWindow& window) {
        return window.requests_.size();
    }
};
