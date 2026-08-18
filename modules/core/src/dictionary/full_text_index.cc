// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_index.h"

#include <algorithm>
#include <cstdint>
#include <regex>
#include <string_view>
#include <tuple>

#include "../foundation/text_folding.h"
#include "../foundation/utf8.h"
#include "generated_index.h"

namespace goldendict::core::dictionary {
namespace {

constexpr std::string_view kFormat = "full-text-v1:";

void Check(const CancellationToken* cancellation,
           std::chrono::steady_clock::time_point deadline) {
    if (cancellation != nullptr && cancellation->IsCancellationRequested()) {
        throw FullTextIndexError(FullTextErrorCode::kCancelled,
                                 "Full-text operation cancelled");
    }
    if (std::chrono::steady_clock::now() >= deadline) {
        throw FullTextIndexError(FullTextErrorCode::kDeadlineExceeded,
                                 "Full-text operation deadline exceeded");
    }
}

void Append32(std::uint32_t value, std::string* output) {
    for (int shift = 24; shift >= 0; shift -= 8)
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
}

void AppendString(std::string_view value, std::string* output) {
    if (value.size() > UINT32_MAX)
        throw FullTextIndexError(FullTextErrorCode::kResourceLimit,
                                 "Full-text index field exceeds its bound");
    Append32(static_cast<std::uint32_t>(value.size()), output);
    output->append(value);
}

class Cursor {
   public:
    explicit Cursor(std::string_view data) : data_(data) {}

    std::uint32_t U32() {
        Need(4U);
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i)
            value = (value << 8U) | static_cast<unsigned char>(data_[pos_++]);
        return value;
    }

    std::string String() {
        const auto size = U32();
        Need(size);
        std::string value(data_.substr(pos_, size));
        pos_ += size;
        return value;
    }

    bool end() const noexcept { return pos_ == data_.size(); }

   private:
    void Need(std::size_t size) {
        if (size > data_.size() - pos_)
            throw FullTextIndexError(FullTextErrorCode::kMalformedIndex,
                                     "Full-text index is truncated");
    }

    std::string_view data_;
    std::size_t pos_ = 0U;
};

void ValidateDocuments(const std::vector<FullTextDocument>& documents) {
    if (documents.size() > kMaximumFullTextDocuments)
        throw FullTextIndexError(FullTextErrorCode::kResourceLimit,
                                 "Full-text corpus has too many documents");
    std::size_t total = 0U;
    for (const auto& document : documents) {
        if (document.dictionary.id.empty() || document.headword.empty() ||
            document.document_id.empty() ||
            document.plain_text.size() > kMaximumFullTextDocumentBytes ||
            !foundation::IsValidUtf8(document.headword) ||
            !foundation::IsValidUtf8(document.plain_text)) {
            throw FullTextIndexError(
                FullTextErrorCode::kResourceLimit,
                "Full-text document is malformed or oversized");
        }
        if (document.plain_text.size() > kMaximumFullTextCorpusBytes - total)
            throw FullTextIndexError(FullTextErrorCode::kResourceLimit,
                                     "Full-text corpus exceeds its byte bound");
        total += document.plain_text.size();
    }
}

std::string Serialize(const std::vector<FullTextDocument>& documents) {
    std::string output;
    Append32(static_cast<std::uint32_t>(documents.size()), &output);
    for (const auto& document : documents) {
        AppendString(document.dictionary.id, &output);
        AppendString(document.dictionary.name, &output);
        AppendString(document.headword, &output);
        AppendString(document.document_id, &output);
        AppendString(document.plain_text, &output);
    }
    return output;
}

std::vector<FullTextDocument> Parse(std::string_view payload) {
    Cursor cursor(payload);
    const auto count = cursor.U32();
    if (count > kMaximumFullTextDocuments)
        throw FullTextIndexError(FullTextErrorCode::kMalformedIndex,
                                 "Full-text index document count is invalid");
    std::vector<FullTextDocument> documents;
    documents.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        FullTextDocument document;
        document.dictionary.id = cursor.String();
        document.dictionary.name = cursor.String();
        document.headword = cursor.String();
        document.document_id = cursor.String();
        document.plain_text = cursor.String();
        documents.push_back(std::move(document));
    }
    if (!cursor.end())
        throw FullTextIndexError(FullTextErrorCode::kMalformedIndex,
                                 "Full-text index has trailing data");
    ValidateDocuments(documents);
    return documents;
}

