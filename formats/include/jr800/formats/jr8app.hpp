// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "jr800/formats/linked_error.hpp"
#include "jr800/formats/sha256.hpp"

namespace jr800::formats::jr8app {

inline constexpr std::uint16_t format_major_version = 1;
inline constexpr std::uint16_t format_minor_version = 0;

enum class SegmentKind : std::uint8_t {
    data = 1,
    zero_fill = 2,
};

struct Segment {
    SegmentKind kind{SegmentKind::data};
    std::uint16_t address{};
    std::uint32_t logical_size{};
    std::vector<std::uint8_t> data;

    bool operator==(const Segment&) const = default;
};

struct Application {
    std::string target_profile;
    std::uint16_t entry_point{};
    Sha256Digest integrity_sha256{};
    std::vector<Segment> segments;

    bool operator==(const Application&) const = default;
};

[[nodiscard]] Sha256Digest compute_integrity(const Application& application);
[[nodiscard]] std::vector<std::uint8_t> write(const Application& application);
[[nodiscard]] Application read(std::span<const std::uint8_t> bytes);

}  // namespace jr800::formats::jr8app
