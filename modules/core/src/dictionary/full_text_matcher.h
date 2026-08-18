// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_MATCHER_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_MATCHER_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "goldendict/core/dictionary_service.h"

namespace goldendict::core::dictionary {

enum class FullTextMatcherErrorCode {
    kMalformedPattern,
    kCancelled,
    kDeadlineExceeded,
};

class FullTextMatcherError final : public std::runtime_error {
   public:
    FullTextMatcherError(FullTextMatcherErrorCode code, std::string message);

    FullTextMatcherErrorCode code() const noexcept { return code_; }

   private:
    FullTextMatcherErrorCode code_;
};

struct FullTextMatcherOptions {
    std::string_view query_text;
    FullTextQueryMode mode = FullTextQueryMode::kWholeWords;
    bool match_case = false;
    bool ignore_diacritics = false;
    bool ignore_word_order = false;
    std::optional<std::uint32_t> maximum_word_distance;
};

struct FullTextMatcherRange {
    std::size_t byte_offset = 0U;
    std::size_t byte_length = 0U;
};

std::vector<FullTextMatcherRange> MatchFullText(
    std::string_view text, const FullTextMatcherOptions& options,
    const CancellationToken* cancellation,
    std::chrono::steady_clock::time_point deadline,
    std::optional<std::size_t> match_limit = std::nullopt);

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_MATCHER_H_
