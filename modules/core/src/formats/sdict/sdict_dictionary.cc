// SPDX-License-Identifier: GPL-3.0-or-later

#include "sdict_dictionary.h"
#include "goldendict/core/dictionary_service.h"

#include <algorithm>
#include <iterator>
#include <system_error>
#include <utility>

#include "../../article/article_assembler.h"

namespace goldendict::core::formats::sdict {
namespace {

dictionary::Error TranslateError(const Error& error) {
    switch (error.code()) {
        case ErrorCode::kMissingFile:
            return dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                     error.what());
        case ErrorCode::kUnsupportedFeature:
            return dictionary::Error(dictionary::ErrorCode::kUnsupported,
                                     error.what());
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
                                                  "text/html",
                                                  std::move(item.data)};
                   });
    return articles;
}

}  // namespace

Dictionary Dictionary::Open(
    std::string id, const std::filesystem::path& dictionary_path,
    const std::optional<std::filesystem::path>& full_text_index_path) {
    try {
        Dictionary dictionary;
        dictionary.reader_ = Reader::Open(dictionary_path);
        dictionary.identity_.id = std::move(id);
        dictionary.identity_.name = dictionary.reader_.metadata().name;
        dictionary.identity_.article_count = dictionary.reader_.article_count();
        dictionary.identity_.headword_count =
            dictionary.reader_.headword_count();
        dictionary.identity_.supports_headword_enumeration = true;
        dictionary.identity_.source_language =
            dictionary.reader_.metadata().source_language;
        dictionary.identity_.target_language =
            dictionary.reader_.metadata().target_language;
        dictionary.identity_.description =
            dictionary.reader_.metadata().description;
        std::error_code filesystem_error;
        const auto canonical = std::filesystem::weakly_canonical(
            dictionary_path, filesystem_error);
        dictionary.identity_.source =
            (filesystem_error ? dictionary_path.lexically_normal() : canonical)
                .string();
        if (full_text_index_path.has_value()) {
            try {
                std::vector<dictionary::FullTextDocument> documents;
                const auto source_articles =
                    dictionary.reader_.ReadFullTextArticles();
                documents.reserve(source_articles.size());
                for (const auto& source : source_articles) {
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
                        "sdict-index:" + std::to_string(source.record_ordinal) +
                        ":" + std::to_string(source.article_offset);
                    document.plain_text = std::move(assembled.plain_text);
                    documents.push_back(std::move(document));
                }
                dictionary.full_text_index_ =
                    dictionary::FullTextIndex::OpenOrBuild(
                        *full_text_index_path,
                        dictionary.reader_.source_snapshot(),
                        std::move(documents));
            } catch (const dictionary::FullTextIndexError& error) {
                dictionary.full_text_error_ = FullTextError{
                    error.code(), dictionary.identity_.id, error.what()};
            } catch (const dictionary::GeneratedIndexError& error) {
                dictionary.full_text_error_ =
                    FullTextError{FullTextErrorCode::kInternal,
                                  dictionary.identity_.id, error.what()};
            } catch (const Error& error) {
                dictionary.full_text_error_ =
                    FullTextError{FullTextErrorCode::kResourceLimit,
                                  dictionary.identity_.id, error.what()};
            } catch (const dictionary::Error& error) {
                dictionary.full_text_error_ =
                    FullTextError{FullTextErrorCode::kResourceLimit,
                                  dictionary.identity_.id, error.what()};
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

}  // namespace goldendict::core::formats::sdict
