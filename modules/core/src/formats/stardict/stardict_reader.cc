// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_reader.h"

#include <charconv>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <system_error>
#include <utility>

#include "../../dictionary/generated_index.h"

namespace goldendict::core::formats::stardict {
namespace {

constexpr std::string_view kInfoSignature = "StarDict's dict ifo file";
constexpr std::uintmax_t kMaximumIndexSize = 256U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumDictionarySize =
    static_cast<std::uintmax_t>(2U) * 1024U * 1024U * 1024U;
constexpr std::string_view kGeneratedIndexFormat = "stardict-records-v1";

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::string ReadFile(const std::filesystem::path& path, ErrorCode error_code,
                     std::uintmax_t maximum_size) {
    std::error_code filesystem_error;
    const auto size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        Throw(ErrorCode::kMissingFile, path, "Cannot read required file");
    }
    if (size > maximum_size ||
        size > static_cast<std::uintmax_t>(
                   std::numeric_limits<std::streamsize>::max())) {
        Throw(error_code, path, "File exceeds the supported size limit");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Throw(ErrorCode::kMissingFile, path, "Cannot open required file");
    }

    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        Throw(error_code, path, "Cannot read complete file");
    }
    return data;
}

std::uint64_t ParseUnsigned(std::string_view text,
                            const std::filesystem::path& path,
                            std::string_view field_name) {
    std::uint64_t value = 0;
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || result.ec != std::errc{} ||
        result.ptr != text.data() + text.size()) {
        Throw(ErrorCode::kInvalidInfo, path,
              "Invalid unsigned value for " + std::string(field_name));
    }
    return value;
}

std::map<std::string, std::string> ParseInfoFields(
    const std::string& contents, const std::filesystem::path& path) {
    std::istringstream input(contents);
    std::string line;
    if (!std::getline(input, line)) {
        Throw(ErrorCode::kInvalidInfo, path, "Info file is empty");
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line != kInfoSignature) {
        Throw(ErrorCode::kInvalidInfo, path, "Invalid StarDict signature");
    }

    std::map<std::string, std::string> fields;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos || separator == 0U) {
            Throw(ErrorCode::kInvalidInfo, path, "Malformed info field");
        }
        fields.insert_or_assign(line.substr(0, separator),
                                line.substr(separator + 1U));
    }
    return fields;
}

const std::string& RequireField(
    const std::map<std::string, std::string>& fields, std::string_view name,
    const std::filesystem::path& path) {
    const auto iterator = fields.find(std::string(name));
    if (iterator == fields.end() || iterator->second.empty()) {
        Throw(ErrorCode::kInvalidInfo, path,
              "Missing required field: " + std::string(name));
    }
    return iterator->second;
}

std::uint32_t ReadBigEndian32(const std::string& data, std::size_t offset) {
    const auto byte = [&data](std::size_t position) {
        return static_cast<std::uint32_t>(
            static_cast<unsigned char>(data[position]));
    };
    return (byte(offset) << 24U) | (byte(offset + 1U) << 16U) |
           (byte(offset + 2U) << 8U) | byte(offset + 3U);
}

void AppendBigEndian32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>((value >> 24U) & 0xffU));
    output->push_back(static_cast<char>((value >> 16U) & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>(value & 0xffU));
}

void AppendBigEndian64(std::uint64_t value, std::string* output) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

