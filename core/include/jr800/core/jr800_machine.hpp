// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <span>
#include <vector>

#include "jr800/core/jr800_bus.hpp"
#include "jr800/core/machine.hpp"

namespace jr800::core {

struct Jr800ResetEntry {
    std::uint16_t program_counter{};
    std::uint8_t condition_code_value{};
    std::uint8_t condition_code_known_mask{};

    bool operator==(const Jr800ResetEntry&) const = default;
};

struct Jr800ResetEntryResult {
    BusFault fault{BusFault::none};
    std::uint16_t fault_address{};
    std::optional<Jr800ResetEntry> entry;

    [[nodiscard]] bool succeeded() const noexcept {
        return fault == BusFault::none && entry.has_value();
    }

    bool operator==(const Jr800ResetEntryResult&) const = default;
};

// Host-selected values only for reset fields that the hardware documentation
// leaves unspecified. PC, fixed CCR bits, and the reset interrupt mask are not
// configurable through this experiment.
struct Jr800ExperimentalResetStateConfiguration {
    std::optional<std::uint16_t> stack_pointer;
    std::optional<std::uint16_t> index_register;
    std::optional<std::uint8_t> accumulator_a;
    std::optional<std::uint8_t> accumulator_b;
    std::optional<bool> half_carry;
    std::optional<bool> negative;
    std::optional<bool> zero;
    std::optional<bool> overflow;
    std::optional<bool> carry;

    bool operator==(const Jr800ExperimentalResetStateConfiguration&) const =
        default;
};

class Jr800Machine final {
public:
    Jr800Machine() noexcept;
    explicit Jr800Machine(
        Jr800ExperimentalMachineConfiguration configuration
    ) noexcept;
    Jr800Machine(
        Jr800ExperimentalMachineConfiguration configuration,
        Jr800ExperimentalResetStateConfiguration reset_state_configuration
    ) noexcept;

    // Versioned host state. ROM and RTC are deliberately excluded.
    [[nodiscard]] std::vector<std::uint8_t> save_state() const;
    void restore_state(std::span<const std::uint8_t> bytes);

    // Isolated host import transactions copy device/CPU state, not observers.
    [[nodiscard]] std::unique_ptr<Jr800Machine> clone() const;
    void copy_state_from(const Jr800Machine& source) noexcept;
    Jr800Machine& operator=(const Jr800Machine&) = delete;
    Jr800Machine(Jr800Machine&&) = delete;
    Jr800Machine& operator=(Jr800Machine&&) = delete;

    [[nodiscard]] Jr800MemoryStatus load_logical_rom(
        std::span<const std::uint8_t> bytes
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
    void host_start_program(std::uint16_t program_counter) noexcept;
    [[nodiscard]] bool host_return_from_subroutine(
        std::uint8_t accumulator_a, std::uint8_t condition_code
    ) noexcept;

    void set_port1_pin_state(
        std::uint8_t value,
        std::uint8_t known_mask
    ) noexcept;
    void set_port2_pin_state(
        std::uint8_t value,
        std::uint8_t known_mask
    ) noexcept;
    void set_ram_standby_power_valid(bool value, bool known) noexcept;
    [[nodiscard]] bool set_keyboard_bus_response(
        std::uint16_t address,
        std::uint8_t value,
        bool known
    ) noexcept;
    [[nodiscard]] bool set_keyboard_key_state(
        Jr800Key key,
        bool pressed
    ) noexcept;
    void clear_keyboard_activity() noexcept;
    [[nodiscard]] Jr800KeyboardActivity keyboard_activity() const noexcept;
    [[nodiscard]] Jr800CalendarOperationStatus
    advance_calendar_oscillator_ticks(std::uint32_t ticks) noexcept;
    [[nodiscard]] Jr800CalendarOperationStatus
    adjust_calendar_seconds() noexcept;
    [[nodiscard]] Jr800CalendarOperationStatus
    set_calendar_datetime(CalendarDateTime value) noexcept;
    [[nodiscard]] Jr800CalendarAlarmTerminalState
    calendar_alarm_terminal_state() const noexcept;
    [[nodiscard]] Hd6301v1Port2TimerOutputState
    port2_timer_output_state() const noexcept;

    [[nodiscard]] std::optional<Jr800LcdControllerState>
    inspect_lcd_controller(std::uint8_t controller_index) const noexcept;
    [[nodiscard]] std::optional<std::uint8_t> lcd_display_ram_value(
        std::uint8_t controller_index,
        std::uint8_t x,
        std::uint8_t y
    ) const noexcept;
    [[nodiscard]] std::optional<bool> lcd_panel_dot(
        std::size_t column,
        std::size_t row
    ) const noexcept;
    [[nodiscard]] std::optional<std::uint8_t> lcd_indicator_ram_value(
        Jr800LcdIndicator indicator
    ) const noexcept;
    [[nodiscard]] std::optional<std::uint64_t>
    lcd_substituted_data_read_count() const noexcept;
    [[nodiscard]] std::optional<std::uint64_t>
    ignored_io_access_count() const noexcept;

    [[nodiscard]] Jr800ResetEntryResult inspect_reset_entry() const noexcept;
    [[nodiscard]] Jr800ResetEntryResult initialize_from_reset_entry() noexcept;

    [[nodiscard]] Machine& execution() noexcept;
    [[nodiscard]] const Machine& execution() const noexcept;

private:
    Jr800Machine(const Jr800Machine& source) noexcept;
    Jr800Bus bus_{};
    Jr800ExperimentalResetStateConfiguration reset_state_configuration_;
    Machine execution_;
};

}  // namespace jr800::core
