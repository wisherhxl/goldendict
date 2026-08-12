// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "goldendict/core/application.h"

namespace {

class TemporaryDirectory final {
   public:
    TemporaryDirectory() {
        const auto suffix =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("goldendict-headless-consumer-" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const noexcept { return path_; }

   private:
    std::filesystem::path path_;
};

void AppendBigEndian32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>((value >> 24U) & 0xffU));
    output->push_back(static_cast<char>((value >> 16U) & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>(value & 0xffU));
}

void AppendBigEndian64(std::uint64_t value, std::string* output) {
    for (unsigned shift = 56U;; shift -= 8U) {
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
        if (shift == 0U) {
            break;
        }
    }
}

void AppendBigEndian16(std::uint16_t value, std::string* output) {
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>(value & 0xffU));
}

void AppendLittle16(std::uint16_t value, std::string* output) {
    output->push_back(static_cast<char>(value & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void AppendLittle32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>(value & 0xffU));
    output->push_back(static_cast<char>((value >> 8U) & 0xffU));
    output->push_back(static_cast<char>((value >> 16U) & 0xffU));
    output->push_back(static_cast<char>((value >> 24U) & 0xffU));
}

void WriteLittle32(std::uint32_t value, std::size_t offset,
                   std::string* output) {
    for (std::size_t byte = 0; byte < 4U; ++byte) {
        (*output)[offset + byte] =
            static_cast<char>((value >> (byte * 8U)) & 0xffU);
    }
}

void WriteLittle64(std::uint64_t value, std::size_t offset,
                   std::string* output) {
    for (std::size_t byte = 0; byte < 8U; ++byte) {
        (*output)[offset + byte] =
            static_cast<char>((value >> (byte * 8U)) & 0xffU);
    }
}

void WriteBigEndian64(std::uint64_t value, std::size_t offset,
                      std::string* output) {
    for (std::size_t byte = 0; byte < 8U; ++byte) {
        (*output)[offset + byte] =
            static_cast<char>((value >> ((7U - byte) * 8U)) & 0xffU);
    }
}

std::uint32_t Crc32(std::string_view data) {
    std::uint32_t crc = 0xffffffffU;
    for (const unsigned char byte : data) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^
                  (0xedb88320U & (0U - static_cast<std::uint32_t>(crc & 1U)));
        }
    }
    return ~crc;
}

std::uint32_t Adler32(std::string_view data) {
    std::uint32_t first = 1U;
    std::uint32_t second = 0U;
    for (const unsigned char byte : data) {
        first = (first + byte) % 65521U;
        second = (second + first) % 65521U;
    }
    return (second << 16U) | first;
}

std::string StoredGzip(std::string_view data) {
    if (data.size() > 65535U) {
        throw std::runtime_error("Consumer gzip fixture is too large");
    }
    std::string gzip{"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff", 10U};
    gzip.push_back('\x01');
    AppendLittle16(static_cast<std::uint16_t>(data.size()), &gzip);
    AppendLittle16(static_cast<std::uint16_t>(~data.size()), &gzip);
    gzip.append(data);
    AppendLittle32(Crc32(data), &gzip);
    AppendLittle32(static_cast<std::uint32_t>(data.size()), &gzip);
    return gzip;
}

std::string StoredZlib(std::string_view data) {
    if (data.size() > 65535U) {
        throw std::runtime_error("Consumer zlib fixture is too large");
    }
    std::string zlib{"\x78\x01", 2U};
    zlib.push_back('\x01');
    AppendLittle16(static_cast<std::uint16_t>(data.size()), &zlib);
    AppendLittle16(static_cast<std::uint16_t>(~data.size()), &zlib);
    zlib.append(data);
    AppendBigEndian32(Adler32(data), &zlib);
    return zlib;
}

void AppendBglBlock(unsigned type, std::string_view data, std::string* stream) {
    if (data.size() <= 11U) {
        stream->push_back(static_cast<char>(((data.size() + 4U) << 4U) | type));
    } else {
        if (data.size() > 255U) {
            throw std::runtime_error("Consumer BGL block is too large");
        }
        stream->push_back(static_cast<char>(type));
        stream->push_back(static_cast<char>(data.size()));
    }
    stream->append(data);
}

std::string EncodeDictdBase64(std::uint32_t value) {
    constexpr char digits[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (value == 0U) {
        return "A";
    }
    std::string result;
    while (value != 0U) {
        result.insert(result.begin(), digits[value % 64U]);
        value /= 64U;
    }
    return result;
}

void Write(const std::filesystem::path& path, const std::string& contents) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::runtime_error("Cannot write consumer fixture");
    }
}

