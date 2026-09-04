// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>

#include "jr800/core/bus.hpp"
#include "jr800/core/cpu.hpp"
#include "jr800/core/jr800_machine.hpp"
#include "jr800/core/jr800_memory.hpp"

namespace jr800::runtime {

enum class Jr800RunStopReason : std::uint8_t {
    instruction_limit,
    cpu_fault,
    suspended_cycle_limit,
    invalid_limits,
    machine_not_initialized,
};

struct Jr800RunLimits {
    std::uint64_t instructions{100'000U};
    std::uint32_t suspended_cycles{65'536U};

    bool operator==(const Jr800RunLimits&) const = default;
};

struct Jr800LcdPanelSummary {
    std::uint64_t unknown_dots{};
    std::uint64_t off_dots{};
    std::uint64_t on_dots{};

    bool operator==(const Jr800LcdPanelSummary&) const = default;
};

struct Jr800Port2TimerOutputCoverage {
    bool disabled{};
    bool unknown{};
    bool low{};
    bool high{};

    bool operator==(const Jr800Port2TimerOutputCoverage&) const = default;
};

struct Jr800RunSummary {
    Jr800RunStopReason stop_reason{Jr800RunStopReason::invalid_limits};
    std::uint64_t instructions_completed{};
    std::uint64_t interrupt_entries{};
    std::uint64_t timer_input_capture_interrupts{};
    std::uint64_t timer_output_compare_interrupts{};
    std::uint64_t timer_overflow_interrupts{};
    std::uint64_t serial_interrupts{};
    std::uint64_t instructions_after_last_interrupt{};
    std::optional<core::Jr800MemoryRegion> last_interrupt_target_region;
    std::uint64_t sleep_entries{};
    std::uint64_t wait_entries{};
    std::uint64_t sleep_resumes{};
    std::uint64_t suspended_cycles{};
    std::uint64_t execution_cycles{};
    core::CpuFault cpu_fault{core::CpuFault::none};
    core::BusFault bus_fault{core::BusFault::none};
    std::optional<core::AccessKind> fault_access;
    std::optional<core::Jr800MemoryRegion> fault_region;
    core::CpuStatePart state_fault{core::CpuStatePart::none};
    core::Jr800KeyboardActivity keyboard_activity;
    core::Jr800CalendarAlarmTerminalState calendar_alarm_terminal;
    core::Hd6301v1Port2TimerOutputState port2_timer_output;
    Jr800Port2TimerOutputCoverage port2_timer_output_coverage;
    std::optional<std::uint64_t> lcd_substituted_data_reads;
    std::optional<Jr800LcdPanelSummary> lcd_panel;

    [[nodiscard]] bool completed_limit() const noexcept {
        return stop_reason == Jr800RunStopReason::instruction_limit;
    }

    bool operator==(const Jr800RunSummary&) const = default;
};

// Runs an initialized JR-800 machine without retaining ROM-derived instruction
// bytes, addresses, register values, per-address keyboard activity, timer
// transitions, or per-dot display contents in the returned data.
[[nodiscard]] Jr800RunSummary run_jr800_machine(
    core::Jr800Machine& machine,
    Jr800RunLimits limits = {}
);

}  // namespace jr800::runtime
