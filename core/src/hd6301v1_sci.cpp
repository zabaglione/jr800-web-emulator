// SPDX-License-Identifier: MIT

#include "jr800/core/hd6301v1_sci.hpp"

namespace jr800::core {

void Hd6301v1Sci::reset() noexcept {
    rate_mode_bits_ = 0U;
    control_bits_ = 0U;
    receive_status_known_ = true;
    status_known_cycles_remaining_ = 0U;
}

void Hd6301v1Sci::advance_cycles(std::uint32_t cycles) noexcept {
    if (!receive_status_known_
        || (control_bits_ & receive_enable_mask) == 0U
        || status_known_cycles_remaining_ == 0U) {
        return;
    }
    if (cycles >= status_known_cycles_remaining_) {
        receive_status_known_ = false;
        status_known_cycles_remaining_ = 0U;
        return;
    }
    status_known_cycles_remaining_ -= cycles;
}

void Hd6301v1Sci::set_receive_pin_state(bool high, bool known) noexcept {
    receive_pin_high_ = high && known;
    receive_pin_known_ = known;
    if (receive_status_known_
        && (control_bits_ & receive_enable_mask) != 0U
        && (!receive_pin_known_ || !receive_pin_high_)) {
        schedule_status_uncertainty(
            minimum_receive_status_latency_cycles
        );
    }
}

Hd6301v1SciReadResult Hd6301v1Sci::read8(
    std::uint16_t address
) const noexcept {
    if (address != control_status_address) {
        return {};
    }
    if (!receive_status_known_) {
        return {true, std::nullopt};
    }
    return {
        true,
        static_cast<std::uint8_t>(
            transmit_data_register_empty_mask | control_bits_
        ),
    };
}

Hd6301v1SciWriteResult Hd6301v1Sci::write8(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    if (address == rate_mode_address) {
        const auto previous = rate_mode_bits_;
        rate_mode_bits_ = static_cast<std::uint8_t>(
            value & writable_rate_mode_mask
        );
        return {true, previous, true};
    }

    if (address == control_status_address) {
        const auto previous = read8(address);
        const auto receive_was_enabled =
            (control_bits_ & receive_enable_mask) != 0U;
        const auto wake_up_was_enabled =
            (control_bits_ & wake_up_mask) != 0U;
        control_bits_ = static_cast<std::uint8_t>(
            value & writable_control_mask
        );
        const auto receive_is_enabled =
            (control_bits_ & receive_enable_mask) != 0U;
        const auto wake_up_is_enabled =
            (control_bits_ & wake_up_mask) != 0U;
        if (receive_status_known_) {
            if (receive_is_enabled && !receive_was_enabled) {
                if (!receive_pin_known_ || !receive_pin_high_) {
                    schedule_status_uncertainty(
                        minimum_receive_status_latency_cycles
                    );
                } else if (wake_up_is_enabled) {
                    schedule_status_uncertainty(
                        minimum_wakeup_clear_latency_cycles
                    );
                }
            } else if (receive_is_enabled && wake_up_is_enabled
                       && !wake_up_was_enabled) {
                schedule_status_uncertainty(
                    minimum_wakeup_clear_latency_cycles
                );
            } else if (!receive_is_enabled) {
                status_known_cycles_remaining_ = 0U;
            }
        }
        return {
            true,
            previous.value.value_or(0U),
            previous.value.has_value(),
        };
    }

    return {};
}

void Hd6301v1Sci::schedule_status_uncertainty(
    std::uint32_t cycles
) noexcept {
    if (status_known_cycles_remaining_ == 0U
        || cycles < status_known_cycles_remaining_) {
        status_known_cycles_remaining_ = cycles;
    }
}

std::uint8_t Hd6301v1Sci::rate_mode_bits() const noexcept {
    return rate_mode_bits_;
}

std::uint8_t Hd6301v1Sci::control_bits() const noexcept {
    return control_bits_;
}

Hd6301v1SciInterruptRequest Hd6301v1Sci::interrupt_request()
    const noexcept {
    if ((control_bits_ & transmit_interrupt_enable_mask) != 0U) {
        return {true, true};
    }
    if ((control_bits_ & receive_interrupt_enable_mask) != 0U
        && !receive_status_known_) {
        return {false, false};
    }
    return {};
}

}  // namespace jr800::core
