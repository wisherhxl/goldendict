// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_reader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include <unicode/ucasemap.h>
#include <unicode/utf8.h>
#include <zlib.h>

#include "../../dictionary/generated_index.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::stardict {

namespace {

constexpr std::string_view kInfoSignature = "StarDict's dict ifo file";
constexpr std::uintmax_t kMaximumIndexSize = 256U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumDictionarySize =
    static_cast<std::uintmax_t>(2U) * 1024U * 1024U * 1024U;
constexpr std::string_view kGeneratedIndexFormat = "stardict-records-v4";

using LegacyLanguageCode = std::pair<std::string_view, std::string_view>;
constexpr LegacyLanguageCode kLegacyLanguageCodes[] = {
#include "legacy_language_codes.inc"
};

struct LegacyHtmlEntity {
    std::string_view name;
    std::uint32_t code_point;
};

constexpr LegacyHtmlEntity kLegacyHtmlEntities[] = {
#include "legacy_html_entities.inc"
};
static_assert(std::size(kLegacyHtmlEntities) == 258U);

constexpr std::array<std::uint32_t, 32U> kWindowsLatin1ExtendedCharacters = {
    0x20acU, 0x0081U, 0x201aU, 0x0192U, 0x201eU, 0x2026U, 0x2020U, 0x2021U,
    0x02c6U, 0x2030U, 0x0160U, 0x2039U, 0x0152U, 0x008dU, 0x017dU, 0x008fU,
    0x0090U, 0x2018U, 0x2019U, 0x201cU, 0x201dU, 0x2022U, 0x2013U, 0x2014U,
    0x02dcU, 0x2122U, 0x0161U, 0x203aU, 0x0153U, 0x009dU, 0x017eU, 0x0178U};

constexpr std::array<UChar32, 25U> kQt5StringWhitespace = {
    0x9U,    0xaU,    0xbU,    0xcU,    0xdU,    0x20U,   0x85U,
    0xa0U,   0x1680U, 0x2000U, 0x2001U, 0x2002U, 0x2003U, 0x2004U,
    0x2005U, 0x2006U, 0x2007U, 0x2008U, 0x2009U, 0x200aU, 0x2028U,
    0x2029U, 0x202fU, 0x205fU, 0x3000U};

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

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::string ReadFile(const std::filesystem::path& path, ErrorCode error_code,
                     std::uintmax_t maximum_size) {
    std::error_code filesystem_error;
    const auto size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        Throw(ErrorCode::kMissingFile, path, "Cannot read required file");
    }
    if (size > maximum_size ||
        size > static_cast<std::uintmax_t>(
                   std::numeric_limits<std::streamsize>::max())) {
        Throw(error_code, path, "File exceeds the supported size limit");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Throw(ErrorCode::kMissingFile, path, "Cannot open required file");
    }

    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        Throw(error_code, path, "Cannot read complete file");
    }
    return data;
}

bool IsCompressedCompanion(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension == ".gz" || extension == ".dz";
}

std::optional<std::filesystem::path> ResolveOptionalCompanion(
    const std::filesystem::path& base_path,
    std::initializer_list<std::string_view> suffixes) {
    for (const auto suffix : suffixes) {
        auto candidate = base_path;
        candidate += suffix;
        std::error_code filesystem_error;
        if (std::filesystem::exists(candidate, filesystem_error)) {
            return candidate;
        }
        if (filesystem_error) {
            Throw(ErrorCode::kMissingFile, candidate,
                  "Cannot inspect companion file");
        }
    }
    return std::nullopt;
}

std::filesystem::path ResolveRequiredCompanion(
    const std::filesystem::path& base_path,
    std::initializer_list<std::string_view> suffixes,
    std::string_view description) {
    if (const auto path = ResolveOptionalCompanion(base_path, suffixes);
        path.has_value()) {
        return *path;
    }
    auto expected_path = base_path;
    expected_path += *suffixes.begin();
    Throw(ErrorCode::kMissingFile, expected_path,
          "Cannot find required " + std::string(description));
}

gzFile OpenCompressedFile(const std::filesystem::path& path) {
#ifdef _WIN32
    return gzopen_w(path.c_str(), "rb");
#else
    return gzopen(path.c_str(), "rb");
#endif
}

std::string ReadCompressedFile(const std::filesystem::path& path,
                               ErrorCode error_code,
                               std::uintmax_t maximum_size,
                               std::string_view description) {
    std::error_code filesystem_error;
    const auto compressed_size =
        std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        Throw(ErrorCode::kMissingFile, path,
              "Cannot inspect compressed " + std::string(description));
    }
    if (compressed_size > maximum_size) {
        Throw(error_code, path,
              "Compressed " + std::string(description) +
                  " exceeds the supported size limit");
    }

    std::ifstream header(path, std::ios::binary);
    if (!header) {
        Throw(ErrorCode::kMissingFile, path,
              "Cannot open compressed " + std::string(description));
    }
    std::array<unsigned char, 2> signature{};
    if (!header.read(reinterpret_cast<char*>(signature.data()),
                     static_cast<std::streamsize>(signature.size()))) {
        Throw(
            error_code, path,
            "Compressed " + std::string(description) + " header is truncated");
    }
    if (signature[0] != 0x1fU || signature[1] != 0x8bU) {
        Throw(error_code, path,
              "Compressed " + std::string(description) +
                  " has an invalid gzip signature");
    }

    gzFile input = OpenCompressedFile(path);
    if (input == nullptr) {
        Throw(ErrorCode::kMissingFile, path,
              "Cannot open compressed " + std::string(description));
    }

    std::string data;
    std::array<char, 64U * 1024U> buffer{};
    while (true) {
        const int count =
            gzread(input, buffer.data(), static_cast<unsigned>(buffer.size()));
        if (count > 0) {
            const auto size = static_cast<std::size_t>(count);
            if (size > maximum_size - data.size()) {
                gzclose(input);
                Throw(error_code, path,
                      "Decompressed " + std::string(description) +
                          " exceeds the supported size limit");
            }
            data.append(buffer.data(), size);
            continue;
        }
        if (count < 0) {
            int zlib_error = Z_OK;
            const char* message = gzerror(input, &zlib_error);
            const std::string detail =
                message == nullptr ? "Unknown decompression error" : message;
            gzclose(input);
            Throw(error_code, path,
                  "Cannot decompress " + std::string(description) + ": " +
                      detail);
        }
        break;
    }
    if (gzclose(input) != Z_OK) {
        Throw(error_code, path,
              "Compressed " + std::string(description) +
                  " checksum or trailer is invalid");
    }
    return data;
}

