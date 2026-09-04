// SPDX-License-Identifier: MIT

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "jr800/formats/native_msave.hpp"

namespace {

constexpr std::uint32_t kSampleRate = 48'000U;
constexpr std::int16_t kAmplitude = 12'000;
constexpr std::size_t kShortHalfPeriod = 11U;
constexpr std::size_t kLongHalfPeriod = 21U;

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

void append_le16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_le32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    append_le16(bytes, static_cast<std::uint16_t>(value & 0xFFFFU));
    append_le16(bytes, static_cast<std::uint16_t>((value >> 16U) & 0xFFFFU));
}

void write_be16(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint16_t value
) {
    bytes[offset] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>(value & 0xFFU);
}

void write_le16(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint16_t value
) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

std::uint16_t additive_sum(std::span<const std::uint8_t> bytes) {
    std::uint32_t sum = 0U;
    for (const auto byte : bytes) {
        sum += byte;
    }
    return static_cast<std::uint16_t>(sum & 0xFFFFU);
}

void append_cycle(std::vector<std::int16_t>& samples, bool long_period) {
    const auto half_period = long_period ? kLongHalfPeriod : kShortHalfPeriod;
    samples.insert(samples.end(), half_period, kAmplitude);
    samples.insert(samples.end(), half_period, static_cast<std::int16_t>(-kAmplitude));
}

void append_cycles(
    std::vector<std::int16_t>& samples,
    bool long_period,
    std::size_t count
) {
    for (std::size_t index = 0U; index < count; ++index) {
        append_cycle(samples, long_period);
    }
}

void append_byte(std::vector<std::int16_t>& samples, std::uint8_t value) {
    for (int bit = 7; bit >= 0; --bit) {
        append_cycle(samples, ((value >> bit) & 1U) != 0U);
    }
    append_cycle(samples, true);
}

