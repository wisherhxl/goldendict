// SPDX-License-Identifier: GPL-3.0-or-later

#include "xdxf_reader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>

#include <expat.h>
#include <zlib.h>

#include "../../foundation/text_folding.h"
#include "../../foundation/utf8.h"

namespace goldendict::core::formats::xdxf {
namespace {

constexpr std::size_t kMaximumStoredFileSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumXmlSize = 512U * 1024U * 1024U;
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
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect XDXF file");
    }
    if (size > kMaximumStoredFileSize) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "XDXF file exceeds the supported size limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Throw(ErrorCode::kMissingFile, path, "Cannot open XDXF file");
    }
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete XDXF file");
    }
    return data;
}

bool HasCompressedSuffix(const std::filesystem::path& path) {
    std::string filename = path.filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return filename.size() >= 8U &&
           filename.compare(filename.size() - 8U, 8U, ".xdxf.dz") == 0;
}

std::string Gunzip(std::string_view compressed,
                   const std::filesystem::path& path) {
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<char*>(compressed.data()));  // zlib's API predates const.
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot initialize XDXF gzip decompression");
    }
    std::string output;
    std::array<char, 64U * 1024U> buffer{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = buffer.size() - stream.avail_out;
        if (produced > kMaximumXmlSize - output.size()) {
            inflateEnd(&stream);
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Decompressed XDXF exceeds the supported size limit");
        }
        output.append(buffer.data(), produced);
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.avail_in != 0U) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid gzip-compressed XDXF file");
    }
    return output;
}

std::string Escape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
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
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped.push_back(character);
        }
    }
    return escaped;
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

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

std::string LanguageCode(std::string_view language) {
    std::string code = Lower(Trim(std::string(language)));
    const auto separator = code.find('-');
    if (separator != std::string::npos) {
        code.resize(separator);
    }
    return code;
}

std::optional<std::string_view> Attribute(const XML_Char** attributes,
                                          std::string_view name) {
    if (attributes == nullptr) {
        return std::nullopt;
    }
    for (std::size_t index = 0; attributes[index] != nullptr; index += 2U) {
        if (name == attributes[index]) {
            return std::string_view(attributes[index + 1U]);
        }
    }
    return std::nullopt;
}

struct Element {
    std::string name;
    std::string text;
    std::string html;
    std::vector<std::string> keys;
};

struct ParsedRecord {
    std::string headword;
    std::size_t article = 0;
};

struct ParsedDocument {
    Metadata metadata;
    std::vector<ParsedRecord> records;
    std::vector<std::string> articles;
};

std::string HtmlTag(std::string_view name) {
    if (name == "k" || name == "def") {
        return "div";
    }
    if (name == "ex") {
        return "blockquote";
    }
    if (name == "tr" || name == "pos" || name == "co" || name == "abr" ||
        name == "abbr") {
        return "span";
    }
    if (name == "b" || name == "strong" || name == "i" || name == "em" ||
        name == "u" || name == "p" || name == "ul" || name == "ol" ||
        name == "li" || name == "dl" || name == "dt" || name == "dd" ||
        name == "table" || name == "thead" || name == "tbody" || name == "th" ||
        name == "td" || name == "code" || name == "pre") {
        return std::string(name);
    }
    return {};
}

void FinalizeElement(Element* element) {
    if (element->name == "k") {
        const std::string key = Trim(element->text);
        if (!key.empty()) {
            element->keys.push_back(key);
        }
        element->html.clear();
    }
    if (element->name == "kref" || element->name == "iref") {
        const std::string target = Trim(element->text);
        element->html = target.empty()
                            ? Escape(element->text)
                            : "<a href=\"bword://" + Escape(target) + "\">" +
                                  Escape(element->text) + "</a>";
        return;
    }
    if (element->name == "rref") {
        const std::string resource = Trim(element->text);
        element->html = resource.empty()
                            ? std::string{}
                            : "<img src=\"" + Escape(resource) + "\">";
        return;
    }
    if (element->name == "br") {
        element->html = "<br>";
        return;
    }
    const std::string tag = HtmlTag(element->name);
    if (!tag.empty()) {
        element->html = "<" + tag + ">" + element->html + "</" + tag + ">";
    }
}

