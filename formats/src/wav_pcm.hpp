// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace jr800::formats::detail {

enum class WavPcmError : std::uint8_t {
    none,
    invalid_wav,
    unsupported_wav,
};

struct WavPcm16 {
    std::uint32_t sample_rate{};
    std::uint16_t channels{};
    std::vector<std::int16_t> interleaved_samples;
};

struct WavPcmParseResult {
    std::optional<WavPcm16> wav;
    WavPcmError error{WavPcmError::none};
};

[[nodiscard]] std::vector<std::uint8_t> encode_pcm16_wav(
    std::span<const std::int16_t> samples, std::uint32_t sample_rate
);

[[nodiscard]] WavPcmParseResult parse_pcm16_wav(
    std::span<const std::uint8_t> bytes
);

[[nodiscard]] std::vector<double> centered_channel(
    const WavPcm16& wav,
    std::size_t channel_index
);

}  // namespace jr800::formats::detail
