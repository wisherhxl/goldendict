// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
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
            loaded.index_directory != configuration.index_directory) {
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
            sdict_catalog.front().source_language != "eng" ||
            sdict_catalog.front().target_language != "deu" ||
            !sdict_response.errors.empty() ||
            sdict_response.entries.size() != 1U ||
            !sdict_response.entries.front()
                 .article.sanitized_html.has_value() ||
            sdict_response.entries.front().article.sanitized_html->find(
                "Installed SDict definition.") == std::string::npos) {
            return Fail("installed SDict lookup failed");
        }

        std::cout << "headless_api_test: installed fixture workflow passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        return Fail(error.what());
    }
}