std::uint64_t ReadBigEndian64(const std::string& data, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8U; ++index) {
        value =
            (value << 8U) | static_cast<unsigned char>(data[offset + index]);
    }
    return value;
}

}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(message + ": " + path.string()),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(
    const std::filesystem::path& info_path,
    const std::optional<std::filesystem::path>& generated_index_path) {
    auto index_path = info_path;
    index_path.replace_extension(".idx");
    auto dictionary_path = info_path;
    dictionary_path.replace_extension(".dict");
    const std::vector<std::filesystem::path> source_paths = {
        info_path, index_path, dictionary_path};
    std::optional<dictionary::SourceSnapshot> initial_sources;
    if (generated_index_path.has_value()) {
        try {
            initial_sources = dictionary::CaptureSourceSnapshot(source_paths);
        } catch (const dictionary::GeneratedIndexError& error) {
            Throw(ErrorCode::kIndexStorage, *generated_index_path,
                  error.what());
        }
    }

    const std::string info_contents =
        ReadFile(info_path, ErrorCode::kInvalidInfo, 1024U * 1024U);
    const auto fields = ParseInfoFields(info_contents, info_path);

    const auto& version = RequireField(fields, "version", info_path);
    if (version != "2.4.2" && version != "3.0.0") {
        Throw(ErrorCode::kUnsupportedFeature, info_path,
              "Unsupported StarDict version: " + version);
    }
    if (const auto iterator = fields.find("idxoffsetbits");
        iterator != fields.end() && iterator->second != "32") {
        Throw(ErrorCode::kUnsupportedFeature, info_path,
              "Only 32-bit index offsets are supported");
    }
    if (const auto iterator = fields.find("dicttype");
        iterator != fields.end() && !iterator->second.empty()) {
        Throw(ErrorCode::kUnsupportedFeature, info_path,
              "Special dictionary types are not supported");
    }

    Reader reader;
    reader.metadata_.book_name = RequireField(fields, "bookname", info_path);
    reader.metadata_.word_count = ParseUnsigned(
        RequireField(fields, "wordcount", info_path), info_path, "wordcount");
    reader.metadata_.index_file_size =
        ParseUnsigned(RequireField(fields, "idxfilesize", info_path), info_path,
                      "idxfilesize");
    reader.metadata_.same_type_sequence =
        RequireField(fields, "sametypesequence", info_path);
    if (reader.metadata_.same_type_sequence != "m" &&
        reader.metadata_.same_type_sequence != "h") {
        Throw(ErrorCode::kUnsupportedFeature, info_path,
              "Only sametypesequence=m or h is supported in this increment");
    }

    reader.dictionary_data_ = ReadFile(
        dictionary_path, ErrorCode::kInvalidDictionary, kMaximumDictionarySize);

    const auto parse_source_index = [&reader, &index_path]() {
        const std::string index_data =
            ReadFile(index_path, ErrorCode::kInvalidIndex, kMaximumIndexSize);
        if (reader.metadata_.index_file_size != index_data.size()) {
            Throw(ErrorCode::kInvalidIndex, index_path,
                  "Index size does not match idxfilesize");
        }

        std::vector<IndexRecord> records;
        std::size_t cursor = 0;
        while (cursor < index_data.size()) {
            const auto terminator = index_data.find('\0', cursor);
            if (terminator == std::string::npos || terminator == cursor ||
                index_data.size() - terminator - 1U < 8U) {
                Throw(ErrorCode::kInvalidIndex, index_path,
                      "Truncated or malformed index record");
            }
            IndexRecord record;
            record.headword = index_data.substr(cursor, terminator - cursor);
            record.article_offset =
                ReadBigEndian32(index_data, terminator + 1U);
            record.article_size = ReadBigEndian32(index_data, terminator + 5U);
            records.push_back(std::move(record));
            cursor = terminator + 9U;
        }
        if (records.size() != reader.metadata_.word_count) {
            Throw(ErrorCode::kInvalidIndex, index_path,
                  "Index record count does not match wordcount");
        }
        return records;
    };

    const auto serialize_records = [](const std::vector<IndexRecord>& records) {
        std::string payload;
        AppendBigEndian64(records.size(), &payload);
        for (const auto& record : records) {
            if (record.headword.size() >
                std::numeric_limits<std::uint32_t>::max()) {
                throw dictionary::GeneratedIndexError(
                    "StarDict headword exceeds the index size limit");
            }
            AppendBigEndian32(
                static_cast<std::uint32_t>(record.headword.size()), &payload);
            payload.append(record.headword);
            AppendBigEndian32(record.article_offset, &payload);
            AppendBigEndian32(record.article_size, &payload);
        }
        return payload;
    };

    const auto parse_generated_index = [&reader](const std::string& payload) {
        const auto require = [&payload](std::size_t position,
                                        std::size_t size) {
            if (position > payload.size() || size > payload.size() - position) {
                throw dictionary::GeneratedIndexError(
                    "Generated StarDict index is truncated");
            }
        };
        require(0U, 8U);
        const auto record_count = ReadBigEndian64(payload, 0U);
        if (record_count != reader.metadata_.word_count ||
            record_count > std::numeric_limits<std::size_t>::max() ||
            record_count > (payload.size() - 8U) / 12U) {
            throw dictionary::GeneratedIndexError(
                "Generated StarDict index has an invalid record count");
        }
        std::vector<IndexRecord> records;
        records.reserve(static_cast<std::size_t>(record_count));
        std::size_t cursor = 8U;
        for (std::uint64_t index = 0; index < record_count; ++index) {
            require(cursor, 4U);
            const auto headword_size = ReadBigEndian32(payload, cursor);
            cursor += 4U;
            require(cursor, static_cast<std::size_t>(headword_size) + 8U);
            IndexRecord record;
            record.headword = payload.substr(cursor, headword_size);
            cursor += headword_size;
            record.article_offset = ReadBigEndian32(payload, cursor);
            record.article_size = ReadBigEndian32(payload, cursor + 4U);
            cursor += 8U;
            records.push_back(std::move(record));
        }
        if (cursor != payload.size()) {
            throw dictionary::GeneratedIndexError(
                "Generated StarDict index contains trailing data");
        }
        return records;
    };

    if (!generated_index_path.has_value()) {
        reader.index_ = parse_source_index();
    } else {
        try {
            const auto sources =
                dictionary::CaptureSourceSnapshot(source_paths);
            if (sources != *initial_sources) {
                Throw(ErrorCode::kIndexStorage, *generated_index_path,
                      "Dictionary sources changed while opening index");
            }
            auto loaded = dictionary::LoadGeneratedIndex(
                *generated_index_path, kGeneratedIndexFormat, sources);
            auto rebuild_state = loaded.state;
            if (loaded.state == dictionary::GeneratedIndexState::kCurrent) {
                try {
                    reader.index_ = parse_generated_index(loaded.payload);
                    reader.index_state_ = IndexState::kReused;
                } catch (const dictionary::GeneratedIndexError&) {
                    rebuild_state = dictionary::GeneratedIndexState::kCorrupt;
                }
            }
            if (reader.index_state_ != IndexState::kReused) {
                reader.index_ = parse_source_index();
                const auto verified_sources =
                    dictionary::CaptureSourceSnapshot(source_paths);
                if (verified_sources != sources) {
                    Throw(ErrorCode::kIndexStorage, *generated_index_path,
                          "Dictionary sources changed while building index");
                }
                dictionary::StoreGeneratedIndex(
                    *generated_index_path, kGeneratedIndexFormat, sources,
                    serialize_records(reader.index_));
                switch (rebuild_state) {
                    case dictionary::GeneratedIndexState::kMissing:
                        reader.index_state_ = IndexState::kCreated;
                        break;
                    case dictionary::GeneratedIndexState::kStale:
                        reader.index_state_ = IndexState::kRebuiltStale;
                        break;
                    case dictionary::GeneratedIndexState::kCorrupt:
                    case dictionary::GeneratedIndexState::kCurrent:
                        reader.index_state_ = IndexState::kRebuiltCorrupt;
                        break;
                }
            }
            const auto final_sources =
                dictionary::CaptureSourceSnapshot(source_paths);
            if (final_sources != sources) {
                Throw(ErrorCode::kIndexStorage, *generated_index_path,
                      "Dictionary sources changed while opening index");
            }
        } catch (const dictionary::GeneratedIndexError& error) {
            Throw(ErrorCode::kIndexStorage, *generated_index_path,
                  error.what());
        }
    }

    for (const auto& record : reader.index_) {
        const auto offset = static_cast<std::uint64_t>(record.article_offset);
        const auto size = static_cast<std::uint64_t>(record.article_size);
        if (offset > reader.dictionary_data_.size() ||
            size > reader.dictionary_data_.size() - offset) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "Index record points outside dictionary data");
        }
    }
    return reader;
}

std::vector<Article> Reader::LookupExact(std::string_view headword,
                                         std::size_t result_limit) const {
    std::vector<Article> articles;
    for (const auto& record : index_) {
        if (articles.size() == result_limit) {
            break;
        }
        if (record.headword != headword) {
            continue;
        }
        Article article;
        article.headword = record.headword;
        article.data =
            dictionary_data_.substr(record.article_offset, record.article_size);
        articles.push_back(std::move(article));
    }
    return articles;
}

}  // namespace goldendict::core::formats::stardict
