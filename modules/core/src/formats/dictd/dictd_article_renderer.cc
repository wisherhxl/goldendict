// SPDX-License-Identifier: GPL-3.0-or-later
// Layout derived from frozen GoldenDict htmlescape.cc, copyright
// 2008-2012 Konstantin Isakov, GPLv3 or later.

#include "dictd_article_renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <QChar>

#include <unicode/uchar.h>

#include "../../article/internal_url.h"
#include "../../dictionary/dictionary_backend.h"

namespace goldendict::core::formats::dictd {
namespace {

constexpr std::size_t kMaximumRenderedBytes = 16U * 1024U * 1024U;

class RenderControl final {
   public:
    explicit RenderControl(const std::function<void()>& checkpoint)
        : checkpoint_(checkpoint) {
        Check();
    }

    void Advance(std::size_t count = 1U) {
        work_ += count;
        if (work_ >= 4096U) {
            work_ %= 4096U;
            Check();
        }
    }

    void Check() const {
        if (checkpoint_) {
            checkpoint_();
        }
    }

    void Append(std::string_view value, std::string* output) {
        if (value.size() > kMaximumRenderedBytes - output->size()) {
            throw dictionary::Error(
                dictionary::ErrorCode::kInvalidData,
                "Rendered Dictd article exceeds the size limit");
        }
        while (!value.empty()) {
            const auto count = std::min(value.size(), 4096U - work_);
            output->append(value.substr(0U, count));
            value.remove_prefix(count);
            Advance(count);
        }
    }

