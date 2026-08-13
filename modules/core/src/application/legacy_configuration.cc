// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/application.h"

#include <expat.h>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace goldendict::core {
namespace {

constexpr std::size_t kMaximumLegacyConfigurationBytes = 1024U * 1024U;
constexpr std::size_t kMaximumLegacyValueBytes = 64U * 1024U;
constexpr std::size_t kMaximumXmlDepth = 64U;
constexpr std::size_t kMaximumImportedPaths = 256U;
constexpr std::size_t kMaximumDictionaryGroups = 256U;
constexpr std::size_t kMaximumDictionariesPerGroup = 256U;
constexpr std::size_t kMaximumGroupValueBytes = 4096U;
constexpr std::size_t kMaximumEncodedGroupIconBytes = 64U * 1024U;

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
    DictionaryGroupConfiguration group;
    std::unordered_set<std::uint32_t> group_ids;
    bool failed = false;
    std::string error;
};

bool IsGroupElement(const LegacyParserState& state) {
    return state.elements.size() == 3U && state.elements[0] == "config" &&
           state.elements[1] == "groups" && state.elements[2] == "group";
}

enum class GroupValue { kNone, kDictionary, kMuted, kPopupMuted };

GroupValue CurrentGroupValue(const LegacyParserState& state) {
    if (state.elements.size() == 4U && state.elements[0] == "config" &&
        state.elements[1] == "groups" && state.elements[2] == "group" &&
        state.elements[3] == "dictionary") {
        return GroupValue::kDictionary;
    }
    if (state.elements.size() == 5U && state.elements[0] == "config" &&
        state.elements[1] == "groups" && state.elements[2] == "group" &&
        state.elements[3] == "mutedDictionaries") {
        if (state.elements[4] == "mutedDictionary") {
            return GroupValue::kMuted;
        }
        if (state.elements[4] == "popupMutedDictionary") {
            return GroupValue::kPopupMuted;
        }
    }
    return GroupValue::kNone;
}

bool IsCanonicalBase64(std::string_view value) {
    if (value.empty())
        return true;
    if (value.size() > kMaximumEncodedGroupIconBytes || value.size() % 4U != 0U)
        return false;
    const auto decode = [](char character) {
        if (character >= 'A' && character <= 'Z')
            return character - 'A';
        if (character >= 'a' && character <= 'z')
            return character - 'a' + 26;
        if (character >= '0' && character <= '9')
            return character - '0' + 52;
        if (character == '+')
            return 62;
        if (character == '/')
            return 63;
        return -1;
    };
    std::size_t padding = value.back() == '=' ? 1U : 0U;
    if (padding == 1U && value[value.size() - 2U] == '=')
        padding = 2U;
    for (std::size_t index = 0U; index < value.size() - padding; ++index) {
        if (decode(value[index]) < 0)
            return false;
    }
    for (std::size_t index = value.size() - padding; index < value.size();
         ++index) {
        if (value[index] != '=')
            return false;
    }
    if (padding == 1U && (decode(value[value.size() - 2U]) & 3) != 0)
        return false;
    if (padding == 2U && (decode(value[value.size() - 3U]) & 15) != 0)
        return false;
    return true;
}

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
    if (IsPathElement(*state) || IsSoundDirectoryElement(*state) ||
        CurrentGroupValue(*state) != GroupValue::kNone) {
        Fail(state, "Legacy configuration values cannot contain markup");
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
    if (IsGroupElement(*state)) {
        if (state->configuration.dictionary_groups.size() ==
            kMaximumDictionaryGroups) {
            Fail(state, "Legacy configuration has too many dictionary groups");
            return;
        }
        state->group = {};
        bool has_id = false;
        for (std::size_t index = 0U; attributes[index] != nullptr;
             index += 2U) {
            const std::string_view key(attributes[index]);
            const std::string_view attribute(attributes[index + 1U]);
            if (attribute.size() > kMaximumGroupValueBytes &&
                key != "iconData") {
                Fail(state, "Legacy dictionary group metadata is too large");
                return;
            }
            if (key == "id") {
                has_id = true;
                const auto conversion = std::from_chars(
                    attribute.data(), attribute.data() + attribute.size(),
                    state->group.id, 10);
                if (conversion.ec != std::errc() ||
                    conversion.ptr != attribute.data() + attribute.size() ||
                    state->group.id == 0U) {
                    Fail(state, "Legacy dictionary group ID is invalid");
                    return;
                }
            } else if (key == "name") {
                state->group.name = attribute;
            } else if (key == "icon") {
                state->group.icon = attribute;
            } else if (key == "favoritesFolder") {
                state->group.favorites_folder = attribute;
            } else if (key == "shortcut") {
                state->group.shortcut = attribute;
            } else if (key == "iconData") {
                if (!IsCanonicalBase64(attribute)) {
                    Fail(state, "Legacy dictionary group icon data is invalid");
                    return;
                }
                state->group.encoded_icon_data = attribute;
            }
        }
        if (!has_id || state->group.name.empty() ||
            !state->group_ids.insert(state->group.id).second) {
            Fail(state, "Legacy dictionary group identity is invalid");
            return;
        }
        return;
    }
    if (CurrentGroupValue(*state) != GroupValue::kNone) {
        state->value.clear();
        return;
    }
    if (!IsPathElement(*state) && !IsSoundDirectoryElement(*state) &&
        CurrentGroupValue(*state) == GroupValue::kNone) {
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
    if (!IsPathElement(*state) && !IsSoundDirectoryElement(*state) &&
        CurrentGroupValue(*state) == GroupValue::kNone) {
        return;
    }
    const std::size_t maximum = CurrentGroupValue(*state) == GroupValue::kNone
                                    ? kMaximumLegacyValueBytes
                                    : kMaximumGroupValueBytes;
    if (length < 0 ||
        state->value.size() + static_cast<std::size_t>(length) > maximum) {
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
    } else if (CurrentGroupValue(*state) != GroupValue::kNone) {
        if (state->value.empty()) {
            Fail(state, "Legacy dictionary group reference is empty");
            return;
        }
        auto* destination = &state->group.dictionary_ids;
        if (CurrentGroupValue(*state) == GroupValue::kMuted) {
            destination = &state->group.muted_dictionary_ids;
        } else if (CurrentGroupValue(*state) == GroupValue::kPopupMuted) {
            destination = &state->group.popup_muted_dictionary_ids;
        }
        if (destination->size() == kMaximumDictionariesPerGroup) {
            Fail(state, "Legacy dictionary group has too many references");
            return;
        }
        if (std::find(destination->begin(), destination->end(), state->value) !=
            destination->end()) {
            Fail(state, "Legacy dictionary group has duplicate references");
            return;
        }
        destination->push_back(std::move(state->value));
    } else if (IsGroupElement(*state)) {
        state->configuration.dictionary_groups.push_back(
            std::move(state->group));
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
