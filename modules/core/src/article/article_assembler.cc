// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_assembler.h"

#include <algorithm>
#include <array>
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
    "p",          "div",  "span", "h3",    "b",      "strong", "i",   "em",
    "u",          "br",   "ul",   "ol",    "li",     "dl",     "dt",  "dd",
    "blockquote", "code", "pre",  "table", "thead",  "tbody",  "tr",  "th",
    "td",         "a",    "img",  "audio", "source", "sub",    "sup", "font"};
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

bool IsAllowedDslClass(std::string_view tag, std::string_view value) {
    static const std::vector<std::string> kDivClasses = {
        "dsl_article", "dsl_headwords", "dsl_definition", "dsl_m",  "dsl_m0",
        "dsl_m1",      "dsl_m2",        "dsl_m3",         "dsl_m4", "dsl_m5",
        "dsl_m6",      "dsl_m7",        "dsl_m8",         "dsl_m9"};
    static const std::vector<std::string> kSpanClasses = {
        "dsl_p", "dsl_u",    "dsl_trn", "dsl_ex", "dsl_com", "dsl_trs",
        "dsl_t", "dsl_lang", "dsl_c",   "dsl_b",  "dsl_i"};
    return (tag == "div" && Contains(kDivClasses, value)) ||
           (tag == "span" && Contains(kSpanClasses, value)) ||
           (tag == "b" && value == "dsl_b") ||
           (tag == "i" && value == "dsl_i") ||
           (tag == "a" && value == "dsl_ref");
}

bool IsAllowedMdictClass(std::string_view tag, std::string_view value) {
    return tag == "div" && value == "mdict";
}

bool IsAllowedStardictClass(std::string_view tag, std::string_view value) {
    static const std::vector<std::string> kDivClasses = {
        "sdct_h", "sdct_m", "sdct_l", "sdct_g", "sdct_t", "sdct_y", "sdct_k",
        "sdct_w", "sdct_n", "sdct_r", "sdct_W", "sdct_P", "sdct_x"};
    static const std::vector<std::string> kSpanClasses = {
        "sdict_h_wav",  "xdxf_k",        "xdxf_ex_old",    "xdxf_ex_orig",
        "xdxf_ex_tran", "xdxf_ex_markd", "xdxf_ex_source", "xdxf_opt",
        "xdxf_abbr",    "xdxf_dtrn",     "xdxf_co_old",    "xdxf_gr_old",
        "xdxf_tr_old",  "xdxf_rref",     "xdxf_wav",       "xdxf_def"};
    return (tag == "div" && Contains(kDivClasses, value)) ||
           (tag == "span" && Contains(kSpanClasses, value)) ||
           (tag == "h3" && value == "sdct_headwords") ||
           (tag == "a" && (value == "xdxf_kref" || value == "xdxf_wav"));
}

bool EqualsAsciiCaseInsensitive(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(),
                      [](unsigned char lhs, unsigned char rhs) {
                          return std::tolower(lhs) == std::tolower(rhs);
                      });
}

bool StartsWithAsciiCaseInsensitive(std::string_view value,
                                    std::string_view prefix) {
    return value.size() >= prefix.size() &&
           EqualsAsciiCaseInsensitive(value.substr(0U, prefix.size()), prefix);
}

bool IsSafeExternalLink(std::string_view value) {
    constexpr std::array<std::string_view, 3U> kPrefixes = {
        "http://", "https://", "mailto:"};
    const auto prefix = std::find_if(kPrefixes.begin(), kPrefixes.end(),
                                     [value](std::string_view candidate) {
                                         return StartsWithAsciiCaseInsensitive(
                                             value, candidate);
                                     });
    if (prefix == kPrefixes.end() || value.size() == prefix->size()) {
        return false;
    }
    return std::none_of(value.begin(), value.end(),
                        [](unsigned char character) {
                            return character <= 0x20U || character == 0x7fU;
                        });
}

bool IsAllowedLegacyColor(std::string_view value) {
    if (value.empty() || value.size() > 64U) {
        return false;
    }
    if (value.front() == '#') {
        if (value.size() != 4U && value.size() != 7U) {
            return false;
        }
        return std::all_of(value.begin() + 1, value.end(), [](unsigned char c) {
            return std::isxdigit(c) != 0;
        });
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '-' || c == '_';
    });
}

std::string_view TrimAsciiSpace(std::string_view value) {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1U);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1U);
    }
    return value;
}

