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
inline constexpr std::size_t kMaximumHeadwordEnumerationPageSize = 256U;
inline constexpr std::size_t kMaximumHeadwordEnumerationCursorBytes = 256U;
inline constexpr std::size_t kMaximumHeadwordEnumerationResponseBytes =
    1024U * 1024U;
inline constexpr std::size_t kMaximumFullTextQueryBytes = 4096U;
inline constexpr std::size_t kMaximumFullTextResults = 1000000U;
inline constexpr std::size_t kMaximumFullTextMatchesPerResult = 32U;
inline constexpr std::size_t kMaximumFullTextExcerptBytes = 4096U;
inline constexpr std::uint32_t kMaximumFullTextWordDistance = 1000U;

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

enum class FullTextQueryMode {
    kWholeWords,
    kPlainText,
    kWildcard,
    kRegularExpression,
};

enum class FullTextErrorCode {
    kInvalidQuery,
    kDictionaryUnavailable,
    kUnsupported,
    kMalformedIndex,
    kCancelled,
    kDeadlineExceeded,
    kResourceLimit,
    kInternal,
};

enum class LookupErrorCode {
    kInvalidQuery,
    kDictionaryUnavailable,
    kCancelled,
    kDeadlineExceeded,
    kUnsupported,
    kInternal,
};

enum class HeadwordEnumerationErrorCode {
    kInvalidRequest,
    kDictionaryUnavailable,
    kUnsupported,
    kMalformedCursor,
    kStaleCursor,
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
    std::string source_language;
    std::string target_language;
    bool supports_headword_enumeration = false;
    bool supports_full_text_search = false;
};

struct HeadwordEnumerationQuery {
    std::string dictionary_id;
    std::string cursor;
    std::size_t page_size = kMaximumHeadwordEnumerationPageSize;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
};

struct HeadwordEnumerationError {
    HeadwordEnumerationErrorCode code = HeadwordEnumerationErrorCode::kInternal;
    std::string dictionary_id;
    std::string message;
};

struct HeadwordEnumerationPage {
    std::string dictionary_id;
    std::vector<std::string> headwords;
    std::string next_cursor;
    bool complete = false;
    std::optional<HeadwordEnumerationError> error;
};

struct LanguageInfo {
    std::string source_language;
    std::string target_language;
};

struct LookupQuery {
    std::string text;
    std::vector<std::string> dictionary_ids;
    // Makes dictionary_ids authoritative even when the collection is empty.
    bool dictionary_filter_active = false;
    std::vector<std::string> languages;
    MatchMode match_mode = MatchMode::kExact;
    std::size_t result_limit = 20;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
    std::uint32_t group_id = 0U;
};

struct SuggestionQuery {
    std::string text;
    std::vector<std::string> dictionary_ids;
    // Makes dictionary_ids authoritative even when the collection is empty.
    bool dictionary_filter_active = false;
    std::vector<std::string> languages;
    std::size_t result_limit = 20;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
    std::uint32_t group_id = 0U;
    HeadwordFilterMode filter_mode = HeadwordFilterMode::kPrefix;
    bool match_case = false;
};

struct FullTextQuery {
    std::string text;
    std::vector<std::string> dictionary_ids;
    bool dictionary_filter_active = false;
    FullTextQueryMode mode = FullTextQueryMode::kWholeWords;
    bool match_case = false;
    bool ignore_diacritics = false;
    bool ignore_word_order = false;
    std::optional<std::uint32_t> maximum_word_distance;
    std::size_t result_limit = 20U;
    std::optional<std::size_t> maximum_articles_per_dictionary = 100U;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
};

struct FullTextMatch {
    std::size_t byte_offset = 0U;
    std::size_t byte_length = 0U;
    std::string text;
};

struct FullTextError {
    FullTextErrorCode code = FullTextErrorCode::kInternal;
    std::string dictionary_id;
    std::string message;
};

struct MatchInfo {
    std::string requested_headword;
    std::string normalized_headword;
    MatchMode mode = MatchMode::kExact;
    double score = 1.0;
};

struct FullTextResult {
    DictionaryIdentity dictionary;
    std::string headword;
    std::string document_id;
    MatchInfo match;
    std::string excerpt;
    std::vector<FullTextMatch> matches;
};

struct FullTextResponse {
    std::vector<FullTextResult> results;
    std::vector<FullTextError> errors;
    bool partial = false;
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
    virtual HeadwordEnumerationPage EnumerateHeadwords(
        const HeadwordEnumerationQuery& query,
        const CancellationToken* cancellation = nullptr) const = 0;
    virtual FullTextResponse SearchFullText(
        const FullTextQuery& query,
        const CancellationToken* cancellation = nullptr) const;
    virtual std::unique_ptr<LookupRequest> StartLookup(
        LookupQuery query) const = 0;
    virtual std::vector<std::byte> GetResource(
        const ResourceReference& resource,
        const CancellationToken* cancellation = nullptr) const = 0;
};

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_DICTIONARY_SERVICE_H_