std::string ReadCompanion(const std::filesystem::path& path,
                          ErrorCode error_code, std::uintmax_t maximum_size,
                          std::string_view description) {
    return IsCompressedCompanion(path)
               ? ReadCompressedFile(path, error_code, maximum_size, description)
               : ReadFile(path, error_code, maximum_size);
}

std::uint32_t ParseLegacyUnsigned(std::string_view text,
                                  const std::filesystem::path& path,
                                  std::string_view field_name) {
    unsigned int value = 0U;
    const std::string terminated(text);
    if (std::sscanf(terminated.c_str(), "%u", &value) != 1) {
        Throw(ErrorCode::kInvalidInfo, path,
              "Invalid unsigned value for " + std::string(field_name));
    }
    return static_cast<std::uint32_t>(value);
}

std::map<std::string, std::string> ParseInfoFields(
    const std::string& contents, const std::filesystem::path& path) {
    std::istringstream input(contents);
    std::string line;
    if (!std::getline(input, line)) {
        Throw(ErrorCode::kInvalidInfo, path, "Info file is empty");
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line != kInfoSignature) {
        Throw(ErrorCode::kInvalidInfo, path, "Invalid StarDict signature");
    }

    std::map<std::string, std::string> fields;
    if (!std::getline(input, line)) {
        Throw(ErrorCode::kInvalidInfo, path,
              "Info file does not declare a version on its second line");
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    constexpr std::string_view kVersionPrefix = "version=";
    if (!HasPrefix(line, kVersionPrefix)) {
        Throw(ErrorCode::kInvalidInfo, path,
              "Info file does not declare a version on its second line");
    }
    fields.insert_or_assign("version", line.substr(kVersionPrefix.size()));

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos || separator == 0U) {
            continue;
        }
        fields.insert_or_assign(line.substr(0, separator),
                                line.substr(separator + 1U));
    }
    return fields;
}

std::string FoldAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return character >= 'A' && character <= 'Z'
                                  ? static_cast<char>(character - 'A' + 'a')
                                  : static_cast<char>(character);
                   });
    return value;
}

std::string FoldUnicodeCase(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() >
        static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return FoldAscii(std::string(value));
    }
    UErrorCode status = U_ZERO_ERROR;
    UCaseMap* case_map = ucasemap_open(nullptr, U_FOLD_CASE_DEFAULT, &status);
    if (U_FAILURE(status) || case_map == nullptr) {
        return FoldAscii(std::string(value));
    }
    status = U_ZERO_ERROR;
    const auto source_size = static_cast<std::int32_t>(value.size());
    const auto output_size = ucasemap_utf8FoldCase(
        case_map, nullptr, 0, value.data(), source_size, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        ucasemap_close(case_map);
        return FoldAscii(std::string(value));
    }
    status = U_ZERO_ERROR;
    std::string output(static_cast<std::size_t>(output_size), '\0');
    const auto written =
        ucasemap_utf8FoldCase(case_map, output.data(), output_size,
                              value.data(), source_size, &status);
    ucasemap_close(case_map);
    if (U_FAILURE(status)) {
        return FoldAscii(std::string(value));
    }
    output.resize(static_cast<std::size_t>(written));
    return output;
}

bool IsAsciiLetter(char character) noexcept {
    return character >= 'a' && character <= 'z';
}

std::string GuessLegacyLanguage(std::string_view token) {
    if (token.size() == 3U) {
        const auto found = std::find_if(
            std::begin(kLegacyLanguageCodes), std::end(kLegacyLanguageCodes),
            [token](const LegacyLanguageCode& entry) {
                return entry.first == token;
            });
        if (found != std::end(kLegacyLanguageCodes)) {
            return std::string(found->second);
        }
    }
    // Frozen Qt 5 accepts any captured token and falls back to its first two
    // letters when the exact three-letter mapping is unknown.
    return std::string(token.substr(0U, 2U));
}

std::pair<std::string, std::string> InferLegacyLanguagePair(std::string name) {
    name = "|" + FoldUnicodeCase(name) + "|";
    for (std::size_t boundary = 0U; boundary < name.size(); ++boundary) {
        if (IsAsciiLetter(name[boundary])) {
            continue;
        }
        for (const std::size_t source_size : {2U, 3U}) {
            const auto source = boundary + 1U;
            const auto separator = source + source_size;
            if (separator >= name.size() || name[separator] != '-' ||
                !std::all_of(
                    name.begin() + static_cast<std::ptrdiff_t>(source),
                    name.begin() + static_cast<std::ptrdiff_t>(separator),
                    IsAsciiLetter)) {
                continue;
            }
            for (const std::size_t target_size : {2U, 3U}) {
                const auto target = separator + 1U;
                const auto end = target + target_size;
                if (end >= name.size() || IsAsciiLetter(name[end]) ||
                    !std::all_of(
                        name.begin() + static_cast<std::ptrdiff_t>(target),
                        name.begin() + static_cast<std::ptrdiff_t>(end),
                        IsAsciiLetter)) {
                    continue;
                }
                auto languages =
                    std::pair{GuessLegacyLanguage(std::string_view(name).substr(
                                  source, source_size)),
                              GuessLegacyLanguage(std::string_view(name).substr(
                                  target, target_size))};
                if (!languages.first.empty() && !languages.second.empty()) {
                    return languages;
                }
            }
        }
    }
    return {};
}

