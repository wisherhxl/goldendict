// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsl_reader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <locale>
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>

#include <zlib.h>

#include "../../foundation/text_encoding.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::dsl {
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
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect DSL file");
    }
    if (size > kMaximumStoredFileSize) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "DSL file exceeds the supported size limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Throw(ErrorCode::kMissingFile, path, "Cannot open DSL file");
    }
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete DSL file");
    }
    return data;
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool HasCompressedSuffix(const std::filesystem::path& path) {
    const std::string filename = Lower(path.filename().string());
    return filename.size() >= 7U &&
           filename.compare(filename.size() - 7U, 7U, ".dsl.dz") == 0;
}

std::string Gunzip(std::string_view compressed,
                   const std::filesystem::path& path) {
    z_stream stream{};
    stream.next_in =
        reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot initialize DSL gzip decompression");
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
                  "Decompressed DSL exceeds the supported size limit");
        }
        output.append(buffer.data(), produced);
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.avail_in != 0U) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid gzip-compressed DSL file");
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

std::string DeclaredEncoding(std::string_view data) {
    const std::string lower = Lower(std::string(
        data.substr(0, std::min<std::size_t>(data.size(), 64U * 1024U))));
    const auto directive = lower.find("#source_code_page");
    if (directive == std::string::npos) {
        return "UTF-8";
    }
    const auto quote = lower.find('"', directive);
    const auto end = quote == std::string::npos ? std::string::npos
                                                : lower.find('"', quote + 1U);
    if (quote == std::string::npos || end == std::string::npos) {
        return "UTF-8";
    }
    const std::string value = lower.substr(quote + 1U, end - quote - 1U);
    if (value == "cyrillic" || value == "windows-1251" || value == "1251") {
        return "windows-1251";
    }
    if (value == "easterneuropean" || value == "windows-1250" ||
        value == "1250") {
        return "windows-1250";
    }
    if (value == "latin" || value == "windows-1252" || value == "1252") {
        return "windows-1252";
    }
    if (value == "utf-8" || value == "utf8") {
        return "UTF-8";
    }
    return value;
}

std::string Decode(std::string data, const std::filesystem::path& path) {
    std::string encoding;
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
        encoding = "UTF-8";
        bom = 3U;
    } else {
        encoding = DeclaredEncoding(data);
    }
    try {
        std::string decoded = foundation::DecodeToUtf8(
            std::string_view(data).substr(bom), encoding, kMaximumDecodedSize);
        NormalizeLineEndings(&decoded);
        return decoded;
    } catch (const foundation::TextEncodingError& error) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot decode DSL text: " + std::string(error.what()));
    }
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

std::optional<std::string> DirectiveValue(std::string_view line,
                                          std::string_view name) {
    if (line.size() < name.size() ||
        Lower(std::string(line.substr(0, name.size()))) !=
            Lower(std::string(name))) {
        return std::nullopt;
    }
    const auto first = line.find('"', name.size());
    const auto last = first == std::string_view::npos
                          ? std::string_view::npos
                          : line.find('"', first + 1U);
    if (first == std::string_view::npos || last == std::string_view::npos) {
        return std::nullopt;
    }
    return std::string(line.substr(first + 1U, last - first - 1U));
}

std::string LanguageCode(std::string value) {
    value = Lower(Trim(std::move(value)));
    const std::array<std::pair<std::string_view, std::string_view>, 10> names{
        {{"english", "en"},
         {"german", "de"},
         {"russian", "ru"},
         {"french", "fr"},
         {"spanish", "es"},
         {"italian", "it"},
         {"polish", "pl"},
         {"ukrainian", "uk"},
         {"chinese", "zh"},
         {"japanese", "ja"}}};
    for (const auto& [name, code] : names) {
        if (value == name) {
            return std::string(code);
        }
    }
    return value;
}

std::string Unescape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\\' && index + 1U < value.size()) {
            ++index;
        }
        result.push_back(value[index]);
    }
    return Trim(std::move(result));
}

