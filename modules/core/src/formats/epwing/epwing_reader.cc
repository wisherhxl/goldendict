// SPDX-License-Identifier: GPL-3.0-or-later
#include "epwing_reader.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include "../../foundation/text_encoding.h"
#include "../../foundation/text_folding.h"

namespace goldendict::core::formats::epwing {
namespace {
constexpr std::size_t kPageSize = 2048U;
constexpr std::size_t kCatalogEntrySize = 164U;
constexpr std::size_t kMaximumFileSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumTextSize = 2800000U;
constexpr std::size_t kMaximumEntries = 10U * 1000U * 1000U;

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::uint16_t Be16(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 2U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated EPWING data");
    return (static_cast<unsigned char>(data[at]) << 8U) |
           static_cast<unsigned char>(data[at + 1U]);
}

std::uint32_t Be32(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 4U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated EPWING data");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4U; ++i)
        value = (value << 8U) | static_cast<unsigned char>(data[at + i]);
    return value;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect EPWING file");
    if (size > kMaximumFileSize)
        Throw(ErrorCode::kInvalidDictionary, path,
              "EPWING file exceeds the supported size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        Throw(ErrorCode::kMissingFile, path, "Cannot open EPWING file");
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size())))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete EPWING file");
    return data;
}

std::string Lower(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::optional<std::filesystem::path> ChildCaseInsensitive(
    const std::filesystem::path& directory, std::string_view wanted) {
    std::error_code error;
    for (std::filesystem::directory_iterator it(directory, error), end;
         !error && it != end; it.increment(error)) {
        if (Lower(it->path().filename().string()) == Lower(std::string(wanted)))
            return it->path();
    }
    return std::nullopt;
}

std::string TrimField(std::string_view field) {
    const auto zero = field.find('\0');
    if (zero != std::string_view::npos)
        field = field.substr(0, zero);
    while (!field.empty() && field.back() == ' ')
        field.remove_suffix(1U);
    return std::string(field);
}

std::string Decode(std::string_view value, const std::filesystem::path& path) {
    try {
        return foundation::DecodeToUtf8(value, "EUC-JP", kMaximumTextSize);
    } catch (const foundation::TextEncodingError&) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid EPWING EUC-JP text");
    }
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

std::uint32_t Bcd(std::string_view data, std::size_t at, std::size_t bytes,
                  const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < bytes)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Truncated EPWING BCD value");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < bytes; ++i) {
        const auto byte = static_cast<unsigned char>(data[at + i]);
        if ((byte >> 4U) > 9U || (byte & 0x0fU) > 9U)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid EPWING BCD value");
        value = value * 100U + (byte >> 4U) * 10U + (byte & 0x0fU);
    }
    return value;
}

struct Location {
    std::uint32_t page;
    std::uint16_t offset;
};

enum class CharacterCode { kIso88591, kJisX0208 };

CharacterCode LoadCharacterCode(const std::filesystem::path& root) {
    const auto language = ChildCaseInsensitive(root, "language");
    if (!language)
        return CharacterCode::kJisX0208;
    const auto data = ReadFile(*language);
    if (data.size() < 16U)
        Throw(ErrorCode::kInvalidDictionary, *language,
              "Truncated EPWING LANGUAGE file");
    const auto code = Be16(data, 0U, *language);
    if (code == 1U)
        return CharacterCode::kIso88591;
    if (code == 2U)
        return CharacterCode::kJisX0208;
    Throw(ErrorCode::kInvalidDictionary, *language,
          "Unsupported EPWING character code");
}

std::string DecodeBookText(std::string_view value, CharacterCode code,
                           const std::filesystem::path& path,
                           bool alphabet = false) {
    if (code == CharacterCode::kIso88591 || alphabet) {
        try {
            return foundation::DecodeToUtf8(value, "ISO-8859-1",
                                            kMaximumTextSize);
        } catch (const foundation::TextEncodingError&) {
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid EPWING Latin-1 text");
        }
    }
    if (value.size() % 2U != 0U)
        Throw(ErrorCode::kInvalidDictionary, path,
              "Odd-length EPWING JIS X 0208 text");
    std::string euc(value);
    for (char& byte : euc) {
        const auto raw = static_cast<unsigned char>(byte);
        if (raw <= 0x20U || raw >= 0x7fU)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid EPWING JIS X 0208 byte");
        byte = static_cast<char>(raw | 0x80U);
    }
    return Decode(euc, path);
}

