// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictd_dictionary.h"

#include <algorithm>
#include <iterator>
#include <system_error>
#include <utility>

namespace goldendict::core::formats::dictd {
namespace {

dictionary::Error TranslateError(const Error& error) {
    switch (error.code()) {
        case ErrorCode::kMissingFile:
            return dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                     error.what());
        case ErrorCode::kInvalidIndex:
        case ErrorCode::kInvalidDictionary:
            return dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                     error.what());
    }
    return dictionary::Error(dictionary::ErrorCode::kInvalidData, error.what());
}

std::vector<dictionary::Article> Translate(std::vector<Article> source) {
    std::vector<dictionary::Article> articles;
    articles.reserve(source.size());
    std::transform(source.begin(), source.end(), std::back_inserter(articles),
                   [](auto&& item) {
                       return dictionary::Article{std::move(item.headword),
                                                  "text/plain",
                                                  std::move(item.data)};
                   });
    return articles;
}

}  // namespace

Dictionary Dictionary::Open(std::string id,
                            const std::filesystem::path& index_path) {
    try {
        Dictionary dictionary;
        dictionary.reader_ = Reader::Open(index_path);
        dictionary.identity_.id = std::move(id);
        dictionary.identity_.name = dictionary.reader_.name();
        std::error_code error;
        const auto canonical =
            std::filesystem::weakly_canonical(index_path, error);
        dictionary.identity_.source =
            (error ? index_path.lexically_normal() : canonical).string();
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
        auto articles = reader_.LookupExact(
            headword, options.result_limit,
            [&options]() { dictionary::CheckRequest(options); });
        dictionary::CheckRequest(options);
        return Translate(std::move(articles));
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

std::vector<dictionary::Article> Dictionary::LookupPrefix(
    std::string_view prefix, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    try {
        auto articles = reader_.LookupPrefix(
            prefix, options.result_limit,
            [&options]() { dictionary::CheckRequest(options); });
        dictionary::CheckRequest(options);
        return Translate(std::move(articles));
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

std::vector<std::string> Dictionary::SuggestPrefix(
    std::string_view prefix, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    try {
        auto suggestions = reader_.SuggestPrefix(
            prefix, options.result_limit,
            [&options]() { dictionary::CheckRequest(options); });
        dictionary::CheckRequest(options);
        return suggestions;
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

std::optional<dictionary::Resource> Dictionary::GetResource(
    std::string_view resource_id,
    const dictionary::RequestOptions& options) const {
    static_cast<void>(resource_id);
    dictionary::CheckRequest(options);
    return std::nullopt;
}

}  // namespace goldendict::core::formats::dictd
