// SPDX-License-Identifier: GPL-3.0-or-later
#include "aard_dictionary.h"
#include <algorithm>
#include <chrono>
#include <iterator>
#include <system_error>
#include <utility>
#include "../../article/article_assembler.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core::formats::aard {
namespace {
using dictionary::FullTextIndexWorkRequest;
using dictionary::FullTextIndexWorkResult;
using dictionary::FullTextIndexWorkStatus;

std::string SourceRevision(const dictionary::SourceSnapshot& snapshot) {
    if (snapshot.size() != 1U)
        throw dictionary::GeneratedIndexError(
            "AARD full-text work requires exactly one source");
    const auto& source = snapshot.front();
    return "aard-source-v1:" + std::to_string(source.path.size()) + ":" +
           source.path + ":" + std::to_string(source.size) + ":" +
           std::to_string(source.modified);
}

void CheckWork(const FullTextIndexWorkRequest& request) {
    if (request.cancellation != nullptr &&
        request.cancellation->IsCancellationRequested()) {
        throw dictionary::FullTextIndexError(FullTextErrorCode::kCancelled,
                                             "Full-text work cancelled");
    }
    if (std::chrono::steady_clock::now() >= request.deadline) {
        throw dictionary::FullTextIndexError(
            FullTextErrorCode::kDeadlineExceeded,
            "Full-text work deadline exceeded");
    }
}

class WorkPort final : public dictionary::FullTextIndexFormatWorkPort {
   public:
    WorkPort(std::shared_ptr<const Reader> reader,
             dictionary::Identity identity,
             std::optional<std::filesystem::path> destination)
        : reader_(std::move(reader)),
          identity_(std::move(identity)),
          destination_(std::move(destination)) {}

    bool IsFullTextIndexSupported() const noexcept override {
        return destination_.has_value();
    }

    std::string FullTextIndexSourceRevision() const override {
        return SourceRevision(reader_->source_snapshot());
    }

   private:
    FullTextIndexWorkResult DoPerformFullTextIndexWork(
        const FullTextIndexWorkRequest& request) override {
        if (!destination_.has_value())
            return {FullTextIndexWorkStatus::kFailed,
                    "AARD full-text index destination is not configured"};
        if (request.maximum_documents == 0U ||
            request.maximum_document_bytes == 0U ||
            request.maximum_corpus_bytes == 0U) {
            return {FullTextIndexWorkStatus::kFailed,
                    "AARD full-text work bounds must be nonzero"};
        }
        try {
            CheckWork(request);
            if (request.source_revision != FullTextIndexSourceRevision() ||
                request.source_revision !=
                    SourceRevision(dictionary::CaptureSourceSnapshot(
                        {reader_->dictionary_path()}))) {
                return {FullTextIndexWorkStatus::kFailed,
                        "AARD source revision changed"};
            }
            std::vector<dictionary::FullTextDocument> documents;
            documents.reserve(
                std::min(reader_->article_count(), request.maximum_documents));
            std::size_t corpus_bytes = 0U;
            reader_->VisitFullTextArticles(
                [&](const FullTextArticle& source) {
                    if (documents.size() >= request.maximum_documents)
                        throw dictionary::FullTextIndexError(
                            FullTextErrorCode::kResourceLimit,
                            "AARD document count bound exceeded");
                    dictionary::Article article{std::string(source.headword),
                                                "text/html",
                                                std::string(source.data)};
                    auto assembled =
                        article::Assemble(identity_, {std::move(article)});
                    const auto bytes = assembled.plain_text.size();
                    if (bytes > request.maximum_document_bytes ||
                        corpus_bytes > request.maximum_corpus_bytes ||
                        bytes > request.maximum_corpus_bytes - corpus_bytes) {
                        throw dictionary::FullTextIndexError(
                            FullTextErrorCode::kResourceLimit,
                            "AARD full-text byte bound exceeded");
                    }
                    corpus_bytes += bytes;
                    dictionary::FullTextDocument document;
                    document.dictionary.id = identity_.id;
                    document.dictionary.name = identity_.name;
                    document.dictionary.source = identity_.source;
                    document.dictionary.description = identity_.description;
                    document.dictionary.article_count = identity_.article_count;
                    document.dictionary.headword_count =
                        identity_.headword_count;
                    document.dictionary.source_language =
                        identity_.source_language;
                    document.dictionary.target_language =
                        identity_.target_language;
                    document.dictionary.supports_headword_enumeration =
                        identity_.supports_headword_enumeration;
                    document.headword = source.headword;
                    document.document_id =
                        "aard-index:" + std::to_string(source.record_ordinal) +
                        ":" + std::to_string(source.article_ordinal);
                    document.plain_text = std::move(assembled.plain_text);
                    documents.push_back(std::move(document));
                },
                [&request]() { CheckWork(request); });
            CheckWork(request);
            if (request.source_revision != FullTextIndexSourceRevision() ||
                request.source_revision !=
                    SourceRevision(dictionary::CaptureSourceSnapshot(
                        {reader_->dictionary_path()})))
                return {FullTextIndexWorkStatus::kFailed,
                        "AARD source revision changed"};
            auto prepared = dictionary::FullTextIndex::PrepareUpdate(
                *destination_, reader_->source_snapshot(), std::move(documents),
                request.cancellation, request.deadline);
            return {FullTextIndexWorkStatus::kCompleted,
                    {},
                    prepared->snapshot(),
                    std::move(prepared)};
        } catch (const dictionary::FullTextIndexError& error) {
            return {error.code() == FullTextErrorCode::kCancelled
                        ? FullTextIndexWorkStatus::kCancelled
                        : FullTextIndexWorkStatus::kFailed,
                    error.what()};
        }
    }

