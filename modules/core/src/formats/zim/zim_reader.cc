// SPDX-License-Identifier: GPL-3.0-or-later
#include "zim_reader.h"
#include <bzlib.h>
#include <zlib.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>
#include "../../foundation/text_folding.h"
#include "../../foundation/utf8.h"

namespace goldendict::core::formats::zim {
namespace {
constexpr std::size_t kHeader = 80U, kMaxFile = 512U * 1024U * 1024U;
constexpr std::size_t kMaxCluster = 64U * 1024U * 1024U;
constexpr std::size_t kMaxEntries = 10U * 1000U * 1000U;

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::uint16_t Le16(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 2U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated ZIM integer");
    return static_cast<unsigned char>(data[at]) |
           (static_cast<std::uint16_t>(
                static_cast<unsigned char>(data[at + 1U]))
            << 8U);
}

std::uint32_t Le32(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 4U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated ZIM integer");
    std::uint32_t value = 0;
    for (int i = 3; i >= 0; --i)
        value = (value << 8U) | static_cast<unsigned char>(data[at + i]);
    return value;
}

std::uint64_t Le64(std::string_view data, std::size_t at,
                   const std::filesystem::path& path) {
    if (at > data.size() || data.size() - at < 8U)
        Throw(ErrorCode::kInvalidDictionary, path, "Truncated ZIM integer");
    std::uint64_t value = 0;
    for (int i = 7; i >= 0; --i)
        value = (value << 8U) | static_cast<unsigned char>(data[at + i]);
    return value;
}

std::string Load(const Files& files) {
    std::string output;
    for (const auto& part : files.parts) {
        std::error_code error;
        const auto size = std::filesystem::file_size(part, error);
        if (error)
            Throw(ErrorCode::kMissingFile, part, "Cannot inspect ZIM part");
        if (size > kMaxFile || output.size() > kMaxFile - size)
            Throw(ErrorCode::kInvalidDictionary, files.primary,
                  "ZIM exceeds size limit");
        std::ifstream input(part, std::ios::binary);
        if (!input)
            Throw(ErrorCode::kMissingFile, part, "Cannot open ZIM part");
        const auto old = output.size();
        output.resize(old + size);
        if (size && !input.read(output.data() + old,
                                static_cast<std::streamsize>(size)))
            Throw(ErrorCode::kInvalidDictionary, part,
                  "Cannot read complete ZIM part");
    }
    return output;
}

std::string Inflate(std::string_view input, std::size_t limit, bool bzip) {
    for (std::size_t size = std::min<std::size_t>(64U * 1024U, limit); size;
         size = std::min(limit, size * 2U)) {
        std::string output(size, '\0');
        int result;
        std::size_t length;
        if (bzip) {
            unsigned int n = output.size();
            result = BZ2_bzBuffToBuffDecompress(output.data(), &n,
                                                const_cast<char*>(input.data()),
                                                input.size(), 0, 0);
            length = n;
            if (result == BZ_OK) {
                output.resize(length);
                return output;
            }
            if (result != BZ_OUTBUFF_FULL || size == limit)
                break;
        } else {
            uLongf n = output.size();
            result = uncompress(reinterpret_cast<Bytef*>(output.data()), &n,
                                reinterpret_cast<const Bytef*>(input.data()),
                                input.size());
            length = n;
            if (result == Z_OK) {
                output.resize(length);
                return output;
            }
            if (result != Z_BUF_ERROR || size == limit)
                break;
        }
    }
    return {};
}

std::string CString(std::string_view data, std::size_t* at,
                    const std::filesystem::path& path) {
    const auto end = data.find('\0', *at);
    if (end == std::string_view::npos)
        Throw(ErrorCode::kInvalidDictionary, path, "Unterminated ZIM string");
    std::string value(data.substr(*at, end - *at));
    *at = end + 1U;
    if (!foundation::IsValidUtf8(value))
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid UTF-8 in ZIM string");
    return value;
}

bool Prefix(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

std::string EscapeHtml(std::string_view value) {
    std::string output;
    output.reserve(value.size());
    for (const char character : value) {
        switch (character) {
            case '&':
                output += "&amp;";
                break;
            case '<':
                output += "&lt;";
                break;
            case '>':
                output += "&gt;";
                break;
            default:
                output.push_back(character);
        }
    }
    return output;
}

std::size_t Points(std::string_view value) {
    return std::count_if(value.begin(), value.end(),
                         [](unsigned char c) { return (c & 0xc0U) != 0x80U; });
}

struct Entry {
    std::uint16_t mime = 0;
    char name_space = 0;
    std::uint32_t cluster = 0, blob = 0, redirect = 0;
    bool redirected = false;
    std::string url, title;
};
}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message) + ": " + path.string()),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const Files& files) {
    Reader reader;
    reader.files_ = files;
    const auto& path = files.primary;
    const std::string file = Load(files);
    if (file.size() < kHeader || Le32(file, 0U, path) != 0x044d495aU ||
        Le64(file, 56U, path) != kHeader)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid ZIM header");
    const unsigned major = Le16(file, 4U, path), minor = Le16(file, 6U, path);
    const std::size_t count = Le32(file, 24U, path),
                      clusters = Le32(file, 28U, path);
    const std::size_t url_table = Le64(file, 32U, path);
    const std::size_t title_table = Le64(file, 40U, path);
    const std::size_t cluster_table = Le64(file, 48U, path);
    const std::size_t checksum = Le64(file, 72U, path);
    if (!count || count > kMaxEntries || !clusters || url_table < kHeader ||
        url_table > file.size() || title_table < kHeader ||
        title_table > file.size() || cluster_table < kHeader ||
        count > (file.size() - url_table) / 8U || cluster_table > file.size() ||
        clusters > (file.size() - cluster_table) / 8U)
        Throw(ErrorCode::kInvalidDictionary, path, "Invalid ZIM table bounds");
    std::vector<std::string> mimes;
    std::size_t at = kHeader;
    const auto mime_end = std::min({url_table, title_table, cluster_table});
    const std::string_view mime_data(file.data(), mime_end);
    while (at < mime_data.size()) {
        std::string mime = CString(mime_data, &at, path);
        if (mime.empty())
            break;
        mimes.push_back(std::move(mime));
    }
    std::vector<std::size_t> cluster_offsets;
    for (std::size_t i = 0; i < clusters; ++i) {
        const auto offset = Le64(file, cluster_table + i * 8U, path);
        if (offset >= file.size())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid ZIM cluster offset");
        cluster_offsets.push_back(offset);
    }
    std::vector<Entry> entries;
    entries.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::size_t position = Le64(file, url_table + i * 8U, path);
        if (position >= file.size())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid ZIM entry offset");
        Entry entry;
        if (file.size() - position < 12U)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Truncated ZIM directory entry");
        entry.mime = Le16(file, position, path);
        const auto parameter_length =
            static_cast<unsigned char>(file[position + 2U]);
        entry.name_space = file[position + 3U];
        if (entry.mime == 0xffffU) {
            entry.redirected = true;
            entry.redirect = Le32(file, position + 8U, path);
            position += 12U;
        } else {
            if (file.size() - position < 16U)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Truncated ZIM directory entry");
            entry.cluster = Le32(file, position + 8U, path);
            entry.blob = Le32(file, position + 12U, path);
            position += 16U;
        }
        entry.url = CString(file, &position, path);
        entry.title = CString(file, &position, path);
        if (parameter_length > file.size() - position)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Truncated ZIM parameters");
        entries.push_back(std::move(entry));
    }
    std::map<std::size_t, std::vector<std::string>> cluster_cache;
    const auto blobs =
        [&](std::size_t cluster) -> const std::vector<std::string>& {
        auto found = cluster_cache.find(cluster);
        if (found != cluster_cache.end())
            return found->second;
        if (cluster >= cluster_offsets.size())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid ZIM cluster number");
        const std::size_t begin = cluster_offsets[cluster];
        std::size_t end =
            checksum && checksum <= file.size() ? checksum : file.size();
        for (const auto offset : cluster_offsets)
            if (offset > begin)
                end = std::min(end, offset);
        if (end <= begin + 1U)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid ZIM cluster length");
        const unsigned info = static_cast<unsigned char>(file[begin]);
        const unsigned compression = info & 0x0fU;
        std::string data;
        const auto input =
            std::string_view(file).substr(begin + 1U, end - begin - 1U);
        if (compression <= 1U)
            data = std::string(input);
        else if (compression == 2U)
            data = Inflate(input, kMaxCluster, false);
        else if (compression == 3U)
            data = Inflate(input, kMaxCluster, true);
        else
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Unsupported ZIM cluster compression");
        if (data.empty())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Cannot decode ZIM cluster");
        const std::size_t width = (major >= 6U && (info & 0x10U)) ? 8U : 4U;
        const auto offset_at = [&](std::size_t n) {
            return width == 8U ? Le64(data, n * width, path)
                               : Le32(data, n * width, path);
        };
        const std::size_t first = offset_at(0U);
        if (first < width * 2U || first % width)
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid ZIM blob table");
        const std::size_t blob_count = first / width - 1U;
        std::vector<std::string> result;
        for (std::size_t i = 0; i < blob_count; ++i) {
            const auto a = offset_at(i), b = offset_at(i + 1U);
            if (a > b || b > data.size())
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid ZIM blob bounds");
            result.emplace_back(data.substr(a, b - a));
        }
        return cluster_cache.emplace(cluster, std::move(result)).first->second;
    };
    const auto resolve = [&entries, &path](std::size_t index) {
        std::set<std::size_t> seen;
        while (index < entries.size() && entries[index].redirected) {
            if (!seen.insert(index).second || seen.size() > 64U)
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Cyclic ZIM redirect");
            index = entries[index].redirect;
        }
        if (index >= entries.size())
            Throw(ErrorCode::kInvalidDictionary, path, "Invalid ZIM redirect");
        return index;
    };
    std::map<std::size_t, std::size_t> article_map;
    const bool new_namespaces = major > 6U || (major == 6U && minor >= 1U);
    std::size_t record_ordinal = 0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto target = resolve(i);
        const auto& source = entries[i];
        const auto& entry = entries[target];
        if (entry.mime >= mimes.size())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid ZIM MIME index");
        const bool text = mimes[entry.mime].rfind("text/html", 0U) == 0U ||
                          mimes[entry.mime].rfind("text/plain", 0U) == 0U;
        const auto& cluster = blobs(entry.cluster);
        if (entry.blob >= cluster.size())
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Invalid ZIM blob number");
        const std::string& data = cluster[entry.blob];
        const bool article =
            source.name_space == 'A' ||
            (new_namespaces && source.name_space == 'C' && text);
        if (article && text) {
            const std::string word =
                source.title.empty() ? source.url : source.title;
            if (word.empty() || !foundation::IsValidUtf8(word) ||
                !foundation::IsValidUtf8(data))
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid ZIM article text");
            auto [position, inserted] =
                article_map.emplace(target, reader.articles_.size());
            if (inserted) {
                const std::size_t article_ordinal = reader.articles_.size();
                reader.articles_.push_back(
                    mimes[entry.mime].rfind("text/plain", 0U) == 0U
                        ? "<pre>" + EscapeHtml(data) + "</pre>"
                        : data);
                reader.full_text_articles_.push_back(
                    {word, reader.articles_.back(), record_ordinal,
                     article_ordinal, target, entry.cluster, entry.blob});
            }
            reader.records_.push_back(
                {word, foundation::FoldForLookup(word), position->second});
            ++record_ordinal;
        } else if (source.name_space == 'M') {
            if (!foundation::IsValidUtf8(data))
                Throw(ErrorCode::kInvalidDictionary, path,
                      "Invalid UTF-8 in ZIM metadata");
            if (source.url == "Title")
                reader.metadata_.name = data;
            else if (source.url == "Description")
                reader.metadata_.description = data;
            else if (source.url == "Language")
                reader.metadata_.source_language =
                    reader.metadata_.target_language = data;
        } else if (source.name_space != 'X') {
            reader.resources_.emplace(
                std::string(1U, source.name_space) + "/" + source.url, data);
        }
    }
    if (reader.records_.empty())
        Throw(ErrorCode::kInvalidDictionary, path, "ZIM contains no articles");
    if (reader.metadata_.name.empty())
        reader.metadata_.name = path.stem().string();
    try {
        reader.source_snapshot_ =
            dictionary::CaptureSourceSnapshot(reader.files_.parts);
    } catch (const dictionary::GeneratedIndexError& error) {
        Throw(ErrorCode::kMissingFile, path, error.what());
    }
    return reader;
}

