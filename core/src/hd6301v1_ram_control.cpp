// SPDX-License-Identifier: MIT

#include "jr800/core/hd6301v1_ram_control.hpp"

namespace jr800::core {

void Hd6301v1RamControl::reset() noexcept {
    ram_enabled_ = true;
}

void Hd6301v1RamControl::set_standby_power_valid(
    bool value,
    bool known
) noexcept {
    standby_power_valid_ = known ? std::optional<bool>{value} : std::nullopt;
}

Hd6301v1RamControlReadResult Hd6301v1RamControl::read8(
    std::uint16_t address
) const noexcept {
    if (address != register_address) {
        return {};
    }
    if (!standby_power_valid_.has_value()) {
        return {true, std::nullopt};
    }
    auto value = unused_read_mask;
    if (ram_enabled_) {
        value = static_cast<std::uint8_t>(value | ram_enable_mask);
    }
    if (*standby_power_valid_) {
        value = static_cast<std::uint8_t>(value | standby_mask);
    }
    return {true, value};
}

Hd6301v1RamControlWriteResult Hd6301v1RamControl::write8(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    const auto previous = read8(address);
    if (!previous.handled) {
        return {};
    }
    standby_power_valid_ = (value & standby_mask) != 0U;
    ram_enabled_ = (value & ram_enable_mask) != 0U;
    return {
        true,
        previous.value.value_or(0U),
        previous.value.has_value(),
    };
}

bool Hd6301v1RamControl::ram_enabled() const noexcept {
    return ram_enabled_;
}

std::optional<bool> Hd6301v1RamControl::standby_power_valid() const noexcept {
    return standby_power_valid_;
}

}  // namespace jr800::core
