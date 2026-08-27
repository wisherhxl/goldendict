// SPDX-License-Identifier: GPL-3.0-or-later

#include "hunspell_provider.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <unicode/uchar.h>
#include <unicode/utf8.h>
#include <hunspell/hunspell.hxx>

#include "../foundation/text_encoding.h"
#include "../foundation/text_folding.h"
#include "../foundation/utf8.h"
#include "hunspell_content.h"

namespace goldendict::core::morphology::hunspell {
namespace {

inline constexpr std::size_t kMaximumExactQueryBytes = 4096U;
inline constexpr std::size_t kMaximumMorphologyCharacters = 80U;
inline constexpr std::size_t kMaximumAnalysisRecordBytes = 16384U;
inline constexpr std::size_t kMaximumCompoundRuns = 21U;
inline constexpr std::size_t kMaximumCompoundStemsPerWord = 2U;

bool IsOuterWhitespaceOrPunctuation(UChar32 code_point) noexcept {
    return u_isUWhiteSpace(code_point) || u_ispunct(code_point);
}

std::string_view TrimOuterWhitespaceOrPunctuation(std::string_view text) {
    int32_t begin = 0;
    const auto length = static_cast<int32_t>(text.size());
    while (begin < length) {
        int32_t next = begin;
        UChar32 code_point = 0;
        U8_NEXT(text.data(), next, length, code_point);
        if (code_point < 0 || !IsOuterWhitespaceOrPunctuation(code_point))
            break;
        begin = next;
    }

    int32_t end = length;
    while (end > begin) {
        int32_t previous = end;
        UChar32 code_point = 0;
        U8_PREV(text.data(), begin, previous, code_point);
        if (code_point < 0 || !IsOuterWhitespaceOrPunctuation(code_point))
            break;
        end = previous;
    }
    return text.substr(static_cast<std::size_t>(begin),
                       static_cast<std::size_t>(end - begin));
}

std::size_t CharacterCount(std::string_view text) noexcept {
    std::size_t count = 0U;
    int32_t position = 0;
    const auto length = static_cast<int32_t>(text.size());
    while (position < length) {
        UChar32 code_point = 0;
        U8_NEXT(text.data(), position, length, code_point);
        ++count;
    }
    return count;
}

bool ContainsWhitespace(std::string_view text) noexcept {
    int32_t position = 0;
    const auto length = static_cast<int32_t>(text.size());
    while (position < length) {
        UChar32 code_point = 0;
        U8_NEXT(text.data(), position, length, code_point);
        if (u_isUWhiteSpace(code_point))
            return true;
    }
    return false;
}

struct CompoundRun {
    std::string_view text;
    bool separator = false;
};

std::vector<CompoundRun> TokenizeCompound(std::string_view text) {
    std::vector<CompoundRun> runs;
    int32_t position = 0;
    const auto length = static_cast<int32_t>(text.size());
    while (position < length) {
        const auto begin = position;
        UChar32 code_point = 0;
        U8_NEXT(text.data(), position, length, code_point);
        const bool separator = IsOuterWhitespaceOrPunctuation(code_point);
        while (position < length) {
            const auto previous = position;
            U8_NEXT(text.data(), position, length, code_point);
            if (IsOuterWhitespaceOrPunctuation(code_point) != separator) {
                position = previous;
                break;
            }
        }
        runs.push_back({text.substr(static_cast<std::size_t>(begin),
                                    static_cast<std::size_t>(position - begin)),
                        separator});
    }
    return runs;
}

bool IsFieldStart(std::string_view record, std::size_t position) noexcept {
    return position == 0U || record[position - 1U] == ' ' ||
           record[position - 1U] == '\t';
}

}  // namespace

std::optional<std::string_view> detail::ExtractStem(std::string_view record) {
    const auto comment = record.find('#');
    if (comment != std::string_view::npos)
        record = record.substr(0U, comment);

    for (std::size_t position = 0U; position + 3U <= record.size();
         ++position) {
        if (!IsFieldStart(record, position) ||
            record.substr(position, 3U) != "st:") {
            continue;
        }
        const auto value_begin = position + 3U;
        auto value_end = value_begin;
        while (value_end < record.size() && record[value_end] != ' ' &&
               record[value_end] != '\t') {
            ++value_end;
        }
        if (value_end != value_begin)
            return record.substr(value_begin, value_end - value_begin);
    }
    return std::nullopt;
}

namespace {

std::mutex& EngineMutex() {
    static std::mutex mutex;
    return mutex;
}

dictionary::Error TranslateContentError(const ContentError& error) {
    const auto code = error.code() == ContentErrorCode::kMissingFile
                          ? dictionary::ErrorCode::kUnavailable
                          : dictionary::ErrorCode::kInvalidData;
    return dictionary::Error(code, error.what());
}

class Provider final : public dictionary::Backend,
                       public dictionary::SynonymBackend {
   public:
    explicit Provider(Content content)
        : encoding_(std::move(content.encoding)),
          engine_(content.files.affix_file.string().c_str(),
                  content.files.dictionary_file.string().c_str()) {
        identity_.id = content.files.dictionary_id;
        identity_.name = content.files.dictionary_id;
        identity_.source = content.files.affix_file.string();
        identity_.headword_count = content.dictionary_entry_count;
    }

    const dictionary::Identity& identity() const noexcept override {
        return identity_;
    }

    std::vector<dictionary::Article> LookupExact(
        std::string_view headword,
        const dictionary::RequestOptions& options) const override {
        dictionary::CheckRequest(options);
        if (options.result_limit == 0U)
            return {};
        if (headword.empty() || headword.size() > kMaximumExactQueryBytes ||
            headword.find('\0') != std::string_view::npos ||
            !foundation::IsValidUtf8(headword)) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    "Invalid Hunspell exact-lookup query");
        }
        if (std::any_of(headword.begin(), headword.end(), [](char character) {
                return character == ' ' || character == '\t' ||
                       character == '\r' || character == '\n';
            })) {
            return {};
        }

