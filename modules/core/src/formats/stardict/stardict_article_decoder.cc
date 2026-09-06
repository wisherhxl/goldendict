// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_article_decoder.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <expat.h>
#include <unicode/uchar.h>
#include <unicode/ucnv.h>

#include "../../foundation/text_encoding.h"

namespace goldendict::core::formats::stardict {
namespace {

constexpr std::size_t kMaximumRenderedArticleBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumXmlNodes = 65536U;
constexpr std::size_t kMaximumXmlDepth = 128U;
constexpr std::size_t kCheckpointInterval = 4096U;
constexpr std::size_t kXmlParseChunkBytes = 64U * 1024U;

class DecodeControl final {
   public:
    explicit DecodeControl(const ArticleDecodeCheckpoint& checkpoint)
        : checkpoint_(checkpoint) {}

    void Advance(std::size_t amount) {
        if (!checkpoint_) {
            return;
        }
        work_since_checkpoint_ += amount;
        if (work_since_checkpoint_ >= kCheckpointInterval) {
            work_since_checkpoint_ %= kCheckpointInterval;
            checkpoint_();
        }
    }

    void Check() const {
        if (checkpoint_) {
            checkpoint_();
        }
    }

   private:
    const ArticleDecodeCheckpoint& checkpoint_;
    std::size_t work_since_checkpoint_ = 0U;
};

std::size_t FindCharacter(std::string_view value, char needle,
                          std::size_t start, DecodeControl* control) {
    for (std::size_t index = start; index < value.size(); ++index) {
        if (control != nullptr) {
            control->Advance(1U);
        }
        if (value[index] == needle) {
            return index;
        }
    }
    return std::string_view::npos;
}

bool IsAsciiLower(char value) {
    return value >= 'a' && value <= 'z';
}

bool IsAsciiUpper(char value) {
    return value >= 'A' && value <= 'Z';
}

bool IsAsciiSpace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\f' || value == '\v';
}

std::uint32_t ReadBigEndian32(std::string_view data, std::size_t offset) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset]))
            << 24U) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(data[offset + 1U]))
            << 16U) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(data[offset + 2U]))
            << 8U) |
           static_cast<std::uint32_t>(
               static_cast<unsigned char>(data[offset + 3U]));
}

class BoundedText final {
   public:
    explicit BoundedText(DecodeControl* control = nullptr)
        : control_(control) {}

    void Append(std::string_view value) {
        if (control_ != nullptr) {
            control_->Advance(value.size());
        }
        if (value.size() > kMaximumRenderedArticleBytes - value_.size()) {
            throw ArticleDecodeError(
                "Rendered StarDict article exceeds the supported size limit");
        }
        value_.append(value);
    }

    void PushBack(char value) {
        if (control_ != nullptr) {
            control_->Advance(1U);
        }
        if (value_.size() == kMaximumRenderedArticleBytes) {
            throw ArticleDecodeError(
                "Rendered StarDict article exceeds the supported size limit");
        }
        value_.push_back(value);
    }

    std::string Take() { return std::move(value_); }

    DecodeControl* control() const noexcept { return control_; }

   private:
    DecodeControl* control_;
    std::string value_;
};

std::string EscapeHtml(std::string_view value,
                       DecodeControl* control = nullptr) {
    BoundedText output(control);
    for (const char character : value) {
        switch (character) {
            case '&':
                output.Append("&amp;");
                break;
            case '<':
                output.Append("&lt;");
                break;
            case '>':
                output.Append("&gt;");
                break;
            case '"':
                output.Append("&quot;");
                break;
            default:
                output.PushBack(character);
        }
    }
    return output.Take();
}

std::string EscapeAttribute(std::string_view value,
                            DecodeControl* control = nullptr) {
    BoundedText output(control);
    for (const char character : value) {
        switch (character) {
            case '&':
                output.Append("&amp;");
                break;
            case '<':
                output.Append("&lt;");
                break;
            case '>':
                output.Append("&gt;");
                break;
            case '"':
                output.Append("&quot;");
                break;
            case '\'':
                output.Append("&#39;");
                break;
            default:
                output.PushBack(character);
        }
    }
    return output.Take();
}

bool IsRightToLeftLanguage(std::string_view language) {
    constexpr std::array<std::string_view, 11U> kRightToLeftLanguages = {
        "ar", "dv", "fa", "he", "ku", "ps", "sd", "syr", "ug", "ur", "yi"};
    return std::find(kRightToLeftLanguages.begin(), kRightToLeftLanguages.end(),
                     language) != kRightToLeftLanguages.end();
}

bool IsRightToLeftText(std::string_view text) {
    for (std::size_t index = 0U; index < text.size();) {
        if (text[index] == '&') {
            const auto entity_end = text.find(';', index + 1U);
            if (entity_end != std::string_view::npos &&
                entity_end - index <= 10U) {
                index = entity_end + 1U;
                continue;
            }
        }
        const unsigned char first = static_cast<unsigned char>(text[index]);
        std::uint32_t code_point = first;
        std::size_t length = 1U;
        if ((first & 0xe0U) == 0xc0U) {
            code_point = first & 0x1fU;
            length = 2U;
        } else if ((first & 0xf0U) == 0xe0U) {
            code_point = first & 0x0fU;
            length = 3U;
        } else if ((first & 0xf8U) == 0xf0U) {
            code_point = first & 0x07U;
            length = 4U;
        }
        if (length > text.size() - index) {
            ++index;
            continue;
        }
        for (std::size_t byte = 1U; byte < length; ++byte) {
            const unsigned char continuation =
                static_cast<unsigned char>(text[index + byte]);
            if ((continuation & 0xc0U) != 0x80U) {
                length = 1U;
                code_point = first;
                break;
            }
            code_point = (code_point << 6U) | (continuation & 0x3fU);
        }
        const UCharDirection direction = u_charDirection(code_point);
        if (direction == U_LEFT_TO_RIGHT) {
            return false;
        }
        if (direction == U_RIGHT_TO_LEFT ||
            direction == U_RIGHT_TO_LEFT_ARABIC) {
            return true;
        }
        index += length;
    }
    return false;
}

