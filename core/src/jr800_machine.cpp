// SPDX-License-Identifier: MIT

#include "jr800/core/jr800_machine.hpp"

namespace jr800::core {
namespace {

void apply_reset_flag(
    std::optional<bool> configured,
    ConditionCode flag,
    CpuState& state
) noexcept {
    if (!configured.has_value()) {
        return;
    }
    const auto mask = condition_mask(flag);
    if (*configured) {
        state.condition_code = static_cast<std::uint8_t>(
            state.condition_code | mask
        );
    } else {
        state.condition_code = static_cast<std::uint8_t>(
            state.condition_code & static_cast<std::uint8_t>(~mask)
        );
    }
    state.knowledge.condition_code = static_cast<std::uint8_t>(
        state.knowledge.condition_code | mask
    );
}

}  // namespace

Jr800Machine::Jr800Machine() noexcept : execution_(bus_) {}

Jr800Machine::Jr800Machine(
    Jr800ExperimentalMachineConfiguration configuration
) noexcept : Jr800Machine(configuration, {}) {}

Jr800Machine::Jr800Machine(
    Jr800ExperimentalMachineConfiguration configuration,
    Jr800ExperimentalResetStateConfiguration reset_state_configuration
) noexcept
    : bus_(configuration),
      reset_state_configuration_(reset_state_configuration),
      execution_(bus_) {}

Jr800MemoryStatus Jr800Machine::load_logical_rom(
    std::span<const std::uint8_t> bytes
) noexcept {
    return bus_.load_logical_rom(bytes);
}

bool Jr800Machine::can_host_load_ram(
    std::uint16_t address,
    std::size_t size
) const noexcept {
    return bus_.can_host_load_ram(address, size);
}

Jr800MemoryStatus Jr800Machine::host_load_ram(
    std::uint16_t address,
    std::span<const std::uint8_t> bytes
) noexcept {
    return bus_.host_load_ram(address, bytes);
}

Jr800MemoryStatus Jr800Machine::host_fill_ram(
    std::uint16_t address,
    std::size_t size,
    std::uint8_t value
) noexcept {
    return bus_.host_fill_ram(address, size, value);
}

void Jr800Machine::host_start_program(
    std::uint16_t program_counter
) noexcept {
    auto state = execution_.cpu().state();
    state.pc = program_counter;
    state.knowledge.registers = static_cast<std::uint8_t>(
        state.knowledge.registers | register_mask(CpuRegister::program_counter)
    );
    state.execution_state = CpuExecutionState::active;
    execution_.initialize_known_state(isa::CpuProfile::hd6301v1, state);
}

void Jr800Machine::set_port1_pin_state(
    std::uint8_t value,
    std::uint8_t known_mask
) noexcept {
    bus_.set_port1_pin_state(value, known_mask);
}

void Jr800Machine::set_port2_pin_state(
    std::uint8_t value,
    std::uint8_t known_mask
) noexcept {
    bus_.set_port2_pin_state(value, known_mask);
}

void Jr800Machine::set_ram_standby_power_valid(
    bool value,
    bool known
) noexcept {
    bus_.set_ram_standby_power_valid(value, known);
}

bool Jr800Machine::set_keyboard_bus_response(
    std::uint16_t address,
    std::uint8_t value,
    bool known
) noexcept {
    return bus_.set_keyboard_bus_response(address, value, known);
}

bool Jr800Machine::set_keyboard_key_state(
    Jr800Key key,
    bool pressed
) noexcept {
    return bus_.set_keyboard_key_state(key, pressed);
}

void Jr800Machine::clear_keyboard_activity() noexcept {
    bus_.clear_keyboard_activity();
}

Jr800KeyboardActivity Jr800Machine::keyboard_activity() const noexcept {
    return bus_.keyboard_activity();
}

Jr800CalendarOperationStatus
Jr800Machine::advance_calendar_oscillator_ticks(
    std::uint32_t ticks
) noexcept {
    return bus_.advance_calendar_oscillator_ticks(ticks);
}

Jr800CalendarOperationStatus
Jr800Machine::adjust_calendar_seconds() noexcept {
    return bus_.adjust_calendar_seconds();
}

Jr800CalendarAlarmTerminalState
Jr800Machine::calendar_alarm_terminal_state() const noexcept {
    return bus_.calendar_alarm_terminal_state();
}

Hd6301v1Port2TimerOutputState
Jr800Machine::port2_timer_output_state() const noexcept {
    return bus_.port2_timer_output_state();
}

std::optional<Jr800LcdControllerState>
Jr800Machine::inspect_lcd_controller(
    std::uint8_t controller_index
) const noexcept {
    return bus_.inspect_lcd_controller(controller_index);
}

std::optional<std::uint8_t> Jr800Machine::lcd_display_ram_value(
    std::uint8_t controller_index,
    std::uint8_t x,
    std::uint8_t y
) const noexcept {
    return bus_.lcd_display_ram_value(controller_index, x, y);
}

std::optional<bool> Jr800Machine::lcd_panel_dot(
    std::size_t column,
    std::size_t row
) const noexcept {
    return bus_.lcd_panel_dot(column, row);
}

std::optional<std::uint8_t> Jr800Machine::lcd_indicator_ram_value(
    Jr800LcdIndicator indicator
) const noexcept {
    return bus_.lcd_indicator_ram_value(indicator);
}

std::optional<std::uint64_t>
Jr800Machine::lcd_substituted_data_read_count() const noexcept {
    return bus_.lcd_substituted_data_read_count();
}

std::optional<std::uint64_t>
Jr800Machine::ignored_io_access_count() const noexcept {
    return bus_.ignored_io_access_count();
}

Jr800ResetEntryResult Jr800Machine::inspect_reset_entry() const noexcept {
    constexpr std::uint16_t vector_msb_address = 0xFFFEU;
    constexpr std::uint16_t vector_lsb_address = 0xFFFFU;
    constexpr auto known_condition_code = static_cast<std::uint8_t>(
        fixed_condition_code_bits
        | condition_mask(ConditionCode::interrupt_mask)
    );

    const auto msb = bus_.inspect8(vector_msb_address);
    if (!msb.succeeded()) {
        return {msb.fault, vector_msb_address, std::nullopt};
    }
    const auto lsb = bus_.inspect8(vector_lsb_address);
    if (!lsb.succeeded()) {
        return {lsb.fault, vector_lsb_address, std::nullopt};
    }

    const auto program_counter = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(*msb.value) << 8U)
        | static_cast<std::uint16_t>(*lsb.value)
    );
    return {
        BusFault::none,
        0U,
        Jr800ResetEntry{
            program_counter,
            known_condition_code,
            known_condition_code,
        },
    };
}