std::pair<std::string, std::string> InferLegacyLanguagePair(
    const std::filesystem::path& dictionary_path, std::string_view book_name) {
    auto languages =
        InferLegacyLanguagePair(dictionary_path.filename().u8string());
    if (languages.first.empty() || languages.second.empty()) {
        languages = InferLegacyLanguagePair(std::string(book_name));
    }
    return languages;
}

std::string ApplyLegacyHtmlHeadwordConversion(std::string headword) {
    if (headword.find("&#") == std::string::npos) {
        return headword;
    }
    // The frozen Qt 5 helper constructs std::string(decoded, saveFormat).
    // Its default false count makes every matching headword empty; preserve
    // that observable index behavior rather than silently repairing it.
    return {};
}

void AppendUtf8(std::uint32_t code_point, std::string* output) {
    if (code_point <= 0x7fU) {
        output->push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7ffU) {
        output->push_back(static_cast<char>(0xc0U | (code_point >> 6U)));
        output->push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    } else if (code_point <= 0xffffU) {
        output->push_back(static_cast<char>(0xe0U | (code_point >> 12U)));
        output->push_back(
            static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
        output->push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    } else {
        output->push_back(static_cast<char>(0xf0U | (code_point >> 18U)));
        output->push_back(
            static_cast<char>(0x80U | ((code_point >> 12U) & 0x3fU)));
        output->push_back(
            static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
        output->push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    }
}

struct DecodedHtmlEntity {
    std::string text;
    std::uint32_t code_point = 0U;
    bool has_code_point = false;
};

std::optional<DecodedHtmlEntity> DecodeHtmlEntity(std::string_view entity) {
    std::uint32_t code_point = 0U;
    if (!entity.empty() && entity.front() == '#') {
        int base = 10;
        std::size_t offset = 1U;
        if (entity.size() > 2U && (entity[1] == 'x' || entity[1] == 'X')) {
            base = 16;
            offset = 2U;
        }
        if (offset < entity.size() && entity[offset] == '+') {
            ++offset;
        }
        const auto parsed =
            std::from_chars(entity.data() + offset,
                            entity.data() + entity.size(), code_point, base);
        if (offset == entity.size() || parsed.ec != std::errc{} ||
            parsed.ptr != entity.data() + entity.size()) {
            return std::nullopt;
        }
        if (code_point > 0x10ffffU) {
            return DecodedHtmlEntity{std::string{"??"}, 0U, false};
        }
        if (code_point >= 0xd800U && code_point <= 0xdfffU) {
            return DecodedHtmlEntity{std::string{"?"}, 0U, false};
        }
        if (code_point >= 0x80U && code_point < 0xa0U) {
            code_point = kWindowsLatin1ExtendedCharacters[code_point - 0x80U];
        }
    } else {
        const auto found = std::lower_bound(
            std::begin(kLegacyHtmlEntities), std::end(kLegacyHtmlEntities),
            entity,
            [](const LegacyHtmlEntity& candidate, std::string_view requested) {
                return candidate.name < requested;
            });
        if (found == std::end(kLegacyHtmlEntities) || found->name != entity) {
            return std::nullopt;
        }
        code_point = found->code_point;
    }
    return DecodedHtmlEntity{{}, code_point, true};
}

std::optional<DecodedHtmlEntity> DecodeHtmlEntityAt(std::string_view source,
                                                    std::size_t ampersand,
                                                    std::size_t* consumed) {
    const auto entity_begin = ampersand + 1U;
    auto cursor = entity_begin;
    std::size_t entity_size = 0U;
    while (cursor < source.size()) {
        const auto character = static_cast<unsigned char>(source[cursor++]);
        if (std::isspace(character) != 0 || cursor - entity_begin > 9U) {
            return std::nullopt;
        }
        if (character == ';') {
            break;
        }
        ++entity_size;
    }
    if (entity_size == 0U) {
        return std::nullopt;
    }
    const auto decoded =
        DecodeHtmlEntity(source.substr(entity_begin, entity_size));
    if (decoded.has_value()) {
        *consumed = cursor - ampersand;
    }
    return decoded;
}

struct HtmlTag {
    std::string name;
    bool closing = false;
    bool self_closing = false;
};

bool HasHtmlTagNamePrefix(std::string_view contents) {
    std::size_t cursor = 0U;
    while (cursor < contents.size() &&
           std::isspace(static_cast<unsigned char>(contents[cursor])) != 0) {
        ++cursor;
    }
    if (cursor < contents.size() && contents[cursor] == '/') {
        ++cursor;
    }
    while (cursor < contents.size() &&
           std::isspace(static_cast<unsigned char>(contents[cursor])) != 0) {
        ++cursor;
    }
    if (cursor == contents.size()) {
        return false;
    }
    const auto character = static_cast<char>(
        std::tolower(static_cast<unsigned char>(contents[cursor])));
    return IsAsciiLetter(character) ||
           (contents[cursor] >= '0' && contents[cursor] <= '9');
}

std::optional<HtmlTag> ParseHtmlTag(std::string_view contents) {
    std::size_t cursor = 0U;
    while (cursor < contents.size() &&
           std::isspace(static_cast<unsigned char>(contents[cursor])) != 0) {
        ++cursor;
    }
    HtmlTag tag;
    if (cursor < contents.size() && contents[cursor] == '/') {
        tag.closing = true;
        ++cursor;
    }
    while (cursor < contents.size() &&
           std::isspace(static_cast<unsigned char>(contents[cursor])) != 0) {
        ++cursor;
    }
    const auto begin = cursor;
    while (cursor < contents.size() &&
           (IsAsciiLetter(static_cast<char>(
                std::tolower(static_cast<unsigned char>(contents[cursor])))) ||
            (contents[cursor] >= '0' && contents[cursor] <= '9'))) {
        ++cursor;
    }
    if (cursor == begin) {
        return std::nullopt;
    }
    tag.name = FoldAscii(std::string(contents.substr(begin, cursor - begin)));
    auto suffix = contents.size();
    while (suffix > cursor && std::isspace(static_cast<unsigned char>(
                                  contents[suffix - 1U])) != 0) {
        --suffix;
    }
    tag.self_closing = suffix > cursor && contents[suffix - 1U] == '/';
    return tag;
}

bool IsLegacyBlockElement(std::string_view name) {
    constexpr std::array<std::string_view, 23U> kBlockElements = {
        "blockquote", "body", "caption", "center", "dd", "div", "dl", "dt",
        "h1",         "h2",   "h3",      "h4",     "h5", "h6",  "hr", "li",
        "ol",         "p",    "pre",     "qt",     "ul", "td",  "th"};
    return std::find(kBlockElements.begin(), kBlockElements.end(), name) !=
           kBlockElements.end();
}

bool IsLegacyHiddenElement(std::string_view name) {
    constexpr std::array<std::string_view, 4U> kHiddenElements = {
        "head", "script", "style", "title"};
    return std::find(kHiddenElements.begin(), kHiddenElements.end(), name) !=
           kHiddenElements.end();
}

void AppendExplicitLineBreak(std::string* output) {
    output->push_back('\n');
}

void EnsureBlockBoundary(std::string* output) {
    if (!output->empty() && output->back() != '\n') {
        output->push_back('\n');
    }
}

std::string_view TrimQt5StringWhitespace(std::string_view source) noexcept {
    std::int32_t begin = 0;
    const auto length = static_cast<std::int32_t>(source.size());
    while (begin < length) {
        auto next = begin;
        UChar32 code_point = 0;
        U8_NEXT(source.data(), next, length, code_point);
        if (code_point < 0 ||
            !std::binary_search(kQt5StringWhitespace.begin(),
                                kQt5StringWhitespace.end(), code_point)) {
            break;
        }
        begin = next;
    }

    auto end = length;
    while (end > begin) {
        auto previous = end;
        UChar32 code_point = 0;
        U8_PREV(source.data(), 0, previous, code_point);
        if (code_point < 0 ||
            !std::binary_search(kQt5StringWhitespace.begin(),
                                kQt5StringWhitespace.end(), code_point)) {
            break;
        }
        end = previous;
    }
    return source.substr(static_cast<std::size_t>(begin),
                         static_cast<std::size_t>(end - begin));
}

bool IsQt5StringWhitespace(UChar32 code_point) noexcept {
    return std::binary_search(kQt5StringWhitespace.begin(),
                              kQt5StringWhitespace.end(), code_point);
}

std::string LegacyDescriptionText(std::string_view source) {
    const bool requires_rich_text =
        source.find('<') != std::string_view::npos ||
        source.find('&') != std::string_view::npos ||
        source.find('\t') != std::string_view::npos ||
        source.find("\\n") != std::string_view::npos;
    if (!requires_rich_text) {
        // Html::unescape returns strings without markup markers verbatim.
        return std::string(source);
    }

    // StarDict replaces its two legacy line-break spellings before calling
    // Html::unescape. That helper trims only the resulting rich-text source;
    // its plain-text result is deliberately not trimmed afterwards.
    std::string rich_text;
    rich_text.reserve(source.size());
    for (std::size_t cursor = 0U; cursor < source.size();) {
        if (source[cursor] == '\t') {
            rich_text += "<br/>";
            ++cursor;
        } else if (source[cursor] == '\\' && cursor + 1U < source.size() &&
                   source[cursor + 1U] == 'n') {
            rich_text += "<br/>";
            cursor += 2U;
        } else {
            rich_text.push_back(source[cursor++]);
        }
    }
    source = TrimQt5StringWhitespace(rich_text);
    std::string output;
    output.reserve(source.size());
    std::vector<std::string> hidden_elements;
    std::size_t preformatted_depth = 0U;
    std::vector<std::size_t> preformatted_output_starts;
    bool pending_block_boundary = false;
    bool pending_hr_boundary = false;
    bool pending_collapsed_space = false;
    auto next_tag_end = source.find('>');
    const auto apply_pending_block_boundary = [&]() {
        if (pending_hr_boundary) {
            pending_collapsed_space = false;
            AppendExplicitLineBreak(&output);
            pending_hr_boundary = false;
        }
        if (pending_block_boundary) {
            pending_collapsed_space = false;
            EnsureBlockBoundary(&output);
            pending_block_boundary = false;
        }
    };
    const auto flush_collapsed_space = [&]() {
        if (pending_collapsed_space && !output.empty() &&
            output.back() != '\n' && output.back() != '\t') {
            output.push_back(' ');
        }
        pending_collapsed_space = false;
    };
    for (std::size_t cursor = 0U; cursor < source.size();) {
        if (source[cursor] == '<') {
            if (source.substr(cursor, 4U) == "<!--") {
                const auto end = source.find("-->", cursor + 4U);
                cursor =
                    end == std::string_view::npos ? source.size() : end + 3U;
                continue;
            }
            if (!HasHtmlTagNamePrefix(source.substr(cursor + 1U))) {
                if (hidden_elements.empty()) {
                    apply_pending_block_boundary();
                    flush_collapsed_space();
                    output.push_back(source[cursor]);
                }
                ++cursor;
                continue;
            }
            while (next_tag_end != std::string_view::npos &&
                   next_tag_end < cursor) {
                next_tag_end = source.find('>', next_tag_end + 1U);
            }
            const auto end = next_tag_end;
            if (end != std::string_view::npos) {
                const auto tag =
                    ParseHtmlTag(source.substr(cursor + 1U, end - cursor - 1U));
                if (tag.has_value()) {
                    if (IsLegacyHiddenElement(tag->name)) {
                        if (!tag->closing && !tag->self_closing) {
                            hidden_elements.push_back(tag->name);
                        } else if (tag->closing && !hidden_elements.empty() &&
                                   hidden_elements.back() == tag->name) {
                            hidden_elements.pop_back();
                        }
                        cursor = end + 1U;
                        continue;
                    }
                    if (!hidden_elements.empty()) {
                        cursor = end + 1U;
                        continue;
                    }
                    if (tag->name == "pre" && tag->closing &&
                        !preformatted_output_starts.empty()) {
                        if (output.size() > preformatted_output_starts.back() &&
                            output.back() == '\n') {
                            output.pop_back();
                        }
                        preformatted_output_starts.pop_back();
                        if (preformatted_depth != 0U) {
                            --preformatted_depth;
                        }
                    }
                    if (tag->name == "br" && !tag->closing) {
                        pending_collapsed_space = false;
                        apply_pending_block_boundary();
                        AppendExplicitLineBreak(&output);
                    } else if (tag->name == "hr" && !tag->closing) {
                        pending_collapsed_space = false;
                        apply_pending_block_boundary();
                        AppendExplicitLineBreak(&output);
                        pending_hr_boundary = true;
                    } else if (!tag->closing &&
                               IsLegacyBlockElement(tag->name)) {
                        pending_collapsed_space = false;
                        apply_pending_block_boundary();
                        EnsureBlockBoundary(&output);
                        pending_block_boundary = false;
                    } else if (tag->closing &&
                               IsLegacyBlockElement(tag->name)) {
                        pending_collapsed_space = false;
                        if (tag->name != "div") {
                            pending_block_boundary = true;
                        }
                    }
                    cursor = end + 1U;
                    if (tag->name == "pre" && !tag->closing) {
                        ++preformatted_depth;
                        preformatted_output_starts.push_back(output.size());
                        if (cursor < source.size() && source[cursor] == '\n') {
                            ++cursor;
                        }
                    }
                    continue;
                }
            }
        }
        if (!hidden_elements.empty()) {
            ++cursor;
            continue;
        }
        if (source[cursor] == '&') {
            std::size_t consumed = 0U;
            const auto decoded = DecodeHtmlEntityAt(source, cursor, &consumed);
            if (decoded.has_value()) {
                if (decoded->has_code_point &&
                    (decoded->code_point == 0x2029U ||
                     (decoded->code_point == 0x2028U &&
                      preformatted_depth != 0U))) {
                    pending_collapsed_space = false;
                    apply_pending_block_boundary();
                    AppendExplicitLineBreak(&output);
                    cursor += consumed;
                    continue;
                }
                if (decoded->has_code_point && decoded->code_point != 0x00a0U &&
                    IsQt5StringWhitespace(
                        static_cast<UChar32>(decoded->code_point)) &&
                    preformatted_depth == 0U) {
                    if (!pending_block_boundary) {
                        pending_collapsed_space = !output.empty();
                    }
                    cursor += consumed;
                    continue;
                }
                apply_pending_block_boundary();
                flush_collapsed_space();
                if (!decoded->has_code_point) {
                    output += decoded->text;
                } else if (decoded->code_point == 0x00a0U) {
                    output.push_back(' ');
                } else {
                    AppendUtf8(decoded->code_point, &output);
                }
                cursor += consumed;
                continue;
            }
        }
        auto next = static_cast<std::int32_t>(cursor);
        UChar32 code_point = 0;
        U8_NEXT(source.data(), next, static_cast<std::int32_t>(source.size()),
                code_point);
        if (code_point >= 0 && IsQt5StringWhitespace(code_point)) {
            if (code_point == 0x2029U ||
                (code_point == 0x2028U && preformatted_depth != 0U)) {
                pending_collapsed_space = false;
                apply_pending_block_boundary();
                AppendExplicitLineBreak(&output);
            } else if (code_point == 0x00a0U) {
                apply_pending_block_boundary();
                flush_collapsed_space();
                output.push_back(' ');
            } else if (preformatted_depth == 0U) {
                if (!pending_block_boundary) {
                    pending_collapsed_space = !output.empty();
                }
            } else {
                output.append(source.substr(
                    cursor, static_cast<std::size_t>(next) - cursor));
            }
            cursor = static_cast<std::size_t>(next);
            continue;
        }
        apply_pending_block_boundary();
        flush_collapsed_space();
        output.push_back(source[cursor++]);
    }
    return output;
}

std::string LegacyCopyrightText(std::string value) {
    std::string output;
    output.reserve(value.size());
    for (std::size_t cursor = 0U; cursor < value.size();) {
        if (cursor + 4U <= value.size() && value[cursor] == '<' &&
            (value[cursor + 1U] == 'b' || value[cursor + 1U] == 'B') &&
            (value[cursor + 2U] == 'r' || value[cursor + 2U] == 'R') &&
            value[cursor + 3U] == '>') {
            output.push_back('\n');
            cursor += 4U;
        } else {
            output.push_back(value[cursor++]);
        }
    }
    return output;
}

const std::string& RequireField(
    const std::map<std::string, std::string>& fields, std::string_view name,
    const std::filesystem::path& path) {
    const auto iterator = fields.find(std::string(name));
    if (iterator == fields.end() || iterator->second.empty()) {
        Throw(ErrorCode::kInvalidInfo, path,
              "Missing required field: " + std::string(name));
    }
    return iterator->second;
}

std::uint32_t ReadBigEndian32(const std::string& data, std::size_t offset) {
    const auto byte = [&data](std::size_t position) {
        return static_cast<std::uint32_t>(
            static_cast<unsigned char>(data[position]));
    };
    return (byte(offset) << 24U) | (byte(offset + 1U) << 16U) |
           (byte(offset + 2U) << 8U) | byte(offset + 3U);
}

void AppendBigEndian32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>((value >> 24U) & 0xffU));
    output->push_back(static_cast<char>((value >> 16U) & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>(value & 0xffU));
}

void AppendBigEndian64(std::uint64_t value, std::string* output) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

std::uint64_t ReadBigEndian64(const std::string& data, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8U; ++index) {
        value =
            (value << 8U) | static_cast<unsigned char>(data[offset + index]);
    }
    return value;
}

}  // namespace