std::vector<FullTextArticle> Reader::ReadFullTextArticles() const {
    return full_text_articles_;
}

std::vector<const Reader::Record*> Reader::Ranked(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const auto folded = foundation::FoldForLookup(prefix);
    std::vector<const Record*> out;
    std::size_t n = 0;
    for (const auto& record : records_) {
        if (checkpoint && n++ % 1024U == 0U)
            checkpoint();
        if (Prefix(record.folded, folded))
            out.push_back(&record);
    }
    std::stable_sort(
        out.begin(), out.end(), [&folded](const Record* a, const Record* b) {
            const bool ae = a->folded == folded, be = b->folded == folded;
            if (ae != be)
                return ae;
            const auto as = Points(a->folded), bs = Points(b->folded);
            if (as != bs)
                return as < bs;
            return a->folded == b->folded ? a->word < b->word
                                          : a->folded < b->folded;
        });
    return out;
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
    std::vector<Article> out;
    if (!limit)
        return out;
    const auto folded = foundation::FoldForLookup(word);
    std::set<std::size_t> seen;
    std::size_t n = 0;
    for (const auto& r : records_) {
        if (checkpoint && n++ % 1024U == 0U)
            checkpoint();
        if (r.folded == folded && seen.insert(r.article).second) {
            out.push_back({r.word, articles_[r.article]});
            if (out.size() == limit)
                break;
        }
    }
    return out;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> out;
    if (!limit)
        return out;
    std::set<std::size_t> seen;
    for (const auto* r : Ranked(prefix, checkpoint))
        if (seen.insert(r->article).second) {
            out.push_back({r->word, articles_[r->article]});
            if (out.size() == limit)
                break;
        }
    return out;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> out;
    if (!limit)
        return out;
    std::unordered_set<std::string> seen;
    for (const auto* r : Ranked(prefix, checkpoint))
        if (seen.insert(r->folded).second) {
            out.push_back(r->word);
            if (out.size() == limit)
                break;
        }
    return out;
}

const std::string* Reader::Resource(std::string_view id) const {
    const auto found = resources_.find(std::string(id));
    return found == resources_.end() ? nullptr : &found->second;
}
}  // namespace goldendict::core::formats::zim
