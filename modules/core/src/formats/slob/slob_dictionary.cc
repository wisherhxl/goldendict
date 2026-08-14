// SPDX-License-Identifier: GPL-3.0-or-later
#include "slob_dictionary.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <system_error>
#include <utility>
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core::formats::slob {
namespace {
dictionary::Error TranslateError(const Error& error) {
    return dictionary::Error(error.code() == ErrorCode::kMissingFile
                                 ? dictionary::ErrorCode::kUnavailable
                                 : dictionary::ErrorCode::kInvalidData,
                             error.what());
}

std::vector<dictionary::Article> Translate(std::vector<Article> source) {
    std::vector<dictionary::Article> result;
    result.reserve(source.size());
    std::transform(source.begin(), source.end(), std::back_inserter(result),
                   [](auto&& article) {
                       return dictionary::Article{std::move(article.headword),
                                                  "text/html",
                                                  std::move(article.data)};
                   });
    return result;
}
}  // namespace

Dictionary Dictionary::Open(std::string id, const std::filesystem::path& path) {
    try {
        Dictionary dictionary;
        dictionary.reader_ = Reader::Open(path);
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
        const auto canonical = std::filesystem::weakly_canonical(path, error);
        dictionary.identity_.source =
            (error ? path.lexically_normal() : canonical).string();
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
        return Translate(reader_.LookupExact(
            headword, options.result_limit,
            [&options]() { dictionary::CheckRequest(options); }));
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

std::vector<dictionary::Article> Dictionary::LookupPrefix(
    std::string_view prefix, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    try {
        return Translate(reader_.LookupPrefix(
            prefix, options.result_limit,
            [&options]() { dictionary::CheckRequest(options); }));
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

std::vector<std::string> Dictionary::SuggestPrefix(
    std::string_view prefix, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    try {
        return reader_.SuggestPrefix(
            prefix, options.result_limit,
            [&options]() { dictionary::CheckRequest(options); });
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
    dictionary::CheckRequest(options);
    const std::string* data = reader_.Resource(resource_id);
    if (data == nullptr)
        return std::nullopt;
    dictionary::Resource resource;
    resource.id = std::string(resource_id);
    resource.media_type = dictionary::MediaTypeForResourceId(resource_id);
    resource.data.resize(data->size());
    std::transform(
        data->begin(), data->end(), resource.data.begin(),
        [](unsigned char byte) { return static_cast<std::byte>(byte); });
    return resource;
}
}  // namespace goldendict::core::formats::slob
