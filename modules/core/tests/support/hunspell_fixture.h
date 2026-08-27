// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_HUNSPELL_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_HUNSPELL_FIXTURE_H_

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "../../src/morphology/hunspell_discovery.h"

namespace goldendict::core::test {

inline void WriteHunspellBytes(const std::filesystem::path& path,
                               std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

inline morphology::hunspell::DataFiles WriteHunspellFixture(
    const std::filesystem::path& directory, std::string_view id,
    std::string_view affix_bytes, std::string_view dictionary_bytes) {
    const std::string name(id);
    morphology::hunspell::DataFiles files{directory / (name + ".aff"),
                                          directory / (name + ".dic"), name};
    WriteHunspellBytes(files.affix_file, affix_bytes);
    WriteHunspellBytes(files.dictionary_file, dictionary_bytes);
    return files;
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_HUNSPELL_FIXTURE_H_
