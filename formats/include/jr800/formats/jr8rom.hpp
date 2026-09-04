// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "jr800/formats/linked_error.hpp"
#include "jr800/formats/sha256.hpp"

namespace jr800::formats::jr8rom {

inline constexpr std::uint16_t format_major_version = 1;
inline constexpr std::uint16_t format_minor_version = 0;
inline constexpr std::size_t maximum_segment_size = 65'536U;
inline constexpr std::size_t maximum_segment_count = 65'535U;
inline constexpr std::size_t maximum_encoded_size = 52U
    + maximum_segment_count * 6U + maximum_segment_size;

struct Segment {
    std::uint16_t address{};
    std::vector<std::uint8_t> data;

    bool operator==(const Segment&) const = default;
};

struct Image {
    Sha256Digest integrity_sha256{};
    std::vector<Segment> segments;

    bool operator==(const Image&) const = default;
};

[[nodiscard]] Sha256Digest compute_integrity(const Image& image);
[[nodiscard]] std::vector<std::uint8_t> write(const Image& image);
[[nodiscard]] Image read(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::optional<std::vector<std::uint8_t>> extract_range(
    const Image& image,
    std::uint16_t address,
    std::size_t size
);

}  // namespace jr800::formats::jr8rom
