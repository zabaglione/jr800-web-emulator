// SPDX-License-Identifier: MIT

#include "jr800/formats/fsk_wav.hpp"

#include "wav_pcm.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace jr800::formats {
namespace {

constexpr std::array<std::uint8_t, 4> kPacketSync{'J', '8', 'F', 'S'};
constexpr std::size_t kMaximumPacketSize = 1U * 1024U * 1024U;
constexpr double kPi = 3.14159265358979323846;

struct WavSamples {
    std::uint32_t sample_rate{};
    std::vector<double> samples;
};

struct SampleRange {
    std::size_t begin{};
    std::size_t end{};
};

struct DecodedByte {
    std::uint8_t value{};
    bool framing_valid{};
};

struct Alignment {
    double first_sample{};
    double samples_per_bit{};
    int score{std::numeric_limits<int>::min()};
};



std::uint32_t read_be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U)
        | static_cast<std::uint32_t>(bytes[offset + 3U]);
}

void append_be32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}


void append_uart_byte(
    std::vector<std::int16_t>& samples,
    std::uint8_t value,
    std::size_t samples_per_bit,
    std::int16_t amplitude
) {
    const auto append_bit = [&](bool bit) {
        const double cycles = bit ? 2.0 : 1.0;
        for (std::size_t index = 0; index < samples_per_bit; ++index) {
            const double position = (static_cast<double>(index) + 0.5)
                / static_cast<double>(samples_per_bit);
            const auto sample = static_cast<long>(std::lround(
                static_cast<double>(amplitude)
                * std::sin(2.0 * kPi * cycles * position)
            ));
            samples.push_back(static_cast<std::int16_t>(sample));
        }
    };

    append_bit(false);
    for (unsigned int bit = 0; bit < 8U; ++bit) {
        append_bit(((value >> bit) & 1U) != 0U);
    }
    append_bit(true);
}

std::vector<std::uint8_t> make_packet(
    std::span<const std::uint8_t> frame,
    std::uint16_t preamble_bytes
) {
    if (frame.empty()) {
        throw std::invalid_argument("FSK frame must not be empty");
    }
    if (frame.size() > kMaximumPacketSize) {
        throw std::length_error("FSK packet is too large");
    }
    std::vector<std::uint8_t> packet(preamble_bytes, 0x55U);
    packet.insert(packet.end(), kPacketSync.begin(), kPacketSync.end());
    append_be32(packet, static_cast<std::uint32_t>(frame.size()));
    packet.insert(packet.end(), frame.begin(), frame.end());
    return packet;
}

std::optional<WavSamples> parse_wav(
    std::span<const std::uint8_t> bytes,
    FskWavDecodeResult& result
) {
    const auto parsed = detail::parse_pcm16_wav(bytes);
    if (!parsed.wav.has_value()) {
        result.issues.push_back({
            parsed.error == detail::WavPcmError::unsupported_wav
                ? FskWavIssueCode::unsupported_wav
                : FskWavIssueCode::invalid_wav,
            0U,
        });
        return std::nullopt;
    }
    if (parsed.wav->channels != 1U) {
        result.issues.push_back({FskWavIssueCode::unsupported_wav, 0U});
        return std::nullopt;
    }
    return WavSamples{
        parsed.wav->sample_rate,
        detail::centered_channel(*parsed.wav, 0U),
    };
}

std::vector<SampleRange> find_bursts(
    std::span<const double> samples,
    double nominal_samples_per_bit
) {
    const auto peak_iterator = std::max_element(
        samples.begin(),
        samples.end(),
        [](double left, double right) { return std::abs(left) < std::abs(right); }
    );
    if (peak_iterator == samples.end()) {
        return {};
    }
    const double peak = std::abs(*peak_iterator);
    if (peak < 32.0) {
        return {};
    }
    const double threshold = std::max(32.0, peak * 0.06);
    const auto gap_limit = std::max<std::size_t>(
        8U,
        static_cast<std::size_t>(std::lround(nominal_samples_per_bit * 3.0))
    );

    std::vector<SampleRange> ranges;
    std::optional<std::size_t> begin;
    std::size_t last_active = 0U;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if (std::abs(samples[index]) >= threshold) {
            if (!begin.has_value()) {
                begin = index;
            }
            last_active = index;
        } else if (begin.has_value() && index - last_active > gap_limit) {
            ranges.push_back({*begin, last_active + 1U});
            begin.reset();
        }
    }
    if (begin.has_value()) {
        ranges.push_back({*begin, last_active + 1U});
    }
    return ranges;
}