struct ParserState {
    XML_Parser parser = nullptr;
    ParsedDocument document;
    std::vector<Element> elements;
    std::optional<std::string> error;
    std::string metadata_element;
    std::string metadata_text;
    std::size_t total_article_bytes = 0;
    bool found_root = false;
    bool inside_article = false;
};

void Fail(ParserState* state, std::string message) {
    if (!state->error.has_value()) {
        state->error = std::move(message);
        XML_StopParser(state->parser, XML_FALSE);
    }
}

void StartElement(void* user_data, const XML_Char* raw_name,
                  const XML_Char** attributes) {
    auto* state = static_cast<ParserState*>(user_data);
    const std::string name = Lower(raw_name);
    if (!state->found_root) {
        if (name != "xdxf") {
            Fail(state, "Not an XDXF dictionary file");
            return;
        }
        state->found_root = true;
        if (const auto language = Attribute(attributes, "lang_from")) {
            state->document.metadata.source_language = LanguageCode(*language);
        }
        if (const auto language = Attribute(attributes, "lang_to")) {
            state->document.metadata.target_language = LanguageCode(*language);
        }
        return;
    }
    if (state->inside_article) {
        state->elements.push_back({name, {}, {}, {}});
        return;
    }
    if (name == "ar") {
        state->inside_article = true;
        state->elements.push_back({name, {}, {}, {}});
        return;
    }
    if ((name == "full_name" || name == "full_title") &&
        state->document.metadata.name.empty()) {
        state->metadata_element = name;
        state->metadata_text.clear();
    } else if (name == "description" &&
               state->document.metadata.description.empty()) {
        state->metadata_element = name;
        state->metadata_text.clear();
    } else if (name == "from" &&
               state->document.metadata.source_language.empty()) {
        auto language = Attribute(attributes, "xml:lang");
        if (!language.has_value()) {
            language = Attribute(attributes, "lang");
        }
        if (language.has_value()) {
            state->document.metadata.source_language = LanguageCode(*language);
        }
    } else if (name == "to" &&
               state->document.metadata.target_language.empty()) {
        auto language = Attribute(attributes, "xml:lang");
        if (!language.has_value()) {
            language = Attribute(attributes, "lang");
        }
        if (language.has_value()) {
            state->document.metadata.target_language = LanguageCode(*language);
        }
    }
}

void CharacterData(void* user_data, const XML_Char* data, int length) {
    auto* state = static_cast<ParserState*>(user_data);
    const std::string_view text(data, static_cast<std::size_t>(length));
    if (state->inside_article && !state->elements.empty()) {
        auto& element = state->elements.back();
        if (text.size() > kMaximumArticleSize - element.text.size()) {
            Fail(state, "XDXF article exceeds the supported size limit");
            return;
        }
        element.text.append(text);
        const std::string escaped = Escape(text);
        if (escaped.size() > kMaximumArticleSize - element.html.size()) {
            Fail(state, "XDXF article exceeds the supported size limit");
            return;
        }
        element.html += escaped;
    } else if (!state->metadata_element.empty()) {
        if (text.size() > kMaximumArticleSize - state->metadata_text.size()) {
            Fail(state, "XDXF metadata exceeds the supported size limit");
            return;
        }
        state->metadata_text.append(text);
    }
}

void EndElement(void* user_data, const XML_Char* raw_name) {
    auto* state = static_cast<ParserState*>(user_data);
    const std::string name = Lower(raw_name);
    if (state->inside_article) {
        if (state->elements.empty() || state->elements.back().name != name) {
            Fail(state, "Malformed XDXF article structure");
            return;
        }
        Element element = std::move(state->elements.back());
        state->elements.pop_back();
        FinalizeElement(&element);
        if (element.name == "ar") {
            if (element.keys.empty()) {
                Fail(state, "XDXF article has no headword");
                return;
            }
            if (element.html.size() >
                kMaximumXmlSize - state->total_article_bytes) {
                Fail(state,
                     "Rendered XDXF articles exceed the supported size limit");
                return;
            }
            const std::size_t article = state->document.articles.size();
            state->total_article_bytes += element.html.size();
            state->document.articles.push_back(std::move(element.html));
            for (auto& key : element.keys) {
                state->document.records.push_back({std::move(key), article});
            }
            state->inside_article = false;
            return;
        }
        if (state->elements.empty()) {
            Fail(state, "Malformed XDXF article structure");
            return;
        }
        auto& parent = state->elements.back();
        if (element.text.size() > kMaximumArticleSize - parent.text.size() ||
            element.html.size() > kMaximumArticleSize - parent.html.size()) {
            Fail(state, "XDXF article exceeds the supported size limit");
            return;
        }
        parent.text += element.text;
        parent.html += element.html;
        parent.keys.insert(parent.keys.end(),
                           std::make_move_iterator(element.keys.begin()),
                           std::make_move_iterator(element.keys.end()));
        return;
    }
    if (name == state->metadata_element) {
        if (name == "description") {
            state->document.metadata.description = Trim(state->metadata_text);
        } else if (state->document.metadata.name.empty()) {
            state->document.metadata.name = Trim(state->metadata_text);
        }
        state->metadata_element.clear();
        state->metadata_text.clear();
    }
}

