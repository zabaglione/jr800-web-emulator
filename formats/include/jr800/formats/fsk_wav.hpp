// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace jr800::formats {

struct FskWavParameters {
    std::uint32_t sample_rate{48'000U};
    std::uint32_t baud{1'200U};
    std::int16_t amplitude{12'000};
    std::uint16_t preamble_bytes{24U};
    std::uint16_t inter_frame_silence_ms{40U};
};

enum class FskWavIssueCode : std::uint8_t {
    invalid_wav,
    unsupported_wav,
    no_signal,
    synchronization_failed,
    truncated_packet,
    framing_error,
    invalid_packet_length,
};

struct FskWavIssue {
    FskWavIssueCode code{};
    std::size_t burst_index{};

    friend bool operator==(const FskWavIssue&, const FskWavIssue&) = default;
};

struct FskWavDecodeResult {
    std::vector<std::vector<std::uint8_t>> frames;
    std::vector<FskWavIssue> issues;
};

[[nodiscard]] std::vector<std::uint8_t> encode_fsk_wav(
    std::span<const std::vector<std::uint8_t>> frames,
    const FskWavParameters& parameters = {}
);

[[nodiscard]] FskWavDecodeResult decode_fsk_wav(
    std::span<const std::uint8_t> wav_bytes,
    std::uint32_t baud = 1'200U
);

[[nodiscard]] std::string_view fsk_wav_issue_name(FskWavIssueCode code) noexcept;

}  // namespace jr800::formats
