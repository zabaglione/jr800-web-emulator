// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace jr800::formats {

inline constexpr std::uint8_t rom_dump_format_version = 1U;
inline constexpr std::uint16_t default_rom_dump_block_size = 256U;

enum class RomDumpIssueCode : std::uint8_t {
    malformed_frame,
    unsupported_version,
    crc_mismatch,
    inconsistent_metadata,
    duplicate_block,
    missing_block,
    missing_final,
    duplicate_final,
    hash_mismatch,
};

struct RomDumpIssue {
    RomDumpIssueCode code{};
    std::optional<std::uint16_t> block_number;

    friend bool operator==(const RomDumpIssue&, const RomDumpIssue&) = default;
};

struct RomDumpRecovery {
    std::uint16_t segment_address{};
    std::uint32_t expected_length{};
    std::vector<std::uint8_t> payload;
    std::vector<RomDumpIssue> issues;
    bool complete{};
};

[[nodiscard]] std::uint16_t crc16_ccitt(
    std::span<const std::uint8_t> bytes
) noexcept;

[[nodiscard]] std::vector<std::vector<std::uint8_t>> make_rom_dump_frames(
    std::uint16_t segment_address,
    std::span<const std::uint8_t> payload,
    std::uint16_t block_size = default_rom_dump_block_size
);

[[nodiscard]] RomDumpRecovery recover_rom_dump(
    std::span<const std::vector<std::uint8_t>> frames
);

[[nodiscard]] std::string_view rom_dump_issue_name(RomDumpIssueCode code) noexcept;

}  // namespace jr800::formats
