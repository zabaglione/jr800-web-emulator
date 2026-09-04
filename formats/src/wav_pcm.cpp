// SPDX-License-Identifier: MIT

#include "wav_pcm.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>
#include <utility>

namespace jr800::formats::detail {
namespace {

std::uint16_t read_le16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U
        );
}

std::uint32_t read_le32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

bool matches(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::array<char, 4> expected
) {
    return offset + expected.size() <= bytes.size()
        && std::equal(expected.begin(), expected.end(), bytes.begin() + offset);
}

}  // namespace

WavPcmParseResult parse_pcm16_wav(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 12U || !matches(bytes, 0U, {'R', 'I', 'F', 'F'})
        || !matches(bytes, 8U, {'W', 'A', 'V', 'E'})) {
        return {std::nullopt, WavPcmError::invalid_wav};
    }

    const auto declared_riff_size = static_cast<std::size_t>(read_le32(bytes, 4U));
    if (declared_riff_size < 4U || declared_riff_size > bytes.size() - 8U) {
        return {std::nullopt, WavPcmError::invalid_wav};
    }
    const auto riff_bytes = bytes.first(declared_riff_size + 8U);

    std::optional<std::uint16_t> channels;
    std::optional<std::uint32_t> sample_rate;
    std::optional<std::span<const std::uint8_t>> data;
    bool format_supported = false;
    std::size_t offset = 12U;
    while (offset + 8U <= riff_bytes.size()) {
        const auto chunk_size = static_cast<std::size_t>(
            read_le32(riff_bytes, offset + 4U)
        );
        const auto payload_offset = offset + 8U;
        if (chunk_size > riff_bytes.size() - payload_offset) {
            return {std::nullopt, WavPcmError::invalid_wav};
        }
        if (matches(riff_bytes, offset, {'f', 'm', 't', ' '})) {
            if (chunk_size >= 16U) {
                const auto format = read_le16(riff_bytes, payload_offset);
                channels = read_le16(riff_bytes, payload_offset + 2U);
                sample_rate = read_le32(riff_bytes, payload_offset + 4U);
                const auto byte_rate = read_le32(riff_bytes, payload_offset + 8U);
                const auto block_alignment = read_le16(
                    riff_bytes,
                    payload_offset + 12U
                );
                const auto bits = read_le16(riff_bytes, payload_offset + 14U);
                const auto expected_alignment = static_cast<std::uint32_t>(*channels)
                    * 2U;
                const auto expected_byte_rate = static_cast<std::uint64_t>(
                    *sample_rate
                ) * expected_alignment;
                format_supported = format == 1U
                    && (*channels == 1U || *channels == 2U)
                    && *sample_rate != 0U && bits == 16U
                    && block_alignment == expected_alignment
                    && expected_byte_rate
                        <= std::numeric_limits<std::uint32_t>::max()
                    && byte_rate == expected_byte_rate;
            }
        } else if (matches(riff_bytes, offset, {'d', 'a', 't', 'a'})) {
            if (data.has_value()) {
                return {std::nullopt, WavPcmError::invalid_wav};
            }
            data = riff_bytes.subspan(payload_offset, chunk_size);
        }
        offset = payload_offset + chunk_size + (chunk_size & 1U);
    }

    if (!format_supported || !channels.has_value() || !sample_rate.has_value()) {
        return {std::nullopt, WavPcmError::unsupported_wav};
    }
    const auto block_alignment = static_cast<std::size_t>(*channels) * 2U;
    if (!data.has_value() || data->empty()
        || data->size() % block_alignment != 0U) {
        return {std::nullopt, WavPcmError::invalid_wav};
    }

    WavPcm16 wav;
    wav.sample_rate = *sample_rate;
    wav.channels = *channels;
    wav.interleaved_samples.reserve(data->size() / 2U);
    for (std::size_t index = 0; index < data->size(); index += 2U) {
        wav.interleaved_samples.push_back(static_cast<std::int16_t>(
            read_le16(*data, index)
        ));
    }
    return {std::move(wav), WavPcmError::none};
}

std::vector<double> centered_channel(
    const WavPcm16& wav,
    std::size_t channel_index
) {
    if (channel_index >= wav.channels) {
        return {};
    }
    std::vector<double> samples;
    samples.reserve(wav.interleaved_samples.size() / wav.channels);
    for (std::size_t index = channel_index;
         index < wav.interleaved_samples.size();
         index += wav.channels) {
        samples.push_back(static_cast<double>(wav.interleaved_samples[index]));
    }
    if (samples.empty()) {
        return samples;
    }
    const double mean = std::accumulate(samples.begin(), samples.end(), 0.0)
        / static_cast<double>(samples.size());
    for (auto& sample : samples) {
        sample -= mean;
    }
    return samples;
}

}  // namespace jr800::formats::detail
