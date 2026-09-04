// SPDX-License-Identifier: MIT

#include "jr800/core/hd6301v1_timer.hpp"

namespace jr800::core {

void Hd6301v1Timer::reset() noexcept {
    control_bits_ = 0U;
    status_bits_ = 0U;
    free_running_counter_ = 0U;
    pending_free_running_counter_high_ = 0U;
    free_running_counter_high_write_pending_ = false;
    output_compare_ = output_compare_reset_value;
    output_compare_level_.reset();
    input_capture_.reset();
    overflow_clear_armed_ = false;
    output_compare_clear_armed_ = false;
    input_capture_clear_armed_ = false;
    input_capture_enabled_ = true;
    input_capture_status_known_ = true;
    comparison_inhibit_cycles_ = 0U;
}

void Hd6301v1Timer::advance_cycles(std::uint32_t cycles) noexcept {
    if (cycles != 0U && input_capture_enabled_
        && !input_capture_pin_known_) {
        input_capture_status_known_ = false;
        input_capture_.reset();
    }
    for (std::uint32_t cycle = 0U; cycle < cycles; ++cycle) {
        free_running_counter_ = static_cast<std::uint16_t>(
            free_running_counter_ + 1U
        );
        if (free_running_counter_ == 0U) {
            status_bits_ = static_cast<std::uint8_t>(
                status_bits_ | overflow_flag_mask
            );
        }
        if (comparison_inhibit_cycles_ != 0U) {
            --comparison_inhibit_cycles_;
        } else if (free_running_counter_ == output_compare_) {
            status_bits_ = static_cast<std::uint8_t>(
                status_bits_ | output_compare_flag_mask
            );
            output_compare_level_ = (control_bits_ & 0x01U) != 0U;
        }
    }
}

void Hd6301v1Timer::set_input_capture_pin_state(
    bool value,
    bool known
) noexcept {
    if (!known) {
        input_capture_pin_known_ = false;
        if (input_capture_enabled_) {
            input_capture_status_known_ = false;
            input_capture_.reset();
        }
        return;
    }
    if (!input_capture_pin_known_) {
        input_capture_pin_value_ = value;
        input_capture_pin_known_ = true;
        return;
    }

    const auto previous = input_capture_pin_value_;
    input_capture_pin_value_ = value;
    if (!input_capture_enabled_ || previous == value) {
        return;
    }

    const auto rising_edge = !previous && value;
    const auto captures_rising_edge =
        (control_bits_ & input_edge_mask) != 0U;
    if (rising_edge == captures_rising_edge) {
        input_capture_ = free_running_counter_;
        input_capture_status_known_ = true;
        status_bits_ = static_cast<std::uint8_t>(
            status_bits_ | input_capture_flag_mask
        );
    }
}

void Hd6301v1Timer::set_input_capture_enabled(bool enabled) noexcept {
    input_capture_enabled_ = enabled;
}

Hd6301v1TimerReadResult Hd6301v1Timer::read8(
    std::uint16_t address
) noexcept {
    const auto result = inspect8(address);
    if (!result.handled || !result.value.has_value()) {
        return result;
    }

    if (address == control_status_address) {
        overflow_clear_armed_ =
            (status_bits_ & overflow_flag_mask) != 0U;
        output_compare_clear_armed_ =
            (status_bits_ & output_compare_flag_mask) != 0U;
        input_capture_clear_armed_ =
            (status_bits_ & input_capture_flag_mask) != 0U;
    } else if (address == free_running_counter_high_address
               && overflow_clear_armed_) {
        status_bits_ = static_cast<std::uint8_t>(
            status_bits_ & static_cast<std::uint8_t>(~overflow_flag_mask)
        );
        overflow_clear_armed_ = false;
    } else if (address == input_capture_high_address
               && input_capture_clear_armed_) {
        status_bits_ = static_cast<std::uint8_t>(
            status_bits_ & static_cast<std::uint8_t>(~input_capture_flag_mask)
        );
        input_capture_clear_armed_ = false;
    }
    return result;
}

Hd6301v1TimerReadResult Hd6301v1Timer::inspect8(
    std::uint16_t address
) const noexcept {
    if (address == control_status_address) {
        if (!input_capture_status_known_) {
            return {true, std::nullopt};
        }
        return {
            true,
            static_cast<std::uint8_t>(status_bits_ | control_bits_),
        };
    }
    if (address == free_running_counter_high_address) {
        return {
            true,
            static_cast<std::uint8_t>(free_running_counter_ >> 8U),
        };
    }
    if (address == free_running_counter_low_address) {
        return {
            true,
            static_cast<std::uint8_t>(free_running_counter_ & 0x00FFU),
        };
    }
    if (address == output_compare_high_address) {
        return {
            true,
            static_cast<std::uint8_t>(output_compare_ >> 8U),
        };
    }
    if (address == output_compare_low_address) {
        return {
            true,
            static_cast<std::uint8_t>(output_compare_ & 0x00FFU),
        };
    }
    if (address == input_capture_high_address) {
        if (!input_capture_.has_value()) {
            return {true, std::nullopt};
        }
        return {
            true,
            static_cast<std::uint8_t>(*input_capture_ >> 8U),
        };
    }
    if (address == input_capture_low_address) {
        if (!input_capture_.has_value()) {
            return {true, std::nullopt};
        }
        return {
            true,
            static_cast<std::uint8_t>(*input_capture_ & 0x00FFU),
        };
    }
    return {};
}

Hd6301v1TimerWriteResult Hd6301v1Timer::write8(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    if (address == control_status_address) {
        const auto previous_value_known = input_capture_status_known_;
        const auto previous = static_cast<std::uint8_t>(
            previous_value_known ? status_bits_ | control_bits_ : 0U
        );
        control_bits_ = static_cast<std::uint8_t>(
            value & writable_control_mask
        );
        return {true, previous, previous_value_known};
    }

    if (address == free_running_counter_high_address) {
        const auto previous = static_cast<std::uint8_t>(
            free_running_counter_ >> 8U
        );
        pending_free_running_counter_high_ = value;
        free_running_counter_high_write_pending_ = true;
        free_running_counter_ = free_running_counter_write_preset;
        comparison_inhibit_cycles_ = comparison_inhibit_cycles_after_write;
        return {true, previous, true};
    }

    if (address == free_running_counter_low_address
        && free_running_counter_high_write_pending_) {
        const auto previous = static_cast<std::uint8_t>(
            free_running_counter_ & 0x00FFU
        );
        free_running_counter_ = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(pending_free_running_counter_high_) << 8U
            | value
        );
        free_running_counter_high_write_pending_ = false;
        comparison_inhibit_cycles_ = comparison_inhibit_cycles_after_write;
        return {true, previous, true};
    }

