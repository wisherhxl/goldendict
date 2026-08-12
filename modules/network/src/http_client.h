// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_NETWORK_HTTP_CLIENT_H_
#define GOLDENDICT_CORE_NETWORK_HTTP_CLIENT_H_

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace goldendict::network {

enum class HttpErrorCode {
    kInvalidRequest,
    kInvalidUrl,
    kCancelled,
    kDeadlineExceeded,
    kResponseTooLarge,
    kRedirectRejected,
    kTooManyRedirects,
    kHttpStatus,
    kTransport,
    kInvalidResponse,
};

class HttpError final : public std::runtime_error {
   public:
    HttpError(HttpErrorCode code, std::string message);

    HttpErrorCode code() const noexcept { return code_; }

   private:
    HttpErrorCode code_;
};

struct HttpRequest {
    struct Credentials {
        std::string username;
        std::string password;
    };

    struct Proxy {
        std::string host;
        unsigned short port = 0;
        std::optional<Credentials> credentials;
    };

    std::string url;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
    std::size_t maximum_response_bytes = 4U * 1024U * 1024U;
    std::size_t maximum_redirects = 5U;
    std::optional<Credentials> credentials;
    std::optional<Proxy> proxy;
};

struct HttpResponse {
    int status_code = 0;
    std::string final_url;
    std::string content_type;
    std::vector<unsigned char> body;
};

HttpResponse FetchHttp(
    const HttpRequest& request,
    const std::function<bool()>& is_cancellation_requested = {});

}  // namespace goldendict::network

#endif  // GOLDENDICT_CORE_NETWORK_HTTP_CLIENT_H_
