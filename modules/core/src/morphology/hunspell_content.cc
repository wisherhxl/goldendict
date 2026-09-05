// SPDX-License-Identifier: GPL-3.0-or-later

#include "hunspell_content.h"

#include <charconv>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>

#include "../foundation/text_encoding.h"

namespace goldendict::core::morphology::hunspell {
namespace {

[[noreturn]] void Throw(ContentErrorCode code,
                        const std::filesystem::path& path,
                        std::string message) {
    throw ContentError(code, path, std::move(message));
}

bool IsAcceptedExtension(const std::filesystem::path& path,
                         std::string_view lower, std::string_view upper) {
    const auto extension = path.extension().string();
    return extension == lower || extension == upper;
}

void ValidatePair(const DataFiles& files) {
    if (files.dictionary_id.empty() || files.affix_file.empty() ||
        files.dictionary_file.empty() || !files.affix_file.is_absolute() ||
        !files.dictionary_file.is_absolute() ||
        files.affix_file != files.affix_file.lexically_normal() ||
        files.dictionary_file != files.dictionary_file.lexically_normal() ||
        !IsAcceptedExtension(files.affix_file, ".aff", ".AFF") ||
        !IsAcceptedExtension(files.dictionary_file, ".dic", ".DIC") ||
        files.affix_file.stem().string() != files.dictionary_id ||
        files.dictionary_file.stem().string() != files.dictionary_id ||
        files.affix_file.parent_path().lexically_normal() !=
            files.dictionary_file.parent_path().lexically_normal()) {
        Throw(ContentErrorCode::kUnsafePath, files.affix_file,
              "Invalid Hunspell data-file pair");
    }
}

void ValidateRegularFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || !std::filesystem::exists(status)) {
        Throw(ContentErrorCode::kMissingFile, path,
              "Hunspell data file is unavailable");
    }
    if (std::filesystem::is_symlink(status) ||
        !std::filesystem::is_regular_file(status)) {
        Throw(ContentErrorCode::kUnsafePath, path,
              "Hunspell data file must be a regular non-symlink file");
    }
}

std::string ReadBounded(const std::filesystem::path& path,
                        std::size_t maximum_bytes) {
    ValidateRegularFile(path);
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        Throw(ContentErrorCode::kMissingFile, path,
              "Cannot inspect Hunspell data file");
    }
    if (size > maximum_bytes) {
        Throw(ContentErrorCode::kResourceLimit, path,
              "Hunspell data file exceeds its size limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Throw(ContentErrorCode::kMissingFile, path,
              "Cannot open Hunspell data file");
    }
    std::string bytes(static_cast<std::size_t>(size), '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
        input.bad()) {
        Throw(ContentErrorCode::kMissingFile, path,
              "Cannot read complete Hunspell data file");
    }
    return bytes;
}

template <typename Callback>
void ForEachLine(std::string_view bytes, const std::filesystem::path& path,
                 Callback callback) {
    std::size_t begin = 0U;
    std::size_t line_number = 1U;
    while (begin < bytes.size()) {
        std::size_t end = bytes.find('\n', begin);
        if (end == std::string_view::npos)
            end = bytes.size();
        std::size_t length = end - begin;
        if (length > 0U && bytes[begin + length - 1U] == '\r')
            --length;
        if (length > kMaximumContentLineBytes) {
            Throw(ContentErrorCode::kResourceLimit, path,
                  "Hunspell content line exceeds its size limit");
        }
        callback(bytes.substr(begin, length), line_number);
        if (end == bytes.size())
            break;
        begin = end + 1U;
        ++line_number;
    }
}

std::string ParseEncoding(std::string_view affix_bytes,
                          const std::filesystem::path& path) {
    std::string encoding;
    ForEachLine(
        affix_bytes, path,
        [&](std::string_view line, std::size_t /*line_number*/) {
            if (line.rfind("SET", 0U) != 0U ||
                (line.size() > 3U && line[3U] != ' ' && line[3U] != '\t')) {
                return;
            }
            const auto first = line.find_first_not_of(" \t", 3U);
            const auto last = line.find_last_not_of(" \t");
            if (first == std::string_view::npos || last < first) {
                Throw(ContentErrorCode::kInvalidAffix, path,
                      "Hunspell SET directive has no encoding");
            }
            const auto value = line.substr(first, last - first + 1U);
            if (value.find_first_of(" \t") != std::string_view::npos ||
                value.find('\0') != std::string_view::npos) {
                Throw(ContentErrorCode::kInvalidAffix, path,
                      "Hunspell SET directive is malformed");
            }
            if (!encoding.empty()) {
                Throw(ContentErrorCode::kInvalidAffix, path,
                      "Hunspell affix file has duplicate SET directives");
            }
            encoding.assign(value);
        });
    if (encoding.empty()) {
        Throw(ContentErrorCode::kInvalidAffix, path,
              "Hunspell affix file has no SET directive");
    }
    return encoding;
}

