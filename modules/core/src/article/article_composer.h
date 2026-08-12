// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_ARTICLE_COMPOSER_H_
#define GOLDENDICT_CORE_ARTICLE_COMPOSER_H_

#include "goldendict/core/dictionary_service.h"

namespace goldendict::core::article {

ArticleContent ComposeLookupPage(const LookupResponse& response);

}  // namespace goldendict::core::article

#endif  // GOLDENDICT_CORE_ARTICLE_COMPOSER_H_
