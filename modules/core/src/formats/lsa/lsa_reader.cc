// SPDX-License-Identifier: GPL-3.0-or-later
#include "lsa_reader.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <tuple>
#include <utility>
#include "../../audio/vorbis_decoder.h"
#include "../../foundation/text_encoding.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::lsa {
namespace {
constexpr std::size_t kMaximumFileSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumEntries = 1000000U;
constexpr std::size_t kMaximumNameBytes = 64U * 1024U;
constexpr std::uint64_t kMaximumPcmBytes = 256U * 1024U * 1024U;
constexpr std::string_view kSignature(
    "L\0"
    "9\0S\0A\0\xff",
    9U);

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::uint32_t Le32(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 4U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated LSA integer");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4U; ++i)
        value |=
            static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + i]))
            << (i * 8U);
    return value;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect LSA archive");
    if (size > kMaximumFileSize)
        Throw(ErrorCode::kInvalidDictionary, path,
              "LSA archive exceeds the supported size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        Throw(ErrorCode::kMissingFile, path, "Cannot open LSA archive");
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size())))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete LSA archive");
    return data;
}

std::string StripWav(std::string value) {
    if (value.size() >= 4U) {
        std::string extension = value.substr(value.size() - 4U);
        std::transform(
            extension.begin(), extension.end(), extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".wav")
            value.resize(value.size() - 4U);
    }
    return value;
}

std::string Escape(std::string_view value) {
    std::string escaped;
    for (const char c : value) {
        switch (c) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped.push_back(c);
        }
    }
    return escaped;
}

}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(path.string() + ": " + std::move(message)),
      code_(code) {}

Reader Reader::Open(const std::filesystem::path& path) {
    Reader reader;
    reader.path_ = path;
    reader.file_ = ReadFile(path);
    if (reader.file_.size() < 17U ||
        std::string_view(reader.file_).substr(0U, kSignature.size()) !=
            kSignature)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid LSA signature");
    const auto count = Le32(reader.file_, 9U, path);
    if (count == 0U || count > kMaximumEntries)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid LSA entry count");
    std::size_t cursor = 13U;
    reader.records_.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto name_start = cursor;
        while (cursor + 3U < reader.file_.size() &&
               !(reader.file_[cursor] == '\r' &&
                 reader.file_[cursor + 1U] == '\0' &&
                 reader.file_[cursor + 2U] == '\n' &&
                 reader.file_[cursor + 3U] == '\0')) {
            cursor += 2U;
            if (cursor - name_start > kMaximumNameBytes)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "LSA entry name is too long");
        }
        if (cursor + 3U >= reader.file_.size())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Truncated LSA entry name");
        std::string name;
        try {
            name = StripWav(foundation::DecodeToUtf8(
                std::string_view(reader.file_)
                    .substr(name_start, cursor - name_start),
                "UTF-16LE", kMaximumNameBytes));
        } catch (const foundation::TextEncodingError&) {
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid LSA UTF-16 entry name");
        }
        if (name.empty() || name.find('/') != std::string::npos ||
            name.find('\\') != std::string::npos ||
            name.find('\0') != std::string::npos ||
            name.find('\r') != std::string::npos ||
            name.find('\n') != std::string::npos)
            Throw(ErrorCode::kInvalidDictionary, path, "Unsafe LSA entry name");
        cursor += 4U;
        if (cursor >= reader.file_.size() ||
            (reader.file_[cursor++] != static_cast<char>(0xff) &&
             (reader.file_[cursor - 1U] != '\0' ||
              cursor >= reader.file_.size() ||
              reader.file_[cursor++] != static_cast<char>(0xff))))
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid LSA entry marker");
        std::uint32_t sample_offset = 0U;
        if (index != 0U) {
            sample_offset = Le32(reader.file_, cursor, path);
            cursor += 4U;
            if (cursor >= reader.file_.size() ||
                reader.file_[cursor++] != static_cast<char>(0xff))
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid LSA sample marker");
        }
        const auto sample_length = Le32(reader.file_, cursor, path);
        cursor += 4U;
        reader.records_.push_back(
            {std::move(name), {}, sample_offset, sample_length});
        reader.records_.back().folded =
            foundation::FoldForLookup(reader.records_.back().word);
    }
    reader.vorbis_offset_ = cursor;
    if (reader.file_.size() - cursor < 4U ||
        std::string_view(reader.file_).substr(cursor, 4U) != "OggS")
        Throw(ErrorCode::kInvalidDictionary, path, "Missing LSA Ogg stream");
    audio::VorbisStreamInfo info;
    try {
        info =
            audio::InspectVorbis(std::string_view(reader.file_).substr(cursor));
    } catch (const audio::VorbisError& error) {
        Throw(ErrorCode::kInvalidDictionary, path,
              std::string("Invalid LSA Vorbis stream: ") + error.what());
    }
    for (const auto& record : reader.records_) {
        if (record.sample_offset > info.frames ||
            record.sample_length > info.frames - record.sample_offset)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "LSA sample range is out of bounds");
    }
    reader.metadata_.name = path.stem().string();
    std::stable_sort(reader.records_.begin(), reader.records_.end(),
                     [](const auto& left, const auto& right) {
                         return std::tie(left.folded, left.word) <
                                std::tie(right.folded, right.word);
                     });
    return reader;
}

