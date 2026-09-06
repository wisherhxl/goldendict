// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_dictionary.h"

#include <algorithm>
#include <iterator>
#include <system_error>
#include <utility>

#include "../../article/article_assembler.h"
#include "goldendict/core/dictionary_service.h"
#include "stardict_resource.h"

namespace goldendict::core::formats::stardict {
namespace {

constexpr std::string_view kFullTextSemanticsStamp =
    "goldendict:stardict-full-text-v2";

dictionary::SourceSnapshot FullTextSources(dictionary::SourceSnapshot sources) {
    sources.push_back(
        dictionary::SourceStamp{std::string(kFullTextSemanticsStamp), 0U, 0});
    return sources;
}

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

std::vector<dictionary::Article> TranslateArticles(
    std::vector<Article> raw_articles, std::string_view same_type_sequence) {
    std::vector<dictionary::Article> articles;
    articles.reserve(raw_articles.size());
    std::transform(
        raw_articles.begin(), raw_articles.end(), std::back_inserter(articles),
        [same_type_sequence](auto&& raw_article) {
            dictionary::Article article;
            article.headword = std::move(raw_article.headword);
            article.format =
                same_type_sequence == "h" ? "text/html" : "text/plain";
            article.data = std::move(raw_article.data);
            return article;
        });
    return articles;
}

}  // namespace

Dictionary Dictionary::Open(
    std::string id, const std::filesystem::path& info_path,
    const std::optional<std::filesystem::path>& generated_index_path,
    const std::optional<std::filesystem::path>& full_text_index_path) {
    try {
        Dictionary dictionary;
        dictionary.reader_ = Reader::Open(info_path, generated_index_path);
        dictionary.identity_.id = std::move(id);
        dictionary.identity_.name = dictionary.reader_.metadata().book_name;
        dictionary.identity_.article_count = dictionary.reader_.article_count();
        dictionary.identity_.headword_count =
            dictionary.reader_.headword_count();
        dictionary.identity_.supports_headword_enumeration = true;
        std::error_code filesystem_error;
        const auto canonical_source =
            std::filesystem::weakly_canonical(info_path, filesystem_error);
        dictionary.identity_.source =
            filesystem_error ? info_path.lexically_normal().string()
                             : canonical_source.string();
        dictionary.identity_.source_language =
            dictionary.reader_.metadata().source_language;
        dictionary.identity_.target_language =
            dictionary.reader_.metadata().target_language;
        dictionary.identity_.description =
            dictionary.reader_.metadata().description;
        dictionary.resource_root_ = info_path.parent_path() / "res";
        if (full_text_index_path.has_value()) {
            try {
                std::vector<dictionary::FullTextDocument> documents;
                const auto primary_articles =
                    dictionary.reader_.ReadPrimaryArticles();
                documents.reserve(primary_articles.size());
                for (const auto& primary : primary_articles) {
                    // The frozen Qt 5 HTML conversion bug can produce an empty
                    // headword. Such an unreachable entry must not disable the
                    // full-text index for every valid article in the
                    // dictionary.
                    if (primary.headword.empty()) {
                        continue;
                    }
                    dictionary::Article article;
                    article.headword = primary.headword;
                    article.format =
                        dictionary.reader_.metadata().same_type_sequence == "h"
                            ? "text/html"
                            : "text/plain";
                    article.data = primary.data;
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
                    document.headword = primary.headword;
                    document.document_id =
                        "stardict-idx:" +
                        std::to_string(primary.record_ordinal) + ":" +
                        std::to_string(primary.article_offset) + ":" +
                        std::to_string(primary.article_size);
                    document.plain_text = std::move(assembled.plain_text);
                    documents.push_back(std::move(document));
                }
                dictionary.full_text_index_ =
                    dictionary::FullTextIndex::OpenOrBuild(
                        *full_text_index_path,
                        FullTextSources(dictionary.reader_.source_snapshot()),
                        std::move(documents));
            } catch (const dictionary::FullTextIndexError& error) {
                dictionary.full_text_error_ = FullTextError{
                    error.code(), dictionary.identity_.id, error.what()};
            } catch (const dictionary::GeneratedIndexError& error) {
                dictionary.full_text_error_ =
                    FullTextError{FullTextErrorCode::kInternal,
                                  dictionary.identity_.id, error.what()};
            } catch (const dictionary::Error& error) {
                dictionary.full_text_error_ =
                    FullTextError{FullTextErrorCode::kResourceLimit,
                                  dictionary.identity_.id, error.what()};
            }
        }
        return dictionary;
    } catch (const Error& error) {
        throw TranslateReaderError(error);
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
    if (headword.empty() || options.result_limit == 0U) {
        return {};
    }

    auto raw_articles = reader_.LookupExact(
        headword, options.result_limit,
        [&options]() { dictionary::CheckRequest(options); });
    dictionary::CheckRequest(options);

    return TranslateArticles(std::move(raw_articles),
                             reader_.metadata().same_type_sequence);
}

std::vector<dictionary::Article> Dictionary::LookupPrefix(
    std::string_view prefix, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    if (prefix.empty() || options.result_limit == 0U) {
        return {};
    }

    auto raw_articles = reader_.LookupPrefix(
        prefix, options.result_limit,
        [&options]() { dictionary::CheckRequest(options); });
    dictionary::CheckRequest(options);
    return TranslateArticles(std::move(raw_articles),
                             reader_.metadata().same_type_sequence);
}

std::vector<std::string> Dictionary::SuggestPrefix(
    std::string_view prefix, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    if (prefix.empty() || options.result_limit == 0U) {
        return {};
    }
    auto suggestions = reader_.SuggestPrefix(
        prefix, options.result_limit,
        [&options]() { dictionary::CheckRequest(options); });
    dictionary::CheckRequest(options);
    return suggestions;
}

std::vector<std::string> Dictionary::FindHeadwordsForSynonym(
    std::string_view headword,
    const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    auto result = reader_.FindHeadwordsForSynonym(
        headword, options.result_limit,
        [&options]() { dictionary::CheckRequest(options); });
    dictionary::CheckRequest(options);
    return result;
}

dictionary::HeadwordPage Dictionary::EnumerateHeadwords(
    std::size_t offset, const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    auto [headwords, complete] = reader_.EnumerateHeadwords(
        offset, options.result_limit,
        core::kMaximumHeadwordEnumerationResponseBytes,
        [&options]() { dictionary::CheckRequest(options); });
    dictionary::CheckRequest(options);
    return {std::move(headwords), complete};
}

std::optional<dictionary::Resource> Dictionary::GetResource(
    std::string_view resource_id,
    const dictionary::RequestOptions& options) const {
    return LoadResource(resource_root_, resource_id, options);
}

}  // namespace goldendict::core::formats::stardict
