// SPDX-License-Identifier: GPL-3.0-or-later

#include "text_encoding.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <unicode/ustring.h>

namespace goldendict::core::foundation {
namespace {

constexpr std::size_t kMaximumEncodingNameBytes = 128U;

void RequireSuccess(UErrorCode status, const char* operation) {
    if (U_FAILURE(status)) {
        throw TextEncodingError(std::string(operation) + ": " +
                                u_errorName(status));
    }
}

std::int32_t BoundedLength(std::size_t size, const char* field) {
    if (size >
        static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        throw TextEncodingError(std::string(field) +
                                " exceeds the supported size limit");
    }
    return static_cast<std::int32_t>(size);
}

std::string ValidateEncoding(std::string_view encoding) {
    if (encoding.empty() || encoding.size() > kMaximumEncodingNameBytes ||
        encoding.find('\0') != std::string_view::npos) {
        throw TextEncodingError("Encoding name is empty or exceeds its bounds");
    }
    return std::string(encoding);
}

struct ConverterCloser {
    void operator()(UConverter* converter) const noexcept {
        if (converter != nullptr) {
            ucnv_close(converter);
        }
    }
};

using Converter = std::unique_ptr<UConverter, ConverterCloser>;

Converter OpenConverter(std::string_view encoding) {
    const std::string name = ValidateEncoding(encoding);
    UErrorCode status = U_ZERO_ERROR;
    Converter converter(ucnv_open(name.c_str(), &status));
    RequireSuccess(status, "Cannot open text encoding");
    return converter;
}

std::vector<UChar> DecodeToUtf16(std::string_view input,
                                 UConverter* converter) {
    const auto input_length = BoundedLength(input.size(), "Encoded input");
    UErrorCode status = U_ZERO_ERROR;
    ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_STOP, nullptr, nullptr,
                        nullptr, &status);
    RequireSuccess(status, "Cannot configure strict decoder");

    std::int32_t output_length = ucnv_toUChars(
        converter, nullptr, 0, input.data(), input_length, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        RequireSuccess(status, "Cannot size decoded text");
    }
    if (output_length == 0) {
        return {};
    }
    status = U_ZERO_ERROR;
    ucnv_resetToUnicode(converter);
    std::vector<UChar> output(static_cast<std::size_t>(output_length));
    output_length = ucnv_toUChars(converter, output.data(), output_length,
                                  input.data(), input_length, &status);
    RequireSuccess(status, "Cannot decode text");
    output.resize(static_cast<std::size_t>(output_length));
    return output;
}

std::vector<UChar> Utf8ToUtf16(std::string_view input) {
    const auto input_length = BoundedLength(input.size(), "UTF-8 input");
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t output_length = 0;
    u_strFromUTF8(nullptr, 0, &output_length, input.data(), input_length,
                  &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        RequireSuccess(status, "Cannot size UTF-8 input");
    }
    status = U_ZERO_ERROR;
    std::vector<UChar> output(static_cast<std::size_t>(output_length));
    u_strFromUTF8(output.data(), output_length, nullptr, input.data(),
                  input_length, &status);
    RequireSuccess(status, "Cannot decode UTF-8 input");
    return output;
}

std::string Utf16ToUtf8(const std::vector<UChar>& input,
                        std::size_t maximum_output_bytes) {
    if (input.empty()) {
        return {};
    }
    const auto input_length = BoundedLength(input.size(), "Decoded text");
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t output_length = 0;
    u_strToUTF8(nullptr, 0, &output_length, input.data(), input_length,
                &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        RequireSuccess(status, "Cannot size UTF-8 output");
    }
    if (static_cast<std::size_t>(output_length) > maximum_output_bytes) {
        throw TextEncodingError("Decoded text exceeds the output limit");
    }
    status = U_ZERO_ERROR;
    std::string output(static_cast<std::size_t>(output_length), '\0');
    u_strToUTF8(output.data(), output_length, nullptr, input.data(),
                input_length, &status);
    RequireSuccess(status, "Cannot encode UTF-8 output");
    return output;
}

}  // namespace

std::string DecodeToUtf8(std::string_view input, std::string_view encoding,
                         std::size_t maximum_output_bytes) {
    Converter converter = OpenConverter(encoding);
    if (input.empty()) {
        return {};
    }
    return Utf16ToUtf8(DecodeToUtf16(input, converter.get()),
                       maximum_output_bytes);
}

std::string EncodeFromUtf8(std::string_view input, std::string_view encoding,
                           std::size_t maximum_output_bytes) {
    Converter converter = OpenConverter(encoding);
    if (input.empty()) {
        return {};
    }
    const std::vector<UChar> utf16 = Utf8ToUtf16(input);
    const auto input_length = BoundedLength(utf16.size(), "UTF-16 input");

    UErrorCode status = U_ZERO_ERROR;
    ucnv_setFromUCallBack(converter.get(), UCNV_FROM_U_CALLBACK_STOP, nullptr,
                          nullptr, nullptr, &status);
    RequireSuccess(status, "Cannot configure strict encoder");
    std::int32_t output_length = ucnv_fromUChars(
        converter.get(), nullptr, 0, utf16.data(), input_length, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        RequireSuccess(status, "Cannot size encoded text");
    }
    if (static_cast<std::size_t>(output_length) > maximum_output_bytes) {
        throw TextEncodingError("Encoded text exceeds the output limit");
    }
    status = U_ZERO_ERROR;
    ucnv_resetFromUnicode(converter.get());
    std::string output(static_cast<std::size_t>(output_length), '\0');
    output_length =
        ucnv_fromUChars(converter.get(), output.data(), output_length,
                        utf16.data(), input_length, &status);
    RequireSuccess(status, "Cannot encode text");
    output.resize(static_cast<std::size_t>(output_length));
    return output;
}

}  // namespace goldendict::core::foundation
