// SPDX-License-Identifier: MIT

#include "jr800/formats/rom_dump.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

#include "jr800/formats/sha256.hpp"

namespace jr800::formats {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{'J', '8', 'R', 'F'};
constexpr std::uint8_t kBlockFrame = 1U;
constexpr std::uint8_t kFinalFrame = 2U;
constexpr std::uint16_t kHeaderSize = 24U;
constexpr std::uint16_t kFinalBlockNumber = 0xFFFFU;
constexpr std::size_t kDigestSize = 32U;
constexpr std::uint16_t kMaximumBlockSize = 4'096U;

struct ParsedFrame {
    std::uint8_t type{};
    std::uint16_t segment_address{};
    std::uint16_t block_number{};
    std::uint16_t block_count{};
    std::uint32_t block_offset{};
    std::uint32_t total_length{};
    std::vector<std::uint8_t> payload;
};

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | static_cast<std::uint16_t>(bytes[offset + 1U])
    );
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U)
        | static_cast<std::uint32_t>(bytes[offset + 3U]);
}

std::vector<std::uint8_t> serialize_frame(
    std::uint8_t type,
    std::uint16_t segment_address,
    std::uint16_t block_number,
    std::uint16_t block_count,
    std::uint32_t block_offset,
    std::uint32_t total_length,
    std::span<const std::uint8_t> payload
) {
    if (payload.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error("ROM dump frame payload is too large");
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(kHeaderSize + payload.size() + 2U);
    bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
    bytes.push_back(rom_dump_format_version);
    bytes.push_back(type);
    append_u16(bytes, kHeaderSize);
    append_u16(bytes, segment_address);
    append_u16(bytes, block_number);
    append_u16(bytes, block_count);
    append_u16(bytes, static_cast<std::uint16_t>(payload.size()));
    append_u32(bytes, block_offset);
    append_u32(bytes, total_length);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    append_u16(bytes, crc16_ccitt(bytes));
    return bytes;
}

std::optional<ParsedFrame> parse_frame(
    std::span<const std::uint8_t> bytes,
    std::vector<RomDumpIssue>& issues
) {
    const auto declared_block = [&]() -> std::optional<std::uint16_t> {
        if (bytes.size() >= 12U) {
            const auto value = read_u16(bytes, 10U);
            if (value != kFinalBlockNumber) {
                return value;
            }
        }
        return std::nullopt;
    }();

    if (bytes.size() < kHeaderSize + 2U
        || !std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
        issues.push_back({RomDumpIssueCode::malformed_frame, declared_block});
        return std::nullopt;
    }
    if (bytes[4] != rom_dump_format_version) {
        issues.push_back({RomDumpIssueCode::unsupported_version, declared_block});
        return std::nullopt;
    }
    if (bytes[5] != kBlockFrame && bytes[5] != kFinalFrame) {
        issues.push_back({RomDumpIssueCode::malformed_frame, declared_block});
        return std::nullopt;
    }
    if (read_u16(bytes, 6U) != kHeaderSize) {
        issues.push_back({RomDumpIssueCode::malformed_frame, declared_block});
        return std::nullopt;
    }
    const auto payload_size = read_u16(bytes, 14U);
    const auto expected_size = static_cast<std::size_t>(kHeaderSize)
        + payload_size + 2U;
    if (bytes.size() != expected_size) {
        issues.push_back({RomDumpIssueCode::malformed_frame, declared_block});
        return std::nullopt;
    }
    const auto recorded_crc = read_u16(bytes, bytes.size() - 2U);
    if (crc16_ccitt(bytes.first(bytes.size() - 2U)) != recorded_crc) {
        issues.push_back({RomDumpIssueCode::crc_mismatch, declared_block});
        return std::nullopt;
    }

    ParsedFrame frame;
    frame.type = bytes[5];
    frame.segment_address = read_u16(bytes, 8U);
    frame.block_number = read_u16(bytes, 10U);
    frame.block_count = read_u16(bytes, 12U);
    frame.block_offset = read_u32(bytes, 16U);
    frame.total_length = read_u32(bytes, 20U);
    frame.payload.assign(
        bytes.begin() + static_cast<std::ptrdiff_t>(kHeaderSize),
        bytes.end() - 2
    );

    const auto segment_end = static_cast<std::uint64_t>(frame.segment_address)
        + frame.total_length;
    const auto block_end = static_cast<std::uint64_t>(frame.block_offset)
        + frame.payload.size();
    const bool common_invalid = frame.block_count == 0U
        || frame.total_length == 0U || frame.total_length > 65'536U
        || segment_end > 65'536U;
    const bool block_invalid = frame.type == kBlockFrame
        && (frame.block_number >= frame.block_count || frame.payload.empty()
            || frame.payload.size() > kMaximumBlockSize
            || block_end > frame.total_length);
    const bool final_invalid = frame.type == kFinalFrame
        && (frame.block_number != kFinalBlockNumber || frame.block_offset != 0U
            || frame.payload.size() != kDigestSize);
    if (common_invalid || block_invalid || final_invalid) {
        issues.push_back({RomDumpIssueCode::malformed_frame, declared_block});
        return std::nullopt;
    }
    return frame;
}

bool metadata_matches(const ParsedFrame& left, const ParsedFrame& right) {
    return left.segment_address == right.segment_address
        && left.block_count == right.block_count
        && left.total_length == right.total_length;
}

}  // namespace

