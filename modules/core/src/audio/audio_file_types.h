// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_SRC_AUDIO_AUDIO_FILE_TYPES_H_
#define GOLDENDICT_CORE_SRC_AUDIO_AUDIO_FILE_TYPES_H_
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace goldendict::core::audio {
inline std::string LowerAudioExtension(std::string_view name) {
    std::size_t end = name.size();
    while (end > 0U &&
           std::isspace(static_cast<unsigned char>(name[end - 1U])) != 0)
        --end;
    name = name.substr(0U, end);
    const auto slash = name.find_last_of("/\\");
    const auto dot = name.find_last_of('.');
    if (dot == std::string_view::npos ||
        (slash != std::string_view::npos && dot < slash))
        return {};
    std::string extension(name.substr(dot));
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

inline bool IsSupportedAudioFile(std::string_view name) {
    static constexpr std::array<std::string_view, 19U> kExtensions = {
        ".wav", ".au",   ".voc",  ".ogg", ".oga", ".mp3", ".m4a",
        ".aac", ".flac", ".mid",  ".kar", ".mpc", ".wma", ".wv",
        ".ape", ".spx",  ".opus", ".mpa", ".mp2"};
    const auto extension = LowerAudioExtension(name);
    return std::find(kExtensions.begin(), kExtensions.end(), extension) !=
           kExtensions.end();
}

inline std::string MediaTypeForAudioFile(std::string_view name) {
    const auto extension = LowerAudioExtension(name);
    if (extension == ".wav")
        return "audio/wav";
    if (extension == ".ogg" || extension == ".oga" || extension == ".spx")
        return "audio/ogg";
    if (extension == ".mp3" || extension == ".mpa" || extension == ".mp2")
        return "audio/mpeg";
    if (extension == ".flac")
        return "audio/flac";
    if (extension == ".opus")
        return "audio/opus";
    if (extension == ".m4a")
        return "audio/mp4";
    if (extension == ".aac")
        return "audio/aac";
    if (extension == ".mid" || extension == ".kar")
        return "audio/midi";
    return "application/octet-stream";
}
}  // namespace goldendict::core::audio
#endif