   private:
    const std::function<void()>& checkpoint_;
    std::size_t work_ = 0U;
};

bool IsRightToLeftLanguage(std::string_view language) {
    constexpr std::array<std::string_view, 11U> kLanguages = {
        "ar", "dv", "fa", "he", "ku", "ps", "sd", "syr", "ug", "ur", "yi"};
    return std::find(kLanguages.begin(), kLanguages.end(), language) !=
           kLanguages.end();
}

struct Character {
    std::uint32_t value;
    std::size_t bytes;
};

// QString::fromUtf8 replaces each invalid byte, including the remaining
// bytes of an incomplete, overlong, surrogate or out-of-range sequence.
Character ReadCharacter(std::string_view text, std::size_t position) {
    const auto first = static_cast<unsigned char>(text[position]);
    if (first < 0x80U) {
        return {first, 1U};
    }
    const std::size_t length = first >= 0xc2U && first <= 0xdfU   ? 2U
                               : first >= 0xe0U && first <= 0xefU ? 3U
                               : first >= 0xf0U && first <= 0xf4U ? 4U
                                                                  : 0U;
    if (length == 0U || length > text.size() - position) {
        return {0xfffdU, 1U};
    }
    std::uint32_t value = first & (length == 2U   ? 0x1fU
                                   : length == 3U ? 0x0fU
                                                  : 0x07U);
    for (std::size_t offset = 1U; offset < length; ++offset) {
        const auto next = static_cast<unsigned char>(text[position + offset]);
        if ((next & 0xc0U) != 0x80U) {
            return {0xfffdU, 1U};
        }
        value = (value << 6U) | (next & 0x3fU);
    }
    if (value < (length == 2U   ? 0x80U
                 : length == 3U ? 0x800U
                                : 0x10000U) ||
        value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU)) {
        return {0xfffdU, 1U};
    }
    return {value, length};
}

std::pair<std::string, bool> NormalizeLine(std::string_view line,
                                           RenderControl* control) {
    std::string normalized;
    std::optional<bool> direction;
    std::size_t isolate_depth = 0U;
    bool entity = false;
    for (std::size_t position = 0U; position < line.size();) {
        const auto character = ReadCharacter(line, position);
        if (character.value == 0xfffdU && character.bytes == 1U) {
            control->Append("\xef\xbf\xbd", &normalized);
        } else {
            control->Append(line.substr(position, character.bytes),
                            &normalized);
        }
        position += character.bytes;
        // Every ampersand here begins one of the renderer's own escaped
        // neutral characters. Literal source entities were escaped first.
        if (character.value == '&') {
            entity = true;
        }
        if (entity) {
            if (character.value == ';') {
                entity = false;
            }
            continue;
        }
        if (direction.has_value()) {
            continue;
        }
        const auto bidi = u_charDirection(character.value);
        if (bidi == U_LEFT_TO_RIGHT_ISOLATE ||
            bidi == U_RIGHT_TO_LEFT_ISOLATE || bidi == U_FIRST_STRONG_ISOLATE) {
            ++isolate_depth;
        } else if (bidi == U_POP_DIRECTIONAL_ISOLATE) {
            if (isolate_depth != 0U) {
                --isolate_depth;
            }
        } else if (isolate_depth == 0U) {
            if (bidi == U_LEFT_TO_RIGHT) {
                direction = false;
            } else if (bidi == U_RIGHT_TO_LEFT ||
                       bidi == U_RIGHT_TO_LEFT_ARABIC) {
                direction = true;
            }
        }
    }
    return {std::move(normalized), direction.value_or(false)};
}

std::string PreformatLines(std::string_view body,
                           std::string_view target_language,
                           RenderControl& control) {
    const bool base_right_to_left = IsRightToLeftLanguage(target_language);
    std::string output;
    std::string line;
    const auto append_line = [&]() {
        const auto normalized = NormalizeLine(line, &control);
        control.Append("<div", &output);
        if (normalized.second != base_right_to_left) {
            control.Append(base_right_to_left ? " dir=\"ltr\"" : " dir=\"rtl\"",
                           &output);
        }
        control.Append(">", &output);
        control.Append(normalized.first, &output);
        control.Append("</div>", &output);
        line.clear();
    };
    bool leading = true;
    for (const char character : body) {
        control.Advance();
        if (character == '\0') {
            break;
        }
        if (leading && (character == ' ' || character == '\t')) {
            control.Append(
                character == ' ' ? "&nbsp;" : "&nbsp;&nbsp;&nbsp;&nbsp;",
                &line);
            continue;
        }
        if (character == '\n') {
            append_line();
            leading = true;
            continue;
        }
        if (character == '\r') {
            continue;
        }
        switch (character) {
            case '&':
                control.Append("&amp;", &line);
                break;
            case '<':
                control.Append("&lt;", &line);
                break;
            case '>':
                control.Append("&gt;", &line);
                break;
            case '"':
                control.Append("&quot;", &line);
                break;
            default:
                control.Append(std::string_view(&character, 1U), &line);
        }
        leading = false;
    }
    if (!line.empty()) {
        append_line();
    }
    control.Check();
    return output;
}

struct ReferenceOrigin {
    std::size_t opening;
    std::size_t capture_size;
    bool rewritten;
};

template <typename T>
void PushBounded(T value, std::vector<T>* values) {
    constexpr auto maximum = kMaximumRenderedBytes / sizeof(T);
    if (values->size() == maximum) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Dictd generated markup storage exceeds limit");
    }
    if (values->size() == values->capacity()) {
        values->reserve(std::min(
            maximum, std::max<std::size_t>(16U, values->capacity() * 2U)));
    }
    values->push_back(std::move(value));
}