std::string Preformat(std::string_view value, bool base_right_to_left,
                      DecodeControl* control) {
    const std::size_t embedded_nul = FindCharacter(value, '\0', 0U, control);
    if (embedded_nul != std::string_view::npos) {
        value = value.substr(0U, embedded_nul);
    }
    const std::string escaped = EscapeHtml(value, control);
    BoundedText output(control);
    std::string line;
    line.reserve(escaped.size());
    bool leading = true;
    const auto append_line = [&output,
                              base_right_to_left](const std::string& current) {
        output.Append("<div");
        const bool line_right_to_left = IsRightToLeftText(current);
        if (line_right_to_left != base_right_to_left) {
            output.Append(base_right_to_left ? " dir=\"ltr\"" : " dir=\"rtl\"");
        }
        output.Append(">");
        output.Append(current);
        output.Append("</div>");
    };
    for (const char character : escaped) {
        if (control != nullptr) {
            control->Advance(1U);
        }
        if (leading && character == ' ') {
            line += "&nbsp;";
            continue;
        }
        if (leading && character == '\t') {
            line += "&nbsp;&nbsp;&nbsp;&nbsp;";
            continue;
        }
        if (character == '\n') {
            append_line(line);
            line.clear();
            leading = true;
            continue;
        }
        if (character == '\r') {
            continue;
        }
        line.push_back(character);
        leading = false;
    }
    if (!line.empty()) {
        append_line(line);
    }
    return output.Take();
}

std::string DecodeLocalText(std::string_view value, DecodeControl* control) {
    if (control != nullptr) {
        control->Check();
    }
    const char* encoding = ucnv_getDefaultName();
    if (encoding == nullptr || *encoding == '\0') {
        return std::string(value);
    }
    try {
        std::string decoded = foundation::DecodeToUtf8(
            value, encoding, kMaximumRenderedArticleBytes);
        if (control != nullptr) {
            control->Check();
        }
        return decoded;
    } catch (const foundation::TextEncodingError&) {
        return std::string(value);
    }
}

struct XmlNode;

struct XmlContent {
    std::string text;
    std::unique_ptr<XmlNode> child;
};

struct XmlNode {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::vector<XmlContent> content;
};

struct XmlState {
    XML_Parser parser = nullptr;
    std::unique_ptr<XmlNode> root;
    std::vector<XmlNode*> stack;
    std::size_t node_count = 0U;
    std::size_t text_bytes = 0U;
    bool failed = false;
};

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

void FailXml(XmlState* state) {
    if (!state->failed) {
        state->failed = true;
        XML_StopParser(state->parser, XML_FALSE);
    }
}

void StartXmlElement(void* user_data, const XML_Char* raw_name,
                     const XML_Char** attributes) {
    auto* state = static_cast<XmlState*>(user_data);
    if (state->stack.size() == kMaximumXmlDepth ||
        state->node_count == kMaximumXmlNodes) {
        FailXml(state);
        return;
    }
    auto node = std::make_unique<XmlNode>();
    node->name = LowerAscii(raw_name);
    if (attributes != nullptr) {
        for (std::size_t index = 0U; attributes[index] != nullptr;
             index += 2U) {
            node->attributes.emplace(LowerAscii(attributes[index]),
                                     attributes[index + 1U]);
        }
    }
    XmlNode* pointer = node.get();
    if (state->stack.empty()) {
        state->root = std::move(node);
    } else {
        state->stack.back()->content.push_back({{}, std::move(node)});
    }
    state->stack.push_back(pointer);
    ++state->node_count;
}

void EndXmlElement(void* user_data, const XML_Char*) {
    auto* state = static_cast<XmlState*>(user_data);
    if (state->stack.empty()) {
        FailXml(state);
        return;
    }
    state->stack.pop_back();
}

void XmlCharacterData(void* user_data, const XML_Char* data, int length) {
    auto* state = static_cast<XmlState*>(user_data);
    if (state->stack.empty() || length < 0) {
        FailXml(state);
        return;
    }
    const auto size = static_cast<std::size_t>(length);
    if (size > kMaximumRenderedArticleBytes - state->text_bytes) {
        FailXml(state);
        return;
    }
    state->text_bytes += size;
    auto& content = state->stack.back()->content;
    if (!content.empty() && !content.back().child) {
        content.back().text.append(data, size);
    } else {
        content.push_back({std::string(data, size), nullptr});
    }
}

std::unique_ptr<XmlNode> ParseXmlFragment(std::string_view value,
                                          DecodeControl* control) {
    BoundedText document(control);
    document.Append("<gdroot>");
    document.Append(value);
    document.Append("</gdroot>");
    const std::string wrapped = document.Take();

    XmlState state;
    state.parser = XML_ParserCreate("UTF-8");
    if (state.parser == nullptr) {
        throw ArticleDecodeError("Cannot initialize StarDict XML decoder");
    }
    XML_SetUserData(state.parser, &state);
    XML_SetElementHandler(state.parser, StartXmlElement, EndXmlElement);
    XML_SetCharacterDataHandler(state.parser, XmlCharacterData);
    XML_SetParamEntityParsing(state.parser, XML_PARAM_ENTITY_PARSING_NEVER);
    XML_Status status = XML_STATUS_OK;
    std::size_t offset = 0U;
    while (offset < wrapped.size() && status == XML_STATUS_OK) {
        if (control != nullptr) {
            control->Check();
        }
        const std::size_t chunk_size =
            std::min(kXmlParseChunkBytes, wrapped.size() - offset);
        const bool final_chunk = offset + chunk_size == wrapped.size();
        status = XML_Parse(state.parser, wrapped.data() + offset,
                           static_cast<int>(chunk_size), final_chunk);
        offset += chunk_size;
    }
    XML_ParserFree(state.parser);
    state.parser = nullptr;
    if (status != XML_STATUS_OK || state.failed || !state.root ||
        !state.stack.empty()) {
        return nullptr;
    }
    return std::move(state.root);
}

std::string PlainXmlText(const XmlNode& node, DecodeControl* control) {
    BoundedText output(control);
    for (const auto& item : node.content) {
        output.Append(item.child ? PlainXmlText(*item.child, control)
                                 : item.text);
    }
    return output.Take();
}

