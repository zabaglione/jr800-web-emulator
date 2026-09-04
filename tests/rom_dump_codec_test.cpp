// SPDX-License-Identifier: MIT

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "jr800/formats/fsk_wav.hpp"
#include "jr800/formats/rom_dump.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool has_issue(
    const jr800::formats::RomDumpRecovery& recovery,
    jr800::formats::RomDumpIssueCode code,
    std::uint16_t block_number
) {
    return std::any_of(
        recovery.issues.begin(),
        recovery.issues.end(),
        [&](const auto& issue) {
            return issue.code == code && issue.block_number.has_value()
                && *issue.block_number == block_number;
        }
    );
}

bool has_issue(
    const jr800::formats::RomDumpRecovery& recovery,
    jr800::formats::RomDumpIssueCode code
) {
    return std::any_of(
        recovery.issues.begin(),
        recovery.issues.end(),
        [&](const auto& issue) { return issue.code == code; }
    );
}

std::uint16_t read_le16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
}

std::uint32_t read_le32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

void write_le16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void write_le32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void write_be16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>(value & 0xFFU);
}

void refresh_frame_crc(std::vector<std::uint8_t>& frame) {
    write_be16(
        frame,
        frame.size() - 2U,
        jr800::formats::crc16_ccitt(
            std::span<const std::uint8_t>{frame}.first(frame.size() - 2U)
        )
    );
}

std::vector<std::int16_t> read_pcm(std::span<const std::uint8_t> wav) {
    const auto data_size = read_le32(wav, 40U);
    std::vector<std::int16_t> samples;
    samples.reserve(data_size / 2U);
    for (std::size_t offset = 44U; offset < 44U + data_size; offset += 2U) {
        samples.push_back(static_cast<std::int16_t>(read_le16(wav, offset)));
    }
    return samples;
}

std::vector<std::uint8_t> replace_pcm(
    std::span<const std::uint8_t> wav,
    std::span<const std::int16_t> samples
) {
    std::vector<std::uint8_t> result(wav.begin(), wav.begin() + 44);
    result.resize(44U + samples.size() * 2U);
    write_le32(result, 4U, static_cast<std::uint32_t>(36U + samples.size() * 2U));
    write_le32(result, 40U, static_cast<std::uint32_t>(samples.size() * 2U));
    for (std::size_t index = 0; index < samples.size(); ++index) {
        write_le16(
            result,
            44U + index * 2U,
            static_cast<std::uint16_t>(samples[index])
        );
    }
    return result;
}

std::vector<std::uint8_t> offset_noise_amplitude_wav(
    std::span<const std::uint8_t> wav
) {
    auto samples = read_pcm(wav);
    std::uint32_t random = 0x12345678U;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        random = random * 1'664'525U + 1'013'904'223U;
        const int noise = (static_cast<int>((random >> 24U) & 0xFFU) - 128) / 2;
        const double gain = index < samples.size() / 2U ? 0.5 : 0.85;
        const auto adjusted = static_cast<long>(std::lround(
            static_cast<double>(samples[index]) * gain + 1'500.0
            + static_cast<double>(noise)
        ));
        samples[index] = static_cast<std::int16_t>(std::clamp<long>(
            adjusted,
            std::numeric_limits<std::int16_t>::min(),
            std::numeric_limits<std::int16_t>::max()
        ));
    }
    return replace_pcm(wav, samples);
}

std::vector<std::uint8_t> clipped_wav(std::span<const std::uint8_t> wav) {
    auto samples = read_pcm(wav);
    for (auto& sample : samples) {
        sample = static_cast<std::int16_t>(std::clamp<int>(sample, -4'000, 4'000));
    }
    return replace_pcm(wav, samples);
}

std::vector<std::uint8_t> stretch_wav(
    std::span<const std::uint8_t> wav,
    double factor
) {
    const auto input = read_pcm(wav);
    const auto output_size = static_cast<std::size_t>(std::lround(
        static_cast<double>(input.size()) * factor
    ));
    std::vector<std::int16_t> output;
    output.reserve(output_size);
    for (std::size_t index = 0; index < output_size; ++index) {
        const double source = static_cast<double>(index) / factor;
        const auto left = std::min<std::size_t>(
            static_cast<std::size_t>(source),
            input.size() - 1U
        );
        const auto right = std::min(left + 1U, input.size() - 1U);
        const double fraction = source - static_cast<double>(left);
        output.push_back(static_cast<std::int16_t>(std::lround(
            static_cast<double>(input[left]) * (1.0 - fraction)
            + static_cast<double>(input[right]) * fraction
        )));
    }
    return replace_pcm(wav, output);
}

