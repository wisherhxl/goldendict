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

        query.text = "missing";
        query.match_mode = goldendict::core::MatchMode::kExact;
        const auto missing = service->Lookup(query);
        if (!missing.entries.empty() || !missing.errors.empty()) {
            return Fail("missing lookup did not complete cleanly");
        }

        std::cout << "headless_api_test: installed fixture workflow passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        return Fail(error.what());
    }
}
