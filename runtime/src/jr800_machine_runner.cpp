// SPDX-License-Identifier: MIT

#include "jr800/runtime/jr800_machine_runner.hpp"

#include <cstddef>

#include "jr800/core/jr800_lcd.hpp"
#include "jr800/isa/instruction_metadata.hpp"

namespace jr800::runtime {
namespace {

void record_interrupt_source(
    core::InterruptSource source,
    Jr800RunSummary& summary
) noexcept {
    switch (source) {
    case core::InterruptSource::timer_input_capture:
        ++summary.timer_input_capture_interrupts;
        break;
    case core::InterruptSource::timer_output_compare:
        ++summary.timer_output_compare_interrupts;
        break;
    case core::InterruptSource::timer_overflow:
        ++summary.timer_overflow_interrupts;
        break;
    case core::InterruptSource::serial:
        ++summary.serial_interrupts;
        break;
    case core::InterruptSource::none:
        break;
    }
}

void record_completed_step(
    const core::StepResult& step,
    const core::CpuState& state,
    Jr800RunSummary& summary
) noexcept {
    switch (step.kind) {
    case core::StepKind::instruction:
        ++summary.instructions_completed;
        if (summary.interrupt_entries != 0U) {
            ++summary.instructions_after_last_interrupt;
        }
        if (state.execution_state == core::CpuExecutionState::sleeping) {
            ++summary.sleep_entries;
        } else if (state.execution_state
                   == core::CpuExecutionState::waiting_for_interrupt) {
            ++summary.wait_entries;
        }
        break;
    case core::StepKind::interrupt_entry:
        ++summary.interrupt_entries;
        summary.instructions_after_last_interrupt = 0U;
        summary.last_interrupt_target_region =
            core::jr800_memory_region(step.pc_after);
        record_interrupt_source(step.interrupt_source, summary);
        break;
    case core::StepKind::sleep_resume:
        ++summary.sleep_resumes;
        break;
    case core::StepKind::dormant:
        break;
    }
}

void record_fault(
    const core::StepResult& step,
    Jr800RunSummary& summary
) noexcept {
    summary.stop_reason = Jr800RunStopReason::cpu_fault;
    summary.cpu_fault = step.fault;
    summary.bus_fault = step.bus_fault;
    summary.state_fault = step.state_fault;
    if (step.fault == core::CpuFault::bus_access) {
        summary.fault_access = step.fault_access;
        summary.fault_region = core::jr800_memory_region(step.fault_address);
    }
}

void record_port2_timer_output(
    const core::Jr800Machine& machine,
    Jr800RunSummary& summary
) noexcept {
    const auto state = machine.port2_timer_output_state();
    summary.port2_timer_output = state;
    if (!state.output_enabled) {
        summary.port2_timer_output_coverage.disabled = true;
    } else if (!state.level.has_value()) {
        summary.port2_timer_output_coverage.unknown = true;
    } else if (*state.level) {
        summary.port2_timer_output_coverage.high = true;
    } else {
        summary.port2_timer_output_coverage.low = true;
    }
}

void record_lcd_summary(
    const core::Jr800Machine& machine,
    Jr800RunSummary& summary
) noexcept {
    summary.lcd_substituted_data_reads =
        machine.lcd_substituted_data_read_count();
    if (!summary.lcd_substituted_data_reads.has_value()) {
        return;
    }

    Jr800LcdPanelSummary panel;
    for (std::size_t row = 0U; row < core::Jr800Lcd::panel_height; ++row) {
        for (
            std::size_t column = 0U;
            column < core::Jr800Lcd::panel_width;
            ++column
        ) {
            const auto dot = machine.lcd_panel_dot(column, row);
            if (!dot.has_value()) {
                ++panel.unknown_dots;
            } else if (*dot) {
                ++panel.on_dots;
            } else {
                ++panel.off_dots;
            }
        }
    }
    summary.lcd_panel = panel;
}

}  // namespace

Jr800RunSummary run_jr800_machine(
    core::Jr800Machine& machine,
    Jr800RunLimits limits
) {
    Jr800RunSummary summary;
    summary.calendar_alarm_terminal =
        machine.calendar_alarm_terminal_state();
    record_port2_timer_output(machine, summary);
    if (limits.instructions == 0U || limits.suspended_cycles == 0U) {
        return summary;
    }
    if (machine.execution().cpu().profile()
        != isa::CpuProfile::hd6301v1) {
        summary.stop_reason = Jr800RunStopReason::machine_not_initialized;
        return summary;
    }

    machine.clear_keyboard_activity();
    const auto initial_cycle_count =
        machine.execution().cpu().state().cycle_count;
    while (summary.instructions_completed < limits.instructions) {
        const auto step = machine.execution().step_instruction();
        record_port2_timer_output(machine, summary);
        if (step.step_completed) {
            record_completed_step(
                step,
                machine.execution().cpu().state(),
                summary
            );
        }
        if (step.fault != core::CpuFault::none) {
            record_fault(step, summary);
            break;
        }
        if (step.step_completed) {
            continue;
        }

        const auto suspended = machine.execution().advance_suspended_cycles(
            limits.suspended_cycles
        );
        record_port2_timer_output(machine, summary);
        summary.suspended_cycles += suspended.cycles_elapsed;
        if (suspended.bus_fault != core::BusFault::none) {
            summary.stop_reason = Jr800RunStopReason::cpu_fault;
            summary.cpu_fault = core::CpuFault::bus_advance;
            summary.bus_fault = suspended.bus_fault;
            break;
        }
        if (!suspended.interrupt_request.known
            || suspended.interrupt_request.asserted()) {
            continue;
        }
        summary.stop_reason = Jr800RunStopReason::suspended_cycle_limit;
        break;
    }

    if (summary.instructions_completed == limits.instructions) {
        summary.stop_reason = Jr800RunStopReason::instruction_limit;
    }
    summary.execution_cycles =
        machine.execution().cpu().state().cycle_count - initial_cycle_count;
    summary.keyboard_activity = machine.keyboard_activity();
    summary.calendar_alarm_terminal =
        machine.calendar_alarm_terminal_state();
    record_port2_timer_output(machine, summary);
    record_lcd_summary(machine, summary);
    return summary;
}

}  // namespace jr800::runtime