bool HasSuffixCaseInsensitive(std::string_view value, std::string_view suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    value.remove_prefix(value.size() - suffix.size());
    return std::equal(value.begin(), value.end(), suffix.begin(),
                      [](unsigned char left, unsigned char right) {
                          return std::tolower(left) == std::tolower(right);
                      });
}

bool EqualsAsciiCaseInsensitive(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(),
                      [](unsigned char lhs, unsigned char rhs) {
                          return std::tolower(lhs) == std::tolower(rhs);
                      });
}

std::size_t FindAsciiCaseInsensitive(std::string_view value,
                                     std::string_view needle, std::size_t start,
                                     DecodeControl* control) {
    if (needle.empty() || start > value.size() ||
        needle.size() > value.size() - start) {
        return std::string_view::npos;
    }
    for (std::size_t index = start; index + needle.size() <= value.size();
         ++index) {
        if (control != nullptr) {
            control->Advance(1U);
        }
        if (EqualsAsciiCaseInsensitive(value.substr(index, needle.size()),
                                       needle)) {
            return index;
        }
    }
    return std::string_view::npos;
}

std::string ConvertHtmlAudio(std::string_view value, DecodeControl* control) {
    BoundedText output(control);
    std::size_t cursor = 0U;
    while (cursor < value.size()) {
        const auto opening =
            FindAsciiCaseInsensitive(value, "<audio", cursor, control);
        if (opening == std::string_view::npos) {
            output.Append(value.substr(cursor));
            break;
        }
        output.Append(value.substr(cursor, opening - cursor));
        std::size_t index = opening + 6U;
        while (index < value.size() && IsAsciiSpace(value[index])) {
            ++index;
        }
        if (index + 3U > value.size() ||
            !EqualsAsciiCaseInsensitive(value.substr(index, 3U), "src")) {
            output.Append(value.substr(opening, 6U));
            cursor = opening + 6U;
            continue;
        }
        index += 3U;
        while (index < value.size() && IsAsciiSpace(value[index])) {
            ++index;
        }
        if (index == value.size() || value[index++] != '=') {
            output.Append(value.substr(opening, 6U));
            cursor = opening + 6U;
            continue;
        }
        while (index < value.size() && IsAsciiSpace(value[index])) {
            ++index;
        }
        if (index == value.size() ||
            (value[index] != '\'' && value[index] != '"')) {
            output.Append(value.substr(opening, 6U));
            cursor = opening + 6U;
            continue;
        }
        const char quote = value[index++];
        const auto source_end = FindCharacter(value, quote, index, control);
        if (source_end == std::string_view::npos) {
            output.Append(value.substr(opening));
            break;
        }
        const std::string_view source = value.substr(index, source_end - index);
        index = source_end + 1U;
        while (index < value.size() && IsAsciiSpace(value[index])) {
            ++index;
        }
        if (index == value.size() || value[index] != '>' ||
            source.find("://") != std::string_view::npos) {
            output.Append(value.substr(opening, index - opening));
            cursor = index;
            continue;
        }
        const auto closing =
            FindAsciiCaseInsensitive(value, "</audio>", index + 1U, control);
        if (closing == std::string_view::npos) {
            output.Append(value.substr(opening));
            break;
        }
        output.Append("<span class=\"sdict_h_wav\"><a href=\"sound://");
        output.Append(EscapeAttribute(source, control));
        output.Append("\">");
        output.Append(value.substr(index + 1U, closing - index - 1U));
        output.Append(" ");
        output.Append("</a></span>");
        cursor = closing + 8U;
    }
    return output.Take();
}

bool IsPictureName(std::string_view value) {
    return HasSuffixCaseInsensitive(value, ".bmp") ||
           HasSuffixCaseInsensitive(value, ".gif") ||
           HasSuffixCaseInsensitive(value, ".ico") ||
           HasSuffixCaseInsensitive(value, ".jpe") ||
           HasSuffixCaseInsensitive(value, ".jpeg") ||
           HasSuffixCaseInsensitive(value, ".jpg") ||
           HasSuffixCaseInsensitive(value, ".png") ||
           HasSuffixCaseInsensitive(value, ".svg") ||
           HasSuffixCaseInsensitive(value, ".tif") ||
           HasSuffixCaseInsensitive(value, ".tiff") ||
           HasSuffixCaseInsensitive(value, ".tga") ||
           HasSuffixCaseInsensitive(value, ".pcx") ||
           HasSuffixCaseInsensitive(value, ".webp");
}

bool IsSoundName(std::string_view value) {
    return HasSuffixCaseInsensitive(value, ".aac") ||
           HasSuffixCaseInsensitive(value, ".ape") ||
           HasSuffixCaseInsensitive(value, ".au") ||
           HasSuffixCaseInsensitive(value, ".flac") ||
           HasSuffixCaseInsensitive(value, ".kar") ||
           HasSuffixCaseInsensitive(value, ".m4a") ||
           HasSuffixCaseInsensitive(value, ".mid") ||
           HasSuffixCaseInsensitive(value, ".mp2") ||
           HasSuffixCaseInsensitive(value, ".mp3") ||
           HasSuffixCaseInsensitive(value, ".mpa") ||
           HasSuffixCaseInsensitive(value, ".mpc") ||
           HasSuffixCaseInsensitive(value, ".oga") ||
           HasSuffixCaseInsensitive(value, ".ogg") ||
           HasSuffixCaseInsensitive(value, ".opus") ||
           HasSuffixCaseInsensitive(value, ".spx") ||
           HasSuffixCaseInsensitive(value, ".voc") ||
           HasSuffixCaseInsensitive(value, ".wav") ||
           HasSuffixCaseInsensitive(value, ".wma") ||
           HasSuffixCaseInsensitive(value, ".wv");
}

std::optional<std::string_view> Attribute(const XmlNode& node,
                                          std::string_view name) {
    const auto found = node.attributes.find(std::string(name));
    if (found == node.attributes.end()) {
        return std::nullopt;
    }
    return found->second;
}

void RenderXmlChildren(const XmlNode& node, BoundedText* output);