double goertzel_power(std::span<const double> samples, unsigned int cycles) {
    if (samples.size() < 4U) {
        return 0.0;
    }
    const double coefficient = 2.0 * std::cos(
        2.0 * kPi * static_cast<double>(cycles)
        / static_cast<double>(samples.size())
    );
    double previous = 0.0;
    double previous_previous = 0.0;
    for (const auto sample : samples) {
        const double current = sample + coefficient * previous - previous_previous;
        previous_previous = previous;
        previous = current;
    }
    return previous_previous * previous_previous + previous * previous
        - coefficient * previous * previous_previous;
}

std::optional<bool> decode_bit(
    std::span<const double> samples,
    double first_sample,
    double samples_per_bit,
    std::size_t bit_index
) {
    const auto begin_value = first_sample
        + static_cast<double>(bit_index) * samples_per_bit;
    const auto end_value = begin_value + samples_per_bit;
    if (begin_value < 0.0 || end_value > static_cast<double>(samples.size())) {
        return std::nullopt;
    }
    const auto begin = static_cast<std::size_t>(std::lround(begin_value));
    const auto end = static_cast<std::size_t>(std::lround(end_value));
    if (end <= begin + 3U || end > samples.size()) {
        return std::nullopt;
    }
    const auto window = samples.subspan(begin, end - begin);
    return goertzel_power(window, 2U) > goertzel_power(window, 1U);
}

std::optional<DecodedByte> decode_byte(
    std::span<const double> samples,
    double first_sample,
    double samples_per_bit,
    std::size_t byte_index
) {
    const auto first_bit = byte_index * 10U;
    const auto start = decode_bit(samples, first_sample, samples_per_bit, first_bit);
    if (!start.has_value()) {
        return std::nullopt;
    }
    std::uint8_t value = 0U;
    for (unsigned int bit = 0; bit < 8U; ++bit) {
        const auto decoded = decode_bit(
            samples,
            first_sample,
            samples_per_bit,
            first_bit + 1U + bit
        );
        if (!decoded.has_value()) {
            return std::nullopt;
        }
        if (*decoded) {
            value = static_cast<std::uint8_t>(value | (1U << bit));
        }
    }
    const auto stop = decode_bit(samples, first_sample, samples_per_bit, first_bit + 9U);
    if (!stop.has_value()) {
        return std::nullopt;
    }
    return DecodedByte{value, !*start && *stop};
}

std::vector<std::uint8_t> expected_prefix(std::uint16_t preamble_bytes) {
    std::vector<std::uint8_t> prefix(preamble_bytes, 0x55U);
    prefix.insert(prefix.end(), kPacketSync.begin(), kPacketSync.end());
    return prefix;
}

int score_alignment(
    std::span<const double> samples,
    double first_sample,
    double samples_per_bit,
    std::span<const std::uint8_t> expected
) {
    int score = 0;
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto decoded = decode_byte(samples, first_sample, samples_per_bit, index);
        if (!decoded.has_value()) {
            return std::numeric_limits<int>::min();
        }
        score += decoded->framing_valid ? 4 : -8;
        const auto difference = static_cast<std::uint8_t>(decoded->value ^ expected[index]);
        for (unsigned int bit = 0; bit < 8U; ++bit) {
            score += ((difference >> bit) & 1U) == 0U ? 1 : -1;
        }
    }
    return score;
}

Alignment find_alignment(
    std::span<const double> samples,
    SampleRange burst,
    double nominal_samples_per_bit,
    std::span<const std::uint8_t> prefix
) {
    Alignment best;
    const auto search = [&](double scale_begin, double scale_end, double scale_step,
                            double offset_begin, double offset_end, double offset_step) {
        for (double scale = scale_begin; scale <= scale_end + 1e-9; scale += scale_step) {
            const double samples_per_bit = nominal_samples_per_bit * scale;
            for (double offset = offset_begin; offset <= offset_end + 1e-9;
                 offset += offset_step) {
                const double first_sample = static_cast<double>(burst.begin) + offset;
                const int score = score_alignment(
                    samples,
                    first_sample,
                    samples_per_bit,
                    prefix
                );
                if (score > best.score) {
                    best = {first_sample, samples_per_bit, score};
                }
            }
        }
    };

    search(
        0.95,
        1.05,
        0.005,
        -nominal_samples_per_bit * 0.35,
        nominal_samples_per_bit * 0.35,
        2.0
    );
    if (best.score == std::numeric_limits<int>::min()) {
        return best;
    }
    const double best_scale = best.samples_per_bit / nominal_samples_per_bit;
    const double best_offset = best.first_sample - static_cast<double>(burst.begin);
    search(
        best_scale - 0.004,
        best_scale + 0.004,
        0.0005,
        best_offset - 2.0,
        best_offset + 2.0,
        0.5
    );
    return best;
}

