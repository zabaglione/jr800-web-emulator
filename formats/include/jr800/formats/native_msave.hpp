// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "jr800/formats/jr8app.hpp"

namespace jr800::formats {

enum class NativeMsaveIssueCode : std::uint8_t {
    invalid_wav,
    unsupported_wav,
    no_signal,
    unexpected_burst_count,
    synchronization_failed,
    truncated_block,
    framing_error,
    checksum_mismatch,
    unsupported_header,
    invalid_length,
    invalid_program_range,
    ambiguous_header_byte_order,
    invalid_basic_program,
    unexpected_trailing_blocks,
};

enum class NativeMsaveByteOrder : std::uint8_t {
    little_endian,
    big_endian,
};

enum class NativeMsaveHeaderLayout : std::uint8_t {
    compact_fields,
    reserved_byte_before_fields,
};

struct NativeMsaveIssue {
    NativeMsaveIssueCode code{};
    std::size_t burst_index{};

    friend bool operator==(const NativeMsaveIssue&, const NativeMsaveIssue&) = default;
};

struct NativeMsaveFile {
    jr8app::ProgramKind kind{jr8app::ProgramKind::machine_code};
    std::string filename;
    std::uint16_t start_address{};
    std::uint16_t execution_address{};
    std::size_t source_channel{};
    NativeMsaveByteOrder header_byte_order{NativeMsaveByteOrder::little_endian};
    NativeMsaveHeaderLayout header_layout{
        NativeMsaveHeaderLayout::compact_fields
    };
    std::vector<std::uint8_t> payload;
};

struct NativeMsaveDecodeResult {
    std::optional<NativeMsaveFile> file;
    std::vector<NativeMsaveIssue> issues;
};

[[nodiscard]] NativeMsaveDecodeResult decode_native_msave_wav(
    std::span<const std::uint8_t> wav_bytes
);

[[nodiscard]] NativeMsaveDecodeResult decode_native_program_wav(
    std::span<const std::uint8_t> wav_bytes
);

[[nodiscard]] jr8app::Application native_program_application(const NativeMsaveFile& file);

// Native tape block bodies, without their additive checksums. The ROM supplies
// these to its output services; validation is shared with WAV imports.
[[nodiscard]] NativeMsaveDecodeResult decode_native_program_blocks(
    std::span<const std::vector<std::uint8_t>> blocks
);
[[nodiscard]] std::vector<std::uint8_t> encode_native_program_wav(
    const NativeMsaveFile& file
);

[[nodiscard]] std::string_view native_msave_issue_name(
    NativeMsaveIssueCode code
) noexcept;

}  // namespace jr800::formats