void WriteFixture(const std::filesystem::path& root) {
    const std::vector<std::pair<std::string, std::string>> entries = {
        {"example",
         "<p>UTF-8: caf\xc3\xa9 <a href=\"bword://linked\">linked</a>"
         "<img src=\"images/pixel.png\"><script>active</script></p>"},
        {"linked", "<p>Linked result.</p>"}};
    std::string index;
    std::string dictionary;
    for (const auto& [headword, article] : entries) {
        index += headword;
        index.push_back('\0');
        AppendBigEndian32(static_cast<std::uint32_t>(dictionary.size()),
                          &index);
        AppendBigEndian32(static_cast<std::uint32_t>(article.size()), &index);
        dictionary += article;
    }
    const std::string info =
        "StarDict's dict ifo file\n"
        "version=2.4.2\n"
        "bookname=Installed Consumer Dictionary\n"
        "lang_from=en\n"
        "lang_to=fr\n"
        "wordcount=2\n"
        "idxfilesize=" +
        std::to_string(index.size()) + "\nsametypesequence=h\n";
    Write(root / "fixture.ifo", info);
    Write(root / "fixture.idx", index);
    Write(root / "fixture.dict", dictionary);
    Write(root / "res" / "images" / "pixel.png", "fixture-png");
}

void WriteDictdFixture(const std::filesystem::path& root) {
    const std::string article = "Installed Dictd definition.";
    const std::string index =
        "example\tA\t" +
        EncodeDictdBase64(static_cast<std::uint32_t>(article.size())) + "\n";
    Write(root / "fixture.index", index);
    Write(root / "fixture.dict", article);
}

void WriteSdictFixture(const std::filesystem::path& root) {
    const std::string headword = "example";
    const std::string title = "Installed SDict";
    const std::string article = "<b>Installed SDict definition.</b>";
    std::string file(43U, '\0');
    file.replace(0U, 4U, "sdct");
    file.replace(4U, 3U, "eng");
    file.replace(7U, 3U, "deu");
    WriteLittle32(1U, 11U, &file);
    WriteLittle32(43U, 19U, &file);
    AppendLittle32(static_cast<std::uint32_t>(title.size()), &file);
    file += title;
    WriteLittle32(static_cast<std::uint32_t>(file.size()), 35U, &file);
    AppendLittle16(static_cast<std::uint16_t>(8U + headword.size()), &file);
    AppendLittle16(0U, &file);
    AppendLittle32(0U, &file);
    file += headword;
    WriteLittle32(static_cast<std::uint32_t>(file.size()), 39U, &file);
    AppendLittle32(static_cast<std::uint32_t>(article.size()), &file);
    file += article;
    Write(root / "fixture.dct", file);
}

void WriteXdxfFixture(const std::filesystem::path& root) {
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<xdxf lang_from=\"eng\" lang_to=\"deu\" format=\"logical\" "
        "revision=\"34\"><full_name>Installed XDXF</full_name>"
        "<ar><k>example</k><def><b>Installed XDXF definition.</b>"
        "<rref>images/pixel.png</rref></def></ar></xdxf>";
    Write(root / "fixture.xdxf", xml);
    Write(root / "images" / "pixel.png", "xdxf-png");
}

void WriteGlsFixture(const std::filesystem::path& root) {
    const std::string glossary =
        "### Glossary title: Installed GLS\n"
        "### Source language: eng\n"
        "### Target language: deu\n"
        "### Glossary section:\n\n"
        "example\n"
        "<b>Installed GLS definition.</b> "
        "<img src=\"images/pixel.png\">\n";
    Write(root / "fixture.gls", glossary);
    Write(root / "images" / "pixel.png", "gls-png");
}

void WriteDslFixture(const std::filesystem::path& root) {
    const std::string dictionary =
        "#NAME \"Installed DSL\"\n"
        "#INDEX_LANGUAGE \"English\"\n"
        "#CONTENTS_LANGUAGE \"German\"\n"
        "#SOURCE_CODE_PAGE \"UTF-8\"\n"
        "example\n"
        "\t[b]Installed DSL definition.[/b] [s]images/pixel.png[/s]\n";
    const auto path = root / "fixture.dsl";
    Write(path, dictionary);
    Write(std::filesystem::u8path(path.string() + ".files") / "images" /
              "pixel.png",
          "dsl-png");
}