Jr800ResetEntryResult Jr800Machine::initialize_from_reset_entry() noexcept {
    const auto result = inspect_reset_entry();
    if (!result.succeeded()) {
        return result;
    }

    bus_.reset_cpu_devices();
    CpuState state;
    state.pc = result.entry->program_counter;
    state.condition_code = result.entry->condition_code_value;
    state.knowledge.registers = register_mask(CpuRegister::program_counter);
    state.knowledge.condition_code = result.entry->condition_code_known_mask;
    if (reset_state_configuration_.stack_pointer.has_value()) {
        state.sp = *reset_state_configuration_.stack_pointer;
        state.knowledge.registers = static_cast<std::uint8_t>(
            state.knowledge.registers
            | register_mask(CpuRegister::stack_pointer)
        );
    }
    if (reset_state_configuration_.index_register.has_value()) {
        state.x = *reset_state_configuration_.index_register;
        state.knowledge.registers = static_cast<std::uint8_t>(
            state.knowledge.registers
            | register_mask(CpuRegister::index_register)
        );
    }
    if (reset_state_configuration_.accumulator_a.has_value()) {
        state.a = *reset_state_configuration_.accumulator_a;
        state.knowledge.registers = static_cast<std::uint8_t>(
            state.knowledge.registers
            | register_mask(CpuRegister::accumulator_a)
        );
    }
    if (reset_state_configuration_.accumulator_b.has_value()) {
        state.b = *reset_state_configuration_.accumulator_b;
        state.knowledge.registers = static_cast<std::uint8_t>(
            state.knowledge.registers
            | register_mask(CpuRegister::accumulator_b)
        );
    }
    apply_reset_flag(
        reset_state_configuration_.half_carry,
        ConditionCode::half_carry,
        state
    );
    apply_reset_flag(
        reset_state_configuration_.negative,
        ConditionCode::negative,
        state
    );
    apply_reset_flag(
        reset_state_configuration_.zero,
        ConditionCode::zero,
        state
    );
    apply_reset_flag(
        reset_state_configuration_.overflow,
        ConditionCode::overflow,
        state
    );
    apply_reset_flag(
        reset_state_configuration_.carry,
        ConditionCode::carry,
        state
    );
    execution_.initialize_known_state(isa::CpuProfile::hd6301v1, state);
    return result;
}

Machine& Jr800Machine::execution() noexcept {
    return execution_;
}

const Machine& Jr800Machine::execution() const noexcept {
    return execution_;
}

}  // namespace jr800::core