namespace internal {

std::string ConvertLegacyDescriptionText(std::string_view source) {
    return LegacyDescriptionText(source);
}

}  // namespace internal

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(message + ": " + path.string()),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(
    const std::filesystem::path& info_path,
    const std::optional<std::filesystem::path>& generated_index_path) {
    const std::string info_contents =
        ReadFile(info_path, ErrorCode::kInvalidInfo, 1024U * 1024U);
    const auto fields = ParseInfoFields(info_contents, info_path);

    if (const auto iterator = fields.find("idxoffsetbits");
        iterator != fields.end()) {
        const auto offset_bits =
            ParseLegacyUnsigned(iterator->second, info_path, "idxoffsetbits");
        if (offset_bits != 32U && offset_bits != 64U) {
            Throw(ErrorCode::kInvalidInfo, info_path,
                  "Invalid idxoffsetbits value");
        }
        if (offset_bits == 64U) {
            Throw(ErrorCode::kUnsupportedFeature, info_path,
                  "Only 32-bit index offsets are supported");
        }
    }
    if (const auto iterator = fields.find("dicttype");
        iterator != fields.end() && !iterator->second.empty()) {
        Throw(ErrorCode::kUnsupportedFeature, info_path,
              "Special dictionary types are not supported");
    }
    const auto book_name_iterator = fields.find("bookname");
    const std::string book_name = book_name_iterator == fields.end()
                                      ? std::string{}
                                      : book_name_iterator->second;
    const auto parse_optional_count = [&fields,
                                       &info_path](std::string_view name) {
        const auto found = fields.find(std::string(name));
        if (found == fields.end()) {
            return std::uint64_t{0};
        }
        return static_cast<std::uint64_t>(
            ParseLegacyUnsigned(found->second, info_path, name));
    };
    const auto word_count = parse_optional_count("wordcount");
    const auto index_file_size = parse_optional_count("idxfilesize");
    const auto& same_type_sequence =
        RequireField(fields, "sametypesequence", info_path);
    if (same_type_sequence != "m" && same_type_sequence != "h") {
        Throw(ErrorCode::kUnsupportedFeature, info_path,
              "Only sametypesequence=m or h is supported in this increment");
    }

    auto base_path = info_path;
    base_path.replace_extension();
    const auto index_path = ResolveRequiredCompanion(
        base_path, {".idx", ".idx.gz", ".idx.dz", ".IDX", ".IDX.GZ", ".IDX.DZ"},
        "StarDict index file");
    const auto dictionary_path = ResolveRequiredCompanion(
        base_path, {".dict", ".dict.dz", ".DICT", ".dict.DZ"},
        "StarDict dictionary data file");
    const auto resolved_synonym_path = ResolveOptionalCompanion(
        base_path,
        {".syn", ".syn.gz", ".syn.dz", ".SYN", ".SYN.GZ", ".SYN.DZ"});
    const auto declared_synonym_count = parse_optional_count("synwordcount");
    const std::optional<std::filesystem::path> synonym_path =
        declared_synonym_count == 0U ? std::nullopt : resolved_synonym_path;
    std::vector<std::filesystem::path> source_paths = {info_path, index_path,
                                                       dictionary_path};
    if (synonym_path.has_value()) {
        source_paths.push_back(*synonym_path);
    }
    std::optional<dictionary::SourceSnapshot> initial_sources;
    if (generated_index_path.has_value()) {
        try {
            initial_sources = dictionary::CaptureSourceSnapshot(source_paths);
        } catch (const dictionary::GeneratedIndexError& error) {
            Throw(ErrorCode::kIndexStorage, *generated_index_path,
                  error.what());
        }
    }

    Reader reader;
    reader.metadata_.book_name = book_name;
    const auto languages = InferLegacyLanguagePair(dictionary_path, book_name);
    reader.metadata_.source_language = languages.first;
    reader.metadata_.target_language = languages.second;
    const auto append_labeled_metadata =
        [&reader, &fields](std::string_view key, std::string_view label) {
            const auto iterator = fields.find(std::string(key));
            if (iterator == fields.end() || iterator->second.empty())
                return;
            std::string value = iterator->second;
            if (key == "copyright") {
                value = LegacyCopyrightText(std::move(value));
            }
            reader.metadata_.description += std::string(label) + value;
            reader.metadata_.description += "\n\n";
        };
    append_labeled_metadata("copyright", "Copyright: ");
    append_labeled_metadata("author", "Author: ");
    append_labeled_metadata("email", "E-mail: ");
    append_labeled_metadata("website", "Website: ");
    append_labeled_metadata("date", "Date: ");
    if (const auto description = fields.find("description");
        description != fields.end() && !description->second.empty()) {
        reader.metadata_.description +=
            internal::ConvertLegacyDescriptionText(description->second);
    }
    if (reader.metadata_.description.empty()) {
        reader.metadata_.description = "NONE";
    }
    reader.metadata_.word_count = word_count;
    reader.metadata_.synonym_count =
        synonym_path.has_value() ? declared_synonym_count : 0U;
    reader.metadata_.index_file_size = index_file_size;
    reader.metadata_.same_type_sequence = same_type_sequence;

    reader.dictionary_data_ =
        ReadCompanion(dictionary_path, ErrorCode::kInvalidDictionary,
                      kMaximumDictionarySize, "dictionary data");

    const auto parse_source_index = [&reader, &index_path, &synonym_path]() {
        const std::string index_data = ReadCompanion(
            index_path, ErrorCode::kInvalidIndex, kMaximumIndexSize, "index");

        std::vector<IndexRecord> records;
        std::size_t cursor = 0;
        while (cursor < index_data.size()) {
            const auto terminator = index_data.find('\0', cursor);
            if (terminator == std::string::npos ||
                index_data.size() - terminator - 1U < 8U) {
                break;
            }
            IndexRecord record;
            record.headword = ApplyLegacyHtmlHeadwordConversion(
                index_data.substr(cursor, terminator - cursor));
            record.primary_headword = record.headword;
            record.article_offset =
                ReadBigEndian32(index_data, terminator + 1U);
            record.article_size = ReadBigEndian32(index_data, terminator + 5U);
            records.push_back(std::move(record));
            cursor = terminator + 9U;
        }
        reader.primary_record_count_ = records.size();
        if (synonym_path.has_value()) {
            const std::string synonym_data =
                ReadCompanion(*synonym_path, ErrorCode::kInvalidIndex,
                              kMaximumIndexSize, "synonym index");
            std::size_t synonym_cursor = 0U;
            while (synonym_cursor < synonym_data.size()) {
                const auto terminator = synonym_data.find('\0', synonym_cursor);
                if (terminator == std::string::npos ||
                    synonym_data.size() - terminator - 1U < 4U) {
                    break;
                }
                const auto article_index =
                    ReadBigEndian32(synonym_data, terminator + 1U);
                if (article_index >= reader.primary_record_count_) {
                    Throw(ErrorCode::kInvalidIndex, *synonym_path,
                          "Synonym references an invalid index record");
                }
                IndexRecord record = records[article_index];
                record.headword =
                    ApplyLegacyHtmlHeadwordConversion(synonym_data.substr(
                        synonym_cursor, terminator - synonym_cursor));
                const bool starts_with_slash =
                    !record.headword.empty() && record.headword.front() == '/';
                const bool ends_with_dollar =
                    !record.headword.empty() && record.headword.back() == '$';
                const bool has_slash =
                    record.headword.find('/') != std::string::npos;
                const bool has_dollar =
                    record.headword.find('$') != std::string::npos;
                if (!((starts_with_slash && has_dollar) ||
                      (!starts_with_slash && ends_with_dollar && has_slash))) {
                    records.push_back(std::move(record));
                }
                synonym_cursor = terminator + 5U;
            }
        }
        return records;
    };

    const auto serialize_records =
        [&reader](const std::vector<IndexRecord>& records) {
            std::string payload;
            AppendBigEndian64(reader.primary_record_count_, &payload);
            AppendBigEndian64(records.size(), &payload);
            for (const auto& record : records) {
                if (record.headword.size() >
                        std::numeric_limits<std::uint32_t>::max() ||
                    record.primary_headword.size() >
                        std::numeric_limits<std::uint32_t>::max()) {
                    throw dictionary::GeneratedIndexError(
                        "StarDict headword exceeds the index size limit");
                }
                AppendBigEndian32(
                    static_cast<std::uint32_t>(record.headword.size()),
                    &payload);
                payload.append(record.headword);
                AppendBigEndian32(
                    static_cast<std::uint32_t>(record.primary_headword.size()),
                    &payload);
                payload.append(record.primary_headword);
                AppendBigEndian32(record.article_offset, &payload);
                AppendBigEndian32(record.article_size, &payload);
            }
            return payload;
        };

    const auto parse_generated_index = [&reader](const std::string& payload) {
        const auto require = [&payload](std::size_t position,
                                        std::size_t size) {
            if (position > payload.size() || size > payload.size() - position) {
                throw dictionary::GeneratedIndexError(
                    "Generated StarDict index is truncated");
            }
        };
        require(0U, 16U);
        const auto primary_record_count = ReadBigEndian64(payload, 0U);
        const auto record_count = ReadBigEndian64(payload, 8U);
        if (primary_record_count > record_count ||
            primary_record_count > std::numeric_limits<std::size_t>::max() ||
            record_count > std::numeric_limits<std::size_t>::max() ||
            record_count > (payload.size() - 16U) / 16U) {
            throw dictionary::GeneratedIndexError(
                "Generated StarDict index has an invalid record count");
        }
        std::vector<IndexRecord> records;
        records.reserve(static_cast<std::size_t>(record_count));
        std::size_t cursor = 16U;
        for (std::uint64_t index = 0; index < record_count; ++index) {
            require(cursor, 4U);
            const auto headword_size = ReadBigEndian32(payload, cursor);
            cursor += 4U;
            require(cursor, static_cast<std::size_t>(headword_size) + 4U);
            IndexRecord record;
            record.headword = payload.substr(cursor, headword_size);
            cursor += headword_size;
            const auto primary_size = ReadBigEndian32(payload, cursor);
            cursor += 4U;
            require(cursor, static_cast<std::size_t>(primary_size) + 8U);
            record.primary_headword = payload.substr(cursor, primary_size);
            cursor += primary_size;
            record.article_offset = ReadBigEndian32(payload, cursor);
            record.article_size = ReadBigEndian32(payload, cursor + 4U);
            cursor += 8U;
            records.push_back(std::move(record));
        }
        if (cursor != payload.size()) {
            throw dictionary::GeneratedIndexError(
                "Generated StarDict index contains trailing data");
        }
        reader.primary_record_count_ =
            static_cast<std::size_t>(primary_record_count);
        return records;
    };

    if (!generated_index_path.has_value()) {
        reader.index_ = parse_source_index();
    } else {
        try {
            const auto sources =
                dictionary::CaptureSourceSnapshot(source_paths);
            if (sources != *initial_sources) {
                Throw(ErrorCode::kIndexStorage, *generated_index_path,
                      "Dictionary sources changed while opening index");
            }
            auto loaded = dictionary::LoadGeneratedIndex(
                *generated_index_path, kGeneratedIndexFormat, sources);
            auto rebuild_state = loaded.state;
            if (loaded.state == dictionary::GeneratedIndexState::kCurrent) {
                try {
                    reader.index_ = parse_generated_index(loaded.payload);
                    reader.index_state_ = IndexState::kReused;
                } catch (const dictionary::GeneratedIndexError&) {
                    rebuild_state = dictionary::GeneratedIndexState::kCorrupt;
                }
            }
            if (reader.index_state_ != IndexState::kReused) {
                reader.index_ = parse_source_index();
                const auto verified_sources =
                    dictionary::CaptureSourceSnapshot(source_paths);
                if (verified_sources != sources) {
                    Throw(ErrorCode::kIndexStorage, *generated_index_path,
                          "Dictionary sources changed while building index");
                }
                dictionary::StoreGeneratedIndex(
                    *generated_index_path, kGeneratedIndexFormat, sources,
                    serialize_records(reader.index_));
                switch (rebuild_state) {
                    case dictionary::GeneratedIndexState::kMissing:
                        reader.index_state_ = IndexState::kCreated;
                        break;
                    case dictionary::GeneratedIndexState::kStale:
                        reader.index_state_ = IndexState::kRebuiltStale;
                        break;
                    case dictionary::GeneratedIndexState::kCorrupt:
                    case dictionary::GeneratedIndexState::kCurrent:
                        reader.index_state_ = IndexState::kRebuiltCorrupt;
                        break;
                }
            }
            const auto final_sources =
                dictionary::CaptureSourceSnapshot(source_paths);
            if (final_sources != sources) {
                Throw(ErrorCode::kIndexStorage, *generated_index_path,
                      "Dictionary sources changed while opening index");
            }
        } catch (const dictionary::GeneratedIndexError& error) {
            Throw(ErrorCode::kIndexStorage, *generated_index_path,
                  error.what());
        }
    }

    for (auto& record : reader.index_) {
        try {
            record.folded_headword = foundation::FoldForLookup(record.headword);
        } catch (const foundation::TextFoldingError& error) {
            Throw(ErrorCode::kInvalidIndex, index_path,
                  std::string("Invalid UTF-8 headword: ") + error.what());
        }
        const auto offset = static_cast<std::uint64_t>(record.article_offset);
        const auto size = static_cast<std::uint64_t>(record.article_size);
        if (offset > reader.dictionary_data_.size() ||
            size > reader.dictionary_data_.size() - offset) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "Index record points outside dictionary data");
        }
    }
    try {
        reader.source_snapshot_ =
            dictionary::CaptureSourceSnapshot(source_paths);
    } catch (const dictionary::GeneratedIndexError& error) {
        Throw(ErrorCode::kIndexStorage, info_path, error.what());
    }
    return reader;
}

