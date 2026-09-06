// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace jr800::core {

inline constexpr std::uint16_t jr800_logical_rom_base = 0x8000U;
inline constexpr std::size_t jr800_logical_rom_size = 32'768U;

enum class Jr800MemoryRegion : std::uint8_t {
    cpu_internal_registers,
    reserved,
    cpu_internal_ram,
    calendar_clock,
    lcd,
    keyboard,
    standard_ram,
    expansion_ram,
    standard_rom,
    expansion_rom,
    cpu_internal_rom,
};

[[nodiscard]] constexpr Jr800MemoryRegion jr800_memory_region(
    std::uint16_t address
) noexcept {
    if (address <= 0x001FU) {
        return Jr800MemoryRegion::cpu_internal_registers;
    }
    if (address <= 0x007FU) {
        return Jr800MemoryRegion::reserved;
    }
    if (address <= 0x00FFU) {
        return Jr800MemoryRegion::cpu_internal_ram;
    }
    if (address <= 0x05FFU) {
        return Jr800MemoryRegion::reserved;
    }
    if (address <= 0x07FFU) {
        return Jr800MemoryRegion::calendar_clock;
    }
    if (address <= 0x09FFU) {
        return Jr800MemoryRegion::reserved;
    }
    if (address <= 0x0BFFU) {
        return Jr800MemoryRegion::lcd;
    }
    if (address <= 0x0FFFU) {
        return Jr800MemoryRegion::keyboard;
    }
    if (address <= 0x1FFFU) {
        return Jr800MemoryRegion::reserved;
    }
    if (address <= 0x5FFFU) {
        return Jr800MemoryRegion::standard_ram;
    }
    if (address <= 0x7FFFU) {
        return Jr800MemoryRegion::expansion_ram;
    }
    if (address <= 0xBFFFU) {
        return Jr800MemoryRegion::standard_rom;
    }
    if (address <= 0xEFFFU) {
        return Jr800MemoryRegion::expansion_rom;
    }
    return Jr800MemoryRegion::cpu_internal_rom;
}

enum class Jr800MemoryStatus : std::uint8_t {
    ok,
    invalid_rom_size,
    rom_not_loaded,
    uninitialized_ram,
    unsupported_region,
    read_only,
};

struct Jr800MemoryReadResult {
    Jr800MemoryStatus status{Jr800MemoryStatus::unsupported_region};
    Jr800MemoryRegion region{Jr800MemoryRegion::reserved};
    std::optional<std::uint8_t> value;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == Jr800MemoryStatus::ok && value.has_value();
    }
};

struct Jr800MemoryWriteResult {
    Jr800MemoryStatus status{Jr800MemoryStatus::unsupported_region};
    Jr800MemoryRegion region{Jr800MemoryRegion::reserved};

    [[nodiscard]] bool succeeded() const noexcept {
        return status == Jr800MemoryStatus::ok;
    }
};

class Jr800Memory final {
public:
    [[nodiscard]] Jr800MemoryStatus load_logical_rom(
        std::span<const std::uint8_t> bytes
    ) noexcept;
    // Host-state initialization only; CPU-device reset does not call this.
    [[nodiscard]] Jr800MemoryStatus initialize_ram(
        Jr800MemoryRegion region,
        std::uint8_t value
    ) noexcept;
    [[nodiscard]] bool can_host_load_ram(
        std::uint16_t address,
        std::size_t size
    ) const noexcept;
    [[nodiscard]] Jr800MemoryStatus host_load_ram(
        std::uint16_t address,
        std::span<const std::uint8_t> bytes
    ) noexcept;
    [[nodiscard]] Jr800MemoryStatus host_fill_ram(
        std::uint16_t address,
        std::size_t size,
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] Jr800MemoryReadResult read8(
        std::uint16_t address
    ) const noexcept;
    [[nodiscard]] Jr800MemoryWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] bool rom_loaded() const noexcept;

private:
    friend class MachineStateCodec;
    static constexpr std::uint16_t internal_ram_base = 0x0080U;
    static constexpr std::uint16_t standard_ram_base = 0x2000U;
    static constexpr std::uint16_t expansion_ram_base = 0x6000U;

    std::array<std::uint8_t, 128U> internal_ram_{};
    std::array<std::uint8_t, 128U> internal_ram_valid_{};
    std::array<std::uint8_t, 16'384U> standard_ram_{};
    std::array<std::uint8_t, 16'384U> standard_ram_valid_{};
    std::array<std::uint8_t, 8'192U> expansion_ram_{};
    std::array<std::uint8_t, jr800_logical_rom_size> logical_rom_{};
    bool expansion_ram_initialized_{};
    bool rom_loaded_{};
};

}  // namespace jr800::core
