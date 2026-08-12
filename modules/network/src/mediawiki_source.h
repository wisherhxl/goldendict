// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_MEDIAWIKI_SOURCE_H_
#define GOLDENDICT_NETWORK_MEDIAWIKI_SOURCE_H_

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "http_client.h"

namespace goldendict::network {

struct MediaWikiArticle {
    std::string title;
    // Untrusted remote markup; callers must pass it through article assembly.
    std::string html;
};

class MediaWikiSource final {
   public:
    explicit MediaWikiSource(std::string base_url, HttpRequest transport = {});

    std::vector<std::string> Suggest(
        std::string_view prefix, std::size_t maximum_results,
        const std::function<bool()>& is_cancellation_requested = {}) const;
    MediaWikiArticle FetchArticle(
        std::string_view title,
        const std::function<bool()>& is_cancellation_requested = {}) const;

   private:
    std::string ApiUrl(
        const std::vector<std::pair<std::string, std::string>>& query) const;
    HttpResponse Fetch(
        const std::string& url,
        const std::function<bool()>& is_cancellation_requested) const;

    std::string base_url_;
    HttpRequest transport_;
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_MEDIAWIKI_SOURCE_H_