void WriteBglFixture(const std::filesystem::path& root) {
    std::string stream;
    AppendBglBlock(3U, std::string("\0\1Installed BGL", 15U), &stream);
    AppendBglBlock(3U, std::string("\0\7\0\0\0\0", 6U), &stream);
    AppendBglBlock(3U, std::string("\0\10\0\0\0\6", 6U), &stream);
    AppendBglBlock(3U, std::string("\0\21\0\0\200", 5U), &stream);
    const std::string definition =
        "<b>Installed BGL definition.</b><img src=\"pixel.png\">";
    std::string entry;
    entry.push_back(7);
    entry += "example";
    AppendBigEndian16(static_cast<std::uint16_t>(definition.size()), &entry);
    entry += definition;
    AppendBglBlock(1U, entry, &stream);
    std::string resource;
    resource.push_back(9);
    resource += "pixel.png";
    resource += "bgl-png";
    AppendBglBlock(2U, resource, &stream);
    stream.push_back(4);

    std::string file{"\x12\x34\x00\x01\x00\x06", 6U};
    file += StoredGzip(stream);
    Write(root / "fixture.bgl", file);
}

void WriteAardFixture(const std::filesystem::path& root) {
    const std::string metadata = StoredZlib(
        "{\"title\":\"Installed Aard\",\"index_language\":\"en\","
        "\"article_language\":\"de\"}");
    const std::string word = "example";
    const std::string article =
        StoredZlib("[\"<b>Installed Aard definition.</b>\"]");
    const std::uint32_t article_base = static_cast<std::uint32_t>(
        86U + metadata.size() + 8U + 2U + word.size());
    std::string file(44U, '\0');
    file.replace(0U, 4U, "aard");
    AppendBigEndian16(1U, &file);
    file.append(16U, '\0');
    AppendBigEndian16(1U, &file);
    AppendBigEndian16(1U, &file);
    AppendBigEndian32(static_cast<std::uint32_t>(metadata.size()), &file);
    AppendBigEndian32(1U, &file);
    AppendBigEndian32(article_base, &file);
    file.append(">LL", 3U);
    file.push_back('\0');
    file += ">H>L";
    file += metadata;
    AppendBigEndian32(0U, &file);
    AppendBigEndian32(0U, &file);
    AppendBigEndian16(static_cast<std::uint16_t>(word.size()), &file);
    file += word;
    AppendBigEndian32(static_cast<std::uint32_t>(article.size()), &file);
    file += article;
    Write(root / "fixture.aar", file);
}

void WriteZimFixture(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    const std::string article = "<b>Installed ZIM definition.</b>";
    std::string cluster_data(8U, '\0');
    WriteLittle32(8U, 0U, &cluster_data);
    WriteLittle32(8U + article.size(), 4U, &cluster_data);
    cluster_data += article;
    std::string cluster(1U, '\1');
    cluster += cluster_data;

    std::string file(80U, '\0');
    WriteLittle32(0x044d495aU, 0U, &file);
    file[4U] = 6;
    file[6U] = 1;
    WriteLittle32(1U, 24U, &file);
    WriteLittle32(1U, 28U, &file);
    WriteLittle64(80U, 56U, &file);
    file.append("text/html\0\0", 11U);
    const auto url_table = file.size();
    file.resize(file.size() + 8U);
    const auto cluster_table = file.size();
    file.resize(file.size() + 8U);
    WriteLittle64(file.size(), url_table, &file);
    const auto entry = file.size();
    file.resize(file.size() + 16U, '\0');
    file[entry + 3U] = 'A';
    file += "example";
    file.push_back('\0');
    file += "Example";
    file.push_back('\0');
    WriteLittle64(file.size(), cluster_table, &file);
    file += cluster;
    WriteLittle64(url_table, 32U, &file);
    WriteLittle64(url_table, 40U, &file);
    WriteLittle64(cluster_table, 48U, &file);
    WriteLittle64(file.size(), 72U, &file);
    Write(root / "fixture.zim", file);
}

void WriteSlobFixture(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    const auto tiny = [](std::string_view value, std::string* output) {
        output->push_back(static_cast<char>(value.size()));
        output->append(value);
    };
    const auto text = [](std::string_view value, std::string* output) {
        AppendBigEndian16(static_cast<std::uint16_t>(value.size()), output);
        output->append(value);
    };
    std::string file("\x21\x2d\x31SLOB\x1f", 8U);
    file.append(16U, '\0');
    tiny("UTF-8", &file);
    tiny("none", &file);
    file.push_back(1);
    tiny("name", &file);
    tiny("Installed SLOB", &file);
    file.push_back(1);
    text("text/html", &file);
    AppendBigEndian32(1U, &file);
    const auto store_field = file.size();
    AppendBigEndian64(0U, &file);
    const auto size_field = file.size();
    AppendBigEndian64(0U, &file);
    AppendBigEndian32(1U, &file);
    const auto ref_offset = file.size();
    AppendBigEndian64(0U, &file);
    text("example", &file);
    AppendBigEndian32(0U, &file);
    AppendBigEndian16(0U, &file);
    tiny("", &file);
    const auto store = file.size();
    AppendBigEndian32(1U, &file);
    AppendBigEndian64(0U, &file);
    const std::string article = "<b>Installed SLOB definition.</b>";
    std::string data;
    AppendBigEndian32(0U, &data);
    AppendBigEndian32(static_cast<std::uint32_t>(article.size()), &data);
    data += article;
    AppendBigEndian32(1U, &file);
    file.push_back(0);
    AppendBigEndian32(static_cast<std::uint32_t>(data.size()), &file);
    file += data;
    WriteBigEndian64(0U, ref_offset, &file);
    WriteBigEndian64(store, store_field, &file);
    WriteBigEndian64(file.size(), size_field, &file);
    Write(root / "fixture.slob", file);
}

