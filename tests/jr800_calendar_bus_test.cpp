// SPDX-License-Identifier: MIT

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
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
    using jr800::core::Jr800CalendarOperationStatus;
    using jr800::core::Jr800ExperimentalCalendarAddressSource;
    using jr800::core::Jr800ExperimentalCalendarConfiguration;
    using jr800::core::Jr800ExperimentalCalendarCpuCycleRatio;
    using jr800::core::Jr800ExperimentalCalendarUpperReadBits;
    using jr800::core::Jr800ExperimentalLcdConfiguration;
    using jr800::core::Jr800ExperimentalMachineConfiguration;
    using jr800::core::Jr800ExperimentalMemoryConfiguration;
    using jr800::core::Jr800Machine;
    using jr800::core::Jr800MemoryStatus;

    bool passed = true;

    {
        Jr800Bus bus;
        const auto read = bus.read8(0x0600U, AccessKind::data_read);
        const auto write = bus.write8(0x0600U, 0x00U);
        const auto alarm_terminal = bus.calendar_alarm_terminal_state();
        passed &= expect(
            read.fault == BusFault::unsupported_access
                && write.fault == BusFault::unsupported_access
                && bus.inspect8(0x0600U).fault
                    == BusFault::unsupported_access
                && bus.advance_calendar_oscillator_ticks(1U)
                    == Jr800CalendarOperationStatus::calendar_disconnected
                && bus.adjust_calendar_seconds()
                    == Jr800CalendarOperationStatus::calendar_disconnected
                && !alarm_terminal.connected
                && !alarm_terminal.pull_low.has_value(),
            "Default JR-800 bus attached the experimental calendar"
        );
    }

    {
        auto machine = std::make_unique<Jr800Machine>();
        const auto alarm_terminal = machine->calendar_alarm_terminal_state();
        passed &= expect(
            machine->adjust_calendar_seconds()
                    == Jr800CalendarOperationStatus::calendar_disconnected
                && !alarm_terminal.connected
                && !alarm_terminal.pull_low.has_value(),
            "Default JR-800 machine attached a calendar"
        );
    }

    {
        Jr800Bus bus(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{},
        });
        RecordingObserver observer;
        const auto observer_attached = bus.set_observer(&observer);
        const auto set_29_units = bus.write8(0x0600U, 0x09U);
        const auto set_29_tens = bus.write8(0x0601U, 0x02U);
        const auto lower_adjusted = bus.adjust_calendar_seconds();
        const auto lower_seconds_units = bus.inspect8(0x0600U);
        const auto lower_seconds_tens = bus.inspect8(0x0601U);
        const auto lower_minutes = bus.inspect8(0x0602U);
        passed &= expect(
            observer_attached
                && set_29_units.succeeded() && set_29_tens.succeeded()
                && lower_adjusted == Jr800CalendarOperationStatus::ok
                && lower_seconds_units.value == 0U
                && lower_seconds_tens.value == 0U
                && lower_minutes.value == 0U
                && observer.event_count == 2U,
            "Lower-half calendar ADJ did not cross the JR-800 bus seam"
        );

        const auto set_30_units = bus.write8(0x0600U, 0x00U);
        const auto set_30_tens = bus.write8(0x0601U, 0x03U);
        const auto upper_adjusted = bus.adjust_calendar_seconds();
        const auto upper_seconds_units = bus.inspect8(0x0600U);
        const auto upper_seconds_tens = bus.inspect8(0x0601U);
        const auto upper_minutes = bus.inspect8(0x0602U);
        passed &= expect(
            set_30_units.succeeded() && set_30_tens.succeeded()
                && upper_adjusted == Jr800CalendarOperationStatus::ok
                && upper_seconds_units.value == 0U
                && upper_seconds_tens.value == 0U
                && upper_minutes.value == 1U
                && observer.event_count == 4U,
            "Upper-half calendar ADJ did not cross the JR-800 bus seam"
        );
    }

    {
        Jr800Bus bus(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{},
        });
        RecordingObserver observer;
        const auto observer_attached = bus.set_observer(&observer);
        const auto initial_terminal = bus.calendar_alarm_terminal_state();
        const auto enable_alarm = bus.write8(0x060DU, 0x04U);
        const auto alarm_terminal = bus.calendar_alarm_terminal_state();
        const auto disable_alarm = bus.write8(0x060DU, 0x00U);
        const auto clock_edge =
            bus.advance_calendar_oscillator_ticks(1'024U);
        const auto clock_terminal = bus.calendar_alarm_terminal_state();
        const auto disable_clocks = bus.write8(0x060FU, 0x0CU);
        const auto released_terminal = bus.calendar_alarm_terminal_state();
        passed &= expect(
            observer_attached
                && initial_terminal.connected
                && initial_terminal.pull_low == false
                && enable_alarm.succeeded()
                && alarm_terminal.connected && alarm_terminal.pull_low == true
                && disable_alarm.succeeded()
                && clock_edge == Jr800CalendarOperationStatus::ok
                && clock_terminal.connected && clock_terminal.pull_low == true
                && disable_clocks.succeeded()
                && released_terminal.connected
                && released_terminal.pull_low == false
                && observer.event_count == 3U,
            "Calendar ALARM terminal diagnostic did not follow device state"
        );
    }

    {
        Jr800Bus bus(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{},
        });
        const auto timer_enabled = bus.write8(0x060DU, 0x08U);
        const auto subsecond =
            bus.advance_calendar_oscillator_ticks(32'767U);
        const auto boundary = bus.advance_calendar_oscillator_ticks(1U);
        passed &= expect(
            timer_enabled.succeeded()
                && subsecond == Jr800CalendarOperationStatus::ok
                && boundary == Jr800CalendarOperationStatus::ok
                && bus.inspect8(0x0600U).value == 1U
                && bus.inspect8(0x0601U).value == 0U,
            "Calendar oscillator boundary did not cross the JR-800 bus"
        );

        constexpr std::array<std::uint8_t, 0x0DU> invalid_april_end{
            9U, 5U,
            9U, 5U,
            3U, 2U,
            0U,
            1U, 3U,
            4U, 0U,
            0U, 0U,
        };
        bool configured = bus.write8(0x060DU, 0x01U).succeeded()
            && bus.write8(0x060AU, 0x01U).succeeded()
            && bus.write8(0x060BU, 0x00U).succeeded()
            && bus.write8(0x060DU, 0x00U).succeeded();
        for (std::size_t address = 0U;
             address < invalid_april_end.size();
             ++address) {
            configured &= bus.write8(
                static_cast<std::uint16_t>(0x0600U + address),
                invalid_april_end[address]
            ).succeeded();
        }
        configured &= bus.write8(0x060DU, 0x08U).succeeded();
        const auto rejected =
            bus.advance_calendar_oscillator_ticks(32'768U);
        const auto rejected_adjustment = bus.adjust_calendar_seconds();
        passed &= expect(
            configured
                && rejected
                    == Jr800CalendarOperationStatus::unsupported_state
                && rejected_adjustment
                    == Jr800CalendarOperationStatus::unsupported_state
                && bus.inspect8(0x0600U).value == 9U
                && bus.inspect8(0x0601U).value == 5U
                && bus.inspect8(0x0607U).value == 1U
                && bus.inspect8(0x0608U).value == 3U,
            "Invalid calendar carry did not fail atomically at the bus seam"
        );
    }

    {
        Jr800Bus explicit_only(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{},
        });
        explicit_only.reset_cpu_devices();
        const auto timer_enabled = explicit_only.write8(0x060DU, 0x08U);
        const auto advanced = explicit_only.advance_cycles(1'228'800U);
        passed &= expect(
            timer_enabled.succeeded()
                && advanced == BusFault::none
                && explicit_only.inspect8(0x0600U).value == 0U
                && explicit_only.inspect8(0x0601U).value == 0U,
            "Default calendar unexpectedly followed CPU cycles"
        );
    }

    {
        Jr800Bus nominal_clock(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{
                .address_source =
                    Jr800ExperimentalCalendarAddressSource::cpu_a0_to_a3,
                .upper_read_bits =
                    Jr800ExperimentalCalendarUpperReadBits::all_zero,
                .cpu_cycle_ratio =
                    Jr800ExperimentalCalendarCpuCycleRatio::
                        e030_nominal_1_2288_mhz,
            },
        });
        nominal_clock.reset_cpu_devices();
        const auto timer_enabled = nominal_clock.write8(0x060DU, 0x08U);
        const auto below_boundary =
            nominal_clock.advance_cycles(1'228'799U);
        const auto seconds_below_boundary =
            nominal_clock.inspect8(0x0600U);
        nominal_clock.reset_cpu_devices();
        const auto boundary = nominal_clock.advance_cycles(1U);
        passed &= expect(
            timer_enabled.succeeded()
                && below_boundary == BusFault::none
                && seconds_below_boundary.value == 0U
                && nominal_clock.inspect8(0x0600U).value == 1U
                && nominal_clock.inspect8(0x0601U).value == 0U
                && boundary == BusFault::none,
            "E-030 CPU-cycle ratio did not produce one exact RTC second"
        );
    }

    {
        Jr800Bus atomic_clock(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{
                .address_source =
                    Jr800ExperimentalCalendarAddressSource::cpu_a0_to_a3,
                .upper_read_bits =
                    Jr800ExperimentalCalendarUpperReadBits::all_zero,
                .cpu_cycle_ratio =
                    Jr800ExperimentalCalendarCpuCycleRatio::
                        e030_nominal_1_2288_mhz,
            },
        });
        atomic_clock.reset_cpu_devices();
        constexpr std::array<std::uint8_t, 0x0DU> invalid_april_end{
            9U, 5U,
            9U, 5U,
            3U, 2U,
            0U,
            1U, 3U,
            4U, 0U,
            0U, 0U,
        };
        bool configured = atomic_clock.write8(0x060DU, 0x01U).succeeded()
            && atomic_clock.write8(0x060AU, 0x01U).succeeded()
            && atomic_clock.write8(0x060BU, 0x00U).succeeded()
            && atomic_clock.write8(0x060DU, 0x00U).succeeded();
        for (std::size_t address = 0U;
             address < invalid_april_end.size();
             ++address) {
            configured &= atomic_clock.write8(
                static_cast<std::uint16_t>(0x0600U + address),
                invalid_april_end[address]
            ).succeeded();
        }
        configured &= atomic_clock.write8(0x060DU, 0x08U).succeeded();
        configured &= atomic_clock.advance_calendar_oscillator_ticks(32'767U)
            == Jr800CalendarOperationStatus::ok;
        const auto primed = atomic_clock.advance_cycles(37U);
        const auto frc_before_failure = atomic_clock.inspect8(0x000AU);
        const auto rejected = atomic_clock.advance_cycles(1U);
        const auto frc_after_failure = atomic_clock.inspect8(0x000AU);
        const auto fixed_date = atomic_clock.write8(0x0607U, 0x00U);
        const auto fixed_date_tens = atomic_clock.write8(0x0608U, 0x03U);
        const auto retried = atomic_clock.advance_cycles(1U);
        passed &= expect(
            configured
                && primed == BusFault::none
                && frc_before_failure.value == 37U
                && rejected == BusFault::device_state_unsupported
                && frc_after_failure.value == 37U
                && atomic_clock.inspect8(0x000AU).value == 38U
                && fixed_date.succeeded() && fixed_date_tens.succeeded()
                && retried == BusFault::none
                && atomic_clock.inspect8(0x0600U).value == 0U
                && atomic_clock.inspect8(0x0601U).value == 0U
                && atomic_clock.inspect8(0x0607U).value == 1U
                && atomic_clock.inspect8(0x0608U).value == 0U
                && atomic_clock.inspect8(0x0609U).value == 5U,
            "Rejected CPU-clock batch partially changed devices or phase"
        );
    }

    {
        Jr800Bus invalid_ratio(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{
                .address_source =
                    Jr800ExperimentalCalendarAddressSource::cpu_a0_to_a3,
                .upper_read_bits =
                    Jr800ExperimentalCalendarUpperReadBits::all_zero,
                .cpu_cycle_ratio = static_cast<
                    Jr800ExperimentalCalendarCpuCycleRatio
                >(1U),
            },
        });
        invalid_ratio.reset_cpu_devices();
        passed &= expect(
            invalid_ratio.advance_cycles(1U)
                    == BusFault::device_state_unsupported
                && invalid_ratio.inspect8(0x0009U).value == 0U
                && invalid_ratio.inspect8(0x000AU).value == 0U,
            "Invalid calendar CPU-cycle ratio advanced another device"
        );
    }

    {
        Jr800Bus bus(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{},
        });
        const auto held =
            bus.advance_calendar_oscillator_ticks(32'768U);
        const auto resumed = bus.write8(0x060DU, 0x08U);
        const auto guarded = bus.inspect8(0x0600U);
        const auto guard_elapsed =
            bus.advance_calendar_oscillator_ticks(4U);
        const auto released = bus.inspect8(0x0600U);
        passed &= expect(
            held == Jr800CalendarOperationStatus::ok
                && resumed.succeeded()
                && guarded.fault == BusFault::unsupported_access
                && guard_elapsed == Jr800CalendarOperationStatus::ok
                && released.succeeded() && released.value == 1U,
            "Clock Hold release guard did not cross the JR-800 bus"
        );
    }

    {
        Jr800Bus bus(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{},
        });
        passed &= expect(
            bus.inspect8(0x0600U).value == 0U
                && bus.inspect8(0x060DU).value == 0U,
            "Experimental calendar did not start in explicit zero state"
        );

        RecordingObserver observer;
        passed &= expect(
            bus.set_observer(&observer),
            "Experimental calendar observer attach failed"
        );
        const auto mode = bus.write8(0x060DU, 0x02U);
        const auto data = bus.write8(0x0601U, 0xABU);
        const auto alias = bus.read8(0x0611U, AccessKind::data_read);
        passed &= expect(
            mode.succeeded() && mode.previous_value_known
                && mode.previous_value == 0U
                && data.succeeded() && data.previous_value_known
                && data.previous_value == 0U
                && alias.succeeded() && alias.value == 0x0BU
                && observer.event_count == 3U
                && observer.events[0].kind == AccessKind::data_write
                && observer.events[0].address == 0x060DU
                && observer.events[0].previous_value == 0U
                && observer.events[1].kind == AccessKind::data_write
                && observer.events[1].value == 0xABU
                && observer.events[2].kind == AccessKind::data_read
                && observer.events[2].address == 0x0611U
                && observer.events[2].value == 0x0BU,
            "Experimental calendar alias, masking, or trace differs"
        );

        static_cast<void>(bus.advance_cycles(1'000'000U));
        bus.reset_cpu_devices();
        const auto retained_mode = bus.inspect8(0x060DU);
        const auto retained_data = bus.inspect8(0x0601U);
        const auto rejected_test = bus.write8(0x060EU, 0x01U);
        passed &= expect(
            retained_mode.value == 0x02U
                && retained_data.value == 0x0BU
                && rejected_test.fault == BusFault::unsupported_access
                && observer.event_count == 3U,
            "Experimental calendar advanced, reset, or accepted test mode"
        );

        const auto discard = bus.read8_discard(0x0601U);
        passed &= expect(
            discard.succeeded() && observer.event_count == 4U
                && observer.events[3].kind == AccessKind::data_read
                && observer.events[3].value == 0x0BU,
            "Discarded calendar read lost its value or trace"
        );

        const auto control = bus.write8(0x060FU, 0x0FU);
        const auto control_read = bus.read8(
            0x060FU,
            AccessKind::data_read
        );
        passed &= expect(
            control.succeeded() && control.previous_value_known
                && control.previous_value == 0U
                && control_read.succeeded() && control_read.value == 0U
                && observer.event_count == 6U
                && observer.events[4].kind == AccessKind::data_write
                && observer.events[4].address == 0x060FU
                && observer.events[4].previous_value == 0U
                && observer.events[5].kind == AccessKind::data_read
                && observer.events[5].address == 0x060FU
                && observer.events[5].value == 0U,
            "Calendar control write-only behavior or trace differs"
        );
    }

    {
        Jr800Bus bus(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{
                .address_source = Jr800ExperimentalCalendarAddressSource::
                    cpu_a1_to_a4,
                .upper_read_bits =
                    Jr800ExperimentalCalendarUpperReadBits::all_one,
                .cpu_cycle_ratio = {},
            },
        });
        const auto write = bus.write8(0x0603U, 0x05U);
        const auto aliased_read = bus.read8(
            0x0602U,
            AccessKind::data_read
        );
        const auto rejected_digit = bus.write8(0x0602U, 0x06U);
        passed &= expect(
            write.succeeded() && write.previous_value_known
                && write.previous_value == 0xF0U
                && aliased_read.succeeded()
                && aliased_read.value == 0xF5U
                && rejected_digit.fault == BusFault::unsupported_access
                && bus.inspect8(0x0603U).value == 0xF5U,
            "Shifted calendar decode, upper bits, or BCD rejection differs"
        );
    }

    {
        Jr800Bus invalid_address(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{
                .address_source = static_cast<
                    Jr800ExperimentalCalendarAddressSource
                >(6U),
                .upper_read_bits =
                    Jr800ExperimentalCalendarUpperReadBits::all_zero,
                .cpu_cycle_ratio = {},
            },
        });
        Jr800Bus invalid_upper(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = {},
            .memory = {},
            .calendar = Jr800ExperimentalCalendarConfiguration{
                .address_source =
                    Jr800ExperimentalCalendarAddressSource::cpu_a0_to_a3,
                .upper_read_bits = static_cast<
                    Jr800ExperimentalCalendarUpperReadBits
                >(0x80U),
                .cpu_cycle_ratio = {},
            },
        });
        passed &= expect(
            invalid_address.inspect8(0x0600U).fault
                    == BusFault::unsupported_access
                && invalid_address.write8(0x0600U, 0x00U).fault
                    == BusFault::unsupported_access
                && invalid_upper.inspect8(0x0600U).fault
                    == BusFault::unsupported_access
                && invalid_upper.write8(0x0600U, 0x00U).fault
                    == BusFault::unsupported_access,
            "Invalid calendar configuration did not fail visibly"
        );
    }

    {
        Jr800Machine machine(Jr800ExperimentalMachineConfiguration{
            .internal_ram = {},
            .lcd = Jr800ExperimentalLcdConfiguration{0xEEU},
            .memory = Jr800ExperimentalMemoryConfiguration{
                .standard_ram_initial_value = 0x00U,
                .expansion_ram_initial_value = 0x00U,
            },
            .calendar = Jr800ExperimentalCalendarConfiguration{},
        });
        const auto initial_machine_alarm =
            machine.calendar_alarm_terminal_state();
        const auto initial_machine_adjustment =
            machine.adjust_calendar_seconds();
        std::vector<std::uint8_t> rom(
            jr800::core::jr800_logical_rom_size,
            0x01U
        );
        constexpr std::array<std::uint8_t, 16U> program{
            0x86U, 0x02U,
            0xB7U, 0x06U, 0x0DU,
            0x86U, 0x05U,
            0xB7U, 0x06U, 0x01U,
            0xB6U, 0x06U, 0x11U,
            0xB7U, 0x20U, 0x00U,
        };
        for (std::size_t index = 0U; index < program.size(); ++index) {
            rom[index] = program[index];
        }
        rom[rom.size() - 2U] = 0x80U;
        rom[rom.size() - 1U] = 0x00U;

        passed &= expect(
            machine.load_logical_rom(rom) == Jr800MemoryStatus::ok
                && machine.initialize_from_reset_entry().succeeded(),
            "Combined experimental machine initialization failed"
        );
        bool program_succeeded = true;
        for (std::size_t instruction = 0U; instruction < 6U; ++instruction) {
            program_succeeded &= machine.execution()
                .step_instruction()
                .succeeded();
        }
        passed &= expect(
            program_succeeded
                && machine.execution().cpu().state().pc == 0x8010U
                && machine.execution().inspect8(0x2000U).value == 0x05U
                && machine.execution().inspect8(0x0611U).value == 0x05U
                && machine.execution().inspect8(0x0A01U).value == 0x60U
                && machine.lcd_substituted_data_read_count() == 0U,
            "Combined LCD, RAM, and calendar experiment did not execute"
        );
        const auto machine_alarm_edge =
            machine.advance_calendar_oscillator_ticks(1'024U);
        const auto active_machine_alarm =
            machine.calendar_alarm_terminal_state();
        const auto machine_alarm_period =
            machine.advance_calendar_oscillator_ticks(1'024U);
        const auto released_machine_alarm =
            machine.calendar_alarm_terminal_state();
        passed &= expect(
            initial_machine_alarm.connected
                && initial_machine_alarm.pull_low == false
                && initial_machine_adjustment
                    == Jr800CalendarOperationStatus::ok
                && machine_alarm_edge == Jr800CalendarOperationStatus::ok
                && active_machine_alarm.connected
                && active_machine_alarm.pull_low == true
                && machine_alarm_period == Jr800CalendarOperationStatus::ok
                && released_machine_alarm.connected
                && released_machine_alarm.pull_low == false,
            "Machine calendar ALARM diagnostic did not follow the bus"
        );
    }

    return passed ? 0 : 1;
}