    std::shared_ptr<const Reader> reader_;
    dictionary::Identity identity_;
    std::optional<std::filesystem::path> destination_;
};

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

Dictionary Dictionary::Open(
    std::string id, const std::filesystem::path& dictionary_path,
    const std::optional<std::filesystem::path>& full_text_index_path) {
    try {
        Dictionary dictionary;
        dictionary.reader_ =
            std::make_shared<const Reader>(Reader::Open(dictionary_path));
        dictionary.full_text_snapshot_holder_ =
            std::make_shared<dictionary::FullTextIndexSnapshotHolder>();
        dictionary.identity_.id = std::move(id);
        dictionary.identity_.name = dictionary.reader_->metadata().name;
        dictionary.identity_.article_count =
            dictionary.reader_->article_count();
        dictionary.identity_.headword_count =
            dictionary.reader_->headword_count();
        dictionary.identity_.supports_headword_enumeration = true;
        dictionary.identity_.description =
            dictionary.reader_->metadata().description;
        dictionary.identity_.source_language =
            dictionary.reader_->metadata().source_language;
        dictionary.identity_.target_language =
            dictionary.reader_->metadata().target_language;
        std::error_code error;
        const auto canonical =
            std::filesystem::weakly_canonical(dictionary_path, error);
        dictionary.identity_.source =
            (error ? dictionary_path.lexically_normal() : canonical).string();
        if (full_text_index_path.has_value()) {
            try {
                std::vector<dictionary::FullTextDocument> documents;
                documents.reserve(dictionary.reader_->article_count());
                dictionary.reader_->VisitFullTextArticles(
                    [&](const FullTextArticle& source) {
                        dictionary::Article article{
                            std::string(source.headword), "text/html",
                            std::string(source.data)};
                        auto assembled = article::Assemble(
                            dictionary.identity_, {std::move(article)});
                        dictionary::FullTextDocument document;
                        document.dictionary.id = dictionary.identity_.id;
                        document.dictionary.name = dictionary.identity_.name;
                        document.dictionary.source =
                            dictionary.identity_.source;
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
                            "aard-index:" +
                            std::to_string(source.record_ordinal) + ":" +
                            std::to_string(source.article_ordinal);
                        document.plain_text = std::move(assembled.plain_text);
                        documents.push_back(std::move(document));
                    });
                auto snapshot =
                    std::make_shared<const dictionary::FullTextIndex>(
                        dictionary::FullTextIndex::OpenOrBuild(
                            *full_text_index_path,
                            dictionary.reader_->source_snapshot(),
                            std::move(documents)));
                if (dictionary.full_text_snapshot_holder_->Publish(snapshot)) {
                    dictionary.startup_full_text_snapshot_ =
                        std::move(snapshot);
                    dictionary.startup_full_text_source_revision_ =
                        SourceRevision(dictionary.reader_->source_snapshot());
                }
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
        dictionary.full_text_work_port_ = std::make_shared<WorkPort>(
            dictionary.reader_, dictionary.identity_, full_text_index_path);
        return dictionary;
    } catch (const Error& error) {
        throw TranslateError(error);
    }
}

std::optional<dictionary::FullTextIndexStartupArtifactEvidence>
Dictionary::StartupArtifactEvidence(
    dictionary::FullTextIndexWorkIdentity identity) const noexcept {
    if (startup_full_text_snapshot_ == nullptr ||
        startup_full_text_source_revision_.empty()) {
        return std::nullopt;
    }
    return dictionary::FullTextIndexStartupArtifactEvidence{
        std::move(identity), startup_full_text_source_revision_,
        startup_full_text_snapshot_};
}

FullTextResponse Dictionary::SearchFullText(
    const FullTextQuery& query, const CancellationToken* cancellation) const {
    const auto snapshot = full_text_snapshot_holder_->Acquire();
    if (snapshot == nullptr && full_text_error_.has_value()) {
        FullTextResponse response;
        response.errors.push_back(*full_text_error_);
        return response;
    }
    if (snapshot == nullptr) {
        FullTextResponse response;
        response.errors.push_back(
            {FullTextErrorCode::kUnsupported, identity_.id,
             "Full-text indexing is not available for this dictionary"});
        return response;
    }
    return snapshot->Search(query, cancellation);
}

std::vector<dictionary::Article> Dictionary::LookupExact(
    std::string_view headword,
    const dictionary::RequestOptions& options) const {
    dictionary::CheckRequest(options);
    try {
        auto result = reader_->LookupExact(
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
        auto result = reader_->LookupPrefix(
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
        auto result = reader_->SuggestPrefix(
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
        auto [headwords, complete] = reader_->EnumerateHeadwords(
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
