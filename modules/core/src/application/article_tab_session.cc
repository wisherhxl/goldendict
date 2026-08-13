// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_tab_session.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

#include "../foundation/utf8.h"

namespace goldendict::core::application {
namespace {

bool IsBoundedText(const std::string& text) {
    return text.size() <= kMaximumLookupTextBytes &&
           text.find('\0') == std::string::npos &&
           foundation::IsValidUtf8(text);
}

}  // namespace

bool IsValidTabNavigation(const TabNavigationState& state) {
    if (!IsBoundedText(state.query) || !IsBoundedText(state.title) ||
        !IsBoundedText(state.internal_url) ||
        !IsBoundedText(state.source_dictionary_id) ||
        !IsBoundedText(state.source_article_id) ||
        !IsBoundedText(state.target_article_id) ||
        !IsBoundedText(state.target_anchor)) {
        return false;
    }
    switch (state.kind) {
        case TabNavigationKind::kEmpty:
            return state.group_id == 0U && state.query.empty() &&
                   state.internal_url.empty() &&
                   state.source_dictionary_id.empty() &&
                   state.source_article_id.empty() &&
                   state.target_article_id.empty() &&
                   state.target_anchor.empty();
        case TabNavigationKind::kLookup:
            return !state.query.empty() && state.internal_url.empty() &&
                   state.source_dictionary_id.empty() &&
                   state.source_article_id.empty() &&
                   state.target_article_id.empty() &&
                   state.target_anchor.empty();
        case TabNavigationKind::kInternalLink:
            return !state.query.empty() && !state.internal_url.empty();
    }
    return false;
}

bool ValidateArticleTabSession(const ArticleTabSession& session,
                               ArticleTabId* next_tab_id) {
    if (session.tabs.empty() || session.tabs.size() > kMaximumArticleTabs) {
        return false;
    }
    std::unordered_set<ArticleTabId> ids;
    ArticleTabId maximum_id = 0U;
    for (const auto& tab : session.tabs) {
        if (tab.id == 0U || !ids.insert(tab.id).second || tab.history.empty() ||
            tab.history.size() > kMaximumTabNavigationEntries ||
            tab.history_cursor >= tab.history.size() ||
            !std::all_of(tab.history.begin(), tab.history.end(),
                         IsValidTabNavigation)) {
            return false;
        }
        maximum_id = std::max(maximum_id, tab.id);
    }
    if (ids.find(session.active_tab_id) == ids.end() ||
        maximum_id == std::numeric_limits<ArticleTabId>::max()) {
        return false;
    }
    if (next_tab_id != nullptr) {
        *next_tab_id = maximum_id + 1U;
    }
    return true;
}

}  // namespace goldendict::core::application
