// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/application.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace goldendict::core {
namespace {

constexpr std::string_view kHeader = "goldendict-core-config-v1\n";
constexpr std::size_t kMaximumConfigurationBytes = 1024U * 1024U;
constexpr std::size_t kMaximumDictionaryPaths = 256U;
constexpr std::size_t kMaximumSoundDirectories = 256U;
constexpr std::size_t kMaximumDictionaryGroups = 256U;
constexpr std::size_t kMaximumDictionariesPerGroup = 256U;
constexpr std::size_t kMaximumGroupValueBytes = 4096U;

bool IsUnescaped(unsigned char character) {
    return std::isalnum(character) != 0 || character == '-' ||
           character == '_' || character == '.' || character == '/' ||
           character == ':';
}

std::string Encode(std::string_view value) {
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char character : value) {
        if (IsUnescaped(character)) {
            encoded.push_back(static_cast<char>(character));
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[character >> 4U]);
            encoded.push_back(kHex[character & 0x0fU]);
        }
    }
    return encoded;
}

int HexValue(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::string Decode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            decoded.push_back(value[index]);
            continue;
        }
        if (index + 2U >= value.size()) {
            throw std::runtime_error("Truncated configuration escape");
        }
        const int high = HexValue(value[index + 1U]);
        const int low = HexValue(value[index + 2U]);
        if (high < 0 || low < 0) {
            throw std::runtime_error("Invalid configuration escape");
        }
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2U;
    }
    return decoded;
}

void Validate(const CoreConfiguration& configuration) {
    if (configuration.dictionary_paths.size() > kMaximumDictionaryPaths) {
        throw std::runtime_error("Configuration has too many dictionary paths");
    }
    if (configuration.sound_directories.size() > kMaximumSoundDirectories) {
        throw std::runtime_error(
            "Configuration has too many sound directories");
    }
    if (configuration.dictionary_groups.size() > kMaximumDictionaryGroups) {
        throw std::runtime_error(
            "Configuration has too many dictionary groups");
    }
    const auto has_nul = [](const std::string& value) {
        return value.find('\0') != std::string::npos;
    };
    if (std::any_of(
            configuration.sound_directories.begin(),
            configuration.sound_directories.end(),
            [](const auto& directory) { return directory.path.empty(); })) {
        throw std::runtime_error("Sound directory paths cannot be empty");
    }
    if (has_nul(configuration.index_directory) ||
        std::any_of(configuration.dictionary_paths.begin(),
                    configuration.dictionary_paths.end(), has_nul) ||
        std::any_of(configuration.sound_directories.begin(),
                    configuration.sound_directories.end(),
                    [&has_nul](const auto& directory) {
                        return has_nul(directory.path) ||
                               has_nul(directory.name);
                    })) {
        throw std::runtime_error("Configuration values cannot contain NUL");
    }

    std::unordered_set<std::uint32_t> group_ids;
    for (const auto& group : configuration.dictionary_groups) {
        if (group.id == 0U || !group_ids.insert(group.id).second) {
            throw std::runtime_error(
                "Dictionary group IDs must be unique and nonzero");
        }
        if (group.name.empty() || group.name.size() > kMaximumGroupValueBytes ||
            group.icon.size() > kMaximumGroupValueBytes ||
            has_nul(group.name) || has_nul(group.icon)) {
            throw std::runtime_error("Dictionary group metadata is invalid");
        }
        if (group.dictionary_ids.size() > kMaximumDictionariesPerGroup) {
            throw std::runtime_error(
                "Dictionary group has too many dictionaries");
        }
        std::unordered_set<std::string> dictionary_ids;
        for (const auto& dictionary_id : group.dictionary_ids) {
            if (dictionary_id.empty() ||
                dictionary_id.size() > kMaximumGroupValueBytes ||
                has_nul(dictionary_id) ||
                !dictionary_ids.insert(dictionary_id).second) {
                throw std::runtime_error(
                    "Dictionary group dictionary IDs must be unique and valid");
            }
        }
    }
}

}  // namespace