std::uint16_t crc16_ccitt(std::span<const std::uint8_t> bytes) noexcept {
    std::uint16_t crc = 0xFFFFU;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint16_t>(byte) << 8U;
        for (unsigned int bit = 0; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) != 0U
                ? static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U)
                : static_cast<std::uint16_t>(crc << 1U);
        }
    }
    return crc;
}

std::vector<std::vector<std::uint8_t>> make_rom_dump_frames(
    std::uint16_t segment_address,
    std::span<const std::uint8_t> payload,
    std::uint16_t block_size
) {
    if (payload.empty()) {
        throw std::invalid_argument("ROM dump payload must not be empty");
    }
    if (block_size == 0U || block_size > kMaximumBlockSize) {
        throw std::invalid_argument("ROM dump block size must be between 1 and 4096");
    }
    if (payload.size() > 65'536U
        || static_cast<std::uint64_t>(segment_address) + payload.size() > 65'536U) {
        throw std::length_error("ROM dump segment exceeds the 16-bit address space");
    }
    const auto block_count_value =
        (payload.size() + static_cast<std::size_t>(block_size) - 1U) / block_size;
    if (block_count_value > kFinalBlockNumber) {
        throw std::length_error("ROM dump has too many blocks");
    }
    const auto block_count = static_cast<std::uint16_t>(block_count_value);
    const auto total_length = static_cast<std::uint32_t>(payload.size());

    std::vector<std::vector<std::uint8_t>> frames;
    frames.reserve(static_cast<std::size_t>(block_count) + 1U);
    for (std::uint16_t block_number = 0U; block_number < block_count; ++block_number) {
        const auto offset = static_cast<std::size_t>(block_number) * block_size;
        const auto length = std::min<std::size_t>(block_size, payload.size() - offset);
        frames.push_back(serialize_frame(
            kBlockFrame,
            segment_address,
            block_number,
            block_count,
            static_cast<std::uint32_t>(offset),
            total_length,
            payload.subspan(offset, length)
        ));
    }
    const auto digest = sha256(payload);
    frames.push_back(serialize_frame(
        kFinalFrame,
        segment_address,
        kFinalBlockNumber,
        block_count,
        0U,
        total_length,
        digest
    ));
    return frames;
}

