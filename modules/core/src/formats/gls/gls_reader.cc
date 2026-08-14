// SPDX-License-Identifier: GPL-3.0-or-later

#include "gls_reader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>

#include <zlib.h>

#include "../../foundation/text_encoding.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::gls {
namespace {

constexpr std::size_t kMaximumStoredFileSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumDecodedSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumArticleSize = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumHeadwordSize = 16U * 1024U;
constexpr std::size_t kMaximumRecords = 10U * 1000U * 1000U;

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::string ReadFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect GLS file");
    }
    if (size > kMaximumStoredFileSize) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "GLS file exceeds the supported size limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Throw(ErrorCode::kMissingFile, path, "Cannot open GLS file");
    }
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete GLS file");
    }
    return data;
}

bool HasCompressedSuffix(const std::filesystem::path& path) {
    std::string filename = path.filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return filename.size() >= 7U &&
           filename.compare(filename.size() - 7U, 7U, ".gls.dz") == 0;
}

std::string Gunzip(std::string_view compressed,
                   const std::filesystem::path& path) {
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<char*>(compressed.data()));  // zlib's API predates const.
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot initialize GLS gzip decompression");
    }
    std::string output;
    std::array<char, 64U * 1024U> buffer{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = buffer.size() - stream.avail_out;
        if (produced > kMaximumDecodedSize - output.size()) {
            inflateEnd(&stream);
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Decompressed GLS exceeds the supported size limit");
        }
        output.append(buffer.data(), produced);
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.avail_in != 0U) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid gzip-compressed GLS file");
    }
    return output;
}

std::string Trim(std::string value) {
    const auto space = [](unsigned char character) {
        return std::isspace(character) != 0;
    };
    value.erase(value.begin(),
                std::find_if_not(value.begin(), value.end(), space));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), space).base(),
                value.end());
    return value;
}

std::string LanguageCode(std::string value) {
    value = Trim(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    const auto separator = value.find('-');
    if (separator != std::string::npos) {
        value.resize(separator);
    }
    return value;
}

void NormalizeLineEndings(std::string* text) {
    std::string normalized;
    normalized.reserve(text->size());
    for (std::size_t index = 0; index < text->size(); ++index) {
        if ((*text)[index] == '\r') {
            normalized.push_back('\n');
            if (index + 1U < text->size() && (*text)[index + 1U] == '\n') {
                ++index;
            }
        } else {
            normalized.push_back((*text)[index]);
        }
    }
    *text = std::move(normalized);
}

std::string Decode(std::string data, const std::filesystem::path& path) {
    std::string_view encoding = "UTF-8";
    std::size_t bom = 0U;
    if (data.size() >= 2U && static_cast<unsigned char>(data[0]) == 0xffU &&
        static_cast<unsigned char>(data[1]) == 0xfeU) {
        encoding = "UTF-16LE";
        bom = 2U;
    } else if (data.size() >= 2U &&
               static_cast<unsigned char>(data[0]) == 0xfeU &&
               static_cast<unsigned char>(data[1]) == 0xffU) {
        encoding = "UTF-16BE";
        bom = 2U;
    } else if (data.size() >= 3U &&
               static_cast<unsigned char>(data[0]) == 0xefU &&
               static_cast<unsigned char>(data[1]) == 0xbbU &&
               static_cast<unsigned char>(data[2]) == 0xbfU) {
        bom = 3U;
    }
    try {
        std::string decoded = foundation::DecodeToUtf8(
            std::string_view(data).substr(bom), encoding, kMaximumDecodedSize);
        NormalizeLineEndings(&decoded);
        return decoded;
    } catch (const foundation::TextEncodingError& error) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot decode GLS text: " + std::string(error.what()));
    }
}

bool HasPrefix(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() &&
           text.compare(0, prefix.size(), prefix) == 0;
}

std::size_t Utf8CodePointCount(std::string_view text) noexcept {
    return static_cast<std::size_t>(
        std::count_if(text.begin(), text.end(), [](char byte) {
            return (static_cast<unsigned char>(byte) & 0xc0U) != 0x80U;
        }));
}

std::optional<std::string_view> NextLine(std::string_view text,
                                         std::size_t* position) {
    if (*position > text.size()) {
        return std::nullopt;
    }
    const auto end = text.find('\n', *position);
    const auto line = text.substr(*position, end == std::string_view::npos
                                                 ? text.size() - *position
                                                 : end - *position);
    *position = end == std::string_view::npos ? text.size() + 1U : end + 1U;
    return line;
}

