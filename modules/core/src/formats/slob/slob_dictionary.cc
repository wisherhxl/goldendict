// SPDX-License-Identifier: GPL-3.0-or-later
#include "slob_dictionary.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <system_error>
#include <utility>
#include "../../article/article_assembler.h"
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

Dictionary Dictionary::Open(
    std::string id, const std::filesystem::path& path,
    const std::optional<std::filesystem::path>& full_text_index_path) {
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
        if (full_text_index_path.has_value()) {
            try {
                std::vector<dictionary::FullTextDocument> documents;
                const auto sources = dictionary.reader_.ReadFullTextArticles();
                documents.reserve(sources.size());
                for (const auto& source : sources) {
                    dictionary::Article article{source.headword, "text/html",
                                                source.data};
                    auto assembled = article::Assemble(dictionary.identity_,
                                                       {std::move(article)});
                    dictionary::FullTextDocument document;
                    document.dictionary.id = dictionary.identity_.id;
                    document.dictionary.name = dictionary.identity_.name;
                    document.dictionary.source = dictionary.identity_.source;
                    document.dictionary.description =
                        dictionary.identity_.description;
                    document.dictionary.article_count =
                        dictionary.identity_.article_count;
                    document.dictionary.headword_count =
                        dictionary.identity_.headword_count;
                    document.dictionary.source_language =
                        dictionary.identity_.source_language;
                    document.dictionary.target_language =
                        dictionary.identity_.target_language;
                    document.dictionary.supports_headword_enumeration =
                        dictionary.identity_.supports_headword_enumeration;
                    document.headword = source.headword;
                    document.document_id =
                        "slob-index:" +
                        std::to_string(source.first_record_ordinal) + ":" +
                        std::to_string(source.article_ordinal) + ":" +
                        std::to_string(source.item_index) + ":" +
                        std::to_string(
                            static_cast<unsigned int>(source.bin_index));
                    document.plain_text = std::move(assembled.plain_text);
                    documents.push_back(std::move(document));
                }
                dictionary.full_text_index_ =
                    dictionary::FullTextIndex::OpenOrBuild(
                        *full_text_index_path,
                        dictionary.reader_.source_snapshot(),
                        std::move(documents));
            } catch (const dictionary::FullTextIndexError& full_text_error) {
                dictionary.full_text_error_ = {full_text_error.code(),
                                               dictionary.identity_.id,
                                               full_text_error.what()};
            } catch (const dictionary::GeneratedIndexError& index_error) {
                dictionary.full_text_error_ = {FullTextErrorCode::kInternal,
                                               dictionary.identity_.id,
                                               index_error.what()};
            } catch (const Error& reader_error) {
                dictionary.full_text_error_ = {
                    FullTextErrorCode::kResourceLimit, dictionary.identity_.id,
                    reader_error.what()};
            } catch (const dictionary::Error& article_error) {
                dictionary.full_text_error_ = {
                    FullTextErrorCode::kResourceLimit, dictionary.identity_.id,
                    article_error.what()};
            }
        }
        return dictionary;
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

FullTextResponse Dictionary::SearchFullText(
    const FullTextQuery& query, const CancellationToken* cancellation) const {
    if (full_text_error_.has_value()) {
        FullTextResponse response;
        response.errors.push_back(*full_text_error_);
        return response;
    }
    if (!full_text_index_.has_value()) {
        FullTextResponse response;
        response.errors.push_back(
            {FullTextErrorCode::kUnsupported, identity_.id,
             "Full-text indexing is not available for this dictionary"});
        return response;
    }
    return full_text_index_->Search(query, cancellation);
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
