// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_NETWORK_HTTP_CLIENT_H_
#define GOLDENDICT_CORE_NETWORK_HTTP_CLIENT_H_

#include <chrono>
#include <cstddef>
#include <functional>
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
};

class HttpError final : public std::runtime_error {
   public:
    HttpError(HttpErrorCode code, std::string message);

    HttpErrorCode code() const noexcept { return code_; }

   private:
    HttpErrorCode code_;
};

struct HttpRequest {
    std::string url;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
    std::size_t maximum_response_bytes = 4U * 1024U * 1024U;
    std::size_t maximum_redirects = 5U;
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
