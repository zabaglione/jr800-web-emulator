// SPDX-License-Identifier: MIT

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "jr800/core/jr800_memory.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

struct RegionCase {
    std::uint16_t address;
    jr800::core::Jr800MemoryRegion expected;
};

}  // namespace

int main() {
    using jr800::core::Jr800Memory;
    using jr800::core::Jr800MemoryRegion;
    using jr800::core::Jr800MemoryStatus;
    using jr800::core::jr800_logical_rom_size;
    using jr800::core::jr800_memory_region;

    constexpr std::array region_cases{
        RegionCase{0x0000U, Jr800MemoryRegion::cpu_internal_registers},
        RegionCase{0x001FU, Jr800MemoryRegion::cpu_internal_registers},
        RegionCase{0x0020U, Jr800MemoryRegion::reserved},
        RegionCase{0x007FU, Jr800MemoryRegion::reserved},
        RegionCase{0x0080U, Jr800MemoryRegion::cpu_internal_ram},
        RegionCase{0x00FFU, Jr800MemoryRegion::cpu_internal_ram},
        RegionCase{0x0100U, Jr800MemoryRegion::reserved},
        RegionCase{0x05FFU, Jr800MemoryRegion::reserved},
        RegionCase{0x0600U, Jr800MemoryRegion::calendar_clock},
        RegionCase{0x07FFU, Jr800MemoryRegion::calendar_clock},
        RegionCase{0x0800U, Jr800MemoryRegion::reserved},
        RegionCase{0x09FFU, Jr800MemoryRegion::reserved},
        RegionCase{0x0A00U, Jr800MemoryRegion::lcd},
        RegionCase{0x0BFFU, Jr800MemoryRegion::lcd},
        RegionCase{0x0C00U, Jr800MemoryRegion::keyboard},
        RegionCase{0x0FFFU, Jr800MemoryRegion::keyboard},
        RegionCase{0x1000U, Jr800MemoryRegion::reserved},
        RegionCase{0x1FFFU, Jr800MemoryRegion::reserved},
        RegionCase{0x2000U, Jr800MemoryRegion::standard_ram},
        RegionCase{0x5FFFU, Jr800MemoryRegion::standard_ram},
        RegionCase{0x6000U, Jr800MemoryRegion::expansion_ram},
        RegionCase{0x7FFFU, Jr800MemoryRegion::expansion_ram},
        RegionCase{0x8000U, Jr800MemoryRegion::standard_rom},
        RegionCase{0xBFFFU, Jr800MemoryRegion::standard_rom},
        RegionCase{0xC000U, Jr800MemoryRegion::expansion_rom},
        RegionCase{0xEFFFU, Jr800MemoryRegion::expansion_rom},
        RegionCase{0xF000U, Jr800MemoryRegion::cpu_internal_rom},
        RegionCase{0xFFFFU, Jr800MemoryRegion::cpu_internal_rom},
    };

    bool passed = true;
    for (const auto& test : region_cases) {
        passed &= expect(
            jr800_memory_region(test.address) == test.expected,
            "memory region boundary differs"
        );
    }

    Jr800Memory memory;
    const auto unloaded = memory.read8(0x8000U);
    passed &= expect(
        unloaded.status == Jr800MemoryStatus::rom_not_loaded
            && unloaded.region == Jr800MemoryRegion::standard_rom
            && !unloaded.value.has_value(),
        "unloaded ROM returned a guessed value"
    );

    std::vector<std::uint8_t> wrong_size(jr800_logical_rom_size - 1U, 0xA5U);
    passed &= expect(
        memory.load_logical_rom(wrong_size)
                == Jr800MemoryStatus::invalid_rom_size
            && !memory.rom_loaded(),
        "invalid ROM size was accepted"
    );

    std::vector<std::uint8_t> rom(jr800_logical_rom_size);
    for (std::size_t index = 0U; index < rom.size(); ++index) {
        rom[index] = static_cast<std::uint8_t>((index * 29U + 3U) & 0xFFU);
    }
    passed &= expect(
        memory.load_logical_rom(rom) == Jr800MemoryStatus::ok
            && memory.rom_loaded(),
        "exact-size logical ROM was rejected"
    );

    for (const auto address : std::array<std::uint16_t, 6>{
             0x8000U,
             0xBFFFU,
             0xC000U,
             0xEFFFU,
             0xF000U,
             0xFFFFU,
         }) {
        const auto result = memory.read8(address);
        const auto expected = rom[static_cast<std::size_t>(address - 0x8000U)];
        passed &= expect(
            result.succeeded() && result.value == expected,
            "logical ROM read differs"
        );
    }

    const auto before_write = memory.read8(0x8000U);
    const auto rom_write = memory.write8(0x8000U, 0xFFU);
    const auto after_write = memory.read8(0x8000U);
    passed &= expect(
        rom_write.status == Jr800MemoryStatus::read_only
            && before_write.value == after_write.value,
        "ROM write changed owner-loaded data"
    );

    wrong_size.push_back(0x5AU);
    wrong_size.push_back(0x5BU);
    passed &= expect(
        memory.load_logical_rom(wrong_size)
                == Jr800MemoryStatus::invalid_rom_size
            && memory.read8(0x8000U).value == before_write.value,
        "invalid replacement changed the loaded ROM"
    );

    const auto internal_uninitialized = memory.read8(0x0080U);
    passed &= expect(
        internal_uninitialized.status == Jr800MemoryStatus::uninitialized_ram
            && !internal_uninitialized.value.has_value(),
        "internal RAM returned an invented power-on value"
    );
    passed &= expect(
        memory.write8(0x0080U, 0x42U).succeeded()
            && memory.read8(0x0080U).value == 0x42U,
        "internal RAM write/read failed"
    );

    const auto standard_uninitialized = memory.read8(0x2000U);
    passed &= expect(
        standard_uninitialized.status == Jr800MemoryStatus::uninitialized_ram
            && !standard_uninitialized.value.has_value(),
        "standard RAM returned an invented power-on value"
    );
    passed &= expect(
        memory.write8(0x5FFFU, 0xA6U).succeeded()
            && memory.read8(0x5FFFU).value == 0xA6U,
        "standard RAM write/read failed"
    );

    const auto expansion_read = memory.read8(0x6000U);
    const auto expansion_write = memory.write8(0x6000U, 0x11U);
    passed &= expect(
        expansion_read.status == Jr800MemoryStatus::unsupported_region
            && expansion_read.region == Jr800MemoryRegion::expansion_ram
            && !expansion_read.value.has_value()
            && expansion_write.status == Jr800MemoryStatus::unsupported_region,
        "unverified expansion RAM behavior was guessed"
    );

    passed &= expect(
        memory.initialize_ram(Jr800MemoryRegion::cpu_internal_ram, 0x00U)
                == Jr800MemoryStatus::ok
            && memory.read8(0x0080U).value == 0x00U
            && memory.read8(0x00FFU).value == 0x00U
            && memory.initialize_ram(Jr800MemoryRegion::standard_ram, 0xA5U)
                == Jr800MemoryStatus::ok
            && memory.read8(0x2000U).value == 0xA5U
            && memory.read8(0x5FFFU).value == 0xA5U,
        "Explicit internal or standard RAM initialization differs"
    );
    passed &= expect(
        memory.initialize_ram(Jr800MemoryRegion::expansion_ram, 0x5AU)
                == Jr800MemoryStatus::ok
            && memory.read8(0x6000U).value == 0x5AU
            && memory.read8(0x7FFFU).value == 0x5AU
            && memory.write8(0x6000U, 0x33U).succeeded()
            && memory.read8(0x6000U).value == 0x33U
            && memory.read8(0x6001U).value == 0x5AU,
        "Explicit expansion-RAM initialization or isolation differs"
    );

    for (const auto address : std::array<std::uint16_t, 4>{
             0x0000U,
             0x0600U,
             0x0A00U,
             0x0C00U,
         }) {
        const auto read = memory.read8(address);
        const auto write = memory.write8(address, 0x00U);
        passed &= expect(
            read.status == Jr800MemoryStatus::unsupported_region
                && !read.value.has_value()
                && write.status == Jr800MemoryStatus::unsupported_region,
            "unsupported device region returned a guessed behavior"
        );
    }

    return passed ? 0 : 1;
}
