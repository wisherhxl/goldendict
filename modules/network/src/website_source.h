// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_WEBSITE_SOURCE_H_
#define GOLDENDICT_NETWORK_WEBSITE_SOURCE_H_

#include <functional>
#include <string>
#include <string_view>

#include "http_client.h"

namespace goldendict::network {

struct WebsitePage {
    std::string final_url;
    // Untrusted remote markup; callers must pass it through article assembly.
    std::string html;
};

class WebsiteSource final {
   public:
    explicit WebsiteSource(std::string url_template,
                           HttpRequest transport = {});

    WebsitePage Fetch(
        std::string_view word,
        const std::function<bool()>& is_cancellation_requested = {}) const;

   private:
    std::string RequestUrl(std::string_view word) const;

    std::string url_template_;
    HttpRequest transport_;
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_WEBSITE_SOURCE_H_
