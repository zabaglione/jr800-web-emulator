// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "jr800/core/bus.hpp"
#include "jr800/core/hd6301v1_ports.hpp"
#include "jr800/core/hd6301v1_ram_control.hpp"
#include "jr800/core/hd6301v1_sci.hpp"
#include "jr800/core/hd6301v1_timer.hpp"
#include "jr800/core/jr800_keyboard.hpp"
#include "jr800/core/jr800_lcd.hpp"
#include "jr800/core/jr800_memory.hpp"
#include "jr800/core/rp5c01_register_file.hpp"

namespace jr800::core {

// Selecting this E-293 experiment also binds controller reset to CPU-device
// reset and completes busy periods at bus-access boundaries. U-010 still owns
// physical verification of those assumptions and the supplied read value.
struct Jr800ExperimentalLcdConfiguration {
    std::uint8_t unknown_data_read_value{};

    bool operator==(const Jr800ExperimentalLcdConfiguration&) const = default;
};

// E-336 host-provided power-on value for the CPU's internal RAM. Omission
// preserves byte-level unknownness rather than implying a reset value.
struct Jr800ExperimentalInternalRamConfiguration {
    std::uint8_t initial_value{};

    bool operator==(const Jr800ExperimentalInternalRamConfiguration&) const =
        default;
};

// E-294 host-provided power-on values. A missing expansion value keeps that
// logical region disconnected rather than implying physical absence.
struct Jr800ExperimentalMemoryConfiguration {
    std::uint8_t standard_ram_initial_value{};
    std::optional<std::uint8_t> expansion_ram_initial_value;

    bool operator==(const Jr800ExperimentalMemoryConfiguration&) const =
        default;
};

enum class Jr800ExperimentalCalendarAddressSource : std::uint8_t {
    cpu_a0_to_a3 = 0U,
    cpu_a1_to_a4 = 1U,
    cpu_a2_to_a5 = 2U,
    cpu_a3_to_a6 = 3U,
    cpu_a4_to_a7 = 4U,
    cpu_a5_to_a8 = 5U,
};

enum class Jr800ExperimentalCalendarUpperReadBits : std::uint8_t {
    all_zero = 0x00U,
    all_one = 0xF0U,
};

enum class Jr800ExperimentalCalendarCpuCycleRatio : std::uint8_t {
    e030_nominal_1_2288_mhz = 0U,
};

// E-295 uses an explicitly all-zero retained register file. Omitting the
// E-355 ratio preserves explicit-only, non-ticking CPU-cycle behavior.
struct Jr800ExperimentalCalendarConfiguration {
    Jr800ExperimentalCalendarAddressSource address_source{
        Jr800ExperimentalCalendarAddressSource::cpu_a0_to_a3
    };
    Jr800ExperimentalCalendarUpperReadBits upper_read_bits{
        Jr800ExperimentalCalendarUpperReadBits::all_zero
    };
    std::optional<Jr800ExperimentalCalendarCpuCycleRatio> cpu_cycle_ratio;

    bool operator==(const Jr800ExperimentalCalendarConfiguration&) const =
        default;
};

// Each optional member enables only its named experiment.
struct Jr800ExperimentalMachineConfiguration {
    std::optional<Jr800ExperimentalInternalRamConfiguration> internal_ram;
    std::optional<Jr800ExperimentalLcdConfiguration> lcd;
    std::optional<Jr800ExperimentalMemoryConfiguration> memory;
    std::optional<Jr800ExperimentalCalendarConfiguration> calendar;

    bool operator==(const Jr800ExperimentalMachineConfiguration&) const =
        default;
};

enum class Jr800CalendarOperationStatus : std::uint8_t {
    ok,
    calendar_disconnected,
    unknown_state,
    unsupported_state,
};

struct Jr800CalendarAlarmTerminalState {
    bool connected{};
    std::optional<bool> pull_low;

    bool operator==(const Jr800CalendarAlarmTerminalState&) const = default;
};

class Jr800Bus final : public Bus {
public:
    Jr800Bus() noexcept = default;
    explicit Jr800Bus(
        Jr800ExperimentalMachineConfiguration configuration
    ) noexcept;

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
    void reset_cpu_devices() noexcept;
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
    [[nodiscard]] Jr800CalendarAlarmTerminalState
    calendar_alarm_terminal_state() const noexcept;
    [[nodiscard]] Hd6301v1Port2TimerOutputState
    port2_timer_output_state() const noexcept;

    [[nodiscard]] BusFault advance_cycles(
        std::uint32_t cycles
    ) noexcept override;
    [[nodiscard]] InterruptRequest maskable_interrupt_request()
        const noexcept override;

    [[nodiscard]] BusReadResult read8(
        std::uint16_t address,
        AccessKind kind
    ) noexcept override;
    [[nodiscard]] BusDiscardedReadResult read8_discard(
        std::uint16_t address
    ) noexcept override;
    [[nodiscard]] BusReadResult inspect8(
        std::uint16_t address
    ) const noexcept override;
    [[nodiscard]] BusWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept override;

    [[nodiscard]] bool rom_loaded() const noexcept;

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

private:
    [[nodiscard]] BusReadResult read_experimental_calendar(
        std::uint16_t address,
        AccessKind kind
    ) noexcept;
    [[nodiscard]] BusWriteResult write_experimental_calendar(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept;
    [[nodiscard]] std::optional<std::uint8_t>
    experimental_calendar_address(std::uint16_t address) const noexcept;
    [[nodiscard]] std::optional<std::uint8_t>
    experimental_calendar_upper_bits() const noexcept;

    [[nodiscard]] BusReadResult read_experimental_lcd(
        std::uint16_t address,
        AccessKind kind
    ) noexcept;
    [[nodiscard]] BusWriteResult write_experimental_lcd(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept;

    Hd6301v1Ports ports_{};
    Hd6301v1RamControl ram_control_{};
    Hd6301v1Sci sci_{};
    Hd6301v1Timer timer_{};
    Jr800Keyboard keyboard_{};
    Rp5c01RegisterFile calendar_{};
    Jr800Lcd lcd_{};
    Jr800Memory memory_{};
    std::optional<Jr800ExperimentalLcdConfiguration>
        experimental_lcd_configuration_;
    std::optional<Jr800ExperimentalCalendarConfiguration>
        experimental_calendar_configuration_;
    std::uint8_t calendar_cpu_cycle_remainder_{};
    std::uint64_t lcd_substituted_data_read_count_{};
};

}  // namespace jr800::core
