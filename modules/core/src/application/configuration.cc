// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/application.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace goldendict::core {
namespace {

constexpr std::string_view kHeader = "goldendict-core-config-v1\n";
constexpr std::size_t kMaximumConfigurationBytes = 1024U * 1024U;
constexpr std::size_t kMaximumDictionaryPaths = 256U;

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
    const auto has_nul = [](const std::string& value) {
        return value.find('\0') != std::string::npos;
    };
    if (has_nul(configuration.index_directory) ||
        std::any_of(configuration.dictionary_paths.begin(),
                    configuration.dictionary_paths.end(), has_nul)) {
        throw std::runtime_error("Configuration paths cannot contain NUL");
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
        if (line.substr(0, kIndex.size()) == kIndex) {
            if (has_index_directory) {
                throw std::runtime_error("Duplicate index directory");
            }
            has_index_directory = true;
            configuration.index_directory = Decode(line.substr(kIndex.size()));
        } else if (line.substr(0, kDictionary.size()) == kDictionary) {
            configuration.dictionary_paths.push_back(
                Decode(line.substr(kDictionary.size())));
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
