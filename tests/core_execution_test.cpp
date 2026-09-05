// SPDX-License-Identifier: MIT

#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <vector>

#include "jr800/core/cpu.hpp"
#include "jr800/core/machine.hpp"
#include "jr800/core/synthetic_machine.hpp"
#include "jr800/debugger/debugger.hpp"
#include "jr800/isa/instruction_metadata.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

class ShortLivedObserver final : public jr800::core::MachineObserver {
private:
    void on_machine_detached(jr800::core::Machine&) noexcept override {}
    void on_step_begin(const jr800::core::CpuState&) noexcept override {}
    void on_step_end(
        const jr800::core::StepResult&,
        const jr800::core::CpuState&
    ) noexcept override {}
    void on_bus_access(const jr800::core::BusAccessEvent&) noexcept override {}
};

class AdvanceFaultBus final : public jr800::core::Bus {
public:
    void set_advance_fault(jr800::core::BusFault fault) noexcept {
        advance_fault_ = fault;
    }

    void poke8(std::uint16_t address, std::uint8_t value) noexcept {
        memory_.poke8(address, value);
    }

    [[nodiscard]] std::uint64_t requested_cycles() const noexcept {
        return requested_cycles_;
    }

    [[nodiscard]] jr800::core::BusFault advance_cycles(
        std::uint32_t cycles
    ) noexcept override {
        requested_cycles_ += cycles;
        return advance_fault_;
    }

    [[nodiscard]] jr800::core::InterruptRequest maskable_interrupt_request()
        const noexcept override {
        return memory_.maskable_interrupt_request();
    }

    [[nodiscard]] jr800::core::BusReadResult read8(
        std::uint16_t address,
        jr800::core::AccessKind kind
    ) noexcept override {
        return memory_.read8(address, kind);
    }

    [[nodiscard]] jr800::core::BusDiscardedReadResult read8_discard(
        std::uint16_t address
    ) noexcept override {
        return memory_.read8_discard(address);
    }

    [[nodiscard]] jr800::core::BusReadResult inspect8(
        std::uint16_t address
    ) const noexcept override {
        return memory_.inspect8(address);
    }

    [[nodiscard]] jr800::core::BusWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept override {
        return memory_.write8(address, value);
    }

private:
    jr800::core::RamBus memory_;
    jr800::core::BusFault advance_fault_{jr800::core::BusFault::none};
    std::uint64_t requested_cycles_{};
};

}  // namespace