void ExpandOptional(std::string value, std::vector<std::string>* result,
                    std::size_t start = 0U) {
    if (result->size() >= 32U) {
        return;
    }
    for (std::size_t index = start; index < value.size(); ++index) {
        if (value[index] == '\\') {
            ++index;
            continue;
        }
        if (value[index] != '(') {
            continue;
        }
        std::size_t depth = 1U;
        for (std::size_t end = index + 1U; end < value.size(); ++end) {
            if (value[end] == '\\') {
                ++end;
            } else if (value[end] == '(') {
                ++depth;
            } else if (value[end] == ')' && --depth == 0U) {
                std::string removed =
                    value.substr(0, index) + value.substr(end + 1U);
                ExpandOptional(std::move(removed), result, index);
                value.erase(end, 1U);
                value.erase(index, 1U);
                ExpandOptional(std::move(value), result, index);
                return;
            }
        }
        value.resize(index);
        break;
    }
    const std::string expanded = Unescape(value);
    if (!expanded.empty() && expanded.size() <= kMaximumHeadwordSize &&
        std::find(result->begin(), result->end(), expanded) == result->end()) {
        result->push_back(expanded);
    }
}

std::string ReplaceTildes(std::string value, std::string_view primary) {
    std::string result;
    result.reserve(value.size() + primary.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\\' && index + 1U < value.size()) {
            result.push_back(value[++index]);
        } else if (value[index] == '~') {
            result.append(primary);
        } else {
            result.push_back(value[index]);
        }
    }
    return result;
}

std::string Escape(std::string_view value) {
    std::string escaped;
    for (const char character : value) {
        switch (character) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            default:
                escaped.push_back(character);
                break;
        }
    }
    return escaped;
}

