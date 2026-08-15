// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_DICTIONARY_BACKEND_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_DICTIONARY_BACKEND_H_

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

void CheckRequest(const RequestOptions& options);
std::string MediaTypeForResourceId(std::string_view resource_id);

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_DICTIONARY_BACKEND_H_