std::vector<const Reader::Record*> Reader::Ranked(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(prefix);
    std::vector<const Record*> result;
    for (const auto& record : records_) {
        if (checkpoint)
            checkpoint();
        if (record.folded.rfind(folded, 0U) == 0U)
            result.push_back(&record);
    }
    return result;
}

std::pair<std::vector<std::string>, bool> Reader::EnumerateHeadwords(
    std::size_t offset, std::size_t result_limit, std::size_t byte_limit,
    const std::function<void()>& checkpoint) const {
    return enumeration_index_.Page(
        records_.size(),
        [this](std::uint32_t ordinal) -> std::string_view {
            return records_[ordinal].word;
        },
        offset, result_limit, byte_limit, checkpoint);
}

std::vector<Article> Reader::LookupExact(
    std::string_view word, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(word);
    std::vector<Article> result;
    for (const auto* record : Ranked(word, checkpoint)) {
        if (record->folded != folded)
            continue;
        result.push_back({record->word,
                          "<div class=\"lsa_article\"><audio "
                          "controls=\"controls\"><source src=\"" +
                              Escape(record->word) +
                              ".wav\" type=\"audio/wav\"></audio><span>" +
                              Escape(record->word) + "</span></div>"});
        if (result.size() == limit)
            break;
    }
    return result;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        result.push_back({record->word,
                          "<div class=\"lsa_article\"><audio "
                          "controls=\"controls\"><source src=\"" +
                              Escape(record->word) +
                              ".wav\" type=\"audio/wav\"></audio><span>" +
                              Escape(record->word) + "</span></div>"});
        if (result.size() == limit)
            break;
    }
    return result;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        if (seen.insert(record->folded).second)
            result.push_back(record->word);
        if (result.size() == limit)
            break;
    }
    return result;
}

std::string Reader::Resource(std::string_view id) const {
    if (id.size() < 5U || id.substr(id.size() - 4U) != ".wav")
        return {};
    const std::string folded =
        foundation::FoldForLookup(id.substr(0U, id.size() - 4U));
    const auto found = std::find_if(
        records_.begin(), records_.end(),
        [&folded](const auto& record) { return record.folded == folded; });
    if (found == records_.end())
        return {};
    try {
        return audio::DecodeVorbisRangeToWav(
            std::string_view(file_).substr(vorbis_offset_),
            found->sample_offset, found->sample_length, kMaximumPcmBytes);
    } catch (const audio::VorbisError& error) {
        Throw(ErrorCode::kInvalidDictionary, path_,
              std::string("Cannot decode LSA audio: ") + error.what());
    }
}
}  // namespace goldendict::core::formats::lsa