std::optional<std::vector<std::uint8_t>> decode_burst(
    std::span<const double> samples,
    SampleRange burst,
    double nominal_samples_per_bit,
    std::size_t burst_index,
    FskWavDecodeResult& result
) {
    constexpr std::uint16_t preamble_bytes = 24U;
    const auto prefix = expected_prefix(preamble_bytes);
    const auto padding = std::min<std::size_t>(
        samples.size() - burst.end,
        static_cast<std::size_t>(std::ceil(nominal_samples_per_bit))
    );
    const auto bounded_samples = samples.first(burst.end + padding);
    const auto alignment = find_alignment(
        bounded_samples,
        burst,
        nominal_samples_per_bit,
        prefix
    );
    const int maximum_score = static_cast<int>(prefix.size()) * 12;
    if (alignment.score < maximum_score * 3 / 4) {
        result.issues.push_back({FskWavIssueCode::synchronization_failed, burst_index});
        return std::nullopt;
    }

    bool framing_valid = true;
    const auto read_byte_at = [&](std::size_t index) -> std::optional<std::uint8_t> {
        const auto decoded = decode_byte(
            bounded_samples,
            alignment.first_sample,
            alignment.samples_per_bit,
            index
        );
        if (!decoded.has_value()) {
            return std::nullopt;
        }
        framing_valid = framing_valid && decoded->framing_valid;
        return decoded->value;
    };

    for (std::size_t index = 0; index < kPacketSync.size(); ++index) {
        const auto value = read_byte_at(
            static_cast<std::size_t>(preamble_bytes) + index
        );
        if (!value.has_value()) {
            result.issues.push_back({FskWavIssueCode::truncated_packet, burst_index});
            return std::nullopt;
        }
        if (*value != kPacketSync[index]) {
            result.issues.push_back({
                FskWavIssueCode::synchronization_failed,
                burst_index,
            });
            return std::nullopt;
        }
    }
    if (!framing_valid) {
        result.issues.push_back({FskWavIssueCode::framing_error, burst_index});
        return std::nullopt;
    }

    const std::size_t length_offset = prefix.size();
    std::array<std::uint8_t, 4> length_bytes{};
    for (std::size_t index = 0; index < length_bytes.size(); ++index) {
        const auto value = read_byte_at(length_offset + index);
        if (!value.has_value()) {
            result.issues.push_back({FskWavIssueCode::truncated_packet, burst_index});
            return std::nullopt;
        }
        length_bytes[index] = *value;
    }
    if (!framing_valid) {
        result.issues.push_back({FskWavIssueCode::framing_error, burst_index});
        return std::nullopt;
    }
    const auto frame_length = static_cast<std::size_t>(read_be32(length_bytes, 0U));
    if (frame_length == 0U || frame_length > kMaximumPacketSize) {
        result.issues.push_back({FskWavIssueCode::invalid_packet_length, burst_index});
        return std::nullopt;
    }

    std::vector<std::uint8_t> frame;
    frame.reserve(frame_length);
    const std::size_t frame_offset = length_offset + length_bytes.size();
    for (std::size_t index = 0; index < frame_length; ++index) {
        const auto value = read_byte_at(frame_offset + index);
        if (!value.has_value()) {
            result.issues.push_back({FskWavIssueCode::truncated_packet, burst_index});
            return std::nullopt;
        }
        if (!framing_valid) {
            result.issues.push_back({FskWavIssueCode::framing_error, burst_index});
            return std::nullopt;
        }
        frame.push_back(*value);
    }
    return frame;
}

}  // namespace