void WriteEpwingFixture(const std::filesystem::path& root) {
    const auto set16 = [](std::uint16_t value, std::size_t at,
                          std::string* output) {
        (*output)[at] = static_cast<char>(value >> 8U);
        (*output)[at + 1U] = static_cast<char>(value);
    };
    const auto set32 = [](std::uint32_t value, std::size_t at,
                          std::string* output) {
        for (unsigned i = 0; i < 4U; ++i) {
            (*output)[at + i] = static_cast<char>(value >> ((3U - i) * 8U));
        }
    };
    std::string catalog(16U + 164U, '\0');
    set16(1U, 0U, &catalog);
    set16(1U, 2U, &catalog);
    catalog.replace(18U, 16U, "Installed EPWING");
    catalog.replace(98U, 7U, "FIXTURE");
    set16(1U, 110U, &catalog);
    Write(root / "CATALOGS", catalog);
    std::string language(16U, '\0');
    set16(1U, 0U, &language);
    Write(root / "LANGUAGE", language);
    std::string honmon(3U * 2048U, '\0');
    honmon[1U] = 1;
    honmon[16U] = static_cast<char>(0x92);
    set32(2U, 18U, &honmon);
    set32(1U, 22U, &honmon);
    honmon[2048U] = static_cast<char>(0xe0);
    set16(1U, 2050U, &honmon);
    std::size_t cursor = 2052U;
    const std::string word = "example";
    honmon[cursor++] = static_cast<char>(word.size());
    honmon.replace(cursor, word.size(), word);
    cursor += word.size();
    set32(3U, cursor, &honmon);
    set16(0U, cursor + 4U, &honmon);
    set32(3U, cursor + 6U, &honmon);
    set16(0U, cursor + 10U, &honmon);
    const std::string article = std::string("\x1f\x02", 2U) +
                                "Installed EPWING definition." +
                                std::string("\x1f\x03", 2U);
    honmon.replace(4096U, article.size(), article);
    Write(root / "FIXTURE" / "DATA" / "HONMON", honmon);
}

std::string MdictUtf16Le(std::string_view ascii) {
    std::string result;
    result.reserve((ascii.size() + 1U) * 2U);
    for (const unsigned char byte : ascii) {
        result.push_back(static_cast<char>(byte));
        result.push_back('\0');
    }
    result.append(2U, '\0');
    return result;
}

std::string MdictPlainBlock(std::string_view data) {
    std::string block(4U, '\0');
    AppendBigEndian32(Adler32(data), &block);
    block.append(data);
    return block;
}

void WriteMdictContainer(
    const std::filesystem::path& path, std::string_view title,
    const std::vector<std::pair<std::string, std::string>>& entries) {
    const std::string header_text =
        "<Dictionary GeneratedByEngineVersion=\"2.0\" Encoding=\"UTF-8\" "
        "Encrypted=\"0\" Left2Right=\"Yes\" Title=\"" +
        std::string(title) + "\"/>";
    const std::string header = MdictUtf16Le(header_text);
    std::string records;
    std::string keys;
    for (const auto& entry : entries) {
        AppendBigEndian64(records.size(), &keys);
        keys += entry.first;
        keys.push_back('\0');
        records += entry.second;
    }
    const std::string key_block = MdictPlainBlock(keys);
    std::string key_info;
    AppendBigEndian64(entries.size(), &key_info);
    AppendBigEndian16(static_cast<std::uint16_t>(entries.front().first.size()),
                      &key_info);
    key_info += entries.front().first;
    key_info.push_back('\0');
    AppendBigEndian16(static_cast<std::uint16_t>(entries.back().first.size()),
                      &key_info);
    key_info += entries.back().first;
    key_info.push_back('\0');
    AppendBigEndian64(key_block.size(), &key_info);
    AppendBigEndian64(keys.size(), &key_info);
    const std::string key_info_block = MdictPlainBlock(key_info);

    std::string file;
    AppendBigEndian32(static_cast<std::uint32_t>(header.size()), &file);
    file += header;
    AppendLittle32(Adler32(header), &file);
    std::string key_header;
    AppendBigEndian64(1U, &key_header);
    AppendBigEndian64(entries.size(), &key_header);
    AppendBigEndian64(key_info.size(), &key_header);
    AppendBigEndian64(key_info_block.size(), &key_header);
    AppendBigEndian64(key_block.size(), &key_header);
    file += key_header;
    AppendBigEndian32(Adler32(key_header), &file);
    file += key_info_block;
    file += key_block;

    const std::string record_block = MdictPlainBlock(records);
    AppendBigEndian64(1U, &file);
    AppendBigEndian64(entries.size(), &file);
    AppendBigEndian64(16U, &file);
    AppendBigEndian64(record_block.size(), &file);
    AppendBigEndian64(record_block.size(), &file);
    AppendBigEndian64(records.size(), &file);
    file += record_block;
    Write(path, file);
}

