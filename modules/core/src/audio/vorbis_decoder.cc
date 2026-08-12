// SPDX-License-Identifier: GPL-3.0-or-later
#include "vorbis_decoder.h"
#include <vorbis/vorbisfile.h>
#include <algorithm>
#include <climits>
#include <cstring>

namespace goldendict::core::audio {
namespace {
struct MemorySource {
    std::string_view data;
    std::size_t cursor = 0;
};

std::size_t Read(void* pointer, std::size_t size, std::size_t count,
                 void* source) {
    auto* memory = static_cast<MemorySource*>(source);
    if (size == 0U)
        return 0U;
    const auto items =
        std::min(count, (memory->data.size() - memory->cursor) / size);
    const auto bytes = items * size;
    std::memcpy(pointer, memory->data.data() + memory->cursor, bytes);
    memory->cursor += bytes;
    return items;
}

int Seek(void* source, ogg_int64_t offset, int whence) {
    auto* memory = static_cast<MemorySource*>(source);
    ogg_int64_t base = 0;
    if (whence == SEEK_CUR)
        base = static_cast<ogg_int64_t>(memory->cursor);
    else if (whence == SEEK_END)
        base = static_cast<ogg_int64_t>(memory->data.size());
    else if (whence != SEEK_SET)
        return -1;
    if (offset < -base || base + offset < 0 ||
        static_cast<std::uint64_t>(base + offset) > memory->data.size())
        return -1;
    memory->cursor = static_cast<std::size_t>(base + offset);
    return 0;
}

long Tell(void* source) {
    return static_cast<long>(static_cast<MemorySource*>(source)->cursor);
}

struct Handle {
    MemorySource source;
    OggVorbis_File value{};
    bool open = false;

    explicit Handle(std::string_view bytes) : source{bytes, 0U} {
        const ov_callbacks callbacks{Read, Seek, nullptr, Tell};
        if (ov_open_callbacks(&source, &value, nullptr, 0, callbacks) != 0)
            throw VorbisError("Invalid Ogg Vorbis stream");
        open = true;
    }

    ~Handle() {
        if (open)
            ov_clear(&value);
    }
};

VorbisStreamInfo Info(Handle* handle) {
    const auto* info = ov_info(&handle->value, -1);
    const auto frames = ov_pcm_total(&handle->value, -1);
    if (info == nullptr || info->channels < 1 || info->channels > 8 ||
        info->rate <= 0 || info->rate > 768000 || frames < 0)
        throw VorbisError("Invalid Ogg Vorbis stream information");
    return {info->channels, static_cast<std::uint32_t>(info->rate),
            static_cast<std::uint64_t>(frames)};
}

void Put16(std::uint16_t value, std::string* data, std::size_t at) {
    (*data)[at] = static_cast<char>(value);
    (*data)[at + 1U] = static_cast<char>(value >> 8U);
}

void Put32(std::uint32_t value, std::string* data, std::size_t at) {
    for (unsigned i = 0; i < 4U; ++i)
        (*data)[at + i] = static_cast<char>(value >> (i * 8U));
}
}  // namespace

VorbisStreamInfo InspectVorbis(std::string_view stream) {
    Handle handle(stream);
    return Info(&handle);
}

std::string DecodeVorbisRangeToWav(std::string_view stream,
                                   std::uint64_t frame_offset,
                                   std::uint64_t frame_count,
                                   std::size_t maximum_output_bytes) {
    Handle handle(stream);
    const auto info = Info(&handle);
    if (frame_offset > info.frames || frame_count > info.frames - frame_offset)
        throw VorbisError("Vorbis sample range is out of bounds");
    const auto bytes_per_frame = static_cast<unsigned>(info.channels) * 2U;
    if (maximum_output_bytes < 44U ||
        frame_count > (maximum_output_bytes - 44U) / bytes_per_frame)
        throw VorbisError("Decoded Vorbis range exceeds the output limit");
    const auto pcm_bytes = frame_count * bytes_per_frame;
    if (pcm_bytes > UINT32_MAX)
        throw VorbisError("Decoded Vorbis range exceeds the WAV size limit");
    if (ov_pcm_seek(&handle.value, static_cast<ogg_int64_t>(frame_offset)) != 0)
        throw VorbisError("Cannot seek Ogg Vorbis stream");
    std::string wav(44U + static_cast<std::size_t>(pcm_bytes), '\0');
    wav.replace(0U, 4U, "RIFF");
    Put32(static_cast<std::uint32_t>(wav.size() - 8U), &wav, 4U);
    wav.replace(8U, 8U, "WAVEfmt ");
    Put32(16U, &wav, 16U);
    Put16(1U, &wav, 20U);
    Put16(static_cast<std::uint16_t>(info.channels), &wav, 22U);
    Put32(info.sample_rate, &wav, 24U);
    Put32(info.sample_rate * static_cast<unsigned>(info.channels) * 2U, &wav,
          28U);
    Put16(static_cast<std::uint16_t>(info.channels * 2), &wav, 32U);
    Put16(16U, &wav, 34U);
    wav.replace(36U, 4U, "data");
    Put32(static_cast<std::uint32_t>(pcm_bytes), &wav, 40U);
    std::size_t cursor = 44U;
    int bitstream = 0;
    while (cursor < wav.size()) {
        const long read = ov_read(&handle.value, wav.data() + cursor,
                                  static_cast<int>(std::min<std::size_t>(
                                      wav.size() - cursor, INT_MAX)),
                                  0, 2, 1, &bitstream);
        if (read <= 0)
            throw VorbisError("Cannot decode complete Ogg Vorbis range");
        cursor += static_cast<std::size_t>(read);
    }
    return wav;
}
}  // namespace goldendict::core::audio