void RenderWrapped(const XmlNode& node, std::string_view tag,
                   std::string_view attributes, BoundedText* output) {
    output->Append("<");
    output->Append(tag);
    output->Append(attributes);
    output->Append(">");
    RenderXmlChildren(node, output);
    output->Append("</");
    output->Append(tag);
    output->Append(">");
}

void RenderXdxfNode(const XmlNode& node, BoundedText* output) {
    if (node.name == "gdroot") {
        RenderXmlChildren(node, output);
    } else if (node.name == "k") {
        RenderWrapped(node, "span", " class=\"xdxf_k\"", output);
    } else if (node.name == "ex") {
        output->Append("<span class=\"xdxf_ex_old\">");
        RenderXmlChildren(node, output);
        const auto author = Attribute(node, "author");
        const auto source = Attribute(node, "source");
        if ((!author.value_or(std::string_view{}).empty() ||
             !source.value_or(std::string_view{}).empty()) &&
            !node.content.empty()) {
            output->Append(" <span class=\"xdxf_ex_source\">");
            if (author.has_value()) {
                output->Append(EscapeHtml(*author, output->control()));
            }
            if (source.has_value() && !source->empty()) {
                if (author.has_value() && !author->empty()) {
                    output->Append(", ");
                }
                output->Append(EscapeHtml(*source, output->control()));
            }
            output->Append("</span>");
        }
        output->Append("</span>");
    } else if (node.name == "ex_orig") {
        RenderWrapped(node, "span", " class=\"xdxf_ex_orig\"", output);
    } else if (node.name == "ex_tran") {
        RenderWrapped(node, "span", " class=\"xdxf_ex_tran\"", output);
    } else if (node.name == "mrkd") {
        RenderWrapped(node, "span", " class=\"xdxf_ex_markd\"", output);
    } else if (node.name == "opt") {
        RenderWrapped(node, "span", " class=\"xdxf_opt\"", output);
    } else if (node.name == "kref") {
        const std::string target = PlainXmlText(node, output->control());
        output->Append("<a class=\"xdxf_kref\" href=\"bword://");
        output->Append(EscapeAttribute(target, output->control()));
        if (const auto id = Attribute(node, "idref"); id.has_value()) {
            output->Append("#");
            output->Append(EscapeAttribute(*id, output->control()));
        }
        output->Append("\">");
        RenderXmlChildren(node, output);
        output->Append("</a>");
        if (const auto comment = Attribute(node, "kcmt"); comment.has_value()) {
            output->Append(" ");
            output->Append(EscapeHtml(*comment, output->control()));
        }
    } else if (node.name == "iref") {
        const auto href = Attribute(node, "href");
        const std::string target = href.has_value() && !href->empty()
                                       ? std::string(*href)
                                       : PlainXmlText(node, output->control());
        output->Append("<a href=\"");
        output->Append(EscapeAttribute(target, output->control()));
        output->Append("\">");
        RenderXmlChildren(node, output);
        output->Append("</a>");
    } else if (node.name == "abr" || node.name == "abbr") {
        RenderWrapped(node, "span", " class=\"xdxf_abbr\"", output);
    } else if (node.name == "dtrn") {
        RenderWrapped(node, "span", " class=\"xdxf_dtrn\"", output);
    } else if (node.name == "co") {
        RenderWrapped(node, "span", " class=\"xdxf_co_old\"", output);
    } else if (node.name == "gr" || node.name == "pos" ||
               node.name == "tense") {
        RenderWrapped(node, "span", " class=\"xdxf_gr_old\"", output);
    } else if (node.name == "tr") {
        RenderWrapped(node, "span", " class=\"xdxf_tr_old\"", output);
    } else if (node.name == "c") {
        const std::string color = Attribute(node, "c").has_value()
                                      ? std::string(*Attribute(node, "c"))
                                      : "blue";
        output->Append("<font color=\"");
        output->Append(EscapeAttribute(color, output->control()));
        output->Append("\">");
        RenderXmlChildren(node, output);
        output->Append("</font>");
    } else if (node.name == "img") {
        bool has_source = false;
        for (const std::string_view attribute_name :
             {"src", "hisrc", "losrc"}) {
            if (Attribute(node, attribute_name).has_value()) {
                has_source = true;
                break;
            }
        }
        if (has_source) {
            output->Append("<img");
            for (const std::string_view attribute_name :
                 {"src", "hisrc", "losrc"}) {
                if (const auto source = Attribute(node, attribute_name);
                    source.has_value()) {
                    output->Append(" ");
                    output->Append(attribute_name);
                    output->Append("=\"");
                    output->Append(EscapeAttribute(*source, output->control()));
                    output->Append("\"");
                }
            }
            if (const auto alt = Attribute(node, "alt"); alt.has_value()) {
                output->Append(" alt=\"");
                output->Append(EscapeAttribute(*alt, output->control()));
                output->Append("\"");
            }
            output->Append(">");
        }
    } else if (node.name == "rref") {
        if (Attribute(node, "start").has_value()) {
            RenderWrapped(node, "span", " class=\"xdxf_rref\"", output);
        } else {
            const std::string resource = PlainXmlText(node, output->control());
            if (IsPictureName(resource)) {
                output->Append("<img src=\"");
                output->Append(EscapeAttribute(resource, output->control()));
                output->Append("\" alt=\"");
                output->Append(EscapeAttribute(resource, output->control()));
                output->Append("\">");
            } else if (IsSoundName(resource)) {
                output->Append("<a class=\"xdxf_wav\" href=\"sound://");
                output->Append(EscapeAttribute(resource, output->control()));
                output->Append("\"></a>");
            } else {
                RenderWrapped(node, "span", " class=\"xdxf_rref\"", output);
            }
        }
    } else if (node.name == "gdnbsp") {
        output->Append("&nbsp;");
    } else if (node.name == "br") {
        output->Append("<br>");
    } else if (node.name == "b" || node.name == "strong" || node.name == "i" ||
               node.name == "em" || node.name == "u" || node.name == "p" ||
               node.name == "blockquote" || node.name == "sub" ||
               node.name == "sup") {
        RenderWrapped(node, node.name, {}, output);
    } else {
        RenderXmlChildren(node, output);
    }
}

