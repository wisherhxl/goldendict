// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_resource_transform.h"
#include "../../../adapters/image_codec.h"

#include <tiffio.h>
#include <QRegularExpression>
#include <QString>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace goldendict::core::formats::stardict {
namespace {

constexpr std::size_t kMaximumResourceSize = 64U * 1024U * 1024U;

bool EqualsAsciiCaseInsensitive(std::string_view left,
                                std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0U; index < left.size(); ++index) {
        const auto lhs = static_cast<unsigned char>(left[index]);
        const auto rhs = static_cast<unsigned char>(right[index]);
        if (std::tolower(lhs) != std::tolower(rhs)) return false;
    }
    return true;
}

bool StartsWithAsciiCaseInsensitive(std::string_view value,
                                    std::size_t offset,
                                    std::string_view prefix) {
    return offset <= value.size() && prefix.size() <= value.size() - offset &&
           EqualsAsciiCaseInsensitive(value.substr(offset, prefix.size()),
                                      prefix);
}

bool IsTiff(std::string_view resource_id) {
    const auto dot = resource_id.find_last_of('.');
    if (dot == std::string_view::npos) return false;
    const auto extension = resource_id.substr(dot);
    return EqualsAsciiCaseInsensitive(extension, ".tif") ||
           EqualsAsciiCaseInsensitive(extension, ".tiff");
}

bool IsCss(std::string_view resource_id) {
    const auto dot = resource_id.find_last_of('.');
    return dot != std::string_view::npos &&
           EqualsAsciiCaseInsensitive(resource_id.substr(dot), ".css");
}

void CheckOutputSize(std::size_t size) {
    if (size > kMaximumResourceSize) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Transformed StarDict resource exceeds the "
                                "size limit");
    }
}

void AppendBounded(std::string* output, std::string_view value) {
    if (value.size() > kMaximumResourceSize - output->size()) {
        CheckOutputSize(kMaximumResourceSize + 1U);
    }
    output->append(value);
}

struct TiffMemory final {
    const std::vector<std::byte>* bytes = nullptr;
    std::size_t position = 0U;
};

tmsize_t ReadTiff(thandle_t handle, void* destination, tmsize_t requested) {
    if (requested <= 0) return 0;
    auto& memory = *static_cast<TiffMemory*>(handle);
    const auto remaining = memory.bytes->size() - memory.position;
    const auto count = std::min<std::size_t>(
        remaining, static_cast<std::size_t>(requested));
    std::memcpy(destination, memory.bytes->data() + memory.position, count);
    memory.position += count;
    return static_cast<tmsize_t>(count);
}

tmsize_t WriteTiff(thandle_t, void*, tmsize_t) { return 0; }

toff_t SeekTiff(thandle_t handle, toff_t offset, int origin) {
    auto& memory = *static_cast<TiffMemory*>(handle);
    std::uint64_t base = 0U;
    if (origin == SEEK_CUR) {
        base = memory.position;
    } else if (origin == SEEK_END) {
        base = memory.bytes->size();
    } else if (origin != SEEK_SET) {
        return static_cast<toff_t>(-1);
    }
    if (offset > std::numeric_limits<std::uint64_t>::max() - base) {
        return static_cast<toff_t>(-1);
    }
    const auto position = base + offset;
    if (position > memory.bytes->size()) return static_cast<toff_t>(-1);
    memory.position = static_cast<std::size_t>(position);
    return static_cast<toff_t>(memory.position);
}

int CloseTiff(thandle_t) { return 0; }
toff_t SizeTiff(thandle_t handle) {
    return static_cast<toff_t>(
        static_cast<TiffMemory*>(handle)->bytes->size());
}
int MapTiff(thandle_t, void**, toff_t*) { return 0; }
void UnmapTiff(thandle_t, void*, toff_t) {}

void AppendLittleEndian16(std::vector<std::byte>* bytes, std::uint16_t value) {
    bytes->push_back(static_cast<std::byte>(value & 0xffU));
    bytes->push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}

void AppendLittleEndian32(std::vector<std::byte>* bytes, std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        bytes->push_back(static_cast<std::byte>((value >> shift) & 0xffU));
    }
}