int main() {
    using jr800::core::AccessKind;
    using jr800::core::ConditionCode;
    using jr800::core::CpuExecutionState;
    using jr800::core::CpuFault;
    using jr800::core::InterruptRequest;
    using jr800::core::InterruptSource;
    using jr800::core::StepKind;
    using jr800::debugger::StopReason;
    using jr800::isa::CpuProfile;

    bool passed = true;
    static_assert(!std::is_copy_constructible_v<jr800::core::Machine>);
    static_assert(!std::is_copy_assignable_v<jr800::core::Machine>);
    static_assert(!std::is_move_constructible_v<jr800::core::Machine>);
    static_assert(!std::is_move_assignable_v<jr800::core::Machine>);
    static_assert(!std::is_copy_constructible_v<jr800::core::SyntheticMachine>);
    static_assert(!std::is_move_constructible_v<jr800::core::SyntheticMachine>);
    static_assert(!std::is_copy_constructible_v<jr800::core::RamBus>);
    static_assert(!std::is_copy_assignable_v<jr800::core::RamBus>);
    static_assert(!std::is_move_constructible_v<jr800::core::RamBus>);
    static_assert(!std::is_move_assignable_v<jr800::core::RamBus>);

    {
        AdvanceFaultBus bus;
        bus.poke8(0x4000U, 0x01U);
        bus.poke8(0x4001U, 0x01U);
        jr800::core::Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x4000U, 0x01FFU);

        bus.set_advance_fault(
            jr800::core::BusFault::device_state_unknown
        );
        const auto fault = cpu.step_instruction(bus);
        passed &= expect(
            fault.kind == StepKind::instruction
                && fault.step_completed
                && !fault.succeeded()
                && fault.fault == CpuFault::bus_advance
                && fault.bus_fault
                    == jr800::core::BusFault::device_state_unknown
                && fault.pc_before == 0x4000U
                && fault.pc_after == 0x4001U
                && fault.cycles == 1U
                && cpu.state().pc == 0x4001U
                && cpu.state().cycle_count == 1U
                && bus.requested_cycles() == 1U,
            "Completed instruction hid or rolled back its bus-advance fault"
        );

        bus.set_advance_fault(jr800::core::BusFault::none);
        const auto recovered = cpu.step_instruction(bus);
        passed &= expect(
            recovered.succeeded()
                && recovered.pc_before == 0x4001U
                && recovered.pc_after == 0x4002U
                && cpu.state().cycle_count == 2U
                && bus.requested_cycles() == 2U,
            "Bus-advance fault caused the completed instruction to repeat"
        );
    }

    {
        jr800::debugger::Debugger outliving_debugger;
        {
            jr800::core::SyntheticMachine short_lived_host;
            auto& short_lived_machine = short_lived_host.execution();
            passed &= expect(
                outliving_debugger.attach(short_lived_machine),
                "Debugger did not attach to short-lived machine"
            );
        }
        passed &= expect(
            outliving_debugger.machine() == nullptr
                && outliving_debugger.step().reason == StopReason::detached,
            "Destroyed machine did not detach its debugger"
        );
    }

    {
        jr800::core::SyntheticMachine externally_detached_host;
        auto& externally_detached_machine = externally_detached_host.execution();
        jr800::debugger::Debugger externally_detached_debugger;
        passed &= expect(
            externally_detached_debugger.attach(externally_detached_machine),
            "Debugger did not attach for observer replacement test"
        );
        externally_detached_machine.clear_observers();
        passed &= expect(
            externally_detached_debugger.machine() == nullptr,
            "Observer replacement left a stale machine pointer"
        );
    }

    {
        jr800::core::SyntheticMachine machine_host;
        auto& machine_outliving_observer = machine_host.execution();
        {
            ShortLivedObserver short_lived_observer;
            passed &= expect(
                machine_outliving_observer.add_observer(&short_lived_observer)
                    && machine_outliving_observer.has_observer(&short_lived_observer),
                "Short-lived observer did not attach"
            );
        }
        passed &= expect(
            !machine_outliving_observer.has_observers(),
            "Destroyed observer remained attached to machine"
        );
    }

    {
        jr800::core::SyntheticMachine first_host;
        jr800::core::SyntheticMachine second_host;
        auto& first_machine = first_host.execution();
        auto& second_machine = second_host.execution();
        ShortLivedObserver observer;
        passed &= expect(
            first_machine.add_observer(&observer),
            "Observer did not attach to first machine"
        );
        passed &= expect(
            !second_machine.add_observer(&observer)
                && first_machine.has_observer(&observer)
                && !second_machine.has_observers(),
            "Observer attached to two machines"
        );
    }

    {
        jr800::core::RamBus bus;
        {
            ShortLivedObserver short_lived_observer;
            passed &= expect(
                bus.set_observer(&short_lived_observer),
                "Short-lived bus observer did not attach"
            );
            static_cast<void>(bus.write8(0x0000U, 0x12U));
        }
        static_cast<void>(bus.write8(0x0000U, 0x34U));
        passed &= expect(
            bus.peek8(0x0000U) == 0x34U,
            "Destroyed observer remained attached to bus"
        );
    }

    {
        jr800::core::RamBus first_bus;
        jr800::core::RamBus second_bus;
        ShortLivedObserver observer;
        passed &= expect(
            first_bus.set_observer(&observer),
            "Observer did not attach to first bus"
        );
        passed &= expect(
            !second_bus.set_observer(&observer),
            "Observer attached to two buses"
        );
    }

    {
        jr800::core::SyntheticMachine sleep_host;
        auto& sleep_machine = sleep_host.execution();
        auto& sleep_memory = sleep_host.bus();
        constexpr std::uint8_t sleep_program[]{0x1AU, 0x01U};
        passed &= expect(
            sleep_memory.load(0x5000U, sleep_program),
            "Sleep program did not fit synthetic RAM"
        );
        sleep_machine.initialize(CpuProfile::hd6301v1, 0x5000U, 0x01FFU);
        jr800::debugger::Debugger sleep_debugger;
        passed &= expect(
            sleep_debugger.attach(sleep_machine),
            "Sleep debugger did not attach"
        );

        const auto entered = sleep_debugger.run(10U);
        passed &= expect(
            entered.reason == StopReason::sleeping
                && entered.instructions_executed == 1U
                && entered.trigger_address == 0x5001U
                && entered.step.succeeded()
                && entered.step.bytes[0] == 0x1AU
                && entered.step.cycles == 4U
                && sleep_machine.cpu().state().pc == 0x5001U
                && sleep_machine.cpu().state().cycle_count == 4U
                && sleep_machine.cpu().state().execution_state
                    == CpuExecutionState::sleeping,
            "Debugger did not report SLP entry"
        );
        passed &= expect(
            sleep_debugger.history_size() == 1U
                && sleep_debugger.memory_access_size() == 1U,
            "SLP entry history or access count differs"
        );

        const auto entered_sleep_state = sleep_machine.cpu().state();
        const auto zero_limit = sleep_machine.advance_suspended_cycles(0U);
        passed &= expect(
            zero_limit.suspended && zero_limit.cycles_elapsed == 0U
                && zero_limit.bus_fault == jr800::core::BusFault::none
                && zero_limit.interrupt_request.known
                && !zero_limit.interrupt_request.asserted()
                && sleep_machine.cpu().state() == entered_sleep_state,
            "Zero sleep-cycle limit changed machine state"
        );

        auto expected_elapsed_state = entered_sleep_state;
        expected_elapsed_state.cycle_count += 17U;
        const auto elapsed = sleep_machine.advance_suspended_cycles(17U);
        passed &= expect(
            elapsed.suspended && elapsed.cycles_elapsed == 17U
                && elapsed.bus_fault == jr800::core::BusFault::none
                && elapsed.interrupt_request.known
                && !elapsed.interrupt_request.asserted()
                && sleep_machine.cpu().state() == expected_elapsed_state
                && sleep_debugger.history_size() == 1U
                && sleep_debugger.memory_access_size() == 1U,
            "Bounded sleep-cycle advance changed CPU work or debugger history"
        );

        sleep_memory.set_maskable_interrupt_request(InterruptRequest{
            InterruptSource::timer_output_compare,
            true,
        });
        const auto pending = sleep_machine.advance_suspended_cycles(100U);
        passed &= expect(
            pending.suspended && pending.cycles_elapsed == 0U
                && pending.bus_fault == jr800::core::BusFault::none
                && pending.interrupt_request.asserted()
                && pending.interrupt_request.source
                    == InterruptSource::timer_output_compare
                && sleep_machine.cpu().state() == expected_elapsed_state,
            "Pending sleep interrupt was advanced past its boundary"
        );

        sleep_memory.set_maskable_interrupt_request(InterruptRequest{
            InterruptSource::none,
            false,
        });
        const auto unknown = sleep_machine.advance_suspended_cycles(100U);
        passed &= expect(
            unknown.suspended && unknown.cycles_elapsed == 0U
                && unknown.bus_fault == jr800::core::BusFault::none
                && !unknown.interrupt_request.known
                && sleep_machine.cpu().state() == expected_elapsed_state,
            "Unknown sleep interrupt request was advanced past its boundary"
        );

        sleep_memory.set_maskable_interrupt_request({});
        sleep_debugger.set_execution_breakpoint(0x5001U, true);
        const auto dormant = sleep_debugger.run(10U);
        passed &= expect(
            dormant.reason == StopReason::sleeping
                && dormant.instructions_executed == 0U
                && dormant.trigger_address == 0x5001U
                && dormant.step.fault == CpuFault::none
                && !dormant.step.step_completed
                && dormant.step.bytes_fetched == 0U
                && dormant.step.cycles == 0U
                && sleep_debugger.history_size() == 1U
                && sleep_debugger.memory_access_size() == 1U,
            "Sleeping CPU executed, faulted, or yielded to a breakpoint"
        );

        sleep_machine.initialize(CpuProfile::hd6301v1, 0x5001U, 0x01FFU);
        const auto active_state = sleep_machine.cpu().state();
        const auto active_advance = sleep_machine.advance_suspended_cycles(100U);
        passed &= expect(
            sleep_machine.cpu().state().execution_state
                    == CpuExecutionState::active
                && !active_advance.suspended
                && active_advance.cycles_elapsed == 0U
                && active_advance.bus_fault == jr800::core::BusFault::none
                && active_advance.interrupt_request.known
                && !active_advance.interrupt_request.asserted()
                && sleep_machine.cpu().state() == active_state,
            "Active machine accepted sleep-cycle advancement"
        );
    }

    {
        jr800::core::SyntheticMachine wait_host;
        auto& wait_machine = wait_host.execution();
        auto& wait_memory = wait_host.bus();
        constexpr std::uint8_t wait_program[]{0x3EU, 0x01U};
        passed &= expect(
            wait_memory.load(0x5800U, wait_program),
            "Wait program did not fit synthetic RAM"
        );
        wait_memory.poke8(0xFFF4U, 0x60U);
        wait_memory.poke8(0xFFF5U, 0x00U);
        wait_machine.initialize(CpuProfile::hd6301v1, 0x5800U, 0x01FFU);
        jr800::debugger::Debugger wait_debugger;
        passed &= expect(
            wait_debugger.attach(wait_machine),
            "Wait debugger did not attach"
        );

        const auto entered = wait_debugger.run(10U);
        passed &= expect(
            entered.reason == StopReason::sleeping
                && entered.instructions_executed == 1U
                && entered.trigger_address == 0x5801U
                && entered.step.succeeded()
                && entered.step.bytes[0] == 0x3EU
                && entered.step.cycles == 9U
                && wait_machine.cpu().state().execution_state
                    == CpuExecutionState::waiting_for_interrupt
                && wait_machine.cpu().state().pc == 0x5801U
                && wait_machine.cpu().state().sp == 0x01F8U
                && wait_debugger.history_size() == 1U
                && wait_debugger.memory_access_size() == 8U,
            "Debugger did not report WAI entry"
        );

        wait_debugger.set_execution_breakpoint(0x5801U, true);
        const auto dormant = wait_debugger.run(10U);
        passed &= expect(
            dormant.reason == StopReason::sleeping
                && dormant.instructions_executed == 0U
                && dormant.trigger_address == 0x5801U
                && dormant.step.kind == StepKind::dormant
                && dormant.step.fault == CpuFault::none
                && wait_debugger.history_size() == 1U
                && wait_debugger.memory_access_size() == 8U,
            "Waiting CPU executed or yielded to a breakpoint"
        );

        wait_memory.set_maskable_interrupt_request(InterruptRequest{
            InterruptSource::timer_output_compare,
            true,
        });
        const auto resumed = wait_debugger.step();
        const auto history = wait_debugger.history();
        passed &= expect(
            resumed.reason == StopReason::step_complete
                && resumed.instructions_executed == 0U
                && resumed.step.succeeded()
                && resumed.step.kind == StepKind::interrupt_entry
                && resumed.step.cycles == 5U
                && wait_machine.cpu().state().execution_state
                    == CpuExecutionState::active
                && wait_machine.cpu().state().pc == 0x6000U
                && wait_machine.cpu().state().sp == 0x01F8U
                && wait_machine.cpu().state().cycle_count == 14U
                && history.size() == 2U
                && history.back().kind == StepKind::interrupt_entry
                && history.back().access_count == 2U
                && wait_debugger.memory_access_size() == 10U,
            "Debugger WAI interrupt-resume record differs"
        );
    }

    {
        jr800::core::SyntheticMachine software_interrupt_host;
        auto& software_interrupt_machine =
            software_interrupt_host.execution();
        auto& software_interrupt_memory = software_interrupt_host.bus();
        constexpr std::uint8_t software_interrupt_program[]{0x3FU, 0x01U};
        constexpr std::uint8_t software_interrupt_handler[]{0x3BU};
        passed &= expect(
            software_interrupt_memory.load(
                0x5A00U,
                software_interrupt_program
            )
                && software_interrupt_memory.load(
                    0x6000U,
                    software_interrupt_handler
                ),
            "Software-interrupt programs did not fit synthetic RAM"
        );
        software_interrupt_memory.poke8(0xFFFAU, 0x60U);
        software_interrupt_memory.poke8(0xFFFBU, 0x00U);
        software_interrupt_machine.initialize(
            CpuProfile::hd6301v1,
            0x5A00U,
            0x01FFU
        );
        jr800::debugger::Debugger software_interrupt_debugger;
        passed &= expect(
            software_interrupt_debugger.attach(software_interrupt_machine),
            "Software-interrupt debugger did not attach"
        );

        const auto entered = software_interrupt_debugger.step();
        const auto entry_history = software_interrupt_debugger.history();
        passed &= expect(
            entered.reason == StopReason::step_complete
                && entered.instructions_executed == 1U
                && entered.step.succeeded()
                && entered.step.kind == StepKind::instruction
                && entered.step.bytes[0] == 0x3FU
                && entered.step.cycles == 12U
                && entered.step.pc_before == 0x5A00U
                && entered.step.pc_after == 0x6000U
                && software_interrupt_machine.cpu().state().pc == 0x6000U
                && software_interrupt_machine.cpu().state().sp == 0x01F8U
                && software_interrupt_machine.cpu().state().cycle_count == 12U
                && entry_history.size() == 1U
                && entry_history[0U].access_count == 10U
                && software_interrupt_debugger.memory_access_size() == 10U,
            "Debugger SWI entry record differs"
        );

        software_interrupt_debugger.set_execution_breakpoint(0x5A01U, true);
        const auto returned = software_interrupt_debugger.run(10U);
        const auto round_trip_history = software_interrupt_debugger.history();
        passed &= expect(
            returned.reason == StopReason::execution_breakpoint
                && returned.instructions_executed == 1U
                && returned.trigger_address == 0x5A01U
                && software_interrupt_machine.cpu().state().pc == 0x5A01U
                && software_interrupt_machine.cpu().state().sp == 0x01FFU
                && software_interrupt_machine.cpu().state().cycle_count == 22U
                && round_trip_history.size() == 2U
                && round_trip_history[1U].bytes[0] == 0x3BU
                && round_trip_history[1U].access_count == 8U
                && software_interrupt_debugger.memory_access_size() == 18U,
            "Debugger did not preserve the SWI/RTI round trip"
        );

        software_interrupt_machine.initialize(
            CpuProfile::hd6301v1,
            0x5A00U,
            0x01FFU
        );
        software_interrupt_debugger.clear_history();
        software_interrupt_debugger.set_execution_breakpoint(0x5A00U, true);
        const auto stepped_over = software_interrupt_debugger.step_over(10U);
        passed &= expect(
            stepped_over.reason == StopReason::address_reached
                && stepped_over.instructions_executed == 2U
                && stepped_over.trigger_address == 0x5A01U
                && !stepped_over.continuation_address.has_value()
                && software_interrupt_machine.cpu().state().pc == 0x5A01U
                && software_interrupt_machine.cpu().state().sp == 0x01FFU
                && software_interrupt_debugger.history_size() == 2U,
            "Step-over did not use SWI's metadata-defined fall-through target"
        );

        software_interrupt_machine.initialize(
            CpuProfile::hd6301v1,
            0x5A00U,
            0x01FFU
        );
        software_interrupt_debugger.clear_history();
        const auto entered_for_step_out = software_interrupt_debugger.step();
        software_interrupt_debugger.set_execution_breakpoint(0x6000U, true);
        const auto stepped_out = software_interrupt_debugger.step_out(10U);
        passed &= expect(
            entered_for_step_out.reason == StopReason::step_complete
                && stepped_out.stop.reason == StopReason::step_out_complete
                && stepped_out.stop.instructions_executed == 1U
                && stepped_out.stop.trigger_address == 0x5A01U
                && !stepped_out.state.continued
                && software_interrupt_machine.cpu().state().pc == 0x5A01U
                && software_interrupt_machine.cpu().state().sp == 0x01FFU
                && software_interrupt_debugger.history_size() == 2U,
            "Step-out did not complete the active SWI frame at RTI"
        );
    }

    {
        jr800::core::SyntheticMachine interrupt_host;
        auto& interrupt_machine = interrupt_host.execution();
        auto& interrupt_memory = interrupt_host.bus();
        constexpr std::uint8_t sleep_program[]{
            0x86U, 0x12U,
            0xC6U, 0x34U,
            0xCEU, 0x56U, 0x78U,
            0x1AU,
        };
        constexpr std::uint8_t handler_program[]{0x01U};
        passed &= expect(
            interrupt_memory.load(0x5000U, sleep_program)
                && interrupt_memory.load(0x6000U, handler_program),
            "Interrupt test program did not fit synthetic RAM"
        );
        interrupt_memory.poke8(0xFFF4U, 0x60U);
        interrupt_memory.poke8(0xFFF5U, 0x00U);
        interrupt_machine.initialize(
            CpuProfile::hd6301v1,
            0x5000U,
            0x01FFU
        );
        jr800::debugger::Debugger interrupt_debugger;
        passed &= expect(
            interrupt_debugger.attach(interrupt_machine)
                && interrupt_debugger.run(10U).reason == StopReason::sleeping,
            "Interrupt test did not enter sleep"
        );

        interrupt_memory.set_maskable_interrupt_request(InterruptRequest{
            InterruptSource::timer_output_compare,
            true,
        });
        const auto interrupt = interrupt_debugger.step();
        const auto& interrupt_state = interrupt_machine.cpu().state();
        passed &= expect(
            interrupt.reason == StopReason::step_complete
                && interrupt.instructions_executed == 0U
                && interrupt.step.succeeded()
                && interrupt.step.kind == StepKind::interrupt_entry
                && interrupt.step.interrupt_source
                    == InterruptSource::timer_output_compare
                && interrupt.step.bytes_fetched == 0U
                && interrupt.step.instruction_length == 0U
                && interrupt.step.cycles == 12U
                && interrupt.step.pc_before == 0x5008U
                && interrupt.step.pc_after == 0x6000U
                && interrupt_state.pc == 0x6000U
                && interrupt_state.sp == 0x01F8U
                && interrupt_state.execution_state == CpuExecutionState::active
                && interrupt_state.cycle_count == 23U
                && (interrupt_state.condition_code
                    & jr800::core::condition_mask(
                        ConditionCode::interrupt_mask
                    )) != 0U,
            "Sleeping interrupt entry state or timing differs"
        );
        passed &= expect(
            interrupt_memory.peek8(0x01FFU) == 0x08U
                && interrupt_memory.peek8(0x01FEU) == 0x50U
                && interrupt_memory.peek8(0x01FDU) == 0x78U
                && interrupt_memory.peek8(0x01FCU) == 0x56U
                && interrupt_memory.peek8(0x01FBU) == 0x12U
                && interrupt_memory.peek8(0x01FAU) == 0x34U
                && interrupt_memory.peek8(0x01F9U) == 0xC0U,
            "Interrupt stack byte order differs"
        );
        const auto interrupt_history = interrupt_debugger.history();
        const auto interrupt_accesses = interrupt_debugger.memory_accesses();
        passed &= expect(
            interrupt_history.size() == 5U
                && interrupt_history.back().kind == StepKind::interrupt_entry
                && interrupt_history.back().interrupt_source
                    == InterruptSource::timer_output_compare
                && interrupt_history.back().access_count == 9U
                && interrupt_accesses.size() == 17U
                && interrupt_accesses[8].kind == AccessKind::data_write
                && interrupt_accesses[8].address == 0x01FFU
                && interrupt_accesses[14].kind == AccessKind::data_write
                && interrupt_accesses[14].address == 0x01F9U
                && interrupt_accesses[15].kind == AccessKind::data_read
                && interrupt_accesses[15].address == 0xFFF4U
                && interrupt_accesses[16].kind == AccessKind::data_read
                && interrupt_accesses[16].address == 0xFFF5U,
            "Interrupt execution history or bus order differs"
        );

        interrupt_memory.set_maskable_interrupt_request({});
        interrupt_machine.initialize(
            CpuProfile::hd6301v1,
            0x5000U,
            0x01FFU
        );
        interrupt_debugger.clear_history();
        passed &= expect(
            interrupt_debugger.run(10U).reason == StopReason::sleeping,
            "Breakpoint wake test did not re-enter sleep"
        );
        interrupt_debugger.set_execution_breakpoint(0x6000U, true);
        interrupt_memory.set_maskable_interrupt_request(InterruptRequest{
            InterruptSource::timer_output_compare,
            true,
        });
        const auto handler_breakpoint = interrupt_debugger.run(1U);
        passed &= expect(
            handler_breakpoint.reason == StopReason::execution_breakpoint
                && handler_breakpoint.instructions_executed == 0U
                && handler_breakpoint.trigger_address == 0x6000U
                && interrupt_machine.cpu().state().pc == 0x6000U
                && interrupt_debugger.history_size() == 5U,
            "Run did not preserve the first handler breakpoint or budget"
        );
    }

    {
        jr800::core::SyntheticMachine masked_host;
        auto& masked_machine = masked_host.execution();
        auto& masked_memory = masked_host.bus();
        constexpr std::uint8_t masked_program[]{0x0FU, 0x1AU, 0x01U};
        passed &= expect(
            masked_memory.load(0x5200U, masked_program),
            "Masked wake program did not fit synthetic RAM"
        );
        masked_machine.initialize(CpuProfile::hd6301v1, 0x5200U, 0x01FFU);
        jr800::debugger::Debugger masked_debugger;
        passed &= expect(
            masked_debugger.attach(masked_machine)
                && masked_debugger.run(10U).reason == StopReason::sleeping,
            "Masked wake test did not enter sleep"
        );
        const auto before_wake = masked_machine.cpu().state();
        masked_memory.set_maskable_interrupt_request(InterruptRequest{
            InterruptSource::timer_overflow,
            true,
        });
        const auto wake = masked_debugger.step();
        const auto& after_wake = masked_machine.cpu().state();
        passed &= expect(
            wake.reason == StopReason::step_complete
                && wake.instructions_executed == 0U
                && wake.step.succeeded()
                && wake.step.kind == StepKind::sleep_resume
                && wake.step.interrupt_source
                    == InterruptSource::timer_overflow
                && wake.step.cycles == 0U
                && after_wake.pc == before_wake.pc
                && after_wake.sp == before_wake.sp
                && after_wake.cycle_count == before_wake.cycle_count
                && after_wake.execution_state == CpuExecutionState::active,
            "Masked interrupt did not resume after SLP without vectoring"
        );
    }

    {
        jr800::core::SyntheticMachine unknown_interrupt_host;
        auto& unknown_interrupt_machine = unknown_interrupt_host.execution();
        auto& unknown_interrupt_memory = unknown_interrupt_host.bus();
        constexpr std::uint8_t sleep_program[]{0x1AU};
        passed &= expect(
            unknown_interrupt_memory.load(0x5400U, sleep_program),
            "Unknown interrupt program did not fit synthetic RAM"
        );
        unknown_interrupt_machine.initialize(
            CpuProfile::hd6301v1,
            0x5400U,
            0x01FFU
        );
        jr800::debugger::Debugger unknown_interrupt_debugger;
        passed &= expect(
            unknown_interrupt_debugger.attach(unknown_interrupt_machine)
                && unknown_interrupt_debugger.step().reason
                    == StopReason::sleeping,
            "Unknown interrupt test did not enter sleep"
        );
        const auto before_unknown = unknown_interrupt_machine.cpu().state();
        unknown_interrupt_memory.set_maskable_interrupt_request(
            InterruptRequest{InterruptSource::none, false}
        );
        const auto unknown_interrupt = unknown_interrupt_debugger.step();
        passed &= expect(
            unknown_interrupt.reason == StopReason::cpu_fault
                && unknown_interrupt.instructions_executed == 0U
                && unknown_interrupt.step.kind == StepKind::interrupt_entry
                && unknown_interrupt.step.fault
                    == CpuFault::unknown_interrupt_request
                && unknown_interrupt_machine.cpu().state() == before_unknown,
            "Unknown sleeping interrupt request was guessed"
        );
    }

    {
        jr800::core::SyntheticMachine active_interrupt_host;
        auto& active_interrupt_machine = active_interrupt_host.execution();
        auto& active_interrupt_memory = active_interrupt_host.bus();
        constexpr std::uint8_t interrupted_program[]{0x01U};
        constexpr std::uint8_t handler_program[]{0x3BU};
        passed &= expect(
            active_interrupt_memory.load(0x6000U, interrupted_program)
                && active_interrupt_memory.load(0x7000U, handler_program),
            "Active interrupt programs did not fit synthetic RAM"
        );
        active_interrupt_memory.poke8(0xFFF4U, 0x70U);
        active_interrupt_memory.poke8(0xFFF5U, 0x00U);
        active_interrupt_machine.initialize(
            CpuProfile::hd6301v1,
            0x6000U,
            0x01FFU
        );
        jr800::debugger::Debugger active_interrupt_debugger;
        passed &= expect(
            active_interrupt_debugger.attach(active_interrupt_machine),
            "Active interrupt debugger did not attach"
        );
        active_interrupt_memory.set_maskable_interrupt_request(
            InterruptRequest{
                InterruptSource::timer_output_compare,
                true,
            }
        );

        const auto entry = active_interrupt_debugger.step();
        const auto& entry_state = active_interrupt_machine.cpu().state();
        const auto entry_history = active_interrupt_debugger.history();
        passed &= expect(
            entry.reason == StopReason::step_complete
                && entry.instructions_executed == 0U
                && entry.step.succeeded()
                && entry.step.kind == StepKind::interrupt_entry
                && entry.step.interrupt_source
                    == InterruptSource::timer_output_compare
                && entry.step.bytes_fetched == 0U
                && entry.step.instruction_length == 0U
                && entry.step.cycles == 12U
                && entry.step.pc_before == 0x6000U
                && entry.step.pc_after == 0x7000U
                && entry_state.pc == 0x7000U
                && entry_state.sp == 0x01F8U
                && entry_state.cycle_count == 12U
                && entry_state.maskable_interrupt_delay_cycles == 0U
                && entry_history.size() == 1U
                && entry_history.back().kind == StepKind::interrupt_entry
                && entry_history.back().access_count == 9U,
            "Active maskable interrupt entry differs"
        );
        passed &= expect(
            active_interrupt_memory.peek8(0x01FFU) == 0x00U
                && active_interrupt_memory.peek8(0x01FEU) == 0x60U
                && active_interrupt_memory.peek8(0x01FDU) == 0x00U
                && active_interrupt_memory.peek8(0x01FCU) == 0x00U
                && active_interrupt_memory.peek8(0x01FBU) == 0x00U
                && active_interrupt_memory.peek8(0x01FAU) == 0x00U
                && active_interrupt_memory.peek8(0x01F9U) == 0xC0U,
            "Active interrupt stack byte order differs"
        );

        active_interrupt_memory.set_maskable_interrupt_request({});
        const auto returned = active_interrupt_debugger.step();
        passed &= expect(
            returned.reason == StopReason::step_complete
                && returned.instructions_executed == 1U
                && returned.step.bytes[0] == 0x3BU
                && active_interrupt_machine.cpu().state().pc == 0x6000U
                && active_interrupt_machine.cpu().state().sp == 0x01FFU,
            "Active interrupt RTI did not restore the boundary"
        );
        active_interrupt_memory.set_maskable_interrupt_request(
            InterruptRequest{
                InterruptSource::timer_output_compare,
                true,
            }
        );
        const auto reentry = active_interrupt_debugger.step();
        passed &= expect(
            reentry.reason == StopReason::step_complete
                && reentry.instructions_executed == 0U
                && reentry.step.kind == StepKind::interrupt_entry
                && reentry.step.pc_before == 0x6000U
                && reentry.step.pc_after == 0x7000U,
            "Pending interrupt was not sampled immediately after RTI"
        );
    }

    {
        jr800::core::SyntheticMachine unknown_active_interrupt_host;
        auto& unknown_active_machine =
            unknown_active_interrupt_host.execution();
        auto& unknown_active_memory = unknown_active_interrupt_host.bus();
        constexpr std::uint8_t program[]{0x01U};
        passed &= expect(
            unknown_active_memory.load(0x6200U, program),
            "Unknown active interrupt program did not fit synthetic RAM"
        );
        unknown_active_machine.initialize(
            CpuProfile::hd6301v1,
            0x6200U,
            0x01FFU
        );
        unknown_active_memory.set_maskable_interrupt_request(
            InterruptRequest{InterruptSource::none, false}
        );
        const auto before = unknown_active_machine.cpu().state();
        const auto unknown = unknown_active_machine.step_instruction();
        passed &= expect(
            unknown.kind == StepKind::interrupt_entry
                && unknown.fault == CpuFault::unknown_interrupt_request
                && unknown.bytes_fetched == 0U
                && !unknown.step_completed
                && unknown_active_machine.cpu().state() == before,
            "Unknown active interrupt request was guessed"
        );
    }

    {
        jr800::core::SyntheticMachine masked_active_interrupt_host;
        auto& masked_active_machine =
            masked_active_interrupt_host.execution();
        auto& masked_active_memory = masked_active_interrupt_host.bus();
        constexpr std::uint8_t program[]{0x0FU, 0x01U, 0x01U};
        passed &= expect(
            masked_active_memory.load(0x6300U, program),
            "Masked active interrupt program did not fit synthetic RAM"
        );
        masked_active_machine.initialize(
            CpuProfile::hd6301v1,
            0x6300U,
            0x01FFU
        );
        passed &= expect(
            masked_active_machine.step_instruction().succeeded(),
            "Masked active interrupt SEI seed failed"
        );
        masked_active_memory.set_maskable_interrupt_request(InterruptRequest{
            InterruptSource::timer_overflow,
            true,
        });
        const auto asserted = masked_active_machine.step_instruction();
        masked_active_memory.set_maskable_interrupt_request(
            InterruptRequest{InterruptSource::none, false}
        );
        const auto unknown = masked_active_machine.step_instruction();
        passed &= expect(
            asserted.succeeded() && asserted.kind == StepKind::instruction
                && asserted.bytes[0] == 0x01U
                && unknown.succeeded() && unknown.kind == StepKind::instruction
                && unknown.bytes[0] == 0x01U
                && masked_active_machine.cpu().state().pc == 0x6303U,
            "Masked active request blocked ordinary instructions"
        );
    }

    {
        jr800::core::SyntheticMachine cli_delay_host;
        auto& cli_delay_machine = cli_delay_host.execution();
        auto& cli_delay_memory = cli_delay_host.bus();
        constexpr std::uint8_t program[]{
            0x0FU,
            0x0EU,
            0x00U,
            0x01U,
            0x01U,
        };
        passed &= expect(
            cli_delay_memory.load(0x6400U, program),
            "CLI delay program did not fit synthetic RAM"
        );
        cli_delay_memory.poke8(0xFFF2U, 0x72U);
        cli_delay_memory.poke8(0xFFF3U, 0x00U);
        cli_delay_machine.initialize(
            CpuProfile::hd6301v1,
            0x6400U,
            0x01FFU
        );
        passed &= expect(
            cli_delay_machine.step_instruction().succeeded(),
            "CLI delay SEI seed failed"
        );
        cli_delay_memory.set_maskable_interrupt_request(InterruptRequest{
            InterruptSource::timer_overflow,
            true,
        });
        const auto cli = cli_delay_machine.step_instruction();
        const auto before_fault = cli_delay_machine.cpu().state();
        const auto fault = cli_delay_machine.step_instruction();
        const auto after_fault = cli_delay_machine.cpu().state();
        cli_delay_memory.poke8(0x6402U, 0x01U);
        const auto first_nop = cli_delay_machine.step_instruction();
        const auto second_nop = cli_delay_machine.step_instruction();
        const auto delayed_entry = cli_delay_machine.step_instruction();
        passed &= expect(
            cli.succeeded() && cli.bytes[0] == 0x0EU
                && fault.fault == CpuFault::unsupported_opcode
                && after_fault == before_fault
                && before_fault.maskable_interrupt_delay_cycles == 2U
                && first_nop.succeeded() && first_nop.bytes[0] == 0x01U
                && second_nop.succeeded() && second_nop.bytes[0] == 0x01U
                && delayed_entry.succeeded()
                && delayed_entry.kind == StepKind::interrupt_entry
                && delayed_entry.interrupt_source
                    == InterruptSource::timer_overflow
                && delayed_entry.pc_before == 0x6404U
                && delayed_entry.pc_after == 0x7200U
                && cli_delay_machine.cpu().state().cycle_count == 16U,
            "CLI one-cycle successor interrupt delay differs"
        );
        passed &= expect(
            fault.bytes_fetched == 1U && fault.pc_before == 0x6402U
                && fault.pc_after == 0x6402U
                && before_fault.pc == 0x6402U
                && before_fault.maskable_interrupt_delay_cycles == 2U,
            "Failed instruction consumed the CLI interrupt delay"
        );
    }

    {
        jr800::core::SyntheticMachine long_cli_delay_host;
        auto& long_cli_delay_machine = long_cli_delay_host.execution();
        auto& long_cli_delay_memory = long_cli_delay_host.bus();
        constexpr std::uint8_t program[]{
            0x0FU,
            0x0EU,
            0x86U,
            0x5AU,
            0x01U,
        };
        passed &= expect(
            long_cli_delay_memory.load(0x6600U, program),
            "Long CLI delay program did not fit synthetic RAM"
        );
        long_cli_delay_memory.poke8(0xFFF4U, 0x74U);
        long_cli_delay_memory.poke8(0xFFF5U, 0x00U);
        long_cli_delay_machine.initialize(
            CpuProfile::hd6301v1,
            0x6600U,
            0x01FFU
        );
        passed &= expect(
            long_cli_delay_machine.step_instruction().succeeded(),
            "Long CLI delay SEI seed failed"
        );
        long_cli_delay_memory.set_maskable_interrupt_request(
            InterruptRequest{
                InterruptSource::timer_output_compare,
                true,
            }
        );
        const auto cli = long_cli_delay_machine.step_instruction();
        const auto load = long_cli_delay_machine.step_instruction();
        const auto delayed_entry = long_cli_delay_machine.step_instruction();
        passed &= expect(
            cli.succeeded() && cli.bytes[0] == 0x0EU
                && load.succeeded() && load.bytes[0] == 0x86U
                && load.cycles == 2U
                && delayed_entry.succeeded()
                && delayed_entry.kind == StepKind::interrupt_entry
                && delayed_entry.pc_before == 0x6604U
                && delayed_entry.pc_after == 0x7400U
                && long_cli_delay_machine.cpu().state().a == 0x5AU
                && long_cli_delay_machine.cpu().state().cycle_count == 16U,
            "CLI multi-cycle successor interrupt delay differs"
        );
    }

    {
        jr800::core::SyntheticMachine tap_delay_host;
        auto& tap_delay_machine = tap_delay_host.execution();
        auto& tap_delay_memory = tap_delay_host.bus();
        constexpr std::uint8_t program[]{
            0x86U,
            0x00U,
            0x0FU,
            0x06U,
            0x01U,
            0x01U,
            0x01U,
        };
        passed &= expect(
            tap_delay_memory.load(0x6800U, program),
            "TAP delay program did not fit synthetic RAM"
        );
        tap_delay_memory.poke8(0xFFF0U, 0x76U);
        tap_delay_memory.poke8(0xFFF1U, 0x00U);
        tap_delay_machine.initialize(
            CpuProfile::hd6301v1,
            0x6800U,
            0x01FFU
        );
        passed &= expect(
            tap_delay_machine.step_instruction().succeeded()
                && tap_delay_machine.step_instruction().succeeded(),
            "TAP delay state seed failed"
        );
        tap_delay_memory.set_maskable_interrupt_request(
            InterruptRequest{InterruptSource::serial, true}
        );
        const auto tap = tap_delay_machine.step_instruction();
        const auto first_nop = tap_delay_machine.step_instruction();
        const auto second_nop = tap_delay_machine.step_instruction();
        const auto delayed_entry = tap_delay_machine.step_instruction();
        passed &= expect(
            tap.succeeded() && tap.bytes[0] == 0x06U
                && first_nop.succeeded() && first_nop.bytes[0] == 0x01U
                && second_nop.succeeded() && second_nop.bytes[0] == 0x01U
                && delayed_entry.succeeded()
                && delayed_entry.kind == StepKind::interrupt_entry
                && delayed_entry.interrupt_source == InterruptSource::serial
                && delayed_entry.pc_before == 0x6806U
                && delayed_entry.pc_after == 0x7600U,
            "TAP pending-interrupt delay differs"
        );
    }

    {
        jr800::core::SyntheticMachine late_interrupt_host;
        auto& late_interrupt_machine = late_interrupt_host.execution();
        auto& late_interrupt_memory = late_interrupt_host.bus();
        constexpr std::uint8_t program[]{0x0FU, 0x0EU, 0x01U};
        passed &= expect(
            late_interrupt_memory.load(0x6A00U, program),
            "Late interrupt program did not fit synthetic RAM"
        );
        late_interrupt_memory.poke8(0xFFF6U, 0x78U);
        late_interrupt_memory.poke8(0xFFF7U, 0x00U);
        late_interrupt_machine.initialize(
            CpuProfile::hd6301v1,
            0x6A00U,
            0x01FFU
        );
        passed &= expect(
            late_interrupt_machine.step_instruction().succeeded()
                && late_interrupt_machine.step_instruction().succeeded()
                && late_interrupt_machine.cpu().state()
                       .maskable_interrupt_delay_cycles
                    == 0U,
            "CLI without a pending request created an interrupt delay"
        );
        late_interrupt_memory.set_maskable_interrupt_request(
            InterruptRequest{
                InterruptSource::timer_input_capture,
                true,
            }
        );
        const auto entry = late_interrupt_machine.step_instruction();
        passed &= expect(
            entry.succeeded() && entry.kind == StepKind::interrupt_entry
                && entry.pc_before == 0x6A02U
                && entry.pc_after == 0x7800U,
            "Request asserted after CLI was incorrectly delayed"
        );
    }

    jr800::core::SyntheticMachine synthetic_machine;
    auto& machine = synthetic_machine.execution();
    auto& memory = synthetic_machine.bus();
    memory.clear();
    const std::vector<std::uint8_t> program{
        0x86, 0x80,
        0x97, 0x20,
        0x20, 0xFA,
    };
    passed &= expect(
        memory.load(0x1000, program),
        "Program did not fit synthetic RAM"
    );
    machine.initialize(CpuProfile::hd6301v1, 0x1000, 0x01FF);

    jr800::debugger::Debugger debugger;
    passed &= expect(debugger.attach(machine), "Debugger did not attach");

    const auto load = debugger.step();
    passed &= expect(load.reason == StopReason::step_complete, "LDAA step failed");
    passed &= expect(
        load.step.bytes_fetched == 2U && load.step.cycles == 2U,
        "LDAA fetch/cycle result mismatch"
    );
    passed &= expect(
        machine.cpu().state().a == 0x80U && machine.cpu().state().pc == 0x1002U,
        "LDAA register result mismatch"
    );
    passed &= expect(
        (machine.cpu().state().condition_code
         & jr800::core::condition_mask(ConditionCode::negative)) != 0U,
        "LDAA negative flag mismatch"
    );
    passed &= expect(
        (machine.cpu().state().condition_code
         & jr800::core::condition_mask(ConditionCode::zero)) == 0U
            && (machine.cpu().state().condition_code
                & jr800::core::condition_mask(ConditionCode::overflow)) == 0U,
        "LDAA zero/overflow flags mismatch"
    );

    const auto store = debugger.step();
    passed &= expect(store.reason == StopReason::step_complete, "STAA step failed");
    passed &= expect(memory.peek8(0x0020) == 0x80U, "STAA memory result mismatch");
    passed &= expect(
        machine.cpu().state().cycle_count == 5U,
        "LDAA/STAA accumulated cycle count mismatch"
    );

    const auto branch = debugger.step();
    passed &= expect(branch.reason == StopReason::step_complete, "BRA step failed");
    passed &= expect(machine.cpu().state().pc == 0x1000U, "BRA signed target mismatch");
    passed &= expect(machine.cpu().state().cycle_count == 8U, "BRA cycle mismatch");

    const auto accesses = debugger.memory_accesses();
    passed &= expect(accesses.size() == 7U, "Structured access count mismatch");
    if (accesses.size() == 7U) {
        passed &= expect(
            accesses[0].kind == AccessKind::instruction_fetch
                && accesses[0].address == 0x1000U
                && accesses[1].kind == AccessKind::instruction_fetch
                && accesses[1].address == 0x1001U,
            "LDAA bus access order mismatch"
        );
        passed &= expect(
            accesses[2].address == 0x1002U
                && accesses[3].address == 0x1003U
                && accesses[4].kind == AccessKind::data_write
                && accesses[4].address == 0x0020U
                && accesses[4].value == 0x80U
                && accesses[4].previous_value == 0U,
            "STAA bus access order mismatch"
        );
        passed &= expect(
            accesses[4].instruction_pc == 0x1002U
                && accesses[4].instruction_cycle == 2U,
            "Bus instruction context mismatch"
        );
    }

    const auto history = debugger.history();
    passed &= expect(history.size() == 3U, "Instruction history count mismatch");
    if (history.size() == 3U) {
        passed &= expect(
            history[1].pc_before == 0x1002U
                && history[1].pc_after == 0x1004U
                && history[1].access_count == 3U
                && history[1].state_after.a == 0x80U,
            "STAA instruction history mismatch"
        );
    }

    const auto disassembly = debugger.disassemble(0x1004);
    passed &= expect(
        disassembly.has_value() && disassembly->supported
            && disassembly->text == "BRA $1000",
        "Metadata-driven disassembly mismatch"
    );
    memory.poke8(0x1010U, 0x02U);
    const auto unknown_disassembly = debugger.disassemble(0x1010U);
    passed &= expect(
        unknown_disassembly.has_value()
            && !unknown_disassembly->supported
            && unknown_disassembly->length == 1U
            && unknown_disassembly->bytes[0] == 0x02U
            && unknown_disassembly->bytes[1] == 0U
            && unknown_disassembly->bytes[2] == 0U
            && unknown_disassembly->text == ".byte $02",
        "Shared unknown-opcode disassembly mismatch"
    );
    debugger.set_execution_breakpoint(0x1000, true);
    const auto breakpoint = debugger.run(10);
    passed &= expect(
        breakpoint.reason == StopReason::execution_breakpoint
            && breakpoint.instructions_executed == 0U
            && machine.cpu().state().pc == 0x1000U,
        "Execution breakpoint did not stop before fetch"
    );
    debugger.set_execution_breakpoint(0x1000, false);
    debugger.detach();

    memory.clear();
    memory.poke8(0x2000, 0x86);
    memory.poke8(0x2001, 0x00);
    machine.initialize(CpuProfile::hd6301v1, 0x2000, 0x01FF);
    passed &= expect(debugger.attach(machine), "Debugger reattach failed");
    const auto zero = debugger.step();
    passed &= expect(zero.reason == StopReason::step_complete, "Zero LDAA step failed");
    passed &= expect(
        (machine.cpu().state().condition_code
         & jr800::core::condition_mask(ConditionCode::zero)) != 0U
            && (machine.cpu().state().condition_code
                & jr800::core::condition_mask(ConditionCode::negative)) == 0U,
        "LDAA zero flag mismatch"
    );
    debugger.detach();

    memory.clear();
    memory.poke8(0x4000, 0x01);
    machine.initialize(CpuProfile::jr800_unresolved, 0x4000, 0x01FF);
    passed &= expect(debugger.attach(machine), "Debugger attach for profile fault failed");
    const auto unsupported = debugger.step();
    passed &= expect(
        unsupported.reason == StopReason::cpu_fault
            && unsupported.step.fault == CpuFault::unsupported_opcode,
        "Unresolved profile inherited executable metadata"
    );

    return passed ? 0 : 1;
}
