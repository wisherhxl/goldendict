// SPDX-License-Identifier: GPL-3.0-or-later

#include "chinese_conversion.h"

#include <filesystem>
#include <stdexcept>

#include <opencc/SimpleConverter.hpp>

#include "../foundation/text_folding.h"
#include "../foundation/utf8.h"

namespace goldendict::core::dictionary {
namespace {

const char* ConfigurationFilename(ChineseConversionVariant variant) {
    switch (variant) {
        case ChineseConversionVariant::kSimplifiedToTaiwan:
            return "s2tw.json";
        case ChineseConversionVariant::kSimplifiedToHongKong:
            return "s2hk.json";
        case ChineseConversionVariant::kTraditionalToSimplified:
            return "t2s.json";
    }
    throw TransliterationError("Unsupported Chinese conversion variant");
}

}  // namespace

std::optional<std::string> ConvertChinese(
    std::string_view text, ChineseConversionVariant variant,
    std::string_view configuration_directory, std::size_t output_limit) {
    if (text.size() > kMaximumTransliterationInputBytes ||
        !foundation::IsValidUtf8(text) || text.find('\0') != text.npos) {
        throw TransliterationError(
            "Chinese conversion input is not valid bounded UTF-8");
    }
    if (configuration_directory.empty() ||
        configuration_directory.find('\0') != configuration_directory.npos) {
        throw TransliterationError(
            "Chinese conversion configuration directory is invalid");
    }

    try {
        const std::string folded = foundation::FoldSimpleCase(text);
        const auto configuration =
            std::filesystem::path(configuration_directory) /
            ConfigurationFilename(variant);
        const opencc::SimpleConverter converter(configuration.string());
        std::string output = converter.Convert(folded);
        if (output.size() > output_limit || !foundation::IsValidUtf8(output) ||
            output.find('\0') != output.npos) {
            throw TransliterationError(
                "Chinese conversion output exceeds its validated bounds");
        }
        if (output == folded) {
            return std::nullopt;
        }
        return output;
    } catch (const TransliterationError&) {
        throw;
    } catch (const std::exception& error) {
        throw TransliterationError(std::string("Chinese conversion failed: ") +
                                   error.what());
    }
}

}  // namespace goldendict::core::dictionary
