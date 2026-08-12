// SPDX-License-Identifier: GPL-3.0-or-later

#include "xdxf_reader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <set>
#include <unordered_set>
#include <utility>

#include <zlib.h>
#include <QByteArray>
#include <QXmlStreamReader>

#include "../../foundation/text_folding.h"
#include "../../foundation/utf8.h"

namespace goldendict::core::formats::xdxf {
namespace {

constexpr std::size_t kMaximumStoredFileSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumXmlSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumArticleSize = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumHeadwordSize = 16U * 1024U;
constexpr std::size_t kMaximumRecords = 10U * 1000U * 1000U;

[[noreturn]] void Throw(ErrorCode code, const std::filesystem::path& path,
                        std::string message) {
    throw Error(code, path, std::move(message));
}

std::string ReadFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        Throw(ErrorCode::kMissingFile, path, "Cannot inspect XDXF file");
    }
    if (size > kMaximumStoredFileSize) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "XDXF file exceeds the supported size limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Throw(ErrorCode::kMissingFile, path, "Cannot open XDXF file");
    }
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot read complete XDXF file");
    }
    return data;
}

bool HasCompressedSuffix(const std::filesystem::path& path) {
    std::string filename = path.filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return filename.size() >= 8U &&
           filename.compare(filename.size() - 8U, 8U, ".xdxf.dz") == 0;
}

std::string Gunzip(std::string_view compressed,
                   const std::filesystem::path& path) {
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<char*>(compressed.data()));  // zlib's API predates const.
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Cannot initialize XDXF gzip decompression");
    }
    std::string output;
    std::array<char, 64U * 1024U> buffer{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = buffer.size() - stream.avail_out;
        if (produced > kMaximumXmlSize - output.size()) {
            inflateEnd(&stream);
            Throw(ErrorCode::kInvalidDictionary, path,
                  "Decompressed XDXF exceeds the supported size limit");
        }
        output.append(buffer.data(), produced);
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.avail_in != 0U) {
        Throw(ErrorCode::kInvalidDictionary, path,
              "Invalid gzip-compressed XDXF file");
    }
    return output;
}

std::string Escape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
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
                escaped.push_back(character);
        }
    }
    return escaped;
}

std::string Trim(std::string value) {
    const auto space = [](unsigned char character) {
        return std::isspace(character) != 0;
    };
    value.erase(value.begin(),
                std::find_if_not(value.begin(), value.end(), space));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), space).base(),
                value.end());
    return value;
}

std::string LanguageCode(QStringView language) {
    QString code = language.toString().trimmed();
    const qsizetype separator = code.indexOf(QLatin1Char('-'));
    if (separator >= 0) {
        code.truncate(separator);
    }
    return code.toLower().toStdString();
}

struct Element {
    std::string text;
    std::string html;
    std::vector<std::string> keys;
};

std::string HtmlTag(QStringView name) {
    if (name == u"k" || name == u"def") {
        return "div";
    }
    if (name == u"ex") {
        return "blockquote";
    }
    if (name == u"tr" || name == u"pos" || name == u"co" || name == u"abr" ||
        name == u"abbr") {
        return "span";
    }
    if (name == u"b" || name == u"strong" || name == u"i" || name == u"em" ||
        name == u"u" || name == u"p" || name == u"ul" || name == u"ol" ||
        name == u"li" || name == u"dl" || name == u"dt" || name == u"dd" ||
        name == u"table" || name == u"thead" || name == u"tbody" ||
        name == u"tr" || name == u"th" || name == u"td" || name == u"code" ||
        name == u"pre") {
        return name.toString().toStdString();
    }
    return {};
}

