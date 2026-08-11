// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_backend.h"

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

}  // namespace goldendict::core::dictionary
