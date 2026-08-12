// SPDX-License-Identifier: GPL-3.0-or-later
#include "sounddir_reader.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <tuple>
#include <utility>
#include "../../audio/audio_file_types.h"
#include "../../foundation/text_folding.h"
#include "../../foundation/utf8.h"

namespace goldendict::core::formats::sounddir {
namespace {
constexpr std::size_t kMaximumEntries = 1000000U;
constexpr std::uintmax_t kMaximumResourceBytes = 256U * 1024U * 1024U;

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::string Headword(const std::filesystem::path& path) {
    std::string name = path.filename().string();
    std::size_t end = name.size();
    while (end > 0U &&
           std::isspace(static_cast<unsigned char>(name[end - 1U])) != 0)
        --end;
    const auto dot = std::string_view(name).substr(0U, end).find_last_of('.');
    return dot == std::string_view::npos ? std::string{} : name.substr(0U, dot);
}

std::string Escape(std::string_view value) {
    std::string result;
    for (const char c : value) {
        switch (c) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\'':
                result += "&#39;";
                break;
            default:
                result.push_back(c);
        }
    }
    return result;
}

bool IsWithin(const std::filesystem::path& root,
              const std::filesystem::path& candidate) {
    const auto relative = candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute())
        return false;
    const auto first = relative.begin();
    return first == relative.end() || *first != "..";
}
}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(path.string() + ": " + std::move(message)),
      code_(code) {}

Reader Reader::Open(const std::filesystem::path& path,
                    std::string display_name) {
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (error || !std::filesystem::exists(status))
        Throw(ErrorCode::kMissingDirectory, path,
              "Configured sound directory is unavailable");
    if (!std::filesystem::is_directory(status))
        Throw(ErrorCode::kInvalidDirectory, path,
              "Configured sound path is not a directory");
    Reader reader;
    reader.root_ = std::filesystem::canonical(path, error);
    if (error)
        Throw(ErrorCode::kMissingDirectory, path,
              "Cannot resolve configured sound directory");
    reader.metadata_.name = std::move(display_name);
    if (reader.metadata_.name.empty())
        reader.metadata_.name = reader.root_.filename().string();
    if (reader.metadata_.name.empty())
        reader.metadata_.name = "Sound directory";

    std::filesystem::recursive_directory_iterator it(
        reader.root_,
        std::filesystem::directory_options::skip_permission_denied, error),
        end;
    if (error)
        Throw(ErrorCode::kMissingDirectory, path,
              "Cannot enumerate configured sound directory");
    while (it != end) {
        const auto entry_path = it->path();
        const auto link_status = it->symlink_status(error);
        if (error)
            Throw(ErrorCode::kInvalidDirectory, entry_path,
                  "Cannot inspect sound directory entry");
        if (std::filesystem::is_symlink(link_status)) {
            if (std::filesystem::is_directory(it->status(error)))
                it.disable_recursion_pending();
            error.clear();
        } else if (std::filesystem::is_regular_file(link_status)) {
            const std::string filename = entry_path.filename().string();
            if (foundation::IsValidUtf8(filename) &&
                audio::IsSupportedAudioFile(filename)) {
                const auto relative =
                    entry_path.lexically_relative(reader.root_);
                const std::string resource_id = relative.generic_string();
                std::string word = Headword(entry_path);
                if (!word.empty() && foundation::IsValidUtf8(resource_id)) {
                    if (reader.records_.size() == kMaximumEntries)
                        Throw(ErrorCode::kInvalidDirectory, path,
                              "Sound directory contains too many audio files");
                    reader.records_.push_back(
                        {std::move(word),
                         {},
                         resource_id,
                         audio::MediaTypeForAudioFile(filename)});
                    reader.records_.back().folded =
                        foundation::FoldForLookup(reader.records_.back().word);
                }
            }
        }
        it.increment(error);
        if (error)
            Throw(ErrorCode::kInvalidDirectory, entry_path,
                  "Cannot continue sound directory traversal");
    }
    if (reader.records_.empty())
        Throw(ErrorCode::kInvalidDirectory, path,
              "Sound directory contains no supported audio files");
    std::stable_sort(
        reader.records_.begin(), reader.records_.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.folded, left.word, left.resource_id) <
                   std::tie(right.folded, right.word, right.resource_id);
        });
    return reader;
}

std::vector<const Reader::Record*> Reader::Ranked(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const auto folded = foundation::FoldForLookup(prefix);
    std::vector<const Record*> result;
    for (const auto& record : records_) {
        if (checkpoint)
            checkpoint();
        if (record.folded.rfind(folded, 0U) == 0U)
            result.push_back(&record);
    }
    return result;
}

std::vector<Article> Reader::LookupExact(
    std::string_view word, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    const auto folded = foundation::FoldForLookup(word);
    std::vector<Article> result;
    for (const auto* record : Ranked(word, checkpoint)) {
        if (record->folded != folded)
            continue;
        result.push_back(
            {record->word,
             "<div class=\"sounddir_article\"><audio controls=\"controls\">"
             "<source src=\"" +
                 Escape(record->resource_id) + "\" type=\"" +
                 record->media_type + "\"></audio><span>" +
                 Escape(record->resource_id) + "</span></div>"});
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
        result.push_back(
            {record->word,
             "<div class=\"sounddir_article\"><audio controls=\"controls\">"
             "<source src=\"" +
                 Escape(record->resource_id) + "\" type=\"" +
                 record->media_type + "\"></audio><span>" +
                 Escape(record->resource_id) + "</span></div>"});
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
    const auto found = std::find_if(
        records_.begin(), records_.end(),
        [id](const auto& record) { return record.resource_id == id; });
    if (found == records_.end())
        return {};
    std::error_code error;
    const auto candidate =
        std::filesystem::canonical(root_ / std::filesystem::u8path(id), error);
    if (error || !IsWithin(root_, candidate))
        Throw(ErrorCode::kInvalidDirectory, root_,
              "Sound resource escaped its configured directory");
    const auto size = std::filesystem::file_size(candidate, error);
    if (error || size > kMaximumResourceBytes)
        Throw(ErrorCode::kInvalidDirectory, candidate,
              "Sound resource is unavailable or exceeds the size limit");
    std::ifstream input(candidate, std::ios::binary);
    if (!input)
        Throw(ErrorCode::kMissingDirectory, candidate,
              "Cannot open sound resource");
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size())))
        Throw(ErrorCode::kInvalidDirectory, candidate,
              "Cannot read complete sound resource");
    return data;
}
}  // namespace goldendict::core::formats::sounddir
