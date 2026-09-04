// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "jr800/isa/instruction_metadata.hpp"

namespace jr800::disassembler {

enum class Status : std::uint8_t {
    decoded,
    unknown_opcode,
    truncated_instruction,
    end_of_input,
};

struct Result {
    Status status{Status::end_of_input};
    std::size_t consumed_bytes{};
    const isa::InstructionMetadata* instruction{};
    std::string text;
};

[[nodiscard]] Result disassemble_one(
    isa::CpuProfile profile,
    std::uint16_t address,
    std::span<const std::uint8_t> bytes
);

}  // namespace jr800::disassembler