// The frozen replacements are global and nonrecursive. Braces inside a brace
// candidate restart it; adjacent backslashes cannot form an empty phonetic.
std::string ConvertInlineMarkup(
    std::string_view input, bool references, RenderControl& control,
    std::vector<ReferenceOrigin>* origins = nullptr) {
    std::string output;
    const char opening = references ? '{' : '\\';
    const char closing = references ? '}' : '\\';
    std::size_t copied = 0U;
    std::optional<std::size_t> begin;
    for (std::size_t position = 0U; position < input.size(); ++position) {
        control.Advance();
        const char value = input[position];
        if (begin && value == closing && position > *begin + 1U) {
            const auto capture =
                input.substr(*begin + 1U, position - *begin - 1U);
            // Check the entire expansion before making either capture copy.
            const std::string_view prefix =
                references ? "<a href=\"gdlookup://localhost/"
                           : "<span class=\"dictd_phonetic\">";
            const std::string_view suffix = references ? "</a>" : "</span>";
            const auto overhead =
                prefix.size() + suffix.size() + (references ? 2U : 0U);
            const std::size_t copies = references ? 2U : 1U;
            const auto remaining = kMaximumRenderedBytes - output.size();
            const auto literal_size = *begin - copied;
            if (literal_size > remaining ||
                overhead > remaining - literal_size ||
                capture.size() >
                    (remaining - literal_size - overhead) / copies) {
                throw dictionary::Error(
                    dictionary::ErrorCode::kInvalidData,
                    "Dictd inline expansion exceeds the size limit");
            }
            control.Append(input.substr(copied, *begin - copied), &output);
            if (origins) {
                bool rewritten = true;
                for (const char byte : capture) {
                    control.Advance();
                    rewritten = rewritten && byte != '"';
                }
                PushBounded(
                    ReferenceOrigin{output.size(), capture.size(), rewritten},
                    origins);
            }
            control.Append(prefix, &output);
            control.Append(capture, &output);
            if (references) {
                control.Append("\">", &output);
                control.Append(capture, &output);
            }
            control.Append(suffix, &output);
            copied = position + 1U;
            begin.reset();
        } else if (value == opening) {
            begin = position;
        } else if (references && value == closing) {
            begin.reset();
        }
    }
    control.Append(input.substr(copied), &output);
    return output;
}

std::string ReferenceHref(std::string_view capture, RenderControl& control) {
    std::string target;
    bool space = false;
    for (std::size_t position = 0U; position < capture.size();) {
        auto character = ReadCharacter(capture, position);
        if (character.value == '<') {
            do {
                control.Advance();
            } while (capture[position++] != '>' && position < capture.size());
            space = !target.empty();
            continue;
        }
        if (capture.substr(position, 6U) == "&nbsp;") {
            character = {' ', 6U};
        }
        control.Advance(character.bytes);
        if (QChar::isSpace(character.value)) {
            space = !target.empty();
        } else {
            if (space) {
                control.Append(" ", &target);
                space = false;
            }
            control.Append(capture.substr(position, character.bytes), &target);
        }
        position += character.bytes;
    }
    // N4 is evaluated only after a successful legacy target rewrite, before
    // browser dot-segment normalization. Other punctuation remains literal.
    if (target.empty() || target == "." || target == "..") {
        return {};
    }
    // Reuse the typed URL boundary in bounded UTF-8 chunks. Its internal
    // encoding/validation scans must not create an uninterruptible large call.
    constexpr std::string_view prefix = "goldendict://lookup/";
    std::string href;
    control.Append(prefix, &href);
    for (std::size_t position = 0U; position < target.size();) {
        std::size_t count =
            std::min<std::size_t>(256U, target.size() - position);
        while (position + count < target.size() &&
               (static_cast<unsigned char>(target[position + count]) & 0xc0U) ==
                   0x80U) {
            --count;
        }
        control.Check();
        const auto part = article::MakeLookupUrl(
            std::string_view(target).substr(position, count));
        control.Append(std::string_view(part).substr(prefix.size()), &href);
        position += count;
    }
    return href;
}

bool HasZeroWidthLegacyGlyph(unsigned char byte) {
    return (byte >= 1U && byte <= 8U) || byte == 11U || byte == 12U ||
           (byte >= 14U && byte <= 31U) || byte == 127U;
}

// WebKit retains these bytes in text but gives them no glyph/advance. Preserve
// them in the DOM while suppressing Chromium's control glyphs. This runs only
// on final display text, after reference targets and independent FTS diverge.
void AppendDisplayText(std::string_view text, RenderControl& control,
                       std::string* output) {
    for (std::size_t first = 0U; first < text.size();) {
        const bool zero_width =
            HasZeroWidthLegacyGlyph(static_cast<unsigned char>(text[first]));
        auto end = first;
        do {
            ++end;
            control.Advance();
        } while (end < text.size() &&
                 HasZeroWidthLegacyGlyph(
                     static_cast<unsigned char>(text[end])) == zero_width);
        if (zero_width) {
            control.Append("<span class=\"dictd_control\">", output);
        }
        control.Append(text.substr(first, end - first), output);
        if (zero_width) {
            control.Append("</span>", output);
        }
        first = end;
    }
}

