// SPDX-License-Identifier: MIT

#include "jr800/core/hd6301v1_ports.hpp"

namespace jr800::core {

void Hd6301v1Ports::reset() noexcept {
    port1_data_direction_ = 0U;
    port1_data_.reset();
    port2_data_direction_ = 0U;
    port2_data_latch_.reset();
    port4_data_direction_ = 0U;
}

void Hd6301v1Ports::set_port1_pin_state(
    std::uint8_t value,
    std::uint8_t known_mask
) noexcept {
    port1_pin_known_mask_ = known_mask;
    port1_pin_value_ = static_cast<std::uint8_t>(value & known_mask);
}

void Hd6301v1Ports::set_port2_pin_state(
    std::uint8_t value,
    std::uint8_t known_mask
) noexcept {
    port2_pin_known_mask_ = static_cast<std::uint8_t>(
        known_mask & port2_data_mask
    );
    port2_pin_value_ = static_cast<std::uint8_t>(
        value & port2_pin_known_mask_
    );
}

Hd6301v1PortReadResult Hd6301v1Ports::read8(
    std::uint16_t address
) const noexcept {
    if (address == port1_data_address) {
        if (port1_pin_known_mask_ != 0xFFU) {
            return {true, std::nullopt};
        }
        return {true, port1_pin_value_};
    }
    if (address == port2_data_address) {
        if (port2_pin_known_mask_ != port2_data_mask) {
            return {true, std::nullopt};
        }
        return {
            true,
            static_cast<std::uint8_t>(port2_mode_bits | port2_pin_value_),
        };
    }
    return {};
}

Hd6301v1PortWriteResult Hd6301v1Ports::write8(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    if (address == port1_data_direction_address) {
        const auto previous_value = port1_data_direction_;
        port1_data_direction_ = value;
        return {true, previous_value, true};
    }
    if (address == port2_data_direction_address) {
        const auto previous_value = port2_data_direction_;
        port2_data_direction_ = static_cast<std::uint8_t>(
            value & port2_data_direction_mask
        );
        return {true, previous_value, true};
    }
    if (address == port1_data_address) {
        const auto previous_value_known = port1_data_.has_value();
        const auto previous_value = port1_data_.value_or(0U);
        port1_data_ = value;
        return {true, previous_value, previous_value_known};
    }
    if (address == port2_data_address) {
        const auto previous_value_known = port2_data_latch_.has_value();
        const auto previous_value = static_cast<std::uint8_t>(
            port2_mode_bits | port2_data_latch_.value_or(0U)
        );
        port2_data_latch_ = static_cast<std::uint8_t>(
            value & port2_data_mask
        );
        return {true, previous_value, previous_value_known};
    }
    if (address == port4_data_direction_address) {
        const auto previous_value = port4_data_direction_;
        port4_data_direction_ = value;
        return {true, previous_value, true};
    }
    return {};
}

std::uint8_t Hd6301v1Ports::port1_data_direction() const noexcept {
    return port1_data_direction_;
}

std::optional<std::uint8_t> Hd6301v1Ports::port1_data() const noexcept {
    return port1_data_;
}

std::uint8_t Hd6301v1Ports::port2_data_direction() const noexcept {
    return port2_data_direction_;
}

std::optional<std::uint8_t> Hd6301v1Ports::port2_data_latch() const noexcept {
    return port2_data_latch_;
}

Hd6301v1Port2TimerOutputState Hd6301v1Ports::port2_timer_output_state(
    std::optional<bool> timer_level
) const noexcept {
    constexpr std::uint8_t timer_output_direction_mask = 0x02U;
    const auto output_enabled =
        (port2_data_direction_ & timer_output_direction_mask) != 0U;
    return {
        output_enabled,
        output_enabled ? timer_level : std::nullopt,
    };
}

std::uint8_t Hd6301v1Ports::port4_data_direction() const noexcept {
    return port4_data_direction_;
}

}  // namespace jr800::core