std::string RenderDsl(std::string_view input, std::string_view primary) {
    std::string html;
    const std::string expanded = ReplaceTildes(std::string(input), primary);
    for (std::size_t index = 0; index < expanded.size();) {
        if (expanded.compare(index, 2U, "{{") == 0) {
            const auto end = expanded.find("}}", index + 2U);
            index = end == std::string::npos ? expanded.size() : end + 2U;
            continue;
        }
        if (expanded.compare(index, 2U, "<<") == 0) {
            const auto end = expanded.find(">>", index + 2U);
            if (end != std::string::npos) {
                const std::string target =
                    expanded.substr(index + 2U, end - index - 2U);
                html += "<a href=\"bword://" + Escape(target) + "\">" +
                        Escape(target) + "</a>";
                index = end + 2U;
                continue;
            }
        }
        if (expanded.compare(index, 3U, "[s]") == 0) {
            const auto end = expanded.find("[/s]", index + 3U);
            if (end != std::string::npos) {
                const std::string resource =
                    expanded.substr(index + 3U, end - index - 3U);
                html += "<img src=\"" + Escape(resource) + "\">";
                index = end + 4U;
                continue;
            }
        }
        if (expanded[index] == '[') {
            const auto end = expanded.find(']', index + 1U);
            if (end != std::string::npos) {
                const std::string tag =
                    Lower(expanded.substr(index + 1U, end - index - 1U));
                if (tag == "b" || tag == "i" || tag == "u" || tag == "sub" ||
                    tag == "sup" || tag == "/b" || tag == "/i" || tag == "/u" ||
                    tag == "/sub" || tag == "/sup") {
                    html += '<' + tag + '>';
                } else if (!tag.empty() && tag[0] == 'm') {
                    html += "<div>";
                } else if (tag == "/m" ||
                           (tag.size() > 1U && tag.substr(0, 2U) == "/m")) {
                    html += "</div>";
                } else if (tag == "c" || tag.rfind("c ", 0) == 0U) {
                    html += "<span>";
                } else if (tag == "/c") {
                    html += "</span>";
                } else if (tag == "br") {
                    html += "<br>";
                } else if (tag == "*") {
                    html += "<gd-optional>";
                } else if (tag == "/*") {
                    html += "</gd-optional>";
                }
                index = end + 1U;
                continue;
            }
        }
        if (expanded[index] == '\n') {
            html += "<br>";
        } else {
            html += Escape(std::string_view(expanded).substr(index, 1U));
        }
        ++index;
        if (html.size() > kMaximumArticleSize) {
            break;
        }
    }
    return html;
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

}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message)),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const std::filesystem::path& dictionary_path,
                    std::string_view preferred_language) {
    Reader reader;
    reader.dictionary_path_ = dictionary_path;
    std::string data = ReadFile(dictionary_path);
    if (HasCompressedSuffix(dictionary_path)) {
        data = Gunzip(data, dictionary_path);
    }
    const std::string text = Decode(std::move(data), dictionary_path);

    auto annotation_path = dictionary_path;
    std::string annotation_name = annotation_path.filename().string();
    const std::string lower_annotation_name = Lower(annotation_name);
    const std::size_t annotation_suffix =
        lower_annotation_name.size() >= 7U &&
                lower_annotation_name.compare(lower_annotation_name.size() - 7U,
                                              7U, ".dsl.dz") == 0
            ? 7U
        : lower_annotation_name.size() >= 4U &&
                lower_annotation_name.compare(lower_annotation_name.size() - 4U,
                                              4U, ".dsl") == 0
            ? 4U
            : 0U;
    if (annotation_suffix != 0U) {
        annotation_name.resize(annotation_name.size() - annotation_suffix);
        annotation_path.replace_filename(annotation_name + ".ann");
        std::error_code annotation_error;
        const auto annotation_size =
            std::filesystem::file_size(annotation_path, annotation_error);
        if (!annotation_error && annotation_size <= 1024U * 1024U) {
            try {
                std::string annotation =
                    Decode(ReadFile(annotation_path), annotation_path);
                if (annotation.rfind("#LANGUAGE ", 0U) == 0U) {
                    std::string preferred =
                        Lower(std::string(preferred_language));
                    if (preferred.empty()) {
                        try {
                            preferred = Lower(std::locale("").name());
                        } catch (const std::runtime_error&) {}
                    }
                    const auto separator = preferred.find_first_of("_.-");
                    if (separator != std::string::npos)
                        preferred.resize(separator);
                    std::size_t selected = 0U;
                    if (!preferred.empty()) {
                        const auto exact =
                            Lower(annotation)
                                .find("#language \"" + preferred + "\"");
                        if (exact != std::string::npos)
                            selected = exact;
                    }
                    const auto first_line = annotation.find('\n', selected);
                    const auto next_section =
                        first_line == std::string::npos
                            ? std::string::npos
                            : annotation.find("\n#LANGUAGE ", first_line);
                    annotation =
                        first_line == std::string::npos
                            ? std::string{}
                            : annotation.substr(
                                  first_line + 1U,
                                  next_section == std::string::npos
                                      ? std::string::npos
                                      : next_section - first_line - 1U);
                }
                reader.metadata_.description = Trim(std::move(annotation));
            } catch (const Error&) {
                // Optional annotations never make the dictionary unavailable.
            }
        }
    }
    std::size_t position = 0U;
    std::optional<std::string_view> pending;
    while (const auto line = NextLine(text, &position)) {
        if (line->empty()) {
            continue;
        }
        if ((*line)[0] == '#') {
            if (const auto name = DirectiveValue(*line, "#NAME")) {
                reader.metadata_.name = *name;
            } else if (const auto source_language =
                           DirectiveValue(*line, "#INDEX_LANGUAGE")) {
                reader.metadata_.source_language =
                    LanguageCode(*source_language);
            } else if (const auto target_language =
                           DirectiveValue(*line, "#CONTENTS_LANGUAGE")) {
                reader.metadata_.target_language =
                    LanguageCode(*target_language);
            }
            continue;
        }
        pending = *line;
        break;
    }
    std::size_t total_article_bytes = 0U;
    while (pending.has_value()) {
        if (pending->empty() ||
            std::isspace(static_cast<unsigned char>((*pending)[0])) != 0) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "DSL article does not start with a headword");
        }
        std::vector<std::string> raw_headwords;
        while (pending.has_value() && !pending->empty() &&
               std::isspace(static_cast<unsigned char>((*pending)[0])) == 0) {
            raw_headwords.emplace_back(*pending);
            pending = NextLine(text, &position);
        }
        std::vector<std::string> primary_expansions;
        ExpandOptional(raw_headwords.front(), &primary_expansions);
        if (primary_expansions.empty()) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "DSL article contains an invalid headword");
        }
        const std::string primary = primary_expansions.front();
        std::vector<std::string> headwords;
        for (auto& raw : raw_headwords) {
            ExpandOptional(ReplaceTildes(std::move(raw), primary), &headwords);
        }
        std::string body;
        while (pending.has_value() &&
               (pending->empty() ||
                std::isspace(static_cast<unsigned char>((*pending)[0])) != 0)) {
            if (!pending->empty()) {
                const auto first = pending->find_first_not_of(" \t");
                if (!body.empty()) {
                    body.push_back('\n');
                }
                body.append(first == std::string_view::npos
                                ? std::string_view{}
                                : pending->substr(first));
            }
            pending = NextLine(text, &position);
        }
        const std::string html = RenderDsl(body, primary);
        if (html.size() > kMaximumArticleSize ||
            html.size() > kMaximumDecodedSize - total_article_bytes) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "DSL article exceeds the supported size limit");
        }
        const std::size_t article = reader.articles_.size();
        total_article_bytes += html.size();
        reader.articles_.push_back(html);
        for (auto& headword : headwords) {
            if (reader.records_.size() == kMaximumRecords) {
                Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                      "DSL contains too many headwords");
            }
            try {
                reader.records_.push_back(
                    {headword, foundation::FoldForLookup(headword), article});
            } catch (const foundation::TextFoldingError& error) {
                Throw(
                    ErrorCode::kInvalidDictionary, dictionary_path,
                    "Invalid UTF-8 DSL headword: " + std::string(error.what()));
            }
        }
    }
    if (reader.records_.empty()) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "DSL contains no articles");
    }
    if (reader.metadata_.name.empty()) {
        const std::string filename = dictionary_path.filename().string();
        const std::size_t suffix =
            HasCompressedSuffix(dictionary_path) ? 7U : 4U;
        reader.metadata_.name = filename.substr(0, filename.size() - suffix);
    }
    return reader;
}

