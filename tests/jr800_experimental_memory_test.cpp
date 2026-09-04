// SPDX-License-Identifier: MIT

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "jr800/core/bus.hpp"
#include "jr800/core/jr800_bus.hpp"
#include "jr800/core/jr800_machine.hpp"
#include "jr800/core/jr800_memory.hpp"

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

    std::array<jr800::core::BusAccessEvent, 8U> events{};
    std::size_t event_count{};
};

}  // namespace

int main() {
    using jr800::core::AccessKind;
    using jr800::core::BusFault;
    using jr800::core::Jr800Bus;
    using jr800::core::Jr800ExperimentalInternalRamConfiguration;
    using jr800::core::Jr800ExperimentalMachineConfiguration;
    using jr800::core::Jr800ExperimentalMemoryConfiguration;
    using jr800::core::Jr800Machine;
    using jr800::core::Jr800MemoryStatus;

    bool passed = true;

    {
        Jr800Bus bus(Jr800ExperimentalMachineConfiguration{
            .internal_ram = Jr800ExperimentalInternalRamConfiguration{0x6BU},
            .lcd = {},
            .memory = Jr800ExperimentalMemoryConfiguration{
                .standard_ram_initial_value = 0xA5U,
                .expansion_ram_initial_value = {},
            },
            .calendar = {},
        });
        const auto internal_low = bus.inspect8(0x0080U);
        const auto internal_high = bus.inspect8(0x00FFU);
        const auto standard_low = bus.read8(
            0x2000U,
            AccessKind::data_read
        );
        const auto standard_high = bus.inspect8(0x5FFFU);
        const auto expansion = bus.inspect8(0x6000U);
        passed &= expect(
            internal_low.value == 0x6BU
                && internal_high.value == 0x6BU
                && standard_low.succeeded()
                && standard_low.value == 0xA5U
                && standard_high.succeeded()
                && standard_high.value == 0xA5U
                && expansion.fault == BusFault::unsupported_access,
            "Explicit RAM experiments crossed a memory-region boundary"
        );

        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "Experimental memory observer attach failed"
        );
        const auto write = bus.write8(0x2000U, 0x12U);
        const auto read = bus.read8(0x2000U, AccessKind::data_read);
        const auto internal_write = bus.write8(0x0080U, 0x34U);
        bus.reset_cpu_devices();
        const auto retained = bus.inspect8(0x2000U);
        const auto internal_retained = bus.inspect8(0x0080U);
        passed &= expect(
            write.succeeded() && write.previous_value_known
                && write.previous_value == 0xA5U
                && read.succeeded() && read.value == 0x12U
                && retained.succeeded() && retained.value == 0x12U
                && internal_write.succeeded()
                && internal_write.previous_value_known
                && internal_write.previous_value == 0x6BU
                && internal_retained.value == 0x34U
                && observer.event_count == 3U
                && observer.events[0].kind == AccessKind::data_write
                && observer.events[0].previous_value_known
                && observer.events[0].previous_value == 0xA5U
                && observer.events[1].kind == AccessKind::data_read
                && observer.events[1].value == 0x12U
                && observer.events[2].kind == AccessKind::data_write
                && observer.events[2].address == 0x0080U
                && observer.events[2].previous_value_known
                && observer.events[2].previous_value == 0x6BU,
            "Experimental RAM lost state or trace provenance"
        );
    }

    {
        Jr800Machine machine(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = Jr800ExperimentalMemoryConfiguration{
                .standard_ram_initial_value = 0x00U,
                .expansion_ram_initial_value = 0x3CU,
            },
            .calendar = {},
        });
        std::vector<std::uint8_t> rom(
            jr800::core::jr800_logical_rom_size,
            0x01U
        );
        constexpr std::array<std::uint8_t, 12U> program{
            0xB6U, 0x60U, 0x00U,
            0xB7U, 0x20U, 0x00U,
            0xB6U, 0x20U, 0x00U,
            0xB7U, 0x7FU, 0xFFU,
        };
        for (std::size_t index = 0U; index < program.size(); ++index) {
            rom[index] = program[index];
        }
        rom[rom.size() - 2U] = 0x80U;
        rom[rom.size() - 1U] = 0x00U;

        passed &= expect(
            machine.load_logical_rom(rom) == Jr800MemoryStatus::ok
                && machine.initialize_from_reset_entry().succeeded(),
            "Experimental RAM machine initialization failed"
        );
        bool program_succeeded = true;
        for (std::size_t instruction = 0U; instruction < 4U; ++instruction) {
            program_succeeded &= machine.execution()
                .step_instruction()
                .succeeded();
        }
        const auto standard = machine.execution().inspect8(0x2000U);
        const auto expansion_low = machine.execution().inspect8(0x6000U);
        const auto expansion_high = machine.execution().inspect8(0x7FFFU);
        const auto lcd = machine.execution().inspect8(0x0A01U);
        passed &= expect(
            program_succeeded
                && machine.execution().cpu().state().pc == 0x800CU
                && standard.value == 0x3CU
                && expansion_low.value == 0x3CU
                && expansion_high.value == 0x3CU
                && lcd.fault == BusFault::unsupported_access
                && !machine.lcd_substituted_data_read_count().has_value(),
            "Configured RAM did not execute independently of the LCD"
        );
    }

    return passed ? 0 : 1;
}
