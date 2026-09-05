// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsl_reader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <locale>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <zlib.h>

#include "../../foundation/text_encoding.h"
#include "../../foundation/text_folding.h"
#include "dsl_abbreviation.h"
#include "dsl_headword_parser.h"

namespace goldendict::core::formats::dsl {
namespace {

constexpr std::size_t kMaximumArticleSize = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumHeadwordSize = 16U * 1024U;
constexpr std::size_t kMaximumRecords = 10U * 1000U * 1000U;
constexpr std::size_t kMaximumDslNesting = 64U;

using AbbreviationMap = std::unordered_map<std::string, std::string>;

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
    if (size > kMaximumDictionaryBytes) {
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
        if (produced > kMaximumDictionaryBytes - output.size()) {
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
        std::string decoded =
            foundation::DecodeToUtf8(std::string_view(data).substr(bom),
                                     encoding, kMaximumDictionaryBytes);
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

struct DslToken {
    std::size_t begin = 0U;
    std::size_t end = 0U;
    std::string_view text;
    bool escaped = false;
};

std::optional<DslToken> NextDslToken(std::string_view value,
                                     std::size_t index) {
    if (index >= value.size()) {
        return std::nullopt;
    }
    const std::size_t begin = index;
    if (value[index] == '\\') {
        if (++index == value.size()) {
            return std::nullopt;
        }
    } else if ((value[index] == '[' || value[index] == ']') &&
               index + 1U < value.size() && value[index + 1U] == value[index]) {
        return DslToken{begin, index + 2U, value.substr(index, 1U), true};
    }

    std::size_t end = index + 1U;
    while (end < value.size() &&
           (static_cast<unsigned char>(value[end]) & 0xc0U) == 0x80U) {
        ++end;
    }
    return DslToken{begin, end, value.substr(index, end - index),
                    begin != index};
}

struct DslSequence {
    std::size_t begin = 0U;
    std::size_t end = 0U;
};

std::optional<DslSequence> FindUnescapedSequence(std::string_view value,
                                                 std::size_t start,
                                                 std::string_view sequence) {
    for (std::size_t index = start; index < value.size();) {
        const auto first = NextDslToken(value, index);
        if (!first.has_value()) {
            return std::nullopt;
        }
        if (!first->escaped && first->text.size() == 1U &&
            first->text.front() == sequence.front()) {
            std::size_t end = first->end;
            bool matches = true;
            for (std::size_t character = 1U; character < sequence.size();
                 ++character) {
                const auto next = NextDslToken(value, end);
                if (!next.has_value() || next->escaped ||
                    next->text.size() != 1U ||
                    next->text.front() != sequence[character]) {
                    matches = false;
                    break;
                }
                end = next->end;
            }
            if (matches) {
                return DslSequence{first->begin, end};
            }
        }
        index = first->end;
    }
    return std::nullopt;
}

std::string DecodeDslLiteral(std::string_view value,
                             bool preserve_escaped_spacing) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0U; index < value.size();) {
        const auto token = NextDslToken(value, index);
        if (!token.has_value()) {
            break;
        }
        if (preserve_escaped_spacing && token->escaped && token->text == " ") {
            decoded += "\xc2\xa0";
        } else {
            decoded += token->text;
        }
        index = token->end;
    }
    return decoded;
}

std::string PlainDslText(std::string_view input, std::string_view primary) {
    std::string plain;
    const std::string expanded =
        headword::ReplaceTildes(std::string(input), primary);
    for (std::size_t index = 0U; index < expanded.size();) {
        const auto token = NextDslToken(expanded, index);
        if (!token.has_value())
            break;
        if (token->escaped) {
            plain += DecodeDslLiteral(
                std::string_view(expanded).substr(index, token->end - index),
                true);
            index = token->end;
            continue;
        }
        if (expanded.compare(index, 2U, "{{") == 0) {
            const auto end = FindUnescapedSequence(expanded, index + 2U, "}}");
            index = end.has_value() ? end->end : expanded.size();
            continue;
        }
        if (expanded.compare(index, 2U, "<<") == 0) {
            const auto end = FindUnescapedSequence(expanded, index + 2U, ">>");
            if (!end.has_value())
                break;
            plain += DecodeDslLiteral(std::string_view(expanded).substr(
                                          index + 2U, end->begin - index - 2U),
                                      true);
            index = end->end;
            continue;
        }
        if (expanded[index] == '[') {
            const auto end = FindUnescapedSequence(expanded, index + 1U, "]");
            if (!end.has_value())
                break;
            index = end->end;
            continue;
        }
        plain += token->text;
        index = token->end;
    }
    return plain;
}

std::size_t Utf8CodePointCount(std::string_view text) noexcept;

std::string TooltipTitle(std::string value) {
    if (Utf8CodePointCount(value) >= 70U)
        return value;
    std::string title;
    title.reserve(value.size());
    for (const char character : value) {
        if (character == ' ' || character == '\t') {
            title += "\xc2\xa0";
        } else if (character == '-') {
            title += "\xe2\x80\x91";
        } else {
            title.push_back(character);
        }
    }
    return title;
}

std::string RenderDsl(std::string_view input, std::string_view primary,
                      const AbbreviationMap& abbreviations,
                      std::size_t nesting = 0U) {
    std::string html;
    const std::string expanded =
        headword::ReplaceTildes(std::string(input), primary);
    for (std::size_t index = 0; index < expanded.size();) {
        const auto token = NextDslToken(expanded, index);
        if (!token.has_value()) {
            break;
        }
        if (token->escaped) {
            html += Escape(DecodeDslLiteral(
                std::string_view(expanded).substr(index, token->end - index),
                true));
            index = token->end;
            continue;
        }
        if (expanded.compare(index, 2U, "{{") == 0) {
            const auto end = FindUnescapedSequence(expanded, index + 2U, "}}");
            index = end.has_value() ? end->end : expanded.size();
            continue;
        }
        if (expanded.compare(index, 2U, "<<") == 0) {
            const auto end = FindUnescapedSequence(expanded, index + 2U, ">>");
            if (end.has_value()) {
                const auto body = std::string_view(expanded).substr(
                    index + 2U, end->begin - index - 2U);
                const std::string target = DecodeDslLiteral(body, false);
                const std::string label = DecodeDslLiteral(body, true);
                html += "<a href=\"bword://" + Escape(target) + "\">" +
                        Escape(label) + "</a>";
                index = end->end;
                continue;
            }
            break;
        }
        if (expanded.compare(index, 3U, "[s]") == 0) {
            const auto end =
                FindUnescapedSequence(expanded, index + 3U, "[/s]");
            if (end.has_value()) {
                const std::string resource =
                    DecodeDslLiteral(std::string_view(expanded).substr(
                                         index + 3U, end->begin - index - 3U),
                                     false);
                html += "<img src=\"" + Escape(resource) + "\">";
                index = end->end;
                continue;
            }
        }
        if (expanded[index] == '[') {
            const auto end = FindUnescapedSequence(expanded, index + 1U, "]");
            if (end.has_value()) {
                const std::string tag = Lower(
                    DecodeDslLiteral(std::string_view(expanded).substr(
                                         index + 1U, end->begin - index - 1U),
                                     false));
                if (tag == "p" && nesting < kMaximumDslNesting) {
                    const auto close =
                        FindUnescapedSequence(expanded, end->end, "[/p]");
                    if (close.has_value()) {
                        const auto body = std::string_view(expanded).substr(
                            end->end, close->begin - end->end);
                        const std::string key = PlainDslText(body, primary);
                        html += "<span class=\"dsl_p\"";
                        const auto abbreviation = abbreviations.find(key);
                        if (abbreviation != abbreviations.end()) {
                            html += " title=\"" +
                                    Escape(TooltipTitle(abbreviation->second)) +
                                    "\"";
                        }
                        html += ">" +
                                RenderDsl(body, primary, abbreviations,
                                          nesting + 1U) +
                                "</span>";
                        index = close->end;
                        continue;
                    }
                } else if (tag == "b" || tag == "i" || tag == "u" ||
                           tag == "sub" || tag == "sup" || tag == "/b" ||
                           tag == "/i" || tag == "/u" || tag == "/sub" ||
                           tag == "/sup") {
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
                index = end->end;
                continue;
            }
            break;
        }
        if (token->text == "\n") {
            html += "<br>";
        } else {
            html += Escape(token->text);
        }
        index = token->end;
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

std::string RemoveDslComments(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0U; index < text.size();) {
        const auto token = NextDslToken(text, index);
        if (!token.has_value())
            break;
        if (!token->escaped && text.compare(index, 2U, "{{") == 0) {
            const auto end = FindUnescapedSequence(text, index + 2U, "}}");
            const std::size_t comment_end =
                end.has_value() ? end->end : text.size();
            index = comment_end;
            continue;
        }
        result.append(text.substr(token->begin, token->end - token->begin));
        index = token->end;
    }
    return result;
}

AbbreviationMap ParseAbbreviations(std::string_view text) {
    const std::string without_comments = RemoveDslComments(text);
    text = without_comments;
    AbbreviationMap abbreviations;
    std::size_t retained_bytes = 0U;
    std::size_t position = 0U;
    std::optional<std::string_view> pending = NextLine(text, &position);
    while (pending.has_value()) {
        if (pending->empty() || (*pending)[0] == '#' ||
            std::isspace(static_cast<unsigned char>((*pending)[0])) != 0) {
            pending = NextLine(text, &position);
            continue;
        }

        std::vector<std::string> raw_headwords;
        while (pending.has_value() && !pending->empty() &&
               (*pending)[0] != '#' &&
               std::isspace(static_cast<unsigned char>((*pending)[0])) == 0) {
            raw_headwords.emplace_back(*pending);
            pending = NextLine(text, &position);
        }
        const headword::Expansion expansion = headword::Parse(raw_headwords);
        if (!pending.has_value() || pending->empty())
            break;
        if (std::isspace(static_cast<unsigned char>((*pending)[0])) == 0)
            continue;

        const auto first = pending->find_first_not_of(" \t");
        const std::string value = PlainDslText(first == std::string_view::npos
                                                   ? std::string_view{}
                                                   : pending->substr(first),
                                               expansion.merged_first);
        for (const auto& key : expansion.records) {
            if (key.empty() || key.size() > kMaximumHeadwordSize)
                continue;
            const auto existing = abbreviations.find(key);
            const std::size_t replaced_bytes =
                existing == abbreviations.end()
                    ? 0U
                    : existing->first.size() + existing->second.size();
            const std::size_t added_bytes = key.size() + value.size();
            if (added_bytes >
                kMaximumDictionaryBytes - (retained_bytes - replaced_bytes)) {
                throw std::length_error("DSL abbreviation table is too large");
            }
            retained_bytes = retained_bytes - replaced_bytes + added_bytes;
            abbreviations[key] = value;
        }
        if (abbreviations.size() > kMaximumRecords)
            throw std::length_error("DSL abbreviation table is too large");
        pending = NextLine(text, &position);
    }
    return abbreviations;
}

AbbreviationMap LoadAbbreviations(
    const std::filesystem::path& abbreviation_path) {
    std::string data = ReadFile(abbreviation_path);
    if (HasCompressedSuffix(abbreviation_path))
        data = Gunzip(data, abbreviation_path);
    return ParseAbbreviations(Decode(std::move(data), abbreviation_path));
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
    AbbreviationMap abbreviations;
    reader.abbreviation_path_ = FindAdjacentAbbreviation(dictionary_path);
    if (reader.abbreviation_path_.has_value()) {
        try {
            abbreviations = LoadAbbreviations(*reader.abbreviation_path_);
        } catch (const std::exception&) {
            // Qt 5 keeps the primary dictionary available when an optional
            // abbreviation companion is malformed or unreadable.
        }
    }

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
        headword::Expansion expansion = headword::Parse(raw_headwords);
        if (expansion.records.empty()) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "DSL article contains an invalid headword");
        }
        if (expansion.records.size() >
            kMaximumRecords - reader.headword_count_) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "DSL contains too many headwords");
        }
        reader.headword_count_ += expansion.records.size();
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
        const std::string html =
            RenderDsl(body, expansion.article_tilde, abbreviations);
        if (html.size() > kMaximumArticleSize ||
            html.size() > kMaximumDictionaryBytes - total_article_bytes) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "DSL article exceeds the supported size limit");
        }
        const std::size_t article = reader.articles_.size();
        total_article_bytes += html.size();
        reader.articles_.push_back(html);
        const std::size_t first_record_ordinal = reader.records_.size();
        for (auto& headword : expansion.records) {
            if (headword.empty() || headword.size() > kMaximumHeadwordSize) {
                continue;
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
        if (reader.records_.size() != first_record_ordinal) {
            reader.full_text_sources_.push_back(
                {first_record_ordinal,
                 expansion.primary.empty()
                     ? reader.records_[first_record_ordinal].headword
                     : std::move(expansion.primary),
                 article});
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
    try {
        reader.source_snapshot_ =
            dictionary::CaptureSourceSnapshot({dictionary_path});
    } catch (const dictionary::GeneratedIndexError& error) {
        Throw(ErrorCode::kMissingFile, dictionary_path, error.what());
    }
    return reader;
}

std::vector<FullTextArticle> Reader::ReadFullTextArticles(
    const std::function<void()>& checkpoint) const {
    std::vector<FullTextArticle> result;
    result.reserve(full_text_sources_.size());
    for (const auto& source : full_text_sources_) {
        if (checkpoint) {
            checkpoint();
        }
        result.push_back({source.first_record_ordinal,
                          source.canonical_headword, source.article,
                          articles_[source.article]});
    }
    return result;
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
