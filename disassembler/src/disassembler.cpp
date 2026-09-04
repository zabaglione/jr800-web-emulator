// SPDX-License-Identifier: MIT

#include "jr800/disassembler/disassembler.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace jr800::disassembler {
namespace {

std::string hex_value(std::uint16_t value, int width) {
    std::ostringstream stream;
    stream << '$' << std::uppercase << std::hex << std::setfill('0')
           << std::setw(width) << value;
    return stream.str();
}

std::uint16_t read_u16_be(std::span<const std::uint8_t> bytes) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[0]) << 8U)
        | static_cast<std::uint16_t>(bytes[1])
    );
}

std::string format_instruction(
    const isa::InstructionMetadata& instruction,
    std::uint16_t address,
    std::span<const std::uint8_t> bytes
) {
    std::string text{instruction.mnemonic};
    const auto byte = [&](std::size_t index) {
        return hex_value(bytes[index], 2);
    };
    const auto word = [&](std::size_t index) {
        return hex_value(read_u16_be(bytes.subspan(index, 2U)), 4);
    };

    switch (instruction.addressing_mode) {
    case isa::AddressingMode::implied:
        break;
    case isa::AddressingMode::immediate8:
        text += " #" + byte(1U);
        break;
    case isa::AddressingMode::immediate16:
        text += " #" + word(1U);
        break;
    case isa::AddressingMode::direct8:
        text += " " + byte(1U);
        break;
    case isa::AddressingMode::extended16:
        text += " " + word(1U);
        break;
    case isa::AddressingMode::relative8: {
        const auto next = static_cast<std::uint16_t>(
            address + instruction.instruction_length
        );
        const auto displacement = bytes[1] < 0x80U
            ? static_cast<int>(bytes[1])
            : static_cast<int>(bytes[1]) - 0x100;
        const auto target = static_cast<std::uint16_t>(next + displacement);
        text += " " + hex_value(target, 4);
        break;
    }
    case isa::AddressingMode::indexed8:
        text += " " + byte(1U) + ",X";
        break;
    case isa::AddressingMode::immediate8_direct8:
        text += " #" + byte(1U) + ", " + byte(2U);
        break;
    case isa::AddressingMode::immediate8_indexed8:
        text += " #" + byte(1U) + ", " + byte(2U) + ",X";
        break;
    }
    return text;
}

}  // namespace

Result disassemble_one(
    isa::CpuProfile profile,
    std::uint16_t address,
    std::span<const std::uint8_t> bytes
) {
    if (bytes.empty()) {
        return {};
    }

    const auto* instruction = isa::decode_instruction(profile, bytes.front());
    if (instruction == nullptr) {
        return Result{
            Status::unknown_opcode,
            1U,
            nullptr,
            ".byte " + hex_value(bytes.front(), 2),
        };
    }
    if (bytes.size() < instruction->instruction_length) {
        return Result{
            Status::truncated_instruction,
            1U,
            instruction,
            ".byte " + hex_value(bytes.front(), 2),
        };
    }

    return Result{
        Status::decoded,
        instruction->instruction_length,
        instruction,
        format_instruction(*instruction, address, bytes),
    };
}

}  // namespace jr800::disassembler
