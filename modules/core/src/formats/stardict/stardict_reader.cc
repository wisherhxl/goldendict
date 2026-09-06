// SPDX-License-Identifier: GPL-3.0-or-later

#include "stardict_reader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

#include <zlib.h>

#include "../../dictionary/generated_index.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::stardict {

namespace {

constexpr std::string_view kInfoSignature = "StarDict's dict ifo file";
constexpr std::uintmax_t kMaximumIndexSize = 256U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumDictionarySize =
    static_cast<std::uintmax_t>(2U) * 1024U * 1024U * 1024U;
constexpr std::string_view kGeneratedIndexFormat = "stardict-records-v3";

bool HasPrefix(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() &&
           text.compare(0, prefix.size(), prefix) == 0;
}

std::size_t Utf8CodePointCount(std::string_view text) noexcept {
    return static_cast<std::size_t>(
        std::count_if(text.begin(), text.end(), [](char byte) {
            return (static_cast<unsigned char>(byte) & 0xc0U) != 0x80U;
        }));
}

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

bool IsCompressedCompanion(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension == ".gz" || extension == ".dz";
}

std::optional<std::filesystem::path> ResolveOptionalCompanion(
    const std::filesystem::path& base_path,
    std::initializer_list<std::string_view> suffixes) {
    for (const auto suffix : suffixes) {
        auto candidate = base_path;
        candidate += suffix;
        std::error_code filesystem_error;
        if (std::filesystem::exists(candidate, filesystem_error)) {
            return candidate;
        }
        if (filesystem_error) {
            Throw(ErrorCode::kMissingFile, candidate,
                  "Cannot inspect companion file");
        }
    }
    return std::nullopt;
}

std::filesystem::path ResolveRequiredCompanion(
    const std::filesystem::path& base_path,
    std::initializer_list<std::string_view> suffixes,
    std::string_view description) {
    if (const auto path = ResolveOptionalCompanion(base_path, suffixes);
        path.has_value()) {
        return *path;
    }
    auto expected_path = base_path;
    expected_path += *suffixes.begin();
    Throw(ErrorCode::kMissingFile, expected_path,
          "Cannot find required " + std::string(description));
}

gzFile OpenCompressedFile(const std::filesystem::path& path) {
#ifdef _WIN32
    return gzopen_w(path.c_str(), "rb");
#else
    return gzopen(path.c_str(), "rb");
#endif
}

std::string ReadCompressedFile(const std::filesystem::path& path,
                               ErrorCode error_code,
                               std::uintmax_t maximum_size,
                               std::string_view description) {
    std::error_code filesystem_error;
    const auto compressed_size =
        std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        Throw(ErrorCode::kMissingFile, path,
              "Cannot inspect compressed " + std::string(description));
    }
    if (compressed_size > maximum_size) {
        Throw(error_code, path,
              "Compressed " + std::string(description) +
                  " exceeds the supported size limit");
    }

    std::ifstream header(path, std::ios::binary);
    if (!header) {
        Throw(ErrorCode::kMissingFile, path,
              "Cannot open compressed " + std::string(description));
    }
    std::array<unsigned char, 2> signature{};
    if (!header.read(reinterpret_cast<char*>(signature.data()),
                     static_cast<std::streamsize>(signature.size()))) {
        Throw(
            error_code, path,
            "Compressed " + std::string(description) + " header is truncated");
    }
    if (signature[0] != 0x1fU || signature[1] != 0x8bU) {
        Throw(error_code, path,
              "Compressed " + std::string(description) +
                  " has an invalid gzip signature");
    }

    gzFile input = OpenCompressedFile(path);
    if (input == nullptr) {
        Throw(ErrorCode::kMissingFile, path,
              "Cannot open compressed " + std::string(description));
    }

    std::string data;
    std::array<char, 64U * 1024U> buffer{};
    while (true) {
        const int count =
            gzread(input, buffer.data(), static_cast<unsigned>(buffer.size()));
        if (count > 0) {
            const auto size = static_cast<std::size_t>(count);
            if (size > maximum_size - data.size()) {
                gzclose(input);
                Throw(error_code, path,
                      "Decompressed " + std::string(description) +
                          " exceeds the supported size limit");
            }
            data.append(buffer.data(), size);
            continue;
        }
        if (count < 0) {
            int zlib_error = Z_OK;
            const char* message = gzerror(input, &zlib_error);
            const std::string detail =
                message == nullptr ? "Unknown decompression error" : message;
            gzclose(input);
            Throw(error_code, path,
                  "Cannot decompress " + std::string(description) + ": " +
                      detail);
        }
        break;
    }
    if (gzclose(input) != Z_OK) {
        Throw(error_code, path,
              "Compressed " + std::string(description) +
                  " checksum or trailer is invalid");
    }
    return data;
}

