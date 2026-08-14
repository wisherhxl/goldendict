// SPDX-License-Identifier: GPL-3.0-or-later
#include "aard_dictionary.h"
#include <algorithm>
#include <iterator>
#include <system_error>
#include <utility>
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core::formats::aard {
namespace {
dictionary::Error TranslateError(const Error& error) {
    return dictionary::Error(error.code() == ErrorCode::kMissingFile
                                 ? dictionary::ErrorCode::kUnavailable
                                 : dictionary::ErrorCode::kInvalidData,
                             error.what());
}

std::vector<dictionary::Article> Translate(std::vector<Article> source) {
    std::vector<dictionary::Article> articles;
    articles.reserve(source.size());
    std::transform(source.begin(), source.end(), std::back_inserter(articles),
                   [](auto&& item) {
                       return dictionary::Article{std::move(item.headword),
                                                  "text/html",
                                                  std::move(item.data)};
                   });
    return articles;
}
}  // namespace

Dictionary Dictionary::Open(std::string id,
                            const std::filesystem::path& dictionary_path) {
    try {
        Dictionary dictionary;
        dictionary.reader_ = Reader::Open(dictionary_path);
        dictionary.identity_.id = std::move(id);
        dictionary.identity_.name = dictionary.reader_.metadata().name;
        dictionary.identity_.article_count = dictionary.reader_.article_count();
        dictionary.identity_.headword_count =
            dictionary.reader_.headword_count();
        dictionary.identity_.supports_headword_enumeration = true;
        dictionary.identity_.description =
            dictionary.reader_.metadata().description;
        dictionary.identity_.source_language =
            dictionary.reader_.metadata().source_language;
        dictionary.identity_.target_language =
            dictionary.reader_.metadata().target_language;
        std::error_code error;
        const auto canonical =
            std::filesystem::weakly_canonical(dictionary_path, error);
        dictionary.identity_.source =
            (error ? dictionary_path.lexically_normal() : canonical).string();
        return dictionary;
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

std::vector<dictionary::Article> Dictionary::LookupExact(
    std::string_view headword,
    const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    try {
        auto result = reader_.LookupExact(
            headword, options.result_limit,
            [&options]() { dictionary::CheckRequest(options); });
        dictionary::CheckRequest(options);
        return Translate(std::move(result));
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

std::vector<dictionary::Article> Dictionary::LookupPrefix(
    std::string_view prefix, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    try {
        auto result = reader_.LookupPrefix(
            prefix, options.result_limit,
            [&options]() { dictionary::CheckRequest(options); });
        dictionary::CheckRequest(options);
        return Translate(std::move(result));
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

std::vector<std::string> Dictionary::SuggestPrefix(
    std::string_view prefix, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    try {
        auto result = reader_.SuggestPrefix(
            prefix, options.result_limit,
            [&options]() { dictionary::CheckRequest(options); });
        dictionary::CheckRequest(options);
        return result;
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

dictionary::HeadwordPage Dictionary::EnumerateHeadwords(
    std::size_t offset, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    try {
        auto [headwords, complete] = reader_.EnumerateHeadwords(
            offset, options.result_limit,
            core::kMaximumHeadwordEnumerationResponseBytes,
            [&options]() { dictionary::CheckRequest(options); });
        dictionary::CheckRequest(options);
        return {std::move(headwords), complete};
    } catch (const dictionary::OrderedHeadwordError& error) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                error.what());
    }
}

std::optional<dictionary::Resource> Dictionary::GetResource(
    std::string_view resource_id,
    const dictionary::RequestOptions& options) const {
    static_cast<void>(resource_id);
    dictionary::CheckRequest(options);
    return std::nullopt;
}
}  // namespace goldendict::core::formats::aard
