// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsl_dictionary.h"
#include "goldendict/core/dictionary_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "../../article/article_assembler.h"

namespace goldendict::core::formats::dsl {
namespace {

constexpr std::uintmax_t kMaximumResourceSize = 16U * 1024U * 1024U;

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

bool IsInside(const std::filesystem::path& root,
              const std::filesystem::path& path) {
    auto root_part = root.begin();
    auto path_part = path.begin();
    for (; root_part != root.end(); ++root_part, ++path_part) {
        if (path_part == path.end() || *root_part != *path_part)
            return false;
    }
    return true;
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

std::vector<std::filesystem::path> ResourceRoots(
    const std::filesystem::path& dictionary_path) {
    std::vector<std::filesystem::path> roots;
    roots.push_back(
        std::filesystem::u8path(dictionary_path.string() + ".files"));
    const std::string filename = Lower(dictionary_path.filename().string());
    if (filename.size() >= 7U &&
        filename.compare(filename.size() - 7U, 7U, ".dsl.dz") == 0) {
        auto plain = dictionary_path;
        plain.replace_filename(dictionary_path.filename().string().substr(
            0, dictionary_path.filename().string().size() - 3U));
        roots.push_back(std::filesystem::u8path(plain.string() + ".files"));
    }
    return roots;
}

std::optional<std::filesystem::path> ResolveResource(
    const std::filesystem::path& dictionary_path,
    std::string_view resource_id) {
    if (resource_id.empty() || resource_id.find('\0') != std::string_view::npos)
        return std::nullopt;
    const auto relative = std::filesystem::u8path(std::string(resource_id));
    if (relative.is_absolute())
        return std::nullopt;
    for (const auto& root : ResourceRoots(dictionary_path)) {
        std::error_code error;
        const auto canonical_root =
            std::filesystem::weakly_canonical(root, error);
        if (error)
            continue;
        const auto candidate =
            std::filesystem::weakly_canonical(root / relative, error);
        if (!error && IsInside(canonical_root, candidate) &&
            std::filesystem::is_regular_file(candidate, error) && !error) {
            return candidate;
        }
    }
    return std::nullopt;
}

}  // namespace

Dictionary Dictionary::Open(
    std::string id, const std::filesystem::path& dictionary_path,
    std::string_view preferred_language,
    const std::optional<std::filesystem::path>& full_text_index_path) {
    try {
        Dictionary dictionary;
        dictionary.reader_ = Reader::Open(dictionary_path, preferred_language);
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
        std::error_code error;
        const auto canonical =
            std::filesystem::weakly_canonical(dictionary_path, error);
        dictionary.identity_.source =
            (error ? dictionary_path.lexically_normal() : canonical).string();
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
                        "dsl-index:" + std::to_string(source.record_ordinal) +
                        ":" + std::to_string(source.article_ordinal);
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
    dictionary::CheckRequest(options);
    const auto path = ResolveResource(reader_.dictionary_path(), resource_id);
    if (!path.has_value())
        return std::nullopt;
    std::error_code error;
    const auto size = std::filesystem::file_size(*path, error);
    if (error || size > kMaximumResourceSize) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "DSL resource exceeds the size limit");
    }
    std::ifstream input(*path, std::ios::binary);
    if (!input) {
        throw dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                "Cannot open DSL resource");
    }
    dictionary::Resource resource;
    resource.id = std::string(resource_id);
    resource.media_type = dictionary::MediaTypeForResourceId(resource_id);
    resource.data.resize(static_cast<std::size_t>(size));
    if (!resource.data.empty() &&
        !input.read(reinterpret_cast<char*>(resource.data.data()),
                    static_cast<std::streamsize>(resource.data.size()))) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Cannot read complete DSL resource");
    }
    dictionary::CheckRequest(options);
    return resource;
}

}  // namespace goldendict::core::formats::dsl
