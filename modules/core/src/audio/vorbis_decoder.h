// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_SRC_AUDIO_VORBIS_DECODER_H_
#define GOLDENDICT_CORE_SRC_AUDIO_VORBIS_DECODER_H_
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace goldendict::core::audio {
class VorbisError final : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

struct VorbisStreamInfo {
    int channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint64_t frames = 0;
};

VorbisStreamInfo InspectVorbis(std::string_view stream);
std::string DecodeVorbisRangeToWav(std::string_view stream,
                                   std::uint64_t frame_offset,
                                   std::uint64_t frame_count,
                                   std::size_t maximum_output_bytes);
}  // namespace goldendict::core::audio
#endif
