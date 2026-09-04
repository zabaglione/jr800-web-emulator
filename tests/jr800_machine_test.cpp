// SPDX-License-Identifier: MIT

#include <cstdint>
#include <iostream>
#include <iterator>
#include <string_view>
#include <type_traits>
#include <vector>

#include "jr800/core/bus.hpp"
#include "jr800/core/cpu.hpp"
#include "jr800/core/jr800_machine.hpp"
#include "jr800/core/jr800_memory.hpp"
#include "jr800/debugger/debugger.hpp"
#include "jr800/isa/instruction_metadata.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    using jr800::core::AccessKind;
    using jr800::core::BusFault;
    using jr800::core::CpuFault;
    using jr800::core::CpuRegister;
    using jr800::core::CpuStatePart;
    using jr800::core::Jr800ExperimentalMachineConfiguration;
    using jr800::core::Jr800ExperimentalResetStateConfiguration;
    using jr800::core::Jr800Machine;
    using jr800::core::Jr800MemoryStatus;
    using jr800::debugger::Debugger;
    using jr800::debugger::StopReason;
    using jr800::isa::CpuProfile;

    static_assert(!std::is_copy_constructible_v<Jr800Machine>);
    static_assert(!std::is_copy_assignable_v<Jr800Machine>);
    static_assert(!std::is_move_constructible_v<Jr800Machine>);
    static_assert(!std::is_move_assignable_v<Jr800Machine>);

    bool passed = true;
    Jr800Machine machine;
    const auto missing_state_before = machine.execution().cpu().state();
    const auto missing_reset_entry = machine.inspect_reset_entry();
    const auto missing_reset_initialization =
        machine.initialize_from_reset_entry();
    passed &= expect(
        !missing_reset_entry.succeeded()
            && missing_reset_entry.fault
                == BusFault::backing_store_unavailable
            && missing_reset_entry.fault_address == 0xFFFEU
            && !missing_reset_entry.entry.has_value()
            && missing_reset_initialization == missing_reset_entry
            && machine.execution().cpu().state() == missing_state_before
            && machine.execution().cpu().profile()
                == CpuProfile::jr800_unresolved,
        "Missing JR-800 ROM did not fail reset-entry inspection"
    );

    std::vector<std::uint8_t> rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    rom[rom.size() - 2U] = 0x81U;
    rom[rom.size() - 1U] = 0x23U;
    passed &= expect(
        machine.load_logical_rom(rom) == Jr800MemoryStatus::ok,
        "JR-800 logical ROM load failed"
    );
    machine.execution().initialize(
        CpuProfile::hd6301v1,
        0x8000U,
        0x0000U
    );
    const auto state_before_reset_inspection = machine.execution().cpu().state();

    const auto reset_entry = machine.inspect_reset_entry();
    const auto expected_known_condition_code = static_cast<std::uint8_t>(
        jr800::core::fixed_condition_code_bits
        | jr800::core::condition_mask(
            jr800::core::ConditionCode::interrupt_mask
        )
    );
    passed &= expect(
        reset_entry.succeeded()
            && reset_entry.entry->program_counter == 0x8123U
            && reset_entry.entry->condition_code_value
                == expected_known_condition_code
            && reset_entry.entry->condition_code_known_mask
                == expected_known_condition_code
            && machine.execution().cpu().state()
                == state_before_reset_inspection,
        "JR-800 reset-entry inspection differs"
    );

    Debugger debugger;
    passed &= expect(
        debugger.attach(machine.execution()),
        "Debugger did not attach to JR-800 machine"
    );
    const auto observed_reset_entry = machine.inspect_reset_entry();
    passed &= expect(
        observed_reset_entry == reset_entry
            && debugger.memory_access_size() == 0U,
        "JR-800 reset-entry inspection produced trace side effects"
    );

    const auto initialized_reset_entry = machine.initialize_from_reset_entry();
    const auto& reset_state = machine.execution().cpu().state();
    passed &= expect(
        initialized_reset_entry == reset_entry
            && machine.execution().cpu().profile() == CpuProfile::hd6301v1
            && reset_state.pc == 0x8123U
            && reset_state.condition_code == expected_known_condition_code
            && reset_state.cycle_count == 0U
            && reset_state.knowledge.knows(CpuRegister::program_counter)
            && !reset_state.knowledge.knows(CpuRegister::stack_pointer)
            && !reset_state.knowledge.knows(CpuRegister::index_register)
            && !reset_state.knowledge.knows(CpuRegister::accumulator_a)
            && !reset_state.knowledge.knows(CpuRegister::accumulator_b)
            && reset_state.knowledge.condition_code
                == expected_known_condition_code,
        "JR-800 reset entry assigned unknown CPU state"
    );
    const auto reset_nop = debugger.step();
    passed &= expect(
        reset_nop.reason == StopReason::step_complete
            && machine.execution().cpu().state().pc == 0x8124U
            && machine.execution().cpu().state().cycle_count == 1U
            && !machine.execution().cpu().state().knowledge.knows(
                CpuRegister::stack_pointer
            ),
        "Known-only reset execution did not preserve unknown state"
    );

    Jr800Machine configured_reset_machine(
        Jr800ExperimentalMachineConfiguration{},
        Jr800ExperimentalResetStateConfiguration{
            .stack_pointer = 0x2345U,
            .index_register = 0x3456U,
            .accumulator_a = 0x67U,
            .accumulator_b = 0x89U,
            .half_carry = true,
            .negative = false,
            .zero = true,
            .overflow = false,
            .carry = true,
        }
    );
    passed &= expect(
        configured_reset_machine.load_logical_rom(rom)
                == Jr800MemoryStatus::ok
            && configured_reset_machine.initialize_from_reset_entry()
                .succeeded(),
        "Configured reset-state setup failed"
    );
    const auto& configured_reset_state =
        configured_reset_machine.execution().cpu().state();
    passed &= expect(
        configured_reset_state.pc == 0x8123U
            && configured_reset_state.sp == 0x2345U
            && configured_reset_state.x == 0x3456U
            && configured_reset_state.a == 0x67U
            && configured_reset_state.b == 0x89U
            && configured_reset_state.condition_code == 0xF5U
            && configured_reset_state.knowledge.registers
                == jr800::core::all_cpu_registers
            && configured_reset_state.knowledge.condition_code == 0xFFU,
        "Configured reset state changed a value or knownness bit"
    );

    Jr800ExperimentalResetStateConfiguration partial_reset_configuration;
    partial_reset_configuration.index_register = 0x4567U;
    Jr800Machine partial_reset_machine(
        Jr800ExperimentalMachineConfiguration{},
        partial_reset_configuration
    );
    passed &= expect(
        partial_reset_machine.load_logical_rom(rom) == Jr800MemoryStatus::ok
            && partial_reset_machine.initialize_from_reset_entry().succeeded()
            && partial_reset_machine.execution().cpu().state().x == 0x4567U
            && partial_reset_machine.execution().cpu().state()
                    .knowledge.registers
                == static_cast<std::uint8_t>(
                    jr800::core::register_mask(CpuRegister::program_counter)
                    | jr800::core::register_mask(CpuRegister::index_register)
                )
            && partial_reset_machine.execution().cpu().state()
                    .knowledge.condition_code
                == expected_known_condition_code,
        "Partial reset-state experiment made an omitted field known"
    );
    debugger.clear_history();
    machine.execution().initialize(
        CpuProfile::hd6301v1,
        0x8000U,
        0x0000U
    );

    const auto disassembled = debugger.disassemble(0x8000U);
    passed &= expect(
        disassembled.has_value() && disassembled->text == "NOP"
            && debugger.memory_access_size() == 0U,
        "Side-effect-free JR-800 disassembly differs"
    );
    passed &= expect(
        !debugger.disassemble(0x0A00U).has_value()
            && debugger.memory_access_size() == 0U,
        "Unsupported device inspection produced data or trace events"
    );

    Jr800Machine wrapped_disassembly_machine;
    std::vector<std::uint8_t> wrapped_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    wrapped_disassembly_rom[wrapped_disassembly_rom.size() - 2U] = 0x20U;
    wrapped_disassembly_rom[wrapped_disassembly_rom.size() - 1U] = 0xFEU;
    passed &= expect(
        wrapped_disassembly_machine.load_logical_rom(wrapped_disassembly_rom)
            == Jr800MemoryStatus::ok,
        "Wrapped disassembly setup failed"
    );
    wrapped_disassembly_machine.execution().initialize(
        CpuProfile::hd6301v1,
        0xFFFEU,
        0x0000U
    );
    Debugger wrapped_disassembler;
    passed &= expect(
        wrapped_disassembler.attach(wrapped_disassembly_machine.execution()),
        "Wrapped disassembler attach failed"
    );
    const auto wrapped_disassembly = wrapped_disassembler.disassemble(0xFFFEU);
    passed &= expect(
        wrapped_disassembly.has_value()
            && wrapped_disassembly->supported
            && wrapped_disassembly->length == 2U
            && wrapped_disassembly->bytes[0] == 0x20U
            && wrapped_disassembly->bytes[1] == 0xFEU
            && wrapped_disassembly->bytes[2] == 0U
            && wrapped_disassembly->text == "BRA $FFFE"
            && wrapped_disassembler.memory_access_size() == 0U,
        "Disassembly inspected past the wrapped instruction boundary"
    );

    const auto nop = debugger.step();
    passed &= expect(
        nop.reason == StopReason::step_complete
            && machine.execution().cpu().state().pc == 0x8001U
            && machine.execution().cpu().state().cycle_count == 1U,
        "JR-800 ROM-backed CPU step failed"
    );

    machine.execution().initialize(
        CpuProfile::hd6301v1,
        0x0A00U,
        0x0000U
    );
    const auto fault = debugger.step();
    passed &= expect(
        fault.reason == StopReason::cpu_fault
            && fault.trigger_address == 0x0A00U
            && fault.step.fault == CpuFault::bus_access
            && fault.step.bus_fault == BusFault::unsupported_access
            && fault.step.fault_address == 0x0A00U
            && fault.step.fault_access == AccessKind::instruction_fetch
            && machine.execution().cpu().state().pc == 0x0A00U,
        "JR-800 bus fault did not propagate through debugger"
    );

    const auto history = debugger.history();
    passed &= expect(
        history.size() == 2U
            && history.back().fault == CpuFault::bus_access
            && history.back().bus_fault == BusFault::unsupported_access
            && history.back().fault_address == 0x0A00U
            && history.back().fault_access == AccessKind::instruction_fetch
            && history.back().access_count == 0U,
        "JR-800 bus fault history differs"
    );

    Jr800Machine reset_jump_machine;
    std::vector<std::uint8_t> reset_jump_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    reset_jump_rom[0U] = 0x7EU;
    reset_jump_rom[1U] = 0x81U;
    reset_jump_rom[2U] = 0x23U;
    reset_jump_rom[0x0123U] = 0x0FU;
    reset_jump_rom[0x0124U] = 0x8EU;
    reset_jump_rom[0x0125U] = 0x00U;
    reset_jump_rom[0x0126U] = 0xFFU;
    reset_jump_rom[0x0127U] = 0x4FU;
    reset_jump_rom[0x0128U] = 0x62U;
    reset_jump_rom[0x0129U] = 0x03U;
    reset_jump_rom[0x012AU] = 0x20U;
    reset_jump_rom[0x012BU] = 0xA6U;
    reset_jump_rom[0x012CU] = 0x20U;
    reset_jump_rom[0x012DU] = 0x4CU;
    reset_jump_rom[0x012EU] = 0x6FU;
    reset_jump_rom[0x012FU] = 0x20U;
    reset_jump_rom[0x0130U] = 0x08U;
    reset_jump_rom[0x0131U] = 0x8CU;
    reset_jump_rom[0x0132U] = 0x12U;
    reset_jump_rom[0x0133U] = 0x34U;
    reset_jump_rom[0x0134U] = 0x09U;
    reset_jump_rom[0x0135U] = 0xDFU;
    reset_jump_rom[0x0136U] = 0x80U;
    reset_jump_rom[reset_jump_rom.size() - 2U] = 0x80U;
    reset_jump_rom[reset_jump_rom.size() - 1U] = 0x00U;
    passed &= expect(
        reset_jump_machine.load_logical_rom(reset_jump_rom)
                == Jr800MemoryStatus::ok
            && reset_jump_machine.initialize_from_reset_entry().succeeded(),
        "Reset JMP setup failed"
    );
    Debugger reset_jump_debugger;
    passed &= expect(
        reset_jump_debugger.attach(reset_jump_machine.execution()),
        "Reset JMP debugger attach failed"
    );
    const auto reset_jump_disassembly = reset_jump_debugger.disassemble(0x8000U);
    const auto reset_jump = reset_jump_debugger.step();
    const auto& reset_jump_state = reset_jump_machine.execution().cpu().state();
    passed &= expect(
        reset_jump_disassembly.has_value()
            && reset_jump_disassembly->text == "JMP $8123"
            && reset_jump.reason == StopReason::step_complete
            && reset_jump.step.bytes_fetched == 3U
            && reset_jump.step.cycles == 3U
            && reset_jump_state.pc == 0x8123U
            && reset_jump_state.cycle_count == 3U
            && !reset_jump_state.knowledge.knows(CpuRegister::stack_pointer)
            && reset_jump_state.knowledge.condition_code
                == expected_known_condition_code,
        "Reset JMP consumed unknown state or decoded incorrectly"
    );
    const auto reset_sei_disassembly = reset_jump_debugger.disassemble(0x8123U);
    const auto reset_sei = reset_jump_debugger.step();
    const auto& reset_sei_state = reset_jump_machine.execution().cpu().state();
    passed &= expect(
        reset_sei_disassembly.has_value()
            && reset_sei_disassembly->text == "SEI"
            && reset_sei.reason == StopReason::step_complete
            && reset_sei.step.bytes_fetched == 1U
            && reset_sei.step.cycles == 1U
            && reset_sei_state.pc == 0x8124U
            && reset_sei_state.cycle_count == 4U
            && !reset_sei_state.knowledge.knows(CpuRegister::stack_pointer)
            && reset_sei_state.condition_code
                == expected_known_condition_code
            && reset_sei_state.knowledge.condition_code
                == expected_known_condition_code,
        "Reset SEI changed unknown state or decoded incorrectly"
    );
    const auto reset_lds_disassembly = reset_jump_debugger.disassemble(0x8124U);
    const auto reset_lds = reset_jump_debugger.step();
    const auto& reset_lds_state = reset_jump_machine.execution().cpu().state();
    const auto reset_lds_known_condition_code = static_cast<std::uint8_t>(
        expected_known_condition_code
        | jr800::core::condition_mask(jr800::core::ConditionCode::negative)
        | jr800::core::condition_mask(jr800::core::ConditionCode::zero)
        | jr800::core::condition_mask(jr800::core::ConditionCode::overflow)
    );
    passed &= expect(
        reset_lds_disassembly.has_value()
            && reset_lds_disassembly->text == "LDS #$00FF"
            && reset_lds.reason == StopReason::step_complete
            && reset_lds.step.bytes_fetched == 3U
            && reset_lds.step.cycles == 3U
            && reset_lds_state.pc == 0x8127U
            && reset_lds_state.sp == 0x00FFU
            && reset_lds_state.cycle_count == 7U
            && reset_lds_state.knowledge.knows(CpuRegister::stack_pointer)
            && reset_lds_state.condition_code
                == expected_known_condition_code
            && reset_lds_state.knowledge.condition_code
                == reset_lds_known_condition_code,
        "Reset LDS did not establish stack state or decode correctly"
    );
    const auto reset_clra_disassembly = reset_jump_debugger.disassemble(0x8127U);
    const auto reset_clra = reset_jump_debugger.step();
    const auto& reset_clra_state = reset_jump_machine.execution().cpu().state();
    const auto reset_clra_known_condition_code = static_cast<std::uint8_t>(
        reset_lds_known_condition_code
        | jr800::core::condition_mask(jr800::core::ConditionCode::carry)
    );
    const auto reset_clra_condition_code = static_cast<std::uint8_t>(
        expected_known_condition_code
        | jr800::core::condition_mask(jr800::core::ConditionCode::zero)
    );
    passed &= expect(
        reset_clra_disassembly.has_value()
            && reset_clra_disassembly->text == "CLRA"
            && reset_clra.reason == StopReason::step_complete
            && reset_clra.step.bytes_fetched == 1U
            && reset_clra.step.cycles == 1U
            && reset_clra_state.pc == 0x8128U
            && reset_clra_state.a == 0U
            && reset_clra_state.cycle_count == 8U
            && reset_clra_state.knowledge.knows(CpuRegister::accumulator_a)
            && reset_clra_state.condition_code
                == reset_clra_condition_code
            && reset_clra_state.knowledge.condition_code
                == reset_clra_known_condition_code,
        "Reset CLRA did not establish accumulator state or decode correctly"
    );
    const auto reset_oim_indexed_disassembly =
        reset_jump_debugger.disassemble(0x8128U);
    passed &= expect(
        reset_oim_indexed_disassembly.has_value()
            && reset_oim_indexed_disassembly->text == "OIM #$03, $20,X"
            && reset_jump_debugger.memory_access_size() == 8U,
        "Indexed OIM disassembly differs or produced trace side effects"
    );
    const auto reset_ldaa_indexed_disassembly =
        reset_jump_debugger.disassemble(0x812BU);
    passed &= expect(
        reset_ldaa_indexed_disassembly.has_value()
            && reset_ldaa_indexed_disassembly->text == "LDAA $20,X"
            && reset_jump_debugger.memory_access_size() == 8U,
        "Indexed LDAA disassembly differs or produced trace side effects"
    );
    const auto reset_inca_disassembly =
        reset_jump_debugger.disassemble(0x812DU);
    passed &= expect(
        reset_inca_disassembly.has_value()
            && reset_inca_disassembly->text == "INCA"
            && reset_jump_debugger.memory_access_size() == 8U,
        "INCA disassembly differs or produced trace side effects"
    );
    const auto reset_clr_indexed_disassembly =
        reset_jump_debugger.disassemble(0x812EU);
    passed &= expect(
        reset_clr_indexed_disassembly.has_value()
            && reset_clr_indexed_disassembly->text == "CLR $20,X"
            && reset_jump_debugger.memory_access_size() == 8U,
        "Indexed CLR disassembly differs or produced trace side effects"
    );
    const auto reset_inx_disassembly =
        reset_jump_debugger.disassemble(0x8130U);
    passed &= expect(
        reset_inx_disassembly.has_value()
            && reset_inx_disassembly->text == "INX"
            && reset_jump_debugger.memory_access_size() == 8U,
        "INX disassembly differs or produced trace side effects"
    );
    const auto reset_cpx_disassembly =
        reset_jump_debugger.disassemble(0x8131U);
    passed &= expect(
        reset_cpx_disassembly.has_value()
            && reset_cpx_disassembly->text == "CPX #$1234"
            && reset_jump_debugger.memory_access_size() == 8U,
        "CPX disassembly differs or produced trace side effects"
    );

    Jr800Machine cpx_direct_disassembly_machine;
    std::vector<std::uint8_t> cpx_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cpx_direct_disassembly_rom[0U] = 0x9CU;
    cpx_direct_disassembly_rom[1U] = 0x20U;
    cpx_direct_disassembly_rom[
        cpx_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    cpx_direct_disassembly_rom[
        cpx_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        cpx_direct_disassembly_machine.load_logical_rom(
            cpx_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && cpx_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct CPX disassembly setup failed"
    );
    Debugger cpx_direct_disassembler;
    passed &= expect(
        cpx_direct_disassembler.attach(
            cpx_direct_disassembly_machine.execution()
        ),
        "Direct CPX disassembler attach failed"
    );
    const auto cpx_direct_disassembly =
        cpx_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        cpx_direct_disassembly.has_value()
            && cpx_direct_disassembly->supported
            && cpx_direct_disassembly->text == "CPX $20"
            && cpx_direct_disassembler.memory_access_size() == 0U,
        "Direct CPX disassembly differs or produced trace side effects"
    );

    Jr800Machine cpx_indexed_disassembly_machine;
    std::vector<std::uint8_t> cpx_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cpx_indexed_disassembly_rom[0U] = 0xACU;
    cpx_indexed_disassembly_rom[1U] = 0x20U;
    cpx_indexed_disassembly_rom[
        cpx_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    cpx_indexed_disassembly_rom[
        cpx_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        cpx_indexed_disassembly_machine.load_logical_rom(
            cpx_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && cpx_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed CPX disassembly setup failed"
    );
    Debugger cpx_indexed_disassembler;
    passed &= expect(
        cpx_indexed_disassembler.attach(
            cpx_indexed_disassembly_machine.execution()
        ),
        "Indexed CPX disassembler attach failed"
    );
    const auto cpx_indexed_disassembly =
        cpx_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        cpx_indexed_disassembly.has_value()
            && cpx_indexed_disassembly->supported
            && cpx_indexed_disassembly->text == "CPX $20,X"
            && cpx_indexed_disassembler.memory_access_size() == 0U,
        "Indexed CPX disassembly differs or produced trace side effects"
    );

    Jr800Machine cpx_extended_disassembly_machine;
    std::vector<std::uint8_t> cpx_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cpx_extended_disassembly_rom[0U] = 0xBCU;
    cpx_extended_disassembly_rom[1U] = 0x81U;
    cpx_extended_disassembly_rom[2U] = 0x23U;
    cpx_extended_disassembly_rom[
        cpx_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    cpx_extended_disassembly_rom[
        cpx_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        cpx_extended_disassembly_machine.load_logical_rom(
            cpx_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && cpx_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended CPX disassembly setup failed"
    );
    Debugger cpx_extended_disassembler;
    passed &= expect(
        cpx_extended_disassembler.attach(
            cpx_extended_disassembly_machine.execution()
        ),
        "Extended CPX disassembler attach failed"
    );
    const auto cpx_extended_disassembly =
        cpx_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        cpx_extended_disassembly.has_value()
            && cpx_extended_disassembly->supported
            && cpx_extended_disassembly->text == "CPX $8123"
            && cpx_extended_disassembler.memory_access_size() == 0U,
        "Extended CPX disassembly differs or produced trace side effects"
    );

    Jr800Machine brn_disassembly_machine;
    std::vector<std::uint8_t> brn_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    brn_disassembly_rom[0U] = 0x21U;
    brn_disassembly_rom[1U] = 0xFEU;
    brn_disassembly_rom[brn_disassembly_rom.size() - 2U] = 0x80U;
    brn_disassembly_rom[brn_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        brn_disassembly_machine.load_logical_rom(brn_disassembly_rom)
                == Jr800MemoryStatus::ok
            && brn_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BRN disassembly setup failed"
    );
    Debugger brn_disassembler;
    passed &= expect(
        brn_disassembler.attach(brn_disassembly_machine.execution()),
        "BRN disassembler attach failed"
    );
    const auto brn_disassembly = brn_disassembler.disassemble(0x8000U);
    passed &= expect(
        brn_disassembly.has_value()
            && brn_disassembly->supported
            && brn_disassembly->text == "BRN $8000"
            && brn_disassembler.memory_access_size() == 0U,
        "BRN disassembly differs or produced trace side effects"
    );

    Jr800Machine bhi_disassembly_machine;
    std::vector<std::uint8_t> bhi_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bhi_disassembly_rom[0U] = 0x22U;
    bhi_disassembly_rom[1U] = 0xFEU;
    bhi_disassembly_rom[bhi_disassembly_rom.size() - 2U] = 0x80U;
    bhi_disassembly_rom[bhi_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bhi_disassembly_machine.load_logical_rom(bhi_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bhi_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BHI disassembly setup failed"
    );
    Debugger bhi_disassembler;
    passed &= expect(
        bhi_disassembler.attach(bhi_disassembly_machine.execution()),
        "BHI disassembler attach failed"
    );
    const auto bhi_disassembly = bhi_disassembler.disassemble(0x8000U);
    passed &= expect(
        bhi_disassembly.has_value()
            && bhi_disassembly->supported
            && bhi_disassembly->text == "BHI $8000"
            && bhi_disassembler.memory_access_size() == 0U,
        "BHI disassembly differs or produced trace side effects"
    );

    Jr800Machine bls_disassembly_machine;
    std::vector<std::uint8_t> bls_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bls_disassembly_rom[0U] = 0x23U;
    bls_disassembly_rom[1U] = 0xFEU;
    bls_disassembly_rom[bls_disassembly_rom.size() - 2U] = 0x80U;
    bls_disassembly_rom[bls_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bls_disassembly_machine.load_logical_rom(bls_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bls_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BLS disassembly setup failed"
    );
    Debugger bls_disassembler;
    passed &= expect(
        bls_disassembler.attach(bls_disassembly_machine.execution()),
        "BLS disassembler attach failed"
    );
    const auto bls_disassembly = bls_disassembler.disassemble(0x8000U);
    passed &= expect(
        bls_disassembly.has_value()
            && bls_disassembly->supported
            && bls_disassembly->text == "BLS $8000"
            && bls_disassembler.memory_access_size() == 0U,
        "BLS disassembly differs or produced trace side effects"
    );

    Jr800Machine bcc_disassembly_machine;
    std::vector<std::uint8_t> bcc_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bcc_disassembly_rom[0U] = 0x24U;
    bcc_disassembly_rom[1U] = 0xFEU;
    bcc_disassembly_rom[bcc_disassembly_rom.size() - 2U] = 0x80U;
    bcc_disassembly_rom[bcc_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bcc_disassembly_machine.load_logical_rom(bcc_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bcc_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BCC disassembly setup failed"
    );
    Debugger bcc_disassembler;
    passed &= expect(
        bcc_disassembler.attach(bcc_disassembly_machine.execution()),
        "BCC disassembler attach failed"
    );
    const auto bcc_disassembly = bcc_disassembler.disassemble(0x8000U);
    passed &= expect(
        bcc_disassembly.has_value()
            && bcc_disassembly->supported
            && bcc_disassembly->text == "BCC $8000"
            && bcc_disassembler.memory_access_size() == 0U,
        "BCC disassembly differs or produced trace side effects"
    );

    Jr800Machine bcs_disassembly_machine;
    std::vector<std::uint8_t> bcs_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bcs_disassembly_rom[0U] = 0x25U;
    bcs_disassembly_rom[1U] = 0xFEU;
    bcs_disassembly_rom[bcs_disassembly_rom.size() - 2U] = 0x80U;
    bcs_disassembly_rom[bcs_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bcs_disassembly_machine.load_logical_rom(bcs_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bcs_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BCS disassembly setup failed"
    );
    Debugger bcs_disassembler;
    passed &= expect(
        bcs_disassembler.attach(bcs_disassembly_machine.execution()),
        "BCS disassembler attach failed"
    );
    const auto bcs_disassembly = bcs_disassembler.disassemble(0x8000U);
    passed &= expect(
        bcs_disassembly.has_value()
            && bcs_disassembly->supported
            && bcs_disassembly->text == "BCS $8000"
            && bcs_disassembler.memory_access_size() == 0U,
        "BCS disassembly differs or produced trace side effects"
    );

    Jr800Machine beq_disassembly_machine;
    std::vector<std::uint8_t> beq_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    beq_disassembly_rom[0U] = 0x27U;
    beq_disassembly_rom[1U] = 0xFEU;
    beq_disassembly_rom[beq_disassembly_rom.size() - 2U] = 0x80U;
    beq_disassembly_rom[beq_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        beq_disassembly_machine.load_logical_rom(beq_disassembly_rom)
                == Jr800MemoryStatus::ok
            && beq_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BEQ disassembly setup failed"
    );
    Debugger beq_disassembler;
    passed &= expect(
        beq_disassembler.attach(beq_disassembly_machine.execution()),
        "BEQ disassembler attach failed"
    );
    const auto beq_disassembly = beq_disassembler.disassemble(0x8000U);
    passed &= expect(
        beq_disassembly.has_value()
            && beq_disassembly->supported
            && beq_disassembly->text == "BEQ $8000"
            && beq_disassembler.memory_access_size() == 0U,
        "BEQ disassembly differs or produced trace side effects"
    );

    Jr800Machine bvc_disassembly_machine;
    std::vector<std::uint8_t> bvc_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bvc_disassembly_rom[0U] = 0x28U;
    bvc_disassembly_rom[1U] = 0xFEU;
    bvc_disassembly_rom[bvc_disassembly_rom.size() - 2U] = 0x80U;
    bvc_disassembly_rom[bvc_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bvc_disassembly_machine.load_logical_rom(bvc_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bvc_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BVC disassembly setup failed"
    );
    Debugger bvc_disassembler;
    passed &= expect(
        bvc_disassembler.attach(bvc_disassembly_machine.execution()),
        "BVC disassembler attach failed"
    );
    const auto bvc_disassembly = bvc_disassembler.disassemble(0x8000U);
    passed &= expect(
        bvc_disassembly.has_value()
            && bvc_disassembly->supported
            && bvc_disassembly->text == "BVC $8000"
            && bvc_disassembler.memory_access_size() == 0U,
        "BVC disassembly differs or produced trace side effects"
    );

    Jr800Machine bvs_disassembly_machine;
    std::vector<std::uint8_t> bvs_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bvs_disassembly_rom[0U] = 0x29U;
    bvs_disassembly_rom[1U] = 0xFEU;
    bvs_disassembly_rom[bvs_disassembly_rom.size() - 2U] = 0x80U;
    bvs_disassembly_rom[bvs_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bvs_disassembly_machine.load_logical_rom(bvs_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bvs_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BVS disassembly setup failed"
    );
    Debugger bvs_disassembler;
    passed &= expect(
        bvs_disassembler.attach(bvs_disassembly_machine.execution()),
        "BVS disassembler attach failed"
    );
    const auto bvs_disassembly = bvs_disassembler.disassemble(0x8000U);
    passed &= expect(
        bvs_disassembly.has_value()
            && bvs_disassembly->supported
            && bvs_disassembly->text == "BVS $8000"
            && bvs_disassembler.memory_access_size() == 0U,
        "BVS disassembly differs or produced trace side effects"
    );

    Jr800Machine bmi_disassembly_machine;
    std::vector<std::uint8_t> bmi_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bmi_disassembly_rom[0U] = 0x2BU;
    bmi_disassembly_rom[1U] = 0xFEU;
    bmi_disassembly_rom[bmi_disassembly_rom.size() - 2U] = 0x80U;
    bmi_disassembly_rom[bmi_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bmi_disassembly_machine.load_logical_rom(bmi_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bmi_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BMI disassembly setup failed"
    );
    Debugger bmi_disassembler;
    passed &= expect(
        bmi_disassembler.attach(bmi_disassembly_machine.execution()),
        "BMI disassembler attach failed"
    );
    const auto bmi_disassembly = bmi_disassembler.disassemble(0x8000U);
    passed &= expect(
        bmi_disassembly.has_value()
            && bmi_disassembly->supported
            && bmi_disassembly->text == "BMI $8000"
            && bmi_disassembler.memory_access_size() == 0U,
        "BMI disassembly differs or produced trace side effects"
    );

    Jr800Machine bge_disassembly_machine;
    std::vector<std::uint8_t> bge_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bge_disassembly_rom[0U] = 0x2CU;
    bge_disassembly_rom[1U] = 0xFEU;
    bge_disassembly_rom[bge_disassembly_rom.size() - 2U] = 0x80U;
    bge_disassembly_rom[bge_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bge_disassembly_machine.load_logical_rom(bge_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bge_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BGE disassembly setup failed"
    );
    Debugger bge_disassembler;
    passed &= expect(
        bge_disassembler.attach(bge_disassembly_machine.execution()),
        "BGE disassembler attach failed"
    );
    const auto bge_disassembly = bge_disassembler.disassemble(0x8000U);
    passed &= expect(
        bge_disassembly.has_value()
            && bge_disassembly->supported
            && bge_disassembly->text == "BGE $8000"
            && bge_disassembler.memory_access_size() == 0U,
        "BGE disassembly differs or produced trace side effects"
    );

    Jr800Machine blt_disassembly_machine;
    std::vector<std::uint8_t> blt_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    blt_disassembly_rom[0U] = 0x2DU;
    blt_disassembly_rom[1U] = 0xFEU;
    blt_disassembly_rom[blt_disassembly_rom.size() - 2U] = 0x80U;
    blt_disassembly_rom[blt_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        blt_disassembly_machine.load_logical_rom(blt_disassembly_rom)
                == Jr800MemoryStatus::ok
            && blt_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BLT disassembly setup failed"
    );
    Debugger blt_disassembler;
    passed &= expect(
        blt_disassembler.attach(blt_disassembly_machine.execution()),
        "BLT disassembler attach failed"
    );
    const auto blt_disassembly = blt_disassembler.disassemble(0x8000U);
    passed &= expect(
        blt_disassembly.has_value()
            && blt_disassembly->supported
            && blt_disassembly->text == "BLT $8000"
            && blt_disassembler.memory_access_size() == 0U,
        "BLT disassembly differs or produced trace side effects"
    );

    Jr800Machine bgt_disassembly_machine;
    std::vector<std::uint8_t> bgt_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bgt_disassembly_rom[0U] = 0x2EU;
    bgt_disassembly_rom[1U] = 0xFEU;
    bgt_disassembly_rom[bgt_disassembly_rom.size() - 2U] = 0x80U;
    bgt_disassembly_rom[bgt_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bgt_disassembly_machine.load_logical_rom(bgt_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bgt_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BGT disassembly setup failed"
    );
    Debugger bgt_disassembler;
    passed &= expect(
        bgt_disassembler.attach(bgt_disassembly_machine.execution()),
        "BGT disassembler attach failed"
    );
    const auto bgt_disassembly = bgt_disassembler.disassemble(0x8000U);
    passed &= expect(
        bgt_disassembly.has_value()
            && bgt_disassembly->supported
            && bgt_disassembly->text == "BGT $8000"
            && bgt_disassembler.memory_access_size() == 0U,
        "BGT disassembly differs or produced trace side effects"
    );

    Jr800Machine ble_disassembly_machine;
    std::vector<std::uint8_t> ble_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ble_disassembly_rom[0U] = 0x2FU;
    ble_disassembly_rom[1U] = 0xFEU;
    ble_disassembly_rom[ble_disassembly_rom.size() - 2U] = 0x80U;
    ble_disassembly_rom[ble_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        ble_disassembly_machine.load_logical_rom(ble_disassembly_rom)
                == Jr800MemoryStatus::ok
            && ble_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BLE disassembly setup failed"
    );
    Debugger ble_disassembler;
    passed &= expect(
        ble_disassembler.attach(ble_disassembly_machine.execution()),
        "BLE disassembler attach failed"
    );
    const auto ble_disassembly = ble_disassembler.disassemble(0x8000U);
    passed &= expect(
        ble_disassembly.has_value()
            && ble_disassembly->supported
            && ble_disassembly->text == "BLE $8000"
            && ble_disassembler.memory_access_size() == 0U,
        "BLE disassembly differs or produced trace side effects"
    );

    Jr800Machine adda_disassembly_machine;
    std::vector<std::uint8_t> adda_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adda_disassembly_rom[0U] = 0x8BU;
    adda_disassembly_rom[1U] = 0x7FU;
    adda_disassembly_rom[adda_disassembly_rom.size() - 2U] = 0x80U;
    adda_disassembly_rom[adda_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        adda_disassembly_machine.load_logical_rom(adda_disassembly_rom)
                == Jr800MemoryStatus::ok
            && adda_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "ADDA disassembly setup failed"
    );
    Debugger adda_disassembler;
    passed &= expect(
        adda_disassembler.attach(adda_disassembly_machine.execution()),
        "ADDA disassembler attach failed"
    );
    const auto adda_disassembly = adda_disassembler.disassemble(0x8000U);
    passed &= expect(
        adda_disassembly.has_value()
            && adda_disassembly->supported
            && adda_disassembly->text == "ADDA #$7F"
            && adda_disassembler.memory_access_size() == 0U,
        "ADDA disassembly differs or produced trace side effects"
    );

    Jr800Machine adda_direct_disassembly_machine;
    std::vector<std::uint8_t> adda_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adda_direct_disassembly_rom[0U] = 0x9BU;
    adda_direct_disassembly_rom[1U] = 0x20U;
    adda_direct_disassembly_rom[
        adda_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    adda_direct_disassembly_rom[
        adda_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        adda_direct_disassembly_machine.load_logical_rom(
            adda_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && adda_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct ADDA disassembly setup failed"
    );
    Debugger adda_direct_disassembler;
    passed &= expect(
        adda_direct_disassembler.attach(
            adda_direct_disassembly_machine.execution()
        ),
        "Direct ADDA disassembler attach failed"
    );
    const auto adda_direct_disassembly =
        adda_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        adda_direct_disassembly.has_value()
            && adda_direct_disassembly->supported
            && adda_direct_disassembly->text == "ADDA $20"
            && adda_direct_disassembler.memory_access_size() == 0U,
        "Direct ADDA disassembly differs or produced trace side effects"
    );

    Jr800Machine adda_indexed_disassembly_machine;
    std::vector<std::uint8_t> adda_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adda_indexed_disassembly_rom[0U] = 0xABU;
    adda_indexed_disassembly_rom[1U] = 0x20U;
    adda_indexed_disassembly_rom[
        adda_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    adda_indexed_disassembly_rom[
        adda_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        adda_indexed_disassembly_machine.load_logical_rom(
            adda_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && adda_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed ADDA disassembly setup failed"
    );
    Debugger adda_indexed_disassembler;
    passed &= expect(
        adda_indexed_disassembler.attach(
            adda_indexed_disassembly_machine.execution()
        ),
        "Indexed ADDA disassembler attach failed"
    );
    const auto adda_indexed_disassembly =
        adda_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        adda_indexed_disassembly.has_value()
            && adda_indexed_disassembly->supported
            && adda_indexed_disassembly->text == "ADDA $20,X"
            && adda_indexed_disassembler.memory_access_size() == 0U,
        "Indexed ADDA disassembly differs or produced trace side effects"
    );

    Jr800Machine adda_extended_disassembly_machine;
    std::vector<std::uint8_t> adda_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adda_extended_disassembly_rom[0U] = 0xBBU;
    adda_extended_disassembly_rom[1U] = 0x81U;
    adda_extended_disassembly_rom[2U] = 0x23U;
    adda_extended_disassembly_rom[
        adda_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    adda_extended_disassembly_rom[
        adda_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        adda_extended_disassembly_machine.load_logical_rom(
            adda_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && adda_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended ADDA disassembly setup failed"
    );
    Debugger adda_extended_disassembler;
    passed &= expect(
        adda_extended_disassembler.attach(
            adda_extended_disassembly_machine.execution()
        ),
        "Extended ADDA disassembler attach failed"
    );
    const auto adda_extended_disassembly =
        adda_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        adda_extended_disassembly.has_value()
            && adda_extended_disassembly->supported
            && adda_extended_disassembly->text == "ADDA $8123"
            && adda_extended_disassembler.memory_access_size() == 0U,
        "Extended ADDA disassembly differs or produced trace side effects"
    );

    Jr800Machine addb_disassembly_machine;
    std::vector<std::uint8_t> addb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    addb_disassembly_rom[0U] = 0xCBU;
    addb_disassembly_rom[1U] = 0x7FU;
    addb_disassembly_rom[addb_disassembly_rom.size() - 2U] = 0x80U;
    addb_disassembly_rom[addb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        addb_disassembly_machine.load_logical_rom(addb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && addb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "ADDB disassembly setup failed"
    );
    Debugger addb_disassembler;
    passed &= expect(
        addb_disassembler.attach(addb_disassembly_machine.execution()),
        "ADDB disassembler attach failed"
    );
    const auto addb_disassembly = addb_disassembler.disassemble(0x8000U);
    passed &= expect(
        addb_disassembly.has_value()
            && addb_disassembly->supported
            && addb_disassembly->text == "ADDB #$7F"
            && addb_disassembler.memory_access_size() == 0U,
        "ADDB disassembly differs or produced trace side effects"
    );

    Jr800Machine addb_indexed_disassembly_machine;
    std::vector<std::uint8_t> addb_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    addb_indexed_disassembly_rom[0U] = 0xEBU;
    addb_indexed_disassembly_rom[1U] = 0x20U;
    addb_indexed_disassembly_rom[
        addb_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    addb_indexed_disassembly_rom[
        addb_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        addb_indexed_disassembly_machine.load_logical_rom(
            addb_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && addb_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed ADDB disassembly setup failed"
    );
    Debugger addb_indexed_disassembler;
    passed &= expect(
        addb_indexed_disassembler.attach(
            addb_indexed_disassembly_machine.execution()
        ),
        "Indexed ADDB disassembler attach failed"
    );
    const auto addb_indexed_disassembly =
        addb_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        addb_indexed_disassembly.has_value()
            && addb_indexed_disassembly->supported
            && addb_indexed_disassembly->text == "ADDB $20,X"
            && addb_indexed_disassembler.memory_access_size() == 0U,
        "Indexed ADDB disassembly differs or produced trace side effects"
    );

    Jr800Machine addb_extended_disassembly_machine;
    std::vector<std::uint8_t> addb_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    addb_extended_disassembly_rom[0U] = 0xFBU;
    addb_extended_disassembly_rom[1U] = 0x81U;
    addb_extended_disassembly_rom[2U] = 0x23U;
    addb_extended_disassembly_rom[
        addb_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    addb_extended_disassembly_rom[
        addb_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        addb_extended_disassembly_machine.load_logical_rom(
            addb_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && addb_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended ADDB disassembly setup failed"
    );
    Debugger addb_extended_disassembler;
    passed &= expect(
        addb_extended_disassembler.attach(
            addb_extended_disassembly_machine.execution()
        ),
        "Extended ADDB disassembler attach failed"
    );
    const auto addb_extended_disassembly =
        addb_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        addb_extended_disassembly.has_value()
            && addb_extended_disassembly->supported
            && addb_extended_disassembly->text == "ADDB $8123"
            && addb_extended_disassembler.memory_access_size() == 0U,
        "Extended ADDB disassembly differs or produced trace side effects"
    );

    Jr800Machine adca_disassembly_machine;
    std::vector<std::uint8_t> adca_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adca_disassembly_rom[0U] = 0x89U;
    adca_disassembly_rom[1U] = 0x7FU;
    adca_disassembly_rom[adca_disassembly_rom.size() - 2U] = 0x80U;
    adca_disassembly_rom[adca_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        adca_disassembly_machine.load_logical_rom(adca_disassembly_rom)
                == Jr800MemoryStatus::ok
            && adca_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "ADCA disassembly setup failed"
    );
    Debugger adca_disassembler;
    passed &= expect(
        adca_disassembler.attach(adca_disassembly_machine.execution()),
        "ADCA disassembler attach failed"
    );
    const auto adca_disassembly = adca_disassembler.disassemble(0x8000U);
    passed &= expect(
        adca_disassembly.has_value()
            && adca_disassembly->supported
            && adca_disassembly->text == "ADCA #$7F"
            && adca_disassembler.memory_access_size() == 0U,
        "ADCA disassembly differs or produced trace side effects"
    );

    Jr800Machine adca_direct_disassembly_machine;
    std::vector<std::uint8_t> adca_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adca_direct_disassembly_rom[0U] = 0x99U;
    adca_direct_disassembly_rom[1U] = 0x20U;
    adca_direct_disassembly_rom[adca_direct_disassembly_rom.size() - 2U]
        = 0x80U;
    adca_direct_disassembly_rom[adca_direct_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        adca_direct_disassembly_machine.load_logical_rom(
            adca_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && adca_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct ADCA disassembly setup failed"
    );
    Debugger adca_direct_disassembler;
    passed &= expect(
        adca_direct_disassembler.attach(
            adca_direct_disassembly_machine.execution()
        ),
        "Direct ADCA disassembler attach failed"
    );
    const auto adca_direct_disassembly =
        adca_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        adca_direct_disassembly.has_value()
            && adca_direct_disassembly->supported
            && adca_direct_disassembly->text == "ADCA $20"
            && adca_direct_disassembler.memory_access_size() == 0U,
        "Direct ADCA disassembly differs or produced trace side effects"
    );

    Jr800Machine adca_indexed_disassembly_machine;
    std::vector<std::uint8_t> adca_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adca_indexed_disassembly_rom[0U] = 0xA9U;
    adca_indexed_disassembly_rom[1U] = 0x20U;
    adca_indexed_disassembly_rom[adca_indexed_disassembly_rom.size() - 2U]
        = 0x80U;
    adca_indexed_disassembly_rom[adca_indexed_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        adca_indexed_disassembly_machine.load_logical_rom(
            adca_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && adca_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed ADCA disassembly setup failed"
    );
    Debugger adca_indexed_disassembler;
    passed &= expect(
        adca_indexed_disassembler.attach(
            adca_indexed_disassembly_machine.execution()
        ),
        "Indexed ADCA disassembler attach failed"
    );
    const auto adca_indexed_disassembly =
        adca_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        adca_indexed_disassembly.has_value()
            && adca_indexed_disassembly->supported
            && adca_indexed_disassembly->text == "ADCA $20,X"
            && adca_indexed_disassembler.memory_access_size() == 0U,
        "Indexed ADCA disassembly differs or produced trace side effects"
    );

    Jr800Machine adca_extended_disassembly_machine;
    std::vector<std::uint8_t> adca_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adca_extended_disassembly_rom[0U] = 0xB9U;
    adca_extended_disassembly_rom[1U] = 0x81U;
    adca_extended_disassembly_rom[2U] = 0x23U;
    adca_extended_disassembly_rom[adca_extended_disassembly_rom.size() - 2U]
        = 0x80U;
    adca_extended_disassembly_rom[adca_extended_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        adca_extended_disassembly_machine.load_logical_rom(
            adca_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && adca_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended ADCA disassembly setup failed"
    );
    Debugger adca_extended_disassembler;
    passed &= expect(
        adca_extended_disassembler.attach(
            adca_extended_disassembly_machine.execution()
        ),
        "Extended ADCA disassembler attach failed"
    );
    const auto adca_extended_disassembly =
        adca_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        adca_extended_disassembly.has_value()
            && adca_extended_disassembly->supported
            && adca_extended_disassembly->text == "ADCA $8123"
            && adca_extended_disassembler.memory_access_size() == 0U,
        "Extended ADCA disassembly differs or produced trace side effects"
    );

    Jr800Machine adcb_disassembly_machine;
    std::vector<std::uint8_t> adcb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adcb_disassembly_rom[0U] = 0xC9U;
    adcb_disassembly_rom[1U] = 0x7FU;
    adcb_disassembly_rom[adcb_disassembly_rom.size() - 2U] = 0x80U;
    adcb_disassembly_rom[adcb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        adcb_disassembly_machine.load_logical_rom(adcb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && adcb_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "ADCB disassembly setup failed"
    );
    Debugger adcb_disassembler;
    passed &= expect(
        adcb_disassembler.attach(adcb_disassembly_machine.execution()),
        "ADCB disassembler attach failed"
    );
    const auto adcb_disassembly = adcb_disassembler.disassemble(0x8000U);
    passed &= expect(
        adcb_disassembly.has_value()
            && adcb_disassembly->supported
            && adcb_disassembly->text == "ADCB #$7F"
            && adcb_disassembler.memory_access_size() == 0U,
        "ADCB disassembly differs or produced trace side effects"
    );

    Jr800Machine adcb_direct_disassembly_machine;
    std::vector<std::uint8_t> adcb_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adcb_direct_disassembly_rom[0U] = 0xD9U;
    adcb_direct_disassembly_rom[1U] = 0x20U;
    adcb_direct_disassembly_rom[adcb_direct_disassembly_rom.size() - 2U]
        = 0x80U;
    adcb_direct_disassembly_rom[adcb_direct_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        adcb_direct_disassembly_machine.load_logical_rom(
            adcb_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && adcb_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct ADCB disassembly setup failed"
    );
    Debugger adcb_direct_disassembler;
    passed &= expect(
        adcb_direct_disassembler.attach(
            adcb_direct_disassembly_machine.execution()
        ),
        "Direct ADCB disassembler attach failed"
    );
    const auto adcb_direct_disassembly =
        adcb_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        adcb_direct_disassembly.has_value()
            && adcb_direct_disassembly->supported
            && adcb_direct_disassembly->text == "ADCB $20"
            && adcb_direct_disassembler.memory_access_size() == 0U,
        "Direct ADCB disassembly differs or produced trace side effects"
    );

    Jr800Machine adcb_indexed_disassembly_machine;
    std::vector<std::uint8_t> adcb_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adcb_indexed_disassembly_rom[0U] = 0xE9U;
    adcb_indexed_disassembly_rom[1U] = 0x20U;
    adcb_indexed_disassembly_rom[adcb_indexed_disassembly_rom.size() - 2U]
        = 0x80U;
    adcb_indexed_disassembly_rom[adcb_indexed_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        adcb_indexed_disassembly_machine.load_logical_rom(
            adcb_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && adcb_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed ADCB disassembly setup failed"
    );
    Debugger adcb_indexed_disassembler;
    passed &= expect(
        adcb_indexed_disassembler.attach(
            adcb_indexed_disassembly_machine.execution()
        ),
        "Indexed ADCB disassembler attach failed"
    );
    const auto adcb_indexed_disassembly =
        adcb_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        adcb_indexed_disassembly.has_value()
            && adcb_indexed_disassembly->supported
            && adcb_indexed_disassembly->text == "ADCB $20,X"
            && adcb_indexed_disassembler.memory_access_size() == 0U,
        "Indexed ADCB disassembly differs or produced trace side effects"
    );

    Jr800Machine adcb_extended_disassembly_machine;
    std::vector<std::uint8_t> adcb_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    adcb_extended_disassembly_rom[0U] = 0xF9U;
    adcb_extended_disassembly_rom[1U] = 0x81U;
    adcb_extended_disassembly_rom[2U] = 0x23U;
    adcb_extended_disassembly_rom[adcb_extended_disassembly_rom.size() - 2U]
        = 0x80U;
    adcb_extended_disassembly_rom[adcb_extended_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        adcb_extended_disassembly_machine.load_logical_rom(
            adcb_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && adcb_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended ADCB disassembly setup failed"
    );
    Debugger adcb_extended_disassembler;
    passed &= expect(
        adcb_extended_disassembler.attach(
            adcb_extended_disassembly_machine.execution()
        ),
        "Extended ADCB disassembler attach failed"
    );
    const auto adcb_extended_disassembly =
        adcb_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        adcb_extended_disassembly.has_value()
            && adcb_extended_disassembly->supported
            && adcb_extended_disassembly->text == "ADCB $8123"
            && adcb_extended_disassembler.memory_access_size() == 0U,
        "Extended ADCB disassembly differs or produced trace side effects"
    );

    Jr800Machine anda_disassembly_machine;
    std::vector<std::uint8_t> anda_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    anda_disassembly_rom[0U] = 0x84U;
    anda_disassembly_rom[1U] = 0x7FU;
    anda_disassembly_rom[anda_disassembly_rom.size() - 2U] = 0x80U;
    anda_disassembly_rom[anda_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        anda_disassembly_machine.load_logical_rom(anda_disassembly_rom)
                == Jr800MemoryStatus::ok
            && anda_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "ANDA disassembly setup failed"
    );
    Debugger anda_disassembler;
    passed &= expect(
        anda_disassembler.attach(anda_disassembly_machine.execution()),
        "ANDA disassembler attach failed"
    );
    const auto anda_disassembly = anda_disassembler.disassemble(0x8000U);
    passed &= expect(
        anda_disassembly.has_value()
            && anda_disassembly->supported
            && anda_disassembly->text == "ANDA #$7F"
            && anda_disassembler.memory_access_size() == 0U,
        "ANDA disassembly differs or produced trace side effects"
    );

    Jr800Machine anda_direct_disassembly_machine;
    std::vector<std::uint8_t> anda_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    anda_direct_disassembly_rom[0U] = 0x94U;
    anda_direct_disassembly_rom[1U] = 0x20U;
    anda_direct_disassembly_rom[
        anda_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    anda_direct_disassembly_rom[
        anda_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        anda_direct_disassembly_machine.load_logical_rom(
            anda_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && anda_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct ANDA disassembly setup failed"
    );
    Debugger anda_direct_disassembler;
    passed &= expect(
        anda_direct_disassembler.attach(
            anda_direct_disassembly_machine.execution()
        ),
        "Direct ANDA disassembler attach failed"
    );
    const auto anda_direct_disassembly =
        anda_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        anda_direct_disassembly.has_value()
            && anda_direct_disassembly->supported
            && anda_direct_disassembly->text == "ANDA $20"
            && anda_direct_disassembler.memory_access_size() == 0U,
        "Direct ANDA disassembly differs or produced trace side effects"
    );

    Jr800Machine anda_indexed_disassembly_machine;
    std::vector<std::uint8_t> anda_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    anda_indexed_disassembly_rom[0U] = 0xA4U;
    anda_indexed_disassembly_rom[1U] = 0x23U;
    anda_indexed_disassembly_rom[
        anda_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    anda_indexed_disassembly_rom[
        anda_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        anda_indexed_disassembly_machine.load_logical_rom(
            anda_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && anda_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed ANDA disassembly setup failed"
    );
    Debugger anda_indexed_disassembler;
    passed &= expect(
        anda_indexed_disassembler.attach(
            anda_indexed_disassembly_machine.execution()
        ),
        "Indexed ANDA disassembler attach failed"
    );
    const auto anda_indexed_disassembly =
        anda_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        anda_indexed_disassembly.has_value()
            && anda_indexed_disassembly->supported
            && anda_indexed_disassembly->text == "ANDA $23,X"
            && anda_indexed_disassembler.memory_access_size() == 0U,
        "Indexed ANDA disassembly differs or produced trace side effects"
    );

    Jr800Machine anda_extended_disassembly_machine;
    std::vector<std::uint8_t> anda_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    anda_extended_disassembly_rom[0U] = 0xB4U;
    anda_extended_disassembly_rom[1U] = 0x81U;
    anda_extended_disassembly_rom[2U] = 0x23U;
    anda_extended_disassembly_rom[
        anda_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    anda_extended_disassembly_rom[
        anda_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        anda_extended_disassembly_machine.load_logical_rom(
            anda_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && anda_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended ANDA disassembly setup failed"
    );
    Debugger anda_extended_disassembler;
    passed &= expect(
        anda_extended_disassembler.attach(
            anda_extended_disassembly_machine.execution()
        ),
        "Extended ANDA disassembler attach failed"
    );
    const auto anda_extended_disassembly =
        anda_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        anda_extended_disassembly.has_value()
            && anda_extended_disassembly->supported
            && anda_extended_disassembly->text == "ANDA $8123"
            && anda_extended_disassembler.memory_access_size() == 0U,
        "Extended ANDA disassembly differs or produced trace side effects"
    );

    Jr800Machine andb_disassembly_machine;
    std::vector<std::uint8_t> andb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    andb_disassembly_rom[0U] = 0xC4U;
    andb_disassembly_rom[1U] = 0x7FU;
    andb_disassembly_rom[andb_disassembly_rom.size() - 2U] = 0x80U;
    andb_disassembly_rom[andb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        andb_disassembly_machine.load_logical_rom(andb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && andb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "ANDB disassembly setup failed"
    );
    Debugger andb_disassembler;
    passed &= expect(
        andb_disassembler.attach(andb_disassembly_machine.execution()),
        "ANDB disassembler attach failed"
    );
    const auto andb_disassembly = andb_disassembler.disassemble(0x8000U);
    passed &= expect(
        andb_disassembly.has_value()
            && andb_disassembly->supported
            && andb_disassembly->text == "ANDB #$7F"
            && andb_disassembler.memory_access_size() == 0U,
        "ANDB disassembly differs or produced trace side effects"
    );

    Jr800Machine andb_direct_disassembly_machine;
    std::vector<std::uint8_t> andb_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    andb_direct_disassembly_rom[0U] = 0xD4U;
    andb_direct_disassembly_rom[1U] = 0x20U;
    andb_direct_disassembly_rom[
        andb_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    andb_direct_disassembly_rom[
        andb_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        andb_direct_disassembly_machine.load_logical_rom(
            andb_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && andb_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct ANDB disassembly setup failed"
    );
    Debugger andb_direct_disassembler;
    passed &= expect(
        andb_direct_disassembler.attach(
            andb_direct_disassembly_machine.execution()
        ),
        "Direct ANDB disassembler attach failed"
    );
    const auto andb_direct_disassembly =
        andb_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        andb_direct_disassembly.has_value()
            && andb_direct_disassembly->supported
            && andb_direct_disassembly->text == "ANDB $20"
            && andb_direct_disassembler.memory_access_size() == 0U,
        "Direct ANDB disassembly differs or produced trace side effects"
    );

    Jr800Machine andb_indexed_disassembly_machine;
    std::vector<std::uint8_t> andb_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    andb_indexed_disassembly_rom[0U] = 0xE4U;
    andb_indexed_disassembly_rom[1U] = 0x23U;
    andb_indexed_disassembly_rom[
        andb_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    andb_indexed_disassembly_rom[
        andb_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        andb_indexed_disassembly_machine.load_logical_rom(
            andb_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && andb_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed ANDB disassembly setup failed"
    );
    Debugger andb_indexed_disassembler;
    passed &= expect(
        andb_indexed_disassembler.attach(
            andb_indexed_disassembly_machine.execution()
        ),
        "Indexed ANDB disassembler attach failed"
    );
    const auto andb_indexed_disassembly =
        andb_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        andb_indexed_disassembly.has_value()
            && andb_indexed_disassembly->supported
            && andb_indexed_disassembly->text == "ANDB $23,X"
            && andb_indexed_disassembler.memory_access_size() == 0U,
        "Indexed ANDB disassembly differs or produced trace side effects"
    );

    Jr800Machine andb_extended_disassembly_machine;
    std::vector<std::uint8_t> andb_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    andb_extended_disassembly_rom[0U] = 0xF4U;
    andb_extended_disassembly_rom[1U] = 0x81U;
    andb_extended_disassembly_rom[2U] = 0x23U;
    andb_extended_disassembly_rom[
        andb_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    andb_extended_disassembly_rom[
        andb_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        andb_extended_disassembly_machine.load_logical_rom(
            andb_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && andb_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended ANDB disassembly setup failed"
    );
    Debugger andb_extended_disassembler;
    passed &= expect(
        andb_extended_disassembler.attach(
            andb_extended_disassembly_machine.execution()
        ),
        "Extended ANDB disassembler attach failed"
    );
    const auto andb_extended_disassembly =
        andb_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        andb_extended_disassembly.has_value()
            && andb_extended_disassembly->supported
            && andb_extended_disassembly->text == "ANDB $8123"
            && andb_extended_disassembler.memory_access_size() == 0U,
        "Extended ANDB disassembly differs or produced trace side effects"
    );

    Jr800Machine bita_disassembly_machine;
    std::vector<std::uint8_t> bita_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bita_disassembly_rom[0U] = 0x85U;
    bita_disassembly_rom[1U] = 0x7FU;
    bita_disassembly_rom[bita_disassembly_rom.size() - 2U] = 0x80U;
    bita_disassembly_rom[bita_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        bita_disassembly_machine.load_logical_rom(bita_disassembly_rom)
                == Jr800MemoryStatus::ok
            && bita_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "BITA disassembly setup failed"
    );
    Debugger bita_disassembler;
    passed &= expect(
        bita_disassembler.attach(bita_disassembly_machine.execution()),
        "BITA disassembler attach failed"
    );
    const auto bita_disassembly = bita_disassembler.disassemble(0x8000U);
    passed &= expect(
        bita_disassembly.has_value()
            && bita_disassembly->supported
            && bita_disassembly->text == "BITA #$7F"
            && bita_disassembler.memory_access_size() == 0U,
        "BITA disassembly differs or produced trace side effects"
    );

    Jr800Machine bita_direct_disassembly_machine;
    std::vector<std::uint8_t> bita_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bita_direct_disassembly_rom[0U] = 0x95U;
    bita_direct_disassembly_rom[1U] = 0x20U;
    bita_direct_disassembly_rom[
        bita_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    bita_direct_disassembly_rom[
        bita_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        bita_direct_disassembly_machine.load_logical_rom(
            bita_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && bita_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct BITA disassembly setup failed"
    );
    Debugger bita_direct_disassembler;
    passed &= expect(
        bita_direct_disassembler.attach(
            bita_direct_disassembly_machine.execution()
        ),
        "Direct BITA disassembler attach failed"
    );
    const auto bita_direct_disassembly =
        bita_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        bita_direct_disassembly.has_value()
            && bita_direct_disassembly->supported
            && bita_direct_disassembly->text == "BITA $20"
            && bita_direct_disassembler.memory_access_size() == 0U,
        "Direct BITA disassembly differs or produced trace side effects"
    );

    Jr800Machine bita_indexed_disassembly_machine;
    std::vector<std::uint8_t> bita_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bita_indexed_disassembly_rom[0U] = 0xA5U;
    bita_indexed_disassembly_rom[1U] = 0x23U;
    bita_indexed_disassembly_rom[
        bita_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    bita_indexed_disassembly_rom[
        bita_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        bita_indexed_disassembly_machine.load_logical_rom(
            bita_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && bita_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed BITA disassembly setup failed"
    );
    Debugger bita_indexed_disassembler;
    passed &= expect(
        bita_indexed_disassembler.attach(
            bita_indexed_disassembly_machine.execution()
        ),
        "Indexed BITA disassembler attach failed"
    );
    const auto bita_indexed_disassembly =
        bita_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        bita_indexed_disassembly.has_value()
            && bita_indexed_disassembly->supported
            && bita_indexed_disassembly->text == "BITA $23,X"
            && bita_indexed_disassembler.memory_access_size() == 0U,
        "Indexed BITA disassembly differs or produced trace side effects"
    );

    Jr800Machine bita_extended_disassembly_machine;
    std::vector<std::uint8_t> bita_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bita_extended_disassembly_rom[0U] = 0xB5U;
    bita_extended_disassembly_rom[1U] = 0x81U;
    bita_extended_disassembly_rom[2U] = 0x23U;
    bita_extended_disassembly_rom[
        bita_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    bita_extended_disassembly_rom[
        bita_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        bita_extended_disassembly_machine.load_logical_rom(
            bita_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && bita_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended BITA disassembly setup failed"
    );
    Debugger bita_extended_disassembler;
    passed &= expect(
        bita_extended_disassembler.attach(
            bita_extended_disassembly_machine.execution()
        ),
        "Extended BITA disassembler attach failed"
    );
    const auto bita_extended_disassembly =
        bita_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        bita_extended_disassembly.has_value()
            && bita_extended_disassembly->supported
            && bita_extended_disassembly->text == "BITA $8123"
            && bita_extended_disassembler.memory_access_size() == 0U,
        "Extended BITA disassembly differs or produced trace side effects"
    );

    Jr800Machine bitb_direct_disassembly_machine;
    std::vector<std::uint8_t> bitb_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bitb_direct_disassembly_rom[0U] = 0xD5U;
    bitb_direct_disassembly_rom[1U] = 0x20U;
    bitb_direct_disassembly_rom[
        bitb_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    bitb_direct_disassembly_rom[
        bitb_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        bitb_direct_disassembly_machine.load_logical_rom(
            bitb_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && bitb_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct BITB disassembly setup failed"
    );
    Debugger bitb_direct_disassembler;
    passed &= expect(
        bitb_direct_disassembler.attach(
            bitb_direct_disassembly_machine.execution()
        ),
        "Direct BITB disassembler attach failed"
    );
    const auto bitb_direct_disassembly =
        bitb_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        bitb_direct_disassembly.has_value()
            && bitb_direct_disassembly->supported
            && bitb_direct_disassembly->text == "BITB $20"
            && bitb_direct_disassembler.memory_access_size() == 0U,
        "Direct BITB disassembly differs or produced trace side effects"
    );

    Jr800Machine bitb_indexed_disassembly_machine;
    std::vector<std::uint8_t> bitb_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bitb_indexed_disassembly_rom[0U] = 0xE5U;
    bitb_indexed_disassembly_rom[1U] = 0x23U;
    bitb_indexed_disassembly_rom[
        bitb_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    bitb_indexed_disassembly_rom[
        bitb_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        bitb_indexed_disassembly_machine.load_logical_rom(
            bitb_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && bitb_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed BITB disassembly setup failed"
    );
    Debugger bitb_indexed_disassembler;
    passed &= expect(
        bitb_indexed_disassembler.attach(
            bitb_indexed_disassembly_machine.execution()
        ),
        "Indexed BITB disassembler attach failed"
    );
    const auto bitb_indexed_disassembly =
        bitb_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        bitb_indexed_disassembly.has_value()
            && bitb_indexed_disassembly->supported
            && bitb_indexed_disassembly->text == "BITB $23,X"
            && bitb_indexed_disassembler.memory_access_size() == 0U,
        "Indexed BITB disassembly differs or produced trace side effects"
    );

    Jr800Machine bitb_extended_disassembly_machine;
    std::vector<std::uint8_t> bitb_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    bitb_extended_disassembly_rom[0U] = 0xF5U;
    bitb_extended_disassembly_rom[1U] = 0x81U;
    bitb_extended_disassembly_rom[2U] = 0x23U;
    bitb_extended_disassembly_rom[
        bitb_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    bitb_extended_disassembly_rom[
        bitb_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        bitb_extended_disassembly_machine.load_logical_rom(
            bitb_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && bitb_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended BITB disassembly setup failed"
    );
    Debugger bitb_extended_disassembler;
    passed &= expect(
        bitb_extended_disassembler.attach(
            bitb_extended_disassembly_machine.execution()
        ),
        "Extended BITB disassembler attach failed"
    );
    const auto bitb_extended_disassembly =
        bitb_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        bitb_extended_disassembly.has_value()
            && bitb_extended_disassembly->supported
            && bitb_extended_disassembly->text == "BITB $8123"
            && bitb_extended_disassembler.memory_access_size() == 0U,
        "Extended BITB disassembly differs or produced trace side effects"
    );

    Jr800Machine suba_disassembly_machine;
    std::vector<std::uint8_t> suba_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    suba_disassembly_rom[0U] = 0x80U;
    suba_disassembly_rom[1U] = 0x7FU;
    suba_disassembly_rom[suba_disassembly_rom.size() - 2U] = 0x80U;
    suba_disassembly_rom[suba_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        suba_disassembly_machine.load_logical_rom(suba_disassembly_rom)
                == Jr800MemoryStatus::ok
            && suba_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "SUBA disassembly setup failed"
    );
    Debugger suba_disassembler;
    passed &= expect(
        suba_disassembler.attach(suba_disassembly_machine.execution()),
        "SUBA disassembler attach failed"
    );
    const auto suba_disassembly = suba_disassembler.disassemble(0x8000U);
    passed &= expect(
        suba_disassembly.has_value()
            && suba_disassembly->supported
            && suba_disassembly->text == "SUBA #$7F"
            && suba_disassembler.memory_access_size() == 0U,
        "SUBA disassembly differs or produced trace side effects"
    );

    Jr800Machine suba_indexed_disassembly_machine;
    std::vector<std::uint8_t> suba_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    suba_indexed_disassembly_rom[0U] = 0xA0U;
    suba_indexed_disassembly_rom[1U] = 0x20U;
    suba_indexed_disassembly_rom[
        suba_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    suba_indexed_disassembly_rom[
        suba_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        suba_indexed_disassembly_machine.load_logical_rom(
            suba_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && suba_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed SUBA disassembly setup failed"
    );
    Debugger suba_indexed_disassembler;
    passed &= expect(
        suba_indexed_disassembler.attach(
            suba_indexed_disassembly_machine.execution()
        ),
        "Indexed SUBA disassembler attach failed"
    );
    const auto suba_indexed_disassembly =
        suba_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        suba_indexed_disassembly.has_value()
            && suba_indexed_disassembly->supported
            && suba_indexed_disassembly->text == "SUBA $20,X"
            && suba_indexed_disassembler.memory_access_size() == 0U,
        "Indexed SUBA disassembly differs or produced trace side effects"
    );

    Jr800Machine suba_extended_disassembly_machine;
    std::vector<std::uint8_t> suba_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    suba_extended_disassembly_rom[0U] = 0xB0U;
    suba_extended_disassembly_rom[1U] = 0x81U;
    suba_extended_disassembly_rom[2U] = 0x23U;
    suba_extended_disassembly_rom[
        suba_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    suba_extended_disassembly_rom[
        suba_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        suba_extended_disassembly_machine.load_logical_rom(
            suba_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && suba_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended SUBA disassembly setup failed"
    );
    Debugger suba_extended_disassembler;
    passed &= expect(
        suba_extended_disassembler.attach(
            suba_extended_disassembly_machine.execution()
        ),
        "Extended SUBA disassembler attach failed"
    );
    const auto suba_extended_disassembly =
        suba_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        suba_extended_disassembly.has_value()
            && suba_extended_disassembly->supported
            && suba_extended_disassembly->text == "SUBA $8123"
            && suba_extended_disassembler.memory_access_size() == 0U,
        "Extended SUBA disassembly differs or produced trace side effects"
    );

    Jr800Machine subb_indexed_disassembly_machine;
    std::vector<std::uint8_t> subb_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    subb_indexed_disassembly_rom[0U] = 0xE0U;
    subb_indexed_disassembly_rom[1U] = 0x20U;
    subb_indexed_disassembly_rom[
        subb_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    subb_indexed_disassembly_rom[
        subb_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        subb_indexed_disassembly_machine.load_logical_rom(
            subb_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && subb_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed SUBB disassembly setup failed"
    );
    Debugger subb_indexed_disassembler;
    passed &= expect(
        subb_indexed_disassembler.attach(
            subb_indexed_disassembly_machine.execution()
        ),
        "Indexed SUBB disassembler attach failed"
    );
    const auto subb_indexed_disassembly =
        subb_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        subb_indexed_disassembly.has_value()
            && subb_indexed_disassembly->supported
            && subb_indexed_disassembly->text == "SUBB $20,X"
            && subb_indexed_disassembler.memory_access_size() == 0U,
        "Indexed SUBB disassembly differs or produced trace side effects"
    );

    Jr800Machine subb_extended_disassembly_machine;
    std::vector<std::uint8_t> subb_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    subb_extended_disassembly_rom[0U] = 0xF0U;
    subb_extended_disassembly_rom[1U] = 0x81U;
    subb_extended_disassembly_rom[2U] = 0x23U;
    subb_extended_disassembly_rom[
        subb_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    subb_extended_disassembly_rom[
        subb_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        subb_extended_disassembly_machine.load_logical_rom(
            subb_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && subb_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended SUBB disassembly setup failed"
    );
    Debugger subb_extended_disassembler;
    passed &= expect(
        subb_extended_disassembler.attach(
            subb_extended_disassembly_machine.execution()
        ),
        "Extended SUBB disassembler attach failed"
    );
    const auto subb_extended_disassembly =
        subb_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        subb_extended_disassembly.has_value()
            && subb_extended_disassembly->supported
            && subb_extended_disassembly->text == "SUBB $8123"
            && subb_extended_disassembler.memory_access_size() == 0U,
        "Extended SUBB disassembly differs or produced trace side effects"
    );

    Jr800Machine cmpa_disassembly_machine;
    std::vector<std::uint8_t> cmpa_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cmpa_disassembly_rom[0U] = 0x81U;
    cmpa_disassembly_rom[1U] = 0x7FU;
    cmpa_disassembly_rom[cmpa_disassembly_rom.size() - 2U] = 0x80U;
    cmpa_disassembly_rom[cmpa_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        cmpa_disassembly_machine.load_logical_rom(cmpa_disassembly_rom)
                == Jr800MemoryStatus::ok
            && cmpa_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "CMPA disassembly setup failed"
    );
    Debugger cmpa_disassembler;
    passed &= expect(
        cmpa_disassembler.attach(cmpa_disassembly_machine.execution()),
        "CMPA disassembler attach failed"
    );
    const auto cmpa_disassembly = cmpa_disassembler.disassemble(0x8000U);
    passed &= expect(
        cmpa_disassembly.has_value()
            && cmpa_disassembly->supported
            && cmpa_disassembly->text == "CMPA #$7F"
            && cmpa_disassembler.memory_access_size() == 0U,
        "CMPA disassembly differs or produced trace side effects"
    );

    Jr800Machine cmpa_direct_disassembly_machine;
    std::vector<std::uint8_t> cmpa_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cmpa_direct_disassembly_rom[0U] = 0x91U;
    cmpa_direct_disassembly_rom[1U] = 0x20U;
    cmpa_direct_disassembly_rom[
        cmpa_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    cmpa_direct_disassembly_rom[
        cmpa_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        cmpa_direct_disassembly_machine.load_logical_rom(
            cmpa_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && cmpa_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct CMPA disassembly setup failed"
    );
    Debugger cmpa_direct_disassembler;
    passed &= expect(
        cmpa_direct_disassembler.attach(
            cmpa_direct_disassembly_machine.execution()
        ),
        "Direct CMPA disassembler attach failed"
    );
    const auto cmpa_direct_disassembly =
        cmpa_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        cmpa_direct_disassembly.has_value()
            && cmpa_direct_disassembly->supported
            && cmpa_direct_disassembly->text == "CMPA $20"
            && cmpa_direct_disassembler.memory_access_size() == 0U,
        "Direct CMPA disassembly differs or produced trace side effects"
    );

    Jr800Machine cmpa_indexed_disassembly_machine;
    std::vector<std::uint8_t> cmpa_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cmpa_indexed_disassembly_rom[0U] = 0xA1U;
    cmpa_indexed_disassembly_rom[1U] = 0x20U;
    cmpa_indexed_disassembly_rom[
        cmpa_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    cmpa_indexed_disassembly_rom[
        cmpa_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        cmpa_indexed_disassembly_machine.load_logical_rom(
            cmpa_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && cmpa_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed CMPA disassembly setup failed"
    );
    Debugger cmpa_indexed_disassembler;
    passed &= expect(
        cmpa_indexed_disassembler.attach(
            cmpa_indexed_disassembly_machine.execution()
        ),
        "Indexed CMPA disassembler attach failed"
    );
    const auto cmpa_indexed_disassembly =
        cmpa_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        cmpa_indexed_disassembly.has_value()
            && cmpa_indexed_disassembly->supported
            && cmpa_indexed_disassembly->text == "CMPA $20,X"
            && cmpa_indexed_disassembler.memory_access_size() == 0U,
        "Indexed CMPA disassembly differs or produced trace side effects"
    );

    Jr800Machine cmpa_extended_disassembly_machine;
    std::vector<std::uint8_t> cmpa_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cmpa_extended_disassembly_rom[0U] = 0xB1U;
    cmpa_extended_disassembly_rom[1U] = 0x81U;
    cmpa_extended_disassembly_rom[2U] = 0x23U;
    cmpa_extended_disassembly_rom[
        cmpa_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    cmpa_extended_disassembly_rom[
        cmpa_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        cmpa_extended_disassembly_machine.load_logical_rom(
            cmpa_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && cmpa_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended CMPA disassembly setup failed"
    );
    Debugger cmpa_extended_disassembler;
    passed &= expect(
        cmpa_extended_disassembler.attach(
            cmpa_extended_disassembly_machine.execution()
        ),
        "Extended CMPA disassembler attach failed"
    );
    const auto cmpa_extended_disassembly =
        cmpa_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        cmpa_extended_disassembly.has_value()
            && cmpa_extended_disassembly->supported
            && cmpa_extended_disassembly->text == "CMPA $8123"
            && cmpa_extended_disassembler.memory_access_size() == 0U,
        "Extended CMPA disassembly differs or produced trace side effects"
    );

    Jr800Machine rti_disassembly_machine;
    std::vector<std::uint8_t> rti_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    rti_disassembly_rom[0U] = 0x3BU;
    rti_disassembly_rom[rti_disassembly_rom.size() - 2U] = 0x80U;
    rti_disassembly_rom[rti_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        rti_disassembly_machine.load_logical_rom(rti_disassembly_rom)
                == Jr800MemoryStatus::ok
            && rti_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "RTI disassembly setup failed"
    );
    Debugger rti_disassembler;
    passed &= expect(
        rti_disassembler.attach(rti_disassembly_machine.execution()),
        "RTI disassembler attach failed"
    );
    const auto rti_disassembly = rti_disassembler.disassemble(0x8000U);
    passed &= expect(
        rti_disassembly.has_value() && rti_disassembly->supported
            && rti_disassembly->text == "RTI"
            && rti_disassembler.memory_access_size() == 0U,
        "RTI disassembly differs or produced trace side effects"
    );

    Jr800Machine wai_disassembly_machine;
    std::vector<std::uint8_t> wai_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    wai_disassembly_rom[0U] = 0x3EU;
    wai_disassembly_rom[wai_disassembly_rom.size() - 2U] = 0x80U;
    wai_disassembly_rom[wai_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        wai_disassembly_machine.load_logical_rom(wai_disassembly_rom)
                == Jr800MemoryStatus::ok
            && wai_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "WAI disassembly setup failed"
    );
    Debugger wai_disassembler;
    passed &= expect(
        wai_disassembler.attach(wai_disassembly_machine.execution()),
        "WAI disassembler attach failed"
    );
    const auto wai_disassembly = wai_disassembler.disassemble(0x8000U);
    passed &= expect(
        wai_disassembly.has_value() && wai_disassembly->supported
            && wai_disassembly->text == "WAI"
            && wai_disassembler.memory_access_size() == 0U,
        "WAI disassembly differs or produced trace side effects"
    );

    Jr800Machine swi_disassembly_machine;
    std::vector<std::uint8_t> swi_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    swi_disassembly_rom[0U] = 0x3FU;
    swi_disassembly_rom[swi_disassembly_rom.size() - 2U] = 0x80U;
    swi_disassembly_rom[swi_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        swi_disassembly_machine.load_logical_rom(swi_disassembly_rom)
                == Jr800MemoryStatus::ok
            && swi_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "SWI disassembly setup failed"
    );
    Debugger swi_disassembler;
    passed &= expect(
        swi_disassembler.attach(swi_disassembly_machine.execution()),
        "SWI disassembler attach failed"
    );
    const auto swi_disassembly = swi_disassembler.disassemble(0x8000U);
    passed &= expect(
        swi_disassembly.has_value() && swi_disassembly->supported
            && swi_disassembly->text == "SWI"
            && swi_disassembler.memory_access_size() == 0U,
        "SWI disassembly differs or produced trace side effects"
    );

    Jr800Machine daa_disassembly_machine;
    std::vector<std::uint8_t> daa_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    daa_disassembly_rom[0U] = 0x19U;
    daa_disassembly_rom[daa_disassembly_rom.size() - 2U] = 0x80U;
    daa_disassembly_rom[daa_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        daa_disassembly_machine.load_logical_rom(daa_disassembly_rom)
                == Jr800MemoryStatus::ok
            && daa_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "DAA disassembly setup failed"
    );
    Debugger daa_disassembler;
    passed &= expect(
        daa_disassembler.attach(daa_disassembly_machine.execution()),
        "DAA disassembler attach failed"
    );
    const auto daa_disassembly = daa_disassembler.disassemble(0x8000U);
    passed &= expect(
        daa_disassembly.has_value() && daa_disassembly->supported
            && daa_disassembly->text == "DAA"
            && daa_disassembler.memory_access_size() == 0U,
        "DAA disassembly differs or produced trace side effects"
    );

    Jr800Machine eora_indexed_disassembly_machine;
    std::vector<std::uint8_t> eora_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    eora_indexed_disassembly_rom[0U] = 0xA8U;
    eora_indexed_disassembly_rom[1U] = 0x23U;
    eora_indexed_disassembly_rom[
        eora_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    eora_indexed_disassembly_rom[
        eora_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        eora_indexed_disassembly_machine.load_logical_rom(
            eora_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && eora_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed EORA disassembly setup failed"
    );
    Debugger eora_indexed_disassembler;
    passed &= expect(
        eora_indexed_disassembler.attach(
            eora_indexed_disassembly_machine.execution()
        ),
        "Indexed EORA disassembler attach failed"
    );
    const auto eora_indexed_disassembly =
        eora_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        eora_indexed_disassembly.has_value()
            && eora_indexed_disassembly->supported
            && eora_indexed_disassembly->text == "EORA $23,X"
            && eora_indexed_disassembler.memory_access_size() == 0U,
        "Indexed EORA disassembly differs or produced trace side effects"
    );

    Jr800Machine eora_extended_disassembly_machine;
    std::vector<std::uint8_t> eora_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    eora_extended_disassembly_rom[0U] = 0xB8U;
    eora_extended_disassembly_rom[1U] = 0x81U;
    eora_extended_disassembly_rom[2U] = 0x23U;
    eora_extended_disassembly_rom[
        eora_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    eora_extended_disassembly_rom[
        eora_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        eora_extended_disassembly_machine.load_logical_rom(
            eora_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && eora_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended EORA disassembly setup failed"
    );
    Debugger eora_extended_disassembler;
    passed &= expect(
        eora_extended_disassembler.attach(
            eora_extended_disassembly_machine.execution()
        ),
        "Extended EORA disassembler attach failed"
    );
    const auto eora_extended_disassembly =
        eora_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        eora_extended_disassembly.has_value()
            && eora_extended_disassembly->supported
            && eora_extended_disassembly->text == "EORA $8123"
            && eora_extended_disassembler.memory_access_size() == 0U,
        "Extended EORA disassembly differs or produced trace side effects"
    );

    Jr800Machine eorb_immediate_disassembly_machine;
    std::vector<std::uint8_t> eorb_immediate_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    eorb_immediate_disassembly_rom[0U] = 0xC8U;
    eorb_immediate_disassembly_rom[1U] = 0x20U;
    eorb_immediate_disassembly_rom[
        eorb_immediate_disassembly_rom.size() - 2U
    ] = 0x80U;
    eorb_immediate_disassembly_rom[
        eorb_immediate_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        eorb_immediate_disassembly_machine.load_logical_rom(
            eorb_immediate_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && eorb_immediate_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Immediate EORB disassembly setup failed"
    );
    Debugger eorb_immediate_disassembler;
    passed &= expect(
        eorb_immediate_disassembler.attach(
            eorb_immediate_disassembly_machine.execution()
        ),
        "Immediate EORB disassembler attach failed"
    );
    const auto eorb_immediate_disassembly =
        eorb_immediate_disassembler.disassemble(0x8000U);
    passed &= expect(
        eorb_immediate_disassembly.has_value()
            && eorb_immediate_disassembly->supported
            && eorb_immediate_disassembly->text == "EORB #$20"
            && eorb_immediate_disassembler.memory_access_size() == 0U,
        "Immediate EORB disassembly differs or produced trace side effects"
    );

    Jr800Machine eorb_direct_disassembly_machine;
    std::vector<std::uint8_t> eorb_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    eorb_direct_disassembly_rom[0U] = 0xD8U;
    eorb_direct_disassembly_rom[1U] = 0x20U;
    eorb_direct_disassembly_rom[
        eorb_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    eorb_direct_disassembly_rom[
        eorb_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        eorb_direct_disassembly_machine.load_logical_rom(
            eorb_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && eorb_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct EORB disassembly setup failed"
    );
    Debugger eorb_direct_disassembler;
    passed &= expect(
        eorb_direct_disassembler.attach(
            eorb_direct_disassembly_machine.execution()
        ),
        "Direct EORB disassembler attach failed"
    );
    const auto eorb_direct_disassembly =
        eorb_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        eorb_direct_disassembly.has_value()
            && eorb_direct_disassembly->supported
            && eorb_direct_disassembly->text == "EORB $20"
            && eorb_direct_disassembler.memory_access_size() == 0U,
        "Direct EORB disassembly differs or produced trace side effects"
    );

    Jr800Machine eorb_indexed_disassembly_machine;
    std::vector<std::uint8_t> eorb_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    eorb_indexed_disassembly_rom[0U] = 0xE8U;
    eorb_indexed_disassembly_rom[1U] = 0x20U;
    eorb_indexed_disassembly_rom[
        eorb_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    eorb_indexed_disassembly_rom[
        eorb_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        eorb_indexed_disassembly_machine.load_logical_rom(
            eorb_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && eorb_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed EORB disassembly setup failed"
    );
    Debugger eorb_indexed_disassembler;
    passed &= expect(
        eorb_indexed_disassembler.attach(
            eorb_indexed_disassembly_machine.execution()
        ),
        "Indexed EORB disassembler attach failed"
    );
    const auto eorb_indexed_disassembly =
        eorb_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        eorb_indexed_disassembly.has_value()
            && eorb_indexed_disassembly->supported
            && eorb_indexed_disassembly->text == "EORB $20,X"
            && eorb_indexed_disassembler.memory_access_size() == 0U,
        "Indexed EORB disassembly differs or produced trace side effects"
    );

    Jr800Machine eorb_extended_disassembly_machine;
    std::vector<std::uint8_t> eorb_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    eorb_extended_disassembly_rom[0U] = 0xF8U;
    eorb_extended_disassembly_rom[1U] = 0x81U;
    eorb_extended_disassembly_rom[2U] = 0x23U;
    eorb_extended_disassembly_rom[
        eorb_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    eorb_extended_disassembly_rom[
        eorb_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        eorb_extended_disassembly_machine.load_logical_rom(
            eorb_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && eorb_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended EORB disassembly setup failed"
    );
    Debugger eorb_extended_disassembler;
    passed &= expect(
        eorb_extended_disassembler.attach(
            eorb_extended_disassembly_machine.execution()
        ),
        "Extended EORB disassembler attach failed"
    );
    const auto eorb_extended_disassembly =
        eorb_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        eorb_extended_disassembly.has_value()
            && eorb_extended_disassembly->supported
            && eorb_extended_disassembly->text == "EORB $8123"
            && eorb_extended_disassembler.memory_access_size() == 0U,
        "Extended EORB disassembly differs or produced trace side effects"
    );

    Jr800Machine oraa_immediate_disassembly_machine;
    std::vector<std::uint8_t> oraa_immediate_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    oraa_immediate_disassembly_rom[0U] = 0x8AU;
    oraa_immediate_disassembly_rom[1U] = 0x20U;
    oraa_immediate_disassembly_rom[
        oraa_immediate_disassembly_rom.size() - 2U
    ] = 0x80U;
    oraa_immediate_disassembly_rom[
        oraa_immediate_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        oraa_immediate_disassembly_machine.load_logical_rom(
            oraa_immediate_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && oraa_immediate_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Immediate ORAA disassembly setup failed"
    );
    Debugger oraa_immediate_disassembler;
    passed &= expect(
        oraa_immediate_disassembler.attach(
            oraa_immediate_disassembly_machine.execution()
        ),
        "Immediate ORAA disassembler attach failed"
    );
    const auto oraa_immediate_disassembly =
        oraa_immediate_disassembler.disassemble(0x8000U);
    passed &= expect(
        oraa_immediate_disassembly.has_value()
            && oraa_immediate_disassembly->supported
            && oraa_immediate_disassembly->text == "ORAA #$20"
            && oraa_immediate_disassembler.memory_access_size() == 0U,
        "Immediate ORAA disassembly differs or produced trace side effects"
    );

    Jr800Machine oraa_direct_disassembly_machine;
    std::vector<std::uint8_t> oraa_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    oraa_direct_disassembly_rom[0U] = 0x9AU;
    oraa_direct_disassembly_rom[1U] = 0x20U;
    oraa_direct_disassembly_rom[
        oraa_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    oraa_direct_disassembly_rom[
        oraa_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        oraa_direct_disassembly_machine.load_logical_rom(
            oraa_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && oraa_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct ORAA disassembly setup failed"
    );
    Debugger oraa_direct_disassembler;
    passed &= expect(
        oraa_direct_disassembler.attach(
            oraa_direct_disassembly_machine.execution()
        ),
        "Direct ORAA disassembler attach failed"
    );
    const auto oraa_direct_disassembly =
        oraa_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        oraa_direct_disassembly.has_value()
            && oraa_direct_disassembly->supported
            && oraa_direct_disassembly->text == "ORAA $20"
            && oraa_direct_disassembler.memory_access_size() == 0U,
        "Direct ORAA disassembly differs or produced trace side effects"
    );

    Jr800Machine oraa_indexed_disassembly_machine;
    std::vector<std::uint8_t> oraa_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    oraa_indexed_disassembly_rom[0U] = 0xAAU;
    oraa_indexed_disassembly_rom[1U] = 0x23U;
    oraa_indexed_disassembly_rom[
        oraa_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    oraa_indexed_disassembly_rom[
        oraa_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        oraa_indexed_disassembly_machine.load_logical_rom(
            oraa_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && oraa_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed ORAA disassembly setup failed"
    );
    Debugger oraa_indexed_disassembler;
    passed &= expect(
        oraa_indexed_disassembler.attach(
            oraa_indexed_disassembly_machine.execution()
        ),
        "Indexed ORAA disassembler attach failed"
    );
    const auto oraa_indexed_disassembly =
        oraa_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        oraa_indexed_disassembly.has_value()
            && oraa_indexed_disassembly->supported
            && oraa_indexed_disassembly->text == "ORAA $23,X"
            && oraa_indexed_disassembler.memory_access_size() == 0U,
        "Indexed ORAA disassembly differs or produced trace side effects"
    );

    Jr800Machine oraa_extended_disassembly_machine;
    std::vector<std::uint8_t> oraa_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    oraa_extended_disassembly_rom[0U] = 0xBAU;
    oraa_extended_disassembly_rom[1U] = 0x81U;
    oraa_extended_disassembly_rom[2U] = 0x23U;
    oraa_extended_disassembly_rom[
        oraa_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    oraa_extended_disassembly_rom[
        oraa_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        oraa_extended_disassembly_machine.load_logical_rom(
            oraa_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && oraa_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended ORAA disassembly setup failed"
    );
    Debugger oraa_extended_disassembler;
    passed &= expect(
        oraa_extended_disassembler.attach(
            oraa_extended_disassembly_machine.execution()
        ),
        "Extended ORAA disassembler attach failed"
    );
    const auto oraa_extended_disassembly =
        oraa_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        oraa_extended_disassembly.has_value()
            && oraa_extended_disassembly->supported
            && oraa_extended_disassembly->text == "ORAA $8123"
            && oraa_extended_disassembler.memory_access_size() == 0U,
        "Extended ORAA disassembly differs or produced trace side effects"
    );

    Jr800Machine orab_immediate_disassembly_machine;
    std::vector<std::uint8_t> orab_immediate_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    orab_immediate_disassembly_rom[0U] = 0xCAU;
    orab_immediate_disassembly_rom[1U] = 0x20U;
    orab_immediate_disassembly_rom[
        orab_immediate_disassembly_rom.size() - 2U
    ] = 0x80U;
    orab_immediate_disassembly_rom[
        orab_immediate_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        orab_immediate_disassembly_machine.load_logical_rom(
            orab_immediate_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && orab_immediate_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Immediate ORAB disassembly setup failed"
    );
    Debugger orab_immediate_disassembler;
    passed &= expect(
        orab_immediate_disassembler.attach(
            orab_immediate_disassembly_machine.execution()
        ),
        "Immediate ORAB disassembler attach failed"
    );
    const auto orab_immediate_disassembly =
        orab_immediate_disassembler.disassemble(0x8000U);
    passed &= expect(
        orab_immediate_disassembly.has_value()
            && orab_immediate_disassembly->supported
            && orab_immediate_disassembly->text == "ORAB #$20"
            && orab_immediate_disassembler.memory_access_size() == 0U,
        "Immediate ORAB disassembly differs or produced trace side effects"
    );

    Jr800Machine orab_direct_disassembly_machine;
    std::vector<std::uint8_t> orab_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    orab_direct_disassembly_rom[0U] = 0xDAU;
    orab_direct_disassembly_rom[1U] = 0x20U;
    orab_direct_disassembly_rom[
        orab_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    orab_direct_disassembly_rom[
        orab_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        orab_direct_disassembly_machine.load_logical_rom(
            orab_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && orab_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct ORAB disassembly setup failed"
    );
    Debugger orab_direct_disassembler;
    passed &= expect(
        orab_direct_disassembler.attach(
            orab_direct_disassembly_machine.execution()
        ),
        "Direct ORAB disassembler attach failed"
    );
    const auto orab_direct_disassembly =
        orab_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        orab_direct_disassembly.has_value()
            && orab_direct_disassembly->supported
            && orab_direct_disassembly->text == "ORAB $20"
            && orab_direct_disassembler.memory_access_size() == 0U,
        "Direct ORAB disassembly differs or produced trace side effects"
    );

    Jr800Machine orab_indexed_disassembly_machine;
    std::vector<std::uint8_t> orab_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    orab_indexed_disassembly_rom[0U] = 0xEAU;
    orab_indexed_disassembly_rom[1U] = 0x20U;
    orab_indexed_disassembly_rom[
        orab_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    orab_indexed_disassembly_rom[
        orab_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        orab_indexed_disassembly_machine.load_logical_rom(
            orab_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && orab_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed ORAB disassembly setup failed"
    );
    Debugger orab_indexed_disassembler;
    passed &= expect(
        orab_indexed_disassembler.attach(
            orab_indexed_disassembly_machine.execution()
        ),
        "Indexed ORAB disassembler attach failed"
    );
    const auto orab_indexed_disassembly =
        orab_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        orab_indexed_disassembly.has_value()
            && orab_indexed_disassembly->supported
            && orab_indexed_disassembly->text == "ORAB $20,X"
            && orab_indexed_disassembler.memory_access_size() == 0U,
        "Indexed ORAB disassembly differs or produced trace side effects"
    );

    Jr800Machine orab_extended_disassembly_machine;
    std::vector<std::uint8_t> orab_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    orab_extended_disassembly_rom[0U] = 0xFAU;
    orab_extended_disassembly_rom[1U] = 0x81U;
    orab_extended_disassembly_rom[2U] = 0x23U;
    orab_extended_disassembly_rom[
        orab_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    orab_extended_disassembly_rom[
        orab_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        orab_extended_disassembly_machine.load_logical_rom(
            orab_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && orab_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended ORAB disassembly setup failed"
    );
    Debugger orab_extended_disassembler;
    passed &= expect(
        orab_extended_disassembler.attach(
            orab_extended_disassembly_machine.execution()
        ),
        "Extended ORAB disassembler attach failed"
    );
    const auto orab_extended_disassembly =
        orab_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        orab_extended_disassembly.has_value()
            && orab_extended_disassembly->supported
            && orab_extended_disassembly->text == "ORAB $8123"
            && orab_extended_disassembler.memory_access_size() == 0U,
        "Extended ORAB disassembly differs or produced trace side effects"
    );

    Jr800Machine addd_immediate_disassembly_machine;
    std::vector<std::uint8_t> addd_immediate_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    addd_immediate_disassembly_rom[0U] = 0xC3U;
    addd_immediate_disassembly_rom[1U] = 0x12U;
    addd_immediate_disassembly_rom[2U] = 0x34U;
    addd_immediate_disassembly_rom[
        addd_immediate_disassembly_rom.size() - 2U
    ] = 0x80U;
    addd_immediate_disassembly_rom[
        addd_immediate_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        addd_immediate_disassembly_machine.load_logical_rom(
            addd_immediate_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && addd_immediate_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Immediate ADDD disassembly setup failed"
    );
    Debugger addd_immediate_disassembler;
    passed &= expect(
        addd_immediate_disassembler.attach(
            addd_immediate_disassembly_machine.execution()
        ),
        "Immediate ADDD disassembler attach failed"
    );
    const auto addd_immediate_disassembly =
        addd_immediate_disassembler.disassemble(0x8000U);
    passed &= expect(
        addd_immediate_disassembly.has_value()
            && addd_immediate_disassembly->supported
            && addd_immediate_disassembly->text == "ADDD #$1234"
            && addd_immediate_disassembler.memory_access_size() == 0U,
        "Immediate ADDD disassembly differs or produced trace side effects"
    );

    Jr800Machine addd_direct_disassembly_machine;
    std::vector<std::uint8_t> addd_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    addd_direct_disassembly_rom[0U] = 0xD3U;
    addd_direct_disassembly_rom[1U] = 0x20U;
    addd_direct_disassembly_rom[
        addd_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    addd_direct_disassembly_rom[
        addd_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        addd_direct_disassembly_machine.load_logical_rom(
            addd_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && addd_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct ADDD disassembly setup failed"
    );
    Debugger addd_direct_disassembler;
    passed &= expect(
        addd_direct_disassembler.attach(
            addd_direct_disassembly_machine.execution()
        ),
        "Direct ADDD disassembler attach failed"
    );
    const auto addd_direct_disassembly =
        addd_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        addd_direct_disassembly.has_value()
            && addd_direct_disassembly->supported
            && addd_direct_disassembly->text == "ADDD $20"
            && addd_direct_disassembler.memory_access_size() == 0U,
        "Direct ADDD disassembly differs or produced trace side effects"
    );

    Jr800Machine addd_indexed_disassembly_machine;
    std::vector<std::uint8_t> addd_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    addd_indexed_disassembly_rom[0U] = 0xE3U;
    addd_indexed_disassembly_rom[1U] = 0x20U;
    addd_indexed_disassembly_rom[
        addd_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    addd_indexed_disassembly_rom[
        addd_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        addd_indexed_disassembly_machine.load_logical_rom(
            addd_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && addd_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed ADDD disassembly setup failed"
    );
    Debugger addd_indexed_disassembler;
    passed &= expect(
        addd_indexed_disassembler.attach(
            addd_indexed_disassembly_machine.execution()
        ),
        "Indexed ADDD disassembler attach failed"
    );
    const auto addd_indexed_disassembly =
        addd_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        addd_indexed_disassembly.has_value()
            && addd_indexed_disassembly->supported
            && addd_indexed_disassembly->text == "ADDD $20,X"
            && addd_indexed_disassembler.memory_access_size() == 0U,
        "Indexed ADDD disassembly differs or produced trace side effects"
    );

    Jr800Machine cmpb_disassembly_machine;
    std::vector<std::uint8_t> cmpb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cmpb_disassembly_rom[0U] = 0xC1U;
    cmpb_disassembly_rom[1U] = 0x7FU;
    cmpb_disassembly_rom[cmpb_disassembly_rom.size() - 2U] = 0x80U;
    cmpb_disassembly_rom[cmpb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        cmpb_disassembly_machine.load_logical_rom(cmpb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && cmpb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "CMPB disassembly setup failed"
    );
    Debugger cmpb_disassembler;
    passed &= expect(
        cmpb_disassembler.attach(cmpb_disassembly_machine.execution()),
        "CMPB disassembler attach failed"
    );
    const auto cmpb_disassembly = cmpb_disassembler.disassemble(0x8000U);
    passed &= expect(
        cmpb_disassembly.has_value()
            && cmpb_disassembly->supported
            && cmpb_disassembly->text == "CMPB #$7F"
            && cmpb_disassembler.memory_access_size() == 0U,
        "CMPB disassembly differs or produced trace side effects"
    );

    Jr800Machine cmpb_direct_disassembly_machine;
    std::vector<std::uint8_t> cmpb_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cmpb_direct_disassembly_rom[0U] = 0xD1U;
    cmpb_direct_disassembly_rom[1U] = 0x20U;
    cmpb_direct_disassembly_rom[
        cmpb_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    cmpb_direct_disassembly_rom[
        cmpb_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        cmpb_direct_disassembly_machine.load_logical_rom(
            cmpb_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && cmpb_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct CMPB disassembly setup failed"
    );
    Debugger cmpb_direct_disassembler;
    passed &= expect(
        cmpb_direct_disassembler.attach(
            cmpb_direct_disassembly_machine.execution()
        ),
        "Direct CMPB disassembler attach failed"
    );
    const auto cmpb_direct_disassembly =
        cmpb_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        cmpb_direct_disassembly.has_value()
            && cmpb_direct_disassembly->supported
            && cmpb_direct_disassembly->text == "CMPB $20"
            && cmpb_direct_disassembler.memory_access_size() == 0U,
        "Direct CMPB disassembly differs or produced trace side effects"
    );

    Jr800Machine cmpb_indexed_disassembly_machine;
    std::vector<std::uint8_t> cmpb_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cmpb_indexed_disassembly_rom[0U] = 0xE1U;
    cmpb_indexed_disassembly_rom[1U] = 0x20U;
    cmpb_indexed_disassembly_rom[
        cmpb_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    cmpb_indexed_disassembly_rom[
        cmpb_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        cmpb_indexed_disassembly_machine.load_logical_rom(
            cmpb_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && cmpb_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed CMPB disassembly setup failed"
    );
    Debugger cmpb_indexed_disassembler;
    passed &= expect(
        cmpb_indexed_disassembler.attach(
            cmpb_indexed_disassembly_machine.execution()
        ),
        "Indexed CMPB disassembler attach failed"
    );
    const auto cmpb_indexed_disassembly =
        cmpb_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        cmpb_indexed_disassembly.has_value()
            && cmpb_indexed_disassembly->supported
            && cmpb_indexed_disassembly->text == "CMPB $20,X"
            && cmpb_indexed_disassembler.memory_access_size() == 0U,
        "Indexed CMPB disassembly differs or produced trace side effects"
    );

    Jr800Machine cmpb_extended_disassembly_machine;
    std::vector<std::uint8_t> cmpb_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cmpb_extended_disassembly_rom[0U] = 0xF1U;
    cmpb_extended_disassembly_rom[1U] = 0x81U;
    cmpb_extended_disassembly_rom[2U] = 0x23U;
    cmpb_extended_disassembly_rom[
        cmpb_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    cmpb_extended_disassembly_rom[
        cmpb_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        cmpb_extended_disassembly_machine.load_logical_rom(
            cmpb_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && cmpb_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended CMPB disassembly setup failed"
    );
    Debugger cmpb_extended_disassembler;
    passed &= expect(
        cmpb_extended_disassembler.attach(
            cmpb_extended_disassembly_machine.execution()
        ),
        "Extended CMPB disassembler attach failed"
    );
    const auto cmpb_extended_disassembly =
        cmpb_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        cmpb_extended_disassembly.has_value()
            && cmpb_extended_disassembly->supported
            && cmpb_extended_disassembly->text == "CMPB $8123"
            && cmpb_extended_disassembler.memory_access_size() == 0U,
        "Extended CMPB disassembly differs or produced trace side effects"
    );

    Jr800Machine sbca_disassembly_machine;
    std::vector<std::uint8_t> sbca_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sbca_disassembly_rom[0U] = 0x82U;
    sbca_disassembly_rom[1U] = 0x7FU;
    sbca_disassembly_rom[sbca_disassembly_rom.size() - 2U] = 0x80U;
    sbca_disassembly_rom[sbca_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        sbca_disassembly_machine.load_logical_rom(sbca_disassembly_rom)
                == Jr800MemoryStatus::ok
            && sbca_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "SBCA disassembly setup failed"
    );
    Debugger sbca_disassembler;
    passed &= expect(
        sbca_disassembler.attach(sbca_disassembly_machine.execution()),
        "SBCA disassembler attach failed"
    );
    const auto sbca_disassembly = sbca_disassembler.disassemble(0x8000U);
    passed &= expect(
        sbca_disassembly.has_value()
            && sbca_disassembly->supported
            && sbca_disassembly->text == "SBCA #$7F"
            && sbca_disassembler.memory_access_size() == 0U,
        "SBCA disassembly differs or produced trace side effects"
    );

    Jr800Machine sbcb_disassembly_machine;
    std::vector<std::uint8_t> sbcb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sbcb_disassembly_rom[0U] = 0xC2U;
    sbcb_disassembly_rom[1U] = 0x7FU;
    sbcb_disassembly_rom[sbcb_disassembly_rom.size() - 2U] = 0x80U;
    sbcb_disassembly_rom[sbcb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        sbcb_disassembly_machine.load_logical_rom(sbcb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && sbcb_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "SBCB disassembly setup failed"
    );
    Debugger sbcb_disassembler;
    passed &= expect(
        sbcb_disassembler.attach(sbcb_disassembly_machine.execution()),
        "SBCB disassembler attach failed"
    );
    const auto sbcb_disassembly = sbcb_disassembler.disassemble(0x8000U);
    passed &= expect(
        sbcb_disassembly.has_value()
            && sbcb_disassembly->supported
            && sbcb_disassembly->text == "SBCB #$7F"
            && sbcb_disassembler.memory_access_size() == 0U,
        "SBCB disassembly differs or produced trace side effects"
    );

    Jr800Machine sbcb_direct_disassembly_machine;
    std::vector<std::uint8_t> sbcb_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sbcb_direct_disassembly_rom[0U] = 0xD2U;
    sbcb_direct_disassembly_rom[1U] = 0x20U;
    sbcb_direct_disassembly_rom[
        sbcb_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    sbcb_direct_disassembly_rom[
        sbcb_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        sbcb_direct_disassembly_machine.load_logical_rom(
            sbcb_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && sbcb_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct SBCB disassembly setup failed"
    );
    Debugger sbcb_direct_disassembler;
    passed &= expect(
        sbcb_direct_disassembler.attach(
            sbcb_direct_disassembly_machine.execution()
        ),
        "Direct SBCB disassembler attach failed"
    );
    const auto sbcb_direct_disassembly =
        sbcb_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        sbcb_direct_disassembly.has_value()
            && sbcb_direct_disassembly->supported
            && sbcb_direct_disassembly->text == "SBCB $20"
            && sbcb_direct_disassembler.memory_access_size() == 0U,
        "Direct SBCB disassembly differs or produced trace side effects"
    );

    Jr800Machine sbcb_indexed_disassembly_machine;
    std::vector<std::uint8_t> sbcb_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sbcb_indexed_disassembly_rom[0U] = 0xE2U;
    sbcb_indexed_disassembly_rom[1U] = 0x20U;
    sbcb_indexed_disassembly_rom[
        sbcb_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    sbcb_indexed_disassembly_rom[
        sbcb_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        sbcb_indexed_disassembly_machine.load_logical_rom(
            sbcb_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && sbcb_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed SBCB disassembly setup failed"
    );
    Debugger sbcb_indexed_disassembler;
    passed &= expect(
        sbcb_indexed_disassembler.attach(
            sbcb_indexed_disassembly_machine.execution()
        ),
        "Indexed SBCB disassembler attach failed"
    );
    const auto sbcb_indexed_disassembly =
        sbcb_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        sbcb_indexed_disassembly.has_value()
            && sbcb_indexed_disassembly->supported
            && sbcb_indexed_disassembly->text == "SBCB $20,X"
            && sbcb_indexed_disassembler.memory_access_size() == 0U,
        "Indexed SBCB disassembly differs or produced trace side effects"
    );

    Jr800Machine sbcb_extended_disassembly_machine;
    std::vector<std::uint8_t> sbcb_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sbcb_extended_disassembly_rom[0U] = 0xF2U;
    sbcb_extended_disassembly_rom[1U] = 0x81U;
    sbcb_extended_disassembly_rom[2U] = 0x23U;
    sbcb_extended_disassembly_rom[
        sbcb_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    sbcb_extended_disassembly_rom[
        sbcb_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        sbcb_extended_disassembly_machine.load_logical_rom(
            sbcb_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && sbcb_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended SBCB disassembly setup failed"
    );
    Debugger sbcb_extended_disassembler;
    passed &= expect(
        sbcb_extended_disassembler.attach(
            sbcb_extended_disassembly_machine.execution()
        ),
        "Extended SBCB disassembler attach failed"
    );
    const auto sbcb_extended_disassembly =
        sbcb_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        sbcb_extended_disassembly.has_value()
            && sbcb_extended_disassembly->supported
            && sbcb_extended_disassembly->text == "SBCB $8123"
            && sbcb_extended_disassembler.memory_access_size() == 0U,
        "Extended SBCB disassembly differs or produced trace side effects"
    );

    Jr800Machine sbca_direct_disassembly_machine;
    std::vector<std::uint8_t> sbca_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sbca_direct_disassembly_rom[0U] = 0x92U;
    sbca_direct_disassembly_rom[1U] = 0x20U;
    sbca_direct_disassembly_rom[
        sbca_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    sbca_direct_disassembly_rom[
        sbca_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        sbca_direct_disassembly_machine.load_logical_rom(
            sbca_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && sbca_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct SBCA disassembly setup failed"
    );
    Debugger sbca_direct_disassembler;
    passed &= expect(
        sbca_direct_disassembler.attach(
            sbca_direct_disassembly_machine.execution()
        ),
        "Direct SBCA disassembler attach failed"
    );
    const auto sbca_direct_disassembly =
        sbca_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        sbca_direct_disassembly.has_value()
            && sbca_direct_disassembly->supported
            && sbca_direct_disassembly->text == "SBCA $20"
            && sbca_direct_disassembler.memory_access_size() == 0U,
        "Direct SBCA disassembly differs or produced trace side effects"
    );

    Jr800Machine sbca_indexed_disassembly_machine;
    std::vector<std::uint8_t> sbca_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sbca_indexed_disassembly_rom[0U] = 0xA2U;
    sbca_indexed_disassembly_rom[1U] = 0x20U;
    sbca_indexed_disassembly_rom[
        sbca_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    sbca_indexed_disassembly_rom[
        sbca_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        sbca_indexed_disassembly_machine.load_logical_rom(
            sbca_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && sbca_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed SBCA disassembly setup failed"
    );
    Debugger sbca_indexed_disassembler;
    passed &= expect(
        sbca_indexed_disassembler.attach(
            sbca_indexed_disassembly_machine.execution()
        ),
        "Indexed SBCA disassembler attach failed"
    );
    const auto sbca_indexed_disassembly =
        sbca_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        sbca_indexed_disassembly.has_value()
            && sbca_indexed_disassembly->supported
            && sbca_indexed_disassembly->text == "SBCA $20,X"
            && sbca_indexed_disassembler.memory_access_size() == 0U,
        "Indexed SBCA disassembly differs or produced trace side effects"
    );

    Jr800Machine sbca_extended_disassembly_machine;
    std::vector<std::uint8_t> sbca_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sbca_extended_disassembly_rom[0U] = 0xB2U;
    sbca_extended_disassembly_rom[1U] = 0x81U;
    sbca_extended_disassembly_rom[2U] = 0x23U;
    sbca_extended_disassembly_rom[
        sbca_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    sbca_extended_disassembly_rom[
        sbca_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        sbca_extended_disassembly_machine.load_logical_rom(
            sbca_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && sbca_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended SBCA disassembly setup failed"
    );
    Debugger sbca_extended_disassembler;
    passed &= expect(
        sbca_extended_disassembler.attach(
            sbca_extended_disassembly_machine.execution()
        ),
        "Extended SBCA disassembler attach failed"
    );
    const auto sbca_extended_disassembly =
        sbca_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        sbca_extended_disassembly.has_value()
            && sbca_extended_disassembly->supported
            && sbca_extended_disassembly->text == "SBCA $8123"
            && sbca_extended_disassembler.memory_access_size() == 0U,
        "Extended SBCA disassembly differs or produced trace side effects"
    );

    const auto reset_dex_disassembly =
        reset_jump_debugger.disassemble(0x8134U);
    passed &= expect(
        reset_dex_disassembly.has_value()
            && reset_dex_disassembly->text == "DEX"
            && reset_jump_debugger.memory_access_size() == 8U,
        "DEX disassembly differs or produced trace side effects"
    );
    const auto reset_stx_disassembly =
        reset_jump_debugger.disassemble(0x8135U);
    passed &= expect(
        reset_stx_disassembly.has_value()
            && reset_stx_disassembly->text == "STX $80"
            && reset_jump_debugger.memory_access_size() == 8U,
        "Direct STX disassembly differs or produced trace side effects"
    );

    Jr800Machine stx_extended_disassembly_machine;
    std::vector<std::uint8_t> stx_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    stx_extended_disassembly_rom[0U] = 0xFFU;
    stx_extended_disassembly_rom[1U] = 0x12U;
    stx_extended_disassembly_rom[2U] = 0x34U;
    stx_extended_disassembly_rom[3U] = 0xEFU;
    stx_extended_disassembly_rom[4U] = 0x20U;
    stx_extended_disassembly_rom[
        stx_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    stx_extended_disassembly_rom[
        stx_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        stx_extended_disassembly_machine.load_logical_rom(
            stx_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && stx_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended STX disassembly setup failed"
    );
    Debugger stx_extended_disassembler;
    passed &= expect(
        stx_extended_disassembler.attach(
            stx_extended_disassembly_machine.execution()
        ),
        "Extended STX disassembler attach failed"
    );
    const auto stx_extended_disassembly =
        stx_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        stx_extended_disassembly.has_value()
            && stx_extended_disassembly->supported
            && stx_extended_disassembly->text == "STX $1234"
            && stx_extended_disassembler.memory_access_size() == 0U,
        "Extended STX disassembly differs or produced trace side effects"
    );
    const auto stx_indexed_disassembly =
        stx_extended_disassembler.disassemble(0x8003U);
    passed &= expect(
        stx_indexed_disassembly.has_value()
            && stx_indexed_disassembly->supported
            && stx_indexed_disassembly->text == "STX $20,X"
            && stx_extended_disassembler.memory_access_size() == 0U,
        "Indexed STX disassembly differs or produced trace side effects"
    );

    Jr800Machine sts_addressed_disassembly_machine;
    std::vector<std::uint8_t> sts_addressed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sts_addressed_disassembly_rom[0U] = 0xAFU;
    sts_addressed_disassembly_rom[1U] = 0x20U;
    sts_addressed_disassembly_rom[2U] = 0xBFU;
    sts_addressed_disassembly_rom[3U] = 0x12U;
    sts_addressed_disassembly_rom[4U] = 0x34U;
    sts_addressed_disassembly_rom[
        sts_addressed_disassembly_rom.size() - 2U
    ] = 0x80U;
    sts_addressed_disassembly_rom[
        sts_addressed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        sts_addressed_disassembly_machine.load_logical_rom(
            sts_addressed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && sts_addressed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Addressed STS disassembly setup failed"
    );
    Debugger sts_addressed_disassembler;
    passed &= expect(
        sts_addressed_disassembler.attach(
            sts_addressed_disassembly_machine.execution()
        ),
        "Addressed STS disassembler attach failed"
    );
    const auto sts_indexed_disassembly =
        sts_addressed_disassembler.disassemble(0x8000U);
    const auto sts_extended_disassembly =
        sts_addressed_disassembler.disassemble(0x8002U);
    passed &= expect(
        sts_indexed_disassembly.has_value()
            && sts_indexed_disassembly->supported
            && sts_indexed_disassembly->text == "STS $20,X"
            && sts_extended_disassembly.has_value()
            && sts_extended_disassembly->supported
            && sts_extended_disassembly->text == "STS $1234"
            && sts_addressed_disassembler.memory_access_size() == 0U,
        "Addressed STS disassembly differs or produced trace side effects"
    );

    Jr800Machine knowledge_machine;
    std::vector<std::uint8_t> knowledge_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    knowledge_rom[0U] = 0x86U;
    knowledge_rom[1U] = 0x80U;
    knowledge_rom[knowledge_rom.size() - 2U] = 0x80U;
    knowledge_rom[knowledge_rom.size() - 1U] = 0x00U;
    passed &= expect(
        knowledge_machine.load_logical_rom(knowledge_rom)
                == Jr800MemoryStatus::ok
            && knowledge_machine.initialize_from_reset_entry().succeeded(),
        "Known-state propagation setup failed"
    );
    const auto known_load = knowledge_machine.execution().step_instruction();
    const auto& known_load_state = knowledge_machine.execution().cpu().state();
    const auto nzv_mask = static_cast<std::uint8_t>(
        jr800::core::condition_mask(jr800::core::ConditionCode::negative)
        | jr800::core::condition_mask(jr800::core::ConditionCode::zero)
        | jr800::core::condition_mask(jr800::core::ConditionCode::overflow)
    );
    passed &= expect(
        known_load.succeeded()
            && known_load_state.a == 0x80U
            && known_load_state.knowledge.knows(CpuRegister::accumulator_a)
            && (known_load_state.knowledge.condition_code & nzv_mask)
                == nzv_mask
            && known_load_state.knowledge.condition_code
                == static_cast<std::uint8_t>(
                    expected_known_condition_code | nzv_mask
                ),
        "Instruction outputs did not become known from reset state"
    );

    Jr800Machine unknown_state_machine;
    std::vector<std::uint8_t> unknown_state_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    unknown_state_rom[0U] = 0x8DU;
    unknown_state_rom[1U] = 0x00U;
    unknown_state_rom[unknown_state_rom.size() - 2U] = 0x80U;
    unknown_state_rom[unknown_state_rom.size() - 1U] = 0x00U;
    passed &= expect(
        unknown_state_machine.load_logical_rom(unknown_state_rom)
                == Jr800MemoryStatus::ok
            && unknown_state_machine.initialize_from_reset_entry().succeeded(),
        "Unknown-state JR-800 setup failed"
    );
    Debugger unknown_state_debugger;
    passed &= expect(
        unknown_state_debugger.attach(unknown_state_machine.execution()),
        "Unknown-state debugger attach failed"
    );
    const auto unknown_before = unknown_state_machine.execution().cpu().state();
    const auto unknown_sp = unknown_state_debugger.step();
    const auto unknown_history = unknown_state_debugger.history();
    passed &= expect(
        unknown_sp.reason == StopReason::cpu_fault
            && unknown_sp.step.fault == CpuFault::unknown_state
            && unknown_sp.step.state_fault == CpuStatePart::stack_pointer
            && unknown_sp.step.bytes_fetched == 2U
            && unknown_sp.step.cycles == 0U
            && unknown_state_machine.execution().cpu().state() == unknown_before
            && unknown_history.size() == 1U
            && unknown_history.front().state_fault
                == CpuStatePart::stack_pointer
            && unknown_history.front().access_count == 2U,
        "Unknown reset SP was consumed as a concrete value"
    );

    Jr800Machine pin_state_machine;
    pin_state_machine.set_port1_pin_state(0xC3U, 0xFFU);
    pin_state_machine.set_port2_pin_state(0x12U, 0x1FU);
    passed &= expect(
        pin_state_machine.load_logical_rom(rom) == Jr800MemoryStatus::ok
            && pin_state_machine.initialize_from_reset_entry().succeeded(),
        "Port pin-state machine setup failed"
    );
    const auto pin_state_after_reset =
        pin_state_machine.execution().inspect8(0x0002U);
    const auto port2_pin_state_after_reset =
        pin_state_machine.execution().inspect8(0x0003U);
    passed &= expect(
        pin_state_after_reset.succeeded()
            && pin_state_after_reset.value == 0xC3U
            && port2_pin_state_after_reset.succeeded()
            && port2_pin_state_after_reset.value == 0xD2U,
        "CPU reset changed an external port pin state"
    );
    pin_state_machine.set_port1_pin_state(0xC3U, 0x0FU);
    pin_state_machine.set_port2_pin_state(0x12U, 0x0FU);
    const auto partial_pin_state =
        pin_state_machine.execution().inspect8(0x0002U);
    const auto partial_port2_pin_state =
        pin_state_machine.execution().inspect8(0x0003U);
    passed &= expect(
        partial_pin_state.fault == BusFault::uninitialized_read
            && !partial_pin_state.value.has_value()
            && partial_port2_pin_state.fault
                == BusFault::uninitialized_read
            && !partial_port2_pin_state.value.has_value(),
        "Partial port pin evidence became a complete machine value"
    );

    Jr800Machine timer_output_machine;
    std::vector<std::uint8_t> timer_output_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    constexpr std::array<std::uint8_t, 13U> timer_output_program{
        0x86U, 0x01U,
        0x97U, 0x08U,
        0x86U, 0x02U,
        0x97U, 0x01U,
        0xCCU, 0xFFU, 0xFCU,
        0xDDU, 0x09U,
    };
    for (std::size_t index = 0U;
         index < timer_output_program.size();
         ++index) {
        timer_output_rom[index] = timer_output_program[index];
    }
    timer_output_rom[timer_output_rom.size() - 2U] = 0x80U;
    timer_output_rom[timer_output_rom.size() - 1U] = 0x00U;
    const auto initial_timer_output =
        timer_output_machine.port2_timer_output_state();
    passed &= expect(
        timer_output_machine.load_logical_rom(timer_output_rom)
                == Jr800MemoryStatus::ok
            && timer_output_machine.initialize_from_reset_entry().succeeded(),
        "Timer-output machine setup failed"
    );
    bool timer_output_program_succeeded = true;
    for (std::size_t instruction = 0U; instruction < 4U; ++instruction) {
        timer_output_program_succeeded &= timer_output_machine.execution()
            .step_instruction()
            .succeeded();
    }
    const auto enabled_unknown_timer_output =
        timer_output_machine.port2_timer_output_state();
    for (std::size_t instruction = 0U; instruction < 2U; ++instruction) {
        timer_output_program_succeeded &= timer_output_machine.execution()
            .step_instruction()
            .succeeded();
    }
    const auto enabled_high_timer_output =
        timer_output_machine.port2_timer_output_state();
    passed &= expect(
        !initial_timer_output.output_enabled
            && !initial_timer_output.level.has_value()
            && timer_output_program_succeeded
            && enabled_unknown_timer_output.output_enabled
            && !enabled_unknown_timer_output.level.has_value()
            && enabled_high_timer_output.output_enabled
            && enabled_high_timer_output.level == true,
        "Machine timer-output forwarding changed or lost the bus state"
    );

    Jr800Machine standby_machine;
    standby_machine.set_ram_standby_power_valid(false, true);
    passed &= expect(
        standby_machine.load_logical_rom(rom) == Jr800MemoryStatus::ok
            && standby_machine.initialize_from_reset_entry().succeeded(),
        "RAM standby-status machine setup failed"
    );
    const auto cleared_standby = standby_machine.execution().inspect8(0x0014U);
    passed &= expect(
        cleared_standby.succeeded()
            && cleared_standby.value == 0x7FU,
        "CPU reset changed cleared RAM standby status"
    );
    standby_machine.set_ram_standby_power_valid(false, false);
    const auto unknown_standby = standby_machine.execution().inspect8(0x0014U);
    passed &= expect(
        unknown_standby.fault == BusFault::uninitialized_read
            && !unknown_standby.value.has_value(),
        "Unknown RAM standby status became a machine value"
    );
    standby_machine.set_ram_standby_power_valid(true, true);
    passed &= expect(
        standby_machine.initialize_from_reset_entry().succeeded(),
        "RAM standby-status reset initialization failed"
    );
    const auto set_standby = standby_machine.execution().inspect8(0x0014U);
    passed &= expect(
        set_standby.succeeded() && set_standby.value == 0xFFU,
        "CPU reset changed set RAM standby status"
    );

    Jr800Machine ram_control_machine;
    std::vector<std::uint8_t> ram_control_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ram_control_rom[0U] = 0x96U;
    ram_control_rom[1U] = 0x14U;
    ram_control_rom[ram_control_rom.size() - 2U] = 0x80U;
    ram_control_rom[ram_control_rom.size() - 1U] = 0x00U;
    passed &= expect(
        ram_control_machine.load_logical_rom(ram_control_rom)
                == Jr800MemoryStatus::ok
            && ram_control_machine.initialize_from_reset_entry().succeeded(),
        "RAM control direct-load setup failed"
    );
    const auto unknown_ram_control_before =
        ram_control_machine.execution().cpu().state();
    const auto unknown_ram_control =
        ram_control_machine.execution().step_instruction();
    passed &= expect(
        unknown_ram_control.fault == CpuFault::bus_access
            && unknown_ram_control.bus_fault == BusFault::uninitialized_read
            && unknown_ram_control.bytes_fetched == 2U
            && ram_control_machine.execution().cpu().state()
                == unknown_ram_control_before,
        "Direct LDAA consumed unknown RAM standby status"
    );
    ram_control_machine.set_ram_standby_power_valid(false, true);
    passed &= expect(
        ram_control_machine.initialize_from_reset_entry().succeeded(),
        "Known RAM control direct-load reset failed"
    );
    const auto known_ram_control =
        ram_control_machine.execution().step_instruction();
    passed &= expect(
        known_ram_control.succeeded() && known_ram_control.cycles == 3U
            && ram_control_machine.execution().cpu().state().pc == 0x8002U
            && ram_control_machine.execution().cpu().state().a == 0x7FU
            && ram_control_machine.execution().cpu().state().knowledge.knows(
                CpuRegister::accumulator_a
            ),
        "Direct LDAA did not consume known RAM control state"
    );

    Jr800Machine ldab_disassembly_machine;
    std::vector<std::uint8_t> ldab_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ldab_disassembly_rom[0U] = 0xC6U;
    ldab_disassembly_rom[1U] = 0xA5U;
    ldab_disassembly_rom[ldab_disassembly_rom.size() - 2U] = 0x80U;
    ldab_disassembly_rom[ldab_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        ldab_disassembly_machine.load_logical_rom(ldab_disassembly_rom)
                == Jr800MemoryStatus::ok
            && ldab_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "Immediate LDAB disassembly setup failed"
    );
    Debugger ldab_disassembler;
    passed &= expect(
        ldab_disassembler.attach(ldab_disassembly_machine.execution()),
        "Immediate LDAB disassembler attach failed"
    );
    const auto ldab_disassembly = ldab_disassembler.disassemble(0x8000U);
    passed &= expect(
        ldab_disassembly.has_value()
            && ldab_disassembly->supported
            && ldab_disassembly->text == "LDAB #$A5"
            && ldab_disassembler.memory_access_size() == 0U,
        "Immediate LDAB disassembly differs or produced trace side effects"
    );

    Jr800Machine ldab_direct_disassembly_machine;
    std::vector<std::uint8_t> ldab_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ldab_direct_disassembly_rom[0U] = 0xD6U;
    ldab_direct_disassembly_rom[1U] = 0x20U;
    ldab_direct_disassembly_rom[
        ldab_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    ldab_direct_disassembly_rom[
        ldab_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        ldab_direct_disassembly_machine.load_logical_rom(
            ldab_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && ldab_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct LDAB disassembly setup failed"
    );
    Debugger ldab_direct_disassembler;
    passed &= expect(
        ldab_direct_disassembler.attach(
            ldab_direct_disassembly_machine.execution()
        ),
        "Direct LDAB disassembler attach failed"
    );
    const auto ldab_direct_disassembly =
        ldab_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        ldab_direct_disassembly.has_value()
            && ldab_direct_disassembly->supported
            && ldab_direct_disassembly->text == "LDAB $20"
            && ldab_direct_disassembler.memory_access_size() == 0U,
        "Direct LDAB disassembly differs or produced trace side effects"
    );

    Jr800Machine ldab_indexed_disassembly_machine;
    std::vector<std::uint8_t> ldab_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ldab_indexed_disassembly_rom[0U] = 0xE6U;
    ldab_indexed_disassembly_rom[1U] = 0x23U;
    ldab_indexed_disassembly_rom[
        ldab_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    ldab_indexed_disassembly_rom[
        ldab_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        ldab_indexed_disassembly_machine.load_logical_rom(
            ldab_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && ldab_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed LDAB disassembly setup failed"
    );
    Debugger ldab_indexed_disassembler;
    passed &= expect(
        ldab_indexed_disassembler.attach(
            ldab_indexed_disassembly_machine.execution()
        ),
        "Indexed LDAB disassembler attach failed"
    );
    const auto ldab_indexed_disassembly =
        ldab_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        ldab_indexed_disassembly.has_value()
            && ldab_indexed_disassembly->supported
            && ldab_indexed_disassembly->text == "LDAB $23,X"
            && ldab_indexed_disassembler.memory_access_size() == 0U,
        "Indexed LDAB disassembly differs or produced trace side effects"
    );

    Jr800Machine ldab_extended_disassembly_machine;
    std::vector<std::uint8_t> ldab_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ldab_extended_disassembly_rom[0U] = 0xF6U;
    ldab_extended_disassembly_rom[1U] = 0x12U;
    ldab_extended_disassembly_rom[2U] = 0x34U;
    ldab_extended_disassembly_rom[
        ldab_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    ldab_extended_disassembly_rom[
        ldab_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        ldab_extended_disassembly_machine.load_logical_rom(
            ldab_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && ldab_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended LDAB disassembly setup failed"
    );
    Debugger ldab_extended_disassembler;
    passed &= expect(
        ldab_extended_disassembler.attach(
            ldab_extended_disassembly_machine.execution()
        ),
        "Extended LDAB disassembler attach failed"
    );
    const auto ldab_extended_disassembly =
        ldab_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        ldab_extended_disassembly.has_value()
            && ldab_extended_disassembly->supported
            && ldab_extended_disassembly->text == "LDAB $1234"
            && ldab_extended_disassembler.memory_access_size() == 0U,
        "Extended LDAB disassembly differs or produced trace side effects"
    );

    Jr800Machine stab_direct_disassembly_machine;
    std::vector<std::uint8_t> stab_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    stab_direct_disassembly_rom[0U] = 0xD7U;
    stab_direct_disassembly_rom[1U] = 0x20U;
    stab_direct_disassembly_rom[2U] = 0xF7U;
    stab_direct_disassembly_rom[3U] = 0x12U;
    stab_direct_disassembly_rom[4U] = 0x34U;
    stab_direct_disassembly_rom[
        stab_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    stab_direct_disassembly_rom[
        stab_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        stab_direct_disassembly_machine.load_logical_rom(
            stab_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && stab_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct STAB disassembly setup failed"
    );
    Debugger stab_direct_disassembler;
    passed &= expect(
        stab_direct_disassembler.attach(
            stab_direct_disassembly_machine.execution()
        ),
        "Direct STAB disassembler attach failed"
    );
    const auto stab_direct_disassembly =
        stab_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        stab_direct_disassembly.has_value()
            && stab_direct_disassembly->supported
            && stab_direct_disassembly->text == "STAB $20"
            && stab_direct_disassembler.memory_access_size() == 0U,
        "Direct STAB disassembly differs or produced trace side effects"
    );
    const auto stab_extended_disassembly =
        stab_direct_disassembler.disassemble(0x8002U);
    passed &= expect(
        stab_extended_disassembly.has_value()
            && stab_extended_disassembly->supported
            && stab_extended_disassembly->text == "STAB $1234"
            && stab_direct_disassembler.memory_access_size() == 0U,
        "Extended STAB disassembly differs or produced trace side effects"
    );

    Jr800Machine ldd_disassembly_machine;
    std::vector<std::uint8_t> ldd_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ldd_disassembly_rom[0U] = 0xCCU;
    ldd_disassembly_rom[1U] = 0x12U;
    ldd_disassembly_rom[2U] = 0x34U;
    ldd_disassembly_rom[3U] = 0xFCU;
    ldd_disassembly_rom[4U] = 0x56U;
    ldd_disassembly_rom[5U] = 0x78U;
    ldd_disassembly_rom[ldd_disassembly_rom.size() - 2U] = 0x80U;
    ldd_disassembly_rom[ldd_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        ldd_disassembly_machine.load_logical_rom(ldd_disassembly_rom)
                == Jr800MemoryStatus::ok
            && ldd_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "Immediate LDD disassembly setup failed"
    );
    Debugger ldd_disassembler;
    passed &= expect(
        ldd_disassembler.attach(ldd_disassembly_machine.execution()),
        "Immediate LDD disassembler attach failed"
    );
    const auto ldd_disassembly = ldd_disassembler.disassemble(0x8000U);
    passed &= expect(
        ldd_disassembly.has_value()
            && ldd_disassembly->supported
            && ldd_disassembly->text == "LDD #$1234"
            && ldd_disassembler.memory_access_size() == 0U,
        "Immediate LDD disassembly differs or produced trace side effects"
    );
    const auto ldd_extended_disassembly =
        ldd_disassembler.disassemble(0x8003U);
    passed &= expect(
        ldd_extended_disassembly.has_value()
            && ldd_extended_disassembly->supported
            && ldd_extended_disassembly->text == "LDD $5678"
            && ldd_disassembler.memory_access_size() == 0U,
        "Extended LDD disassembly differs or produced trace side effects"
    );

    Jr800Machine tab_disassembly_machine;
    std::vector<std::uint8_t> tab_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    tab_disassembly_rom[0U] = 0x16U;
    tab_disassembly_rom[tab_disassembly_rom.size() - 2U] = 0x80U;
    tab_disassembly_rom[tab_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        tab_disassembly_machine.load_logical_rom(tab_disassembly_rom)
                == Jr800MemoryStatus::ok
            && tab_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "TAB disassembly setup failed"
    );
    Debugger tab_disassembler;
    passed &= expect(
        tab_disassembler.attach(tab_disassembly_machine.execution()),
        "TAB disassembler attach failed"
    );
    const auto tab_disassembly = tab_disassembler.disassemble(0x8000U);
    passed &= expect(
        tab_disassembly.has_value()
            && tab_disassembly->supported
            && tab_disassembly->text == "TAB"
            && tab_disassembler.memory_access_size() == 0U,
        "TAB disassembly differs or produced trace side effects"
    );

    Jr800Machine tba_disassembly_machine;
    std::vector<std::uint8_t> tba_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    tba_disassembly_rom[0U] = 0x17U;
    tba_disassembly_rom[tba_disassembly_rom.size() - 2U] = 0x80U;
    tba_disassembly_rom[tba_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        tba_disassembly_machine.load_logical_rom(tba_disassembly_rom)
                == Jr800MemoryStatus::ok
            && tba_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "TBA disassembly setup failed"
    );
    Debugger tba_disassembler;
    passed &= expect(
        tba_disassembler.attach(tba_disassembly_machine.execution()),
        "TBA disassembler attach failed"
    );
    const auto tba_disassembly = tba_disassembler.disassemble(0x8000U);
    passed &= expect(
        tba_disassembly.has_value()
            && tba_disassembly->supported
            && tba_disassembly->text == "TBA"
            && tba_disassembler.memory_access_size() == 0U,
        "TBA disassembly differs or produced trace side effects"
    );

    Jr800Machine aba_disassembly_machine;
    std::vector<std::uint8_t> aba_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    aba_disassembly_rom[0U] = 0x1BU;
    aba_disassembly_rom[aba_disassembly_rom.size() - 2U] = 0x80U;
    aba_disassembly_rom[aba_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        aba_disassembly_machine.load_logical_rom(aba_disassembly_rom)
                == Jr800MemoryStatus::ok
            && aba_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "ABA disassembly setup failed"
    );
    Debugger aba_disassembler;
    passed &= expect(
        aba_disassembler.attach(aba_disassembly_machine.execution()),
        "ABA disassembler attach failed"
    );
    const auto aba_disassembly = aba_disassembler.disassemble(0x8000U);
    passed &= expect(
        aba_disassembly.has_value()
            && aba_disassembly->supported
            && aba_disassembly->text == "ABA"
            && aba_disassembler.memory_access_size() == 0U,
        "ABA disassembly differs or produced trace side effects"
    );

    Jr800Machine cba_disassembly_machine;
    std::vector<std::uint8_t> cba_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cba_disassembly_rom[0U] = 0x11U;
    cba_disassembly_rom[cba_disassembly_rom.size() - 2U] = 0x80U;
    cba_disassembly_rom[cba_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        cba_disassembly_machine.load_logical_rom(cba_disassembly_rom)
                == Jr800MemoryStatus::ok
            && cba_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "CBA disassembly setup failed"
    );
    Debugger cba_disassembler;
    passed &= expect(
        cba_disassembler.attach(cba_disassembly_machine.execution()),
        "CBA disassembler attach failed"
    );
    const auto cba_disassembly = cba_disassembler.disassemble(0x8000U);
    passed &= expect(
        cba_disassembly.has_value()
            && cba_disassembly->supported
            && cba_disassembly->text == "CBA"
            && cba_disassembler.memory_access_size() == 0U,
        "CBA disassembly differs or produced trace side effects"
    );

    Jr800Machine sba_disassembly_machine;
    std::vector<std::uint8_t> sba_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sba_disassembly_rom[0U] = 0x10U;
    sba_disassembly_rom[sba_disassembly_rom.size() - 2U] = 0x80U;
    sba_disassembly_rom[sba_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        sba_disassembly_machine.load_logical_rom(sba_disassembly_rom)
                == Jr800MemoryStatus::ok
            && sba_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "SBA disassembly setup failed"
    );
    Debugger sba_disassembler;
    passed &= expect(
        sba_disassembler.attach(sba_disassembly_machine.execution()),
        "SBA disassembler attach failed"
    );
    const auto sba_disassembly = sba_disassembler.disassemble(0x8000U);
    passed &= expect(
        sba_disassembly.has_value()
            && sba_disassembly->supported
            && sba_disassembly->text == "SBA"
            && sba_disassembler.memory_access_size() == 0U,
        "SBA disassembly differs or produced trace side effects"
    );

    Jr800Machine lsrd_disassembly_machine;
    std::vector<std::uint8_t> lsrd_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    lsrd_disassembly_rom[0U] = 0x04U;
    lsrd_disassembly_rom[lsrd_disassembly_rom.size() - 2U] = 0x80U;
    lsrd_disassembly_rom[lsrd_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        lsrd_disassembly_machine.load_logical_rom(lsrd_disassembly_rom)
                == Jr800MemoryStatus::ok
            && lsrd_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "LSRD disassembly setup failed"
    );
    Debugger lsrd_disassembler;
    passed &= expect(
        lsrd_disassembler.attach(lsrd_disassembly_machine.execution()),
        "LSRD disassembler attach failed"
    );
    const auto lsrd_disassembly = lsrd_disassembler.disassemble(0x8000U);
    passed &= expect(
        lsrd_disassembly.has_value()
            && lsrd_disassembly->supported
            && lsrd_disassembly->text == "LSRD"
            && lsrd_disassembler.memory_access_size() == 0U,
        "LSRD disassembly differs or produced trace side effects"
    );

    Jr800Machine asld_disassembly_machine;
    std::vector<std::uint8_t> asld_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    asld_disassembly_rom[0U] = 0x05U;
    asld_disassembly_rom[asld_disassembly_rom.size() - 2U] = 0x80U;
    asld_disassembly_rom[asld_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        asld_disassembly_machine.load_logical_rom(asld_disassembly_rom)
                == Jr800MemoryStatus::ok
            && asld_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "ASLD disassembly setup failed"
    );
    Debugger asld_disassembler;
    passed &= expect(
        asld_disassembler.attach(asld_disassembly_machine.execution()),
        "ASLD disassembler attach failed"
    );
    const auto asld_disassembly = asld_disassembler.disassemble(0x8000U);
    passed &= expect(
        asld_disassembly.has_value()
            && asld_disassembly->supported
            && asld_disassembly->text == "ASLD"
            && asld_disassembler.memory_access_size() == 0U,
        "ASLD disassembly differs or produced trace side effects"
    );

    Jr800Machine abx_disassembly_machine;
    std::vector<std::uint8_t> abx_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    abx_disassembly_rom[0U] = 0x3AU;
    abx_disassembly_rom[abx_disassembly_rom.size() - 2U] = 0x80U;
    abx_disassembly_rom[abx_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        abx_disassembly_machine.load_logical_rom(abx_disassembly_rom)
                == Jr800MemoryStatus::ok
            && abx_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "ABX disassembly setup failed"
    );
    Debugger abx_disassembler;
    passed &= expect(
        abx_disassembler.attach(abx_disassembly_machine.execution()),
        "ABX disassembler attach failed"
    );
    const auto abx_disassembly = abx_disassembler.disassemble(0x8000U);
    passed &= expect(
        abx_disassembly.has_value()
            && abx_disassembly->supported
            && abx_disassembly->text == "ABX"
            && abx_disassembler.memory_access_size() == 0U,
        "ABX disassembly differs or produced trace side effects"
    );

    Jr800Machine xgdx_disassembly_machine;
    std::vector<std::uint8_t> xgdx_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    xgdx_disassembly_rom[0U] = 0x18U;
    xgdx_disassembly_rom[xgdx_disassembly_rom.size() - 2U] = 0x80U;
    xgdx_disassembly_rom[xgdx_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        xgdx_disassembly_machine.load_logical_rom(xgdx_disassembly_rom)
                == Jr800MemoryStatus::ok
            && xgdx_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "XGDX disassembly setup failed"
    );
    Debugger xgdx_disassembler;
    passed &= expect(
        xgdx_disassembler.attach(xgdx_disassembly_machine.execution()),
        "XGDX disassembler attach failed"
    );
    const auto xgdx_disassembly = xgdx_disassembler.disassemble(0x8000U);
    passed &= expect(
        xgdx_disassembly.has_value()
            && xgdx_disassembly->supported
            && xgdx_disassembly->text == "XGDX"
            && xgdx_disassembler.memory_access_size() == 0U,
        "XGDX disassembly differs or produced trace side effects"
    );

    Jr800Machine mul_disassembly_machine;
    std::vector<std::uint8_t> mul_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    mul_disassembly_rom[0U] = 0x3DU;
    mul_disassembly_rom[mul_disassembly_rom.size() - 2U] = 0x80U;
    mul_disassembly_rom[mul_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        mul_disassembly_machine.load_logical_rom(mul_disassembly_rom)
                == Jr800MemoryStatus::ok
            && mul_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "MUL disassembly setup failed"
    );
    Debugger mul_disassembler;
    passed &= expect(
        mul_disassembler.attach(mul_disassembly_machine.execution()),
        "MUL disassembler attach failed"
    );
    const auto mul_disassembly = mul_disassembler.disassemble(0x8000U);
    passed &= expect(
        mul_disassembly.has_value()
            && mul_disassembly->supported
            && mul_disassembly->text == "MUL"
            && mul_disassembler.memory_access_size() == 0U,
        "MUL disassembly differs or produced trace side effects"
    );

    Jr800Machine nega_disassembly_machine;
    std::vector<std::uint8_t> nega_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    nega_disassembly_rom[0U] = 0x40U;
    nega_disassembly_rom[nega_disassembly_rom.size() - 2U] = 0x80U;
    nega_disassembly_rom[nega_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        nega_disassembly_machine.load_logical_rom(nega_disassembly_rom)
                == Jr800MemoryStatus::ok
            && nega_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "NEGA disassembly setup failed"
    );
    Debugger nega_disassembler;
    passed &= expect(
        nega_disassembler.attach(nega_disassembly_machine.execution()),
        "NEGA disassembler attach failed"
    );
    const auto nega_disassembly = nega_disassembler.disassemble(0x8000U);
    passed &= expect(
        nega_disassembly.has_value()
            && nega_disassembly->supported
            && nega_disassembly->text == "NEGA"
            && nega_disassembler.memory_access_size() == 0U,
        "NEGA disassembly differs or produced trace side effects"
    );

    Jr800Machine negb_disassembly_machine;
    std::vector<std::uint8_t> negb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    negb_disassembly_rom[0U] = 0x50U;
    negb_disassembly_rom[negb_disassembly_rom.size() - 2U] = 0x80U;
    negb_disassembly_rom[negb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        negb_disassembly_machine.load_logical_rom(negb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && negb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "NEGB disassembly setup failed"
    );
    Debugger negb_disassembler;
    passed &= expect(
        negb_disassembler.attach(negb_disassembly_machine.execution()),
        "NEGB disassembler attach failed"
    );
    const auto negb_disassembly = negb_disassembler.disassemble(0x8000U);
    passed &= expect(
        negb_disassembly.has_value()
            && negb_disassembly->supported
            && negb_disassembly->text == "NEGB"
            && negb_disassembler.memory_access_size() == 0U,
        "NEGB disassembly differs or produced trace side effects"
    );

    Jr800Machine incb_disassembly_machine;
    std::vector<std::uint8_t> incb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    incb_disassembly_rom[0U] = 0x5CU;
    incb_disassembly_rom[incb_disassembly_rom.size() - 2U] = 0x80U;
    incb_disassembly_rom[incb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        incb_disassembly_machine.load_logical_rom(incb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && incb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "INCB disassembly setup failed"
    );
    Debugger incb_disassembler;
    passed &= expect(
        incb_disassembler.attach(incb_disassembly_machine.execution()),
        "INCB disassembler attach failed"
    );
    const auto incb_disassembly = incb_disassembler.disassemble(0x8000U);
    passed &= expect(
        incb_disassembly.has_value()
            && incb_disassembly->supported
            && incb_disassembly->text == "INCB"
            && incb_disassembler.memory_access_size() == 0U,
        "INCB disassembly differs or produced trace side effects"
    );

    Jr800Machine coma_disassembly_machine;
    std::vector<std::uint8_t> coma_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    coma_disassembly_rom[0U] = 0x43U;
    coma_disassembly_rom[coma_disassembly_rom.size() - 2U] = 0x80U;
    coma_disassembly_rom[coma_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        coma_disassembly_machine.load_logical_rom(coma_disassembly_rom)
                == Jr800MemoryStatus::ok
            && coma_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "COMA disassembly setup failed"
    );
    Debugger coma_disassembler;
    passed &= expect(
        coma_disassembler.attach(coma_disassembly_machine.execution()),
        "COMA disassembler attach failed"
    );
    const auto coma_disassembly = coma_disassembler.disassemble(0x8000U);
    passed &= expect(
        coma_disassembly.has_value()
            && coma_disassembly->supported
            && coma_disassembly->text == "COMA"
            && coma_disassembler.memory_access_size() == 0U,
        "COMA disassembly differs or produced trace side effects"
    );

    Jr800Machine comb_disassembly_machine;
    std::vector<std::uint8_t> comb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    comb_disassembly_rom[0U] = 0x53U;
    comb_disassembly_rom[comb_disassembly_rom.size() - 2U] = 0x80U;
    comb_disassembly_rom[comb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        comb_disassembly_machine.load_logical_rom(comb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && comb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "COMB disassembly setup failed"
    );
    Debugger comb_disassembler;
    passed &= expect(
        comb_disassembler.attach(comb_disassembly_machine.execution()),
        "COMB disassembler attach failed"
    );
    const auto comb_disassembly = comb_disassembler.disassemble(0x8000U);
    passed &= expect(
        comb_disassembly.has_value()
            && comb_disassembly->supported
            && comb_disassembly->text == "COMB"
            && comb_disassembler.memory_access_size() == 0U,
        "COMB disassembly differs or produced trace side effects"
    );

    Jr800Machine lsra_disassembly_machine;
    std::vector<std::uint8_t> lsra_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    lsra_disassembly_rom[0U] = 0x44U;
    lsra_disassembly_rom[lsra_disassembly_rom.size() - 2U] = 0x80U;
    lsra_disassembly_rom[lsra_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        lsra_disassembly_machine.load_logical_rom(lsra_disassembly_rom)
                == Jr800MemoryStatus::ok
            && lsra_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "LSRA disassembly setup failed"
    );
    Debugger lsra_disassembler;
    passed &= expect(
        lsra_disassembler.attach(lsra_disassembly_machine.execution()),
        "LSRA disassembler attach failed"
    );
    const auto lsra_disassembly = lsra_disassembler.disassemble(0x8000U);
    passed &= expect(
        lsra_disassembly.has_value()
            && lsra_disassembly->supported
            && lsra_disassembly->text == "LSRA"
            && lsra_disassembler.memory_access_size() == 0U,
        "LSRA disassembly differs or produced trace side effects"
    );

    Jr800Machine lsrb_disassembly_machine;
    std::vector<std::uint8_t> lsrb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    lsrb_disassembly_rom[0U] = 0x54U;
    lsrb_disassembly_rom[lsrb_disassembly_rom.size() - 2U] = 0x80U;
    lsrb_disassembly_rom[lsrb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        lsrb_disassembly_machine.load_logical_rom(lsrb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && lsrb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "LSRB disassembly setup failed"
    );
    Debugger lsrb_disassembler;
    passed &= expect(
        lsrb_disassembler.attach(lsrb_disassembly_machine.execution()),
        "LSRB disassembler attach failed"
    );
    const auto lsrb_disassembly = lsrb_disassembler.disassemble(0x8000U);
    passed &= expect(
        lsrb_disassembly.has_value()
            && lsrb_disassembly->supported
            && lsrb_disassembly->text == "LSRB"
            && lsrb_disassembler.memory_access_size() == 0U,
        "LSRB disassembly differs or produced trace side effects"
    );

    Jr800Machine rola_disassembly_machine;
    std::vector<std::uint8_t> rola_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    rola_disassembly_rom[0U] = 0x49U;
    rola_disassembly_rom[rola_disassembly_rom.size() - 2U] = 0x80U;
    rola_disassembly_rom[rola_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        rola_disassembly_machine.load_logical_rom(rola_disassembly_rom)
                == Jr800MemoryStatus::ok
            && rola_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "ROLA disassembly setup failed"
    );
    Debugger rola_disassembler;
    passed &= expect(
        rola_disassembler.attach(rola_disassembly_machine.execution()),
        "ROLA disassembler attach failed"
    );
    const auto rola_disassembly = rola_disassembler.disassemble(0x8000U);
    passed &= expect(
        rola_disassembly.has_value()
            && rola_disassembly->supported
            && rola_disassembly->text == "ROLA"
            && rola_disassembler.memory_access_size() == 0U,
        "ROLA disassembly differs or produced trace side effects"
    );

    Jr800Machine rolb_disassembly_machine;
    std::vector<std::uint8_t> rolb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    rolb_disassembly_rom[0U] = 0x59U;
    rolb_disassembly_rom[rolb_disassembly_rom.size() - 2U] = 0x80U;
    rolb_disassembly_rom[rolb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        rolb_disassembly_machine.load_logical_rom(rolb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && rolb_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "ROLB disassembly setup failed"
    );
    Debugger rolb_disassembler;
    passed &= expect(
        rolb_disassembler.attach(rolb_disassembly_machine.execution()),
        "ROLB disassembler attach failed"
    );
    const auto rolb_disassembly = rolb_disassembler.disassemble(0x8000U);
    passed &= expect(
        rolb_disassembly.has_value()
            && rolb_disassembly->supported
            && rolb_disassembly->text == "ROLB"
            && rolb_disassembler.memory_access_size() == 0U,
        "ROLB disassembly differs or produced trace side effects"
    );

    Jr800Machine rora_disassembly_machine;
    std::vector<std::uint8_t> rora_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    rora_disassembly_rom[0U] = 0x46U;
    rora_disassembly_rom[rora_disassembly_rom.size() - 2U] = 0x80U;
    rora_disassembly_rom[rora_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        rora_disassembly_machine.load_logical_rom(rora_disassembly_rom)
                == Jr800MemoryStatus::ok
            && rora_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "RORA disassembly setup failed"
    );
    Debugger rora_disassembler;
    passed &= expect(
        rora_disassembler.attach(rora_disassembly_machine.execution()),
        "RORA disassembler attach failed"
    );
    const auto rora_disassembly = rora_disassembler.disassemble(0x8000U);
    passed &= expect(
        rora_disassembly.has_value()
            && rora_disassembly->supported
            && rora_disassembly->text == "RORA"
            && rora_disassembler.memory_access_size() == 0U,
        "RORA disassembly differs or produced trace side effects"
    );

    Jr800Machine rorb_disassembly_machine;
    std::vector<std::uint8_t> rorb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    rorb_disassembly_rom[0U] = 0x56U;
    rorb_disassembly_rom[rorb_disassembly_rom.size() - 2U] = 0x80U;
    rorb_disassembly_rom[rorb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        rorb_disassembly_machine.load_logical_rom(rorb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && rorb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "RORB disassembly setup failed"
    );
    Debugger rorb_disassembler;
    passed &= expect(
        rorb_disassembler.attach(rorb_disassembly_machine.execution()),
        "RORB disassembler attach failed"
    );
    const auto rorb_disassembly = rorb_disassembler.disassemble(0x8000U);
    passed &= expect(
        rorb_disassembly.has_value()
            && rorb_disassembly->supported
            && rorb_disassembly->text == "RORB"
            && rorb_disassembler.memory_access_size() == 0U,
        "RORB disassembly differs or produced trace side effects"
    );

    Jr800Machine aslb_disassembly_machine;
    std::vector<std::uint8_t> aslb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    aslb_disassembly_rom[0U] = 0x58U;
    aslb_disassembly_rom[aslb_disassembly_rom.size() - 2U] = 0x80U;
    aslb_disassembly_rom[aslb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        aslb_disassembly_machine.load_logical_rom(aslb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && aslb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "ASLB disassembly setup failed"
    );
    Debugger aslb_disassembler;
    passed &= expect(
        aslb_disassembler.attach(aslb_disassembly_machine.execution()),
        "ASLB disassembler attach failed"
    );
    const auto aslb_disassembly = aslb_disassembler.disassemble(0x8000U);
    passed &= expect(
        aslb_disassembly.has_value()
            && aslb_disassembly->supported
            && aslb_disassembly->text == "ASLB"
            && aslb_disassembler.memory_access_size() == 0U,
        "ASLB disassembly differs or produced trace side effects"
    );

    Jr800Machine asra_disassembly_machine;
    std::vector<std::uint8_t> asra_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    asra_disassembly_rom[0U] = 0x47U;
    asra_disassembly_rom[asra_disassembly_rom.size() - 2U] = 0x80U;
    asra_disassembly_rom[asra_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        asra_disassembly_machine.load_logical_rom(asra_disassembly_rom)
                == Jr800MemoryStatus::ok
            && asra_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "ASRA disassembly setup failed"
    );
    Debugger asra_disassembler;
    passed &= expect(
        asra_disassembler.attach(asra_disassembly_machine.execution()),
        "ASRA disassembler attach failed"
    );
    const auto asra_disassembly = asra_disassembler.disassemble(0x8000U);
    passed &= expect(
        asra_disassembly.has_value()
            && asra_disassembly->supported
            && asra_disassembly->text == "ASRA"
            && asra_disassembler.memory_access_size() == 0U,
        "ASRA disassembly differs or produced trace side effects"
    );

    Jr800Machine asrb_disassembly_machine;
    std::vector<std::uint8_t> asrb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    asrb_disassembly_rom[0U] = 0x57U;
    asrb_disassembly_rom[asrb_disassembly_rom.size() - 2U] = 0x80U;
    asrb_disassembly_rom[asrb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        asrb_disassembly_machine.load_logical_rom(asrb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && asrb_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "ASRB disassembly setup failed"
    );
    Debugger asrb_disassembler;
    passed &= expect(
        asrb_disassembler.attach(asrb_disassembly_machine.execution()),
        "ASRB disassembler attach failed"
    );
    const auto asrb_disassembly = asrb_disassembler.disassemble(0x8000U);
    passed &= expect(
        asrb_disassembly.has_value()
            && asrb_disassembly->supported
            && asrb_disassembly->text == "ASRB"
            && asrb_disassembler.memory_access_size() == 0U,
        "ASRB disassembly differs or produced trace side effects"
    );

    Jr800Machine subd_direct_disassembly_machine;
    std::vector<std::uint8_t> subd_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    subd_direct_disassembly_rom[0U] = 0x93U;
    subd_direct_disassembly_rom[1U] = 0x20U;
    subd_direct_disassembly_rom[subd_direct_disassembly_rom.size() - 2U]
        = 0x80U;
    subd_direct_disassembly_rom[subd_direct_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        subd_direct_disassembly_machine.load_logical_rom(
            subd_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && subd_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct SUBD disassembly setup failed"
    );
    Debugger subd_direct_disassembler;
    passed &= expect(
        subd_direct_disassembler.attach(
            subd_direct_disassembly_machine.execution()
        ),
        "Direct SUBD disassembler attach failed"
    );
    const auto subd_direct_disassembly = subd_direct_disassembler.disassemble(
        0x8000U
    );
    passed &= expect(
        subd_direct_disassembly.has_value()
            && subd_direct_disassembly->supported
            && subd_direct_disassembly->text == "SUBD $20"
            && subd_direct_disassembler.memory_access_size() == 0U,
        "Direct SUBD disassembly differs or produced trace side effects"
    );

    Jr800Machine std_disassembly_machine;
    std::vector<std::uint8_t> std_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    std_disassembly_rom[0U] = 0xDDU;
    std_disassembly_rom[1U] = 0x80U;
    std_disassembly_rom[std_disassembly_rom.size() - 2U] = 0x80U;
    std_disassembly_rom[std_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        std_disassembly_machine.load_logical_rom(std_disassembly_rom)
                == Jr800MemoryStatus::ok
            && std_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "Direct STD disassembly setup failed"
    );
    Debugger std_disassembler;
    passed &= expect(
        std_disassembler.attach(std_disassembly_machine.execution()),
        "Direct STD disassembler attach failed"
    );
    const auto std_disassembly = std_disassembler.disassemble(0x8000U);
    passed &= expect(
        std_disassembly.has_value()
            && std_disassembly->supported
            && std_disassembly->text == "STD $80"
            && std_disassembler.memory_access_size() == 0U,
        "Direct STD disassembly differs or produced trace side effects"
    );

    Jr800Machine jsr_disassembly_machine;
    std::vector<std::uint8_t> jsr_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    jsr_disassembly_rom[0U] = 0xBDU;
    jsr_disassembly_rom[1U] = 0x34U;
    jsr_disassembly_rom[2U] = 0x56U;
    jsr_disassembly_rom[3U] = 0x9DU;
    jsr_disassembly_rom[4U] = 0x20U;
    jsr_disassembly_rom[5U] = 0xADU;
    jsr_disassembly_rom[6U] = 0x21U;
    jsr_disassembly_rom[jsr_disassembly_rom.size() - 2U] = 0x80U;
    jsr_disassembly_rom[jsr_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        jsr_disassembly_machine.load_logical_rom(jsr_disassembly_rom)
                == Jr800MemoryStatus::ok
            && jsr_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "Extended JSR disassembly setup failed"
    );
    Debugger jsr_disassembler;
    passed &= expect(
        jsr_disassembler.attach(jsr_disassembly_machine.execution()),
        "Extended JSR disassembler attach failed"
    );
    const auto jsr_disassembly = jsr_disassembler.disassemble(0x8000U);
    const auto jsr_direct_disassembly = jsr_disassembler.disassemble(0x8003U);
    const auto jsr_indexed_disassembly = jsr_disassembler.disassemble(0x8005U);
    passed &= expect(
        jsr_disassembly.has_value()
            && jsr_disassembly->supported
            && jsr_disassembly->text == "JSR $3456"
            && jsr_direct_disassembly.has_value()
            && jsr_direct_disassembly->supported
            && jsr_direct_disassembly->text == "JSR $20"
            && jsr_indexed_disassembly.has_value()
            && jsr_indexed_disassembly->supported
            && jsr_indexed_disassembly->text == "JSR $21,X"
            && jsr_disassembler.memory_access_size() == 0U,
        "JSR disassembly differs or produced trace side effects"
    );

    Jr800Machine tap_disassembly_machine;
    std::vector<std::uint8_t> tap_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    tap_disassembly_rom[0U] = 0x06U;
    tap_disassembly_rom[tap_disassembly_rom.size() - 2U] = 0x80U;
    tap_disassembly_rom[tap_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        tap_disassembly_machine.load_logical_rom(tap_disassembly_rom)
                == Jr800MemoryStatus::ok
            && tap_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "TAP disassembly setup failed"
    );
    Debugger tap_disassembler;
    passed &= expect(
        tap_disassembler.attach(tap_disassembly_machine.execution()),
        "TAP disassembler attach failed"
    );
    const auto tap_disassembly = tap_disassembler.disassemble(0x8000U);
    passed &= expect(
        tap_disassembly.has_value()
            && tap_disassembly->supported
            && tap_disassembly->text == "TAP"
            && tap_disassembler.memory_access_size() == 0U,
        "TAP disassembly differs or produced trace side effects"
    );

    Jr800Machine tpa_disassembly_machine;
    std::vector<std::uint8_t> tpa_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    tpa_disassembly_rom[0U] = 0x07U;
    tpa_disassembly_rom[tpa_disassembly_rom.size() - 2U] = 0x80U;
    tpa_disassembly_rom[tpa_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        tpa_disassembly_machine.load_logical_rom(tpa_disassembly_rom)
                == Jr800MemoryStatus::ok
            && tpa_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "TPA disassembly setup failed"
    );
    Debugger tpa_disassembler;
    passed &= expect(
        tpa_disassembler.attach(tpa_disassembly_machine.execution()),
        "TPA disassembler attach failed"
    );
    const auto tpa_disassembly = tpa_disassembler.disassemble(0x8000U);
    passed &= expect(
        tpa_disassembly.has_value()
            && tpa_disassembly->supported
            && tpa_disassembly->text == "TPA"
            && tpa_disassembler.memory_access_size() == 0U,
        "TPA disassembly differs or produced trace side effects"
    );

    Jr800Machine clv_disassembly_machine;
    std::vector<std::uint8_t> clv_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    clv_disassembly_rom[0U] = 0x0AU;
    clv_disassembly_rom[clv_disassembly_rom.size() - 2U] = 0x80U;
    clv_disassembly_rom[clv_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        clv_disassembly_machine.load_logical_rom(clv_disassembly_rom)
                == Jr800MemoryStatus::ok
            && clv_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "CLV disassembly setup failed"
    );
    Debugger clv_disassembler;
    passed &= expect(
        clv_disassembler.attach(clv_disassembly_machine.execution()),
        "CLV disassembler attach failed"
    );
    const auto clv_disassembly = clv_disassembler.disassemble(0x8000U);
    passed &= expect(
        clv_disassembly.has_value()
            && clv_disassembly->supported
            && clv_disassembly->text == "CLV"
            && clv_disassembler.memory_access_size() == 0U,
        "CLV disassembly differs or produced trace side effects"
    );

    Jr800Machine sev_disassembly_machine;
    std::vector<std::uint8_t> sev_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sev_disassembly_rom[0U] = 0x0BU;
    sev_disassembly_rom[sev_disassembly_rom.size() - 2U] = 0x80U;
    sev_disassembly_rom[sev_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        sev_disassembly_machine.load_logical_rom(sev_disassembly_rom)
                == Jr800MemoryStatus::ok
            && sev_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "SEV disassembly setup failed"
    );
    Debugger sev_disassembler;
    passed &= expect(
        sev_disassembler.attach(sev_disassembly_machine.execution()),
        "SEV disassembler attach failed"
    );
    const auto sev_disassembly = sev_disassembler.disassemble(0x8000U);
    passed &= expect(
        sev_disassembly.has_value()
            && sev_disassembly->supported
            && sev_disassembly->text == "SEV"
            && sev_disassembler.memory_access_size() == 0U,
        "SEV disassembly differs or produced trace side effects"
    );

    Jr800Machine clc_disassembly_machine;
    std::vector<std::uint8_t> clc_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    clc_disassembly_rom[0U] = 0x0CU;
    clc_disassembly_rom[clc_disassembly_rom.size() - 2U] = 0x80U;
    clc_disassembly_rom[clc_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        clc_disassembly_machine.load_logical_rom(clc_disassembly_rom)
                == Jr800MemoryStatus::ok
            && clc_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "CLC disassembly setup failed"
    );
    Debugger clc_disassembler;
    passed &= expect(
        clc_disassembler.attach(clc_disassembly_machine.execution()),
        "CLC disassembler attach failed"
    );
    const auto clc_disassembly = clc_disassembler.disassemble(0x8000U);
    passed &= expect(
        clc_disassembly.has_value()
            && clc_disassembly->supported
            && clc_disassembly->text == "CLC"
            && clc_disassembler.memory_access_size() == 0U,
        "CLC disassembly differs or produced trace side effects"
    );

    Jr800Machine sec_disassembly_machine;
    std::vector<std::uint8_t> sec_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    sec_disassembly_rom[0U] = 0x0DU;
    sec_disassembly_rom[sec_disassembly_rom.size() - 2U] = 0x80U;
    sec_disassembly_rom[sec_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        sec_disassembly_machine.load_logical_rom(sec_disassembly_rom)
                == Jr800MemoryStatus::ok
            && sec_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "SEC disassembly setup failed"
    );
    Debugger sec_disassembler;
    passed &= expect(
        sec_disassembler.attach(sec_disassembly_machine.execution()),
        "SEC disassembler attach failed"
    );
    const auto sec_disassembly = sec_disassembler.disassemble(0x8000U);
    passed &= expect(
        sec_disassembly.has_value()
            && sec_disassembly->supported
            && sec_disassembly->text == "SEC"
            && sec_disassembler.memory_access_size() == 0U,
        "SEC disassembly differs or produced trace side effects"
    );

    Jr800Machine cli_disassembly_machine;
    std::vector<std::uint8_t> cli_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    cli_disassembly_rom[0U] = 0x0EU;
    cli_disassembly_rom[cli_disassembly_rom.size() - 2U] = 0x80U;
    cli_disassembly_rom[cli_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        cli_disassembly_machine.load_logical_rom(cli_disassembly_rom)
                == Jr800MemoryStatus::ok
            && cli_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "CLI disassembly setup failed"
    );
    Debugger cli_disassembler;
    passed &= expect(
        cli_disassembler.attach(cli_disassembly_machine.execution()),
        "CLI disassembler attach failed"
    );
    const auto cli_disassembly = cli_disassembler.disassemble(0x8000U);
    passed &= expect(
        cli_disassembly.has_value()
            && cli_disassembly->supported
            && cli_disassembly->text == "CLI"
            && cli_disassembler.memory_access_size() == 0U,
        "CLI disassembly differs or produced trace side effects"
    );

    Jr800Machine tsx_disassembly_machine;
    std::vector<std::uint8_t> tsx_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    tsx_disassembly_rom[0U] = 0x30U;
    tsx_disassembly_rom[tsx_disassembly_rom.size() - 2U] = 0x80U;
    tsx_disassembly_rom[tsx_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        tsx_disassembly_machine.load_logical_rom(tsx_disassembly_rom)
                == Jr800MemoryStatus::ok
            && tsx_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "TSX disassembly setup failed"
    );
    Debugger tsx_disassembler;
    passed &= expect(
        tsx_disassembler.attach(tsx_disassembly_machine.execution()),
        "TSX disassembler attach failed"
    );
    const auto tsx_disassembly = tsx_disassembler.disassemble(0x8000U);
    passed &= expect(
        tsx_disassembly.has_value()
            && tsx_disassembly->supported
            && tsx_disassembly->text == "TSX"
            && tsx_disassembler.memory_access_size() == 0U,
        "TSX disassembly differs or produced trace side effects"
    );

    Jr800Machine ins_disassembly_machine;
    std::vector<std::uint8_t> ins_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ins_disassembly_rom[0U] = 0x31U;
    ins_disassembly_rom[ins_disassembly_rom.size() - 2U] = 0x80U;
    ins_disassembly_rom[ins_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        ins_disassembly_machine.load_logical_rom(ins_disassembly_rom)
                == Jr800MemoryStatus::ok
            && ins_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "INS disassembly setup failed"
    );
    Debugger ins_disassembler;
    passed &= expect(
        ins_disassembler.attach(ins_disassembly_machine.execution()),
        "INS disassembler attach failed"
    );
    const auto ins_disassembly = ins_disassembler.disassemble(0x8000U);
    passed &= expect(
        ins_disassembly.has_value()
            && ins_disassembly->supported
            && ins_disassembly->text == "INS"
            && ins_disassembler.memory_access_size() == 0U,
        "INS disassembly differs or produced trace side effects"
    );

    Jr800Machine pula_disassembly_machine;
    std::vector<std::uint8_t> pula_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    pula_disassembly_rom[0U] = 0x32U;
    pula_disassembly_rom[pula_disassembly_rom.size() - 2U] = 0x80U;
    pula_disassembly_rom[pula_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        pula_disassembly_machine.load_logical_rom(pula_disassembly_rom)
                == Jr800MemoryStatus::ok
            && pula_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "PULA disassembly setup failed"
    );
    Debugger pula_disassembler;
    passed &= expect(
        pula_disassembler.attach(pula_disassembly_machine.execution()),
        "PULA disassembler attach failed"
    );
    const auto pula_disassembly = pula_disassembler.disassemble(0x8000U);
    passed &= expect(
        pula_disassembly.has_value()
            && pula_disassembly->supported
            && pula_disassembly->text == "PULA"
            && pula_disassembler.memory_access_size() == 0U,
        "PULA disassembly differs or produced trace side effects"
    );

    Jr800Machine pulb_disassembly_machine;
    std::vector<std::uint8_t> pulb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    pulb_disassembly_rom[0U] = 0x33U;
    pulb_disassembly_rom[pulb_disassembly_rom.size() - 2U] = 0x80U;
    pulb_disassembly_rom[pulb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        pulb_disassembly_machine.load_logical_rom(pulb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && pulb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "PULB disassembly setup failed"
    );
    Debugger pulb_disassembler;
    passed &= expect(
        pulb_disassembler.attach(pulb_disassembly_machine.execution()),
        "PULB disassembler attach failed"
    );
    const auto pulb_disassembly = pulb_disassembler.disassemble(0x8000U);
    passed &= expect(
        pulb_disassembly.has_value()
            && pulb_disassembly->supported
            && pulb_disassembly->text == "PULB"
            && pulb_disassembler.memory_access_size() == 0U,
        "PULB disassembly differs or produced trace side effects"
    );

    Jr800Machine des_disassembly_machine;
    std::vector<std::uint8_t> des_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    des_disassembly_rom[0U] = 0x34U;
    des_disassembly_rom[des_disassembly_rom.size() - 2U] = 0x80U;
    des_disassembly_rom[des_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        des_disassembly_machine.load_logical_rom(des_disassembly_rom)
                == Jr800MemoryStatus::ok
            && des_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "DES disassembly setup failed"
    );
    Debugger des_disassembler;
    passed &= expect(
        des_disassembler.attach(des_disassembly_machine.execution()),
        "DES disassembler attach failed"
    );
    const auto des_disassembly = des_disassembler.disassemble(0x8000U);
    passed &= expect(
        des_disassembly.has_value()
            && des_disassembly->supported
            && des_disassembly->text == "DES"
            && des_disassembler.memory_access_size() == 0U,
        "DES disassembly differs or produced trace side effects"
    );

    Jr800Machine txs_disassembly_machine;
    std::vector<std::uint8_t> txs_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    txs_disassembly_rom[0U] = 0x35U;
    txs_disassembly_rom[txs_disassembly_rom.size() - 2U] = 0x80U;
    txs_disassembly_rom[txs_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        txs_disassembly_machine.load_logical_rom(txs_disassembly_rom)
                == Jr800MemoryStatus::ok
            && txs_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "TXS disassembly setup failed"
    );
    Debugger txs_disassembler;
    passed &= expect(
        txs_disassembler.attach(txs_disassembly_machine.execution()),
        "TXS disassembler attach failed"
    );
    const auto txs_disassembly = txs_disassembler.disassemble(0x8000U);
    passed &= expect(
        txs_disassembly.has_value()
            && txs_disassembly->supported
            && txs_disassembly->text == "TXS"
            && txs_disassembler.memory_access_size() == 0U,
        "TXS disassembly differs or produced trace side effects"
    );

    Jr800Machine psha_disassembly_machine;
    std::vector<std::uint8_t> psha_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    psha_disassembly_rom[0U] = 0x36U;
    psha_disassembly_rom[psha_disassembly_rom.size() - 2U] = 0x80U;
    psha_disassembly_rom[psha_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        psha_disassembly_machine.load_logical_rom(psha_disassembly_rom)
                == Jr800MemoryStatus::ok
            && psha_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "PSHA disassembly setup failed"
    );
    Debugger psha_disassembler;
    passed &= expect(
        psha_disassembler.attach(psha_disassembly_machine.execution()),
        "PSHA disassembler attach failed"
    );
    const auto psha_disassembly = psha_disassembler.disassemble(0x8000U);
    passed &= expect(
        psha_disassembly.has_value()
            && psha_disassembly->supported
            && psha_disassembly->text == "PSHA"
            && psha_disassembler.memory_access_size() == 0U,
        "PSHA disassembly differs or produced trace side effects"
    );

    Jr800Machine pshb_disassembly_machine;
    std::vector<std::uint8_t> pshb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    pshb_disassembly_rom[0U] = 0x37U;
    pshb_disassembly_rom[pshb_disassembly_rom.size() - 2U] = 0x80U;
    pshb_disassembly_rom[pshb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        pshb_disassembly_machine.load_logical_rom(pshb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && pshb_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "PSHB disassembly setup failed"
    );
    Debugger pshb_disassembler;
    passed &= expect(
        pshb_disassembler.attach(pshb_disassembly_machine.execution()),
        "PSHB disassembler attach failed"
    );
    const auto pshb_disassembly = pshb_disassembler.disassemble(0x8000U);
    passed &= expect(
        pshb_disassembly.has_value()
            && pshb_disassembly->supported
            && pshb_disassembly->text == "PSHB"
            && pshb_disassembler.memory_access_size() == 0U,
        "PSHB disassembly differs or produced trace side effects"
    );

    Jr800Machine pshx_disassembly_machine;
    std::vector<std::uint8_t> pshx_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    pshx_disassembly_rom[0U] = 0x3CU;
    pshx_disassembly_rom[pshx_disassembly_rom.size() - 2U] = 0x80U;
    pshx_disassembly_rom[pshx_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        pshx_disassembly_machine.load_logical_rom(pshx_disassembly_rom)
                == Jr800MemoryStatus::ok
            && pshx_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "PSHX disassembly setup failed"
    );
    Debugger pshx_disassembler;
    passed &= expect(
        pshx_disassembler.attach(pshx_disassembly_machine.execution()),
        "PSHX disassembler attach failed"
    );
    const auto pshx_disassembly = pshx_disassembler.disassemble(0x8000U);
    passed &= expect(
        pshx_disassembly.has_value()
            && pshx_disassembly->supported
            && pshx_disassembly->text == "PSHX"
            && pshx_disassembler.memory_access_size() == 0U,
        "PSHX disassembly differs or produced trace side effects"
    );

    Jr800Machine pulx_disassembly_machine;
    std::vector<std::uint8_t> pulx_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    pulx_disassembly_rom[0U] = 0x38U;
    pulx_disassembly_rom[pulx_disassembly_rom.size() - 2U] = 0x80U;
    pulx_disassembly_rom[pulx_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        pulx_disassembly_machine.load_logical_rom(pulx_disassembly_rom)
                == Jr800MemoryStatus::ok
            && pulx_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "PULX disassembly setup failed"
    );
    Debugger pulx_disassembler;
    passed &= expect(
        pulx_disassembler.attach(pulx_disassembly_machine.execution()),
        "PULX disassembler attach failed"
    );
    const auto pulx_disassembly = pulx_disassembler.disassemble(0x8000U);
    passed &= expect(
        pulx_disassembly.has_value()
            && pulx_disassembly->supported
            && pulx_disassembly->text == "PULX"
            && pulx_disassembler.memory_access_size() == 0U,
        "PULX disassembly differs or produced trace side effects"
    );

    Jr800Machine clrb_disassembly_machine;
    std::vector<std::uint8_t> clrb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    clrb_disassembly_rom[0U] = 0x5FU;
    clrb_disassembly_rom[clrb_disassembly_rom.size() - 2U] = 0x80U;
    clrb_disassembly_rom[clrb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        clrb_disassembly_machine.load_logical_rom(clrb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && clrb_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "CLRB disassembly setup failed"
    );
    Debugger clrb_disassembler;
    passed &= expect(
        clrb_disassembler.attach(clrb_disassembly_machine.execution()),
        "CLRB disassembler attach failed"
    );
    const auto clrb_disassembly = clrb_disassembler.disassemble(0x8000U);
    passed &= expect(
        clrb_disassembly.has_value()
            && clrb_disassembly->supported
            && clrb_disassembly->text == "CLRB"
            && clrb_disassembler.memory_access_size() == 0U,
        "CLRB disassembly differs or produced trace side effects"
    );

    Jr800Machine decb_disassembly_machine;
    std::vector<std::uint8_t> decb_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    decb_disassembly_rom[0U] = 0x5AU;
    decb_disassembly_rom[decb_disassembly_rom.size() - 2U] = 0x80U;
    decb_disassembly_rom[decb_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        decb_disassembly_machine.load_logical_rom(decb_disassembly_rom)
                == Jr800MemoryStatus::ok
            && decb_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "DECB disassembly setup failed"
    );
    Debugger decb_disassembler;
    passed &= expect(
        decb_disassembler.attach(decb_disassembly_machine.execution()),
        "DECB disassembler attach failed"
    );
    const auto decb_disassembly = decb_disassembler.disassemble(0x8000U);
    passed &= expect(
        decb_disassembly.has_value()
            && decb_disassembly->supported
            && decb_disassembly->text == "DECB"
            && decb_disassembler.memory_access_size() == 0U,
        "DECB disassembly differs or produced trace side effects"
    );

    Jr800Machine lds_direct_disassembly_machine;
    std::vector<std::uint8_t> lds_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    lds_direct_disassembly_rom[0U] = 0x9EU;
    lds_direct_disassembly_rom[1U] = 0x20U;
    lds_direct_disassembly_rom[2U] = 0xAEU;
    lds_direct_disassembly_rom[3U] = 0x21U;
    lds_direct_disassembly_rom[4U] = 0xBEU;
    lds_direct_disassembly_rom[5U] = 0x34U;
    lds_direct_disassembly_rom[6U] = 0x56U;
    lds_direct_disassembly_rom[lds_direct_disassembly_rom.size() - 2U]
        = 0x80U;
    lds_direct_disassembly_rom[lds_direct_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        lds_direct_disassembly_machine.load_logical_rom(
            lds_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && lds_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct LDS disassembly setup failed"
    );
    Debugger lds_direct_disassembler;
    passed &= expect(
        lds_direct_disassembler.attach(
            lds_direct_disassembly_machine.execution()
        ),
        "Direct LDS disassembler attach failed"
    );
    const auto lds_direct_disassembly = lds_direct_disassembler.disassemble(
        0x8000U
    );
    const auto lds_indexed_disassembly = lds_direct_disassembler.disassemble(
        0x8002U
    );
    const auto lds_extended_disassembly = lds_direct_disassembler.disassemble(
        0x8004U
    );
    passed &= expect(
        lds_direct_disassembly.has_value()
            && lds_direct_disassembly->supported
            && lds_direct_disassembly->text == "LDS $20"
            && lds_indexed_disassembly.has_value()
            && lds_indexed_disassembly->supported
            && lds_indexed_disassembly->text == "LDS $21,X"
            && lds_extended_disassembly.has_value()
            && lds_extended_disassembly->supported
            && lds_extended_disassembly->text == "LDS $3456"
            && lds_direct_disassembler.memory_access_size() == 0U,
        "Addressed LDS disassembly differs or produced trace side effects"
    );

    Jr800Machine ldx_direct_disassembly_machine;
    std::vector<std::uint8_t> ldx_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ldx_direct_disassembly_rom[0U] = 0xDEU;
    ldx_direct_disassembly_rom[1U] = 0x20U;
    ldx_direct_disassembly_rom[ldx_direct_disassembly_rom.size() - 2U]
        = 0x80U;
    ldx_direct_disassembly_rom[ldx_direct_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        ldx_direct_disassembly_machine.load_logical_rom(
            ldx_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && ldx_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct LDX disassembly setup failed"
    );
    Debugger ldx_direct_disassembler;
    passed &= expect(
        ldx_direct_disassembler.attach(ldx_direct_disassembly_machine.execution()),
        "Direct LDX disassembler attach failed"
    );
    const auto ldx_direct_disassembly = ldx_direct_disassembler.disassemble(
        0x8000U
    );
    passed &= expect(
        ldx_direct_disassembly.has_value()
            && ldx_direct_disassembly->supported
            && ldx_direct_disassembly->text == "LDX $20"
            && ldx_direct_disassembler.memory_access_size() == 0U,
        "Direct LDX disassembly differs or produced trace side effects"
    );

    Jr800Machine ldx_indexed_disassembly_machine;
    std::vector<std::uint8_t> ldx_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ldx_indexed_disassembly_rom[0U] = 0xEEU;
    ldx_indexed_disassembly_rom[1U] = 0x20U;
    ldx_indexed_disassembly_rom[ldx_indexed_disassembly_rom.size() - 2U]
        = 0x80U;
    ldx_indexed_disassembly_rom[ldx_indexed_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        ldx_indexed_disassembly_machine.load_logical_rom(
            ldx_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && ldx_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed LDX disassembly setup failed"
    );
    Debugger ldx_indexed_disassembler;
    passed &= expect(
        ldx_indexed_disassembler.attach(
            ldx_indexed_disassembly_machine.execution()
        ),
        "Indexed LDX disassembler attach failed"
    );
    const auto ldx_indexed_disassembly = ldx_indexed_disassembler.disassemble(
        0x8000U
    );
    passed &= expect(
        ldx_indexed_disassembly.has_value()
            && ldx_indexed_disassembly->supported
            && ldx_indexed_disassembly->text == "LDX $20,X"
            && ldx_indexed_disassembler.memory_access_size() == 0U,
        "Indexed LDX disassembly differs or produced trace side effects"
    );

    Jr800Machine jmp_indexed_disassembly_machine;
    std::vector<std::uint8_t> jmp_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    jmp_indexed_disassembly_rom[0U] = 0x6EU;
    jmp_indexed_disassembly_rom[1U] = 0x20U;
    jmp_indexed_disassembly_rom[jmp_indexed_disassembly_rom.size() - 2U]
        = 0x80U;
    jmp_indexed_disassembly_rom[jmp_indexed_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        jmp_indexed_disassembly_machine.load_logical_rom(
            jmp_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && jmp_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed JMP disassembly setup failed"
    );
    Debugger jmp_indexed_disassembler;
    passed &= expect(
        jmp_indexed_disassembler.attach(
            jmp_indexed_disassembly_machine.execution()
        ),
        "Indexed JMP disassembler attach failed"
    );
    const auto jmp_indexed_disassembly = jmp_indexed_disassembler.disassemble(
        0x8000U
    );
    passed &= expect(
        jmp_indexed_disassembly.has_value()
            && jmp_indexed_disassembly->supported
            && jmp_indexed_disassembly->text == "JMP $20,X"
            && jmp_indexed_disassembler.memory_access_size() == 0U,
        "Indexed JMP disassembly differs or produced trace side effects"
    );

    Jr800Machine staa_indexed_disassembly_machine;
    std::vector<std::uint8_t> staa_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    staa_indexed_disassembly_rom[0U] = 0xA7U;
    staa_indexed_disassembly_rom[1U] = 0x20U;
    staa_indexed_disassembly_rom[staa_indexed_disassembly_rom.size() - 2U]
        = 0x80U;
    staa_indexed_disassembly_rom[staa_indexed_disassembly_rom.size() - 1U]
        = 0x00U;
    passed &= expect(
        staa_indexed_disassembly_machine.load_logical_rom(
            staa_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && staa_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed STAA disassembly setup failed"
    );
    Debugger staa_indexed_disassembler;
    passed &= expect(
        staa_indexed_disassembler.attach(
            staa_indexed_disassembly_machine.execution()
        ),
        "Indexed STAA disassembler attach failed"
    );
    const auto staa_indexed_disassembly = staa_indexed_disassembler.disassemble(
        0x8000U
    );
    passed &= expect(
        staa_indexed_disassembly.has_value()
            && staa_indexed_disassembly->supported
            && staa_indexed_disassembly->text == "STAA $20,X"
            && staa_indexed_disassembler.memory_access_size() == 0U,
        "Indexed STAA disassembly differs or produced trace side effects"
    );

    Jr800Machine tim_disassembly_machine;
    std::vector<std::uint8_t> tim_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    tim_disassembly_rom[0U] = 0x6BU;
    tim_disassembly_rom[1U] = 0xF0U;
    tim_disassembly_rom[2U] = 0x20U;
    tim_disassembly_rom[tim_disassembly_rom.size() - 2U] = 0x80U;
    tim_disassembly_rom[tim_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        tim_disassembly_machine.load_logical_rom(tim_disassembly_rom)
                == Jr800MemoryStatus::ok
            && tim_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "Indexed TIM disassembly setup failed"
    );
    Debugger tim_disassembler;
    passed &= expect(
        tim_disassembler.attach(tim_disassembly_machine.execution()),
        "Indexed TIM disassembler attach failed"
    );
    const auto tim_disassembly = tim_disassembler.disassemble(0x8000U);
    passed &= expect(
        tim_disassembly.has_value() && tim_disassembly->supported
            && tim_disassembly->text == "TIM #$F0, $20,X"
            && tim_disassembler.memory_access_size() == 0U,
        "Indexed TIM disassembly differs or produced trace side effects"
    );

    Jr800Machine tim_direct_disassembly_machine;
    std::vector<std::uint8_t> tim_direct_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    tim_direct_disassembly_rom[0U] = 0x7BU;
    tim_direct_disassembly_rom[1U] = 0xF0U;
    tim_direct_disassembly_rom[2U] = 0x20U;
    tim_direct_disassembly_rom[
        tim_direct_disassembly_rom.size() - 2U
    ] = 0x80U;
    tim_direct_disassembly_rom[
        tim_direct_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        tim_direct_disassembly_machine.load_logical_rom(
            tim_direct_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && tim_direct_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Direct TIM disassembly setup failed"
    );
    Debugger tim_direct_disassembler;
    passed &= expect(
        tim_direct_disassembler.attach(
            tim_direct_disassembly_machine.execution()
        ),
        "Direct TIM disassembler attach failed"
    );
    const auto tim_direct_disassembly =
        tim_direct_disassembler.disassemble(0x8000U);
    passed &= expect(
        tim_direct_disassembly.has_value()
            && tim_direct_disassembly->supported
            && tim_direct_disassembly->text == "TIM #$F0, $20"
            && tim_direct_disassembler.memory_access_size() == 0U,
        "Direct TIM disassembly differs or produced trace side effects"
    );

    Jr800Machine eim_disassembly_machine;
    std::vector<std::uint8_t> eim_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    eim_disassembly_rom[0U] = 0x75U;
    eim_disassembly_rom[1U] = 0xF0U;
    eim_disassembly_rom[2U] = 0x20U;
    eim_disassembly_rom[eim_disassembly_rom.size() - 2U] = 0x80U;
    eim_disassembly_rom[eim_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        eim_disassembly_machine.load_logical_rom(eim_disassembly_rom)
                == Jr800MemoryStatus::ok
            && eim_disassembly_machine.initialize_from_reset_entry().succeeded(),
        "Direct EIM disassembly setup failed"
    );
    Debugger eim_disassembler;
    passed &= expect(
        eim_disassembler.attach(eim_disassembly_machine.execution()),
        "Direct EIM disassembler attach failed"
    );
    const auto eim_disassembly = eim_disassembler.disassemble(0x8000U);
    passed &= expect(
        eim_disassembly.has_value() && eim_disassembly->supported
            && eim_disassembly->text == "EIM #$F0, $20"
            && eim_disassembler.memory_access_size() == 0U,
        "Direct EIM disassembly differs or produced trace side effects"
    );

    Jr800Machine indexed_logic_disassembly_machine;
    std::vector<std::uint8_t> indexed_logic_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    indexed_logic_disassembly_rom[0U] = 0x61U;
    indexed_logic_disassembly_rom[1U] = 0x0FU;
    indexed_logic_disassembly_rom[2U] = 0x20U;
    indexed_logic_disassembly_rom[3U] = 0x65U;
    indexed_logic_disassembly_rom[4U] = 0xF0U;
    indexed_logic_disassembly_rom[5U] = 0x21U;
    indexed_logic_disassembly_rom[
        indexed_logic_disassembly_rom.size() - 2U
    ] = 0x80U;
    indexed_logic_disassembly_rom[
        indexed_logic_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        indexed_logic_disassembly_machine.load_logical_rom(
            indexed_logic_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && indexed_logic_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed AIM/EIM disassembly setup failed"
    );
    Debugger indexed_logic_disassembler;
    passed &= expect(
        indexed_logic_disassembler.attach(
            indexed_logic_disassembly_machine.execution()
        ),
        "Indexed AIM/EIM disassembler attach failed"
    );
    const auto aim_indexed_disassembly =
        indexed_logic_disassembler.disassemble(0x8000U);
    const auto eim_indexed_disassembly =
        indexed_logic_disassembler.disassemble(0x8003U);
    passed &= expect(
        aim_indexed_disassembly.has_value()
            && aim_indexed_disassembly->supported
            && aim_indexed_disassembly->text == "AIM #$0F, $20,X"
            && eim_indexed_disassembly.has_value()
            && eim_indexed_disassembly->supported
            && eim_indexed_disassembly->text == "EIM #$F0, $21,X"
            && indexed_logic_disassembler.memory_access_size() == 0U,
        "Indexed AIM/EIM disassembly differs or produced trace side effects"
    );

    Jr800Machine sleep_timer_machine;
    std::vector<std::uint8_t> sleep_timer_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    constexpr std::uint8_t sleep_timer_program[]{
        0x86U, 0x00U,
        0x97U, 0x0BU,
        0x86U, 0x20U,
        0x97U, 0x0CU,
        0x86U, 0x08U,
        0x97U, 0x08U,
        0x1AU,
    };
    for (std::size_t index = 0U; index < std::size(sleep_timer_program); ++index) {
        sleep_timer_rom[index] = sleep_timer_program[index];
    }
    sleep_timer_rom[0x0100U] = 0x01U;
    sleep_timer_rom[0x7FF4U] = 0x81U;
    sleep_timer_rom[0x7FF5U] = 0x00U;
    sleep_timer_rom[sleep_timer_rom.size() - 2U] = 0x80U;
    sleep_timer_rom[sleep_timer_rom.size() - 1U] = 0x00U;
    passed &= expect(
        sleep_timer_machine.load_logical_rom(sleep_timer_rom)
                == Jr800MemoryStatus::ok
            && sleep_timer_machine.initialize_from_reset_entry().succeeded(),
        "Sleep-timer machine setup failed"
    );
    sleep_timer_machine.execution().initialize(
        CpuProfile::hd6301v1,
        0x8000U,
        0x5FFFU
    );
    Debugger sleep_timer_debugger;
    passed &= expect(
        sleep_timer_debugger.attach(sleep_timer_machine.execution()),
        "Sleep-timer debugger attach failed"
    );
    const auto sleep_timer_stop = sleep_timer_debugger.run(16U);
    const auto sleep_timer_history_size = sleep_timer_debugger.history_size();
    const auto sleep_timer_access_size = sleep_timer_debugger.memory_access_size();
    passed &= expect(
        sleep_timer_stop.reason == StopReason::sleeping
            && sleep_timer_stop.instructions_executed == 7U
            && sleep_timer_machine.execution().cpu().state().cycle_count == 19U,
        "Timer program did not enter sleep at the expected E-cycle boundary"
    );
    const auto before_compare =
        sleep_timer_machine.execution().advance_suspended_cycles(12U);
    const auto compare = sleep_timer_machine.execution().advance_suspended_cycles(10U);
    const auto counter_high = sleep_timer_machine.execution().inspect8(0x0009U);
    const auto counter_low = sleep_timer_machine.execution().inspect8(0x000AU);
    passed &= expect(
        before_compare.suspended && before_compare.cycles_elapsed == 12U
            && before_compare.interrupt_request.known
            && !before_compare.interrupt_request.asserted()
            && compare.suspended && compare.cycles_elapsed == 1U
            && compare.interrupt_request.asserted()
            && compare.interrupt_request.source
                == jr800::core::InterruptSource::timer_output_compare
            && counter_high.value == 0x00U && counter_low.value == 0x20U
            && sleep_timer_machine.execution().cpu().state().cycle_count == 32U
            && sleep_timer_debugger.history_size() == sleep_timer_history_size
            && sleep_timer_debugger.memory_access_size()
                == sleep_timer_access_size,
        "Sleep advancement missed or crossed the first timer request"
    );
    const auto sleep_timer_interrupt = sleep_timer_debugger.step();
    passed &= expect(
        sleep_timer_interrupt.reason == StopReason::step_complete
            && sleep_timer_interrupt.step.kind
                == jr800::core::StepKind::interrupt_entry
            && sleep_timer_interrupt.step.interrupt_source
                == jr800::core::InterruptSource::timer_output_compare
            && sleep_timer_machine.execution().cpu().state().pc == 0x8100U
            && sleep_timer_machine.execution().cpu().state().cycle_count == 44U,
        "Timer request did not remain available for the next CPU step"
    );

    Jr800Machine sleep_unknown_machine;
    std::vector<std::uint8_t> sleep_unknown_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    constexpr std::uint8_t sleep_unknown_program[]{
        0x86U, 0x18U,
        0x97U, 0x11U,
        0x1AU,
    };
    for (std::size_t index = 0U; index < std::size(sleep_unknown_program); ++index) {
        sleep_unknown_rom[index] = sleep_unknown_program[index];
    }
    sleep_unknown_rom[sleep_unknown_rom.size() - 2U] = 0x80U;
    sleep_unknown_rom[sleep_unknown_rom.size() - 1U] = 0x00U;
    passed &= expect(
        sleep_unknown_machine.load_logical_rom(sleep_unknown_rom)
                == Jr800MemoryStatus::ok
            && sleep_unknown_machine.initialize_from_reset_entry().succeeded(),
        "Unknown receive sleep setup failed"
    );
    sleep_unknown_machine.execution().initialize(
        CpuProfile::hd6301v1,
        0x8000U,
        0x5FFFU
    );
    Debugger sleep_unknown_debugger;
    passed &= expect(
        sleep_unknown_debugger.attach(sleep_unknown_machine.execution())
            && sleep_unknown_debugger.run(8U).reason == StopReason::sleeping,
        "Unknown receive program did not enter sleep"
    );
    const auto before_unknown =
        sleep_unknown_machine.execution().advance_suspended_cycles(152U);
    const auto became_unknown =
        sleep_unknown_machine.execution().advance_suspended_cycles(5U);
    passed &= expect(
        before_unknown.cycles_elapsed == 152U
            && before_unknown.interrupt_request.known
            && !before_unknown.interrupt_request.asserted()
            && became_unknown.cycles_elapsed == 1U
            && !became_unknown.interrupt_request.known
            && sleep_unknown_machine.execution().cpu().state().execution_state
                == jr800::core::CpuExecutionState::sleeping
            && sleep_unknown_machine.execution().cpu().state().cycle_count
                == 162U,
        "Sleep advancement crossed an unresolved receive-request boundary"
    );

    Jr800Machine tst_indexed_disassembly_machine;
    std::vector<std::uint8_t> tst_indexed_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    tst_indexed_disassembly_rom[0U] = 0x6DU;
    tst_indexed_disassembly_rom[1U] = 0x23U;
    tst_indexed_disassembly_rom[
        tst_indexed_disassembly_rom.size() - 2U
    ] = 0x80U;
    tst_indexed_disassembly_rom[
        tst_indexed_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        tst_indexed_disassembly_machine.load_logical_rom(
            tst_indexed_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && tst_indexed_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Indexed TST disassembly setup failed"
    );
    Debugger tst_indexed_disassembler;
    passed &= expect(
        tst_indexed_disassembler.attach(
            tst_indexed_disassembly_machine.execution()
        ),
        "Indexed TST disassembler attach failed"
    );
    const auto tst_indexed_disassembly =
        tst_indexed_disassembler.disassemble(0x8000U);
    passed &= expect(
        tst_indexed_disassembly.has_value()
            && tst_indexed_disassembly->supported
            && tst_indexed_disassembly->text == "TST $23,X"
            && tst_indexed_disassembler.memory_access_size() == 0U,
        "Indexed TST disassembly differs or produced trace side effects"
    );

    Jr800Machine tst_extended_disassembly_machine;
    std::vector<std::uint8_t> tst_extended_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    tst_extended_disassembly_rom[0U] = 0x7DU;
    tst_extended_disassembly_rom[1U] = 0x81U;
    tst_extended_disassembly_rom[2U] = 0x23U;
    tst_extended_disassembly_rom[
        tst_extended_disassembly_rom.size() - 2U
    ] = 0x80U;
    tst_extended_disassembly_rom[
        tst_extended_disassembly_rom.size() - 1U
    ] = 0x00U;
    passed &= expect(
        tst_extended_disassembly_machine.load_logical_rom(
            tst_extended_disassembly_rom
        ) == Jr800MemoryStatus::ok
            && tst_extended_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Extended TST disassembly setup failed"
    );
    Debugger tst_extended_disassembler;
    passed &= expect(
        tst_extended_disassembler.attach(
            tst_extended_disassembly_machine.execution()
        ),
        "Extended TST disassembler attach failed"
    );
    const auto tst_extended_disassembly =
        tst_extended_disassembler.disassemble(0x8000U);
    passed &= expect(
        tst_extended_disassembly.has_value()
            && tst_extended_disassembly->supported
            && tst_extended_disassembly->text == "TST $8123"
            && tst_extended_disassembler.memory_access_size() == 0U,
        "Extended TST disassembly differs or produced trace side effects"
    );

    Jr800Machine neg_disassembly_machine;
    std::vector<std::uint8_t> neg_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    neg_disassembly_rom[0U] = 0x60U;
    neg_disassembly_rom[1U] = 0x23U;
    neg_disassembly_rom[2U] = 0x70U;
    neg_disassembly_rom[3U] = 0x81U;
    neg_disassembly_rom[4U] = 0x23U;
    neg_disassembly_rom[neg_disassembly_rom.size() - 2U] = 0x80U;
    neg_disassembly_rom[neg_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        neg_disassembly_machine.load_logical_rom(neg_disassembly_rom)
                == Jr800MemoryStatus::ok
            && neg_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Memory NEG disassembly setup failed"
    );
    Debugger neg_disassembler;
    passed &= expect(
        neg_disassembler.attach(neg_disassembly_machine.execution()),
        "Memory NEG disassembler attach failed"
    );
    const auto neg_indexed_disassembly = neg_disassembler.disassemble(0x8000U);
    const auto neg_extended_disassembly = neg_disassembler.disassemble(0x8002U);
    passed &= expect(
        neg_indexed_disassembly.has_value()
            && neg_indexed_disassembly->supported
            && neg_indexed_disassembly->text == "NEG $23,X"
            && neg_extended_disassembly.has_value()
            && neg_extended_disassembly->supported
            && neg_extended_disassembly->text == "NEG $8123"
            && neg_disassembler.memory_access_size() == 0U,
        "Memory NEG disassembly differs or produced trace side effects"
    );

    Jr800Machine com_disassembly_machine;
    std::vector<std::uint8_t> com_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    com_disassembly_rom[0U] = 0x63U;
    com_disassembly_rom[1U] = 0x23U;
    com_disassembly_rom[2U] = 0x73U;
    com_disassembly_rom[3U] = 0x81U;
    com_disassembly_rom[4U] = 0x23U;
    com_disassembly_rom[com_disassembly_rom.size() - 2U] = 0x80U;
    com_disassembly_rom[com_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        com_disassembly_machine.load_logical_rom(com_disassembly_rom)
                == Jr800MemoryStatus::ok
            && com_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Memory COM disassembly setup failed"
    );
    Debugger com_disassembler;
    passed &= expect(
        com_disassembler.attach(com_disassembly_machine.execution()),
        "Memory COM disassembler attach failed"
    );
    const auto com_indexed_disassembly = com_disassembler.disassemble(0x8000U);
    const auto com_extended_disassembly = com_disassembler.disassemble(0x8002U);
    passed &= expect(
        com_indexed_disassembly.has_value()
            && com_indexed_disassembly->supported
            && com_indexed_disassembly->text == "COM $23,X"
            && com_extended_disassembly.has_value()
            && com_extended_disassembly->supported
            && com_extended_disassembly->text == "COM $8123"
            && com_disassembler.memory_access_size() == 0U,
        "Memory COM disassembly differs or produced trace side effects"
    );

    Jr800Machine lsr_disassembly_machine;
    std::vector<std::uint8_t> lsr_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    lsr_disassembly_rom[0U] = 0x64U;
    lsr_disassembly_rom[1U] = 0x23U;
    lsr_disassembly_rom[2U] = 0x74U;
    lsr_disassembly_rom[3U] = 0x81U;
    lsr_disassembly_rom[4U] = 0x23U;
    lsr_disassembly_rom[lsr_disassembly_rom.size() - 2U] = 0x80U;
    lsr_disassembly_rom[lsr_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        lsr_disassembly_machine.load_logical_rom(lsr_disassembly_rom)
                == Jr800MemoryStatus::ok
            && lsr_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Memory LSR disassembly setup failed"
    );
    Debugger lsr_disassembler;
    passed &= expect(
        lsr_disassembler.attach(lsr_disassembly_machine.execution()),
        "Memory LSR disassembler attach failed"
    );
    const auto lsr_indexed_disassembly = lsr_disassembler.disassemble(0x8000U);
    const auto lsr_extended_disassembly = lsr_disassembler.disassemble(0x8002U);
    passed &= expect(
        lsr_indexed_disassembly.has_value()
            && lsr_indexed_disassembly->supported
            && lsr_indexed_disassembly->text == "LSR $23,X"
            && lsr_extended_disassembly.has_value()
            && lsr_extended_disassembly->supported
            && lsr_extended_disassembly->text == "LSR $8123"
            && lsr_disassembler.memory_access_size() == 0U,
        "Memory LSR disassembly differs or produced trace side effects"
    );

    Jr800Machine ror_disassembly_machine;
    std::vector<std::uint8_t> ror_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    ror_disassembly_rom[0U] = 0x66U;
    ror_disassembly_rom[1U] = 0x23U;
    ror_disassembly_rom[2U] = 0x76U;
    ror_disassembly_rom[3U] = 0x81U;
    ror_disassembly_rom[4U] = 0x23U;
    ror_disassembly_rom[ror_disassembly_rom.size() - 2U] = 0x80U;
    ror_disassembly_rom[ror_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        ror_disassembly_machine.load_logical_rom(ror_disassembly_rom)
                == Jr800MemoryStatus::ok
            && ror_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Memory ROR disassembly setup failed"
    );
    Debugger ror_disassembler;
    passed &= expect(
        ror_disassembler.attach(ror_disassembly_machine.execution()),
        "Memory ROR disassembler attach failed"
    );
    const auto ror_indexed_disassembly = ror_disassembler.disassemble(0x8000U);
    const auto ror_extended_disassembly = ror_disassembler.disassemble(0x8002U);
    passed &= expect(
        ror_indexed_disassembly.has_value()
            && ror_indexed_disassembly->supported
            && ror_indexed_disassembly->text == "ROR $23,X"
            && ror_extended_disassembly.has_value()
            && ror_extended_disassembly->supported
            && ror_extended_disassembly->text == "ROR $8123"
            && ror_disassembler.memory_access_size() == 0U,
        "Memory ROR disassembly differs or produced trace side effects"
    );

    Jr800Machine asr_disassembly_machine;
    std::vector<std::uint8_t> asr_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    asr_disassembly_rom[0U] = 0x67U;
    asr_disassembly_rom[1U] = 0x23U;
    asr_disassembly_rom[2U] = 0x77U;
    asr_disassembly_rom[3U] = 0x81U;
    asr_disassembly_rom[4U] = 0x23U;
    asr_disassembly_rom[asr_disassembly_rom.size() - 2U] = 0x80U;
    asr_disassembly_rom[asr_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        asr_disassembly_machine.load_logical_rom(asr_disassembly_rom)
                == Jr800MemoryStatus::ok
            && asr_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Memory ASR disassembly setup failed"
    );
    Debugger asr_disassembler;
    passed &= expect(
        asr_disassembler.attach(asr_disassembly_machine.execution()),
        "Memory ASR disassembler attach failed"
    );
    const auto asr_indexed_disassembly = asr_disassembler.disassemble(0x8000U);
    const auto asr_extended_disassembly = asr_disassembler.disassemble(0x8002U);
    passed &= expect(
        asr_indexed_disassembly.has_value()
            && asr_indexed_disassembly->supported
            && asr_indexed_disassembly->text == "ASR $23,X"
            && asr_extended_disassembly.has_value()
            && asr_extended_disassembly->supported
            && asr_extended_disassembly->text == "ASR $8123"
            && asr_disassembler.memory_access_size() == 0U,
        "Memory ASR disassembly differs or produced trace side effects"
    );

    Jr800Machine asl_disassembly_machine;
    std::vector<std::uint8_t> asl_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    asl_disassembly_rom[0U] = 0x68U;
    asl_disassembly_rom[1U] = 0x23U;
    asl_disassembly_rom[2U] = 0x78U;
    asl_disassembly_rom[3U] = 0x81U;
    asl_disassembly_rom[4U] = 0x23U;
    asl_disassembly_rom[asl_disassembly_rom.size() - 2U] = 0x80U;
    asl_disassembly_rom[asl_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        asl_disassembly_machine.load_logical_rom(asl_disassembly_rom)
                == Jr800MemoryStatus::ok
            && asl_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Memory ASL disassembly setup failed"
    );
    Debugger asl_disassembler;
    passed &= expect(
        asl_disassembler.attach(asl_disassembly_machine.execution()),
        "Memory ASL disassembler attach failed"
    );
    const auto asl_indexed_disassembly = asl_disassembler.disassemble(0x8000U);
    const auto asl_extended_disassembly = asl_disassembler.disassemble(0x8002U);
    passed &= expect(
        asl_indexed_disassembly.has_value()
            && asl_indexed_disassembly->supported
            && asl_indexed_disassembly->text == "ASL $23,X"
            && asl_extended_disassembly.has_value()
            && asl_extended_disassembly->supported
            && asl_extended_disassembly->text == "ASL $8123"
            && asl_disassembler.memory_access_size() == 0U,
        "Memory ASL disassembly differs or produced trace side effects"
    );

    Jr800Machine rol_disassembly_machine;
    std::vector<std::uint8_t> rol_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    rol_disassembly_rom[0U] = 0x69U;
    rol_disassembly_rom[1U] = 0x23U;
    rol_disassembly_rom[2U] = 0x79U;
    rol_disassembly_rom[3U] = 0x81U;
    rol_disassembly_rom[4U] = 0x23U;
    rol_disassembly_rom[rol_disassembly_rom.size() - 2U] = 0x80U;
    rol_disassembly_rom[rol_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        rol_disassembly_machine.load_logical_rom(rol_disassembly_rom)
                == Jr800MemoryStatus::ok
            && rol_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Memory ROL disassembly setup failed"
    );
    Debugger rol_disassembler;
    passed &= expect(
        rol_disassembler.attach(rol_disassembly_machine.execution()),
        "Memory ROL disassembler attach failed"
    );
    const auto rol_indexed_disassembly = rol_disassembler.disassemble(0x8000U);
    const auto rol_extended_disassembly = rol_disassembler.disassemble(0x8002U);
    passed &= expect(
        rol_indexed_disassembly.has_value()
            && rol_indexed_disassembly->supported
            && rol_indexed_disassembly->text == "ROL $23,X"
            && rol_extended_disassembly.has_value()
            && rol_extended_disassembly->supported
            && rol_extended_disassembly->text == "ROL $8123"
            && rol_disassembler.memory_access_size() == 0U,
        "Memory ROL disassembly differs or produced trace side effects"
    );

    Jr800Machine dec_disassembly_machine;
    std::vector<std::uint8_t> dec_disassembly_rom(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
    dec_disassembly_rom[0U] = 0x6AU;
    dec_disassembly_rom[1U] = 0x23U;
    dec_disassembly_rom[2U] = 0x7AU;
    dec_disassembly_rom[3U] = 0x81U;
    dec_disassembly_rom[4U] = 0x23U;
    dec_disassembly_rom[dec_disassembly_rom.size() - 2U] = 0x80U;
    dec_disassembly_rom[dec_disassembly_rom.size() - 1U] = 0x00U;
    passed &= expect(
        dec_disassembly_machine.load_logical_rom(dec_disassembly_rom)
                == Jr800MemoryStatus::ok
            && dec_disassembly_machine.initialize_from_reset_entry()
                .succeeded(),
        "Memory DEC disassembly setup failed"
    );
    Debugger dec_disassembler;
    passed &= expect(
        dec_disassembler.attach(dec_disassembly_machine.execution()),
        "Memory DEC disassembler attach failed"
    );
    const auto dec_indexed_disassembly = dec_disassembler.disassemble(0x8000U);
    const auto dec_extended_disassembly = dec_disassembler.disassemble(0x8002U);
    passed &= expect(
        dec_indexed_disassembly.has_value()
            && dec_indexed_disassembly->supported
            && dec_indexed_disassembly->text == "DEC $23,X"
            && dec_extended_disassembly.has_value()
            && dec_extended_disassembly->supported
            && dec_extended_disassembly->text == "DEC $8123"
            && dec_disassembler.memory_access_size() == 0U,
        "Memory DEC disassembly differs or produced trace side effects"
    );

    return passed ? 0 : 1;
}
