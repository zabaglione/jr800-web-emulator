// SPDX-License-Identifier: MIT

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

#include "jr800/disassembler/disassembler.hpp"
#include "jr800/isa/instruction_metadata.hpp"

namespace {

using jr800::disassembler::Status;
using jr800::isa::CpuProfile;

bool require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

template<std::size_t Size>
bool expect_decoded(
    const std::array<std::uint8_t, Size>& bytes,
    std::uint16_t address,
    std::string_view expected
) {
    const auto result = jr800::disassembler::disassemble_one(
        CpuProfile::hd6301v1,
        address,
        bytes
    );
    return require(result.status == Status::decoded, "instruction was not decoded")
        && require(result.consumed_bytes == bytes.size(), "decoded length differs")
        && require(result.instruction != nullptr, "metadata is missing")
        && require(result.text == expected, "instruction text differs");
}

bool test_addressing_modes() {
    bool passed = true;
    passed &= expect_decoded(std::array<std::uint8_t, 1>{0x01U}, 0x1000U, "NOP");
    passed &= expect_decoded(
        std::array<std::uint8_t, 2>{0x86U, 0x7FU},
        0x1000U,
        "LDAA #$7F"
    );
    passed &= expect_decoded(
        std::array<std::uint8_t, 3>{0xCEU, 0x12U, 0x34U},
        0x1000U,
        "LDX #$1234"
    );
    passed &= expect_decoded(
        std::array<std::uint8_t, 2>{0x97U, 0x80U},
        0x1000U,
        "STAA $80"
    );
    passed &= expect_decoded(
        std::array<std::uint8_t, 3>{0x7EU, 0x12U, 0x34U},
        0x1000U,
        "JMP $1234"
    );
    passed &= expect_decoded(
        std::array<std::uint8_t, 2>{0x20U, 0xFEU},
        0x1000U,
        "BRA $1000"
    );
    passed &= expect_decoded(
        std::array<std::uint8_t, 2>{0x20U, 0x80U},
        0x0080U,
        "BRA $0002"
    );
    passed &= expect_decoded(
        std::array<std::uint8_t, 2>{0xA6U, 0x20U},
        0x1000U,
        "LDAA $20,X"
    );
    passed &= expect_decoded(
        std::array<std::uint8_t, 3>{0x71U, 0xF0U, 0x20U},
        0x1000U,
        "AIM #$F0, $20"
    );
    passed &= expect_decoded(
        std::array<std::uint8_t, 3>{0x61U, 0xF0U, 0x20U},
        0x1000U,
        "AIM #$F0, $20,X"
    );
    passed &= expect_decoded(
        std::array<std::uint8_t, 2>{0x20U, 0x00U},
        0xFFFFU,
        "BRA $0001"
    );
    return passed;
}

bool test_fail_closed_results() {
    bool passed = true;

    const std::array<std::uint8_t, 1> unknown{0x02U};
    const auto unknown_result = jr800::disassembler::disassemble_one(
        CpuProfile::hd6301v1,
        0U,
        unknown
    );
    passed &= require(
        unknown_result.status == Status::unknown_opcode
            && unknown_result.consumed_bytes == 1U
            && unknown_result.instruction == nullptr
            && unknown_result.text == ".byte $02",
        "unknown opcode result differs"
    );

    const std::array<std::uint8_t, 2> truncated{0xCEU, 0x12U};
    const auto truncated_result = jr800::disassembler::disassemble_one(
        CpuProfile::hd6301v1,
        0U,
        truncated
    );
    passed &= require(
        truncated_result.status == Status::truncated_instruction
            && truncated_result.consumed_bytes == 1U
            && truncated_result.instruction != nullptr
            && truncated_result.text == ".byte $CE",
        "truncated instruction result differs"
    );

    const std::array<std::uint8_t, 1> hd6301_only{0x1AU};
    const auto wrong_profile = jr800::disassembler::disassemble_one(
        CpuProfile::mc6801,
        0U,
        hd6301_only
    );
    passed &= require(
        wrong_profile.status == Status::unknown_opcode
            && wrong_profile.text == ".byte $1A",
        "profile-specific opcode was inferred"
    );

    const auto end = jr800::disassembler::disassemble_one(
        CpuProfile::hd6301v1,
        0U,
        std::span<const std::uint8_t>{}
    );
    passed &= require(
        end.status == Status::end_of_input
            && end.consumed_bytes == 0U
            && end.instruction == nullptr
            && end.text.empty(),
        "empty input result differs"
    );
    return passed;
}

bool test_every_reviewed_encoding() {
    bool passed = true;
    for (const auto& instruction : jr800::isa::all_instructions()) {
        if (!jr800::isa::instruction_applies_to(
                instruction,
                CpuProfile::hd6301v1
            )) {
            continue;
        }
        std::array<std::uint8_t, 3> bytes{};
        bytes[0] = instruction.opcode;
        const auto result = jr800::disassembler::disassemble_one(
            CpuProfile::hd6301v1,
            0x2000U,
            std::span{bytes}.first(instruction.instruction_length)
        );
        passed &= require(
            result.status == Status::decoded
                && result.consumed_bytes == instruction.instruction_length
                && result.instruction == &instruction
                && !result.text.empty(),
            "reviewed encoding did not round-trip through the disassembler"
        );
    }
    return passed;
}

}  // namespace

int main() {
    const bool passed = test_addressing_modes()
        && test_fail_closed_results()
        && test_every_reviewed_encoding();
    return passed ? 0 : 1;
}
