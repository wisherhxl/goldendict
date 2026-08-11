// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_DICTIONARY_BACKEND_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_DICTIONARY_BACKEND_H_

#include <chrono>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace goldendict::core::dictionary {

enum class ErrorCode {
    kUnavailable,
    kInvalidData,
    kCancelled,
    kDeadlineExceeded,
    kUnsupported,
};

class Error final : public std::runtime_error {
   public:
    Error(ErrorCode code, std::string message);

    ErrorCode code() const noexcept { return code_; }

   private:
    ErrorCode code_;
};

class CancellationSignal {
   public:
    virtual ~CancellationSignal() = default;

    virtual bool IsCancellationRequested() const noexcept = 0;
};

struct RequestOptions {
    std::size_t result_limit = 20;
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max();
    const CancellationSignal* cancellation = nullptr;
};

struct Identity {
    std::string id;
    std::string name;
    std::string source;
    std::string source_language;
    std::string target_language;
};

struct Article {
    std::string headword;
    std::string format;
    std::string data;
};

struct Resource {
    std::string id;
    std::string media_type;
    std::vector<std::byte> data;
};

class Backend {
   public:
    virtual ~Backend() = default;

    virtual const Identity& identity() const noexcept = 0;
    virtual std::vector<Article> LookupExact(
        std::string_view headword,
        const RequestOptions& options = RequestOptions{}) const = 0;
    virtual std::optional<Resource> GetResource(
        std::string_view resource_id,
        const RequestOptions& options = RequestOptions{}) const = 0;
};

void CheckRequest(const RequestOptions& options);
std::string MediaTypeForResourceId(std::string_view resource_id);

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_DICTIONARY_BACKEND_H_