std::vector<std::byte> MakeMonochromeBmp(
    std::uint32_t width, std::uint32_t height,
    const std::vector<std::byte>& top_down_pixels, std::size_t row_stride) {
    constexpr std::uint32_t kHeaderSize = 14U + 40U + 8U;
    const auto image_size = static_cast<std::uint32_t>(row_stride * height);
    std::vector<std::byte> result;
    result.reserve(kHeaderSize + image_size);
    result.push_back(std::byte{'B'});
    result.push_back(std::byte{'M'});
    AppendLittleEndian32(&result, kHeaderSize + image_size);
    AppendLittleEndian16(&result, 0U);
    AppendLittleEndian16(&result, 0U);
    AppendLittleEndian32(&result, kHeaderSize);
    AppendLittleEndian32(&result, 40U);
    AppendLittleEndian32(&result, width);
    AppendLittleEndian32(&result, height);
    AppendLittleEndian16(&result, 1U);
    AppendLittleEndian16(&result, 1U);
    AppendLittleEndian32(&result, 0U);
    AppendLittleEndian32(&result, image_size);
    // Qt 5's BMP writer records the 120-DPI acceptance display density.
    // Preserve that inert metadata so the converted payload remains byte-
    // compatible with the frozen product; it does not affect image pixels.
    AppendLittleEndian32(&result, 4724U);
    AppendLittleEndian32(&result, 4724U);
    AppendLittleEndian32(&result, 2U);
    AppendLittleEndian32(&result, 2U);
    result.insert(result.end(), {std::byte{0xff}, std::byte{0xff},
                                 std::byte{0xff}, std::byte{0x00},
                                 std::byte{0x00}, std::byte{0x00},
                                 std::byte{0x00}, std::byte{0x00}});
    for (std::uint32_t row = height; row > 0U; --row) {
        const auto begin = top_down_pixels.begin() +
                           static_cast<std::ptrdiff_t>((row - 1U) * row_stride);
        result.insert(result.end(), begin,
                      begin + static_cast<std::ptrdiff_t>(row_stride));
    }
    return result;
}

std::vector<std::byte> ConvertLegacyMonochromeTiff(
    const std::vector<std::byte>& data,
    const dictionary::RequestOptions& options) {
    if (data.empty()) return {};
    TiffMemory memory{&data, 0U};
    TIFF* raw = TIFFClientOpen("StarDict resource", "r", &memory, ReadTiff,
                               WriteTiff, SeekTiff, CloseTiff, SizeTiff,
                               MapTiff, UnmapTiff);
    if (raw == nullptr) return {};
    const auto close = [](TIFF* value) { TIFFClose(value); };
    std::unique_ptr<TIFF, decltype(close)> tiff(raw, close);

    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint16_t bits_per_sample = 1U;
    std::uint16_t samples_per_pixel = 1U;
    if (TIFFGetField(tiff.get(), TIFFTAG_IMAGEWIDTH, &width) != 1 ||
        TIFFGetField(tiff.get(), TIFFTAG_IMAGELENGTH, &height) != 1 ||
        width == 0U || height == 0U) {
        return {};
    }
    TIFFGetFieldDefaulted(tiff.get(), TIFFTAG_BITSPERSAMPLE,
                          &bits_per_sample);
    TIFFGetFieldDefaulted(tiff.get(), TIFFTAG_SAMPLESPERPIXEL,
                          &samples_per_pixel);
    if (bits_per_sample != 1U || samples_per_pixel != 1U) return {};

    const std::uint64_t row_stride =
        (static_cast<std::uint64_t>(width) + 31U) / 32U * 4U;
    if (row_stride == 0U || height > kMaximumResourceSize / row_stride ||
        row_stride * height > kMaximumResourceSize - 62U) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Decoded StarDict TIFF exceeds the size limit");
    }
    const auto scanline_size = TIFFScanlineSize64(tiff.get());
    if (scanline_size == 0U || scanline_size > row_stride) return {};
    std::vector<std::byte> pixels(
        static_cast<std::size_t>(row_stride * height), std::byte{0});
    for (std::uint32_t row = 0U; row < height; ++row) {
        dictionary::CheckRequest(options);
        auto* scanline = pixels.data() +
                         static_cast<std::size_t>(row * row_stride);
        if (TIFFReadScanline(tiff.get(), scanline, row, 0U) < 0) return {};
    }
    dictionary::CheckRequest(options);
    return MakeMonochromeBmp(width, height, pixels,
                             static_cast<std::size_t>(row_stride));
}

std::string StripCssComments(std::string_view css,
                             const dictionary::RequestOptions& options) {
    std::string result;
    result.reserve(css.size());
    std::size_t position = 0U;
    while (position < css.size()) {
        dictionary::CheckRequest(options);
        const auto comment = css.find("/*", position);
        if (comment == std::string_view::npos) {
            AppendBounded(&result, css.substr(position));
            break;
        }
        AppendBounded(&result, css.substr(position, comment - position));
        const auto end = css.find("*/", comment + 2U);
        if (end == std::string_view::npos) {
            AppendBounded(&result, css.substr(comment));
            break;
        }
        position = end + 2U;
    }
    return result;
}

