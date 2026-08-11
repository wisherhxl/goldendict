// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_dictionary.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace goldendict::core::formats::stardict {
namespace {

dictionary::Error TranslateReaderError(const Error& error) {
    switch (error.code()) {
        case ErrorCode::kMissingFile:
        case ErrorCode::kIndexStorage:
            return dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                     error.what());
        case ErrorCode::kUnsupportedFeature:
            return dictionary::Error(dictionary::ErrorCode::kUnsupported,
                                     error.what());
        case ErrorCode::kInvalidInfo:
        case ErrorCode::kInvalidIndex:
        case ErrorCode::kInvalidDictionary:
            return dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                     error.what());
    }
    return dictionary::Error(dictionary::ErrorCode::kInvalidData, error.what());
}

}  // namespace

Dictionary Dictionary::Open(
    std::string id, const std::filesystem::path& info_path,
    const std::optional<std::filesystem::path>& generated_index_path) {
    try {
        Dictionary dictionary;
        dictionary.reader_ = Reader::Open(info_path, generated_index_path);
        dictionary.identity_.id = std::move(id);
        dictionary.identity_.name = dictionary.reader_.metadata().book_name;
        dictionary.identity_.source = info_path.string();
        return dictionary;
    } catch (const Error& error) {
        throw TranslateReaderError(error);
    }
}

std::vector<dictionary::Article> Dictionary::LookupExact(
    std::string_view headword,
    const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    if (headword.empty() || options.result_limit == 0U) {
        return {};
    }

    auto raw_articles = reader_.LookupExact(headword, options.result_limit);
    dictionary::CheckRequest(options);

    std::vector<dictionary::Article> articles;
    articles.reserve(raw_articles.size());
    std::transform(raw_articles.begin(), raw_articles.end(),
                   std::back_inserter(articles), [](auto&& raw_article) {
                       dictionary::Article article;
                       article.headword = std::move(raw_article.headword);
                       article.format = "stardict/m";
                       article.data = std::move(raw_article.data);
                       return article;
                   });
    return articles;
}

std::optional<dictionary::Resource> Dictionary::GetResource(
    std::string_view resource_id,
    const dictionary::RequestOptions& options) const {
    static_cast<void>(resource_id);
    dictionary::CheckRequest(options);
    throw dictionary::Error(dictionary::ErrorCode::kUnsupported,
                            "StarDict resources are not implemented yet");
}

}  // namespace goldendict::core::formats::stardict
