// SPDX-License-Identifier: GPL-3.0-or-later

#include "mdict_dictionary.h"

#include <algorithm>
#include <iterator>
#include <system_error>
#include <utility>

namespace goldendict::core::formats::mdict {
namespace {

dictionary::Error TranslateError(const Error& error) {
    dictionary::ErrorCode code = dictionary::ErrorCode::kInvalidData;
    if (error.code() == ErrorCode::kMissingFile)
        code = dictionary::ErrorCode::kUnavailable;
    else if (error.code() == ErrorCode::kUnsupported)
        code = dictionary::ErrorCode::kUnsupported;
    return dictionary::Error(code, error.what());
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

Dictionary Dictionary::Open(std::string id, const DictionaryFiles& files) {
    try {
        Dictionary dictionary;
        dictionary.reader_ = Reader::Open(files);
        dictionary.identity_.id = std::move(id);
        dictionary.identity_.name = dictionary.reader_.metadata().name;
        dictionary.identity_.description =
            dictionary.reader_.metadata().description;
        std::error_code error;
        const auto canonical =
            std::filesystem::weakly_canonical(files.mdx, error);
        dictionary.identity_.source =
            (error ? files.mdx.lexically_normal() : canonical).string();
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
    dictionary::CheckRequest(options);
    return resource;
}

}  // namespace goldendict::core::formats::mdict