void RenderXmlChildren(const XmlNode& node, BoundedText* output) {
    for (const auto& item : node.content) {
        if (item.child) {
            RenderXdxfNode(*item.child, output);
        } else {
            output->Append(EscapeHtml(item.text, output->control()));
        }
    }
}

std::string PrepareVisualXdxf(std::string_view value, DecodeControl* control) {
    BoundedText converted(control);
    bool after_newline = false;
    for (const char character : value) {
        if (character == '\r') {
            continue;
        }
        if (character == '\n') {
            converted.Append("<br/>");
            after_newline = true;
            continue;
        }
        if (after_newline && character == ' ') {
            converted.Append("<gdnbsp/>");
            continue;
        }
        converted.PushBack(character);
        after_newline = false;
    }
    return converted.Take();
}

std::string ConvertXdxf(std::string_view value, DecodeControl* control) {
    const std::string visual = PrepareVisualXdxf(value, control);
    auto root = ParseXmlFragment(visual, control);
    if (!root) {
        return std::string(value);
    }
    BoundedText output(control);
    output.Append("<div class=\"sdct_x\">");
    for (const auto& item : root->content) {
        if (item.child) {
            output.Append(" ");
            RenderXdxfNode(*item.child, &output);
        } else {
            output.Append(EscapeHtml(item.text, control));
        }
    }
    output.Append("</div>");
    return output.Take();
}

void CollectXmlText(const XmlNode& node, std::vector<std::string>* values,
                    DecodeControl* control) {
    for (const auto& item : node.content) {
        if (item.child) {
            CollectXmlText(*item.child, values, control);
        } else {
            if (control != nullptr) {
                control->Advance(item.text.size());
            }
            values->push_back(item.text);
        }
    }
}

std::string TranslatePowerWord(std::string_view value, DecodeControl* control) {
    std::string current(value);
    BoundedText translated(control);
    for (std::size_t index = 0U; index < current.size();) {
        if (current[index] != '&' || index + 2U >= current.size()) {
            translated.PushBack(current[index++]);
            continue;
        }
        const char code = static_cast<char>(
            std::tolower(static_cast<unsigned char>(current[index + 1U])));
        std::size_t brace = index + 2U;
        while (brace < current.size() && IsAsciiSpace(current[brace])) {
            if (control != nullptr) {
                control->Advance(1U);
            }
            ++brace;
        }
        if (brace == current.size() || current[brace] != '{') {
            translated.PushBack(current[index++]);
            continue;
        }
        std::size_t end = brace + 1U;
        while (end < current.size() && current[end] != '{' &&
               current[end] != '}' && current[end] != '&') {
            if (control != nullptr) {
                control->Advance(1U);
            }
            ++end;
        }
        if (end < current.size() && current[end] == '}' && end > brace + 1U &&
            (code == 'b' || code == 'i' || code == 'u' || code == 'l' ||
             code == '2')) {
            const std::string_view body(current.data() + brace + 1U,
                                        end - brace - 1U);
            if (code == 'b') {
                translated.Append("<b>");
                translated.Append(body);
                translated.Append("</b>");
            } else if (code == 'i') {
                translated.Append("<i>");
                translated.Append(body);
                translated.Append("</i>");
            } else if (code == 'u') {
                translated.Append("<u>");
                translated.Append(body);
                translated.Append("</u>");
            } else {
                translated.Append("<font color=\"blue\">");
                translated.Append(body);
                translated.Append("</font>");
            }
            index = end + 1U;
            continue;
        }
        translated.PushBack(current[index++]);
    }
    current = translated.Take();

    BoundedText output(control);
    for (std::size_t index = 0U; index < current.size();) {
        if (current[index] == '}') {
            ++index;
            continue;
        }
        if (current[index] == '&' && index + 2U < current.size()) {
            std::size_t brace = index + 2U;
            while (brace < current.size() && IsAsciiSpace(current[brace])) {
                ++brace;
            }
            if (brace < current.size() && current[brace] == '{') {
                index = brace + 1U;
                continue;
            }
        }
        output.PushBack(current[index++]);
    }
    return output.Take();
}

std::string ConvertPowerWord(std::string_view value, DecodeControl* control) {
    BoundedText output(control);
    output.Append("<div class=\"sdct_k\">");
    auto root = ParseXmlFragment(value, control);
    if (!root) {
        output.Append(value);
    } else {
        std::vector<std::string> text;
        CollectXmlText(*root, &text, control);
        for (const auto& line : text) {
            output.Append(TranslatePowerWord(line, control));
            output.Append("<br>");
        }
    }
    output.Append("</div>");
    return output.Take();
}

bool IsSafeColor(std::string_view value) {
    if (value.empty() || value.size() > 64U) {
        return false;
    }
    if (value.front() == '#') {
        if (value.size() != 4U && value.size() != 7U) {
            return false;
        }
        return std::all_of(value.begin() + 1, value.end(),
                           [](unsigned char character) {
                               return std::isxdigit(character) != 0;
                           });
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) != 0 || character == '-' ||
               character == '_';
    });
}

bool IsSafeCssFamily(std::string_view value) {
    return !value.empty() && value.size() <= 256U &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isalnum(character) != 0 || character >= 0x80U ||
                      character == ' ' || character == '-' ||
                      character == '_' || character == ',';
           });
}

bool ParseInteger(std::string_view value, long long* result) {
    if (value.empty()) {
        return false;
    }
    std::size_t index = value.front() == '-' || value.front() == '+' ? 1U : 0U;
    if (index == value.size()) {
        return false;
    }
    long long parsed = 0;
    for (; index < value.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(value[index]);
        if (std::isdigit(character) == 0) {
            return false;
        }
        const int digit = character - '0';
        if (parsed > (std::numeric_limits<long long>::max() - digit) / 10LL) {
            return false;
        }
        parsed = parsed * 10LL + digit;
    }
    *result = value.front() == '-' ? -parsed : parsed;
    return true;
}

bool HasCssLengthSuffix(std::string_view value) {
    return HasSuffixCaseInsensitive(value, "px") ||
           HasSuffixCaseInsensitive(value, "pt") ||
           HasSuffixCaseInsensitive(value, "em") ||
           HasSuffixCaseInsensitive(value, "%");
}