std::pair<std::vector<std::string>, bool> Reader::EnumerateHeadwords(
    std::size_t offset, std::size_t result_limit, std::size_t byte_limit,
    const std::function<void()>& checkpoint) const {
    return enumeration_index_.Page(
        records_.size(),
        [this](std::uint32_t ordinal) -> std::string_view {
            return records_[ordinal].headword;
        },
        offset, result_limit, byte_limit, checkpoint);
}

std::vector<Article> Reader::LookupExact(
    std::string_view headword, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    if (result_limit == 0U)
        return result;
    const std::string folded = foundation::FoldForLookup(headword);
    std::set<std::size_t> seen;
    std::size_t number = 0U;
    for (const auto& record : records_) {
        if (checkpoint && (number++ % 1024U) == 0U)
            checkpoint();
        if (record.folded_headword == folded &&
            seen.insert(record.article).second) {
            result.push_back({record.headword, articles_[record.article]});
            if (result.size() == result_limit)
                break;
        }
    }
    return result;
}

std::vector<const Reader::Record*> Reader::RankedPrefixMatches(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(prefix);
    std::vector<const Record*> matches;
    std::size_t number = 0U;
    for (const auto& record : records_) {
        if (checkpoint && (number++ % 1024U) == 0U)
            checkpoint();
        if (HasPrefix(record.folded_headword, folded))
            matches.push_back(&record);
    }
    std::stable_sort(
        matches.begin(), matches.end(),
        [&folded](const Record* left, const Record* right) {
            const bool left_exact = left->folded_headword == folded;
            const bool right_exact = right->folded_headword == folded;
            if (left_exact != right_exact)
                return left_exact;
            const auto left_size = Utf8CodePointCount(left->folded_headword);
            const auto right_size = Utf8CodePointCount(right->folded_headword);
            if (left_size != right_size)
                return left_size < right_size;
            if (left->folded_headword != right->folded_headword)
                return left->folded_headword < right->folded_headword;
            return left->headword < right->headword;
        });
    return matches;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    if (result_limit == 0U)
        return result;
    std::set<std::size_t> seen;
    for (const auto* record : RankedPrefixMatches(prefix, checkpoint)) {
        if (seen.insert(record->article).second) {
            result.push_back({record->headword, articles_[record->article]});
            if (result.size() == result_limit)
                break;
        }
    }
    return result;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> result;
    if (result_limit == 0U)
        return result;
    std::unordered_set<std::string> seen;
    for (const auto* record : RankedPrefixMatches(prefix, checkpoint)) {
        if (seen.insert(record->folded_headword).second) {
            result.push_back(record->headword);
            if (result.size() == result_limit)
                break;
        }
    }
    return result;
}

}  // namespace goldendict::core::formats::dsl