std::string WildcardRegex(std::string_view pattern) {
    std::string result;
    for (char ch : pattern) {
        if (ch == '*')
            result += ".*";
        else if (ch == '?')
            result += '.';
        else {
            if (std::string_view(".^$|()[]{}+\\").find(ch) !=
                std::string_view::npos)
                result += '\\';
            result += ch;
        }
    }
    return result;
}

struct Word {
    std::string_view text;
    std::size_t offset;
};

struct MappedText {
    std::string text;
    std::vector<std::pair<std::size_t, std::size_t>> original;
};

std::size_t Utf8CharacterBytes(unsigned char byte) {
    if ((byte & 0x80U) == 0U)
        return 1U;
    if ((byte & 0xe0U) == 0xc0U)
        return 2U;
    if ((byte & 0xf0U) == 0xe0U)
        return 3U;
    return 4U;
}

bool IsUtf8Boundary(std::string_view text, std::size_t offset) {
    return offset <= text.size() &&
           (offset == text.size() ||
            (static_cast<unsigned char>(text[offset]) & 0xc0U) != 0x80U);
}

struct Excerpt {
    std::string text;
    std::size_t byte_offset = 0U;
};

Excerpt BuildExcerpt(std::string_view document, std::size_t match_offset,
                     std::size_t match_length) {
    const auto match_end = match_offset + match_length;
    if (match_length > kMaximumFullTextExcerptBytes) {
        auto end = std::min(document.size(),
                            match_offset + kMaximumFullTextExcerptBytes);
        while (!IsUtf8Boundary(document, end))
            --end;
        return {std::string(document.substr(match_offset, end - match_offset)),
                match_offset};
    }

    const auto earliest = match_offset > kMaximumFullTextExcerptBytes
                              ? match_offset - kMaximumFullTextExcerptBytes
                              : 0U;
    std::size_t best_begin = match_offset;
    std::size_t best_end = match_end;
    std::size_t best_length = match_length;
    std::size_t best_imbalance = 0U;
    for (std::size_t begin = earliest; begin <= match_offset; ++begin) {
        if (!IsUtf8Boundary(document, begin))
            continue;
        auto end =
            std::min(document.size(), begin + kMaximumFullTextExcerptBytes);
        while (!IsUtf8Boundary(document, end))
            --end;
        if (end < match_end)
            continue;
        const auto length = end - begin;
        const auto before = match_offset - begin;
        const auto after = end - match_end;
        const auto imbalance = before > after ? before - after : after - before;
        if (length > best_length ||
            (length == best_length && imbalance < best_imbalance) ||
            (length == best_length && imbalance == best_imbalance &&
             begin < best_begin)) {
            best_begin = begin;
            best_end = end;
            best_length = length;
            best_imbalance = imbalance;
        }
    }
    return {std::string(document.substr(best_begin, best_end - best_begin)),
            best_begin};
}

MappedText NormalizeMapped(std::string_view value, bool match_case,
                           bool ignore_diacritics) {
    MappedText result;
    for (std::size_t begin = 0U; begin < value.size();) {
        const auto end = std::min(
            value.size(), begin + Utf8CharacterBytes(static_cast<unsigned char>(
                                      value[begin])));
        const auto normalized =
            match_case
                ? std::string(value.substr(begin, end - begin))
                : foundation::NormalizeForExactLookup(
                      value.substr(begin, end - begin), ignore_diacritics);
        result.text += normalized;
        for (std::size_t i = 0U; i < normalized.size(); ++i)
            result.original.push_back({begin, end});
        begin = end;
    }
    return result;
}

std::vector<Word> Words(std::string_view text) {
    std::vector<Word> words;
    for (std::size_t begin = 0U; begin < text.size();) {
        while (begin < text.size() &&
               std::isspace(static_cast<unsigned char>(text[begin])))
            ++begin;
        if (begin == text.size())
            break;
        std::size_t end = begin;
        while (end < text.size() &&
               !std::isspace(static_cast<unsigned char>(text[end])))
            ++end;
        words.push_back({text.substr(begin, end - begin), begin});
        begin = end;
    }
    return words;
}