void append_block(
    std::vector<std::int16_t>& samples,
    std::span<const std::uint8_t> bytes,
    std::size_t long_sync_cycles,
    std::size_t short_sync_cycles
) {
    append_cycles(samples, false, 4'000U);
    append_cycles(samples, true, long_sync_cycles);
    append_cycles(samples, false, short_sync_cycles);
    append_cycles(samples, true, 2U);
    for (const auto byte : bytes) {
        append_byte(samples, byte);
    }
}

std::vector<std::uint8_t> make_header(
    std::size_t payload_size,
    bool corrupt_checksum,
    std::uint16_t start_address,
    bool big_endian = false,
    std::uint16_t execution_address = 0x8123U,
    bool reserved_byte_before_fields = false
) {
    std::vector<std::uint8_t> header(34U, 0U);
    header[0] = 0x01U;
    constexpr std::string_view filename = "SYNTHETIC";
    std::copy(filename.begin(), filename.end(), header.begin() + 1);
    const auto write_word = big_endian ? write_be16 : write_le16;
    const std::size_t offset = reserved_byte_before_fields ? 18U : 17U;
    write_word(header, offset, static_cast<std::uint16_t>(payload_size));
    write_word(header, offset + 2U, start_address);
    write_word(header, offset + 4U, execution_address);
    auto checksum = additive_sum(std::span<const std::uint8_t>{header}.first(32U));
    if (corrupt_checksum) {
        checksum = static_cast<std::uint16_t>(checksum + 1U);
    }
    write_be16(header, 32U, checksum);
    return header;
}

std::vector<std::uint8_t> make_data(
    std::span<const std::uint8_t> payload,
    bool corrupt_checksum
) {
    std::vector<std::uint8_t> block(payload.begin(), payload.end());
    auto checksum = additive_sum(payload);
    if (corrupt_checksum) {
        checksum = static_cast<std::uint16_t>(checksum + 1U);
    }
    block.push_back(static_cast<std::uint8_t>((checksum >> 8U) & 0xFFU));
    block.push_back(static_cast<std::uint8_t>(checksum & 0xFFU));
    return block;
}

std::vector<std::uint8_t> make_wav(
    std::span<const std::uint8_t> payload,
    std::size_t signal_channel,
    bool corrupt_header_checksum = false,
    bool corrupt_data_checksum = false,
    std::uint16_t start_address = 0x8000U,
    bool continuous_blocks = false,
    bool big_endian = false,
    std::uint16_t execution_address = 0x8123U,
    bool reserved_byte_before_fields = false
) {
    std::vector<std::int16_t> signal(kSampleRate / 10U, 0);
    const auto header = make_header(
        payload.size(),
        corrupt_header_checksum,
        start_address,
        big_endian,
        execution_address,
        reserved_byte_before_fields
    );
    append_block(signal, header, 40U, 40U);
    signal.insert(
        signal.end(),
        continuous_blocks ? 720U : kSampleRate / 500U,
        continuous_blocks ? static_cast<std::int16_t>(-kAmplitude) : 0
    );
    const auto data = make_data(payload, corrupt_data_checksum);
    append_block(signal, data, 20U, 20U);
    signal.insert(signal.end(), kSampleRate / 10U, 0);

    std::vector<std::int16_t> interleaved;
    interleaved.reserve(signal.size() * 2U);
    std::uint32_t noise = 0x12345678U;
    for (const auto sample : signal) {
        noise = noise * 1'664'525U + 1'013'904'223U;
        const auto quiet = static_cast<std::int16_t>(
            static_cast<int>((noise >> 24U) & 0xFFU) - 128
        );
        if (signal_channel == 0U) {
            interleaved.push_back(sample);
            interleaved.push_back(quiet);
        } else {
            interleaved.push_back(quiet);
            interleaved.push_back(sample);
        }
    }

    const auto data_size = static_cast<std::uint32_t>(interleaved.size() * 2U);
    std::vector<std::uint8_t> wav;
    wav.reserve(44U + data_size);
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    append_le32(wav, 36U + data_size);
    wav.insert(wav.end(), {'W', 'A', 'V', 'E'});
    wav.insert(wav.end(), {'f', 'm', 't', ' '});
    append_le32(wav, 16U);
    append_le16(wav, 1U);
    append_le16(wav, 2U);
    append_le32(wav, kSampleRate);
    append_le32(wav, kSampleRate * 4U);
    append_le16(wav, 4U);
    append_le16(wav, 16U);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    append_le32(wav, data_size);
    for (const auto sample : interleaved) {
        append_le16(wav, static_cast<std::uint16_t>(sample));
    }
    return wav;
}

bool has_issue(
    const jr800::formats::NativeMsaveDecodeResult& result,
    jr800::formats::NativeMsaveIssueCode code
) {
    return std::any_of(
        result.issues.begin(),
        result.issues.end(),
        [&](const auto& issue) { return issue.code == code; }
    );
}

}  // namespace

