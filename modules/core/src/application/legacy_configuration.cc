// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/application.h"

#include <expat.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace goldendict::core {
namespace {

constexpr std::size_t kMaximumLegacyConfigurationBytes = 1024U * 1024U;
constexpr std::size_t kMaximumLegacyValueBytes = 64U * 1024U;
constexpr std::size_t kMaximumXmlDepth = 64U;
constexpr std::size_t kMaximumImportedPaths = 256U;

struct ParserDeleter {
    void operator()(XML_Parser parser) const noexcept {
        XML_ParserFree(parser);
    }
};

struct LegacyParserState {
    XML_Parser parser = nullptr;
    CoreConfiguration configuration;
    std::vector<std::string> elements;
    std::string value;
    std::string sound_name;
    bool failed = false;
    std::string error;
};

bool IsPathElement(const LegacyParserState& state) {
    return state.elements.size() == 3U && state.elements[0] == "config" &&
           state.elements[1] == "paths" && state.elements[2] == "path";
}

bool IsSoundDirectoryElement(const LegacyParserState& state) {
    return state.elements.size() == 3U && state.elements[0] == "config" &&
           state.elements[1] == "sounddirs" && state.elements[2] == "sounddir";
}

void Fail(LegacyParserState* state, std::string message) {
    if (state->failed) {
        return;
    }
    state->failed = true;
    state->error = std::move(message);
    XML_StopParser(state->parser, XML_FALSE);
}

void XMLCALL StartElement(void* user_data, const XML_Char* name,
                          const XML_Char** attributes) {
    auto* state = static_cast<LegacyParserState*>(user_data);
    if (IsPathElement(*state) || IsSoundDirectoryElement(*state)) {
        Fail(state, "Legacy configuration paths cannot contain markup");
        return;
    }
    if (state->elements.size() == kMaximumXmlDepth) {
        Fail(state, "Legacy configuration XML is too deeply nested");
        return;
    }
    state->elements.emplace_back(name);
    if (state->elements.size() == 1U && state->elements.front() != "config") {
        Fail(state, "Legacy configuration has an invalid root element");
        return;
    }
    if (!IsPathElement(*state) && !IsSoundDirectoryElement(*state)) {
        return;
    }
    state->value.clear();
    state->sound_name.clear();
    if (IsSoundDirectoryElement(*state)) {
        for (std::size_t index = 0U; attributes[index] != nullptr;
             index += 2U) {
            if (std::string_view(attributes[index]) == "name") {
                if (std::string_view(attributes[index + 1U]).size() >
                    kMaximumLegacyValueBytes) {
                    Fail(state, "Legacy sound directory name is too large");
                    return;
                }
                state->sound_name = attributes[index + 1U];
            }
        }
    }
}

void XMLCALL CharacterData(void* user_data, const XML_Char* value, int length) {
    auto* state = static_cast<LegacyParserState*>(user_data);
    if (!IsPathElement(*state) && !IsSoundDirectoryElement(*state)) {
        return;
    }
    if (length < 0 || state->value.size() + static_cast<std::size_t>(length) >
                          kMaximumLegacyValueBytes) {
        Fail(state, "Legacy configuration value is too large");
        return;
    }
    state->value.append(value, static_cast<std::size_t>(length));
}

void XMLCALL EndElement(void* user_data, const XML_Char*) {
    auto* state = static_cast<LegacyParserState*>(user_data);
    if (IsPathElement(*state)) {
        if (state->value.empty()) {
            Fail(state, "Legacy dictionary path is empty");
            return;
        }
        if (state->configuration.dictionary_paths.size() ==
            kMaximumImportedPaths) {
            Fail(state, "Legacy configuration has too many dictionary paths");
            return;
        }
        state->configuration.dictionary_paths.push_back(
            std::move(state->value));
    } else if (IsSoundDirectoryElement(*state)) {
        if (state->value.empty()) {
            Fail(state, "Legacy sound directory path is empty");
            return;
        }
        if (state->configuration.sound_directories.size() ==
            kMaximumImportedPaths) {
            Fail(state, "Legacy configuration has too many sound directories");
            return;
        }
        state->configuration.sound_directories.push_back(
            {std::move(state->value), std::move(state->sound_name)});
    }
    state->elements.pop_back();
}

void XMLCALL RejectEntity(void* user_data, const XML_Char*, int,
                          const XML_Char*, int, const XML_Char*,
                          const XML_Char*, const XML_Char*, const XML_Char*) {
    Fail(static_cast<LegacyParserState*>(user_data),
         "Legacy configuration entities are not supported");
}

std::string ReadLegacyConfiguration(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open legacy configuration file");
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 ||
        static_cast<std::uintmax_t>(size) > kMaximumLegacyConfigurationBytes) {
        throw std::runtime_error(
            "Cannot read bounded legacy configuration file");
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    input.seekg(0, std::ios::beg);
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input ||
        input.gcount() != static_cast<std::streamsize>(contents.size())) {
        throw std::runtime_error(
            "Cannot read complete legacy configuration file");
    }
    return contents;
}

CoreConfiguration ImportLegacyConfiguration(const std::string& path,
                                            std::string index_directory) {
    const std::string contents = ReadLegacyConfiguration(path);
    std::unique_ptr<std::remove_pointer_t<XML_Parser>, ParserDeleter> parser(
        XML_ParserCreate(nullptr));
    if (!parser) {
        throw std::runtime_error("Cannot create legacy configuration parser");
    }
    LegacyParserState state;
    state.parser = parser.get();
    state.configuration.index_directory = std::move(index_directory);
    XML_SetUserData(parser.get(), &state);
    XML_SetElementHandler(parser.get(), StartElement, EndElement);
    XML_SetCharacterDataHandler(parser.get(), CharacterData);
    XML_SetEntityDeclHandler(parser.get(), RejectEntity);
    const XML_Status status = XML_Parse(parser.get(), contents.data(),
                                        static_cast<int>(contents.size()), 1);
    if (state.failed) {
        throw std::runtime_error(state.error);
    }
    if (status != XML_STATUS_OK) {
        throw std::runtime_error(
            std::string("Malformed legacy configuration XML: ") +
            XML_ErrorString(XML_GetErrorCode(parser.get())));
    }
    if (!state.elements.empty()) {
        throw std::runtime_error("Legacy configuration XML is incomplete");
    }
    return state.configuration;
}

bool Exists(const std::string& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw std::runtime_error("Cannot inspect configuration path: " +
                                 error.message());
    }
    return exists;
}

}  // namespace

CoreConfiguration LoadOrMigrateConfiguration(
    const std::string& configuration_path,
    const std::string& legacy_configuration_path,
    const std::string& index_directory) {
    if (Exists(configuration_path)) {
        return LoadConfiguration(configuration_path);
    }
    if (!Exists(legacy_configuration_path)) {
        CoreConfiguration configuration;
        configuration.index_directory = index_directory;
        return configuration;
    }
    CoreConfiguration configuration =
        ImportLegacyConfiguration(legacy_configuration_path, index_directory);
    SaveConfiguration(configuration_path, configuration);
    return configuration;
}

}  // namespace goldendict::core
