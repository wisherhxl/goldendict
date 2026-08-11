// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_backend.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

namespace goldendict::core::dictionary {

Error::Error(ErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

void CheckRequest(const RequestOptions& options) {
    if (options.cancellation != nullptr &&
        options.cancellation->IsCancellationRequested()) {
        throw Error(ErrorCode::kCancelled, "Dictionary request was cancelled");
    }
    if (std::chrono::steady_clock::now() >= options.deadline) {
        throw Error(ErrorCode::kDeadlineExceeded,
                    "Dictionary request deadline was exceeded");
    }
}

std::string MediaTypeForResourceId(std::string_view resource_id) {
    std::string extension =
        std::filesystem::u8path(std::string(resource_id)).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    if (extension == ".css") {
        return "text/css";
    }
    if (extension == ".gif") {
        return "image/gif";
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return "image/jpeg";
    }
    if (extension == ".png") {
        return "image/png";
    }
    if (extension == ".svg") {
        return "image/svg+xml";
    }
    if (extension == ".wav") {
        return "audio/wav";
    }
    return "application/octet-stream";
}

}  // namespace goldendict::core::dictionary
