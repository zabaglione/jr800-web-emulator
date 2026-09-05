// SPDX-License-Identifier: MIT
#include "jr800/formats/native_msave.hpp"
#include "wav_pcm.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace jr800::formats {
std::vector<std::uint8_t> encode_native_program_wav(const NativeMsaveFile& file) {
    static_cast<void>(native_program_application(file));
    using jr8app::ProgramKind;
    std::vector<std::vector<std::uint8_t>> blocks(1U, std::vector<std::uint8_t>(32U));
    auto& header = blocks.front();
    header[0] = file.kind == ProgramKind::machine_code ? 1U
        : file.kind == ProgramKind::basic_binary ? 2U : 3U;
    if (file.filename.size() > 16U) throw std::invalid_argument("Native name is too long");
    std::copy(file.filename.begin(), file.filename.end(), header.begin() + 1);
    if (file.kind == ProgramKind::basic_text) {
        const auto count = (file.payload.size() + 1U) / 256U + 1U;
        for (std::size_t index = 0; index < count; ++index) {
            std::vector<std::uint8_t> block(257U);
            block[0] = index + 1U == count ? 1U : 0U;
            for (std::size_t j = 0; j < 256U; ++j) {
                const auto position = index * 256U + j;
                block[j + 1U] = position < file.payload.size() ? file.payload[position]
                    : position == file.payload.size() ? 0x1AU : 0U;
            }
            blocks.push_back(std::move(block));
        }
    } else {
        const auto put = [&](std::size_t offset, std::uint16_t value) {
            header[offset] = static_cast<std::uint8_t>(value >> 8U);
            header[offset + 1U] = static_cast<std::uint8_t>(value);
        };
        // E-425/E-427: the supported ROMs emit reserved-byte, big-endian fields.
        put(18U, static_cast<std::uint16_t>(file.payload.size()));
        put(20U, file.start_address);
        if (file.kind == ProgramKind::machine_code) put(22U, file.execution_address);
        blocks.push_back(file.payload);
    }
    const auto checked = decode_native_program_blocks(blocks);
    if (!checked.file || !checked.issues.empty()) throw std::invalid_argument("Invalid native SAVE blocks");

    std::vector<std::int16_t> pcm(4800U, 0);
    double position = static_cast<double>(pcm.size());
    const auto cycle = [&](double duration) {
        position += duration / 2.0;
        pcm.resize(static_cast<std::size_t>(std::lround(position)), 12000);
        position += duration / 2.0;
        pcm.resize(static_cast<std::size_t>(std::lround(position)), -12000);
    };
    // E-031 measured cycle means. This is a synthesized transfer signal, not
    // an analog recording of the emulated machine or a speaker waveform.
    constexpr double short_cycle = 48000.0 * 465.56e-6;
    constexpr double long_cycle = 48000.0 * 881.60e-6;
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        for (unsigned i = 0; i < 4000U; ++i) cycle(48000.0 / 2122.1);
        const auto sync = index == 0U ? 40U : 20U;
        for (unsigned i = 0; i < sync; ++i) cycle(long_cycle);
        for (unsigned i = 0; i < sync; ++i) cycle(short_cycle);
        cycle(long_cycle); cycle(long_cycle);
        auto bytes = blocks[index];
        std::uint16_t sum = 0U;
        for (auto value : bytes) sum = static_cast<std::uint16_t>(sum + value);
        bytes.push_back(static_cast<std::uint8_t>(sum >> 8U));
        bytes.push_back(static_cast<std::uint8_t>(sum));
        for (auto value : bytes) {
            for (int bit = 7; bit >= 0; --bit) cycle((value & (1U << bit)) != 0U ? long_cycle : short_cycle);
            cycle(long_cycle);
        }
        pcm.insert(pcm.end(), 4800U, 0);
        position = static_cast<double>(pcm.size());
    }
    return detail::encode_pcm16_wav(pcm, 48000U);
}
}
