// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_H_

#include <filesystem>
#include <string>
#include <vector>

#include "generated_index.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core::dictionary {

inline constexpr std::size_t kMaximumFullTextDocuments = 100000U;
inline constexpr std::size_t kMaximumFullTextDocumentBytes =
    16U * 1024U * 1024U;
inline constexpr std::size_t kMaximumFullTextCorpusBytes = 256U * 1024U * 1024U;

struct FullTextDocument {
    DictionaryIdentity dictionary;
    std::string headword;
    std::string document_id;
    std::string plain_text;
};

enum class FullTextIndexState {
    kCreated,
    kReused,
    kRebuiltStale,
    kRebuiltCorrupt,
};

class FullTextIndexError final : public std::runtime_error {
   public:
    FullTextIndexError(FullTextErrorCode code, std::string message);

    FullTextErrorCode code() const noexcept { return code_; }

   private:
    FullTextErrorCode code_;
};

class FullTextIndex final {
   public:
    static FullTextIndex OpenOrBuild(
        const std::filesystem::path& path, const SourceSnapshot& sources,
        std::vector<FullTextDocument> documents,
        const CancellationToken* cancellation = nullptr,
        std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::time_point::max());

    FullTextResponse Search(
        const FullTextQuery& query,
        const CancellationToken* cancellation = nullptr) const;

    FullTextIndexState state() const noexcept { return state_; }

   private:
    std::vector<FullTextDocument> documents_;
    FullTextIndexState state_ = FullTextIndexState::kCreated;
};

}  // namespace goldendict::core::dictionary

#endif
