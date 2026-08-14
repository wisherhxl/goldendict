// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_LSA_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_LSA_FIXTURE_H_
#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace goldendict::core::test {
inline void LsaLe32(std::uint32_t value, std::string* data) {
    for (unsigned i = 0; i < 4U; ++i)
        data->push_back(static_cast<char>(value >> (i * 8U)));
}

inline void LsaAppendName(std::string_view name, std::string* data) {
    for (const unsigned char c : name) {
        data->push_back(static_cast<char>(c));
        data->push_back('\0');
    }
    data->append("\r\0\n\0", 4U);
}

inline std::string LsaVorbis() {
    vorbis_info info;
    vorbis_info_init(&info);
    if (vorbis_encode_init_vbr(&info, 1, 8000, -0.1F) != 0)
        throw std::runtime_error("cannot initialize Vorbis fixture encoder");
    vorbis_comment comment;
    vorbis_comment_init(&comment);
    vorbis_comment_add_tag(&comment, const_cast<char*>("ENCODER"),
                           const_cast<char*>("GoldenDict test"));
    vorbis_dsp_state dsp;
    vorbis_block block;
    ogg_stream_state stream;
    if (vorbis_analysis_init(&dsp, &info) != 0 ||
        vorbis_block_init(&dsp, &block) != 0 ||
        ogg_stream_init(&stream, 4242) != 0)
        throw std::runtime_error("cannot initialize Vorbis fixture stream");
    ogg_packet header;
    ogg_packet comment_header;
    ogg_packet codebooks;
    vorbis_analysis_headerout(&dsp, &comment, &header, &comment_header,
                              &codebooks);
    ogg_stream_packetin(&stream, &header);
    ogg_stream_packetin(&stream, &comment_header);
    ogg_stream_packetin(&stream, &codebooks);
    std::string encoded;
    ogg_page page;
    while (ogg_stream_flush(&stream, &page) != 0) {
        encoded.append(reinterpret_cast<char*>(page.header), page.header_len);
        encoded.append(reinterpret_cast<char*>(page.body), page.body_len);
    }
    float** samples = vorbis_analysis_buffer(&dsp, 32);
    for (int i = 0; i < 32; ++i)
        samples[0][i] = i < 16 ? 0.25F : -0.25F;
    vorbis_analysis_wrote(&dsp, 32);
    vorbis_analysis_wrote(&dsp, 0);
    bool done = false;
    while (!done && vorbis_analysis_blockout(&dsp, &block) == 1) {
        vorbis_analysis(&block, nullptr);
        vorbis_bitrate_addblock(&block);
        ogg_packet packet;
        while (vorbis_bitrate_flushpacket(&dsp, &packet) != 0) {
            ogg_stream_packetin(&stream, &packet);
            while (ogg_stream_pageout(&stream, &page) != 0) {
                encoded.append(reinterpret_cast<char*>(page.header),
                               page.header_len);
                encoded.append(reinterpret_cast<char*>(page.body),
                               page.body_len);
                if (ogg_page_eos(&page) != 0)
                    done = true;
            }
        }
    }
    ogg_stream_clear(&stream);
    vorbis_block_clear(&block);
    vorbis_dsp_clear(&dsp);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
    return encoded;
}

inline std::filesystem::path WriteLsaFixture(
    const std::filesystem::path& root,
    std::string_view filename = "fixture.lsa") {
    std::filesystem::create_directories(root);
    std::string archive(
        "L\0"
        "9\0S\0A\0\xff",
        9U);
    LsaLe32(6U, &archive);
    LsaAppendName("example.wav", &archive);
    archive.push_back(static_cast<char>(0xff));
    LsaLe32(16U, &archive);
    LsaAppendName("second.WAV", &archive);
    archive.append("\0\xff", 2U);
    LsaLe32(0U, &archive);
    archive.push_back(static_cast<char>(0xff));
    LsaLe32(16U, &archive);
    LsaAppendName("duplicate.WaV", &archive);
    archive.append("\0\xff", 2U);
    LsaLe32(0U, &archive);
    archive.push_back(static_cast<char>(0xff));
    LsaLe32(16U, &archive);
    LsaAppendName("duplicate.wav", &archive);
    archive.append("\0\xff", 2U);
    LsaLe32(0U, &archive);
    archive.push_back(static_cast<char>(0xff));
    LsaLe32(16U, &archive);
    LsaAppendName("Apple.wav", &archive);
    archive.append("\0\xff", 2U);
    LsaLe32(0U, &archive);
    archive.push_back(static_cast<char>(0xff));
    LsaLe32(16U, &archive);
    LsaAppendName("apple.WAV", &archive);
    archive.append("\0\xff", 2U);
    LsaLe32(0U, &archive);
    archive.push_back(static_cast<char>(0xff));
    LsaLe32(16U, &archive);
    archive += LsaVorbis();
    const auto path = root / filename;
    std::ofstream output(path, std::ios::binary);
    output.write(archive.data(), static_cast<std::streamsize>(archive.size()));
    if (!output)
        throw std::runtime_error("cannot write LSA fixture");
    return path;
}
}  // namespace goldendict::core::test
#endif