std::vector<std::uint8_t> remove_second_burst(
    std::span<const std::uint8_t> wav,
    std::span<const std::vector<std::uint8_t>> frames
) {
    auto samples = read_pcm(wav);
    constexpr std::size_t samples_per_bit = 40U;
    constexpr std::size_t silence_samples = 1'920U;
    constexpr std::size_t packet_overhead = 24U + 4U + 4U;
    const auto first_burst_samples = (packet_overhead + frames[0].size())
        * 10U * samples_per_bit;
    const auto second_begin = silence_samples + first_burst_samples + silence_samples;
    const auto second_samples = (packet_overhead + frames[1].size())
        * 10U * samples_per_bit;
    std::fill(
        samples.begin() + static_cast<std::ptrdiff_t>(second_begin),
        samples.begin() + static_cast<std::ptrdiff_t>(second_begin + second_samples),
        0
    );
    return replace_pcm(wav, samples);
}

void overwrite_uart_byte(
    std::vector<std::int16_t>& samples,
    std::size_t first_sample,
    std::uint8_t value
) {
    constexpr std::size_t samples_per_bit = 40U;
    constexpr std::int16_t amplitude = 12'000;
    constexpr double pi = 3.14159265358979323846;
    const auto write_bit = [&](std::size_t bit_index, bool bit) {
        const double cycles = bit ? 2.0 : 1.0;
        for (std::size_t index = 0; index < samples_per_bit; ++index) {
            const double position = (static_cast<double>(index) + 0.5)
                / static_cast<double>(samples_per_bit);
            samples[first_sample + bit_index * samples_per_bit + index]
                = static_cast<std::int16_t>(std::lround(
                    static_cast<double>(amplitude)
                    * std::sin(2.0 * pi * cycles * position)
                ));
        }
    };

    write_bit(0U, false);
    for (unsigned int bit = 0U; bit < 8U; ++bit) {
        write_bit(1U + bit, ((value >> bit) & 1U) != 0U);
    }
    write_bit(9U, true);
}

std::vector<std::uint8_t> corrupt_first_sync_byte(
    std::span<const std::uint8_t> wav
) {
    auto samples = read_pcm(wav);
    constexpr std::size_t silence_samples = 1'920U;
    constexpr std::size_t preamble_samples = 24U * 10U * 40U;
    overwrite_uart_byte(samples, silence_samples + preamble_samples, 'K');
    return replace_pcm(wav, samples);
}

bool has_fsk_issue(
    const jr800::formats::FskWavDecodeResult& result,
    jr800::formats::FskWavIssueCode code
) {
    return std::any_of(
        result.issues.begin(),
        result.issues.end(),
        [&](const auto& issue) { return issue.code == code; }
    );
}

bool expect_audio_recovery(
    std::span<const std::uint8_t> wav,
    std::span<const std::vector<std::uint8_t>> expected_frames,
    std::span<const std::uint8_t> expected_payload,
    std::string_view label
) {
    const auto decoded = jr800::formats::decode_fsk_wav(wav);
    bool passed = true;
    if (!decoded.issues.empty()) {
        std::cerr << label << " FSK issues:";
        for (const auto& issue : decoded.issues) {
            std::cerr << ' ' << jr800::formats::fsk_wav_issue_name(issue.code)
                      << '@' << issue.burst_index;
        }
        std::cerr << '\n';
    }
    passed &= expect(decoded.issues.empty(), std::string(label) + " should decode without FSK issues");
    passed &= expect(
        decoded.frames.size() == expected_frames.size()
            && std::equal(
                decoded.frames.begin(),
                decoded.frames.end(),
                expected_frames.begin()
            ),
        std::string(label) + " frame bytes differ"
    );
    const auto recovery = jr800::formats::recover_rom_dump(decoded.frames);
    passed &= expect(recovery.complete, std::string(label) + " ROM recovery must complete");
    passed &= expect(
        recovery.payload.size() == expected_payload.size()
            && std::equal(
                recovery.payload.begin(),
                recovery.payload.end(),
                expected_payload.begin()
            ),
        std::string(label) + " payload differs"
    );
    return passed;
}

}  // namespace