std::vector<std::uint8_t> encode_fsk_wav(
    std::span<const std::vector<std::uint8_t>> frames,
    const FskWavParameters& parameters
) {
    if (frames.empty()) {
        throw std::invalid_argument("at least one FSK frame is required");
    }
    if (parameters.sample_rate == 0U || parameters.baud == 0U
        || parameters.sample_rate % parameters.baud != 0U) {
        throw std::invalid_argument("FSK sample rate must be divisible by baud");
    }
    const auto samples_per_bit = static_cast<std::size_t>(
        parameters.sample_rate / parameters.baud
    );
    if (samples_per_bit < 8U || samples_per_bit > 4'096U
        || parameters.sample_rate > std::numeric_limits<std::uint32_t>::max() / 2U) {
        throw std::invalid_argument("unsupported FSK sample rate");
    }
    if (parameters.amplitude <= 0 || parameters.preamble_bytes != 24U
        || parameters.inter_frame_silence_ms < 10U) {
        throw std::invalid_argument("unsupported FSK encoding parameters");
    }
    const auto silence_samples_value =
        (static_cast<std::uint64_t>(parameters.sample_rate)
         * parameters.inter_frame_silence_ms)
        / 1'000U;
    constexpr auto maximum_sample_count =
        (static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())
         - 36U)
        / 2U;
    if (silence_samples_value > maximum_sample_count) {
        throw std::length_error("WAV output is too large");
    }
    const auto silence_samples = static_cast<std::size_t>(silence_samples_value);

    std::vector<std::vector<std::uint8_t>> packets;
    packets.reserve(frames.size());
    std::uint64_t total_samples = silence_samples_value;
    for (const auto& frame : frames) {
        auto packet = make_packet(frame, parameters.preamble_bytes);
        const auto burst_samples = static_cast<std::uint64_t>(packet.size())
            * 10U * samples_per_bit;
        if (burst_samples > maximum_sample_count - total_samples
            || silence_samples_value
                > maximum_sample_count - total_samples - burst_samples) {
            throw std::length_error("WAV output is too large");
        }
        total_samples += burst_samples + silence_samples_value;
        packets.push_back(std::move(packet));
    }

    std::vector<std::int16_t> pcm(silence_samples, 0);
    pcm.reserve(static_cast<std::size_t>(total_samples));
    for (const auto& packet : packets) {
        for (const auto byte : packet) {
            append_uart_byte(pcm, byte, samples_per_bit, parameters.amplitude);
        }
        pcm.insert(pcm.end(), silence_samples, 0);
    }
    return detail::encode_pcm16_wav(pcm, parameters.sample_rate);
}

FskWavDecodeResult decode_fsk_wav(
    std::span<const std::uint8_t> wav_bytes,
    std::uint32_t baud
) {
    FskWavDecodeResult result;
    if (baud == 0U) {
        result.issues.push_back({FskWavIssueCode::unsupported_wav, 0U});
        return result;
    }
    const auto wav = parse_wav(wav_bytes, result);
    if (!wav.has_value()) {
        return result;
    }
    const double nominal_samples_per_bit = static_cast<double>(wav->sample_rate)
        / static_cast<double>(baud);
    if (nominal_samples_per_bit < 8.0 || nominal_samples_per_bit > 4'096.0) {
        result.issues.push_back({FskWavIssueCode::unsupported_wav, 0U});
        return result;
    }
    const auto bursts = find_bursts(wav->samples, nominal_samples_per_bit);
    if (bursts.empty()) {
        result.issues.push_back({FskWavIssueCode::no_signal, 0U});
        return result;
    }
    for (std::size_t index = 0; index < bursts.size(); ++index) {
        auto frame = decode_burst(
            wav->samples,
            bursts[index],
            nominal_samples_per_bit,
            index,
            result
        );
        if (frame.has_value()) {
            result.frames.push_back(std::move(*frame));
        }
    }
    return result;
}

std::string_view fsk_wav_issue_name(FskWavIssueCode code) noexcept {
    switch (code) {
    case FskWavIssueCode::invalid_wav:
        return "invalid-wav";
    case FskWavIssueCode::unsupported_wav:
        return "unsupported-wav";
    case FskWavIssueCode::no_signal:
        return "no-signal";
    case FskWavIssueCode::synchronization_failed:
        return "synchronization-failed";
    case FskWavIssueCode::truncated_packet:
        return "truncated-packet";
    case FskWavIssueCode::framing_error:
        return "framing-error";
    case FskWavIssueCode::invalid_packet_length:
        return "invalid-packet-length";
    }
    return "unknown";
}

}  // namespace jr800::formats
