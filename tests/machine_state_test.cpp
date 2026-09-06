// SPDX-License-Identifier: MIT
#include "jr800/core/jr800_machine.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace jr800::core;
    const auto require = [](bool ok) { if (!ok) throw std::runtime_error("Machine checkpoint test failed"); };
    Jr800ExperimentalMachineConfiguration config;
    config.internal_ram = Jr800ExperimentalInternalRamConfiguration{0};
    config.memory = Jr800ExperimentalMemoryConfiguration{0, 0};
    config.lcd = Jr800ExperimentalLcdConfiguration{0};
    config.calendar = Jr800ExperimentalCalendarConfiguration{};
    Jr800Machine machine(config);
    std::array<std::uint8_t,32768> rom{};
    require(machine.load_logical_rom(rom) == Jr800MemoryStatus::ok);
    const std::array<std::uint8_t,7> program{0x86,1,0x97,0x80,0x4c,0x20,0xfb};
    require(machine.host_load_ram(0x2800, program) == Jr800MemoryStatus::ok);
    machine.execution().cpu().initialize(jr800::isa::CpuProfile::hd6301v1, 0x2800, 0x5fff);
    for (int i = 0; i < 37; ++i) require(machine.execution().step_instruction().succeeded());
    const auto saved = machine.save_state();
    const auto cpu = machine.execution().cpu().state();
    for (int i = 0; i < 53; ++i) require(machine.execution().step_instruction().succeeded());
    const auto expected = machine.save_state();
    machine.restore_state(saved);
    require(machine.execution().cpu().state() == cpu);
    require(machine.save_state() == saved);
    for (int i = 0; i < 53; ++i) require(machine.execution().step_instruction().succeeded());
    require(machine.save_state() == expected);
    auto broken = saved; broken.pop_back();
    bool rejected = false;
    try { machine.restore_state(broken); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && machine.save_state() == expected);
    // Calendar state is independent: changing time must not change the checkpoint.
    const auto before_clock = machine.save_state();
    require(machine.set_calendar_datetime({2026,9,6,12,34,56}) == Jr800CalendarOperationStatus::ok);
    require(machine.save_state() == before_clock);
    std::array<std::optional<std::uint8_t>,16> clock_before;
    for (std::size_t i = 0; i < clock_before.size(); ++i)
        clock_before[i] = machine.execution().inspect8(static_cast<std::uint16_t>(0x0600U + i)).value;
    require(clock_before[0] == 6);
    const auto calendar_before = machine.calendar_alarm_terminal_state();
    machine.restore_state(saved);
    require(machine.calendar_alarm_terminal_state() == calendar_before);
    for (std::size_t i = 0; i < clock_before.size(); ++i)
        require(machine.execution().inspect8(static_cast<std::uint16_t>(0x0600U + i)).value == clock_before[i]);
    std::cout << "Machine state round trip, deterministic continuation and RTC separation passed\n";
}