int main() {
    using jr800::formats::RomDumpIssueCode;

    bool passed = true;
    constexpr std::array<std::uint8_t, 9> crc_vector{
        '1', '2', '3', '4', '5', '6', '7', '8', '9',
    };
    passed &= expect(
        jr800::formats::crc16_ccitt(crc_vector) == 0x29B1U,
        "CRC-16/CCITT-FALSE check vector differs"
    );

    std::vector<std::uint8_t> payload(700U);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<std::uint8_t>((index * 37U + 11U) & 0xFFU);
    }
    const auto frames = jr800::formats::make_rom_dump_frames(0x8000U, payload, 256U);
    passed &= expect(frames.size() == 4U, "three block frames and one final frame expected");

    const auto recovered = jr800::formats::recover_rom_dump(frames);
    passed &= expect(recovered.complete, "clean ROM frames must recover completely");
    passed &= expect(recovered.segment_address == 0x8000U, "segment address differs");
    passed &= expect(recovered.payload == payload, "clean ROM payload differs");

    auto corrupt_frames = frames;
    corrupt_frames[1][30] ^= 0x80U;
    const auto corrupt = jr800::formats::recover_rom_dump(corrupt_frames);
    passed &= expect(!corrupt.complete, "CRC-corrupt block must not complete");
    passed &= expect(has_issue(corrupt, RomDumpIssueCode::crc_mismatch, 1U), "CRC issue missing");
    passed &= expect(has_issue(corrupt, RomDumpIssueCode::missing_block, 1U), "missing block issue missing");

    auto missing_frames = frames;
    missing_frames.erase(missing_frames.begin() + 1);
    const auto missing = jr800::formats::recover_rom_dump(missing_frames);
    passed &= expect(!missing.complete, "missing block must not complete");
    passed &= expect(has_issue(missing, RomDumpIssueCode::missing_block, 1U), "gap not reported");

    auto coverage_gap_frames = frames;
    auto& shortened = coverage_gap_frames[1];
    shortened.erase(shortened.end() - 3);
    write_be16(shortened, 14U, 255U);
    refresh_frame_crc(shortened);
    const auto coverage_gap = jr800::formats::recover_rom_dump(coverage_gap_frames);
    passed &= expect(!coverage_gap.complete, "uncovered byte must not complete");
    passed &= expect(
        has_issue(coverage_gap, RomDumpIssueCode::inconsistent_metadata),
        "uncovered byte must report inconsistent metadata"
    );

    auto no_final_frames = frames;
    no_final_frames.pop_back();
    const auto no_final = jr800::formats::recover_rom_dump(no_final_frames);
    passed &= expect(!no_final.complete, "missing final frame must not complete");
    passed &= expect(has_issue(no_final, RomDumpIssueCode::missing_final), "missing final not reported");

    auto bad_hash_frames = frames;
    bad_hash_frames.back()[24U] ^= 0x01U;
    refresh_frame_crc(bad_hash_frames.back());
    const auto bad_hash = jr800::formats::recover_rom_dump(bad_hash_frames);
    passed &= expect(!bad_hash.complete, "wrong final hash must not complete");
    passed &= expect(
        has_issue(bad_hash, RomDumpIssueCode::hash_mismatch),
        "wrong final hash not reported"
    );

    const auto wav = jr800::formats::encode_fsk_wav(frames);
    passed &= expect_audio_recovery(wav, frames, payload, "clean WAV");

    const auto distorted = offset_noise_amplitude_wav(wav);
    passed &= expect_audio_recovery(
        distorted,
        frames,
        payload,
        "DC/amplitude/noise WAV"
    );

    const auto clipped = clipped_wav(wav);
    passed &= expect_audio_recovery(clipped, frames, payload, "clipped WAV");

    const auto stretched = stretch_wav(wav, 1.03);
    passed &= expect_audio_recovery(stretched, frames, payload, "3 percent slow WAV");

    const auto bad_sync_wav = corrupt_first_sync_byte(wav);
    const auto bad_sync = jr800::formats::decode_fsk_wav(bad_sync_wav);
    passed &= expect(
        has_fsk_issue(
            bad_sync,
            jr800::formats::FskWavIssueCode::synchronization_failed
        ),
        "altered packet sync must fail explicitly"
    );
    passed &= expect(
        bad_sync.frames.size() == frames.size() - 1U,
        "altered packet sync must not emit the affected frame"
    );

    const auto dropout = remove_second_burst(wav, frames);
    const auto dropout_decode = jr800::formats::decode_fsk_wav(dropout);
    const auto dropout_recovery = jr800::formats::recover_rom_dump(dropout_decode.frames);
    passed &= expect(!dropout_recovery.complete, "dropout must not be guessed as complete");
    passed &= expect(
        has_issue(dropout_recovery, RomDumpIssueCode::missing_block, 1U),
        "dropout must report the missing block"
    );

    constexpr std::array<std::uint8_t, 4> invalid_wav{'N', 'O', 'P', 'E'};
    const auto invalid = jr800::formats::decode_fsk_wav(invalid_wav);
    passed &= expect(
        !invalid.issues.empty()
            && invalid.issues.front().code == jr800::formats::FskWavIssueCode::invalid_wav,
        "invalid WAV must fail visibly"
    );

    return passed ? 0 : 1;
}
