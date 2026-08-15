// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_DICT_SERVER_SOURCE_H_
#define GOLDENDICT_NETWORK_DICT_SERVER_SOURCE_H_

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "http_client.h"

namespace goldendict::network {

enum class DictServerErrorCode {
    kInvalidRequest,
    kInvalidConfiguration,
    kCancelled,
    kDeadlineExceeded,
    kResponseTooLarge,
    kProxyAuthenticationRequired,
    kProxyTransport,
    kTransport,
    kInvalidResponse,
};

class DictServerError final : public std::runtime_error {
   public:
    DictServerError(DictServerErrorCode code, std::string message);

    DictServerErrorCode code() const noexcept { return code_; }

   private:
    DictServerErrorCode code_;
};

struct DictServerOptions {
    std::string host;
    unsigned short port = 2628;
    std::string database = "*";
    std::string strategy = "prefix";
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
    std::size_t maximum_response_bytes = 4U * 1024U * 1024U;
    std::optional<HttpRequest::Proxy> proxy;
};

struct DictServerArticle {
    std::string headword;
    std::string database;
    std::string database_name;
    std::string content_type;
    // Untrusted remote content; callers must pass it through article assembly.
    std::string body;
};

class DictServerSource final {
   public:
    explicit DictServerSource(DictServerOptions options);

    std::vector<std::string> Suggest(
        std::string_view prefix, std::size_t maximum_results,
        const std::function<bool()>& is_cancellation_requested = {}) const;
    std::vector<DictServerArticle> Define(
        std::string_view word, std::size_t maximum_articles,
        const std::function<bool()>& is_cancellation_requested = {}) const;

   private:
    DictServerOptions options_;
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_DICT_SERVER_SOURCE_H_