std::optional<std::pair<std::size_t, std::size_t>> MatchWords(
    std::string_view haystack, std::string_view query, bool whole_words,
    bool ignore_order, std::optional<std::uint32_t> maximum_distance) {
    const auto source = Words(haystack);
    const auto wanted = Words(query);
    if (wanted.empty())
        return std::nullopt;
    const auto matches = [whole_words](std::string_view left,
                                       std::string_view right) {
        return whole_words ? left == right
                           : left.find(right) != std::string_view::npos;
    };
    for (std::size_t start = 0U; start < source.size(); ++start) {
        std::vector<std::size_t> selected;
        std::size_t cursor = start;
        bool found = true;
        for (const auto& term : wanted) {
            std::size_t position = source.size();
            const std::size_t begin = ignore_order ? start : cursor;
            for (std::size_t i = begin; i < source.size(); ++i) {
                if ((!ignore_order ||
                     std::find(selected.begin(), selected.end(), i) ==
                         selected.end()) &&
                    matches(source[i].text, term.text)) {
                    position = i;
                    cursor = i + 1U;
                    break;
                }
            }
            if (position == source.size()) {
                found = false;
                break;
            }
            selected.push_back(position);
        }
        if (!found)
            continue;
        std::sort(selected.begin(), selected.end());
        if (maximum_distance.has_value()) {
            for (std::size_t i = 1U; i < selected.size(); ++i) {
                if (selected[i] - selected[i - 1U] - 1U > *maximum_distance)
                    found = false;
            }
        }
        if (!found)
            continue;
        const auto first = source[selected.front()].offset;
        const auto last = source[selected.back()];
        return std::pair{first, last.offset + last.text.size() - first};
    }
    return std::nullopt;
}

}  // namespace