std::optional<std::string> PangoLength(std::string_view value,
                                       bool allow_keywords) {
    if (value.empty() || value.size() > 64U) {
        return std::nullopt;
    }
    const auto numeric_prefix = value.back() == '%'
                                    ? value.substr(0U, value.size() - 1U)
                                    : value.substr(0U, value.size() - 2U);
    long long numeric = 0;
    if (HasCssLengthSuffix(value) && ParseInteger(numeric_prefix, &numeric)) {
        return std::string(value);
    }
    if (allow_keywords) {
        static const std::array<std::string_view, 9U> kKeywords = {
            "xx-small", "x-small",  "small",   "medium", "large",
            "x-large",  "xx-large", "smaller", "larger"};
        const auto keyword = std::find_if(
            kKeywords.begin(), kKeywords.end(), [value](std::string_view item) {
                return EqualsAsciiCaseInsensitive(value, item);
            });
        if (keyword != kKeywords.end()) {
            return std::string(*keyword);
        }
    }
    if (!ParseInteger(value, &numeric) || numeric == 0LL) {
        return std::nullopt;
    }
    std::ostringstream converted;
    converted << std::fixed << std::setprecision(3)
              << static_cast<double>(numeric) / 1024.0 << "pt";
    return converted.str();
}

std::optional<std::string> PangoWeight(std::string_view value) {
    static const std::map<std::string_view, std::string_view> kWeights = {
        {"ultralight", "100"}, {"light", "200"},     {"normal", "normal"},
        {"bold", "bold"},      {"ultrabold", "800"}, {"heavy", "900"}};
    const auto found = std::find_if(
        kWeights.begin(), kWeights.end(), [value](const auto& item) {
            return EqualsAsciiCaseInsensitive(value, item.first);
        });
    if (found != kWeights.end()) {
        return std::string(found->second);
    }
    long long numeric = 0;
    if (ParseInteger(value, &numeric) && numeric >= 100LL &&
        numeric <= 1000LL) {
        return std::to_string(numeric);
    }
    return std::nullopt;
}

void AppendPangoDeclaration(std::string_view property, std::string_view value,
                            BoundedText* style) {
    style->Append(property);
    style->Append(":");
    style->Append(value);
    style->Append(";");
}

void AppendOptionalPangoDeclaration(std::string_view property,
                                    const std::optional<std::string>& value,
                                    BoundedText* style) {
    if (value.has_value()) {
        AppendPangoDeclaration(property, std::string_view(*value), style);
    }
}

std::optional<std::string> PangoKeyword(
    std::string_view value,
    std::initializer_list<std::string_view> allowed_values) {
    const auto found =
        std::find_if(allowed_values.begin(), allowed_values.end(),
                     [value](std::string_view item) {
                         return EqualsAsciiCaseInsensitive(value, item);
                     });
    return found == allowed_values.end()
               ? std::nullopt
               : std::optional<std::string>(std::string(*found));
}

std::optional<std::string> PangoVariant(std::string_view value) {
    if (EqualsAsciiCaseInsensitive(value, "smallcaps") ||
        EqualsAsciiCaseInsensitive(value, "small-caps")) {
        return "small-caps";
    }
    return PangoKeyword(value, {"normal"});
}

std::optional<std::string> PangoStretch(std::string_view value) {
    static const std::map<std::string_view, std::string_view> kStretches = {
        {"normal", "normal"},
        {"ultracondensed", "ultra-condensed"},
        {"ultra-condensed", "ultra-condensed"},
        {"extracondensed", "extra-condensed"},
        {"extra-condensed", "extra-condensed"},
        {"condensed", "condensed"},
        {"semicondensed", "semi-condensed"},
        {"semi-condensed", "semi-condensed"},
        {"semiexpanded", "semi-expanded"},
        {"semi-expanded", "semi-expanded"},
        {"expanded", "expanded"},
        {"extraexpanded", "extra-expanded"},
        {"extra-expanded", "extra-expanded"},
        {"ultraexpanded", "ultra-expanded"},
        {"ultra-expanded", "ultra-expanded"}};
    const auto found = std::find_if(
        kStretches.begin(), kStretches.end(), [value](const auto& item) {
            return EqualsAsciiCaseInsensitive(value, item.first);
        });
    return found == kStretches.end()
               ? std::nullopt
               : std::optional<std::string>(std::string(found->second));
}

std::optional<std::string_view> FirstAttribute(
    const XmlNode& node,
    std::initializer_list<std::string_view> attribute_names) {
    for (const std::string_view name : attribute_names) {
        if (const auto value = Attribute(node, name); value.has_value()) {
            return value;
        }
    }
    return std::nullopt;
}

void AppendPangoFontDescription(std::string_view description,
                                BoundedText* style) {
    std::istringstream input{std::string(description)};
    std::vector<std::string> tokens;
    for (std::string token; input >> token;) {
        tokens.push_back(std::move(token));
    }
    if (tokens.empty()) {
        return;
    }
    std::optional<std::string> size;
    std::optional<std::string> font_style;
    std::optional<std::string> variant;
    std::optional<std::string> weight;
    std::optional<std::string> stretch;
    std::ptrdiff_t family_end = static_cast<std::ptrdiff_t>(tokens.size()) - 1;
    for (; family_end >= 0; --family_end) {
        const std::string_view token(
            tokens[static_cast<std::size_t>(family_end)]);
        if (!token.empty() &&
            std::isdigit(static_cast<unsigned char>(token.front())) != 0) {
            size = PangoLength(token, true);
            continue;
        }
        if (auto candidate =
                PangoKeyword(token, {"normal", "oblique", "italic"});
            candidate.has_value()) {
            if (!font_style.has_value()) {
                font_style = std::move(candidate);
            }
            continue;
        }
        if (auto candidate = PangoVariant(token);
            candidate.has_value() &&
            !EqualsAsciiCaseInsensitive(token, "normal")) {
            variant = std::move(candidate);
            continue;
        }
        if (auto candidate = PangoWeight(token);
            candidate.has_value() &&
            !EqualsAsciiCaseInsensitive(token, "normal")) {
            weight = std::move(candidate);
            continue;
        }
        if (auto candidate = PangoStretch(token);
            candidate.has_value() &&
            !EqualsAsciiCaseInsensitive(token, "normal")) {
            stretch = std::move(candidate);
            continue;
        }
        if (PangoKeyword(token, {"south", "east", "north", "west", "auto"})
                .has_value()) {
            continue;
        }
        break;
    }
    if (family_end >= 0) {
        std::string family;
        for (std::ptrdiff_t index = 0; index <= family_end; ++index) {
            if (!family.empty() && family.back() != ',') {
                family.push_back(',');
            }
            family += tokens[static_cast<std::size_t>(index)];
        }
        if (IsSafeCssFamily(family)) {
            AppendPangoDeclaration("font-family", std::string_view(family),
                                   style);
        }
    }
    AppendOptionalPangoDeclaration("font-style", font_style, style);
    AppendOptionalPangoDeclaration("font-variant", variant, style);
    AppendOptionalPangoDeclaration("font-weight", weight, style);
    AppendOptionalPangoDeclaration("font-stretch", stretch, style);
    AppendOptionalPangoDeclaration("font-size", size, style);
}