Element ReadElement(QXmlStreamReader* stream,
                    const std::filesystem::path& path) {
    const QString name = stream->name().toString().toLower();
    Element result;
    while (!stream->atEnd()) {
        stream->readNext();
        if (stream->isCharacters()) {
            const auto text = stream->text().toString().toStdString();
            result.text += text;
            result.html += Escape(text);
        } else if (stream->isStartElement()) {
            Element child = ReadElement(stream, path);
            result.text += child.text;
            result.keys.insert(result.keys.end(), child.keys.begin(),
                               child.keys.end());
            result.html += std::move(child.html);
        } else if (stream->isEndElement()) {
            break;
        }
        if (result.html.size() > kMaximumArticleSize) {
            Throw(ErrorCode::kInvalidDictionary, path,
                  "XDXF article exceeds the supported size limit");
        }
    }
    if (name == u"k") {
        const std::string key = Trim(result.text);
        if (!key.empty()) {
            result.keys.push_back(key);
        }
    }
    if (name == u"kref" || name == u"iref") {
        const std::string target = Trim(result.text);
        result.html = target.empty() ? Escape(result.text)
                                     : "<a href=\"bword://" + Escape(target) +
                                           "\">" + Escape(result.text) + "</a>";
        return result;
    }
    if (name == u"rref") {
        const std::string resource = Trim(result.text);
        if (!resource.empty()) {
            result.html = "<img src=\"" + Escape(resource) + "\">";
        }
        return result;
    }
    if (name == u"br") {
        result.html = "<br>";
        return result;
    }
    const std::string tag = HtmlTag(name);
    if (!tag.empty()) {
        result.html = "<" + tag + ">" + result.html + "</" + tag + ">";
    }
    return result;
}

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

}  // namespace

Error::Error(ErrorCode code, std::filesystem::path path, std::string message)
    : std::runtime_error(std::move(message)),
      code_(code),
      path_(std::move(path)) {}

Reader Reader::Open(const std::filesystem::path& dictionary_path) {
    Reader reader;
    reader.dictionary_path_ = dictionary_path;
    std::string xml = ReadFile(dictionary_path);
    if (HasCompressedSuffix(dictionary_path)) {
        xml = Gunzip(xml, dictionary_path);
    } else if (xml.size() > kMaximumXmlSize) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "XDXF file exceeds the supported size limit");
    }
    if (!foundation::IsValidUtf8(xml)) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "XDXF is not valid UTF-8");
    }

    QXmlStreamReader stream(QByteArray::fromRawData(
        xml.data(), static_cast<qsizetype>(xml.size())));
    stream.setEntityExpansionLimit(4096);
    bool found_root = false;
    std::size_t total_article_bytes = 0;
    while (!stream.atEnd()) {
        stream.readNext();
        if (!stream.isStartElement()) {
            continue;
        }
        if (!found_root) {
            if (stream.name().compare(u"xdxf", Qt::CaseInsensitive) != 0) {
                Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                      "Not an XDXF dictionary file");
            }
            found_root = true;
            reader.metadata_.source_language =
                LanguageCode(stream.attributes().value(u"lang_from"));
            reader.metadata_.target_language =
                LanguageCode(stream.attributes().value(u"lang_to"));
            continue;
        }
        const QString name = stream.name().toString().toLower();
        if ((name == u"full_name" || name == u"full_title") &&
            reader.metadata_.name.empty()) {
            reader.metadata_.name =
                stream.readElementText(QXmlStreamReader::SkipChildElements)
                    .trimmed()
                    .toStdString();
        } else if (name == u"from" &&
                   reader.metadata_.source_language.empty()) {
            reader.metadata_.source_language =
                LanguageCode(stream.attributes().value(u"xml:lang"));
            stream.skipCurrentElement();
        } else if (name == u"to" && reader.metadata_.target_language.empty()) {
            reader.metadata_.target_language =
                LanguageCode(stream.attributes().value(u"xml:lang"));
            stream.skipCurrentElement();
        } else if (name == u"ar") {
            Element article = ReadElement(&stream, dictionary_path);
            if (article.keys.empty()) {
                Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                      "XDXF article has no headword");
            }
            if (article.html.size() > kMaximumXmlSize - total_article_bytes) {
                Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                      "Rendered XDXF articles exceed the supported size limit");
            }
            total_article_bytes += article.html.size();
            const std::size_t article_number = reader.articles_.size();
            reader.articles_.push_back(std::move(article.html));
            for (auto& key : article.keys) {
                if (reader.records_.size() == kMaximumRecords) {
                    Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                          "XDXF contains too many headwords");
                }
                key = Trim(std::move(key));
                if (key.empty() || key.size() > kMaximumHeadwordSize) {
                    Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                          "Invalid XDXF headword size");
                }
                try {
                    reader.records_.push_back(
                        {key, foundation::FoldForLookup(key), article_number});
                } catch (const foundation::TextFoldingError& error) {
                    Throw(ErrorCode::kInvalidDictionary, dictionary_path,
                          "Invalid UTF-8 XDXF headword: " +
                              std::string(error.what()));
                }
            }
        }
    }
    if (!found_root) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "Not an XDXF dictionary file");
    }
    if (stream.hasError()) {
        Throw(ErrorCode::kInvalidDictionary, dictionary_path,
              "Malformed XDXF XML at line " +
                  std::to_string(stream.lineNumber()) + ": " +
                  stream.errorString().toStdString());
    }
    if (reader.metadata_.name.empty()) {
        std::string filename = dictionary_path.filename().string();
        const auto suffix = HasCompressedSuffix(dictionary_path) ? 8U : 5U;
        reader.metadata_.name = filename.substr(0, filename.size() - suffix);
    }
    return reader;
}