void WriteMdictFixture(const std::filesystem::path& root) {
    WriteMdictContainer(root / "fixture.mdx", "Installed MDict",
                        {{"example",
                          "<b>Installed MDict definition.</b>"
                          "<img src=\"pixel.png\">"}});
    WriteMdictContainer(root / "fixture.mdd", "Installed resources",
                        {{"\\pixel.png", "mdict-png"}});
}

int Fail(const std::string& message) {
    std::cerr << "headless_api_test: " << message << '\n';
    return EXIT_FAILURE;
}

}  // namespace

int main() {
    try {
        TemporaryDirectory directory;
        const auto dictionary_root = directory.path() / "dictionary";
        WriteFixture(dictionary_root);

        goldendict::core::CoreConfiguration configuration;
        configuration.dictionary_paths = {dictionary_root.string()};
        configuration.index_directory = (directory.path() / "indexes").string();
        const auto configuration_path = directory.path() / "core.conf";
        goldendict::core::SaveConfiguration(configuration_path.string(),
                                            configuration);
        const auto loaded =
            goldendict::core::LoadConfiguration(configuration_path.string());
        if (loaded.dictionary_paths != configuration.dictionary_paths ||
            loaded.index_directory != configuration.index_directory ||
            loaded.sound_directories != configuration.sound_directories) {
            return Fail("configuration round-trip failed");
        }

        auto service = goldendict::core::CreateDictionaryService(loaded);
        const auto catalog = service->GetCatalog();
        if (catalog.size() != 1U ||
            catalog.front().name != "Installed Consumer Dictionary" ||
            catalog.front().source !=
                (dictionary_root / "fixture.ifo").string()) {
            return Fail("catalog discovery or provenance failed");
        }

        goldendict::core::LookupQuery query;
        query.text = "example";
        query.result_limit = 1U;
        auto request = service->StartLookup(query);
        const auto response = request->Await();
        if (!request->IsFinished() || !response.errors.empty() ||
            response.entries.size() != 1U) {
            return Fail("asynchronous exact lookup failed");
        }
        const auto& entry = response.entries.front();
        if (entry.dictionary.id != catalog.front().id ||
            entry.language.source_language != "en" ||
            entry.language.target_language != "fr" ||
            entry.match.normalized_headword != "example" ||
            entry.article.plain_text.find("caf\xc3\xa9") == std::string::npos ||
            !entry.article.sanitized_html.has_value() ||
            entry.article.sanitized_html->find("<script") !=
                std::string::npos ||
            entry.resources.size() != 1U ||
            entry.resources.front().media_type != "image/png") {
            return Fail("structured inert result validation failed");
        }
        const auto resource = service->GetResource(entry.resources.front());
        const std::string resource_text(
            reinterpret_cast<const char*>(resource.data()), resource.size());
        if (resource_text != "fixture-png") {
            return Fail("typed resource retrieval failed");
        }
        if (!std::filesystem::exists(directory.path() / "indexes" /
                                     (catalog.front().id + ".gdidx"))) {
            return Fail("generated index was not created");
        }

        query.text = "EXA";
        query.match_mode = goldendict::core::MatchMode::kPrefix;
        const auto prefix = service->Lookup(query);
        if (!prefix.errors.empty() || prefix.entries.size() != 1U ||
            prefix.entries.front().match.mode !=
                goldendict::core::MatchMode::kPrefix ||
            prefix.entries.front().match.normalized_headword != "example") {
            return Fail("installed prefix lookup failed");
        }

        goldendict::core::SuggestionQuery suggestion_query;
        suggestion_query.text = "EXA";
        suggestion_query.result_limit = 1U;
        const auto suggestions = service->Suggest(suggestion_query);
        if (!suggestions.errors.empty() ||
            suggestions.suggestions.size() != 1U ||
            suggestions.suggestions.front().headword != "example" ||
            suggestions.suggestions.front().match.mode !=
                goldendict::core::MatchMode::kPrefix) {
            return Fail("installed headword suggestion failed");
        }

        query.text = "missing";
        query.match_mode = goldendict::core::MatchMode::kExact;
        const auto missing = service->Lookup(query);
        if (!missing.entries.empty() || !missing.errors.empty()) {
            return Fail("missing lookup did not complete cleanly");
        }

        const auto dictd_root = directory.path() / "dictd";
        WriteDictdFixture(dictd_root);
        goldendict::core::CoreConfiguration dictd_configuration;
        dictd_configuration.dictionary_paths = {dictd_root.string()};
        auto dictd_service =
            goldendict::core::CreateDictionaryService(dictd_configuration);
        const auto dictd_catalog = dictd_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto dictd_response = dictd_service->Lookup(query);
        if (dictd_catalog.size() != 1U ||
            dictd_catalog.front().id.rfind("dictd-", 0) != 0U ||
            !dictd_response.errors.empty() ||
            dictd_response.entries.size() != 1U ||
            dictd_response.entries.front().article.plain_text.find(
                "Installed Dictd definition.") == std::string::npos) {
            return Fail("installed Dictd lookup failed");
        }

        const auto sdict_root = directory.path() / "sdict";
        WriteSdictFixture(sdict_root);
        goldendict::core::CoreConfiguration sdict_configuration;
        sdict_configuration.dictionary_paths = {sdict_root.string()};
        auto sdict_service =
            goldendict::core::CreateDictionaryService(sdict_configuration);
        const auto sdict_catalog = sdict_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto sdict_response = sdict_service->Lookup(query);
        if (sdict_catalog.size() != 1U ||
            sdict_catalog.front().id.rfind("sdict-", 0) != 0U ||
            !sdict_response.errors.empty() ||
            sdict_response.entries.size() != 1U ||
            sdict_response.entries.front().language.source_language != "eng" ||
            sdict_response.entries.front().language.target_language != "deu" ||
            !sdict_response.entries.front()
                 .article.sanitized_html.has_value() ||
            sdict_response.entries.front().article.sanitized_html->find(
                "Installed SDict definition.") == std::string::npos) {
            return Fail("installed SDict lookup failed");
        }

        const auto xdxf_root = directory.path() / "xdxf";
        WriteXdxfFixture(xdxf_root);
        goldendict::core::CoreConfiguration xdxf_configuration;
        xdxf_configuration.dictionary_paths = {xdxf_root.string()};
        auto xdxf_service =
            goldendict::core::CreateDictionaryService(xdxf_configuration);
        const auto xdxf_catalog = xdxf_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto xdxf_response = xdxf_service->Lookup(query);
        if (xdxf_catalog.size() != 1U ||
            xdxf_catalog.front().id.rfind("xdxf-", 0) != 0U ||
            !xdxf_response.errors.empty() ||
            xdxf_response.entries.size() != 1U ||
            xdxf_response.entries.front().language.source_language != "eng" ||
            xdxf_response.entries.front().language.target_language != "deu" ||
            !xdxf_response.entries.front().article.sanitized_html.has_value() ||
            xdxf_response.entries.front().article.sanitized_html->find(
                "Installed XDXF definition.") == std::string::npos ||
            xdxf_response.entries.front().resources.size() != 1U) {
            return Fail("installed XDXF lookup failed");
        }
        const auto xdxf_resource = xdxf_service->GetResource(
            xdxf_response.entries.front().resources.front());
        const std::string xdxf_resource_text(
            reinterpret_cast<const char*>(xdxf_resource.data()),
            xdxf_resource.size());
        if (xdxf_resource_text != "xdxf-png") {
            return Fail("installed XDXF resource retrieval failed");
        }

        const auto gls_root = directory.path() / "gls";
        WriteGlsFixture(gls_root);
        goldendict::core::CoreConfiguration gls_configuration;
        gls_configuration.dictionary_paths = {gls_root.string()};
        auto gls_service =
            goldendict::core::CreateDictionaryService(gls_configuration);
        const auto gls_catalog = gls_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto gls_response = gls_service->Lookup(query);
        if (gls_catalog.size() != 1U ||
            gls_catalog.front().id.rfind("gls-", 0) != 0U ||
            !gls_response.errors.empty() || gls_response.entries.size() != 1U ||
            gls_response.entries.front().language.source_language != "eng" ||
            gls_response.entries.front().language.target_language != "deu" ||
            !gls_response.entries.front().article.sanitized_html.has_value() ||
            gls_response.entries.front().article.sanitized_html->find(
                "Installed GLS definition.") == std::string::npos ||
            gls_response.entries.front().resources.size() != 1U) {
            return Fail("installed GLS lookup failed");
        }
        const auto gls_resource = gls_service->GetResource(
            gls_response.entries.front().resources.front());
        const std::string gls_resource_text(
            reinterpret_cast<const char*>(gls_resource.data()),
            gls_resource.size());
        if (gls_resource_text != "gls-png") {
            return Fail("installed GLS resource retrieval failed");
        }

        const auto dsl_root = directory.path() / "dsl";
        WriteDslFixture(dsl_root);
        goldendict::core::CoreConfiguration dsl_configuration;
        dsl_configuration.dictionary_paths = {dsl_root.string()};
        auto dsl_service =
            goldendict::core::CreateDictionaryService(dsl_configuration);
        const auto dsl_catalog = dsl_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto dsl_response = dsl_service->Lookup(query);
        if (dsl_catalog.size() != 1U ||
            dsl_catalog.front().id.rfind("dsl-", 0) != 0U ||
            !dsl_response.errors.empty() || dsl_response.entries.size() != 1U ||
            dsl_response.entries.front().language.source_language != "en" ||
            dsl_response.entries.front().language.target_language != "de" ||
            !dsl_response.entries.front().article.sanitized_html.has_value() ||
            dsl_response.entries.front().article.sanitized_html->find(
                "Installed DSL definition.") == std::string::npos ||
            dsl_response.entries.front().resources.size() != 1U) {
            return Fail("installed DSL lookup failed");
        }
        const auto dsl_resource = dsl_service->GetResource(
            dsl_response.entries.front().resources.front());
        const std::string dsl_resource_text(
            reinterpret_cast<const char*>(dsl_resource.data()),
            dsl_resource.size());
        if (dsl_resource_text != "dsl-png") {
            return Fail("installed DSL resource retrieval failed");
        }

        const auto bgl_root = directory.path() / "bgl";
        WriteBglFixture(bgl_root);
        goldendict::core::CoreConfiguration bgl_configuration;
        bgl_configuration.dictionary_paths = {bgl_root.string()};
        auto bgl_service =
            goldendict::core::CreateDictionaryService(bgl_configuration);
        const auto bgl_catalog = bgl_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto bgl_response = bgl_service->Lookup(query);
        if (bgl_catalog.size() != 1U ||
            bgl_catalog.front().id.rfind("bgl-", 0) != 0U ||
            !bgl_response.errors.empty() || bgl_response.entries.size() != 1U ||
            bgl_response.entries.front().language.source_language != "en" ||
            bgl_response.entries.front().language.target_language != "de" ||
            !bgl_response.entries.front().article.sanitized_html.has_value() ||
            bgl_response.entries.front().article.sanitized_html->find(
                "Installed BGL definition.") == std::string::npos ||
            bgl_response.entries.front().resources.size() != 1U) {
            return Fail("installed BGL lookup failed");
        }
        const auto bgl_resource = bgl_service->GetResource(
            bgl_response.entries.front().resources.front());
        const std::string bgl_resource_text(
            reinterpret_cast<const char*>(bgl_resource.data()),
            bgl_resource.size());
        if (bgl_resource_text != "bgl-png") {
            return Fail("installed BGL resource retrieval failed");
        }

        const auto mdict_root = directory.path() / "mdict";
        WriteMdictFixture(mdict_root);
        goldendict::core::CoreConfiguration mdict_configuration;
        mdict_configuration.dictionary_paths = {mdict_root.string()};
        auto mdict_service =
            goldendict::core::CreateDictionaryService(mdict_configuration);
        const auto mdict_catalog = mdict_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto mdict_response = mdict_service->Lookup(query);
        if (mdict_catalog.size() != 1U ||
            mdict_catalog.front().id.rfind("mdict-", 0) != 0U ||
            mdict_catalog.front().name != "Installed MDict" ||
            !mdict_response.errors.empty() ||
            mdict_response.entries.size() != 1U ||
            !mdict_response.entries.front()
                 .article.sanitized_html.has_value() ||
            mdict_response.entries.front().article.sanitized_html->find(
                "Installed MDict definition.") == std::string::npos ||
            mdict_response.entries.front().resources.size() != 1U) {
            return Fail("installed MDict lookup failed");
        }
        const auto mdict_resource = mdict_service->GetResource(
            mdict_response.entries.front().resources.front());
        const std::string mdict_resource_text(
            reinterpret_cast<const char*>(mdict_resource.data()),
            mdict_resource.size());
        if (mdict_resource_text != "mdict-png") {
            return Fail("installed MDict resource retrieval failed");
        }

        const auto aard_root = directory.path() / "aard";
        WriteAardFixture(aard_root);
        goldendict::core::CoreConfiguration aard_configuration;
        aard_configuration.dictionary_paths = {aard_root.string()};
        auto aard_service =
            goldendict::core::CreateDictionaryService(aard_configuration);
        const auto aard_catalog = aard_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto aard_response = aard_service->Lookup(query);
        if (aard_catalog.size() != 1U ||
            aard_catalog.front().id.rfind("aard-", 0) != 0U ||
            aard_catalog.front().name != "Installed Aard" ||
            !aard_response.errors.empty() ||
            aard_response.entries.size() != 1U ||
            aard_response.entries.front().language.source_language != "en" ||
            aard_response.entries.front().language.target_language != "de" ||
            !aard_response.entries.front().article.sanitized_html.has_value() ||
            aard_response.entries.front().article.sanitized_html->find(
                "Installed Aard definition.") == std::string::npos) {
            return Fail("installed Aard lookup failed");
        }

        const auto zim_root = directory.path() / "zim";
        WriteZimFixture(zim_root);
        goldendict::core::CoreConfiguration zim_configuration;
        zim_configuration.dictionary_paths = {zim_root.string()};
        auto zim_service =
            goldendict::core::CreateDictionaryService(zim_configuration);
        const auto zim_catalog = zim_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto zim_response = zim_service->Lookup(query);
        if (zim_catalog.size() != 1U ||
            zim_catalog.front().id.rfind("zim-", 0) != 0U ||
            !zim_response.errors.empty() || zim_response.entries.size() != 1U ||
            !zim_response.entries.front().article.sanitized_html.has_value() ||
            zim_response.entries.front().article.sanitized_html->find(
                "Installed ZIM definition.") == std::string::npos) {
            return Fail("installed ZIM lookup failed");
        }

        const auto slob_root = directory.path() / "slob";
        WriteSlobFixture(slob_root);
        goldendict::core::CoreConfiguration slob_configuration;
        slob_configuration.dictionary_paths = {slob_root.string()};
        auto slob_service =
            goldendict::core::CreateDictionaryService(slob_configuration);
        const auto slob_catalog = slob_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto slob_response = slob_service->Lookup(query);
        if (slob_catalog.size() != 1U ||
            slob_catalog.front().id.rfind("slob-", 0) != 0U ||
            slob_catalog.front().name != "Installed SLOB" ||
            !slob_response.errors.empty() ||
            slob_response.entries.size() != 1U ||
            !slob_response.entries.front().article.sanitized_html.has_value() ||
            slob_response.entries.front().article.sanitized_html->find(
                "Installed SLOB definition.") == std::string::npos) {
            return Fail("installed SLOB lookup failed");
        }

        const auto epwing_root = directory.path() / "epwing";
        WriteEpwingFixture(epwing_root);
        goldendict::core::CoreConfiguration epwing_configuration;
        epwing_configuration.dictionary_paths = {epwing_root.string()};
        auto epwing_service =
            goldendict::core::CreateDictionaryService(epwing_configuration);
        const auto epwing_catalog = epwing_service->GetCatalog();
        query = {};
        query.text = "EXAMPLE";
        const auto epwing_response = epwing_service->Lookup(query);
        if (epwing_catalog.size() != 1U ||
            epwing_catalog.front().id.rfind("epwing-", 0) != 0U ||
            epwing_catalog.front().name != "Installed EPWING" ||
            !epwing_response.errors.empty() ||
            epwing_response.entries.size() != 1U ||
            !epwing_response.entries.front()
                 .article.sanitized_html.has_value() ||
            epwing_response.entries.front().article.sanitized_html->find(
                "Installed EPWING definition.") == std::string::npos) {
            return Fail("installed EPWING lookup failed");
        }

        const auto sound_root = directory.path() / "sounds";
        Write(sound_root / "nested" / "spoken.ogg", "OggSconsumer-audio");
        goldendict::core::CoreConfiguration sound_configuration;
        sound_configuration.sound_directories = {
            {sound_root.string(), "Installed sounds"}};
        auto sound_service =
            goldendict::core::CreateDictionaryService(sound_configuration);
        const auto sound_catalog = sound_service->GetCatalog();
        query = {};
        query.text = "SPOKEN";
        const auto sound_response = sound_service->Lookup(query);
        if (sound_catalog.size() != 1U ||
            sound_catalog.front().id.rfind("sounddir-", 0) != 0U ||
            sound_catalog.front().name != "Installed sounds" ||
            !sound_response.errors.empty() ||
            sound_response.entries.size() != 1U ||
            sound_response.entries.front().resources.size() != 1U ||
            sound_service
                    ->GetResource(
                        sound_response.entries.front().resources.front())
                    .size() != 18U) {
            return Fail("installed sound directory lookup failed");
        }

        std::cout << "headless_api_test: installed fixture workflow passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        return Fail(error.what());
    }
}