std::string BuildPangoStyle(const XmlNode& node, DecodeControl* control) {
    BoundedText style(control);
    if (const auto description = FirstAttribute(node, {"font_desc", "font"});
        description.has_value()) {
        AppendPangoFontDescription(*description, &style);
    }
    if (const auto family = FirstAttribute(node, {"font_family", "face"});
        family.has_value() && IsSafeCssFamily(*family)) {
        AppendPangoDeclaration("font-family", *family, &style);
    }
    if (const auto size = FirstAttribute(node, {"font_size", "size"});
        size.has_value()) {
        AppendOptionalPangoDeclaration("font-size", PangoLength(*size, true),
                                       &style);
    }
    if (const auto font_style = FirstAttribute(node, {"font_style", "style"});
        font_style.has_value()) {
        AppendOptionalPangoDeclaration(
            "font-style",
            PangoKeyword(*font_style, {"normal", "oblique", "italic"}), &style);
    }
    if (const auto weight = Attribute(node, "weight"); weight.has_value()) {
        AppendOptionalPangoDeclaration("font-weight", PangoWeight(*weight),
                                       &style);
    }
    if (const auto variant = FirstAttribute(node, {"font_variant", "variant"});
        variant.has_value()) {
        AppendOptionalPangoDeclaration("font-variant", PangoVariant(*variant),
                                       &style);
    }
    if (const auto stretch = FirstAttribute(node, {"font_stretch", "stretch"});
        stretch.has_value()) {
        AppendOptionalPangoDeclaration("font-stretch", PangoStretch(*stretch),
                                       &style);
    }
    if (const auto color =
            FirstAttribute(node, {"foreground", "fgcolor", "color"});
        color.has_value() && IsSafeColor(*color)) {
        AppendPangoDeclaration("color", *color, &style);
    }
    if (const auto background = FirstAttribute(node, {"background", "bgcolor"});
        background.has_value() && IsSafeColor(*background)) {
        AppendPangoDeclaration("background-color", *background, &style);
    }
    if (const auto decoration_color =
            FirstAttribute(node, {"underline_color", "strikethrough_color"});
        decoration_color.has_value() && IsSafeColor(*decoration_color)) {
        AppendPangoDeclaration("text-decoration-color", *decoration_color,
                               &style);
    }
    if (const auto underline = Attribute(node, "underline");
        underline.has_value()) {
        // Preserve the frozen Qt 5 comparison semantics, including its
        // inverted "none" branch.
        if (!EqualsAsciiCaseInsensitive(*underline, "none")) {
            AppendPangoDeclaration("text-decoration-line", "none", &style);
        } else {
            AppendPangoDeclaration("text-decoration-line", "underline", &style);
            AppendPangoDeclaration("text-decoration-style", "dotted", &style);
        }
    }
    if (const auto strike = Attribute(node, "strikethrough");
        strike.has_value()) {
        AppendPangoDeclaration("text-decoration-line",
                               EqualsAsciiCaseInsensitive(*strike, "true")
                                   ? "none"
                                   : "line-through",
                               &style);
    }
    if (const auto rise = Attribute(node, "rise"); rise.has_value()) {
        AppendOptionalPangoDeclaration("vertical-align",
                                       PangoLength(*rise, false), &style);
    }
    if (const auto spacing = Attribute(node, "letter_spacing");
        spacing.has_value()) {
        AppendOptionalPangoDeclaration("letter-spacing",
                                       PangoLength(*spacing, false), &style);
    }
    return style.Take();
}

void RenderPangoChildren(const XmlNode& node, BoundedText* output);

void RenderPangoNode(const XmlNode& node, BoundedText* output) {
    if (node.name != "span") {
        if (node.name == "b" || node.name == "strong" || node.name == "i" ||
            node.name == "em" || node.name == "u") {
            output->Append("<");
            output->Append(node.name);
            output->Append(">");
            RenderPangoChildren(node, output);
            output->Append("</");
            output->Append(node.name);
            output->Append(">");
        } else {
            RenderPangoChildren(node, output);
        }
        return;
    }

    const std::string style = BuildPangoStyle(node, output->control());
    output->Append("<span");
    if (!style.empty()) {
        output->Append(" style=\"");
        output->Append(EscapeAttribute(style, output->control()));
        output->Append("\"");
    }
    output->Append(">");
    RenderPangoChildren(node, output);
    output->Append("</span>");
}

void RenderPangoChildren(const XmlNode& node, BoundedText* output) {
    for (const auto& item : node.content) {
        if (item.child) {
            RenderPangoNode(*item.child, output);
            continue;
        }
        std::size_t start = 0U;
        while (start < item.text.size()) {
            const auto newline = item.text.find('\n', start);
            const auto end =
                newline == std::string::npos ? item.text.size() : newline;
            output->Append(EscapeHtml(
                std::string_view(item.text).substr(start, end - start),
                output->control()));
            if (newline == std::string::npos) {
                break;
            }
            output->Append("<br>");
            start = newline + 1U;
        }
    }
}

