// SPDX-License-Identifier: GPL-3.0-or-later

#include "text_folding.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <unicode/utf16.h>

#include "utf8.h"

namespace goldendict::core::foundation {
namespace {

void RequireSuccess(UErrorCode status, const char* operation) {
    if (U_FAILURE(status)) {
        throw TextFoldingError(std::string(operation) + ": " +
                               u_errorName(status));
    }
}

std::vector<UChar> FromUtf8(std::string_view text) {
    if (!IsValidUtf8(text) ||
        text.size() > static_cast<std::size_t>(
                          std::numeric_limits<std::int32_t>::max())) {
        throw TextFoldingError("Lookup text is not valid bounded UTF-8");
    }
    const auto source_length = static_cast<std::int32_t>(text.size());
    std::int32_t output_length = 0;
    UErrorCode status = U_ZERO_ERROR;
    u_strFromUTF8(nullptr, 0, &output_length, text.data(), source_length,
                  &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        RequireSuccess(status, "Cannot size UTF-8 conversion");
    }
    status = U_ZERO_ERROR;
    std::vector<UChar> output(static_cast<std::size_t>(output_length));
    u_strFromUTF8(output.data(), output_length, nullptr, text.data(),
                  source_length, &status);
    RequireSuccess(status, "Cannot convert UTF-8 text");
    return output;
}

std::vector<UChar> Normalize(const UNormalizer2* normalizer,
                             const std::vector<UChar>& input) {
    UErrorCode status = U_ZERO_ERROR;
    const auto input_length = static_cast<std::int32_t>(input.size());
    std::int32_t output_length = unorm2_normalize(
        normalizer, input.data(), input_length, nullptr, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        RequireSuccess(status, "Cannot size Unicode normalization");
    }
    status = U_ZERO_ERROR;
    std::vector<UChar> output(static_cast<std::size_t>(output_length));
    output_length = unorm2_normalize(normalizer, input.data(), input_length,
                                     output.data(), output_length, &status);
    RequireSuccess(status, "Cannot normalize Unicode text");
    output.resize(static_cast<std::size_t>(output_length));
    return output;
}

std::vector<UChar> FoldCase(const std::vector<UChar>& input) {
    UErrorCode status = U_ZERO_ERROR;
    const auto input_length = static_cast<std::int32_t>(input.size());
    std::int32_t output_length = u_strFoldCase(
        nullptr, 0, input.data(), input_length, U_FOLD_CASE_DEFAULT, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        RequireSuccess(status, "Cannot size Unicode case folding");
    }
    status = U_ZERO_ERROR;
    std::vector<UChar> output(static_cast<std::size_t>(output_length));
    output_length = u_strFoldCase(output.data(), output_length, input.data(),
                                  input_length, U_FOLD_CASE_DEFAULT, &status);
    RequireSuccess(status, "Cannot fold Unicode case");
    output.resize(static_cast<std::size_t>(output_length));
    return output;
}

std::vector<UChar> FoldSimpleCase(const std::vector<UChar>& input) {
    std::vector<UChar> output;
    output.reserve(input.size());
    std::int32_t position = 0;
    const auto length = static_cast<std::int32_t>(input.size());
    while (position < length) {
        UChar32 code_point = 0;
        U16_NEXT(input.data(), position, length, code_point);
        code_point = u_foldCase(code_point, U_FOLD_CASE_DEFAULT);
        if (code_point <= 0xffff) {
            output.push_back(static_cast<UChar>(code_point));
        } else {
            output.push_back(U16_LEAD(code_point));
            output.push_back(U16_TRAIL(code_point));
        }
    }
    return output;
}

bool IsMark(UChar32 code_point) noexcept {
    const auto category = static_cast<UCharCategory>(u_charType(code_point));
    return category == U_NON_SPACING_MARK ||
           category == U_COMBINING_SPACING_MARK || category == U_ENCLOSING_MARK;
}

bool IsCurrentUnicodeSeparator(char32_t code_point) {
    const auto value = static_cast<UChar32>(code_point);
    return u_isUWhiteSpace(value) || u_ispunct(value);
}

std::vector<UChar> RemoveMarksAndSeparators(
    const std::vector<UChar>& input,
    LookupSeparatorPredicate is_separator) {
    std::vector<UChar> output;
    output.reserve(input.size());
    std::int32_t position = 0;
    const auto length = static_cast<std::int32_t>(input.size());
    while (position < length) {
        UChar32 code_point = 0;
        U16_NEXT(input.data(), position, length, code_point);
        if (code_point < 0 || IsMark(code_point) ||
            is_separator(static_cast<char32_t>(code_point))) {
            continue;
        }
        if (code_point <= 0xffff) {
            output.push_back(static_cast<UChar>(code_point));
        } else {
            output.push_back(U16_LEAD(code_point));
            output.push_back(U16_TRAIL(code_point));
        }
    }
    return output;
}

std::vector<UChar> RemoveMarks(const std::vector<UChar>& input) {
    std::vector<UChar> output;
    output.reserve(input.size());
    std::int32_t position = 0;
    const auto length = static_cast<std::int32_t>(input.size());
    while (position < length) {
        const auto begin = position;
        UChar32 code_point = 0;
        U16_NEXT(input.data(), position, length, code_point);
        if (code_point >= 0 && !IsMark(code_point)) {
            output.insert(output.end(), input.begin() + begin,
                          input.begin() + position);
        }
    }
    return output;
}

std::string ToUtf8(const std::vector<UChar>& input) {
    const auto input_length = static_cast<std::int32_t>(input.size());
    std::int32_t output_length = 0;
    UErrorCode status = U_ZERO_ERROR;
    u_strToUTF8(nullptr, 0, &output_length, input.data(), input_length,
                &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        RequireSuccess(status, "Cannot size UTF-8 output");
    }
    status = U_ZERO_ERROR;
    std::string output(static_cast<std::size_t>(output_length), '\0');
    u_strToUTF8(output.data(), output_length, nullptr, input.data(),
                input_length, &status);
    RequireSuccess(status, "Cannot encode folded UTF-8 text");
    return output;
}

}  // namespace

std::string FoldForLookup(std::string_view text) {
    return FoldForLookupWithSeparatorPolicy(text, IsCurrentUnicodeSeparator);
}

std::string FoldForLookupWithSeparatorPolicy(
    std::string_view text, LookupSeparatorPredicate is_separator) {
    if (text.empty()) {
        return {};
    }
    if (!is_separator) {
        throw TextFoldingError("Lookup separator policy is missing");
    }

    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer = unorm2_getNFKDInstance(&status);
    RequireSuccess(status, "Cannot initialize Unicode normalization");

    auto folded = Normalize(normalizer, FromUtf8(text));
    folded = FoldCase(folded);
    folded = Normalize(normalizer, folded);
    return ToUtf8(RemoveMarksAndSeparators(folded, is_separator));
}

std::string FoldSimpleCase(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    return ToUtf8(FoldSimpleCase(FromUtf8(text)));
}

std::string NormalizeForExactLookup(std::string_view text,
                                    bool ignore_diacritics) {
    if (text.empty())
        return {};
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* nfc = unorm2_getNFCInstance(&status);
    RequireSuccess(status, "Cannot initialize NFC normalization");
    auto result = Normalize(nfc, FromUtf8(text));
    result = FoldCase(result);
    result = Normalize(nfc, result);
    if (!ignore_diacritics)
        return ToUtf8(result);
    status = U_ZERO_ERROR;
    const UNormalizer2* nfd = unorm2_getNFDInstance(&status);
    RequireSuccess(status, "Cannot initialize NFD normalization");
    result = RemoveMarks(Normalize(nfd, result));
    return ToUtf8(Normalize(nfc, result));
}

}  // namespace goldendict::core::foundation
