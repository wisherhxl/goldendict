// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_EXTERNAL_EXTERNAL_PROGRAM_SOURCE_H_
#define GOLDENDICT_EXTERNAL_EXTERNAL_PROGRAM_SOURCE_H_

#include <chrono>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace goldendict::external {

enum class ExternalProgramErrorCode {
    kInvalidConfiguration,
    kInvalidRequest,
    kCancelled,
    kDeadlineExceeded,
    kOutputTooLarge,
    kFailedToStart,
    kCrashed,
    kNonZeroExit,
    kInvalidOutput,
};

class ExternalProgramError final : public std::runtime_error {
   public:
    ExternalProgramError(ExternalProgramErrorCode code, std::string message);

    ExternalProgramErrorCode code() const noexcept { return code_; }

   private:
    ExternalProgramErrorCode code_;
};

struct ExternalProgramOptions {
    std::string executable;
    std::vector<std::string> arguments;
    std::string working_directory;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
    std::size_t maximum_output_bytes = 4U * 1024U * 1024U;
};

struct ExternalProgramResult {
    std::string standard_output;
    std::string standard_error;
};

class ExternalProgramSource final {
   public:
    explicit ExternalProgramSource(ExternalProgramOptions options);

    ExternalProgramResult Run(
        std::string_view word,
        const std::function<bool()>& is_cancellation_requested = {}) const;

   private:
    ExternalProgramOptions options_;
};

}  // namespace goldendict::external

#endif  // GOLDENDICT_EXTERNAL_EXTERNAL_PROGRAM_SOURCE_H_