bool IsSafeCssFamily(std::string_view value) {
    return !value.empty() && value.size() <= 256U &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isalnum(character) != 0 || character >= 0x80U ||
                      character == ' ' || character == '-' ||
                      character == '_' || character == ',';
           });
}

bool IsSafeCssLength(std::string_view value, bool allow_size_keyword) {
    static const std::array<std::string_view, 9U> kSizeKeywords = {
        "xx-small", "x-small",  "small",   "medium", "large",
        "x-large",  "xx-large", "smaller", "larger"};
    if (allow_size_keyword &&
        std::any_of(kSizeKeywords.begin(), kSizeKeywords.end(),
                    [value](std::string_view item) {
                        return EqualsAsciiCaseInsensitive(value, item);
                    })) {
        return true;
    }
    std::string_view number = value;
    if (!number.empty() && number.back() == '%') {
        number.remove_suffix(1U);
    } else {
        constexpr std::array<std::string_view, 3U> kUnits = {"px", "pt", "em"};
        const auto unit = std::find_if(
            kUnits.begin(), kUnits.end(), [number](std::string_view candidate) {
                return number.size() > candidate.size() &&
                       EqualsAsciiCaseInsensitive(
                           number.substr(number.size() - candidate.size()),
                           candidate);
            });
        if (unit == kUnits.end()) {
            return false;
        }
        number.remove_suffix(unit->size());
    }
    if (!number.empty() && (number.front() == '-' || number.front() == '+')) {
        number.remove_prefix(1U);
    }
    bool decimal = false;
    bool digit = false;
    for (const unsigned char character : number) {
        if (character == '.' && !decimal) {
            decimal = true;
        } else if (std::isdigit(character) != 0) {
            digit = true;
        } else {
            return false;
        }
    }
    return digit;
}

bool IsSafePangoDeclaration(std::string_view property, std::string_view value) {
    if (property == "color" || property == "background-color" ||
        property == "text-decoration-color") {
        return IsAllowedLegacyColor(value);
    }
    if (property == "font-family") {
        return IsSafeCssFamily(value);
    }
    if (property == "font-size") {
        return IsSafeCssLength(value, true);
    }
    if (property == "vertical-align" || property == "letter-spacing") {
        return IsSafeCssLength(value, false);
    }
    const auto is_one_of =
        [value](std::initializer_list<std::string_view> items) {
            return std::any_of(
                items.begin(), items.end(), [value](std::string_view item) {
                    return EqualsAsciiCaseInsensitive(value, item);
                });
        };
    if (property == "font-style") {
        return is_one_of({"normal", "oblique", "italic"});
    }
    if (property == "font-weight") {
        return is_one_of({"normal", "bold", "100", "200", "300", "400", "500",
                          "600", "700", "800", "900", "1000"});
    }
    if (property == "font-variant") {
        return is_one_of({"normal", "small-caps"});
    }
    if (property == "font-stretch") {
        return is_one_of({"normal", "ultra-condensed", "extra-condensed",
                          "condensed", "semi-condensed", "semi-expanded",
                          "expanded", "extra-expanded", "ultra-expanded"});
    }
    if (property == "text-decoration-line") {
        return is_one_of({"none", "underline", "line-through"});
    }
    if (property == "text-decoration-style") {
        return is_one_of({"solid", "double", "dotted", "dashed", "wavy"});
    }
    return false;
}

