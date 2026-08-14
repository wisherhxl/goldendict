// SPDX-License-Identifier: GPL-3.0-or-later

#include "xdxf_dictionary.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace goldendict::core::formats::xdxf {
namespace {

constexpr std::uintmax_t kMaximumResourceSize = 16U * 1024U * 1024U;

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

bool IsInside(const std::filesystem::path& root,
              const std::filesystem::path& path) {
    auto root_part = root.begin();
    auto path_part = path.begin();
    for (; root_part != root.end(); ++root_part, ++path_part) {
        if (path_part == path.end() || *root_part != *path_part) {
            return false;
        }
    }
    return true;
}

std::optional<std::filesystem::path> ResolveResource(
    const std::filesystem::path& dictionary_path,
    std::string_view resource_id) {
    if (resource_id.empty() ||
        resource_id.find('\0') != std::string_view::npos) {
        return std::nullopt;
    }
    const auto relative = std::filesystem::u8path(std::string(resource_id));
    if (relative.is_absolute()) {
        return std::nullopt;
    }
    const std::array<std::filesystem::path, 2> roots = {
        dictionary_path.parent_path(),
        std::filesystem::u8path(dictionary_path.string() + ".files")};
    for (const auto& candidate_root : roots) {
        std::error_code error;
        const auto canonical_root =
            std::filesystem::weakly_canonical(candidate_root, error);
        if (error) {
            continue;
        }
        const auto candidate =
            std::filesystem::weakly_canonical(candidate_root / relative, error);
        if (!error && IsInside(canonical_root, candidate) &&
            std::filesystem::is_regular_file(candidate, error) && !error) {
            return candidate;
        }
    }
    return std::nullopt;
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
    dictionary::CheckRequest(options);
    const auto path = ResolveResource(reader_.dictionary_path(), resource_id);
    if (!path.has_value()) {
        return std::nullopt;
    }
    std::error_code error;
    const auto size = std::filesystem::file_size(*path, error);
    if (error || size > kMaximumResourceSize) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "XDXF resource exceeds the size limit");
    }
    std::ifstream input(*path, std::ios::binary);
    if (!input) {
        throw dictionary::Error(dictionary::ErrorCode::kUnavailable,
                                "Cannot open XDXF resource");
    }
    dictionary::Resource resource;
    resource.id = std::string(resource_id);
    resource.media_type = dictionary::MediaTypeForResourceId(resource_id);
    resource.data.resize(static_cast<std::size_t>(size));
    if (!resource.data.empty() &&
        !input.read(reinterpret_cast<char*>(resource.data.data()),
                    static_cast<std::streamsize>(resource.data.size()))) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Cannot read complete XDXF resource");
    }
    dictionary::CheckRequest(options);
    return resource;
}

}  // namespace goldendict::core::formats::xdxf