FullTextIndexError::FullTextIndexError(FullTextErrorCode code,
                                       std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

FullTextIndex FullTextIndex::OpenOrBuild(
    const std::filesystem::path& path, const SourceSnapshot& sources,
    std::vector<FullTextDocument> documents,
    const CancellationToken* cancellation,
    std::chrono::steady_clock::time_point deadline) {
    Check(cancellation, deadline);
    ValidateDocuments(documents);
    std::sort(documents.begin(), documents.end(),
              [](const auto& left, const auto& right) {
                  const auto left_headword =
                      foundation::FoldForLookup(left.headword);
                  const auto right_headword =
                      foundation::FoldForLookup(right.headword);
                  return std::tie(left.dictionary.id, left_headword,
                                  left.document_id) <
                         std::tie(right.dictionary.id, right_headword,
                                  right.document_id);
              });
    auto loaded = LoadGeneratedIndex(path, kFormat, sources);
    FullTextIndex result;
    if (loaded.state == GeneratedIndexState::kCurrent) {
        try {
            result.documents_ = Parse(loaded.payload);
            result.state_ = FullTextIndexState::kReused;
            return result;
        } catch (const FullTextIndexError&) {
            loaded.state = GeneratedIndexState::kCorrupt;
        }
    }
    Check(cancellation, deadline);
    StoreGeneratedIndex(path, kFormat, sources, Serialize(documents));
    result.documents_ = std::move(documents);
    result.state_ = loaded.state == GeneratedIndexState::kMissing
                        ? FullTextIndexState::kCreated
                    : loaded.state == GeneratedIndexState::kStale
                        ? FullTextIndexState::kRebuiltStale
                        : FullTextIndexState::kRebuiltCorrupt;
    return result;
}

FullTextResponse FullTextIndex::Search(
    const FullTextQuery& query, const CancellationToken* cancellation) const {
    FullTextResponse response;
    if (query.text.empty() || query.text.size() > kMaximumFullTextQueryBytes ||
        query.result_limit == 0U ||
        query.result_limit > kMaximumFullTextResults ||
        query.timeout <= std::chrono::milliseconds::zero() ||
        query.dictionary_ids.size() > kMaximumLookupDictionaryFilters ||
        (query.maximum_word_distance.has_value() &&
         *query.maximum_word_distance > kMaximumFullTextWordDistance) ||
        !foundation::IsValidUtf8(query.text)) {
        response.errors.push_back({FullTextErrorCode::kInvalidQuery,
                                   {},
                                   "Invalid bounded full-text query"});
        return response;
    }
    if ((query.mode == FullTextQueryMode::kWildcard ||
         query.mode == FullTextQueryMode::kRegularExpression) &&
        (query.ignore_word_order || query.maximum_word_distance.has_value())) {
        response.errors.push_back(
            {FullTextErrorCode::kInvalidQuery,
             {},
             "Pattern modes do not accept word constraints"});
        return response;
    }
    const auto deadline = std::chrono::steady_clock::now() + query.timeout;
    std::string needle = query.match_case
                             ? query.text
                             : foundation::NormalizeForExactLookup(
                                   query.text, query.ignore_diacritics);
    std::optional<std::regex> expression;
    try {
        if (query.mode == FullTextQueryMode::kWildcard)
            expression.emplace(WildcardRegex(needle));
        else if (query.mode == FullTextQueryMode::kRegularExpression)
            expression.emplace(needle);
    } catch (const std::regex_error&) {
        response.errors.push_back({FullTextErrorCode::kInvalidQuery,
                                   {},
                                   "Malformed full-text pattern"});
        return response;
    }
    for (const auto& document : documents_) {
        try {
            Check(cancellation, deadline);
        } catch (const FullTextIndexError& error) {
            response.errors.push_back({error.code(), {}, error.what()});
            response.partial = !response.results.empty();
            return response;
        }
        if (query.dictionary_filter_active &&
            std::find(query.dictionary_ids.begin(), query.dictionary_ids.end(),
                      document.dictionary.id) == query.dictionary_ids.end())
            continue;
        const auto mapped = NormalizeMapped(
            document.plain_text, query.match_case, query.ignore_diacritics);
        const std::string& haystack = mapped.text;
        std::size_t offset = std::string::npos;
        std::size_t length = 0U;
        if (expression.has_value()) {
            std::smatch match;
            if (std::regex_search(haystack, match, *expression)) {
                offset = static_cast<std::size_t>(match.position());
                length = static_cast<std::size_t>(match.length());
                if (length == 0U)
                    offset = std::string::npos;
            }
        } else {
            const auto matched = MatchWords(
                haystack, needle, query.mode == FullTextQueryMode::kWholeWords,
                query.ignore_word_order, query.maximum_word_distance);
            if (matched.has_value()) {
                offset = matched->first;
                length = matched->second;
            }
        }
        if (offset == std::string::npos)
            continue;
        const auto original_offset = mapped.original[offset].first;
        const auto original_end = mapped.original[offset + length - 1U].second;
        offset = original_offset;
        length = original_end - original_offset;
        FullTextResult found;
        found.dictionary = document.dictionary;
        found.headword = document.headword;
        found.document_id = document.document_id;
        found.match = {query.text, needle, MatchMode::kFullText, 1.0};
        if (offset < document.plain_text.size()) {
            length = std::min(length, document.plain_text.size() - offset);
            found.matches.push_back(
                {offset, length, document.plain_text.substr(offset, length)});
            const auto excerpt =
                BuildExcerpt(document.plain_text, offset, length);
            found.excerpt = excerpt.text;
            found.excerpt_byte_offset = excerpt.byte_offset;
        }
        response.results.push_back(std::move(found));
        if (response.results.size() == query.result_limit)
            break;
    }
    return response;
}

std::optional<ResolvedFullTextDocument> FullTextIndex::ResolveDocument(
    std::string_view document_id) const {
    const auto found =
        std::find_if(documents_.begin(), documents_.end(),
                     [document_id](const FullTextDocument& document) {
                         return document.document_id == document_id;
                     });
    if (found == documents_.end()) {
        return std::nullopt;
    }
    return ResolvedFullTextDocument{found->dictionary, found->document_id,
                                    found->headword};
}

}  // namespace goldendict::core::dictionary