int main() {
    using jr800::formats::NativeMsaveIssueCode;

    std::vector<std::uint8_t> payload(256U);
    for (std::size_t index = 0U; index < payload.size(); ++index) {
        payload[index] = static_cast<std::uint8_t>((index * 37U + 11U) & 0xFFU);
    }

    bool passed = true;
    for (std::size_t signal_channel = 0U; signal_channel < 2U; ++signal_channel) {
        const auto decoded = jr800::formats::decode_native_msave_wav(
            make_wav(payload, signal_channel)
        );
        passed &= expect(decoded.issues.empty(), "clean recording has issues");
        passed &= expect(decoded.file.has_value(), "clean recording did not decode");
        if (decoded.file.has_value()) {
            passed &= expect(decoded.file->filename == "SYNTHETIC", "filename differs");
            passed &= expect(decoded.file->start_address == 0x8000U, "start differs");
            passed &= expect(
                decoded.file->execution_address == 0x8123U,
                "execution address differs"
            );
            passed &= expect(
                decoded.file->source_channel == signal_channel,
                "selected channel differs"
            );
            passed &= expect(decoded.file->payload == payload, "payload differs");
        }
    }

    auto distributed_payload = payload;
    distributed_payload.push_back(0xA5U);
    distributed_payload.push_back(0x5AU);
    const auto continuous_big_endian =
        jr800::formats::decode_native_program_wav(make_wav(
            distributed_payload,
            0U,
            false,
            false,
            0x2800U,
            true,
            true,
            0x2800U,
            true
        ));
    passed &= expect(
        continuous_big_endian.issues.empty(),
        "continuous big-endian recording has issues"
    );
    passed &= expect(
        continuous_big_endian.file.has_value(),
        "continuous big-endian recording did not decode"
    );
    if (continuous_big_endian.file.has_value()) {
        passed &= expect(
            continuous_big_endian.file->start_address == 0x2800U,
            "continuous big-endian start differs"
        );
        passed &= expect(
            continuous_big_endian.file->execution_address == 0x2800U,
            "continuous big-endian execution address differs"
        );
        passed &= expect(
            continuous_big_endian.file->header_byte_order
                == jr800::formats::NativeMsaveByteOrder::big_endian,
            "continuous big-endian header order differs"
        );
        passed &= expect(
            continuous_big_endian.file->header_layout
                == jr800::formats::NativeMsaveHeaderLayout::
                    reserved_byte_before_fields,
            "continuous big-endian header layout differs"
        );
        passed &= expect(
            continuous_big_endian.file->payload == distributed_payload,
            "continuous big-endian payload differs"
        );
    }

    const auto continuous_native = jr800::formats::decode_native_msave_wav(
        make_wav(payload, 0U, false, false, 0x8000U, true)
    );
    passed &= expect(
        continuous_native.issues.empty() && continuous_native.file.has_value(),
        "continuous native recording did not decode"
    );

    const auto corrupt = jr800::formats::decode_native_msave_wav(
        make_wav(payload, 0U, true)
    );
    passed &= expect(!corrupt.file.has_value(), "bad checksum produced a file");
    passed &= expect(
        has_issue(corrupt, NativeMsaveIssueCode::checksum_mismatch),
        "bad header checksum was not reported"
    );

    const auto corrupt_data = jr800::formats::decode_native_msave_wav(
        make_wav(payload, 0U, false, true)
    );
    passed &= expect(!corrupt_data.file.has_value(), "bad data checksum produced a file");
    passed &= expect(
        has_issue(corrupt_data, NativeMsaveIssueCode::checksum_mismatch),
        "bad data checksum was not reported"
    );

    std::vector<std::uint8_t> large_payload(32'768U);
    for (std::size_t index = 0U; index < large_payload.size(); ++index) {
        large_payload[index] = static_cast<std::uint8_t>(
            (index * 23U + 5U) & 0xFFU
        );
    }
    const auto large = jr800::formats::decode_native_msave_wav(
        make_wav(large_payload, 0U)
    );
    passed &= expect(large.issues.empty(), "32 KiB recording has issues");
    passed &= expect(large.file.has_value(), "32 KiB recording did not decode");
    if (large.file.has_value()) {
        passed &= expect(
            large.file->payload == large_payload,
            "32 KiB payload differs"
        );
    }

    auto oversized_payload = large_payload;
    oversized_payload.push_back(0x5AU);
    const auto oversized = jr800::formats::decode_native_msave_wav(
        make_wav(oversized_payload, 0U, false, false, 0U)
    );
    passed &= expect(!oversized.file.has_value(), "unsupported length produced a file");
    passed &= expect(
        has_issue(oversized, NativeMsaveIssueCode::invalid_length),
        "unsupported length was not reported"
    );

    constexpr std::array<std::uint8_t, 4> invalid{'N', 'O', 'P', 'E'};
    const auto invalid_result = jr800::formats::decode_native_msave_wav(invalid);
    passed &= expect(!invalid_result.file.has_value(), "invalid WAV produced a file");
    passed &= expect(
        has_issue(invalid_result, NativeMsaveIssueCode::invalid_wav),
        "invalid WAV was not reported"
    );

    return passed ? 0 : 1;
}