        std::string encoded;
        try {
            encoded = foundation::EncodeFromUtf8(headword, encoding_,
                                                 kMaximumExactQueryBytes * 4U);
        } catch (const foundation::TextEncodingError& error) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    error.what());
        }

        bool accepted = false;
        {
            std::lock_guard<std::mutex> lock(EngineMutex());
            accepted = engine_.spell(encoded) != 0;
        }
        dictionary::CheckRequest(options);
        if (!accepted)
            return {};
        return {{std::string(headword), "text/plain", {}}};
    }

    std::vector<std::string> FindHeadwordsForSynonym(
        std::string_view headword,
        const dictionary::RequestOptions& options) const override {
        dictionary::CheckRequest(options);
        if (options.result_limit == 0U)
            return {};
        if (headword.size() > kMaximumExactQueryBytes ||
            headword.find('\0') != std::string_view::npos ||
            !foundation::IsValidUtf8(headword)) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    "Invalid Hunspell morphology query");
        }

        headword = TrimOuterWhitespaceOrPunctuation(headword);
        if (headword.empty() ||
            CharacterCount(headword) > kMaximumMorphologyCharacters) {
            return {};
        }

        if (ContainsWhitespace(headword))
            return FindCompoundStems(headword, options);

        return AnalyzeWord(headword, options);
    }

    std::vector<dictionary::Article> LookupPrefix(
        std::string_view prefix,
        const dictionary::RequestOptions& options) const override {
        dictionary::CheckRequest(options);
        if (options.result_limit == 0U)
            return {};
        if (prefix.empty() || prefix.size() > kMaximumExactQueryBytes ||
            prefix.find('\0') != std::string_view::npos ||
            !foundation::IsValidUtf8(prefix)) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    "Invalid Hunspell prefix-lookup query");
        }

        prefix = TrimOuterWhitespaceOrPunctuation(prefix);
        if (prefix.empty() || ContainsWhitespace(prefix))
            return {};

        std::string encoded;
        try {
            encoded = foundation::EncodeFromUtf8(prefix, encoding_,
                                                 kMaximumExactQueryBytes * 4U);
        } catch (const foundation::TextEncodingError& error) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    error.what());
        }

        dictionary::CheckRequest(options);
        bool accepted = false;
        {
            std::lock_guard<std::mutex> lock(EngineMutex());
            dictionary::CheckRequest(options);
            accepted = engine_.spell(encoded) != 0;
        }
        dictionary::CheckRequest(options);
        if (!accepted)
            return {};
        return {{std::string(prefix), "text/plain", {}}};
    }

    std::vector<std::string> SuggestPrefix(
        std::string_view /*prefix*/,
        const dictionary::RequestOptions& options) const override {
        dictionary::CheckRequest(options);
        return {};
    }

    std::optional<dictionary::Resource> GetResource(
        std::string_view /*resource_id*/,
        const dictionary::RequestOptions& options) const override {
        dictionary::CheckRequest(options);
        return std::nullopt;
    }

   private:
    std::vector<std::string> AnalyzeWord(
        std::string_view word, const dictionary::RequestOptions& options,
        std::size_t stem_limit =
            std::numeric_limits<std::size_t>::max()) const {

        std::string encoded;
        try {
            encoded = foundation::EncodeFromUtf8(word, encoding_,
                                                 kMaximumExactQueryBytes * 4U);
        } catch (const foundation::TextEncodingError& error) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    error.what());
        }

        dictionary::CheckRequest(options);
        std::vector<std::string> records;
        {
            std::lock_guard<std::mutex> lock(EngineMutex());
            dictionary::CheckRequest(options);
            records = engine_.analyze(encoded);
        }
        dictionary::CheckRequest(options);

        std::vector<std::string> stems;
        const auto folded_input = foundation::FoldSimpleCase(word);
        for (const auto& encoded_record : records) {
            dictionary::CheckRequest(options);
            try {
                const auto record = foundation::DecodeToUtf8(
                    encoded_record, encoding_, kMaximumAnalysisRecordBytes);
                const auto stem = detail::ExtractStem(record);
                if (!stem.has_value() || !foundation::IsValidUtf8(*stem) ||
                    foundation::FoldSimpleCase(*stem) == folded_input) {
                    continue;
                }
                stems.emplace_back(*stem);
                if (stems.size() == stem_limit)
                    break;
            } catch (const foundation::TextEncodingError&) {
                continue;
            } catch (const foundation::TextFoldingError&) {
                continue;
            }
        }
        return stems;
    }

    std::vector<std::string> FindCompoundStems(
        std::string_view expression,
        const dictionary::RequestOptions& options) const {
        const auto runs = TokenizeCompound(expression);
        if (runs.size() > kMaximumCompoundRuns)
            return {};

        const auto candidate_limit =
            options.result_limit == std::numeric_limits<std::size_t>::max()
                ? options.result_limit
                : options.result_limit + 1U;
        std::vector<std::string> candidates;
        for (const auto& run : runs) {
            dictionary::CheckRequest(options);
            if (run.separator) {
                for (auto& candidate : candidates) {
                    dictionary::CheckRequest(options);
                    candidate.append(run.text);
                }
                continue;
            }

            const auto stems =
                AnalyzeWord(run.text, options, kMaximumCompoundStemsPerWord);
            if (candidates.empty()) {
                candidates.emplace_back(run.text);
                for (const auto& stem : stems) {
                    if (candidates.size() == candidate_limit)
                        break;
                    candidates.push_back(stem);
                }
                continue;
            }

            const auto parent_count = candidates.size();
            for (std::size_t parent = 0U; parent < parent_count; ++parent) {
                dictionary::CheckRequest(options);
                const auto prefix = candidates[parent];
                candidates[parent].append(run.text);
                for (const auto& stem : stems) {
                    if (candidates.size() == candidate_limit)
                        break;
                    candidates.push_back(prefix + stem);
                }
            }
        }

        dictionary::CheckRequest(options);
        std::vector<std::string> alternatives;
        alternatives.reserve(std::min(options.result_limit, candidates.size()));
        for (auto& candidate : candidates) {
            dictionary::CheckRequest(options);
            if (candidate != expression)
                alternatives.push_back(std::move(candidate));
            if (alternatives.size() == options.result_limit)
                break;
        }
        return alternatives;
    }

    dictionary::Identity identity_;
    std::string encoding_;
    mutable Hunspell engine_;
};

}  // namespace

std::unique_ptr<dictionary::Backend> OpenProvider(const DataFiles& files) {
    try {
        return std::make_unique<Provider>(LoadContent(files));
    } catch (const ContentError& error) {
        throw TranslateContentError(error);
    } catch (const std::exception& error) {
        throw dictionary::Error(
            dictionary::ErrorCode::kInvalidData,
            std::string("Cannot initialize Hunspell: ") + error.what());
    }
}

}  // namespace goldendict::core::morphology::hunspell
