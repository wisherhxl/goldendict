// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_APPLICATION_EXACT_ARTICLE_TARGET_RESOLVER_H_
#define GOLDENDICT_CORE_SRC_APPLICATION_EXACT_ARTICLE_TARGET_RESOLVER_H_

#include "goldendict/core/desktop_facade.h"

namespace goldendict::core::application {

ResolvedExactArticleTarget ResolveExactArticleTarget(
    const DictionaryService& service, const ExactArticleTarget& target);

}  // namespace goldendict::core::application

#endif