RomDumpRecovery recover_rom_dump(
    std::span<const std::vector<std::uint8_t>> frames
) {
    RomDumpRecovery recovery;
    std::optional<ParsedFrame> reference;
    std::optional<ParsedFrame> final_frame;
    std::map<std::uint16_t, ParsedFrame> blocks;

    for (const auto& bytes : frames) {
        auto parsed = parse_frame(bytes, recovery.issues);
        if (!parsed.has_value()) {
            continue;
        }
        if (!reference.has_value()) {
            reference = *parsed;
            recovery.segment_address = parsed->segment_address;
            recovery.expected_length = parsed->total_length;
        } else if (!metadata_matches(*reference, *parsed)) {
            recovery.issues.push_back({
                RomDumpIssueCode::inconsistent_metadata,
                parsed->type == kBlockFrame
                    ? std::optional<std::uint16_t>{parsed->block_number}
                    : std::nullopt,
            });
            continue;
        }

        if (parsed->type == kFinalFrame) {
            if (final_frame.has_value()) {
                recovery.issues.push_back({RomDumpIssueCode::duplicate_final, std::nullopt});
            } else {
                final_frame = std::move(*parsed);
            }
            continue;
        }
        const auto block_number = parsed->block_number;
        const auto [iterator, inserted] = blocks.emplace(block_number, std::move(*parsed));
        static_cast<void>(iterator);
        if (!inserted) {
            recovery.issues.push_back({
                RomDumpIssueCode::duplicate_block,
                block_number,
            });
        }
    }

    if (!reference.has_value()) {
        recovery.issues.push_back({RomDumpIssueCode::missing_final, std::nullopt});
        return recovery;
    }

    recovery.payload.assign(reference->total_length, 0U);
    std::vector<bool> covered(reference->total_length, false);
    for (std::uint16_t block_number = 0U; block_number < reference->block_count;
         ++block_number) {
        const auto block = blocks.find(block_number);
        if (block == blocks.end()) {
            recovery.issues.push_back({RomDumpIssueCode::missing_block, block_number});
            continue;
        }
        const auto& frame = block->second;
        bool overlaps = false;
        for (std::size_t index = 0; index < frame.payload.size(); ++index) {
            const auto destination = static_cast<std::size_t>(frame.block_offset) + index;
            if (covered[destination]) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) {
            recovery.issues.push_back({
                RomDumpIssueCode::inconsistent_metadata,
                block_number,
            });
            continue;
        }
        std::copy(
            frame.payload.begin(),
            frame.payload.end(),
            recovery.payload.begin() + static_cast<std::ptrdiff_t>(frame.block_offset)
        );
        std::fill(
            covered.begin() + static_cast<std::ptrdiff_t>(frame.block_offset),
            covered.begin()
                + static_cast<std::ptrdiff_t>(frame.block_offset + frame.payload.size()),
            true
        );
    }

    const bool fully_covered = std::all_of(
        covered.begin(),
        covered.end(),
        [](bool value) { return value; }
    );
    const bool coverage_failure_reported = std::any_of(
        recovery.issues.begin(),
        recovery.issues.end(),
        [](const RomDumpIssue& issue) {
            return issue.code == RomDumpIssueCode::missing_block
                || issue.code == RomDumpIssueCode::inconsistent_metadata;
        }
    );
    if (!fully_covered && !coverage_failure_reported) {
        recovery.issues.push_back({
            RomDumpIssueCode::inconsistent_metadata,
            std::nullopt,
        });
    }

    if (!final_frame.has_value()) {
        recovery.issues.push_back({RomDumpIssueCode::missing_final, std::nullopt});
    } else if (fully_covered) {
        const auto digest = sha256(recovery.payload);
        if (!std::equal(digest.begin(), digest.end(), final_frame->payload.begin())) {
            recovery.issues.push_back({RomDumpIssueCode::hash_mismatch, std::nullopt});
        }
    }
    recovery.complete = recovery.issues.empty() && fully_covered;
    return recovery;
}

std::string_view rom_dump_issue_name(RomDumpIssueCode code) noexcept {
    switch (code) {
    case RomDumpIssueCode::malformed_frame:
        return "malformed-frame";
    case RomDumpIssueCode::unsupported_version:
        return "unsupported-version";
    case RomDumpIssueCode::crc_mismatch:
        return "crc-mismatch";
    case RomDumpIssueCode::inconsistent_metadata:
        return "inconsistent-metadata";
    case RomDumpIssueCode::duplicate_block:
        return "duplicate-block";
    case RomDumpIssueCode::missing_block:
        return "missing-block";
    case RomDumpIssueCode::missing_final:
        return "missing-final";
    case RomDumpIssueCode::duplicate_final:
        return "duplicate-final";
    case RomDumpIssueCode::hash_mismatch:
        return "hash-mismatch";
    }
    return "unknown";
}

}  // namespace jr800::formats
