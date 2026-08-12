// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_FORVO_SOURCE_H_
#define GOLDENDICT_NETWORK_FORVO_SOURCE_H_

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "http_client.h"

namespace goldendict::network {

struct ForvoPronunciation {
    std::string audio_url;
    std::string username;
    std::string country;
    std::string sex;
    int positive_votes = 0;
    int negative_votes = 0;
};

struct ForvoAudio {
    std::string content_type;
    std::vector<unsigned char> bytes;
};

class ForvoSource final {
   public:
    ForvoSource(std::string api_base_url, std::string api_key,
                std::string language_code, HttpRequest transport = {});

    std::vector<ForvoPronunciation> Lookup(
        std::string_view word, std::size_t maximum_results,
        const std::function<bool()>& is_cancellation_requested = {}) const;
    ForvoAudio FetchAudio(
        std::string_view audio_url,
        const std::function<bool()>& is_cancellation_requested = {}) const;

   private:
    std::string RequestUrl(std::string_view word) const;
    HttpResponse Fetch(
        const std::string& url,
        const std::function<bool()>& is_cancellation_requested) const;

    std::string api_base_url_;
    std::string api_key_;
    std::string language_code_;
    HttpRequest transport_;
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_FORVO_SOURCE_H_