std::string RewriteCssUrls(std::string_view css, std::string_view dictionary_id,
                           const dictionary::RequestOptions& options) {
    // Use the same expression engine and captures as frozen Qt 5, including
    // greedy matching and backtracking across unquoted URLs.
    constexpr std::string_view kPattern =
        R"(url\(\s*(['"]?)([^'"]*)(['"]?)\s*\))";
    int error = 0;
    PCRE2_SIZE error_offset = 0U;
    const auto free_code = [](pcre2_code* value) { pcre2_code_free(value); };
    std::unique_ptr<pcre2_code, decltype(free_code)> code(
        pcre2_compile(reinterpret_cast<PCRE2_SPTR>(kPattern.data()),
                      kPattern.size(), PCRE2_CASELESS, &error, &error_offset,
                      nullptr),
        free_code);
    const auto free_match = [](pcre2_match_data* value) {
        pcre2_match_data_free(value);
    };
    std::unique_ptr<pcre2_match_data, decltype(free_match)> match(
        code ? pcre2_match_data_create_from_pattern(code.get(), nullptr)
             : nullptr,
        free_match);
    const auto free_context = [](pcre2_match_context* value) {
        pcre2_match_context_free(value);
    };
    std::unique_ptr<pcre2_match_context, decltype(free_context)> context(
        pcre2_match_context_create(nullptr), free_context);
    if (!code || !match || !context) {
        throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                "Could not initialize StarDict CSS rewriting");
    }
    pcre2_set_match_limit(context.get(), 1000000U);
    pcre2_set_depth_limit(context.get(), 1000U);
    pcre2_set_heap_limit(context.get(), 1024U);
    std::string result;
    result.reserve(css.size());
    std::size_t position = 0U;
    while (position < css.size()) {
        dictionary::CheckRequest(options);
        const int status = pcre2_match(
            code.get(), reinterpret_cast<PCRE2_SPTR>(css.data()), css.size(),
            position, 0U, match.get(), context.get());
        if (status == PCRE2_ERROR_NOMATCH) break;
        if (status < 0) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    "StarDict CSS rewriting exceeded its limits");
        }
        const auto* offsets = pcre2_get_ovector_pointer(match.get());
        const auto capture = [&](std::size_t index) {
            return css.substr(offsets[index * 2U],
                              offsets[index * 2U + 1U] - offsets[index * 2U]);
        };
        AppendBounded(&result, css.substr(position, offsets[0] - position));
        const auto url = capture(2U);
        if (url.find(":/") != std::string_view::npos ||
            url.find("data:") != std::string_view::npos) {
            AppendBounded(&result, capture(0U));
        } else {
            AppendBounded(&result, "url(");
            AppendBounded(&result, capture(1U));
            AppendBounded(&result, "bres://");
            AppendBounded(&result, dictionary_id);
            AppendBounded(&result, "/");
            AppendBounded(&result, url);
            AppendBounded(&result, capture(3U));
            AppendBounded(&result, ")");
        }
        position = offsets[1];
    }
    AppendBounded(&result, css.substr(position));
    return result;
}