std::string LocationKey(const std::filesystem::path& path, Location location) {
    return path.string() + ":" + std::to_string(location.page) + ":" +
           std::to_string(static_cast<unsigned>(location.offset));
}

std::string RenderText(
    std::string_view file, Location location, const std::filesystem::path& path,
    const std::unordered_map<std::string, std::string>& targets,
    CharacterCode character_code) {
    if (location.page == 0U)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid EPWING text page");
    std::size_t cursor =
        (static_cast<std::size_t>(location.page) - 1U) * kPageSize +
        location.offset;
    if (cursor >= file.size())
        Throw(ErrorCode::kInvalidDictionary, path,
              "EPWING text position is out of range");
    const auto end = std::min(file.size(), cursor + kMaximumTextSize);
    std::string html;
    std::string bytes;
    std::optional<std::string> reference_text;
    std::vector<std::string> decoration_closers;
    unsigned skip_code = 0U;
    auto append = [&](std::string value) {
        if (reference_text)
            *reference_text += std::move(value);
        else
            html += std::move(value);
    };
    auto flush = [&]() {
        if (!bytes.empty()) {
            append(Escape(character_code == CharacterCode::kIso88591
                              ? DecodeBookText(bytes, character_code, path)
                              : Decode(bytes, path)));
            bytes.clear();
        }
    };
    while (cursor < end) {
        const auto first = static_cast<unsigned char>(file[cursor]);
        if (first != 0x1fU) {
            if (skip_code != 0U) {
                cursor += character_code == CharacterCode::kIso88591 ? 1U : 2U;
                continue;
            }
            if (character_code == CharacterCode::kIso88591) {
                bytes.push_back(file[cursor++]);
            } else {
                if (end - cursor < 2U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING JIS X 0208 character");
                const auto second =
                    static_cast<unsigned char>(file[cursor + 1U]);
                if (first <= 0x20U || first >= 0x7fU || second <= 0x20U ||
                    second >= 0x7fU)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Invalid EPWING JIS X 0208 character");
                bytes.push_back(static_cast<char>(first | 0x80U));
                bytes.push_back(static_cast<char>(second | 0x80U));
                cursor += 2U;
            }
            continue;
        }
        flush();
        if (end - cursor < 2U)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Truncated EPWING control code");
        const auto code = static_cast<unsigned char>(file[cursor + 1U]);
        if (skip_code != 0U) {
            cursor += 2U;
            if (code == skip_code)
                skip_code = 0U;
            continue;
        }
        if (code == 0x03U) {
            flush();
            while (!decoration_closers.empty()) {
                append(decoration_closers.back());
                decoration_closers.pop_back();
            }
            return "<div class=\"epwing_article\">" + html + "</div>";
        }
        switch (code) {
            case 0x02:
                cursor += 2U;
                break;
            case 0x04:
            case 0x05:
            case 0x0b:
            case 0x0c:
            case 0x10:
            case 0x11:
                cursor += 2U;
                break;
            case 0x0a:
                append("<br>");
                cursor += 2U;
                break;
            case 0x06:
                append("<sub>");
                cursor += 2U;
                break;
            case 0x07:
                append("</sub>");
                cursor += 2U;
                break;
            case 0x0e:
                append("<sup>");
                cursor += 2U;
                break;
            case 0x0f:
                append("</sup>");
                cursor += 2U;
                break;
            case 0x12:
                append("<em>");
                cursor += 2U;
                break;
            case 0x13:
                append("</em>");
                cursor += 2U;
                break;
            case 0x09:
            case 0x1a:
            case 0x1b:
            case 0x1c:
            case 0x1d:
            case 0x1e:
            case 0x1f:
            case 0x41:
                if (end - cursor < 4U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING control code");
                cursor += 4U;
                break;
            case 0x14:
                if (end - cursor < 4U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING control code");
                skip_code = 0x15U;
                cursor += 4U;
                break;
            case 0x42:
                reference_text = std::string();
                cursor +=
                    (end - cursor >= 4U && file[cursor + 2U] == '\0') ? 4U : 2U;
                break;
            case 0x62: {
                if (end - cursor < 8U || !reference_text)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Invalid EPWING reference");
                const Location target{Bcd(file, cursor + 2U, 4U, path),
                                      static_cast<std::uint16_t>(
                                          Bcd(file, cursor + 6U, 2U, path))};
                const auto found = targets.find(LocationKey(path, target));
                if (found == targets.end())
                    html += *reference_text;
                else
                    html += "<a href=\"bword://" + Escape(found->second) +
                            "\">" + *reference_text + "</a>";
                reference_text.reset();
                cursor += 8U;
                break;
            }
            case 0x32:
                cursor += 2U;
                break;
            case 0x39:
                if (end - cursor < 46U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING MPEG control");
                cursor += 46U;
                break;
            case 0x3c:
            case 0x4d:
                if (end - cursor < 20U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING graphic control");
                cursor += 20U;
                break;
            case 0x44:
                if (end - cursor < 12U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING graphic control");
                cursor += 12U;
                break;
            case 0x45:
            case 0x4c:
                if (end - cursor < 4U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING graphic control");
                cursor += 4U;
                break;
            case 0x4a:
                if (end - cursor < 18U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING sound control");
                cursor += 18U;
                break;
            case 0x4b:
            case 0x52:
            case 0x63:
            case 0x64:
                if (end - cursor < 8U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING position control");
                cursor += 8U;
                break;
            case 0x4f:
                if (end - cursor < 34U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING clickable-area control");
                cursor += 34U;
                break;
            case 0x53:
                if (end - cursor < 10U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING sound control");
                cursor += 10U;
                break;
            case 0x43:
            case 0x59:
            case 0x5c:
            case 0x61:
            case 0x6a:
            case 0x6b:
            case 0x6c:
            case 0x6d:
            case 0x6f:
                cursor += 2U;
                break;
            case 0xe0: {
                if (end - cursor < 4U)
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Truncated EPWING decoration control");
                const auto decoration = Be16(file, cursor + 2U, path);
                const auto tags =
                    decoration == 1U   ? std::pair{"<i>", "</i>"}
                    : decoration == 3U ? std::pair{"<b>", "</b>"}
                    : decoration == 4U ? std::pair{"<em>", "</em>"}
                    : decoration == 5U ? std::pair{"<sub>", "</sub>"}
                    : decoration == 6U ? std::pair{"<sup>", "</sup>"}
                                       : std::pair{"<span>", "</span>"};
                append(tags.first);
                decoration_closers.emplace_back(tags.second);
                cursor += 4U;
                break;
            }
            case 0xe1:
                if (!decoration_closers.empty()) {
                    append(decoration_closers.back());
                    decoration_closers.pop_back();
                }
                cursor += 2U;
                break;
            default:
                if ((code >= 0x35U && code <= 0x3fU) || code == 0x49U ||
                    code == 0x4eU || (code >= 0x70U && code <= 0x8fU) ||
                    (code >= 0xe4U && (code & 1U) == 0U)) {
                    skip_code = code >= 0xe4U ? code + 1U : code + 0x20U;
                    cursor += 2U;
                } else {
                    Throw(ErrorCode::kInvalidDictionary, path,
                          "Unsupported EPWING text control code");
                }
        }
    }
    Throw(ErrorCode::kInvalidDictionary, path, "Unterminated EPWING article");
}
}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message) + ": " + path.string()),
      code_(code) {}

Reader Reader::Open(const std::filesystem::path& catalog_path) {
    Reader reader;
    reader.path_ = catalog_path;
    const std::string catalog = ReadFile(catalog_path);
    if (catalog.size() < 16U)
        Throw(ErrorCode::kInvalidDictionary, catalog_path,
              "Truncated EPWING catalog");
    const auto count = Be16(catalog, 0U, catalog_path);
    const auto version = Be16(catalog, 2U, catalog_path);
    if (count == 0U || count > 50U ||
        (version != 1U && version != 2U && version != 3U))
        Throw(ErrorCode::kInvalidDictionary, catalog_path,
              "Invalid EPWING catalog header");
    if (catalog.size() <
        16U + static_cast<std::size_t>(count) * kCatalogEntrySize)
        Throw(ErrorCode::kInvalidDictionary, catalog_path,
              "Truncated EPWING catalog entries");

    struct Pending {
        std::string word;
        Location text;
        Location heading;
        std::shared_ptr<const std::string> file;
        std::filesystem::path file_path;
    };

    std::vector<Pending> pending;
    const auto root = catalog_path.parent_path();
    const auto character_code = LoadCharacterCode(root);
    for (std::size_t book = 0; book < count; ++book) {
        const auto at = 16U + book * kCatalogEntrySize;
        const std::string title = DecodeBookText(
            TrimField(std::string_view(catalog).substr(at + 2U, 80U)),
            character_code, catalog_path);
        const std::string directory =
            TrimField(std::string_view(catalog).substr(at + 82U, 8U));
        const auto index_page = Be16(catalog, at + 94U, catalog_path);
        if (directory.empty() || index_page == 0U)
            Throw(ErrorCode::kInvalidDictionary, catalog_path,
                  "Invalid EPWING subbook catalog entry");
        const auto subbook = ChildCaseInsensitive(root, directory);
        if (!subbook || !std::filesystem::is_directory(*subbook))
            Throw(ErrorCode::kMissingFile, root / directory,
                  "Missing EPWING subbook directory");
        const auto data_dir = ChildCaseInsensitive(*subbook, "data");
        if (!data_dir || !std::filesystem::is_directory(*data_dir))
            Throw(ErrorCode::kMissingFile, *subbook / "data",
                  "Missing EPWING data directory");
        std::string text_name = "honmon";
        if (version != 1U &&
            catalog.size() >= 16U + static_cast<std::size_t>(count) *
                                        kCatalogEntrySize * 2U) {
            const auto extra =
                16U + static_cast<std::size_t>(count) * kCatalogEntrySize +
                book * kCatalogEntrySize;
            const auto candidate =
                TrimField(std::string_view(catalog).substr(extra + 4U, 8U));
            if (!candidate.empty())
                text_name = candidate;
        }
        const auto text_path = ChildCaseInsensitive(*data_dir, text_name);
        if (!text_path)
            Throw(ErrorCode::kMissingFile, *data_dir / text_name,
                  "Missing EPWING text file");
        const auto text =
            std::make_shared<const std::string>(ReadFile(*text_path));
        const std::size_t table =
            (static_cast<std::size_t>(index_page) - 1U) * kPageSize;
        if (table > text->size() || text->size() - table < kPageSize)
            Throw(ErrorCode::kInvalidDictionary, *text_path,
                  "EPWING index table is out of range");
        const auto index_count =
            static_cast<unsigned char>((*text)[table + 1U]);
        if (index_count >= 127U)
            Throw(ErrorCode::kInvalidDictionary, *text_path,
                  "Invalid EPWING index count");
        for (std::size_t i = 0; i < index_count; ++i) {
            const auto entry = table + 16U + i * 16U;
            const auto id = static_cast<unsigned char>((*text)[entry]);
            if (id != 0x90U && id != 0x91U && id != 0x92U)
                continue;
            const auto start_page = Be32(*text, entry + 2U, *text_path);
            const auto page_count = Be32(*text, entry + 6U, *text_path);
            if (start_page == 0U || page_count == 0U ||
                page_count > kMaximumEntries ||
                start_page >
                    std::numeric_limits<std::uint32_t>::max() - page_count)
                Throw(ErrorCode::kInvalidDictionary, *text_path,
                      "Invalid EPWING word index range");
            for (std::uint32_t page = start_page;
                 page < start_page + page_count; ++page) {
                const std::size_t page_at =
                    (static_cast<std::size_t>(page) - 1U) * kPageSize;
                if (page_at > text->size() ||
                    text->size() - page_at < kPageSize)
                    Throw(ErrorCode::kInvalidDictionary, *text_path,
                          "EPWING word index page is out of range");
                const auto page_id =
                    static_cast<unsigned char>((*text)[page_at]);
                if ((page_id & 0x80U) == 0U || (page_id & 0x10U) != 0U)
                    Throw(ErrorCode::kInvalidDictionary, *text_path,
                          "Unsupported EPWING word index layout");
                const auto fixed =
                    static_cast<unsigned char>((*text)[page_at + 1U]);
                const auto entries = Be16(*text, page_at + 2U, *text_path);
                std::size_t cursor = page_at + 4U;
                for (std::size_t n = 0; n < entries; ++n) {
                    const auto length =
                        fixed == 0U
                            ? static_cast<unsigned char>(text->at(cursor++))
                            : fixed;
                    if (length == 0U || cursor > page_at + kPageSize ||
                        page_at + kPageSize - cursor < length + 12U)
                        Throw(ErrorCode::kInvalidDictionary, *text_path,
                              "Invalid EPWING word index entry");
                    const std::string word = DecodeBookText(
                        std::string_view(*text).substr(cursor, length),
                        character_code, *text_path, id == 0x92U);
                    const Location text_location{
                        Be32(*text, cursor + length, *text_path),
                        Be16(*text, cursor + length + 4U, *text_path)};
                    const Location heading_location{
                        Be32(*text, cursor + length + 6U, *text_path),
                        Be16(*text, cursor + length + 10U, *text_path)};
                    pending.push_back({word, text_location, heading_location,
                                       text, *text_path});
                    cursor += length + 12U;
                    if (pending.size() > kMaximumEntries)
                        Throw(ErrorCode::kInvalidDictionary, *text_path,
                              "Too many EPWING entries");
                }
            }
        }
        if (reader.metadata_.name.empty())
            reader.metadata_.name = title;

        std::error_code resource_error;
        for (std::filesystem::recursive_directory_iterator
                 it(*subbook,
                    std::filesystem::directory_options::skip_permission_denied,
                    resource_error),
             end;
             !resource_error && it != end; it.increment(resource_error)) {
            if (it->is_symlink(resource_error))
                continue;
            if (!it->is_regular_file(resource_error) || resource_error)
                continue;
            if (std::filesystem::equivalent(it->path(), *text_path,
                                            resource_error))
                continue;
            const auto relative =
                std::filesystem::relative(it->path(), *subbook, resource_error);
            if (resource_error)
                break;
            const auto id = directory + "/" + relative.generic_string();
            reader.resources_.emplace(id, ReadFile(it->path()));
        }
    }
    std::unordered_map<std::string, std::string> targets;
    for (const auto& item : pending)
        targets.emplace(LocationKey(item.file_path, item.heading), item.word);
    std::set<std::tuple<std::string, std::string, std::uint32_t, std::uint16_t>>
        seen;
    for (auto& item : pending) {
        if (!seen.emplace(item.file_path.string(), item.word, item.text.page,
                          item.text.offset)
                 .second)
            continue;
        reader.records_.push_back(
            {item.word, foundation::FoldForLookup(item.word),
             RenderText(*item.file, item.text, item.file_path, targets,
                        character_code)});
    }
    if (reader.records_.empty())
        Throw(ErrorCode::kInvalidDictionary, catalog_path,
              "EPWING book has no supported word index");
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
    std::stable_sort(
        result.begin(), result.end(), [&folded](const auto* a, const auto* b) {
            const bool ae = a->folded == folded, be = b->folded == folded;
            if (ae != be)
                return ae;
            if (a->word.size() != b->word.size())
                return a->word.size() < b->word.size();
            return a->word < b->word;
        });
    return result;
}

std::vector<Article> Reader::LookupExact(
    std::string_view word, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    const auto folded = foundation::FoldForLookup(word);
    std::vector<Article> result;
    for (const auto& record : records_) {
        if (checkpoint)
            checkpoint();
        if (record.folded == folded && result.size() < limit)
            result.push_back({record.word, record.article});
    }
    return result;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        if (result.size() == limit)
            break;
        result.push_back({record->word, record->article});
    }
    return result;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const auto* record : Ranked(prefix, checkpoint)) {
        if (result.size() == limit)
            break;
        if (seen.insert(record->word).second)
            result.push_back(record->word);
    }
    return result;
}

const std::string* Reader::Resource(std::string_view id) const {
    const auto found = resources_.find(std::string(id));
    return found == resources_.end() ? nullptr : &found->second;
}
}  // namespace goldendict::core::formats::epwing
