// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_SOUNDDIR_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_SOUNDDIR_FIXTURE_H_
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace goldendict::core::formats::sounddir::test {
inline void WriteFile(const std::filesystem::path& path,
                      std::string_view data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!output)
        throw std::runtime_error("cannot write sound fixture");
}

inline std::filesystem::path WriteSoundDirectoryFixture(
    const std::filesystem::path& root) {
    WriteFile(root / "example.wav", "RIFFfixture-wave");
    WriteFile(root / "nested" / "second.ogg", "OggSfixture-ogg");
    WriteFile(root / "ignored.txt", "not audio");
    return root;
}
}  // namespace goldendict::core::formats::sounddir::test
#endif