std::string Decode(std::string_view bytes, std::string_view encoding,
                   const std::filesystem::path& path,
                   std::size_t maximum_output_bytes) {
    try {
        return foundation::DecodeToUtf8(bytes, encoding, maximum_output_bytes);
    } catch (const foundation::TextEncodingError& error) {
        Throw(ContentErrorCode::kInvalidEncoding, path, error.what());
    }
}

void ValidateEncoding(std::string_view encoding,
                      const std::filesystem::path& path) {
    try {
        foundation::DecodeToUtf8({}, encoding, 0U);
    } catch (const foundation::TextEncodingError& error) {
        Throw(ContentErrorCode::kUnsupportedEncoding, path, error.what());
    }
}

std::size_t ParseDictionary(std::string_view decoded,
                            const std::filesystem::path& path) {
    std::size_t declared_count = 0U;
    std::size_t actual_count = 0U;
    bool has_header = false;
    ForEachLine(
        decoded, path, [&](std::string_view line, std::size_t line_number) {
            if (line_number == 1U) {
                if (line.size() >= 3U &&
                    line.substr(0U, 3U) == "\xef\xbb\xbf") {
                    line.remove_prefix(3U);
                }
                if (line.empty()) {
                    Throw(ContentErrorCode::kInvalidDictionary, path,
                          "Hunspell dictionary entry header is empty");
                }
                const auto result = std::from_chars(
                    line.data(), line.data() + line.size(), declared_count);
                if (result.ec != std::errc{} ||
                    result.ptr != line.data() + line.size()) {
                    Throw(ContentErrorCode::kInvalidDictionary, path,
                          "Hunspell dictionary entry header is invalid");
                }
                if (declared_count > kMaximumDictionaryEntries) {
                    Throw(ContentErrorCode::kResourceLimit, path,
                          "Hunspell dictionary entry limit exceeded");
                }
                has_header = true;
            } else if (!line.empty()) {
                ++actual_count;
                if (actual_count > kMaximumDictionaryEntries) {
                    Throw(ContentErrorCode::kResourceLimit, path,
                          "Hunspell dictionary entry limit exceeded");
                }
            }
        });
    if (!has_header) {
        Throw(ContentErrorCode::kInvalidDictionary, path,
              "Hunspell dictionary has no entry header");
    }
    return actual_count;
}

}  // namespace

ContentError::ContentError(ContentErrorCode code, std::filesystem::path path,
                           std::string message)
    : std::runtime_error(path.string() + ": " + std::move(message)),
      code_(code),
      path_(std::move(path)) {}

Content LoadContent(const DataFiles& files) {
    ValidatePair(files);
    Content content;
    content.files = files;
    content.affix_bytes = ReadBounded(files.affix_file, kMaximumAffixBytes);
    content.dictionary_bytes =
        ReadBounded(files.dictionary_file, kMaximumDictionaryBytes);
    if (content.affix_bytes.size() + content.dictionary_bytes.size() >
        kMaximumContentBytes) {
        Throw(ContentErrorCode::kResourceLimit, files.affix_file,
              "Hunspell data pair exceeds its aggregate size limit");
    }
    content.encoding = ParseEncoding(content.affix_bytes, files.affix_file);
    ValidateEncoding(content.encoding, files.affix_file);
    Decode(content.affix_bytes, content.encoding, files.affix_file,
           kMaximumAffixBytes);
    const std::string dictionary =
        Decode(content.dictionary_bytes, content.encoding,
               files.dictionary_file, kMaximumDictionaryBytes);
    content.dictionary_entry_count =
        ParseDictionary(dictionary, files.dictionary_file);
    return content;
}

}  // namespace goldendict::core::morphology::hunspell