std::string IsolateCss(std::string css_bytes, std::string_view dictionary_id,
                       const dictionary::RequestOptions& options) {
    const auto stripped = StripCssComments(css_bytes, options);
    const QString css = QString::fromUtf8(stripped.data(), stripped.size());
    const QString prefix = QStringLiteral("#gdfrom-") +
                           QString::fromUtf8(dictionary_id.data(),
                                             dictionary_id.size());
    const QRegularExpression selector_stops(QStringLiteral("[ \\*\\>\\+,;:\\[\\{\\]]"));
    const QRegularExpression rule_stops(QStringLiteral("[,;\\{]"));
    QString result;
    std::size_t output_bytes = 0U;
    const auto append = [&](const QString& value) {
        dictionary::CheckRequest(options);
        const auto bytes = static_cast<std::size_t>(value.toUtf8().size());
        if (bytes > kMaximumResourceSize - output_bytes) {
            CheckOutputSize(kMaximumResourceSize + 1U);
        }
        output_bytes += bytes;
        result.append(value);
    };
    qsizetype current = 0;
    while (current < css.size()) {
        dictionary::CheckRequest(options);
        const QChar ch = css[current];
        if (ch == '@') {
            qsizetype end = -1;
            if (css.mid(current, 7).compare("@import", Qt::CaseInsensitive) == 0 ||
                css.mid(current, 10).compare("@font-face", Qt::CaseInsensitive) == 0 ||
                css.mid(current, 10).compare("@namespace", Qt::CaseInsensitive) == 0 ||
                css.mid(current, 8).compare("@charset", Qt::CaseInsensitive) == 0) {
                end = css.indexOf(';', current);
                const auto brace = css.indexOf('{', current);
                if (brace > 0 && end > brace) end = brace - 1;
            } else if (css.mid(current, 6).compare("@media", Qt::CaseInsensitive) == 0) {
                end = css.indexOf('{', current);
            } else if (css.mid(current, 5).compare("@page", Qt::CaseInsensitive) == 0) {
                end = css.indexOf('}', current);
                if (end < 0) break;
                current = end + 1;
                continue;
            } else {
                end = css.indexOf('}', current);
            }
            append(css.mid(current, end < 0 ? -1 : end - current + 1));
            if (end < 0) break;
            current = end + 1;
            continue;
        }
        if (ch == '{') {
            const auto end = css.indexOf('}', current);
            append(css.mid(current, end < 0 ? -1 : end - current + 1));
            if (end < 0) break;
            current = end + 1;
            continue;
        }
        if (ch.isLetter() || ch == '.' || ch == '#' || ch == '*' ||
            ch == '\\' || ch == ':') {
            if (ch.isLetter() || ch == '*') {
                QChar name_character;
                for (qsizetype name = current; name < css.size(); ++name) {
                    name_character = css[name];
                    if (name_character.isLetterOrNumber() ||
                        name_character.isMark() || name_character == '_' ||
                        name_character == '-' ||
                        (name_character == '*' && name == current)) continue;
                    if (name_character == '|') {
                        append(css.mid(current, name - current + 1));
                        current = name + 1;
                    }
                    break;
                }
                if (name_character == '|') continue;
            }
            const auto selector_end = css.indexOf(selector_stops, current + 1);
            const auto selector = css.mid(
                current, selector_end < 0 ? -1 : selector_end - current);
            if (selector_end < 0) {
                append(selector);
                break;
            }
            const auto trimmed = selector.trimmed();
            if (trimmed.compare("body", Qt::CaseInsensitive) == 0 ||
                trimmed.compare("html", Qt::CaseInsensitive) == 0) {
                append(selector + " " + prefix + " ");
                current += 4;
            } else {
                append(prefix + " ");
            }
            const auto end = css.indexOf(rule_stops, current);
            append(css.mid(current, end < 0 ? -1 : end - current));
            if (end < 0) break;
            current = end;
            continue;
        }
        append(QString(ch));
        ++current;
    }
    return result.toUtf8().toStdString();
}

}  // namespace

dictionary::Resource TransformResource(
    std::string_view normalized_id, std::string_view dictionary_id,
    std::vector<std::byte> data,
    const dictionary::RequestOptions& options) {
    dictionary::CheckRequest(options);
    dictionary::Resource result;
    result.id = std::string(normalized_id);
    result.media_type = dictionary::MediaTypeForResourceId(normalized_id);
    if (IsTiff(normalized_id)) {
        std::vector<std::byte> converted;
        try {
            converted = image_codec::DecodeToBmp(
                data, kMaximumResourceSize,
                [&options] { dictionary::CheckRequest(options); });
        } catch (const std::length_error& error) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    error.what());
        }
        if (converted.empty()) {
            converted = ConvertLegacyMonochromeTiff(data, options);
        }
        if (!converted.empty()) {
            result.media_type = "image/bmp";
            data = std::move(converted);
        }
    } else if (IsCss(normalized_id)) {
        std::string css = QString::fromUtf8(
            reinterpret_cast<const char*>(data.data()), data.size())
                              .toUtf8().toStdString();
        CheckOutputSize(css.size());
        css = RewriteCssUrls(css, dictionary_id, options);
        css = IsolateCss(std::move(css), dictionary_id, options);
        CheckOutputSize(css.size());
        data.assign(reinterpret_cast<const std::byte*>(css.data()),
                    reinterpret_cast<const std::byte*>(css.data() + css.size()));
        result.media_type = "text/css";
    }
    dictionary::CheckRequest(options);
    result.data = std::move(data);
    return result;
}

}  // namespace goldendict::core::formats::stardict
