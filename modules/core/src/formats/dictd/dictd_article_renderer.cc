// SPDX-License-Identifier: GPL-3.0-or-later
// Layout derived from frozen GoldenDict htmlescape.cc, copyright
// 2008-2012 Konstantin Isakov, GPLv3 or later.

#include "dictd_article_renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

#include <unicode/uchar.h>

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
        Advance(value.size());
        output->append(value);
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

}  // namespace

std::string RenderArticleBody(std::string_view body,
                              std::string_view target_language,
                              const std::function<void()>& checkpoint) {
    RenderControl control(checkpoint);
    const bool base_right_to_left = IsRightToLeftLanguage(target_language);
    std::string output;
    control.Append(base_right_to_left
                       ? "<div class=\"dictd_article\" dir=\"rtl\">"
                       : "<div class=\"dictd_article\">",
                   &output);
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
    control.Append("</div>", &output);
    control.Check();
    return output;
}

}  // namespace goldendict::core::formats::dictd