// This grammar is generated exclusively by PreformatLines/ConvertInlineMarkup.
// Only anchors participate in formatting reconstruction. Their origin survives
// stack removal; spans neither reconstruct nor close across block boundaries.
std::string NormalizeGeneratedDisplay(
    std::string_view input, const std::vector<ReferenceOrigin>& origins,
    RenderControl& control) {
    enum class Element { kDiv, kSpan, kAnchor };
    std::vector<Element> stack;
    std::optional<std::size_t> active;
    std::optional<std::size_t> anchor_depth;
    std::string href;
    std::string output;
    std::size_t next_origin = 0U;
    const auto close_top = [&]() {
        const auto element = stack.back();
        control.Append(element == Element::kDiv    ? "</div>"
                       : element == Element::kSpan ? "</span>"
                                                   : "</a>",
                       &output);
        if (element == Element::kAnchor) {
            anchor_depth.reset();
        }
        stack.pop_back();
    };
    const auto reconstruct = [&]() {
        if (!active || anchor_depth) {
            return;
        }
        if (href.empty()) {
            control.Append("<a class=\"dictd_inert_reference\">", &output);
        } else {
            control.Append("<a href=\"", &output);
            control.Append(href, &output);
            control.Append("\">", &output);
        }
        anchor_depth = stack.size();
        PushBounded(Element::kAnchor, &stack);
    };
    for (std::size_t position = 0U; position < input.size();) {
        control.Advance();
        if (next_origin < origins.size() &&
            origins[next_origin].opening == position) {
            // A new anchor closes the preceding formatting entry, including
            // any generated spans above it, before acquiring a new identity.
            if (anchor_depth) {
                const auto depth = *anchor_depth;
                while (stack.size() > depth) {
                    close_top();
                }
            }
            active = next_origin++;
            const auto& origin = origins[*active];
            constexpr std::size_t prefix_size = 30U;
            const auto capture =
                input.substr(position + prefix_size, origin.capture_size);
            href = origin.rewritten ? ReferenceHref(capture, control)
                                    : std::string();
            if (origin.rewritten) {
                position += prefix_size + origin.capture_size + 2U;
            } else {
                // The first generated quote closes href. HTML's following
                // attribute-name state consumes the next '>'; leaked text
                // following that boundary must remain visible.
                position += prefix_size;
                while (input[position] != '"') {
                    ++position;
                    control.Advance();
                }
                while (input[position++] != '>') {
                    control.Advance();
                }
            }
            reconstruct();
            continue;
        }
        if (input[position] != '<') {
            reconstruct();
            const auto begin = position;
            do {
                ++position;
                control.Advance();
            } while (position < input.size() && input[position] != '<');
            AppendDisplayText(input.substr(begin, position - begin), control,
                              &output);
            continue;
        }
        const auto begin = position;
        while (input[position++] != '>') {
            control.Advance();
        }
        const auto tag = input.substr(begin, position - begin);
        if (tag == "</a>") {
            if (anchor_depth) {
                const auto depth = *anchor_depth;
                while (stack.size() > depth) {
                    close_top();
                }
            }
            active.reset();
            href.clear();
        } else if (tag == "</div>" || tag == "</span>") {
            const auto wanted =
                tag == "</div>" ? Element::kDiv : Element::kSpan;
            std::size_t depth = stack.size();
            while (depth != 0U) {
                control.Advance();
                --depth;
                if (stack[depth] == wanted || stack[depth] == Element::kDiv) {
                    break;
                }
            }
            if (!stack.empty() && stack[depth] == wanted) {
                while (stack.size() > depth) {
                    close_top();
                }
            }
        } else {
            const bool span = tag.substr(0U, 5U) == "<span";
            if (span) {
                reconstruct();
            }
            control.Append(tag, &output);
            PushBounded(span ? Element::kSpan : Element::kDiv, &stack);
        }
    }
    while (!stack.empty()) {
        close_top();
    }
    return output;
}

