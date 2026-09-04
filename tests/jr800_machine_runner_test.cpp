// SPDX-License-Identifier: MIT

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "jr800/core/jr800_machine.hpp"
#include "jr800/core/jr800_memory.hpp"
#include "jr800/runtime/jr800_machine_runner.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

std::vector<std::uint8_t> make_rom(
    std::span<const std::uint8_t> program
) {
    std::vector<std::uint8_t> rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    std::copy(program.begin(), program.end(), rom.begin());
    rom[rom.size() - 2U] = 0x80U;
    rom[rom.size() - 1U] = 0x00U;
    return rom;
}

bool load_and_reset(
    jr800::core::Jr800Machine& machine,
    std::span<const std::uint8_t> rom
) {
    return machine.load_logical_rom(rom)
            == jr800::core::Jr800MemoryStatus::ok
        && machine.initialize_from_reset_entry().succeeded();
}

}  // namespace

int main() {
    using jr800::core::AccessKind;
    using jr800::core::BusFault;
    using jr800::core::CpuFault;
    using jr800::core::CpuStatePart;
    using jr800::core::Jr800CalendarOperationStatus;
    using jr800::core::Jr800ExperimentalCalendarConfiguration;
    using jr800::core::Jr800ExperimentalCalendarCpuCycleRatio;
    using jr800::core::Jr800ExperimentalLcdConfiguration;
    using jr800::core::Jr800ExperimentalMachineConfiguration;
    using jr800::core::Jr800Machine;
    using jr800::core::Jr800MemoryRegion;
    using jr800::runtime::Jr800RunLimits;
    using jr800::runtime::Jr800RunStopReason;
    using jr800::runtime::run_jr800_machine;

    bool passed = true;

    Jr800Machine uninitialized;
    const auto uninitialized_state = uninitialized.execution().cpu().state();
    const auto uninitialized_result = run_jr800_machine(uninitialized);
    passed &= expect(
        uninitialized_result.stop_reason
                == Jr800RunStopReason::machine_not_initialized
            && uninitialized_result.instructions_completed == 0U
            && uninitialized_result.execution_cycles == 0U
            && !uninitialized_result.calendar_alarm_terminal.connected
            && !uninitialized_result.calendar_alarm_terminal.pull_low
                .has_value()
            && !uninitialized_result.port2_timer_output.output_enabled
            && !uninitialized_result.port2_timer_output.level.has_value()
            && uninitialized_result.port2_timer_output_coverage.disabled
            && !uninitialized_result.port2_timer_output_coverage.unknown
            && !uninitialized_result.port2_timer_output_coverage.low
            && !uninitialized_result.port2_timer_output_coverage.high
            && uninitialized.execution().cpu().state() == uninitialized_state,
        "Uninitialized machine did not fail without changing state"
    );

    Jr800Machine invalid_limit_machine;
    const auto nop_rom = make_rom(std::array<std::uint8_t, 1U>{0x01U});
    passed &= expect(
        load_and_reset(invalid_limit_machine, nop_rom),
        "Invalid-limit machine setup failed"
    );
    const auto invalid_limit_state =
        invalid_limit_machine.execution().cpu().state();
    const auto zero_instruction_limit = run_jr800_machine(
        invalid_limit_machine,
        Jr800RunLimits{0U, 1U}
    );
    const auto zero_suspended_limit = run_jr800_machine(
        invalid_limit_machine,
        Jr800RunLimits{1U, 0U}
    );
    passed &= expect(
        zero_instruction_limit.stop_reason
                == Jr800RunStopReason::invalid_limits
            && zero_suspended_limit.stop_reason
                == Jr800RunStopReason::invalid_limits
            && zero_instruction_limit.port2_timer_output_coverage.disabled
            && !zero_instruction_limit.port2_timer_output_coverage.unknown
            && !zero_instruction_limit.port2_timer_output_coverage.low
            && !zero_instruction_limit.port2_timer_output_coverage.high
            && zero_suspended_limit.port2_timer_output_coverage.disabled
            && !zero_suspended_limit.port2_timer_output_coverage.unknown
            && !zero_suspended_limit.port2_timer_output_coverage.low
            && !zero_suspended_limit.port2_timer_output_coverage.high
            && invalid_limit_machine.execution().cpu().state()
                == invalid_limit_state,
        "Invalid run limits changed machine state"
    );

    Jr800Machine nop_machine;
    passed &= expect(
        load_and_reset(nop_machine, nop_rom),
        "NOP runner setup failed"
    );
    const auto nop_result = run_jr800_machine(
        nop_machine,
        Jr800RunLimits{3U, 32U}
    );
    passed &= expect(
        nop_result.completed_limit()
            && nop_result.instructions_completed == 3U
            && nop_result.execution_cycles == 3U
            && nop_result.interrupt_entries == 0U
            && nop_result.timer_input_capture_interrupts == 0U
            && nop_result.timer_output_compare_interrupts == 0U
            && nop_result.timer_overflow_interrupts == 0U
            && nop_result.serial_interrupts == 0U
            && nop_result.instructions_after_last_interrupt == 0U
            && !nop_result.last_interrupt_target_region.has_value()
            && nop_result.sleep_entries == 0U
            && nop_result.wait_entries == 0U
            && nop_result.sleep_resumes == 0U
            && nop_result.suspended_cycles == 0U
            && nop_result.cpu_fault == CpuFault::none
            && nop_result.bus_fault == BusFault::none
            && !nop_result.fault_access.has_value()
            && !nop_result.fault_region.has_value()
            && nop_result.state_fault == CpuStatePart::none
            && nop_result.keyboard_activity.read_attempts == 0U
            && nop_result.keyboard_activity.distinct_addresses == 0U
            && !nop_result.calendar_alarm_terminal.connected
            && !nop_result.calendar_alarm_terminal.pull_low.has_value()
            && !nop_result.port2_timer_output.output_enabled
            && !nop_result.port2_timer_output.level.has_value()
            && nop_result.port2_timer_output_coverage.disabled
            && !nop_result.port2_timer_output_coverage.unknown
            && !nop_result.port2_timer_output_coverage.low
            && !nop_result.port2_timer_output_coverage.high
            && !nop_result.lcd_substituted_data_reads.has_value()
            && !nop_result.lcd_panel.has_value(),
        "Straight-line run summary differs"
    );

    auto calendar_machine = std::make_unique<Jr800Machine>(
        Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{
                .cpu_cycle_ratio =
                    Jr800ExperimentalCalendarCpuCycleRatio::
                        e030_nominal_1_2288_mhz,
            },
        }
    );
    passed &= expect(
        load_and_reset(*calendar_machine, nop_rom)
            && calendar_machine->advance_calendar_oscillator_ticks(1'023U)
                == Jr800CalendarOperationStatus::ok,
        "Calendar ALARM summary runner setup failed"
    );
    const auto calendar_result = run_jr800_machine(
        *calendar_machine,
        Jr800RunLimits{38U, 32U}
    );
    const auto calendar_released =
        calendar_machine->advance_calendar_oscillator_ticks(1'024U);
    const auto released_calendar_state =
        calendar_machine->calendar_alarm_terminal_state();
    passed &= expect(
        calendar_result.completed_limit()
            && calendar_result.instructions_completed == 38U
            && calendar_result.calendar_alarm_terminal.connected
            && calendar_result.calendar_alarm_terminal.pull_low == true
            && calendar_released == Jr800CalendarOperationStatus::ok
            && released_calendar_state.connected
            && released_calendar_state.pull_low == false
            && calendar_result.calendar_alarm_terminal.pull_low == true,
        "Calendar ALARM run summary did not retain its terminal snapshot"
    );

    constexpr std::array<std::uint8_t, 13U> timer_output_program{
        0x86U, 0x01U,
        0x97U, 0x08U,
        0x86U, 0x02U,
        0x97U, 0x01U,
        0xCCU, 0xFFU, 0xFCU,
        0xDDU, 0x09U,
    };
    Jr800Machine timer_output_machine;
    const auto timer_output_rom = make_rom(timer_output_program);
    passed &= expect(
        load_and_reset(timer_output_machine, timer_output_rom),
        "Timer-output summary runner setup failed"
    );
    const auto unknown_timer_output_result = run_jr800_machine(
        timer_output_machine,
        Jr800RunLimits{4U, 32U}
    );
    const auto timer_output_result = run_jr800_machine(
        timer_output_machine,
        Jr800RunLimits{2U, 32U}
    );
    const auto timer_output_reset =
        timer_output_machine.initialize_from_reset_entry();
    const auto reset_timer_output =
        timer_output_machine.port2_timer_output_state();
    passed &= expect(
        unknown_timer_output_result.completed_limit()
            && unknown_timer_output_result.port2_timer_output.output_enabled
            && !unknown_timer_output_result.port2_timer_output.level
                .has_value()
            && unknown_timer_output_result.port2_timer_output_coverage.disabled
            && unknown_timer_output_result.port2_timer_output_coverage.unknown
            && !unknown_timer_output_result.port2_timer_output_coverage.low
            && !unknown_timer_output_result.port2_timer_output_coverage.high
            && timer_output_result.completed_limit()
            && timer_output_result.port2_timer_output.output_enabled
            && timer_output_result.port2_timer_output.level == true
            && !timer_output_result.port2_timer_output_coverage.disabled
            && timer_output_result.port2_timer_output_coverage.unknown
            && !timer_output_result.port2_timer_output_coverage.low
            && timer_output_result.port2_timer_output_coverage.high
            && timer_output_reset.succeeded()
            && !reset_timer_output.output_enabled
            && !reset_timer_output.level.has_value()
            && timer_output_result.port2_timer_output.output_enabled
            && timer_output_result.port2_timer_output.level == true
            && unknown_timer_output_result.port2_timer_output_coverage.unknown
            && !unknown_timer_output_result.port2_timer_output_coverage.high,
        "Timer-output run summary did not retain its endpoint snapshot"
    );

    constexpr std::array<std::uint8_t, 9U> keyboard_program{
        0xB6U, 0x0CU, 0x00U,
        0xB6U, 0x0CU, 0x01U,
        0xB6U, 0x0CU, 0x00U,
    };
    Jr800Machine keyboard_machine;
    const auto keyboard_rom = make_rom(keyboard_program);
    passed &= expect(
        load_and_reset(keyboard_machine, keyboard_rom)
            && keyboard_machine.set_keyboard_bus_response(
                0x0C00U,
                0xFFU,
                true
            )
            && keyboard_machine.set_keyboard_bus_response(
                0x0C01U,
                0xFFU,
                true
            ),
        "Keyboard summary runner setup failed"
    );
    const auto keyboard_result = run_jr800_machine(
        keyboard_machine,
        Jr800RunLimits{3U, 32U}
    );
    passed &= expect(
        keyboard_result.completed_limit()
            && keyboard_result.keyboard_activity.read_attempts == 3U
            && keyboard_result.keyboard_activity.distinct_addresses == 2U,
        "Keyboard aggregate summary exposed the wrong bounds"
    );

    Jr800Machine unknown_keyboard_machine;
    passed &= expect(
        load_and_reset(unknown_keyboard_machine, keyboard_rom),
        "Unknown-keyboard summary runner setup failed"
    );
    const auto unknown_keyboard_result = run_jr800_machine(
        unknown_keyboard_machine,
        Jr800RunLimits{1U, 32U}
    );
    passed &= expect(
        unknown_keyboard_result.stop_reason == Jr800RunStopReason::cpu_fault
            && unknown_keyboard_result.bus_fault
                == BusFault::uninitialized_read
            && unknown_keyboard_result.fault_region
                == Jr800MemoryRegion::keyboard
            && unknown_keyboard_result.keyboard_activity.read_attempts == 1U
            && unknown_keyboard_result.keyboard_activity.distinct_addresses
                == 1U,
        "Faulting keyboard read was missing from aggregate summary"
    );

    constexpr std::array<std::uint8_t, 20U> lcd_program{
        0x86U, 0x3EU,
        0xB7U, 0x0AU, 0x01U,
        0x86U, 0x39U,
        0xB7U, 0x0AU, 0x01U,
        0x86U, 0x00U,
        0xB7U, 0x0AU, 0x01U,
        0x86U, 0x01U,
        0xB7U, 0x0BU, 0x01U,
    };
    Jr800Machine lcd_machine(Jr800ExperimentalMachineConfiguration{
        .internal_ram = {},
        .lcd = Jr800ExperimentalLcdConfiguration{0U},
        .memory = {},
        .calendar = {},
    });
    const auto lcd_rom = make_rom(lcd_program);
    passed &= expect(
        load_and_reset(lcd_machine, lcd_rom),
        "LCD summary runner setup failed"
    );
    const auto lcd_result = run_jr800_machine(
        lcd_machine,
        Jr800RunLimits{8U, 32U}
    );
    const auto lcd_dot_count = lcd_result.lcd_panel.has_value()
        ? lcd_result.lcd_panel->unknown_dots
            + lcd_result.lcd_panel->off_dots
            + lcd_result.lcd_panel->on_dots
        : 0U;
    passed &= expect(
        lcd_result.completed_limit()
            && lcd_result.lcd_substituted_data_reads == 0U
            && lcd_result.lcd_panel.has_value()
            && lcd_dot_count == 192U * 64U
            && lcd_result.lcd_panel->unknown_dots != 0U
            && lcd_result.lcd_panel->off_dots != 0U
            && lcd_result.lcd_panel->on_dots != 0U,
        "LCD aggregate summary lost a dot state or panel bound"
    );

    constexpr std::array<std::uint8_t, 3U> reserved_read_program{
        0xB6U,
        0x10U,
        0x00U,
    };
    Jr800Machine fault_machine;
    const auto fault_rom = make_rom(reserved_read_program);
    passed &= expect(
        load_and_reset(fault_machine, fault_rom),
        "Fault runner setup failed"
    );
    const auto fault_result = run_jr800_machine(
        fault_machine,
        Jr800RunLimits{1U, 32U}
    );
    passed &= expect(
        fault_result.stop_reason == Jr800RunStopReason::cpu_fault
            && fault_result.instructions_completed == 0U
            && fault_result.execution_cycles == 0U
            && fault_result.cpu_fault == CpuFault::bus_access
            && fault_result.bus_fault == BusFault::unsupported_access
            && fault_result.fault_access == AccessKind::data_read
            && fault_result.fault_region == Jr800MemoryRegion::reserved
            && fault_result.state_fault == CpuStatePart::none,
        "Privacy-bounded bus-fault summary differs"
    );

    constexpr std::array<std::uint8_t, 13U> sleep_program{
        0x86U, 0x00U,
        0x97U, 0x0BU,
        0x86U, 0x20U,
        0x97U, 0x0CU,
        0x86U, 0x08U,
        0x97U, 0x08U,
        0x1AU,
    };
    Jr800Machine bounded_sleep_machine;
    const auto sleep_rom = make_rom(sleep_program);
    passed &= expect(
        load_and_reset(bounded_sleep_machine, sleep_rom),
        "Bounded-sleep runner setup failed"
    );
    const auto bounded_sleep_result = run_jr800_machine(
        bounded_sleep_machine,
        Jr800RunLimits{8U, 12U}
    );
    passed &= expect(
        bounded_sleep_result.stop_reason
                == Jr800RunStopReason::suspended_cycle_limit
            && bounded_sleep_result.instructions_completed == 7U
            && bounded_sleep_result.sleep_entries == 1U
            && bounded_sleep_result.sleep_resumes == 0U
            && bounded_sleep_result.suspended_cycles == 12U
            && bounded_sleep_result.execution_cycles == 31U
            && bounded_sleep_result.cpu_fault == CpuFault::none,
        "Suspended-cycle limit did not stop before the timer boundary"
    );

    Jr800Machine waking_sleep_machine;
    passed &= expect(
        load_and_reset(waking_sleep_machine, sleep_rom),
        "Waking-sleep runner setup failed"
    );
    const auto waking_sleep_result = run_jr800_machine(
        waking_sleep_machine,
        Jr800RunLimits{8U, 32U}
    );
    passed &= expect(
        waking_sleep_result.stop_reason
                == Jr800RunStopReason::instruction_limit
            && waking_sleep_result.instructions_completed == 8U
            && waking_sleep_result.sleep_entries == 1U
            && waking_sleep_result.sleep_resumes == 1U
            && waking_sleep_result.interrupt_entries == 0U
            && waking_sleep_result.suspended_cycles == 13U
            && waking_sleep_result.execution_cycles == 33U
            && waking_sleep_result.cpu_fault == CpuFault::none,
        "Timer wake did not resume bounded headless execution"
    );

    constexpr std::array<std::uint8_t, 22U> interrupt_program{
        0x8EU, 0x5FU, 0xFFU,
        0xCEU, 0x00U, 0x00U,
        0x5FU,
        0x4FU,
        0x06U,
        0x86U, 0x00U,
        0x97U, 0x0BU,
        0x86U, 0x40U,
        0x97U, 0x0CU,
        0x86U, 0x08U,
        0x97U, 0x08U,
        0x1AU,
    };
    Jr800Machine interrupt_machine;
    auto interrupt_rom = make_rom(interrupt_program);
    interrupt_rom[0x7FF4U] = 0x81U;
    interrupt_rom[0x7FF5U] = 0x00U;
    passed &= expect(
        load_and_reset(interrupt_machine, interrupt_rom),
        "Interrupt runner setup failed"
    );
    const auto interrupt_result = run_jr800_machine(
        interrupt_machine,
        Jr800RunLimits{13U, 64U}
    );
    passed &= expect(
        interrupt_result.stop_reason == Jr800RunStopReason::instruction_limit
            && interrupt_result.instructions_completed == 13U
            && interrupt_result.sleep_entries == 1U
            && interrupt_result.sleep_resumes == 0U
            && interrupt_result.interrupt_entries == 1U
            && interrupt_result.timer_input_capture_interrupts == 0U
            && interrupt_result.timer_output_compare_interrupts == 1U
            && interrupt_result.timer_overflow_interrupts == 0U
            && interrupt_result.serial_interrupts == 0U
            && interrupt_result.instructions_after_last_interrupt == 1U
            && interrupt_result.last_interrupt_target_region
                == Jr800MemoryRegion::standard_rom
            && interrupt_result.suspended_cycles == 36U
            && interrupt_result.execution_cycles == 77U
            && interrupt_result.cpu_fault == CpuFault::none,
        "Unmasked timer interrupt was not counted separately"
    );

    return passed ? 0 : 1;
}