std::string ReadCompanion(const std::filesystem::path& path,
                          ErrorCode error_code, std::uintmax_t maximum_size,
                          std::string_view description) {
    return IsCompressedCompanion(path)
               ? ReadCompressedFile(path, error_code, maximum_size, description)
               : ReadFile(path, error_code, maximum_size);
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
    const auto& book_name = RequireField(fields, "bookname", info_path);
    const auto word_count = ParseUnsigned(
        RequireField(fields, "wordcount", info_path), info_path, "wordcount");
    const auto index_file_size =
        ParseUnsigned(RequireField(fields, "idxfilesize", info_path), info_path,
                      "idxfilesize");
    const auto& same_type_sequence =
        RequireField(fields, "sametypesequence", info_path);
    if (same_type_sequence != "m" && same_type_sequence != "h") {
        Throw(ErrorCode::kUnsupportedFeature, info_path,
              "Only sametypesequence=m or h is supported in this increment");
    }

    auto base_path = info_path;
    base_path.replace_extension();
    const auto index_path = ResolveRequiredCompanion(
        base_path, {".idx", ".idx.gz", ".idx.dz", ".IDX", ".IDX.GZ", ".IDX.DZ"},
        "StarDict index file");
    const auto dictionary_path = ResolveRequiredCompanion(
        base_path, {".dict", ".dict.dz", ".DICT", ".dict.DZ"},
        "StarDict dictionary data file");
    const auto resolved_synonym_path = ResolveOptionalCompanion(
        base_path,
        {".syn", ".syn.gz", ".syn.dz", ".SYN", ".SYN.GZ", ".SYN.DZ"});
    const auto declared_synonym_count = [&fields, &info_path]() {
        const auto declared = fields.find("synwordcount");
        return declared == fields.end()
                   ? std::uint64_t{0}
                   : ParseUnsigned(declared->second, info_path, "synwordcount");
    }();
    const std::optional<std::filesystem::path> synonym_path =
        declared_synonym_count == 0U ? std::nullopt : resolved_synonym_path;
    std::vector<std::filesystem::path> source_paths = {info_path, index_path,
                                                       dictionary_path};
    if (synonym_path.has_value()) {
        source_paths.push_back(*synonym_path);
    }
    std::optional<dictionary::SourceSnapshot> initial_sources;
    if (generated_index_path.has_value()) {
        try {
            initial_sources = dictionary::CaptureSourceSnapshot(source_paths);
        } catch (const dictionary::GeneratedIndexError& error) {
            Throw(ErrorCode::kIndexStorage, *generated_index_path,
                  error.what());
        }
    }

    Reader reader;
    reader.metadata_.book_name = book_name;
    if (const auto iterator = fields.find("lang_from");
        iterator != fields.end()) {
        reader.metadata_.source_language = iterator->second;
    }
    if (const auto iterator = fields.find("lang_to");
        iterator != fields.end()) {
        reader.metadata_.target_language = iterator->second;
    }
    const auto append_metadata = [&reader, &fields](std::string_view key,
                                                    std::string_view label) {
        const auto iterator = fields.find(std::string(key));
        if (iterator == fields.end() || iterator->second.empty())
            return;
        if (!reader.metadata_.description.empty())
            reader.metadata_.description += "\n\n";
        reader.metadata_.description += std::string(label) + iterator->second;
    };
    append_metadata("copyright", "Copyright: ");
    append_metadata("author", "Author: ");
    append_metadata("email", "E-mail: ");
    append_metadata("website", "Website: ");
    append_metadata("date", "Date: ");
    append_metadata("description", "");
    reader.metadata_.word_count = word_count;
    reader.metadata_.index_file_size = index_file_size;
    reader.metadata_.same_type_sequence = same_type_sequence;

    reader.dictionary_data_ =
        ReadCompanion(dictionary_path, ErrorCode::kInvalidDictionary,
                      kMaximumDictionarySize, "dictionary data");

    const auto parse_source_index = [&reader, &index_path, &synonym_path,
                                     declared_synonym_count, &info_path]() {
        const std::string index_data = ReadCompanion(
            index_path, ErrorCode::kInvalidIndex, kMaximumIndexSize, "index");
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
            record.primary_headword = record.headword;
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
        if (synonym_path.has_value()) {
            const std::string synonym_data =
                ReadCompanion(*synonym_path, ErrorCode::kInvalidIndex,
                              kMaximumIndexSize, "synonym index");
            std::size_t synonym_cursor = 0U;
            std::size_t synonym_count = 0U;
            while (synonym_cursor < synonym_data.size()) {
                const auto terminator = synonym_data.find('\0', synonym_cursor);
                if (terminator == std::string::npos ||
                    terminator == synonym_cursor ||
                    synonym_data.size() - terminator - 1U < 4U) {
                    Throw(ErrorCode::kInvalidIndex, *synonym_path,
                          "Truncated or malformed synonym record");
                }
                const auto article_index =
                    ReadBigEndian32(synonym_data, terminator + 1U);
                if (article_index >= reader.metadata_.word_count) {
                    Throw(ErrorCode::kInvalidIndex, *synonym_path,
                          "Synonym references an invalid index record");
                }
                IndexRecord record = records[article_index];
                record.headword = synonym_data.substr(
                    synonym_cursor, terminator - synonym_cursor);
                records.push_back(std::move(record));
                synonym_cursor = terminator + 5U;
                ++synonym_count;
            }
            if (declared_synonym_count != synonym_count) {
                Throw(ErrorCode::kInvalidInfo, info_path,
                      "Synonym count does not match synwordcount");
            }
        }
        return records;
    };

    const auto serialize_records = [](const std::vector<IndexRecord>& records) {
        std::string payload;
        AppendBigEndian64(records.size(), &payload);
        for (const auto& record : records) {
            if (record.headword.size() >
                    std::numeric_limits<std::uint32_t>::max() ||
                record.primary_headword.size() >
                    std::numeric_limits<std::uint32_t>::max()) {
                throw dictionary::GeneratedIndexError(
                    "StarDict headword exceeds the index size limit");
            }
            AppendBigEndian32(
                static_cast<std::uint32_t>(record.headword.size()), &payload);
            payload.append(record.headword);
            AppendBigEndian32(
                static_cast<std::uint32_t>(record.primary_headword.size()),
                &payload);
            payload.append(record.primary_headword);
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
        if (record_count < reader.metadata_.word_count ||
            record_count > std::numeric_limits<std::size_t>::max() ||
            record_count > (payload.size() - 8U) / 16U) {
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
            require(cursor, static_cast<std::size_t>(headword_size) + 4U);
            IndexRecord record;
            record.headword = payload.substr(cursor, headword_size);
            cursor += headword_size;
            const auto primary_size = ReadBigEndian32(payload, cursor);
            cursor += 4U;
            require(cursor, static_cast<std::size_t>(primary_size) + 8U);
            record.primary_headword = payload.substr(cursor, primary_size);
            cursor += primary_size;
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

    for (auto& record : reader.index_) {
        try {
            record.folded_headword = foundation::FoldForLookup(record.headword);
        } catch (const foundation::TextFoldingError& error) {
            Throw(ErrorCode::kInvalidIndex, index_path,
                  std::string("Invalid UTF-8 headword: ") + error.what());
        }
        const auto offset = static_cast<std::uint64_t>(record.article_offset);
        const auto size = static_cast<std::uint64_t>(record.article_size);
        if (offset > reader.dictionary_data_.size() ||
            size > reader.dictionary_data_.size() - offset) {
            Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                  "Index record points outside dictionary data");
        }
    }
    try {
        reader.source_snapshot_ =
            dictionary::CaptureSourceSnapshot(source_paths);
    } catch (const dictionary::GeneratedIndexError& error) {
        Throw(ErrorCode::kIndexStorage, info_path, error.what());
    }
    return reader;
}

std::vector<PrimaryArticle> Reader::ReadPrimaryArticles(
    const std::function<void()>& checkpoint) const {
    std::vector<PrimaryArticle> articles;
    articles.reserve(static_cast<std::size_t>(metadata_.word_count));
    for (std::size_t ordinal = 0U;
         ordinal < static_cast<std::size_t>(metadata_.word_count); ++ordinal) {
        if (checkpoint)
            checkpoint();
        const auto& record = index_[ordinal];
        PrimaryArticle article;
        article.record_ordinal = ordinal;
        article.headword = record.headword;
        article.article_offset = record.article_offset;
        article.article_size = record.article_size;
        article.data =
            dictionary_data_.substr(record.article_offset, record.article_size);
        articles.push_back(std::move(article));
    }
    return articles;
}

std::pair<std::vector<std::string>, bool> Reader::EnumerateHeadwords(
    std::size_t offset, std::size_t result_limit, std::size_t byte_limit,
    const std::function<void()>& checkpoint) const {
    try {
        return enumeration_index_.Page(
            index_.size(),
            [this](std::uint32_t ordinal) -> std::string_view {
                return index_[ordinal].headword;
            },
            offset, result_limit, byte_limit, checkpoint);
    } catch (const dictionary::OrderedHeadwordError& error) {
        Throw(ErrorCode::kInvalidIndex, {}, error.what());
    }
}

std::vector<Article> Reader::LookupExact(
    std::string_view headword, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> articles;
    const std::string folded_headword = foundation::FoldForLookup(headword);
    if (folded_headword.empty() || result_limit == 0U) {
        return articles;
    }
    std::size_t record_number = 0;
    for (const auto& record : index_) {
        if (checkpoint && (record_number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (articles.size() == result_limit) {
            break;
        }
        if (record.folded_headword != folded_headword) {
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

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> articles;
    if (result_limit == 0U) {
        return articles;
    }

    const auto matches = RankedPrefixMatches(prefix, checkpoint);

    articles.reserve(std::min(result_limit, matches.size()));
    for (const auto* record : matches) {
        if (articles.size() == result_limit) {
            break;
        }
        Article article;
        article.headword = record->headword;
        article.data = dictionary_data_.substr(record->article_offset,
                                               record->article_size);
        articles.push_back(std::move(article));
    }
    return articles;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> suggestions;
    if (result_limit == 0U) {
        return suggestions;
    }
    const auto matches = RankedPrefixMatches(prefix, checkpoint);
    std::unordered_set<std::string> seen;
    suggestions.reserve(std::min(result_limit, matches.size()));
    for (const auto* record : matches) {
        if (suggestions.size() == result_limit) {
            break;
        }
        if (seen.insert(record->headword).second) {
            suggestions.push_back(record->headword);
        }
    }
    return suggestions;
}

std::vector<std::string> Reader::FindHeadwordsForSynonym(
    std::string_view headword, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(headword);
    std::vector<std::string> result;
    if (result_limit == 0U)
        return result;
    std::unordered_set<std::string> seen;
    std::size_t number = 0U;
    for (const auto& record : index_) {
        if (checkpoint && (number++ % 1024U) == 0U)
            checkpoint();
        if (record.folded_headword == folded &&
            foundation::FoldForLookup(record.primary_headword) != folded &&
            seen.insert(record.primary_headword).second) {
            result.push_back(record.primary_headword);
            if (result.size() == result_limit)
                break;
        }
    }
    return result;
}

std::vector<const Reader::IndexRecord*> Reader::RankedPrefixMatches(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded_prefix = foundation::FoldForLookup(prefix);
    if (folded_prefix.empty()) {
        return {};
    }
    std::vector<const IndexRecord*> matches;
    std::size_t record_number = 0;
    for (const auto& record : index_) {
        if (checkpoint && (record_number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (HasPrefix(record.folded_headword, folded_prefix)) {
            matches.push_back(&record);
        }
    }
    std::stable_sort(
        matches.begin(), matches.end(),
        [&folded_prefix](const auto* left, const auto* right) {
            const bool left_exact = left->folded_headword == folded_prefix;
            const bool right_exact = right->folded_headword == folded_prefix;
            if (left_exact != right_exact) {
                return left_exact;
            }
            const auto left_length = Utf8CodePointCount(left->folded_headword);
            const auto right_length =
                Utf8CodePointCount(right->folded_headword);
            if (left_length != right_length) {
                return left_length < right_length;
            }
            if (left->folded_headword != right->folded_headword) {
                return left->folded_headword < right->folded_headword;
            }
            return left->headword < right->headword;
        });
    return matches;
}

}  // namespace goldendict::core::formats::stardict
