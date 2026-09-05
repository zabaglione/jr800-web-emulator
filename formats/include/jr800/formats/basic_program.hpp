// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace jr800::formats {
// Validate the host-supported native BASIC encodings, without interpreting tokens.
// Invalid input throws linked::Error. Text preserves JR-800 bytes and CR endings.
[[nodiscard]] std::vector<std::uint16_t> basic_text_line_numbers(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::vector<std::uint16_t> basic_binary_line_numbers(std::span<const std::uint8_t> bytes);
}
