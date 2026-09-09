// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_ARTICLE_RENDERER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_ARTICLE_RENDERER_H_

#include <functional>
#include <string>
#include <string_view>

namespace goldendict::core::formats::dictd {

std::string RenderArticleBody(std::string_view body,
                              std::string_view target_language,
                              const std::function<void()>& checkpoint = {});

}  // namespace goldendict::core::formats::dictd

#endif