ParsedDocument ParseXml(std::string_view xml,
                        const std::filesystem::path& path) {
    ParserState state;
    state.parser = XML_ParserCreate("UTF-8");
    if (state.parser == nullptr) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot initialize XDXF XML parser");
    }
    XML_SetUserData(state.parser, &state);
    XML_SetElementHandler(state.parser, StartElement, EndElement);
    XML_SetCharacterDataHandler(state.parser, CharacterData);
    XML_SetParamEntityParsing(state.parser, XML_PARAM_ENTITY_PARSING_NEVER);
    const XML_Status status = XML_Parse(state.parser, xml.data(),
                                        static_cast<int>(xml.size()), XML_TRUE);
    const XML_Size line = XML_GetCurrentLineNumber(state.parser);
    const XML_Error parser_error = XML_GetErrorCode(state.parser);
    XML_ParserFree(state.parser);
    state.parser = nullptr;
    if (state.error.has_value()) {
        Throw(ErrorCode::kInvalidDictionary, path, *state.error);
    }
    if (status != XML_STATUS_OK) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Malformed XDXF XML at line " + std::to_string(line) + ": " +
                  XML_ErrorString(parser_error));
    }
    if (!state.found_root) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Not an XDXF dictionary file");
    }
    return std::move(state.document);
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

Reader Reader::Open(const std::filesystem::path& dictionary_path) {
    Reader reader;
    reader.dictionary_path_ = dictionary_path;
    std::string xml = ReadFile(dictionary_path);
    if (HasCompressedSuffix(dictionary_path)) {
        xml = Gunzip(xml, dictionary_path);
    } else if (xml.size() > kMaximumXmlSize) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "XDXF file exceeds the supported size limit");
    }
    if (!foundation::IsValidUtf8(xml)) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "XDXF is not valid UTF-8");
    }

    ParsedDocument parsed = ParseXml(xml, dictionary_path);
    reader.metadata_ = std::move(parsed.metadata);
    reader.articles_ = std::move(parsed.articles);
    if (parsed.records.size() > kMaximumRecords) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "XDXF contains too many headwords");
    }
    reader.records_.reserve(parsed.records.size());
    for (auto& parsed_record : parsed.records) {
        parsed_record.headword = Trim(std::move(parsed_record.headword));
        if (parsed_record.headword.empty() ||
            parsed_record.headword.size() > kMaximumHeadwordSize) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "Invalid XDXF headword size");
        }
        try {
            reader.records_.push_back(
                {parsed_record.headword,
                 foundation::FoldForLookup(parsed_record.headword),
                 parsed_record.article});
        } catch (const foundation::TextFoldingError& error) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "Invalid UTF-8 XDXF headword: " + std::string(error.what()));
        }
    }
    if (reader.metadata_.name.empty()) {
        std::string filename = dictionary_path.filename().string();
        const auto suffix = HasCompressedSuffix(dictionary_path) ? 8U : 5U;
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
    result.reserve(articles_.size());
    std::vector<bool> seen(articles_.size(), false);
    for (std::size_t ordinal = 0U; ordinal < records_.size(); ++ordinal) {
        if (checkpoint) {
            checkpoint();
        }
        const auto& record = records_[ordinal];
        if (seen[record.article]) {
            continue;
        }
        seen[record.article] = true;
        result.push_back({ordinal, record.headword, record.article,
                          articles_[record.article]});
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
    if (result_limit == 0U) {
        return result;
    }
    const std::string folded = foundation::FoldForLookup(headword);
    std::set<std::size_t> seen;
    std::size_t number = 0;
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
    std::size_t number = 0;
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

}  // namespace goldendict::core::formats::xdxf