std::vector<PrimaryArticle> Reader::ReadPrimaryArticles(
    const std::function<void()>& checkpoint) const {
    std::vector<PrimaryArticle> articles;
    articles.reserve(primary_record_count_);
    for (std::size_t ordinal = 0U; ordinal < primary_record_count_; ++ordinal) {
        if (checkpoint)
            checkpoint();
        const auto& record = index_[ordinal];
        PrimaryArticle article;
        article.record_ordinal = ordinal;
        article.headword = record.headword;
        article.article_offset = record.article_offset;
        article.article_size = record.article_size;
        article.data =
            dictionary_data_.substr(record.article_offset, record.article_size);
        articles.push_back(std::move(article));
    }
    return articles;
}

std::pair<std::vector<std::string>, bool> Reader::EnumerateHeadwords(
    std::size_t offset, std::size_t result_limit, std::size_t byte_limit,
    const std::function<void()>& checkpoint) const {
    try {
        return enumeration_index_.Page(
            index_.size(),
            [this](std::uint32_t ordinal) -> std::string_view {
                return index_[ordinal].headword;
            },
            offset, result_limit, byte_limit, checkpoint);
    } catch (const dictionary::OrderedHeadwordError& error) {
        Throw(ErrorCode::kInvalidIndex, {}, error.what());
    }
}