std::optional<std::string> SafeInlineStyle(std::string_view value) {
    if (value.empty() || value.size() > 2048U) {
        return std::nullopt;
    }
    std::string safe;
    std::size_t position = 0U;
    while (position < value.size()) {
        const auto end = value.find(';', position);
        const auto declaration = TrimAsciiSpace(value.substr(
            position, end == std::string_view::npos ? value.size() - position
                                                    : end - position));
        if (!declaration.empty()) {
            const auto separator = declaration.find(':');
            if (separator == std::string_view::npos) {
                return std::nullopt;
            }
            std::string property(
                TrimAsciiSpace(declaration.substr(0U, separator)));
            std::transform(
                property.begin(), property.end(), property.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
            const auto property_value =
                TrimAsciiSpace(declaration.substr(separator + 1U));
            if (!IsSafePangoDeclaration(property, property_value)) {
                return std::nullopt;
            }
            safe += property + ":" + std::string(property_value) + ";";
        }
        if (end == std::string_view::npos) {
            break;
        }
        position = end + 1U;
    }
    return safe.empty() ? std::nullopt
                        : std::optional<std::string>(std::move(safe));
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
        } else if (entity == "nbsp" || entity == "#160" || entity == "#xa0" ||
                   entity == "#xA0") {
            decoded += "\xc2\xa0";
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
    if (tag.name == "br" || tag.name == "img" || tag.name == "link" ||
        tag.name == "source") {
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
    return tag == "p" || tag == "div" || tag == "h3" || tag == "li" ||
           tag == "dt" || tag == "dd" || tag == "blockquote" || tag == "pre" ||
           tag == "tr";
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

void AppendDocument(std::string_view value, std::string* document) {
    if (document->size() > kMaximumDocumentBytes ||
        value.size() > kMaximumDocumentBytes - document->size()) {
        throw dictionary::Error(
            dictionary::ErrorCode::kInvalidData,
            "Rendered article document exceeds the size limit");
    }
    document->append(value);
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
                const auto class_name = tag.attributes.find("class");
                if (class_name != tag.attributes.end() &&
                    (IsAllowedDslClass(tag.name, class_name->second) ||
                     IsAllowedMdictClass(tag.name, class_name->second) ||
                     IsAllowedStardictClass(tag.name, class_name->second))) {
                    html->append(" class=\"" + class_name->second + "\"");
                }
                const auto direction = tag.attributes.find("dir");
                if (direction != tag.attributes.end() &&
                    (EqualsAsciiCaseInsensitive(direction->second, "ltr") ||
                     EqualsAsciiCaseInsensitive(direction->second, "rtl"))) {
                    html->append(
                        EqualsAsciiCaseInsensitive(direction->second, "rtl")
                            ? " dir=\"rtl\""
                            : " dir=\"ltr\"");
                }
                if (tag.name == "a") {
                    const auto href = tag.attributes.find("href");
                    constexpr std::string_view kBword = "bword://";
                    constexpr std::string_view kBwordShort = "bword:";
                    constexpr std::string_view kEntry = "entry://";
                    if (href != tag.attributes.end() &&
                        (StartsWithAsciiCaseInsensitive(href->second, kBword) ||
                         StartsWithAsciiCaseInsensitive(href->second,
                                                        kBwordShort) ||
                         StartsWithAsciiCaseInsensitive(href->second, kEntry) ||
                         href->second.find(':') == std::string::npos)) {
                        try {
                            const std::size_t prefix_size =
                                StartsWithAsciiCaseInsensitive(href->second,
                                                               kBword)
                                    ? kBword.size()
                                : StartsWithAsciiCaseInsensitive(href->second,
                                                                 kBwordShort)
                                    ? kBwordShort.size()
                                : StartsWithAsciiCaseInsensitive(href->second,
                                                                 kEntry)
                                    ? kEntry.size()
                                    : 0U;
                            std::string_view target(href->second);
                            target.remove_prefix(prefix_size);
                            target = target.substr(0U, target.find('#'));
                            html->append(" href=\"" +
                                         Escape(MakeLookupUrl(target)) + "\"");
                        } catch (const std::invalid_argument&) {}
                    } else if (href != tag.attributes.end() &&
                               IsSafeExternalLink(href->second)) {
                        html->append(" href=\"" + Escape(href->second) + "\"");
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
                } else if (tag.name == "span" &&
                           class_name != tag.attributes.end() &&
                           class_name->second == "dsl_p") {
                    const auto title = tag.attributes.find("title");
                    if (title != tag.attributes.end()) {
                        html->append(" title=\"" + Escape(title->second) +
                                     "\"");
                    }
                } else if (tag.name == "span") {
                    const auto style = tag.attributes.find("style");
                    if (style != tag.attributes.end()) {
                        const auto safe_style = SafeInlineStyle(style->second);
                        if (safe_style.has_value()) {
                            html->append(" style=\"" + *safe_style + "\"");
                        }
                    }
                } else if (tag.name == "font") {
                    const auto color = tag.attributes.find("color");
                    if (color != tag.attributes.end() &&
                        IsAllowedLegacyColor(color->second)) {
                        html->append(" color=\"" + Escape(color->second) +
                                     "\"");
                    }
                } else if (tag.name == "img" || tag.name == "source" ||
                           tag.name == "audio") {
                    const auto append_resource_attribute =
                        [&dictionary, &tag, html,
                         resources](std::string_view attribute_name) {
                            const auto source = tag.attributes.find(
                                std::string(attribute_name));
                            if (source != tag.attributes.end()) {
                                const auto resource_id =
                                    NormalizeResourceId(source->second);
                                if (resource_id.has_value()) {
                                    html->append(
                                        " " + std::string(attribute_name) +
                                        "=\"" +
                                        Escape(MakeResourceUrl(dictionary.id,
                                                               *resource_id)) +
                                        "\"");
                                    AddResourceReference(
                                        dictionary, *resource_id, resources);
                                }
                            }
                        };
                    append_resource_attribute("src");
                    if (tag.name == "img") {
                        append_resource_attribute("losrc");
                        append_resource_attribute("hisrc");
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
                    (tag.attributes.find("controls") != tag.attributes.end() ||
                     tag.attributes.find("src") != tag.attributes.end())) {
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
            if (tag.name == "link" && suppressed_depth == 0) {
                const auto relationship = tag.attributes.find("rel");
                const auto href = tag.attributes.find("href");
                if (relationship != tag.attributes.end() &&
                    href != tag.attributes.end() &&
                    EqualsAsciiCaseInsensitive(relationship->second,
                                               "stylesheet")) {
                    const auto resource_id = NormalizeResourceId(href->second);
                    if (resource_id.has_value()) {
                        html->append("<link rel=\"stylesheet\" href=\"" +
                                     Escape(MakeResourceUrl(dictionary.id,
                                                            *resource_id)) +
                                     "\">");
                        AddResourceReference(dictionary, *resource_id,
                                             resources);
                    }
                }
            } else if (tag.name == "script" && suppressed_depth == 1) {
                const auto source = tag.attributes.find("src");
                if (source != tag.attributes.end()) {
                    const auto resource_id =
                        NormalizeResourceId(source->second);
                    if (resource_id.has_value()) {
                        html->append(
                            "<script type=\"application/x-goldendict-inert\" "
                            "src=\"" +
                            Escape(
                                MakeResourceUrl(dictionary.id, *resource_id)) +
                            "\"></script>");
                        AddResourceReference(dictionary, *resource_id,
                                             resources);
                    }
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
    if (document.sanitized_html.size() > kMaximumDocumentBytes) {
        throw dictionary::Error(
            dictionary::ErrorCode::kInvalidData,
            "Rendered article document exceeds the size limit");
    }
    for (const auto& article : articles) {
        AppendDocument("<section class=\"gd-article\">",
                       &document.sanitized_html);
        if (article.format == "text/html") {
            std::string sanitized;
            std::string plain;
            std::vector<ResourceReference> resources;
            if (SanitizeMarkup(dictionary, article.data, &sanitized, &plain,
                               &resources)) {
                AppendDocument(sanitized, &document.sanitized_html);
                if (plain.size() >
                    kMaximumDocumentBytes - document.plain_text.size()) {
                    throw dictionary::Error(
                        dictionary::ErrorCode::kInvalidData,
                        "Rendered article text exceeds the size limit");
                }
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
                AppendDocument("<pre>", &document.sanitized_html);
                AppendDocument(Escape(article.data), &document.sanitized_html);
                AppendDocument("</pre>", &document.sanitized_html);
                if (article.data.size() >
                    kMaximumDocumentBytes - document.plain_text.size()) {
                    throw dictionary::Error(
                        dictionary::ErrorCode::kInvalidData,
                        "Rendered article text exceeds the size limit");
                }
                document.plain_text += article.data;
            }
        } else {
            AppendDocument("<p>", &document.sanitized_html);
            AppendDocument(Escape(article.data), &document.sanitized_html);
            AppendDocument("</p>", &document.sanitized_html);
            if (article.data.size() >
                kMaximumDocumentBytes - document.plain_text.size()) {
                throw dictionary::Error(
                    dictionary::ErrorCode::kInvalidData,
                    "Rendered article text exceeds the size limit");
            }
            document.plain_text += article.data;
        }
        AppendDocument("</section>", &document.sanitized_html);
        AddBreak(&document.plain_text);
    }
    while (!document.plain_text.empty() && document.plain_text.back() == '\n') {
        document.plain_text.pop_back();
    }
    FinishDocument(&document.sanitized_html);
    if (document.sanitized_html.size() > kMaximumDocumentBytes) {
        throw dictionary::Error(
            dictionary::ErrorCode::kInvalidData,
            "Rendered article document exceeds the size limit");
    }
    return document;
}

}  // namespace goldendict::core::article
