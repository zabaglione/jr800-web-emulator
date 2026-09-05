// SPDX-License-Identifier: MIT
#pragma once
#include "jr800/core/jr800_machine.hpp"
#include <vector>
namespace jr800::runtime::basic_rom {
[[nodiscard]] std::uint8_t byte(const core::Jr800Machine&, std::uint16_t);
[[nodiscard]] std::uint16_t word(const core::Jr800Machine&, std::uint16_t);
[[nodiscard]] std::vector<std::uint8_t> memory(const core::Jr800Machine&, std::uint16_t, std::size_t);
[[nodiscard]] bool supported_rom(const core::Jr800Machine&);
}