std::vector<Article> Reader::LookupExact(
    std::string_view headword, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> articles;
    const std::string folded_headword = foundation::FoldForLookup(headword);
    if (folded_headword.empty() || result_limit == 0U) {
        return articles;
    }
    std::size_t record_number = 0;
    for (const auto& record : index_) {
        if (checkpoint && (record_number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (articles.size() == result_limit) {
            break;
        }
        if (record.folded_headword != folded_headword) {
            continue;
        }
        Article article;
        article.headword = record.headword;
        article.data =
            dictionary_data_.substr(record.article_offset, record.article_size);
        articles.push_back(std::move(article));
    }
    return articles;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> articles;
    if (result_limit == 0U) {
        return articles;
    }

    const auto matches = RankedPrefixMatches(prefix, checkpoint);

    articles.reserve(std::min(result_limit, matches.size()));
    for (const auto* record : matches) {
        if (articles.size() == result_limit) {
            break;
        }
        Article article;
        article.headword = record->headword;
        article.data = dictionary_data_.substr(record->article_offset,
                                               record->article_size);
        articles.push_back(std::move(article));
    }
    return articles;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> suggestions;
    if (result_limit == 0U) {
        return suggestions;
    }
    const auto matches = RankedPrefixMatches(prefix, checkpoint);
    std::unordered_set<std::string> seen;
    suggestions.reserve(std::min(result_limit, matches.size()));
    for (const auto* record : matches) {
        if (suggestions.size() == result_limit) {
            break;
        }
        if (seen.insert(record->headword).second) {
            suggestions.push_back(record->headword);
        }
    }
    return suggestions;
}

std::vector<std::string> Reader::FindHeadwordsForSynonym(
    std::string_view headword, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(headword);
    std::vector<std::string> result;
    if (result_limit == 0U)
        return result;
    std::unordered_set<std::string> seen;
    std::size_t number = 0U;
    for (const auto& record : index_) {
        if (checkpoint && (number++ % 1024U) == 0U)
            checkpoint();
        if (record.folded_headword == folded &&
            foundation::FoldForLookup(record.primary_headword) != folded &&
            seen.insert(record.primary_headword).second) {
            result.push_back(record.primary_headword);
            if (result.size() == result_limit)
                break;
        }
    }
    return result;
}

std::vector<const Reader::IndexRecord*> Reader::RankedPrefixMatches(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded_prefix = foundation::FoldForLookup(prefix);
    if (folded_prefix.empty()) {
        return {};
    }
    std::vector<const IndexRecord*> matches;
    std::size_t record_number = 0;
    for (const auto& record : index_) {
        if (checkpoint && (record_number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (HasPrefix(record.folded_headword, folded_prefix)) {
            matches.push_back(&record);
        }
    }
    std::stable_sort(
        matches.begin(), matches.end(),
        [&folded_prefix](const auto* left, const auto* right) {
            const bool left_exact = left->folded_headword == folded_prefix;
            const bool right_exact = right->folded_headword == folded_prefix;
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

}  // namespace goldendict::core::formats::stardict