CoreConfiguration LoadConfiguration(const std::string& configuration_path) {
    std::ifstream input(configuration_path, std::ios::binary);
    if (!input) {
        std::error_code error;
        if (!std::filesystem::exists(configuration_path, error) && !error) {
            return {};
        }
        throw std::runtime_error("Cannot open configuration file");
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 ||
        static_cast<std::uintmax_t>(size) > kMaximumConfigurationBytes) {
        throw std::runtime_error("Cannot read bounded configuration file");
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    input.seekg(0, std::ios::beg);
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input ||
        input.gcount() != static_cast<std::streamsize>(contents.size())) {
        throw std::runtime_error("Cannot read complete configuration file");
    }
    if (contents.substr(0, kHeader.size()) != kHeader) {
        throw std::runtime_error("Unsupported configuration format");
    }

    CoreConfiguration configuration;
    bool has_index_directory = false;
    std::size_t position = kHeader.size();
    while (position < contents.size()) {
        const auto end = contents.find('\n', position);
        if (end == std::string::npos) {
            throw std::runtime_error("Configuration line is not terminated");
        }
        const std::string_view line(contents.data() + position, end - position);
        constexpr std::string_view kIndex = "index_directory=";
        constexpr std::string_view kDictionary = "dictionary_path=";
        constexpr std::string_view kSoundDirectory = "sound_directory=";
        constexpr std::string_view kDictionaryGroup = "dictionary_group=";
        if (line.substr(0, kIndex.size()) == kIndex) {
            if (has_index_directory) {
                throw std::runtime_error("Duplicate index directory");
            }
            has_index_directory = true;
            configuration.index_directory = Decode(line.substr(kIndex.size()));
        } else if (line.substr(0, kDictionary.size()) == kDictionary) {
            configuration.dictionary_paths.push_back(
                Decode(line.substr(kDictionary.size())));
        } else if (line.substr(0, kSoundDirectory.size()) == kSoundDirectory) {
            const auto value = line.substr(kSoundDirectory.size());
            const auto separator = value.find('|');
            if (separator == std::string_view::npos) {
                throw std::runtime_error("Malformed sound directory field");
            }
            configuration.sound_directories.push_back(
                {Decode(value.substr(0U, separator)),
                 Decode(value.substr(separator + 1U))});
        } else if (line.substr(0, kDictionaryGroup.size()) ==
                   kDictionaryGroup) {
            const auto value = line.substr(kDictionaryGroup.size());
            std::vector<std::string_view> fields;
            std::size_t field_position = 0U;
            while (field_position <= value.size()) {
                const auto separator = value.find('|', field_position);
                fields.push_back(value.substr(
                    field_position, separator == std::string_view::npos
                                        ? value.size() - field_position
                                        : separator - field_position));
                if (separator == std::string_view::npos) {
                    break;
                }
                field_position = separator + 1U;
            }
            if (fields.size() < 3U) {
                throw std::runtime_error("Malformed dictionary group field");
            }
            DictionaryGroupConfiguration group;
            const auto id_text = fields.front();
            const auto conversion = std::from_chars(
                id_text.data(), id_text.data() + id_text.size(), group.id, 10);
            if (conversion.ec != std::errc() ||
                conversion.ptr != id_text.data() + id_text.size()) {
                throw std::runtime_error("Dictionary group ID is invalid");
            }
            group.name = Decode(fields[1U]);
            group.icon = Decode(fields[2U]);
            for (std::size_t index = 3U; index < fields.size(); ++index) {
                group.dictionary_ids.push_back(Decode(fields[index]));
            }
            configuration.dictionary_groups.push_back(std::move(group));
        } else if (!line.empty()) {
            throw std::runtime_error("Unknown configuration field");
        }
        position = end + 1U;
    }
    Validate(configuration);
    return configuration;
}

void SaveConfiguration(const std::string& configuration_path,
                       const CoreConfiguration& configuration) {
    Validate(configuration);
    std::string contents(kHeader);
    contents +=
        "index_directory=" + Encode(configuration.index_directory) + "\n";
    for (const auto& path : configuration.dictionary_paths) {
        contents += "dictionary_path=" + Encode(path) + "\n";
    }
    for (const auto& directory : configuration.sound_directories) {
        contents += "sound_directory=" + Encode(directory.path) + "|" +
                    Encode(directory.name) + "\n";
    }
    for (const auto& group : configuration.dictionary_groups) {
        contents += "dictionary_group=" + std::to_string(group.id) + "|" +
                    Encode(group.name) + "|" + Encode(group.icon);
        for (const auto& dictionary_id : group.dictionary_ids) {
            contents += "|" + Encode(dictionary_id);
        }
        contents += "\n";
    }
    if (contents.size() > kMaximumConfigurationBytes) {
        throw std::runtime_error("Configuration exceeds the size limit");
    }

    const std::filesystem::path destination(configuration_path);
    if (!destination.parent_path().empty()) {
        std::filesystem::create_directories(destination.parent_path());
    }
    const auto temporary = destination.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(contents.data(),
                     static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("Cannot write configuration file");
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Cannot replace configuration file: " +
                                 error.message());
    }
}

}  // namespace goldendict::core
