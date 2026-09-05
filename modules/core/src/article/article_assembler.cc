// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_assembler.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "article_document.h"
#include "internal_url.h"

namespace goldendict::core::article {
namespace {

const std::vector<std::string> kAllowedTags = {
    "p",  "div",        "span", "b",   "strong", "i",     "em",
    "u",  "br",         "ul",   "ol",  "li",     "dl",    "dt",
    "dd", "blockquote", "code", "pre", "table",  "thead", "tbody",
    "tr", "th",         "td",   "a",   "img",    "audio", "source"};
const std::vector<std::string> kSuppressedTags = {
    "script", "style", "iframe", "object", "embed", "svg", "math"};
constexpr std::size_t kMaximumDocumentBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumResourceReferences = 256U;

struct Tag {
    std::string name;
    std::map<std::string, std::string> attributes;
    bool closing = false;
    bool self_closing = false;
};

bool Contains(const std::vector<std::string>& values, std::string_view value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool IsAllowedAudioType(std::string_view value) {
    static const std::vector<std::string> kAllowedAudioTypes = {
        "audio/wav",  "audio/ogg", "audio/mpeg", "audio/flac",
        "audio/opus", "audio/mp4", "audio/aac",  "audio/midi"};
    return Contains(kAllowedAudioTypes, value);
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

std::optional<std::string> DecodeEntities(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size();) {
        if (value[index] != '&') {
            decoded.push_back(value[index++]);
            continue;
        }
        const auto end = value.find(';', index + 1U);
        if (end == std::string_view::npos) {
            return std::nullopt;
        }
        const auto entity = value.substr(index + 1U, end - index - 1U);
        if (entity == "amp") {
            decoded.push_back('&');
        } else if (entity == "lt") {
            decoded.push_back('<');
        } else if (entity == "gt") {
            decoded.push_back('>');
        } else if (entity == "quot") {
            decoded.push_back('"');
        } else if (entity == "apos" || entity == "#39") {
            decoded.push_back('\'');
        } else {
            return std::nullopt;
        }
        index = end + 1U;
    }
    return decoded;
}

void SkipSpace(std::string_view value, std::size_t* index) {
    while (*index < value.size() &&
           std::isspace(static_cast<unsigned char>(value[*index])) != 0) {
        ++*index;
    }
}

std::string ReadName(std::string_view value, std::size_t* index) {
    const std::size_t start = *index;
    while (*index < value.size()) {
        const unsigned char character =
            static_cast<unsigned char>(value[*index]);
        if (std::isalnum(character) == 0 && character != '-' &&
            character != '_' && character != ':') {
            break;
        }
        ++*index;
    }
    std::string name(value.substr(start, *index - start));
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name;
}

std::optional<Tag> ParseTag(std::string_view raw) {
    Tag tag;
    std::size_t index = 0;
    SkipSpace(raw, &index);
    if (index < raw.size() && raw[index] == '/') {
        tag.closing = true;
        ++index;
        SkipSpace(raw, &index);
    }
    tag.name = ReadName(raw, &index);
    if (tag.name.empty()) {
        return std::nullopt;
    }
    if (tag.closing) {
        SkipSpace(raw, &index);
        return index == raw.size() ? std::optional<Tag>(tag) : std::nullopt;
    }
    while (index < raw.size()) {
        SkipSpace(raw, &index);
        if (index == raw.size()) {
            break;
        }
        if (raw[index] == '/') {
            tag.self_closing = true;
            ++index;
            SkipSpace(raw, &index);
            if (index != raw.size()) {
                return std::nullopt;
            }
            break;
        }
        const std::string name = ReadName(raw, &index);
        if (name.empty()) {
            return std::nullopt;
        }
        SkipSpace(raw, &index);
        if (index >= raw.size() || raw[index] != '=') {
            tag.attributes.emplace(name, std::string{});
            continue;
        }
        ++index;
        SkipSpace(raw, &index);
        if (index >= raw.size() || (raw[index] != '"' && raw[index] != '\'')) {
            return std::nullopt;
        }
        const char quote = raw[index++];
        const auto end = raw.find(quote, index);
        if (end == std::string_view::npos) {
            return std::nullopt;
        }
        const auto decoded = DecodeEntities(raw.substr(index, end - index));
        if (!decoded.has_value()) {
            return std::nullopt;
        }
        tag.attributes.emplace(name, *decoded);
        index = end + 1U;
    }
    if (tag.name == "br" || tag.name == "img" || tag.name == "source") {
        tag.self_closing = true;
    }
    return tag;
}

std::optional<std::size_t> FindTagEnd(std::string_view markup,
                                      std::size_t start) {
    char quote = 0;
    for (std::size_t index = start; index < markup.size(); ++index) {
        const char character = markup[index];
        if (quote != 0) {
            if (character == quote) {
                quote = 0;
            }
        } else if (character == '"' || character == '\'') {
            quote = character;
        } else if (character == '>') {
            return index;
        }
    }
    return std::nullopt;
}

void AddBreak(std::string* plain_text) {
    if (!plain_text->empty() && plain_text->back() != '\n') {
        plain_text->push_back('\n');
    }
}

bool IsBlock(std::string_view tag) {
    return tag == "p" || tag == "div" || tag == "li" || tag == "dt" ||
           tag == "dd" || tag == "blockquote" || tag == "pre" || tag == "tr";
}

std::optional<std::string> NormalizeResourceId(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    try {
        static_cast<void>(MakeResourceUrl("probe", value));
        return value;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

bool HasResource(const std::vector<ResourceReference>& resources,
                 std::string_view dictionary_id, std::string_view resource_id) {
    return std::any_of(resources.begin(), resources.end(),
                       [dictionary_id, resource_id](const auto& resource) {
                           return resource.dictionary_id == dictionary_id &&
                                  resource.resource_id == resource_id;
                       });
}

void AddResourceReference(const dictionary::Identity& dictionary,
                          std::string_view resource_id,
                          std::vector<ResourceReference>* resources) {
    if (HasResource(*resources, dictionary.id, resource_id)) {
        return;
    }
    if (resources->size() == kMaximumResourceReferences) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Article contains too many resources");
    }
    resources->push_back({dictionary.id, std::string(resource_id)});
}

bool IsTrackedTag(std::string_view name) {
    return name == "gd-optional" || Contains(kAllowedTags, name) ||
           Contains(kSuppressedTags, name);
}

bool SanitizeMarkup(const dictionary::Identity& dictionary,
                    std::string_view markup, std::string* html,
                    std::string* plain_text,
                    std::vector<ResourceReference>* resources) {
    std::vector<std::string> stack;
    int suppressed_depth = 0;
    std::size_t position = 0;
    while (position < markup.size()) {
        const auto opening = markup.find('<', position);
        const auto text_end =
            opening == std::string_view::npos ? markup.size() : opening;
        if (text_end > position && suppressed_depth == 0) {
            const auto decoded =
                DecodeEntities(markup.substr(position, text_end - position));
            if (!decoded.has_value()) {
                return false;
            }
            html->append(Escape(*decoded));
            plain_text->append(*decoded);
        }
        if (opening == std::string_view::npos) {
            break;
        }
        if (markup.substr(opening, 4U) == "<!--") {
            const auto end = markup.find("-->", opening + 4U);
            if (end == std::string_view::npos) {
                return false;
            }
            position = end + 3U;
            continue;
        }
        const auto closing = FindTagEnd(markup, opening + 1U);
        if (!closing.has_value()) {
            return false;
        }
        const auto parsed =
            ParseTag(markup.substr(opening + 1U, *closing - opening - 1U));
        if (!parsed.has_value()) {
            return false;
        }
        const Tag& tag = *parsed;
        if (tag.closing) {
            if (!IsTrackedTag(tag.name)) {
                position = *closing + 1U;
                continue;
            }
            if (std::find(stack.begin(), stack.end(), tag.name) ==
                stack.end()) {
                position = *closing + 1U;
                continue;
            }
            if (stack.back() != tag.name) {
                return false;
            }
            stack.pop_back();
            if (suppressed_depth > 0) {
                --suppressed_depth;
            } else if (tag.name == "gd-optional") {
                html->append("</span>");
            } else if (Contains(kAllowedTags, tag.name)) {
                html->append("</" + tag.name + ">");
                if (IsBlock(tag.name)) {
                    AddBreak(plain_text);
                }
            }
        } else {
            if (!tag.self_closing && IsTrackedTag(tag.name)) {
                stack.push_back(tag.name);
            }
            if (suppressed_depth > 0 || Contains(kSuppressedTags, tag.name)) {
                if (!tag.self_closing) {
                    ++suppressed_depth;
                }
            } else if (tag.name == "gd-optional") {
                html->append("<span class=\"gd-optional-part\">");
            } else if (Contains(kAllowedTags, tag.name)) {
                html->append("<" + tag.name);
                if (tag.name == "a") {
                    const auto href = tag.attributes.find("href");
                    constexpr std::string_view kBword = "bword://";
                    if (href != tag.attributes.end() &&
                        href->second.substr(0, kBword.size()) == kBword) {
                        try {
                            html->append(" href=\"" +
                                         Escape(MakeLookupUrl(
                                             std::string_view(href->second)
                                                 .substr(kBword.size()))) +
                                         "\"");
                        } catch (const std::invalid_argument&) {}
                    } else if (href != tag.attributes.end()) {
                        constexpr std::string_view kSound = "sound://";
                        if (href->second.substr(0, kSound.size()) == kSound) {
                            const auto resource_id = NormalizeResourceId(
                                href->second.substr(kSound.size()));
                            if (resource_id.has_value()) {
                                html->append(" href=\"" +
                                             Escape(MakeResourceUrl(
                                                 dictionary.id, *resource_id)) +
                                             "\"");
                                AddResourceReference(dictionary, *resource_id,
                                                     resources);
                            }
                        }
                    }
                } else if (tag.name == "span") {
                    const auto class_name = tag.attributes.find("class");
                    if (class_name != tag.attributes.end() &&
                        class_name->second == "dsl_p") {
                        html->append(" class=\"dsl_p\"");
                        const auto title = tag.attributes.find("title");
                        if (title != tag.attributes.end()) {
                            html->append(" title=\"" + Escape(title->second) +
                                         "\"");
                        }
                    }
                } else if (tag.name == "img" || tag.name == "source") {
                    const auto source = tag.attributes.find("src");
                    if (source != tag.attributes.end()) {
                        const auto resource_id =
                            NormalizeResourceId(source->second);
                        if (resource_id.has_value()) {
                            html->append(" src=\"" +
                                         Escape(MakeResourceUrl(dictionary.id,
                                                                *resource_id)) +
                                         "\"");
                            AddResourceReference(dictionary, *resource_id,
                                                 resources);
                        }
                    }
                }
                for (const std::string_view attribute_name : {"alt", "title"}) {
                    const auto attribute =
                        tag.attributes.find(std::string(attribute_name));
                    if ((tag.name == "img" || tag.name == "a") &&
                        attribute != tag.attributes.end()) {
                        html->append(" " + std::string(attribute_name) + "=\"" +
                                     Escape(attribute->second) + "\"");
                    }
                }
                if (tag.name == "audio" &&
                    tag.attributes.find("controls") != tag.attributes.end()) {
                    html->append(" controls=\"controls\"");
                }
                if (tag.name == "source") {
                    const auto type = tag.attributes.find("type");
                    if (type != tag.attributes.end() &&
                        IsAllowedAudioType(type->second)) {
                        html->append(" type=\"" + Escape(type->second) + "\"");
                    }
                }
                html->append(">");
                if (tag.name == "br") {
                    AddBreak(plain_text);
                }
            }
        }
        position = *closing + 1U;
    }
    return stack.empty() && suppressed_depth == 0;
}

}  // namespace

Document Assemble(const dictionary::Identity& dictionary,
                  const std::vector<dictionary::Article>& articles) {
    Document document;
    std::size_t input_size = 0;
    for (const auto& article : articles) {
        if (article.data.size() > kMaximumDocumentBytes - input_size) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    "Article document exceeds the size limit");
        }
        input_size += article.data.size();
    }
    document.sanitized_html = NewDocument();
    for (const auto& article : articles) {
        document.sanitized_html += "<section class=\"gd-article\">";
        if (article.format == "text/html") {
            std::string sanitized;
            std::string plain;
            std::vector<ResourceReference> resources;
            if (SanitizeMarkup(dictionary, article.data, &sanitized, &plain,
                               &resources)) {
                document.sanitized_html += sanitized;
                document.plain_text += plain;
                for (auto& resource : resources) {
                    if (HasResource(document.resources, resource.dictionary_id,
                                    resource.resource_id)) {
                        continue;
                    }
                    if (document.resources.size() ==
                        kMaximumResourceReferences) {
                        throw dictionary::Error(
                            dictionary::ErrorCode::kInvalidData,
                            "Article contains too many resources");
                    }
                    document.resources.push_back(std::move(resource));
                }
            } else {
                document.sanitized_html +=
                    "<pre>" + Escape(article.data) + "</pre>";
                document.plain_text += article.data;
            }
        } else {
            document.sanitized_html += "<p>" + Escape(article.data) + "</p>";
            document.plain_text += article.data;
        }
        document.sanitized_html += "</section>";
        AddBreak(&document.plain_text);
    }
    while (!document.plain_text.empty() && document.plain_text.back() == '\n') {
        document.plain_text.pop_back();
    }
    FinishDocument(&document.sanitized_html);
    return document;
}

}  // namespace goldendict::core::article
