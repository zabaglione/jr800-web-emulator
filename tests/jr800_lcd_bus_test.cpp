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

    std::array<jr800::core::BusAccessEvent, 16U> events{};
    std::size_t event_count{};
};

}  // namespace

int main() {
    using jr800::core::AccessKind;
    using jr800::core::BusFault;
    using jr800::core::Jr800Bus;
    using jr800::core::Jr800ExperimentalLcdConfiguration;
    using jr800::core::Jr800ExperimentalMachineConfiguration;
    using jr800::core::Jr800LcdIndicator;
    using jr800::core::Jr800Machine;
    using jr800::core::Jr800MemoryStatus;

    constexpr std::uint16_t control_zero = 0x0A01U;
    constexpr std::uint16_t control_one = 0x0A02U;
    constexpr std::uint16_t data_zero = 0x0B01U;

    bool passed = true;

    {
        Jr800Bus bus;
        bus.reset_cpu_devices();
        const auto read = bus.read8(control_zero, AccessKind::data_read);
        const auto write = bus.write8(control_zero, 0x39U);
        passed &= expect(
            read.fault == BusFault::unsupported_access
                && !read.value.has_value()
                && write.fault == BusFault::unsupported_access
                && !bus.inspect_lcd_controller(0U).has_value()
                && !bus.lcd_display_ram_value(0U, 0U, 0U).has_value()
                && !bus.lcd_panel_dot(0U, 0U).has_value()
                && !bus.lcd_indicator_ram_value(
                    Jr800LcdIndicator::page_1
                ).has_value()
                && !bus.lcd_substituted_data_read_count().has_value(),
            "Default JR-800 bus attached the experimental LCD"
        );
    }

    {
        Jr800Bus bus(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = Jr800ExperimentalLcdConfiguration{0xA5U},
            .memory = {},
            .calendar = {},
        });
        const auto unreset_read = bus.read8(
            control_zero,
            AccessKind::data_read
        );
        const auto unreset_inspection = bus.inspect8(control_zero);
        const auto unreset_write = bus.write8(control_zero, 0x39U);
        passed &= expect(
            unreset_read.fault == BusFault::uninitialized_read
                && unreset_inspection.fault == BusFault::uninitialized_read
                && unreset_write.fault == BusFault::uninitialized_read,
            "Experimental LCD guessed its pre-reset state"
        );

        bus.reset_cpu_devices();
        const auto reset_status = bus.inspect8(control_zero);
        const auto reset_state = bus.inspect_lcd_controller(0U);
        passed &= expect(
            reset_status.succeeded() && reset_status.value == 0x60U
                && reset_state.has_value()
                && reset_state->status.fully_known()
                && reset_state->status.value == 0x60U,
            "Experimental LCD reset attachment differs"
        );

        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "Experimental LCD observer attach failed"
        );
        const auto unsupported_select = bus.read8(
            0x0A03U,
            AccessKind::data_read
        );
        const auto data_inspection = bus.inspect8(data_zero);
        passed &= expect(
            unsupported_select.fault == BusFault::unsupported_access
                && data_inspection.fault == BusFault::unsupported_access
                && observer.event_count == 0U,
            "Experimental LCD accepted an unsupported or invasive access"
        );

        const auto address_write = bus.write8(control_zero, 0x00U);
        const auto data_write = bus.write8(data_zero, 0x5AU);
        const auto address_for_dummy = bus.write8(control_zero, 0x00U);
        const auto dummy_read = bus.read8(data_zero, AccessKind::data_read);
        const auto address_for_pipeline = bus.write8(control_zero, 0x00U);
        const auto pipelined_read = bus.read8(
            data_zero,
            AccessKind::data_read
        );
        const auto final_state = bus.inspect_lcd_controller(0U);
        passed &= expect(
            address_write.succeeded() && data_write.succeeded()
                && address_for_dummy.succeeded() && dummy_read.succeeded()
                && dummy_read.value == 0xA5U
                && address_for_pipeline.succeeded()
                && pipelined_read.succeeded()
                && pipelined_read.value == 0x5AU
                && bus.lcd_substituted_data_read_count() == 1U
                && bus.lcd_display_ram_value(0U, 0U, 0U) == 0x5AU
                && final_state.has_value()
                && final_state->status.fully_known()
                && final_state->status.value == 0x60U
                && final_state->y_address == 1U,
            "Experimental LCD access or immediate completion differs"
        );
        passed &= expect(
            observer.event_count == 6U
                && observer.events[0].kind == AccessKind::data_write
                && observer.events[0].address == control_zero
                && !observer.events[0].previous_value_known
                && observer.events[1].kind == AccessKind::data_write
                && observer.events[1].address == data_zero
                && observer.events[1].value == 0x5AU
                && observer.events[3].kind == AccessKind::data_read
                && observer.events[3].value == 0xA5U
                && observer.events[5].kind == AccessKind::data_read
                && observer.events[5].value == 0x5AU,
            "Experimental LCD trace lost ordering or knownness"
        );

        const auto controller_one_address = bus.write8(control_one, 0x00U);
        passed &= expect(
            controller_one_address.succeeded()
                && !bus.lcd_display_ram_value(1U, 0U, 0U).has_value()
                && bus.lcd_display_ram_value(0U, 0U, 0U) == 0x5AU,
            "Experimental LCD controller states were not isolated"
        );

        const auto indicator_address = bus.write8(control_zero, 0x2EU);
        const auto indicator_write = bus.write8(data_zero, 0x80U);
        passed &= expect(
            indicator_address.succeeded() && indicator_write.succeeded()
                && bus.lcd_indicator_ram_value(
                    Jr800LcdIndicator::page_1
                ) == 0x80U,
            "Experimental LCD raw indicator view differs"
        );
        bus.reset_cpu_devices();
        passed &= expect(
            !bus.lcd_display_ram_value(0U, 0U, 0U).has_value()
                && !bus.lcd_indicator_ram_value(
                    Jr800LcdIndicator::page_1
                ).has_value()
                && bus.lcd_substituted_data_read_count() == 0U
                && bus.inspect8(control_zero).value == 0x60U,
            "Experimental LCD did not reset with CPU devices"
        );
    }

    {
        Jr800Machine machine(
            Jr800ExperimentalMachineConfiguration{
                .internal_ram = {},
                .lcd = Jr800ExperimentalLcdConfiguration{0x00U},
                .memory = {},
                .calendar = {},
            }
        );
        std::vector<std::uint8_t> rom(
            jr800::core::jr800_logical_rom_size,
            0x01U
        );
        constexpr std::array<std::uint8_t, 20U> program{
            0x86U, 0x3EU,
            0xB7U, 0x0AU, 0x01U,
            0x86U, 0x39U,
            0xB7U, 0x0AU, 0x01U,
            0x86U, 0x00U,
            0xB7U, 0x0AU, 0x01U,
            0x86U, 0x01U,
            0xB7U, 0x0BU, 0x01U,
        };
        for (std::size_t index = 0U; index < program.size(); ++index) {
            rom[index] = program[index];
        }
        rom[rom.size() - 2U] = 0x80U;
        rom[rom.size() - 1U] = 0x00U;

        passed &= expect(
            machine.load_logical_rom(rom) == Jr800MemoryStatus::ok
                && machine.initialize_from_reset_entry().succeeded(),
            "Experimental LCD machine initialization failed"
        );
        bool program_succeeded = true;
        for (std::size_t instruction = 0U; instruction < 8U; ++instruction) {
            program_succeeded &= machine.execution()
                .step_instruction()
                .succeeded();
        }
        const auto controller = machine.inspect_lcd_controller(0U);
        passed &= expect(
            program_succeeded
                && machine.execution().cpu().state().pc == 0x8014U
                && controller.has_value()
                && controller->display_start_page == 0U
                && controller->y_address == 1U
                && machine.lcd_display_ram_value(0U, 0U, 0U) == 0x01U
                && machine.lcd_panel_dot(45U, 0U) == true
                && machine.lcd_panel_dot(45U, 1U) == false,
            "Owner-ROM-style LCD writes did not reach the panel view"
        );

        Jr800Machine disconnected_machine;
        passed &= expect(
            !disconnected_machine.inspect_lcd_controller(0U).has_value()
                && !disconnected_machine.lcd_panel_dot(45U, 0U).has_value(),
            "Default JR-800 machine exposed experimental LCD state"
        );
    }

    {
        Jr800Bus bus(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = Jr800ExperimentalLcdConfiguration{0xFFU},
            .memory = {},
            .calendar = {},
        });
        bus.reset_cpu_devices();
        const auto address = bus.write8(control_zero, 0x00U);
        const auto discard = bus.read8_discard(data_zero);
        const auto state = bus.inspect_lcd_controller(0U);
        passed &= expect(
            address.succeeded() && discard.succeeded()
                && bus.lcd_substituted_data_read_count() == 1U
                && state.has_value() && state->y_address == 1U,
            "Discarded LCD read did not complete or report substitution"
        );
    }

    return passed ? 0 : 1;
}
