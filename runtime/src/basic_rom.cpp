// SPDX-License-Identifier: MIT
#include "basic_rom.hpp"
#include "jr800/formats/sha256.hpp"
#include <stdexcept>
#include <string>

namespace jr800::runtime::basic_rom {
using core::Jr800Machine;
std::uint8_t byte(const Jr800Machine& machine, std::uint16_t address) {
    const auto result = machine.execution().inspect8(address);
    if (!result.succeeded()) throw std::runtime_error("Unknown BASIC state");
    return *result.value;
}
std::uint16_t word(const Jr800Machine& machine, std::uint16_t address) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(byte(machine, address)) << 8U)
        | byte(machine, static_cast<std::uint16_t>(address + 1U)));
}
std::vector<std::uint8_t> memory(const Jr800Machine& machine, std::uint16_t address, std::size_t length) {
    std::vector<std::uint8_t> result;
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i) result.push_back(byte(machine, static_cast<std::uint16_t>(address + i)));
    return result;
}
bool supported_rom(const Jr800Machine& machine) {
    const auto hash = formats::sha256(memory(machine, 0x8000U, 32768U));
    std::string encoded;
    for (auto value : hash) {
        encoded += "0123456789abcdef"[value >> 4U];
        encoded += "0123456789abcdef"[value & 15U];
    }
    // Full-image identities only; no ROM instructions or token tables are embedded.
    return encoded == "a650327c2df6311df04546ebb25bc355b06f97ef01c1e90f76992841ca974654"
        || encoded == "6872b585452ba4ddea5ad9103e9484adddc0d4aced04f802157849b9d0d52a17";
}
}