std::string ConvertPango(std::string_view value, DecodeControl* control) {
    BoundedText output(control);
    output.Append("<div class=\"sdct_g\">");
    auto root = ParseXmlFragment(value, control);
    if (!root) {
        output.Append(value);
    } else {
        RenderPangoChildren(*root, &output);
    }
    output.Append("</div>");
    return output.Take();
}

std::string HandleField(char type, std::string_view value,
                        bool target_right_to_left, DecodeControl* control) {
    BoundedText output(control);
    switch (type) {
        case 'x':
            return ConvertXdxf(value, control);
        case 'h':
            output.Append("<div class=\"sdct_h\">");
            output.Append(ConvertHtmlAudio(value, control));
            output.Append("</div>");
            return output.Take();
        case 'm':
            output.Append("<div class=\"sdct_m\">");
            output.Append(Preformat(value, target_right_to_left, control));
            output.Append("</div>");
            return output.Take();
        case 'l':
            output.Append("<div class=\"sdct_l\">");
            output.Append(Preformat(DecodeLocalText(value, control),
                                    target_right_to_left, control));
            output.Append("</div>");
            return output.Take();
        case 'g':
            return ConvertPango(value, control);
        case 't':
        case 'y':
        case 'w':
        case 'n':
        case 'r':
            output.Append("<div class=\"sdct_");
            output.PushBack(type);
            output.Append("\">");
            output.Append(EscapeHtml(value, control));
            output.Append("</div>");
            return output.Take();
        case 'k':
            return ConvertPowerWord(value, control);
        case 'W':
            return "<div class=\"sdct_W\">(an embedded .wav file)</div>";
        case 'P':
            return "<div class=\"sdct_P\">(an embedded picture file)</div>";
        default:
            if (IsAsciiLower(type)) {
                output.Append("<b>Unknown textual entry type ");
                output.PushBack(type);
                output.Append(":</b> ");
                output.Append(EscapeHtml(value, control));
                output.Append("<br>");
            } else {
                output.Append("<b>Unknown blob entry type ");
                output.PushBack(type);
                output.Append("</b><br>");
            }
            return output.Take();
    }
}

std::string DecodeArticleFieldsImpl(std::string_view record,
                                    std::string_view same_type_sequence,
                                    std::string_view target_language,
                                    DecodeControl* control) {
    if (record.size() > kMaximumRenderedArticleBytes) {
        throw ArticleDecodeError(
            "StarDict article record exceeds the supported size limit");
    }
    if (control != nullptr) {
        control->Check();
    }
    BoundedText output(control);
    std::size_t cursor = 0U;
    const bool target_right_to_left = IsRightToLeftLanguage(target_language);

    if (!same_type_sequence.empty()) {
        for (std::size_t sequence_index = 0U;
             sequence_index < same_type_sequence.size(); ++sequence_index) {
            const bool last = sequence_index + 1U == same_type_sequence.size();
            const char type = same_type_sequence[sequence_index];
            if (cursor >= record.size()) {
                break;
            }
            std::size_t field_size = record.size() - cursor;
            if (IsAsciiLower(type)) {
                if (!last) {
                    const auto terminator =
                        FindCharacter(record, '\0', cursor, control);
                    if (terminator == std::string_view::npos) {
                        break;
                    }
                    field_size = terminator - cursor;
                }
            } else if (IsAsciiUpper(record[cursor])) {
                if (!last) {
                    if (record.size() - cursor < 4U) {
                        break;
                    }
                    field_size = ReadBigEndian32(record, cursor);
                    cursor += 4U;
                }
            } else {
                break;
            }
            if (field_size > record.size() - cursor) {
                break;
            }
            output.Append(HandleField(type, record.substr(cursor, field_size),
                                      target_right_to_left, control));
            cursor += field_size;
            if (IsAsciiLower(type) && !last) {
                ++cursor;
            }
        }
        return output.Take();
    }

    while (cursor < record.size()) {
        const char type = record[cursor++];
        if (IsAsciiLower(type)) {
            const auto terminator =
                FindCharacter(record, '\0', cursor, control);
            if (terminator == std::string_view::npos) {
                break;
            }
            output.Append(
                HandleField(type, record.substr(cursor, terminator - cursor),
                            target_right_to_left, control));
            cursor = terminator + 1U;
        } else if (IsAsciiUpper(type)) {
            if (record.size() - cursor < 4U) {
                break;
            }
            const std::size_t field_size = ReadBigEndian32(record, cursor);
            cursor += 4U;
            if (field_size > record.size() - cursor) {
                break;
            }
            output.Append(HandleField(type, record.substr(cursor, field_size),
                                      target_right_to_left, control));
            cursor += field_size;
        } else {
            break;
        }
    }
    return output.Take();
}

}  // namespace

std::string DecodeArticleFields(std::string_view record,
                                std::string_view same_type_sequence,
                                std::string_view target_language,
                                const ArticleDecodeCheckpoint& checkpoint) {
    DecodeControl control(checkpoint);
    return DecodeArticleFieldsImpl(record, same_type_sequence, target_language,
                                   &control);
}

std::string DecodeArticle(std::string_view headword, std::string_view record,
                          std::string_view same_type_sequence,
                          std::string_view source_language,
                          std::string_view target_language,
                          const ArticleDecodeCheckpoint& checkpoint) {
    DecodeControl control(checkpoint);
    control.Check();
    BoundedText output(&control);
    output.Append("<h3 class=\"sdct_headwords\"");
    if (IsRightToLeftLanguage(source_language)) {
        output.Append(" dir=\"rtl\"");
    }
    output.Append(">");
    output.Append(EscapeHtml(headword, &control));
    output.Append("</h3>");
    if (IsRightToLeftLanguage(target_language)) {
        output.Append("<div dir=\"rtl\">");
    }
    output.Append(DecodeArticleFieldsImpl(record, same_type_sequence,
                                          target_language, &control));
    if (IsRightToLeftLanguage(target_language)) {
        output.Append("</div>");
    }
    control.Check();
    return output.Take();
}

}  // namespace goldendict::core::formats::stardict
