// SPDX-License-Identifier: MIT

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "jr800/core/cpu.hpp"
#include "jr800/core/jr800_bus.hpp"
#include "jr800/isa/instruction_metadata.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

class RecordingObserver final : public jr800::core::BusObserver {
public:
    void on_bus_access(
        const jr800::core::BusAccessEvent& event
    ) noexcept override {
        if (event_count < events.size()) {
            events[event_count] = event;
            ++event_count;
        }
    }

    std::array<jr800::core::BusAccessEvent, 16U> events{};
    std::size_t event_count{};
};

std::vector<std::uint8_t> nop_rom() {
    return std::vector<std::uint8_t>(
        jr800::core::jr800_logical_rom_size,
        0x01U
    );
}

}  // namespace

int main() {
    using jr800::core::AccessKind;
    using jr800::core::BusFault;
    using jr800::core::Cpu;
    using jr800::core::CpuFault;
    using jr800::core::InterruptSource;
    using jr800::core::Jr800Bus;
    using jr800::core::Jr800MemoryStatus;
    using jr800::isa::CpuProfile;

    bool passed = true;

    {
        Jr800Bus bus;
        static_cast<void>(bus.advance_cycles(1U));
        const auto unknown_capture_status = bus.inspect8(0x0008U);
        bus.set_port2_pin_state(0U, 0x1FU);
        bus.reset_cpu_devices();
        const auto reset_status = bus.inspect8(0x0008U);
        passed &= expect(
            unknown_capture_status.fault == BusFault::uninitialized_read
                && !unknown_capture_status.value.has_value()
                && reset_status.succeeded()
                && reset_status.value == 0U,
            "Unknown capture input produced known TCSR status"
        );
    }

    {
        Jr800Bus bus;
        const auto missing_rom = bus.read8(
            0x8000U,
            AccessKind::instruction_fetch
        );
        passed &= expect(
            missing_rom.fault == BusFault::backing_store_unavailable
                && !missing_rom.value.has_value(),
            "Failed bus read exposed a fallback byte"
        );
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto before = cpu.state();
        const auto step = cpu.step_instruction(bus);
        const auto counter_after_fault = bus.inspect8(0x000AU);
        passed &= expect(
            step.fault == CpuFault::bus_access
                && step.bus_fault == BusFault::backing_store_unavailable
                && step.fault_address == 0x8000U
                && step.fault_access == AccessKind::instruction_fetch
                && step.bytes_fetched == 0U
                && counter_after_fault.succeeded()
                && counter_after_fault.value == 0U
                && cpu.state() == before,
            "Missing ROM advanced CPU or device time"
        );
    }

    {
        Jr800Bus bus;
        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "Discarded-read observer attach failed"
        );

        const auto missing_rom = bus.read8_discard(0x8000U);
        const auto unsupported = bus.read8_discard(0x0A00U);
        const auto unknown_port = bus.read8_discard(0x0002U);
        const auto unknown_ram_control = bus.read8_discard(0x0014U);
        const auto ordinary_unknown = bus.read8(
            0x0080U,
            AccessKind::data_read
        );
        const auto internal_discard = bus.read8_discard(0x0080U);
        const auto internal_after_discard = bus.inspect8(0x0080U);
        const auto standard_discard = bus.read8_discard(0x2000U);
        const auto standard_after_discard = bus.inspect8(0x2000U);
        passed &= expect(
            missing_rom.fault == BusFault::backing_store_unavailable
                && unsupported.fault == BusFault::unsupported_access
                && unknown_port.fault == BusFault::uninitialized_read
                && unknown_ram_control.fault == BusFault::uninitialized_read
                && ordinary_unknown.fault == BusFault::uninitialized_read
                && internal_discard.succeeded()
                && standard_discard.succeeded()
                && internal_after_discard.fault
                    == BusFault::uninitialized_read
                && standard_after_discard.fault
                    == BusFault::uninitialized_read,
            "Discarded read guessed a device or initialized RAM value"
        );
        passed &= expect(
            observer.event_count == 2U
                && observer.events[0].kind == AccessKind::data_read
                && observer.events[0].address == 0x0080U
                && !observer.events[0].value_known
                && !observer.events[0].previous_value_known
                && observer.events[1].kind == AccessKind::data_read
                && observer.events[1].address == 0x2000U
                && !observer.events[1].value_known
                && !observer.events[1].previous_value_known,
            "Discarded unknown RAM reads were not traced as unknown"
        );

        const auto write = bus.write8(0x0080U, 0x42U);
        const auto known_discard = bus.read8_discard(0x0080U);
        passed &= expect(
            write.succeeded() && !write.previous_value_known
                && known_discard.succeeded()
                && observer.event_count == 4U
                && observer.events[2].kind == AccessKind::data_write
                && observer.events[2].value_known
                && !observer.events[2].previous_value_known
                && observer.events[3].kind == AccessKind::data_read
                && observer.events[3].value == 0x42U
                && observer.events[3].value_known,
            "Discarded known RAM read lost its value or knownness"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "Logical ROM load failed"
        );

        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto nop = cpu.step_instruction(bus);
        passed &= expect(
            nop.succeeded() && nop.bytes_fetched == 1U
                && cpu.state().pc == 0x8001U
                && cpu.state().cycle_count == 1U,
            "ROM-backed NOP did not execute"
        );

        cpu.initialize(CpuProfile::hd6301v1, 0x2000U, 0x01FFU);
        const auto ram_before = cpu.state();
        const auto uninitialized = cpu.step_instruction(bus);
        passed &= expect(
            uninitialized.fault == CpuFault::bus_access
                && uninitialized.bus_fault == BusFault::uninitialized_read
                && uninitialized.fault_address == 0x2000U
                && cpu.state() == ram_before,
            "Uninitialized RAM fetch returned an invented value"
        );

        cpu.initialize(CpuProfile::hd6301v1, 0x0A00U, 0x01FFU);
        const auto lcd_before = cpu.state();
        const auto lcd = cpu.step_instruction(bus);
        passed &= expect(
            lcd.fault == CpuFault::bus_access
                && lcd.bus_fault == BusFault::unsupported_access
                && lcd.fault_address == 0x0A00U
                && cpu.state() == lcd_before,
            "Unsupported LCD fetch returned a guessed value"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom.back() = 0x86U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "Wraparound operand ROM load failed"
        );

        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0xFFFFU, 0x01FFU);
        const auto before = cpu.state();
        const auto step = cpu.step_instruction(bus);
        passed &= expect(
            step.fault == CpuFault::bus_access
                && step.bus_fault == BusFault::unsupported_access
                && step.fault_address == 0x0000U
                && step.bytes[0] == 0x86U
                && step.bytes_fetched == 1U
                && step.instruction_length == 2U
                && cpu.state() == before,
            "Operand fault changed CPU state or consumed a guessed byte"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0x1FU;
        rom[2U] = 0x97U;
        rom[3U] = 0x01U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "Port 2 direction-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "Port 2 direction observer attach failed"
        );
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        const auto read = bus.read8(0x0001U, AccessKind::data_read);
        const auto inspected = bus.inspect8(0x0001U);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8004U
                && cpu.state().cycle_count == 5U,
            "Port 2 direction write did not complete"
        );
        passed &= expect(
            observer.event_count == 5U
                && observer.events[4].kind == AccessKind::data_write
                && observer.events[4].address == 0x0001U
                && observer.events[4].value == 0x1FU
                && observer.events[4].previous_value == 0U
                && observer.events[4].previous_value_known,
            "Port 2 direction write trace differs"
        );
        passed &= expect(
            read.fault == BusFault::unsupported_access
                && inspected.fault == BusFault::unsupported_access
                && !read.value.has_value()
                && !inspected.value.has_value(),
            "Write-only Port 2 direction register returned a value"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0x80U;
        rom[2U] = 0x97U;
        rom[3U] = 0x15U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "Write-fault ROM load failed"
        );

        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        passed &= expect(
            cpu.step_instruction(bus).succeeded(),
            "Write-fault setup instruction failed"
        );
        const auto before = cpu.state();
        const auto step = cpu.step_instruction(bus);
        passed &= expect(
            step.fault == CpuFault::bus_access
                && step.bus_fault == BusFault::unsupported_access
                && step.fault_address == 0x0015U
                && step.fault_access == AccessKind::data_write
                && step.bytes_fetched == 2U
                && cpu.state() == before,
            "Rejected write changed CPU state"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0x3CU;
        rom[2U] = 0x97U;
        rom[3U] = 0x00U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "Port 1 direction-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "Port 1 direction observer attach failed"
        );
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        const auto read = bus.read8(0x0000U, AccessKind::data_read);
        const auto inspected = bus.inspect8(0x0000U);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8004U
                && cpu.state().cycle_count == 5U,
            "Port 1 direction write did not complete"
        );
        passed &= expect(
            observer.event_count == 5U
                && observer.events[4].kind == AccessKind::data_write
                && observer.events[4].address == 0x0000U
                && observer.events[4].value == 0x3CU
                && observer.events[4].previous_value == 0U
                && observer.events[4].previous_value_known,
            "Port 1 direction write trace differs"
        );
        passed &= expect(
            read.fault == BusFault::unsupported_access
                && inspected.fault == BusFault::unsupported_access
                && !read.value.has_value()
                && !inspected.value.has_value(),
            "Write-only Port 1 direction register returned a value"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0x5AU;
        rom[2U] = 0x97U;
        rom[3U] = 0x02U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "Port 1 data-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "Port 1 data observer attach failed"
        );
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        const auto read = bus.read8(0x0002U, AccessKind::data_read);
        const auto inspected = bus.inspect8(0x0002U);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8004U
                && cpu.state().cycle_count == 5U,
            "Port 1 data write did not complete"
        );
        passed &= expect(
            observer.event_count == 5U
                && observer.events[4].kind == AccessKind::data_write
                && observer.events[4].address == 0x0002U
                && observer.events[4].value == 0x5AU
                && !observer.events[4].previous_value_known,
            "Port 1 data write trace exposed an undefined reset value"
        );
        passed &= expect(
            read.fault == BusFault::uninitialized_read
                && inspected.fault == BusFault::uninitialized_read
                && !read.value.has_value()
                && !inspected.value.has_value(),
            "Port 1 data read returned an invented pin state"
        );

        bus.set_port1_pin_state(0xC3U, 0x0FU);
        const auto partial = bus.read8(0x0002U, AccessKind::data_read);
        passed &= expect(
            partial.fault == BusFault::uninitialized_read
                && !partial.value.has_value()
                && observer.event_count == 5U,
            "Partially known Port 1 pins produced a bus value"
        );

        bus.set_port1_pin_state(0xC3U, 0xFFU);
        const auto pin_read = bus.read8(0x0002U, AccessKind::data_read);
        const auto pin_inspected = bus.inspect8(0x0002U);
        passed &= expect(
            pin_read.succeeded() && pin_read.value == 0xC3U
                && pin_inspected.succeeded()
                && pin_inspected.value == 0xC3U
                && observer.event_count == 6U
                && observer.events[5].kind == AccessKind::data_read
                && observer.events[5].address == 0x0002U
                && observer.events[5].value == 0xC3U,
            "Port 1 read did not return the supplied pin state"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0x2AU;
        rom[2U] = 0x97U;
        rom[3U] = 0x03U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "Port 2 data-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "Port 2 data observer attach failed"
        );
        const auto unknown = bus.read8(0x0003U, AccessKind::data_read);
        const auto unknown_inspection = bus.inspect8(0x0003U);
        bus.set_port2_pin_state(0x12U, 0x0FU);
        const auto partial = bus.read8(0x0003U, AccessKind::data_read);
        passed &= expect(
            unknown.fault == BusFault::uninitialized_read
                && unknown_inspection.fault == BusFault::uninitialized_read
                && partial.fault == BusFault::uninitialized_read
                && !unknown.value.has_value()
                && !unknown_inspection.value.has_value()
                && !partial.value.has_value()
                && observer.event_count == 0U,
            "Absent or partial Port 2 pins produced a bus value"
        );

        bus.set_port2_pin_state(0x12U, 0x1FU);
        const auto pin_inspection = bus.inspect8(0x0003U);
        const auto pin_read = bus.read8(0x0003U, AccessKind::data_read);
        passed &= expect(
            pin_inspection.succeeded() && pin_inspection.value == 0xD2U
                && pin_read.succeeded() && pin_read.value == 0xD2U
                && observer.event_count == 1U
                && observer.events[0].kind == AccessKind::data_read
                && observer.events[0].address == 0x0003U
                && observer.events[0].value == 0xD2U,
            "Port 2 read did not combine mode bits with supplied pins"
        );

        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8004U
                && cpu.state().cycle_count == 5U
                && observer.event_count == 6U
                && observer.events[5].kind == AccessKind::data_write
                && observer.events[5].address == 0x0003U
                && observer.events[5].value == 0x2AU
                && !observer.events[5].previous_value_known,
            "First Port 2 data write exposed an undefined reset latch"
        );

        const auto second_write = bus.write8(0x0003U, 0xF5U);
        const auto read_after_write = bus.read8(
            0x0003U,
            AccessKind::data_read
        );
        passed &= expect(
            second_write.succeeded()
                && second_write.previous_value_known
                && second_write.previous_value == 0xCAU
                && read_after_write.succeeded()
                && read_after_write.value == 0xD2U
                && observer.event_count == 8U
                && observer.events[6].kind == AccessKind::data_write
                && observer.events[6].value == 0xF5U
                && observer.events[6].previous_value == 0xCAU
                && observer.events[6].previous_value_known,
            "Port 2 write changed pin reads or lost the prior latch"
        );

        bus.reset_cpu_devices();
        const auto pin_state_after_reset = bus.inspect8(0x0003U);
        const auto first_write_after_reset = bus.write8(0x0003U, 0x01U);
        passed &= expect(
            pin_state_after_reset.succeeded()
                && pin_state_after_reset.value == 0xD2U
                && first_write_after_reset.succeeded()
                && !first_write_after_reset.previous_value_known
                && observer.event_count == 9U,
            "Port 2 reset changed pins or retained the undefined latch"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0xA5U;
        rom[2U] = 0x97U;
        rom[3U] = 0x05U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "Port 4 direction-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "Port 4 direction observer attach failed"
        );
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        const auto read = bus.read8(0x0005U, AccessKind::data_read);
        const auto inspected = bus.inspect8(0x0005U);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8004U
                && cpu.state().cycle_count == 5U,
            "Port 4 direction write did not complete"
        );
        passed &= expect(
            observer.event_count == 5U
                && observer.events[4].kind == AccessKind::data_write
                && observer.events[4].address == 0x0005U
                && observer.events[4].value == 0xA5U
                && observer.events[4].previous_value == 0U
                && observer.events[4].previous_value_known,
            "Port 4 direction write trace differs"
        );
        passed &= expect(
            read.fault == BusFault::unsupported_access
                && inspected.fault == BusFault::unsupported_access
                && !read.value.has_value()
                && !inspected.value.has_value(),
            "Write-only Port 4 direction register returned a value"
        );
    }

    {
        Jr800Bus bus;
        bus.set_port2_pin_state(0U, 0x1FU);
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0xFFU;
        rom[2U] = 0x97U;
        rom[3U] = 0x08U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "TCSR-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(bus.set_observer(&observer), "TCSR observer attach failed");
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        const auto read = bus.read8(0x0008U, AccessKind::data_read);
        const auto inspected = bus.inspect8(0x0008U);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8004U
                && cpu.state().cycle_count == 5U,
            "TCSR control write did not complete"
        );
        passed &= expect(
            observer.event_count == 6U
                && observer.events[4].kind == AccessKind::data_write
                && observer.events[4].address == 0x0008U
                && observer.events[4].value == 0xFFU
                && observer.events[4].previous_value_known
                && observer.events[4].previous_value == 0U,
            "TCSR write trace or previous value differs"
        );
        passed &= expect(
            read.succeeded() && read.value == 0x1FU
                && inspected.succeeded() && inspected.value == 0x1FU
                && observer.events[5].kind == AccessKind::data_read
                && observer.events[5].address == 0x0008U
                && observer.events[5].value == 0x1FU,
            "TCSR read or trace differs"
        );
    }

    {
        Jr800Bus bus;
        bus.reset_cpu_devices();
        const auto input_state = bus.port2_timer_output_state();
        static_cast<void>(bus.write8(0x0001U, 0x02U));
        const auto unresolved_output = bus.port2_timer_output_state();
        static_cast<void>(bus.write8(0x0008U, 0x01U));
        static_cast<void>(bus.write8(0x0009U, 0xFFU));
        static_cast<void>(bus.write8(0x000AU, 0xFCU));
        static_cast<void>(bus.advance_cycles(3U));
        const auto high_output = bus.port2_timer_output_state();
        static_cast<void>(bus.write8(0x0001U, 0x00U));
        const auto disabled_output = bus.port2_timer_output_state();
        passed &= expect(
            !input_state.output_enabled && !input_state.level.has_value()
                && unresolved_output.output_enabled
                && !unresolved_output.level.has_value()
                && high_output.output_enabled
                && high_output.level == true
                && !disabled_output.output_enabled
                && !disabled_output.level.has_value(),
            "Port 2 bit 1 did not gate the timer output by its DDR"
        );
    }

    {
        Jr800Bus bus;
        bus.set_port2_pin_state(0U, 0x1FU);
        bus.reset_cpu_devices();
        const auto edge_select = bus.write8(0x0008U, 0x02U);
        static_cast<void>(bus.advance_cycles(0x1234U));
        bus.set_port2_pin_state(1U, 0x1FU);
        const auto status = bus.read8(0x0008U, AccessKind::data_read);
        const auto captured_high = bus.inspect8(0x000DU);
        const auto captured_low = bus.inspect8(0x000EU);
        const auto clear = bus.read8(0x000DU, AccessKind::data_read);
        const auto cleared_status = bus.inspect8(0x0008U);
        passed &= expect(
            edge_select.succeeded()
                && edge_select.previous_value_known
                && edge_select.previous_value == 0U
                && status.succeeded() && status.value == 0x82U
                && captured_high.succeeded()
                && captured_high.value == 0x12U
                && captured_low.succeeded()
                && captured_low.value == 0x34U
                && clear.succeeded() && clear.value == 0x12U
                && cleared_status.succeeded()
                && cleared_status.value == 0x02U,
            "Port 2 input capture or ICF clear sequence differs"
        );

        bus.reset_cpu_devices();
        static_cast<void>(bus.write8(0x0001U, 0x01U));
        bus.set_port2_pin_state(0U, 0x1FU);
        bus.set_port2_pin_state(1U, 0x1FU);
        passed &= expect(
            bus.inspect8(0x0008U).value == 0U
                && bus.inspect8(0x000DU).fault
                    == BusFault::uninitialized_read,
            "Port 2 output mode admitted an input-capture edge"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0xCCU;
        rom[1U] = 0x5AU;
        rom[2U] = 0xF3U;
        rom[3U] = 0xDDU;
        rom[4U] = 0x09U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "FRC-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(bus.set_observer(&observer), "FRC observer attach failed");
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        const auto read_high = bus.read8(0x0009U, AccessKind::data_read);
        const auto inspected_low = bus.inspect8(0x000AU);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8005U
                && cpu.state().cycle_count == 7U,
            "FRC double-store did not complete"
        );
        passed &= expect(
            observer.event_count == 8U
                && observer.events[3].kind == AccessKind::instruction_fetch
                && observer.events[3].address == 0x8003U
                && observer.events[3].value == 0xDDU
                && observer.events[4].kind == AccessKind::instruction_fetch
                && observer.events[4].address == 0x8004U
                && observer.events[4].value == 0x09U
                && observer.events[5].kind == AccessKind::data_write
                && observer.events[5].address == 0x0009U
                && observer.events[5].value == 0x5AU
                && observer.events[5].previous_value_known
                && observer.events[5].previous_value == 0U
                && observer.events[6].kind == AccessKind::data_write
                && observer.events[6].address == 0x000AU
                && observer.events[6].value == 0xF3U
                && observer.events[6].previous_value_known
                && observer.events[6].previous_value == 0xF8U,
            "FRC double-store trace or write order differs"
        );
        passed &= expect(
            read_high.succeeded() && read_high.value == 0x5AU
                && inspected_low.succeeded()
                && inspected_low.value == 0xF7U
                && observer.event_count == 8U
                && observer.events[7].kind == AccessKind::data_read
                && observer.events[7].address == 0x0009U
                && observer.events[7].value == 0x5AU,
            "FRC progression, read, or trace differs"
        );
    }

    {
        Jr800Bus bus;
        const auto reset_high = bus.inspect8(0x000BU);
        const auto reset_low = bus.inspect8(0x000CU);
        auto rom = nop_rom();
        rom[0U] = 0xCCU;
        rom[1U] = 0x12U;
        rom[2U] = 0x34U;
        rom[3U] = 0xDDU;
        rom[4U] = 0x0BU;
        passed &= expect(
            reset_high.succeeded() && reset_high.value == 0xFFU
                && reset_low.succeeded() && reset_low.value == 0xFFU
                && bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "OCR reset value or ROM load differs"
        );

        RecordingObserver observer;
        passed &= expect(bus.set_observer(&observer), "OCR observer attach failed");
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8005U
                && cpu.state().cycle_count == 7U,
            "OCR double-store did not complete"
        );
        passed &= expect(
            observer.event_count == 7U
                && observer.events[5].kind == AccessKind::data_write
                && observer.events[5].address == 0x000BU
                && observer.events[5].value == 0x12U
                && observer.events[5].previous_value_known
                && observer.events[5].previous_value == 0xFFU
                && observer.events[6].kind == AccessKind::data_write
                && observer.events[6].address == 0x000CU
                && observer.events[6].value == 0x34U
                && observer.events[6].previous_value_known
                && observer.events[6].previous_value == 0xFFU,
            "OCR double-store trace or previous values differ"
        );

        const auto read_high = bus.read8(0x000BU, AccessKind::data_read);
        const auto inspected_low = bus.inspect8(0x000CU);
        passed &= expect(
            read_high.succeeded() && read_high.value == 0x12U
                && inspected_low.succeeded() && inspected_low.value == 0x34U
                && observer.event_count == 8U
                && observer.events[7].kind == AccessKind::data_read
                && observer.events[7].address == 0x000BU
                && observer.events[7].value == 0x12U,
            "OCR read or side-effect-free inspection differs"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0xFFU;
        rom[2U] = 0x97U;
        rom[3U] = 0x10U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "RMCR-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(bus.set_observer(&observer), "RMCR observer attach failed");
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        const auto read = bus.read8(0x0010U, AccessKind::data_read);
        const auto inspected = bus.inspect8(0x0010U);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8004U
                && cpu.state().cycle_count == 5U,
            "RMCR control write did not complete"
        );
        passed &= expect(
            observer.event_count == 5U
                && observer.events[4].kind == AccessKind::data_write
                && observer.events[4].address == 0x0010U
                && observer.events[4].value == 0xFFU
                && observer.events[4].previous_value_known
                && observer.events[4].previous_value == 0U,
            "RMCR write trace or reset value differs"
        );
        passed &= expect(
            read.fault == BusFault::unsupported_access
                && inspected.fault == BusFault::unsupported_access
                && !read.value.has_value()
                && !inspected.value.has_value(),
            "Write-only RMCR returned a value"
        );
    }

    {
        Jr800Bus bus;
        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "TRCSR reset-read observer attach failed"
        );
        const auto read = bus.read8(0x0011U, AccessKind::data_read);
        const auto inspected = bus.inspect8(0x0011U);
        const auto discarded = bus.read8_discard(0x0011U);
        passed &= expect(
            read.succeeded() && read.value == 0x20U
                && inspected.succeeded() && inspected.value == 0x20U
                && discarded.succeeded()
                && observer.event_count == 2U
                && observer.events[0].kind == AccessKind::data_read
                && observer.events[0].address == 0x0011U
                && observer.events[0].value == 0x20U
                && observer.events[1].kind == AccessKind::data_read
                && observer.events[1].address == 0x0011U
                && observer.events[1].value == 0x20U,
            "TRCSR reset read, inspection, discard, or trace differs"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0xD6U;
        rom[1U] = 0x11U;
        bus.set_port2_pin_state(0x08U, 0x08U);
        const auto configured = bus.write8(0x0011U, 0x18U);
        static_cast<void>(bus.advance_cycles(4096U));
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok
                && configured.succeeded(),
            "TRCSR direct-load setup failed"
        );

        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "TRCSR direct-load observer attach failed"
        );
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        passed &= expect(
            load.succeeded() && load.bytes_fetched == 2U
                && load.cycles == 3U && load.pc_after == 0x8002U
                && cpu.state().b == 0x38U
                && observer.event_count == 3U
                && observer.events[0].kind
                    == AccessKind::instruction_fetch
                && observer.events[0].address == 0x8000U
                && observer.events[0].value == 0xD6U
                && observer.events[1].kind
                    == AccessKind::instruction_fetch
                && observer.events[1].address == 0x8001U
                && observer.events[1].value == 0x11U
                && observer.events[2].kind == AccessKind::data_read
                && observer.events[2].address == 0x0011U
                && observer.events[2].value == 0x38U,
            "Direct LDAB did not consume the known idle TRCSR value"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0xFFU;
        rom[2U] = 0x97U;
        rom[3U] = 0x11U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "TRCSR-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(bus.set_observer(&observer), "TRCSR observer attach failed");
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        const auto load = cpu.step_instruction(bus);
        const auto write = cpu.step_instruction(bus);
        const auto read = bus.read8(0x0011U, AccessKind::data_read);
        const auto inspected = bus.inspect8(0x0011U);
        passed &= expect(
            load.succeeded() && write.succeeded()
                && cpu.state().pc == 0x8004U
                && cpu.state().cycle_count == 5U,
            "TRCSR control write did not complete"
        );
        passed &= expect(
            observer.event_count == 6U
                && observer.events[4].kind == AccessKind::data_write
                && observer.events[4].address == 0x0011U
                && observer.events[4].value == 0xFFU
                && observer.events[4].previous_value_known
                && observer.events[4].previous_value == 0x20U,
            "TRCSR write trace lost the known reset value"
        );
        passed &= expect(
            read.succeeded() && read.value == 0x3FU
                && inspected.succeeded() && inspected.value == 0x3FU
                && observer.events[5].kind == AccessKind::data_read
                && observer.events[5].address == 0x0011U
                && observer.events[5].value == 0x3FU,
            "TRCSR changed before a receive event could occur"
        );

        static_cast<void>(bus.advance_cycles(157U));
        const auto unknown_read =
            bus.read8(0x0011U, AccessKind::data_read);
        const auto unknown_inspection = bus.inspect8(0x0011U);
        passed &= expect(
            unknown_read.fault == BusFault::uninitialized_read
                && unknown_inspection.fault == BusFault::uninitialized_read
                && !unknown_read.value.has_value()
                && !unknown_inspection.value.has_value()
                && observer.event_count == 6U,
            "Potential receiver status returned a guessed value"
        );
    }

    {
        Jr800Bus bus;
        passed &= expect(
            bus.maskable_interrupt_request().known
                && !bus.maskable_interrupt_request().asserted(),
            "Reset devices reported a maskable interrupt"
        );
        static_cast<void>(bus.write8(0x0008U, 0x08U));
        static_cast<void>(bus.write8(0x0009U, 0xFFU));
        static_cast<void>(bus.write8(0x000AU, 0xFCU));
        static_cast<void>(bus.advance_cycles(3U));
        const auto output_compare_request =
            bus.maskable_interrupt_request();
        static_cast<void>(bus.write8(0x0011U, 0x04U));
        const auto output_compare_over_unknown_sci =
            bus.maskable_interrupt_request();
        passed &= expect(
            output_compare_request.asserted()
                && output_compare_request.source
                    == InterruptSource::timer_output_compare
                && output_compare_over_unknown_sci.asserted()
                && output_compare_over_unknown_sci.source
                    == InterruptSource::timer_output_compare,
            "JR-800 bus did not expose the timer output-compare request"
        );

        bus.reset_cpu_devices();
        static_cast<void>(bus.write8(0x0011U, 0x04U));
        const auto transmit_request = bus.maskable_interrupt_request();
        passed &= expect(
            transmit_request.known && transmit_request.asserted()
                && transmit_request.source == InterruptSource::serial,
            "Known TDRE did not produce the enabled SCI request"
        );

        bus.reset_cpu_devices();
        static_cast<void>(bus.write8(0x0011U, 0x18U));
        const auto initial_receive_request =
            bus.maskable_interrupt_request();
        passed &= expect(
            initial_receive_request.known
                && !initial_receive_request.asserted(),
            "Receiver requested an interrupt before a byte could arrive"
        );
        static_cast<void>(bus.advance_cycles(160U));
        const auto unknown_receive_request =
            bus.maskable_interrupt_request();
        passed &= expect(
            !unknown_receive_request.known
                && !unknown_receive_request.asserted(),
            "Unmodeled enabled receiver request was guessed"
        );

        bus.reset_cpu_devices();
        bus.set_port2_pin_state(0x08U, 0x08U);
        static_cast<void>(bus.write8(0x0011U, 0x18U));
        static_cast<void>(bus.advance_cycles(4096U));
        const auto idle_status =
            bus.read8(0x0011U, AccessKind::data_read);
        const auto idle_receive_request =
            bus.maskable_interrupt_request();
        passed &= expect(
            idle_status.succeeded() && idle_status.value == 0x38U
                && idle_receive_request.known
                && !idle_receive_request.asserted(),
            "Known idle Port 2 receive input changed SCI status"
        );
    }

    {
        Jr800Bus bus;
        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "RAM control observer attach failed"
        );
        const auto unknown = bus.read8(0x0014U, AccessKind::data_read);
        passed &= expect(
            unknown.fault == BusFault::uninitialized_read
                && !unknown.value.has_value()
                && observer.event_count == 0U,
            "Unknown standby status produced a RAM control value"
        );

        bus.set_ram_standby_power_valid(false, true);
        const auto reset_value = bus.read8(0x0014U, AccessKind::data_read);
        const auto initial_ram_write = bus.write8(0x0080U, 0x42U);
        const auto disable = bus.write8(0x0014U, 0x00U);
        passed &= expect(
            reset_value.succeeded() && reset_value.value == 0x7FU
                && initial_ram_write.succeeded()
                && disable.succeeded()
                && disable.previous_value_known
                && disable.previous_value == 0x7FU
                && observer.event_count == 3U
                && observer.events[2].kind == AccessKind::data_write
                && observer.events[2].address == 0x0014U
                && observer.events[2].value == 0x00U
                && observer.events[2].previous_value == 0x7FU
                && observer.events[2].previous_value_known,
            "RAM control reset value, write, or trace differs"
        );

        const auto disabled_read = bus.read8(0x0080U, AccessKind::data_read);
        const auto disabled_discard = bus.read8_discard(0x0080U);
        const auto disabled_inspection = bus.inspect8(0x0080U);
        const auto disabled_write = bus.write8(0x0080U, 0xA5U);
        passed &= expect(
            disabled_read.fault == BusFault::unsupported_access
                && disabled_discard.fault == BusFault::unsupported_access
                && disabled_inspection.fault == BusFault::unsupported_access
                && disabled_write.fault == BusFault::unsupported_access
                && observer.event_count == 3U,
            "Disabled internal RAM used an unmodeled external fallback"
        );

        const auto enable = bus.write8(0x0014U, 0x40U);
        const auto restored_ram = bus.read8(0x0080U, AccessKind::data_read);
        const auto enabled_control = bus.inspect8(0x0014U);
        passed &= expect(
            enable.succeeded() && enable.previous_value_known
                && enable.previous_value == 0x3FU
                && restored_ram.succeeded() && restored_ram.value == 0x42U
                && enabled_control.succeeded()
                && enabled_control.value == 0x7FU
                && observer.event_count == 5U,
            "Re-enabled internal RAM or retained data differs"
        );
    }

    {
        Jr800Bus bus;
        auto rom = nop_rom();
        rom[0U] = 0x86U;
        rom[1U] = 0x42U;
        rom[2U] = 0x97U;
        rom[3U] = 0x80U;
        passed &= expect(
            bus.load_logical_rom(rom) == Jr800MemoryStatus::ok,
            "RAM-write ROM load failed"
        );

        RecordingObserver observer;
        passed &= expect(bus.set_observer(&observer), "Bus observer attach failed");
        Cpu cpu;
        cpu.initialize(CpuProfile::hd6301v1, 0x8000U, 0x01FFU);
        passed &= expect(
            cpu.step_instruction(bus).succeeded()
                && cpu.step_instruction(bus).succeeded(),
            "RAM write program failed"
        );
        const auto read = bus.read8(0x0080U, AccessKind::data_read);
        passed &= expect(
            read.succeeded() && read.value == 0x42U,
            "Written internal RAM did not read back"
        );
        passed &= expect(
            observer.event_count == 6U
                && observer.events[4].kind == AccessKind::data_write
                && observer.events[4].address == 0x0080U
                && observer.events[4].value == 0x42U
                && !observer.events[4].previous_value_known,
            "First RAM write did not preserve unknown previous-value state"
        );

        const auto rom_write = bus.write8(0x8000U, 0xFFU);
        const auto rom_read = bus.read8(0x8000U, AccessKind::data_read);
        const auto event_count_before_inspection = observer.event_count;
        const auto inspected_rom = bus.inspect8(0x8000U);
        const auto inspected_lcd = bus.inspect8(0x0A00U);
        passed &= expect(
            rom_write.fault == BusFault::read_only_write
                && rom_read.succeeded() && rom_read.value == 0x86U
                && inspected_rom.succeeded() && inspected_rom.value == 0x86U
                && inspected_lcd.fault == BusFault::unsupported_access
                && !inspected_lcd.value.has_value()
                && observer.event_count == event_count_before_inspection,
            "ROM write or side-effect-free inspection differs"
        );
    }

    return passed ? 0 : 1;
}
