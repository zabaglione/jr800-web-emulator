// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "jr800/formats/linked_error.hpp"
#include "jr800/formats/sha256.hpp"

namespace jr800::formats::jr8app {

inline constexpr std::uint16_t format_major_version = 1;
inline constexpr std::uint16_t format_minor_version = 0;

enum class ProgramKind : std::uint8_t {
    machine_code = 1,
    basic_text = 2,
    basic_binary = 3,
};

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
    ProgramKind kind{ProgramKind::machine_code};
    std::vector<std::uint8_t> name;
    std::vector<std::uint8_t> basic_data;

    bool operator==(const Application&) const = default;
};

[[nodiscard]] std::string_view program_kind_name(ProgramKind kind) noexcept;
[[nodiscard]] bool entry_point_is_loaded(const Application& application) noexcept;

[[nodiscard]] Sha256Digest compute_integrity(const Application& application);
[[nodiscard]] std::vector<std::uint8_t> write(const Application& application);
[[nodiscard]] Application read(std::span<const std::uint8_t> bytes);

}  // namespace jr800::formats::jr8app