// Only generated tags reach this stage: source '<' and '&' were escaped.
// Block replacement must precede other tag removal, even inside broken hrefs.
std::string RemoveGeneratedTags(std::string_view input, bool blocks_only,
                                RenderControl& control) {
    std::string output;
    std::size_t copied = 0U;
    for (std::size_t position = 0U; position < input.size(); ++position) {
        control.Advance();
        if (input[position] != '<' ||
            (blocks_only && input.substr(position, 4U) != "<div" &&
             input.substr(position, 5U) != "</div")) {
            continue;
        }
        const auto start = position;
        while (position < input.size() && input[position] != '>') {
            ++position;
            control.Advance();
        }
        if (position == input.size()) {
            break;
        }
        control.Append(input.substr(copied, start - copied), &output);
        if (blocks_only) {
            control.Append(" ", &output);
        }
        copied = position + 1U;
    }
    control.Append(input.substr(copied), &output);
    return output;
}

std::string ExtractGeneratedText(std::string_view input,
                                 RenderControl& control) {
    // Match QString::trimmed before entity decoding, not final text trimming.
    std::size_t first = input.size();
    std::size_t end = 0U;
    for (std::size_t position = 0U; position < input.size();) {
        const auto character = ReadCharacter(input, position);
        control.Advance(character.bytes);
        if (!QChar::isSpace(character.value)) {
            first = std::min(first, position);
            end = position + character.bytes;
        }
        position += character.bytes;
    }
    if (end == 0U) {
        return {};
    }
    input = input.substr(first, end - first);
    std::string output;
    bool collapsible = true;
    for (std::size_t position = 0U; position < input.size();) {
        auto character = ReadCharacter(input, position);
        auto bytes = input.substr(position, character.bytes);
        if (character.value == '&') {
            constexpr std::pair<std::string_view, std::string_view> entities[] =
                {{"&amp;", "&"},
                 {"&lt;", "<"},
                 {"&gt;", ">"},
                 {"&quot;", "\""},
                 {"&nbsp;", "\xc2\xa0"}};
            for (const auto& entity : entities) {
                if (input.substr(position, entity.first.size()) ==
                    entity.first) {
                    bytes = entity.second;
                    character = ReadCharacter(bytes, 0U);
                    character.bytes = entity.first.size();
                    break;
                }
            }
        }
        control.Advance(character.bytes);
        position += character.bytes;
        if (character.value == 0xa0U) {
            control.Append(" ", &output);
            collapsible = false;
        } else if (character.value == 0x2029U) {
            control.Append("\n", &output);
            collapsible = true;
        } else if (QChar::isSpace(character.value)) {
            if (!collapsible) {
                control.Append(" ", &output);
            }
            collapsible = true;
        } else {
            control.Append(bytes, &output);
            collapsible = false;
        }
    }
    return output;
}

}  // namespace

std::string RenderArticleBody(std::string_view body,
                              std::string_view target_language,
                              const std::function<void()>& checkpoint) {
    RenderControl control(checkpoint);
    auto lines = PreformatLines(body, target_language, control);
    lines = ConvertInlineMarkup(lines, false, control);
    std::vector<ReferenceOrigin> origins;
    lines = ConvertInlineMarkup(lines, true, control, &origins);
    lines = NormalizeGeneratedDisplay(lines, origins, control);
    std::string output;
    control.Append(IsRightToLeftLanguage(target_language)
                       ? "<div class=\"dictd_article\" dir=\"rtl\">"
                       : "<div class=\"dictd_article\">",
                   &output);
    control.Append(lines, &output);
    control.Append("</div>", &output);
    control.Check();
    return output;
}

std::string ExtractArticleText(std::string_view body,
                               std::string_view target_language,
                               const std::function<void()>& checkpoint) {
    RenderControl control(checkpoint);
    auto text = PreformatLines(body, target_language, control);
    text = ConvertInlineMarkup(text, false, control);
    text = ConvertInlineMarkup(text, true, control);
    text = RemoveGeneratedTags(text, true, control);
    text = RemoveGeneratedTags(text, false, control);
    text = ExtractGeneratedText(text, control);
    control.Check();
    return text;
}

}  // namespace goldendict::core::formats::dictd