    if (address == output_compare_high_address) {
        const auto previous = static_cast<std::uint8_t>(
            output_compare_ >> 8U
        );
        output_compare_ = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(value) << 8U
            | (output_compare_ & 0x00FFU)
        );
        if (output_compare_clear_armed_) {
            status_bits_ = static_cast<std::uint8_t>(
                status_bits_
                & static_cast<std::uint8_t>(~output_compare_flag_mask)
            );
            output_compare_clear_armed_ = false;
        }
        comparison_inhibit_cycles_ = comparison_inhibit_cycles_after_write;
        return {true, previous, true};
    }

    if (address == output_compare_low_address) {
        const auto previous = static_cast<std::uint8_t>(
            output_compare_ & 0x00FFU
        );
        output_compare_ = static_cast<std::uint16_t>(
            (output_compare_ & 0xFF00U) | value
        );
        if (output_compare_clear_armed_) {
            status_bits_ = static_cast<std::uint8_t>(
                status_bits_
                & static_cast<std::uint8_t>(~output_compare_flag_mask)
            );
            output_compare_clear_armed_ = false;
        }
        return {true, previous, true};
    }

    return {};
}

std::uint8_t Hd6301v1Timer::control_bits() const noexcept {
    return control_bits_;
}

std::uint8_t Hd6301v1Timer::status_bits() const noexcept {
    return status_bits_;
}

bool Hd6301v1Timer::status_bits_known() const noexcept {
    return input_capture_status_known_;
}

std::uint16_t Hd6301v1Timer::free_running_counter() const noexcept {
    return free_running_counter_;
}

bool Hd6301v1Timer::free_running_counter_high_write_pending() const noexcept {
    return free_running_counter_high_write_pending_;
}

std::optional<std::uint16_t> Hd6301v1Timer::input_capture() const noexcept {
    return input_capture_;
}

std::optional<bool> Hd6301v1Timer::output_compare_level() const noexcept {
    return output_compare_level_;
}

Hd6301v1TimerInterruptRequest Hd6301v1Timer::interrupt_request()
    const noexcept {
    const auto input_capture_enabled =
        (control_bits_ & input_capture_interrupt_enable_mask) != 0U;
    if (input_capture_enabled
        && (status_bits_ & input_capture_flag_mask) != 0U) {
        return {Hd6301v1TimerInterruptSource::input_capture, true};
    }
    if (input_capture_enabled && !input_capture_status_known_) {
        return {std::nullopt, false};
    }
    if ((control_bits_ & output_compare_interrupt_enable_mask) != 0U
        && (status_bits_ & output_compare_flag_mask) != 0U) {
        return {Hd6301v1TimerInterruptSource::output_compare, true};
    }
    if ((control_bits_ & overflow_interrupt_enable_mask) != 0U
        && (status_bits_ & overflow_flag_mask) != 0U) {
        return {Hd6301v1TimerInterruptSource::overflow, true};
    }
    return {};
}

}  // namespace jr800::core
