// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_DICTIONARY_SERVICE_H_
#define GOLDENDICT_CORE_DICTIONARY_SERVICE_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"

namespace goldendict::core {

inline constexpr std::size_t kMaximumLookupTextBytes = 4096U;
inline constexpr std::size_t kMaximumLookupDictionaryFilters = 100U;
inline constexpr std::size_t kMaximumLookupLanguageFilters = 32U;
inline constexpr std::size_t kMaximumLookupFilterBytes = 256U;
inline constexpr std::size_t kMaximumLookupResults = 100U;
inline constexpr std::size_t kMaximumHeadwordPatternBytes = 256U;

enum class HeadwordFilterMode {
    kPrefix,
    kWildcard,
    kRegularExpression,
};

enum class MatchMode {
    kExact,
    kPrefix,
    kFuzzy,
    kFullText,
};

enum class LookupErrorCode {
    kInvalidQuery,
    kDictionaryUnavailable,
    kCancelled,
    kDeadlineExceeded,
    kInternal,
};

struct DictionaryIdentity {
    std::string id;
    std::string name;
    std::string edition;
    std::string source;
    std::string description;
    std::size_t article_count = 0;
    std::size_t headword_count = 0;
};

struct LanguageInfo {
    std::string source_language;
    std::string target_language;
};

struct LookupQuery {
    std::string text;
    std::vector<std::string> dictionary_ids;
    std::vector<std::string> languages;
    MatchMode match_mode = MatchMode::kExact;
    std::size_t result_limit = 20;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
    std::uint32_t group_id = 0U;
};

struct SuggestionQuery {
    std::string text;
    std::vector<std::string> dictionary_ids;
    std::vector<std::string> languages;
    std::size_t result_limit = 20;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
    std::uint32_t group_id = 0U;
    HeadwordFilterMode filter_mode = HeadwordFilterMode::kPrefix;
    bool match_case = false;
};

struct MatchInfo {
    std::string requested_headword;
    std::string normalized_headword;
    MatchMode mode = MatchMode::kExact;
    double score = 1.0;
};

struct ArticleContent {
    std::string plain_text;
    std::optional<std::string> sanitized_html;
};

struct ResourceReference {
    std::string dictionary_id;
    std::string resource_id;
    std::string media_type;
};

struct DictionaryEntry {
    DictionaryIdentity dictionary;
    LanguageInfo language;
    MatchInfo match;
    ArticleContent article;
    std::vector<ResourceReference> resources;
};

struct LookupError {
    LookupErrorCode code = LookupErrorCode::kInternal;
    std::string dictionary_id;
    std::string message;
};

struct LookupResponse {
    std::vector<DictionaryEntry> entries;
    std::vector<LookupError> errors;
    bool partial = false;
};

struct HeadwordSuggestion {
    DictionaryIdentity dictionary;
    LanguageInfo language;
    MatchInfo match;
    std::string headword;
};

struct SuggestionResponse {
    std::vector<HeadwordSuggestion> suggestions;
    std::vector<LookupError> errors;
    bool partial = false;
};

class GOLDENDICT_EXPORTS CancellationToken {
   public:
    virtual ~CancellationToken();

    virtual bool IsCancellationRequested() const noexcept = 0;
};

class GOLDENDICT_EXPORTS LookupRequest {
   public:
    virtual ~LookupRequest();

    virtual void Cancel() noexcept = 0;
    virtual bool IsFinished() const noexcept = 0;
    virtual LookupResponse Await() = 0;
};

class GOLDENDICT_EXPORTS DictionaryService {
   public:
    virtual ~DictionaryService();

    virtual std::vector<DictionaryIdentity> GetCatalog() const = 0;
    virtual LookupResponse Lookup(
        const LookupQuery& query,
        const CancellationToken* cancellation = nullptr) const = 0;
    virtual SuggestionResponse Suggest(
        const SuggestionQuery& query,
        const CancellationToken* cancellation = nullptr) const = 0;
    virtual std::unique_ptr<LookupRequest> StartLookup(
        LookupQuery query) const = 0;
    virtual std::vector<std::byte> GetResource(
        const ResourceReference& resource,
        const CancellationToken* cancellation = nullptr) const = 0;
};

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_DICTIONARY_SERVICE_H_