std::vector<std::string> SplitHeadwords(std::string_view line,
                                        const std::filesystem::path& path) {
    std::vector<std::string> result;
    std::size_t start = 0U;
    while (start <= line.size()) {
        const auto end = line.find('|', start);
        std::string headword = Trim(std::string(line.substr(
            start, end == std::string_view::npos ? line.size() - start
                                                 : end - start)));
        if (headword.empty() || headword.size() > kMaximumHeadwordSize) {
            Throw(ErrorCode::kInvalidDictionary, path,
                  "GLS article contains an invalid headword");
        }
        result.push_back(std::move(headword));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
    }
    return result;
}

}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message)),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const std::filesystem::path& dictionary_path) {
    Reader reader;
    reader.dictionary_path_ = dictionary_path;
    std::string data = ReadFile(dictionary_path);
    if (HasCompressedSuffix(dictionary_path)) {
        data = Gunzip(data, dictionary_path);
    }
    const std::string text = Decode(std::move(data), dictionary_path);

    constexpr std::string_view kTitle = "### Glossary title:";
    constexpr std::string_view kSource = "### Source language:";
    constexpr std::string_view kTarget = "### Target language:";
    constexpr std::string_view kAuthor = "### Author:";
    constexpr std::string_view kDescription = "### Description:";
    constexpr std::string_view kSection = "### Glossary section:";
    std::size_t position = 0U;
    bool found_section = false;
    while (const auto current_line = NextLine(text, &position)) {
        const auto current = *current_line;
        if (HasPrefix(current, kTitle)) {
            reader.metadata_.name =
                Trim(std::string(current.substr(kTitle.size())));
        } else if (HasPrefix(current, kSource)) {
            reader.metadata_.source_language =
                LanguageCode(std::string(current.substr(kSource.size())));
        } else if (HasPrefix(current, kTarget)) {
            reader.metadata_.target_language =
                LanguageCode(std::string(current.substr(kTarget.size())));
        } else if (HasPrefix(current, kAuthor)) {
            const std::string author =
                Trim(std::string(current.substr(kAuthor.size())));
            if (!author.empty())
                reader.metadata_.description = "Author: " + author;
        } else if (HasPrefix(current, kDescription)) {
            const std::string description =
                Trim(std::string(current.substr(kDescription.size())));
            if (!description.empty()) {
                if (!reader.metadata_.description.empty())
                    reader.metadata_.description += "\n\n";
                reader.metadata_.description += description;
            }
        } else if (HasPrefix(current, kSection)) {
            found_section = true;
            break;
        }
    }
    if (!found_section) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "GLS glossary section header is missing");
    }

    std::size_t total_article_bytes = 0U;
    while (true) {
        auto line = NextLine(text, &position);
        while (line.has_value() && line->empty()) {
            line = NextLine(text, &position);
        }
        if (!line.has_value()) {
            break;
        }
        auto headwords = SplitHeadwords(*line, dictionary_path);
        std::string article;
        line = NextLine(text, &position);
        while (line.has_value() && !line->empty()) {
            if (!article.empty()) {
                article.push_back('\n');
            }
            if (line->size() > kMaximumArticleSize - article.size()) {
                Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                      "GLS article exceeds the supported size limit");
            }
            article.append(*line);
            line = NextLine(text, &position);
        }
        if (article.size() > kMaximumDecodedSize - total_article_bytes) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "GLS articles exceed the supported size limit");
        }
        const std::size_t article_index = reader.articles_.size();
        total_article_bytes += article.size();
        reader.articles_.push_back(std::move(article));
        for (auto& headword : headwords) {
            if (reader.records_.size() == kMaximumRecords) {
                Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                      "GLS contains too many headwords");
            }
            try {
                reader.records_.push_back({headword,
                                           foundation::FoldForLookup(headword),
                                           article_index});
            } catch (const foundation::TextFoldingError& error) {
                Throw(
                    ErrorCode::kInvalidDictionary, dictionary_path,
                    "Invalid UTF-8 GLS headword: " + std::string(error.what()));
            }
        }
    }
    if (reader.records_.empty()) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "GLS contains no articles");
    }
    if (reader.metadata_.name.empty()) {
        const std::string filename = dictionary_path.filename().string();
        const auto suffix = HasCompressedSuffix(dictionary_path) ? 7U : 4U;
        reader.metadata_.name = filename.substr(0, filename.size() - suffix);
    }
    return reader;
}

std::vector<Article> Reader::LookupExact(
    std::string_view headword, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    if (result_limit == 0U) {
        return result;
    }
    const std::string folded = foundation::FoldForLookup(headword);
    std::set<std::size_t> seen;
    std::size_t number = 0U;
    for (const auto& record : records_) {
        if (checkpoint && (number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (record.folded_headword == folded &&
            seen.insert(record.article).second) {
            result.push_back({record.headword, articles_[record.article]});
            if (result.size() == result_limit) {
                break;
            }
        }
    }
    return result;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    if (result_limit == 0U) {
        return result;
    }
    const auto matches = RankedPrefixMatches(prefix, checkpoint);
    std::set<std::size_t> seen;
    for (const auto* record : matches) {
        if (seen.insert(record->article).second) {
            result.push_back({record->headword, articles_[record->article]});
            if (result.size() == result_limit) {
                break;
            }
        }
    }
    return result;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> result;
    if (result_limit == 0U) {
        return result;
    }
    const auto matches = RankedPrefixMatches(prefix, checkpoint);
    std::unordered_set<std::string> seen;
    for (const auto* record : matches) {
        if (seen.insert(record->headword).second) {
            result.push_back(record->headword);
            if (result.size() == result_limit) {
                break;
            }
        }
    }
    return result;
}

std::vector<const Reader::Record*> Reader::RankedPrefixMatches(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(prefix);
    if (folded.empty()) {
        return {};
    }
    std::vector<const Record*> matches;
    std::size_t number = 0U;
    for (const auto& record : records_) {
        if (checkpoint && (number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (HasPrefix(record.folded_headword, folded)) {
            matches.push_back(&record);
        }
    }
    std::stable_sort(
        matches.begin(), matches.end(),
        [&folded](const Record* left, const Record* right) {
            const bool left_exact = left->folded_headword == folded;
            const bool right_exact = right->folded_headword == folded;
            if (left_exact != right_exact) {
                return left_exact;
            }
            const auto left_length = Utf8CodePointCount(left->folded_headword);
            const auto right_length =
                Utf8CodePointCount(right->folded_headword);
            if (left_length != right_length) {
                return left_length < right_length;
            }
            if (left->folded_headword != right->folded_headword) {
                return left->folded_headword < right->folded_headword;
            }
            return left->headword < right->headword;
        });
    return matches;
}

}  // namespace goldendict::core::formats::gls
