// SPDX-License-Identifier: MIT

#include "jr800/core/jr800_memory.hpp"

#include <algorithm>
#include <cstddef>

namespace jr800::core {
namespace {

bool is_rom(Jr800MemoryRegion region) noexcept {
    return region == Jr800MemoryRegion::standard_rom
        || region == Jr800MemoryRegion::expansion_rom
        || region == Jr800MemoryRegion::cpu_internal_rom;
}

}  // namespace

Jr800MemoryStatus Jr800Memory::load_logical_rom(
    std::span<const std::uint8_t> bytes
) noexcept {
    if (bytes.size() != logical_rom_.size()) {
        return Jr800MemoryStatus::invalid_rom_size;
    }
    std::copy(bytes.begin(), bytes.end(), logical_rom_.begin());
    rom_loaded_ = true;
    return Jr800MemoryStatus::ok;
}

Jr800MemoryStatus Jr800Memory::initialize_ram(
    Jr800MemoryRegion region,
    std::uint8_t value
) noexcept {
    if (region == Jr800MemoryRegion::cpu_internal_ram) {
        internal_ram_.fill(value);
        internal_ram_valid_.fill(1U);
        return Jr800MemoryStatus::ok;
    }
    if (region == Jr800MemoryRegion::standard_ram) {
        standard_ram_.fill(value);
        standard_ram_valid_.fill(1U);
        return Jr800MemoryStatus::ok;
    }
    if (region == Jr800MemoryRegion::expansion_ram) {
        expansion_ram_.fill(value);
        expansion_ram_initialized_ = true;
        return Jr800MemoryStatus::ok;
    }
    return Jr800MemoryStatus::unsupported_region;
}

bool Jr800Memory::can_host_load_ram(
    std::uint16_t address,
    std::size_t size
) const noexcept {
    if (size == 0U
        || size > 0x1'0000U - static_cast<std::size_t>(address)) {
        return false;
    }
    for (std::size_t offset = 0U; offset < size; ++offset) {
        const auto current = static_cast<std::uint16_t>(address + offset);
        const auto region = jr800_memory_region(current);
        if (region != Jr800MemoryRegion::standard_ram
            && (region != Jr800MemoryRegion::expansion_ram
                || !expansion_ram_initialized_)) {
            return false;
        }
    }
    return true;
}

Jr800MemoryStatus Jr800Memory::host_load_ram(
    std::uint16_t address,
    std::span<const std::uint8_t> bytes
) noexcept {
    if (!can_host_load_ram(address, bytes.size())) {
        return Jr800MemoryStatus::unsupported_region;
    }
    for (std::size_t offset = 0U; offset < bytes.size(); ++offset) {
        const auto current = static_cast<std::uint16_t>(address + offset);
        static_cast<void>(write8(current, bytes[offset]));
    }
    return Jr800MemoryStatus::ok;
}

Jr800MemoryStatus Jr800Memory::host_fill_ram(
    std::uint16_t address,
    std::size_t size,
    std::uint8_t value
) noexcept {
    if (!can_host_load_ram(address, size)) {
        return Jr800MemoryStatus::unsupported_region;
    }
    for (std::size_t offset = 0U; offset < size; ++offset) {
        const auto current = static_cast<std::uint16_t>(address + offset);
        static_cast<void>(write8(current, value));
    }
    return Jr800MemoryStatus::ok;
}

Jr800MemoryReadResult Jr800Memory::read8(std::uint16_t address) const noexcept {
    const auto region = jr800_memory_region(address);
    if (region == Jr800MemoryRegion::cpu_internal_ram) {
        const auto offset = static_cast<std::size_t>(address - internal_ram_base);
        if (internal_ram_valid_[offset] == 0U) {
            return {Jr800MemoryStatus::uninitialized_ram, region, std::nullopt};
        }
        return {Jr800MemoryStatus::ok, region, internal_ram_[offset]};
    }
    if (region == Jr800MemoryRegion::standard_ram) {
        const auto offset = static_cast<std::size_t>(address - standard_ram_base);
        if (standard_ram_valid_[offset] == 0U) {
            return {Jr800MemoryStatus::uninitialized_ram, region, std::nullopt};
        }
        return {Jr800MemoryStatus::ok, region, standard_ram_[offset]};
    }
    if (region == Jr800MemoryRegion::expansion_ram
        && expansion_ram_initialized_) {
        const auto offset = static_cast<std::size_t>(
            address - expansion_ram_base
        );
        return {Jr800MemoryStatus::ok, region, expansion_ram_[offset]};
    }
    if (is_rom(region)) {
        if (!rom_loaded_) {
            return {Jr800MemoryStatus::rom_not_loaded, region, std::nullopt};
        }
        const auto offset = static_cast<std::size_t>(
            address - jr800_logical_rom_base
        );
        return {Jr800MemoryStatus::ok, region, logical_rom_[offset]};
    }
    return {Jr800MemoryStatus::unsupported_region, region, std::nullopt};
}

Jr800MemoryWriteResult Jr800Memory::write8(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    const auto region = jr800_memory_region(address);
    if (region == Jr800MemoryRegion::cpu_internal_ram) {
        const auto offset = static_cast<std::size_t>(address - internal_ram_base);
        internal_ram_[offset] = value;
        internal_ram_valid_[offset] = 1U;
        return {Jr800MemoryStatus::ok, region};
    }
    if (region == Jr800MemoryRegion::standard_ram) {
        const auto offset = static_cast<std::size_t>(address - standard_ram_base);
        standard_ram_[offset] = value;
        standard_ram_valid_[offset] = 1U;
        return {Jr800MemoryStatus::ok, region};
    }
    if (region == Jr800MemoryRegion::expansion_ram
        && expansion_ram_initialized_) {
        const auto offset = static_cast<std::size_t>(
            address - expansion_ram_base
        );
        expansion_ram_[offset] = value;
        return {Jr800MemoryStatus::ok, region};
    }
    if (is_rom(region)) {
        return {Jr800MemoryStatus::read_only, region};
    }
    return {Jr800MemoryStatus::unsupported_region, region};
}

bool Jr800Memory::rom_loaded() const noexcept {
    return rom_loaded_;
}

}  // namespace jr800::core