std::vector<Article> Reader::LookupExact(
    std::string_view headword, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    if (result_limit == 0U) {
        return result;
    }
    const std::string folded = foundation::FoldForLookup(headword);
    std::set<std::size_t> seen;
    std::size_t number = 0;
    for (const auto& record : records_) {
        if (checkpoint && (number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (record.folded_headword == folded &&
            seen.insert(record.article).second) {
            result.push_back({record.headword, articles_[record.article]});
            if (result.size() == result_limit) {
                break;
            }
        }
    }
    return result;
}

std::vector<Article> Reader::LookupPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<Article> result;
    if (result_limit == 0U) {
        return result;
    }
    const auto matches = RankedPrefixMatches(prefix, checkpoint);
    std::set<std::size_t> seen;
    for (const auto* record : matches) {
        if (seen.insert(record->article).second) {
            result.push_back({record->headword, articles_[record->article]});
            if (result.size() == result_limit) {
                break;
            }
        }
    }
    return result;
}

std::vector<std::string> Reader::SuggestPrefix(
    std::string_view prefix, std::size_t result_limit,
    const std::function<void()>& checkpoint) const {
    std::vector<std::string> result;
    if (result_limit == 0U) {
        return result;
    }
    const auto matches = RankedPrefixMatches(prefix, checkpoint);
    std::unordered_set<std::string> seen;
    for (const auto* record : matches) {
        if (seen.insert(record->headword).second) {
            result.push_back(record->headword);
            if (result.size() == result_limit) {
                break;
            }
        }
    }
    return result;
}

std::vector<const Reader::Record*> Reader::RankedPrefixMatches(
    std::string_view prefix, const std::function<void()>& checkpoint) const {
    const std::string folded = foundation::FoldForLookup(prefix);
    if (folded.empty()) {
        return {};
    }
    std::vector<const Record*> matches;
    std::size_t number = 0;
    for (const auto& record : records_) {
        if (checkpoint && (number++ % 1024U) == 0U) {
            checkpoint();
        }
        if (HasPrefix(record.folded_headword, folded)) {
            matches.push_back(&record);
        }
    }
    std::stable_sort(
        matches.begin(), matches.end(),
        [&folded](const Record* left, const Record* right) {
            const bool left_exact = left->folded_headword == folded;
            const bool right_exact = right->folded_headword == folded;
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

}  // namespace goldendict::core::formats::xdxf
