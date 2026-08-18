// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_DICTIONARY_BACKEND_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_DICTIONARY_BACKEND_H_

#include "full_text_index.h"
#include "goldendict/core/runtime_dictionary_source.h"

namespace goldendict::core::dictionary {

using ErrorCode = RuntimeSourceErrorCode;
using Error = RuntimeSourceError;
using CancellationSignal = RuntimeCancellationSignal;
using RequestOptions = RuntimeRequestOptions;
using Identity = RuntimeDictionaryIdentity;
using Article = RuntimeDictionaryArticle;
using Resource = RuntimeDictionaryResource;
using HeadwordPage = RuntimeHeadwordPage;
using Backend = RuntimeDictionarySource;

// Private capability implemented only by the legacy synonym-list formats.
// Runtime sources deliberately do not inherit this interface.
class SynonymBackend {
   public:
    virtual ~SynonymBackend() = default;
    virtual std::vector<std::string> FindHeadwordsForSynonym(
        std::string_view headword, const RequestOptions& options) const = 0;
};

// Private capability implemented only by formats with accepted full-text
// ingestion. Runtime sources deliberately do not inherit this interface.
class FullTextBackend {
   public:
    virtual ~FullTextBackend() = default;
    virtual FullTextResponse SearchFullText(
        const FullTextQuery& query,
        const CancellationToken* cancellation = nullptr) const = 0;
    virtual std::optional<ResolvedFullTextDocument> ResolveFullTextDocument(
        std::string_view document_id) const = 0;
    virtual bool IsFullTextIndexAvailable() const noexcept = 0;
};

void CheckRequest(const RequestOptions& options);
std::string MediaTypeForResourceId(std::string_view resource_id);

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_DICTIONARY_BACKEND_H_
