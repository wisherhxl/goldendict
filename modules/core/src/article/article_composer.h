// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_ARTICLE_COMPOSER_H_
#define GOLDENDICT_CORE_ARTICLE_COMPOSER_H_

#include "goldendict/core/dictionary_service.h"

namespace goldendict::core::article {

struct ArticleCompositionOptions {
    bool always_expand_optional_parts = false;
    bool collapse_large_articles = false;
    std::uint32_t article_size_limit = 2000U;
};

ArticleContent ComposeLookupPage(
    const LookupResponse& response,
    const ArticleCompositionOptions& options = ArticleCompositionOptions{});

}  // namespace goldendict::core::article

#endif  // GOLDENDICT_CORE_ARTICLE_COMPOSER_H_
