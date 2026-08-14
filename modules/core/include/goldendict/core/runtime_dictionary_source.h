// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_RUNTIME_DICTIONARY_SOURCE_H_
#define GOLDENDICT_CORE_RUNTIME_DICTIONARY_SOURCE_H_

#include <chrono>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"

namespace goldendict::core {

enum class RuntimeSourceErrorCode {
    kUnavailable,
    kInvalidData,
    kCancelled,
    kDeadlineExceeded,
    kUnsupported,
};

class GOLDENDICT_EXPORTS RuntimeSourceError final : public std::runtime_error {
   public:
    RuntimeSourceError(RuntimeSourceErrorCode code, std::string message);

    RuntimeSourceErrorCode code() const noexcept { return code_; }

   private:
    RuntimeSourceErrorCode code_;
};

class GOLDENDICT_EXPORTS RuntimeCancellationSignal {
   public:
    virtual ~RuntimeCancellationSignal();
    virtual bool IsCancellationRequested() const noexcept = 0;
};

struct RuntimeRequestOptions {
    // Sources must reject work that begins cancelled or expired and must not
    // return a successful result after either condition becomes true.
    // result_limit is the maximum number of articles or suggestions to return;
    // zero requests no results and must not start source I/O.
    std::size_t result_limit = 20U;
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max();
    const RuntimeCancellationSignal* cancellation = nullptr;
    // Exact lookup sources advertising diacritic-insensitive capability must
    // apply this policy before result_limit. Prefix and suggestion operations
    // ignore this field.
    bool ignore_diacritics = false;
};

struct RuntimeDictionaryIdentity {
    std::string id;
    std::string name;
    std::string source;
    std::string description;
    std::string source_language;
    std::string target_language;
    std::size_t article_count = 0U;
    std::size_t headword_count = 0U;
    bool supports_headword_enumeration = false;
    bool supports_diacritic_insensitive_lookup = false;
};

struct RuntimeHeadwordPage {
    std::vector<std::string> headwords;
    bool complete = false;
};

struct RuntimeDictionaryArticle {
    std::string headword;
    // Raw adapter format (for example "html" or "text"). Core treats data as
    // untrusted and passes it through the normal article assembly policy.
    std::string format;
    std::string data;
};

struct RuntimeDictionaryResource {
    std::string id;
    std::string media_type;
    std::vector<std::byte> data;
};

class GOLDENDICT_EXPORTS RuntimeDictionarySource {
   public:
    virtual ~RuntimeDictionarySource();
    virtual const RuntimeDictionaryIdentity& identity() const noexcept = 0;
    virtual std::vector<RuntimeDictionaryArticle> LookupExact(
        std::string_view headword, const RuntimeRequestOptions& options =
                                       RuntimeRequestOptions{}) const = 0;
    virtual std::vector<RuntimeDictionaryArticle> LookupPrefix(
        std::string_view prefix, const RuntimeRequestOptions& options =
                                     RuntimeRequestOptions{}) const = 0;
    virtual std::vector<std::string> SuggestPrefix(
        std::string_view prefix, const RuntimeRequestOptions& options =
                                     RuntimeRequestOptions{}) const = 0;
    virtual RuntimeHeadwordPage EnumerateHeadwords(
        std::size_t offset,
        const RuntimeRequestOptions& options = RuntimeRequestOptions{}) const;
    virtual std::optional<RuntimeDictionaryResource> GetResource(
        std::string_view resource_id, const RuntimeRequestOptions& options =
                                          RuntimeRequestOptions{}) const = 0;
};

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_RUNTIME_DICTIONARY_SOURCE_H_
