// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_ARTICLE_INTERNAL_URL_H_
#define GOLDENDICT_CORE_SRC_ARTICLE_INTERNAL_URL_H_

#include <optional>
#include <string>
#include <string_view>

namespace goldendict::core::article {

enum class InternalUrlKind {
    kLookup,
    kResource,
};

struct InternalUrl {
    InternalUrlKind kind = InternalUrlKind::kLookup;
    std::string dictionary_id;
    std::string target;
};

std::string MakeLookupUrl(std::string_view headword);
std::string MakeResourceUrl(std::string_view dictionary_id,
                            std::string_view resource_id);
std::optional<InternalUrl> ParseInternalUrl(std::string_view url);

}  // namespace goldendict::core::article

#endif  // GOLDENDICT_CORE_SRC_ARTICLE_INTERNAL_URL_H_
