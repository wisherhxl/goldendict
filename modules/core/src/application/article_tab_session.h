// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_APPLICATION_ARTICLE_TAB_SESSION_H_
#define GOLDENDICT_CORE_APPLICATION_ARTICLE_TAB_SESSION_H_

#include "goldendict/core/desktop_facade.h"

namespace goldendict::core::application {

bool IsValidExactArticleTarget(const ExactArticleTarget& target);
bool IsValidTabNavigation(const TabNavigationState& state);
bool ValidateArticleTabSession(const ArticleTabSession& session,
                               ArticleTabId* next_tab_id = nullptr);

}  // namespace goldendict::core::application

#endif  // GOLDENDICT_CORE_APPLICATION_ARTICLE_TAB_SESSION_H_
