// SPDX-License-Identifier: MIT

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "jr800/core/rp5c01_register_file.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool configure_clock(
    jr800::core::Rp5c01RegisterFile& registers,
    const std::array<std::uint8_t, 0x0DU>& time,
    std::uint8_t hour_system,
    std::uint8_t leap_year_counter,
    bool running = true
) {
    registers.initialize_zero();
    bool configured = registers.write(0x0DU, 0x01U).succeeded();
    configured &= registers.write(0x0AU, hour_system).succeeded();
    configured &= registers.write(0x0BU, leap_year_counter).succeeded();
    configured &= registers.write(0x0DU, 0x00U).succeeded();
    for (std::size_t address = 0U; address < time.size(); ++address) {
        configured &= registers.write(
            static_cast<std::uint8_t>(address),
            time[address]
        ).succeeded();
    }
    configured &= registers.write(
        0x0DU,
        running ? 0x08U : 0x00U
    ).succeeded();
    return configured;
}

}  // namespace

int main() {
    using jr800::core::Rp5c01RegisterFile;
    using jr800::core::Rp5c01RegisterStatus;

    Rp5c01RegisterFile registers;
    bool passed = true;

    Rp5c01RegisterFile zero_initialized;
    zero_initialized.initialize_zero();
    bool zero_state_known = true;
    for (std::uint8_t mode = 0U; mode < 4U; ++mode) {
        zero_state_known &= zero_initialized.write(0x0DU, mode).succeeded();
        for (std::uint8_t address = 0U; address < 0x0DU; ++address) {
            const auto read = zero_initialized.read(address);
            zero_state_known &= read.succeeded() && read.value == 0U;
        }
    }
    passed &= expect(
        zero_state_known,
        "Explicit zero initialization left retained state unknown"
    );
    passed &= expect(
        zero_initialized.clock_16hz_output_enabled() == true
            && zero_initialized.clock_1hz_output_enabled() == true
            && zero_initialized.divider_16hz_signal() == false
            && zero_initialized.divider_1hz_signal() == false
            && zero_initialized.clock_16hz_gate_output() == false
            && zero_initialized.clock_1hz_gate_output() == false
            && zero_initialized.clock_hold_pending() == false
            && zero_initialized.alarm_comparator_output() == true
            && zero_initialized.alarm_comparator_gate_output() == false
            && zero_initialized.alarm_terminal_pull_low() == false,
        "Explicit zero initialization did not initialize clock controls"
    );

    const auto initial_mode = registers.read(0x0DU);
    const auto initial_bank = registers.read(0x00U);
    const auto initial_test = registers.read(0x0EU);
    const auto initial_control = registers.read(0x0FU);
    const auto invalid_read = registers.read(0x10U);
    passed &= expect(
        initial_mode.status == Rp5c01RegisterStatus::unknown_state
            && !initial_mode.value.has_value()
            && initial_bank.status == Rp5c01RegisterStatus::unknown_state
            && !initial_bank.value.has_value()
            && initial_test.succeeded() && initial_test.value == 0U
            && initial_control.succeeded() && initial_control.value == 0U
            && invalid_read.status == Rp5c01RegisterStatus::invalid_address
            && !invalid_read.value.has_value()
            && !registers.mode_register().has_value()
            && !registers.clock_16hz_output_enabled().has_value()
            && !registers.clock_1hz_output_enabled().has_value()
            && !registers.divider_16hz_signal().has_value()
            && !registers.divider_1hz_signal().has_value()
            && !registers.clock_16hz_gate_output().has_value()
            && !registers.clock_1hz_gate_output().has_value()
            && !registers.clock_hold_pending().has_value()
            && !registers.alarm_comparator_output().has_value()
            && !registers.alarm_comparator_gate_output().has_value()
            && !registers.alarm_terminal_pull_low().has_value(),
        "Initial unknown state or write-only read behavior differs"
    );

    const auto pre_mode_write = registers.write(0x00U, 0x05U);
    passed &= expect(
        pre_mode_write.status == Rp5c01RegisterStatus::unknown_state
            && !pre_mode_write.previous_value_known
            && registers.read(0x00U).status
                == Rp5c01RegisterStatus::unknown_state,
        "A bank write was accepted before the retained mode was known"
    );

    const auto first_mode_write = registers.write(0x0DU, 0xACU);
    const auto second_mode_write = registers.write(0x0DU, 0x0CU);
    passed &= expect(
        first_mode_write.succeeded()
            && !first_mode_write.previous_value_known
            && registers.mode_register() == 0x0CU
            && second_mode_write.succeeded()
            && second_mode_write.previous_value_known
            && second_mode_write.previous_value == 0x0CU
            && registers.read(0x0DU).value == 0x0CU,
        "Mode register nibble handling or previous-value tracking differs"
    );

    constexpr std::array<std::uint8_t, 0x0DU> time_masks{
        0x0FU,
        0x07U,
        0x0FU,
        0x07U,
        0x0FU,
        0x03U,
        0x07U,
        0x0FU,
        0x03U,
        0x0FU,
        0x01U,
        0x0FU,
        0x0FU,
    };
    constexpr std::array<std::uint8_t, 0x0DU> time_maximums{
        0x09U,
        0x05U,
        0x09U,
        0x05U,
        0x09U,
        0x03U,
        0x06U,
        0x09U,
        0x03U,
        0x09U,
        0x01U,
        0x09U,
        0x09U,
    };
    for (std::size_t index = 0U; index < time_masks.size(); ++index) {
        const auto address = static_cast<std::uint8_t>(index);
        const auto first = registers.write(address, time_maximums[index]);
        const auto second = registers.write(address, 0xA5U);
        const auto read = registers.read(address);
        passed &= expect(
            first.succeeded() && !first.previous_value_known
                && second.succeeded() && second.previous_value_known
                && second.previous_value == time_maximums[index]
                && read.succeeded()
                && read.value
                    == static_cast<std::uint8_t>(0x05U & time_masks[index]),
            "MODE 00 register mask or knownness differs"
        );
    }

    static_cast<void>(registers.write(0x0DU, 0x0DU));
    constexpr std::array<std::uint8_t, 0x0DU> alarm_masks{
        0x00U,
        0x00U,
        0x0FU,
        0x07U,
        0x0FU,
        0x03U,
        0x07U,
        0x0FU,
        0x03U,
        0x00U,
        0x01U,
        0x03U,
        0x00U,
    };
    constexpr std::array<std::uint8_t, 0x0DU> alarm_maximums{
        0x00U,
        0x00U,
        0x09U,
        0x05U,
        0x09U,
        0x03U,
        0x06U,
        0x09U,
        0x03U,
        0x00U,
        0x01U,
        0x03U,
        0x00U,
    };
    for (std::size_t index = 0U; index < alarm_masks.size(); ++index) {
        const auto address = static_cast<std::uint8_t>(index);
        const auto write = registers.write(address, alarm_maximums[index]);
        const auto read = registers.read(address);
        const auto unused = alarm_masks[index] == 0U;
        passed &= expect(
            write.succeeded()
                && write.previous_value_known == unused
                && read.succeeded()
                && read.value == alarm_maximums[index],
            "MODE 01 register mask, ignored write, or knownness differs"
        );
    }

    constexpr std::array<std::uint8_t, 0x0DU> ram_masks{
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
        0x0FU,
    };
    constexpr std::array mode_masks{
        time_masks,
        alarm_masks,
        ram_masks,
        ram_masks,
    };
    constexpr std::array mode_maximums{
        time_maximums,
        alarm_maximums,
        ram_masks,
        ram_masks,
    };
    for (std::size_t mode = 0U; mode < mode_masks.size(); ++mode) {
        for (std::size_t index = 0U; index < time_masks.size(); ++index) {
            for (std::uint16_t value = 0U; value <= 0xFFU; ++value) {
                Rp5c01RegisterFile candidate;
                const auto mode_value = static_cast<std::uint8_t>(mode);
                const auto address = static_cast<std::uint8_t>(index);
                static_cast<void>(candidate.write(0x0DU, mode_value));
                const auto write = candidate.write(
                    address,
                    static_cast<std::uint8_t>(value)
                );
                const auto read = candidate.read(address);
                const auto mask = mode_masks[mode][index];
                const auto expected = static_cast<std::uint8_t>(
                    value & mask
                );
                const auto valid = mask == 0U
                    || expected <= mode_maximums[mode][index];
                if (valid) {
                    passed &= expect(
                        write.succeeded()
                            && write.previous_value_known == (mask == 0U)
                            && read.succeeded() && read.value == expected,
                        "Exhaustive valid bank or input-byte result differs"
                    );
                } else {
                    passed &= expect(
                        write.status
                            == Rp5c01RegisterStatus::unsupported_operation
                            && !write.previous_value_known
                            && read.status
                                == Rp5c01RegisterStatus::unknown_state
                            && !read.value.has_value(),
                        "Exhaustive invalid BCD input was not fail-closed"
                    );
                }
            }
        }
    }

    Rp5c01RegisterFile rejected_digit;
    static_cast<void>(rejected_digit.write(0x0DU, 0x00U));
    const auto accepted_digit = rejected_digit.write(0x00U, 0x05U);
    const auto rejected_write = rejected_digit.write(0x00U, 0x0AU);
    passed &= expect(
        accepted_digit.succeeded()
            && rejected_write.status
                == Rp5c01RegisterStatus::unsupported_operation
            && rejected_digit.read(0x00U).value == 0x05U,
        "Rejected BCD digit changed an already known counter value"
    );

    static_cast<void>(registers.write(0x0DU, 0x0CU));
    passed &= expect(
        registers.read(0x00U).value == 0x05U
            && registers.read(0x0AU).value == 0x01U,
        "Switching banks changed retained MODE 00 registers"
    );

    static_cast<void>(registers.write(0x0DU, 0x0EU));
    for (std::uint8_t address = 0U; address < 0x0DU; ++address) {
        passed &= expect(
            registers.write(address, address).succeeded(),
            "MODE 10 RAM write failed"
        );
    }
    static_cast<void>(registers.write(0x0DU, 0x0FU));
    for (std::uint8_t address = 0U; address < 0x0DU; ++address) {
        passed &= expect(
            registers.write(address, static_cast<std::uint8_t>(0x0FU - address))
                .succeeded(),
            "MODE 11 RAM write failed"
        );
    }
    static_cast<void>(registers.write(0x0DU, 0x0EU));
    for (std::uint8_t address = 0U; address < 0x0DU; ++address) {
        passed &= expect(
            registers.read(address).value
                == static_cast<std::uint8_t>(address & 0x0FU),
            "MODE 10 and MODE 11 RAM blocks aliased"
        );
    }

    Rp5c01RegisterFile control;
    static_cast<void>(control.write(0x0DU, 0x00U));
    static_cast<void>(control.write(0x02U, 0x04U));
    static_cast<void>(control.write(0x0DU, 0x02U));
    static_cast<void>(control.write(0x02U, 0x0AU));
    static_cast<void>(control.write(0x0DU, 0x03U));
    static_cast<void>(control.write(0x02U, 0x0BU));
    static_cast<void>(control.write(0x0DU, 0x01U));
    for (std::uint8_t address = 0x02U; address <= 0x08U; ++address) {
        static_cast<void>(control.write(address, 0x01U));
    }
    static_cast<void>(control.write(0x0AU, 0x01U));
    static_cast<void>(control.write(0x0BU, 0x03U));
    const auto alarm_reset = control.write(0x0FU, 0x0FU);
    bool alarm_cleared = true;
    for (std::uint8_t address = 0x02U; address <= 0x08U; ++address) {
        alarm_cleared &= control.read(address).value == 0U;
    }
    const auto mode_preserved = control.mode_register() == 0x01U;
    static_cast<void>(control.write(0x0DU, 0x00U));
    const auto time_preserved = control.read(0x02U).value == 0x04U;
    static_cast<void>(control.write(0x0DU, 0x02U));
    const auto ram_10_preserved = control.read(0x02U).value == 0x0AU;
    static_cast<void>(control.write(0x0DU, 0x03U));
    const auto ram_11_preserved = control.read(0x02U).value == 0x0BU;
    static_cast<void>(control.write(0x0DU, 0x01U));
    passed &= expect(
        alarm_reset.succeeded() && alarm_reset.previous_value_known
            && alarm_reset.previous_value == 0U
            && alarm_cleared
            && control.read(0x0AU).value == 0x01U
            && control.read(0x0BU).value == 0x03U
            && mode_preserved && time_preserved
            && ram_10_preserved && ram_11_preserved
            && control.clock_16hz_output_enabled() == false
            && control.clock_1hz_output_enabled() == false
            && control.alarm_comparator_output() == true,
        "Alarm reset range or active-low clock disable differs"
    );

    static_cast<void>(control.write(0x02U, 0x05U));
    const auto divider_reset = control.write(0x0FU, 0x02U);
    passed &= expect(
        divider_reset.succeeded() && divider_reset.previous_value_known
            && divider_reset.previous_value == 0U
            && control.read(0x02U).value == 0x05U
            && control.clock_16hz_output_enabled() == true
            && control.clock_1hz_output_enabled() == true
            && control.alarm_comparator_output() == false
            && control.read(0x0FU).value == 0U,
        "Divider reset acceptance, alarm retention, or clock enable differs"
    );

    Rp5c01RegisterFile alarm_clock;
    alarm_clock.initialize_zero();
    const auto alarm_initially_open =
        alarm_clock.alarm_comparator_output();
    const auto alarm_bank = alarm_clock.write(0x0DU, 0x01U);
    const auto minute_alarm = alarm_clock.write(0x02U, 0x05U);
    const auto minute_mismatch = alarm_clock.alarm_comparator_output();
    const auto time_bank = alarm_clock.write(0x0DU, 0x00U);
    const auto minute_match_write = alarm_clock.write(0x02U, 0x05U);
    const auto minute_match = alarm_clock.alarm_comparator_output();
    const auto minute_tens_mismatch = alarm_clock.write(0x03U, 0x01U);
    const auto grouped_minute_mismatch =
        alarm_clock.alarm_comparator_output();
    const auto minute_tens_match = alarm_clock.write(0x03U, 0x00U);
    static_cast<void>(alarm_clock.write(0x0DU, 0x01U));
    const auto hour_alarm = alarm_clock.write(0x04U, 0x02U);
    const auto week_alarm = alarm_clock.write(0x06U, 0x03U);
    const auto day_alarm = alarm_clock.write(0x07U, 0x04U);
    static_cast<void>(alarm_clock.write(0x0DU, 0x00U));
    const auto hour_match = alarm_clock.write(0x04U, 0x02U);
    const auto week_match = alarm_clock.write(0x06U, 0x03U);
    const auto day_match = alarm_clock.write(0x07U, 0x04U);
    const auto all_groups_match = alarm_clock.alarm_comparator_output();
    const auto day_tens_mismatch = alarm_clock.write(0x08U, 0x01U);
    const auto grouped_day_mismatch =
        alarm_clock.alarm_comparator_output();
    passed &= expect(
        alarm_initially_open == true
            && alarm_bank.succeeded() && minute_alarm.succeeded()
            && minute_mismatch == false
            && time_bank.succeeded() && minute_match_write.succeeded()
            && minute_match == true && minute_tens_mismatch.succeeded()
            && grouped_minute_mismatch == false
            && minute_tens_match.succeeded()
            && hour_alarm.succeeded() && week_alarm.succeeded()
            && day_alarm.succeeded() && hour_match.succeeded()
            && week_match.succeeded() && day_match.succeeded()
            && all_groups_match == true && day_tens_mismatch.succeeded()
            && grouped_day_mismatch == false,
        "Alarm comparator wildcard or grouped matching differs"
    );

    Rp5c01RegisterFile unknown_alarm;
    const auto unknown_alarm_initial =
        unknown_alarm.alarm_comparator_output();
    static_cast<void>(unknown_alarm.write(0x0DU, 0x01U));
    static_cast<void>(unknown_alarm.write(0x02U, 0x05U));
    static_cast<void>(unknown_alarm.write(0x0DU, 0x00U));
    static_cast<void>(unknown_alarm.write(0x02U, 0x04U));
    const auto known_alarm_mismatch =
        unknown_alarm.alarm_comparator_output();
    static_cast<void>(unknown_alarm.write(0x02U, 0x05U));
    const auto unresolved_alarm_match =
        unknown_alarm.alarm_comparator_output();
    const auto wildcard_reset = unknown_alarm.write(0x0FU, 0x01U);
    passed &= expect(
        !unknown_alarm_initial.has_value()
            && known_alarm_mismatch == false
            && !unresolved_alarm_match.has_value()
            && wildcard_reset.succeeded()
            && unknown_alarm.alarm_comparator_output() == true,
        "Alarm comparator did not preserve tri-state knownness"
    );

    Rp5c01RegisterFile alarm_gate;
    alarm_gate.initialize_zero();
    const auto disabled_matching_gate =
        alarm_gate.alarm_comparator_gate_output();
    const auto alarm_enabled = alarm_gate.write(0x0DU, 0x05U);
    const auto enabled_matching_gate =
        alarm_gate.alarm_comparator_gate_output();
    const auto mismatching_alarm = alarm_gate.write(0x02U, 0x05U);
    const auto enabled_mismatch_gate =
        alarm_gate.alarm_comparator_gate_output();
    const auto matching_time_bank = alarm_gate.write(0x0DU, 0x04U);
    const auto matching_time = alarm_gate.write(0x02U, 0x05U);
    const auto enabled_match_gate =
        alarm_gate.alarm_comparator_gate_output();
    const auto alarm_disabled = alarm_gate.write(0x0DU, 0x00U);
    const auto disabled_match_gate =
        alarm_gate.alarm_comparator_gate_output();
    Rp5c01RegisterFile unknown_gate;
    const auto unknown_gate_disabled = unknown_gate.write(0x0DU, 0x00U);
    const auto disabled_unknown_gate =
        unknown_gate.alarm_comparator_gate_output();
    Rp5c01RegisterFile enabled_unknown_gate;
    const auto unknown_gate_enabled =
        enabled_unknown_gate.write(0x0DU, 0x04U);
    const auto enabled_unknown_output =
        enabled_unknown_gate.alarm_comparator_gate_output();
    passed &= expect(
        disabled_matching_gate == false
            && alarm_enabled.succeeded()
            && enabled_matching_gate == true
            && mismatching_alarm.succeeded()
            && enabled_mismatch_gate == false
            && matching_time_bank.succeeded()
            && matching_time.succeeded()
            && enabled_match_gate == true
            && alarm_disabled.succeeded()
            && disabled_match_gate == false
            && unknown_gate_disabled.succeeded()
            && disabled_unknown_gate == false
            && unknown_gate_enabled.succeeded()
            && !enabled_unknown_output.has_value(),
        "Alarm EN gate did not preserve three-state AND behavior"
    );

    const auto normal_test = registers.write(0x0EU, 0x10U);
    const auto unsupported_test = registers.write(0x0EU, 0x01U);
    const auto control_write = registers.write(0x0FU, 0xF0U);
    const auto invalid_write = registers.write(0x10U, 0x00U);
    passed &= expect(
        normal_test.succeeded() && normal_test.previous_value_known
            && normal_test.previous_value == 0U
            && unsupported_test.status
                == Rp5c01RegisterStatus::unsupported_operation
            && control_write.succeeded()
            && control_write.previous_value_known
            && control_write.previous_value == 0U
            && invalid_write.status == Rp5c01RegisterStatus::invalid_address
            && registers.read(0x0EU).value == 0U
            && registers.read(0x0FU).value == 0U
            && registers.clock_16hz_output_enabled() == true
            && registers.clock_1hz_output_enabled() == true
            && registers.mode_register() == 0x0EU,
        "Test/control handling changed visible register state"
    );

    Rp5c01RegisterFile unknown_clock;
    passed &= expect(
        unknown_clock.advance_oscillator_ticks(0U)
                == Rp5c01RegisterStatus::ok
            && unknown_clock.advance_oscillator_ticks(1U)
                == Rp5c01RegisterStatus::unknown_state
            && unknown_clock.advance_one_second()
                == Rp5c01RegisterStatus::unknown_state,
        "Unknown timer state advanced the clock"
    );

    Rp5c01RegisterFile stopped_clock;
    stopped_clock.initialize_zero();
    passed &= expect(
        stopped_clock.advance_oscillator_ticks(32'768U)
                == Rp5c01RegisterStatus::ok
            && stopped_clock.advance_one_second()
                == Rp5c01RegisterStatus::ok
            && stopped_clock.read(0x00U).value == 0U
            && stopped_clock.read(0x01U).value == 0U
            && stopped_clock.clock_hold_pending() == true,
        "Stopped clock changed its seconds or lost its held pulse"
    );

    Rp5c01RegisterFile divider_signals;
    divider_signals.initialize_zero();
    const auto first_16hz_half =
        divider_signals.advance_oscillator_ticks(1'023U);
    const auto before_16hz_edge = divider_signals.divider_16hz_signal();
    const auto first_16hz_edge = divider_signals.advance_oscillator_ticks(1U);
    const auto after_16hz_edge = divider_signals.divider_16hz_signal();
    const auto before_next_16hz_period =
        divider_signals.advance_oscillator_ticks(1'023U);
    const auto end_of_16hz_high_half =
        divider_signals.divider_16hz_signal();
    const auto full_16hz_period = divider_signals.advance_oscillator_ticks(1U);
    const auto next_16hz_period = divider_signals.divider_16hz_signal();
    const auto first_1hz_half =
        divider_signals.advance_oscillator_ticks(14'336U);
    const auto after_1hz_edge = divider_signals.divider_1hz_signal();
    const auto second_16hz_half =
        divider_signals.advance_oscillator_ticks(1'024U);
    const auto high_16hz_and_1hz =
        divider_signals.divider_16hz_signal() == true
            && divider_signals.divider_1hz_signal() == true;
    const auto before_second_boundary =
        divider_signals.advance_oscillator_ticks(15'359U);
    const auto end_of_1hz_high_half = divider_signals.divider_1hz_signal();
    const auto second_1hz_half = divider_signals.advance_oscillator_ticks(1U);
    const auto wrapped_signals =
        divider_signals.divider_16hz_signal() == false
            && divider_signals.divider_1hz_signal() == false;
    const auto phase_advanced = divider_signals.advance_oscillator_ticks(1U);
    const auto signal_divider_reset = divider_signals.write(0x0FU, 0x02U);
    passed &= expect(
        first_16hz_half == Rp5c01RegisterStatus::ok
            && before_16hz_edge == false
            && first_16hz_edge == Rp5c01RegisterStatus::ok
            && after_16hz_edge == true
            && before_next_16hz_period == Rp5c01RegisterStatus::ok
            && end_of_16hz_high_half == true
            && full_16hz_period == Rp5c01RegisterStatus::ok
            && next_16hz_period == false
            && first_1hz_half == Rp5c01RegisterStatus::ok
            && after_1hz_edge == true
            && second_16hz_half == Rp5c01RegisterStatus::ok
            && high_16hz_and_1hz
            && before_second_boundary == Rp5c01RegisterStatus::ok
            && end_of_1hz_high_half == true
            && second_1hz_half == Rp5c01RegisterStatus::ok
            && wrapped_signals
            && phase_advanced == Rp5c01RegisterStatus::ok
            && signal_divider_reset.succeeded()
            && divider_signals.divider_16hz_signal() == false
            && divider_signals.divider_1hz_signal() == false,
        "Divider-derived 16 Hz or 1 Hz phase differs"
    );

    Rp5c01RegisterFile clock_gates;
    clock_gates.initialize_zero();
    const auto clock_gate_16hz_edge =
        clock_gates.advance_oscillator_ticks(1'024U);
    const auto enabled_16hz_gate = clock_gates.clock_16hz_gate_output();
    const auto low_1hz_gate = clock_gates.clock_1hz_gate_output();
    const auto disable_16hz_gate = clock_gates.write(0x0FU, 0x04U);
    const auto disabled_16hz_gate = clock_gates.clock_16hz_gate_output();
    const auto clock_gate_1hz_edge =
        clock_gates.advance_oscillator_ticks(15'360U);
    const auto enabled_1hz_gate = clock_gates.clock_1hz_gate_output();
    const auto disable_1hz_gate = clock_gates.write(0x0FU, 0x08U);
    const auto clock_gate_16hz_high =
        clock_gates.advance_oscillator_ticks(1'024U);
    const auto reenabled_16hz_gate = clock_gates.clock_16hz_gate_output();
    const auto disabled_1hz_gate = clock_gates.clock_1hz_gate_output();
    Rp5c01RegisterFile disabled_unknown_clock_gates;
    const auto disable_unknown_clock_gates =
        disabled_unknown_clock_gates.write(0x0FU, 0x0CU);
    Rp5c01RegisterFile enabled_unknown_clock_gates;
    const auto enable_unknown_clock_gates =
        enabled_unknown_clock_gates.write(0x0FU, 0x00U);
    passed &= expect(
        clock_gate_16hz_edge == Rp5c01RegisterStatus::ok
            && enabled_16hz_gate == true
            && low_1hz_gate == false
            && disable_16hz_gate.succeeded()
            && disabled_16hz_gate == false
            && clock_gate_1hz_edge == Rp5c01RegisterStatus::ok
            && enabled_1hz_gate == true
            && disable_1hz_gate.succeeded()
            && clock_gate_16hz_high == Rp5c01RegisterStatus::ok
            && reenabled_16hz_gate == true
            && disabled_1hz_gate == false
            && disable_unknown_clock_gates.succeeded()
            && disabled_unknown_clock_gates.clock_16hz_gate_output() == false
            && disabled_unknown_clock_gates.clock_1hz_gate_output() == false
            && enable_unknown_clock_gates.succeeded()
            && !enabled_unknown_clock_gates.clock_16hz_gate_output().has_value()
            && !enabled_unknown_clock_gates.clock_1hz_gate_output().has_value(),
        "Active-low clock output gates did not preserve three-state behavior"
    );

    Rp5c01RegisterFile alarm_terminal_alarm_branch;
    alarm_terminal_alarm_branch.initialize_zero();
    const auto enable_terminal_alarm =
        alarm_terminal_alarm_branch.write(0x0DU, 0x04U);
    Rp5c01RegisterFile alarm_terminal_16hz_branch;
    alarm_terminal_16hz_branch.initialize_zero();
    const auto terminal_16hz_edge =
        alarm_terminal_16hz_branch.advance_oscillator_ticks(1'024U);
    Rp5c01RegisterFile alarm_terminal_1hz_branch;
    alarm_terminal_1hz_branch.initialize_zero();
    const auto terminal_1hz_edge =
        alarm_terminal_1hz_branch.advance_oscillator_ticks(16'384U);
    Rp5c01RegisterFile terminal_all_known_false;
    const auto terminal_clock_branches_disabled =
        terminal_all_known_false.write(0x0FU, 0x0CU);
    const auto terminal_alarm_branch_disabled =
        terminal_all_known_false.write(0x0DU, 0x00U);
    Rp5c01RegisterFile terminal_true_over_unknown;
    const auto terminal_alarm_reset =
        terminal_true_over_unknown.write(0x0FU, 0x01U);
    const auto terminal_unknown_alarm_enabled =
        terminal_true_over_unknown.write(0x0DU, 0x04U);
    Rp5c01RegisterFile terminal_unknown_branch;
    const auto terminal_known_clocks_disabled =
        terminal_unknown_branch.write(0x0FU, 0x0CU);
    passed &= expect(
        enable_terminal_alarm.succeeded()
            && alarm_terminal_alarm_branch.alarm_terminal_pull_low() == true
            && terminal_16hz_edge == Rp5c01RegisterStatus::ok
            && alarm_terminal_16hz_branch.alarm_terminal_pull_low() == true
            && terminal_1hz_edge == Rp5c01RegisterStatus::ok
            && alarm_terminal_1hz_branch.alarm_terminal_pull_low() == true
            && terminal_clock_branches_disabled.succeeded()
            && terminal_alarm_branch_disabled.succeeded()
            && terminal_all_known_false.alarm_terminal_pull_low() == false
            && terminal_alarm_reset.succeeded()
            && terminal_unknown_alarm_enabled.succeeded()
            && terminal_true_over_unknown.alarm_terminal_pull_low() == true
            && terminal_known_clocks_disabled.succeeded()
            && !terminal_unknown_branch.alarm_terminal_pull_low().has_value(),
        "ALARM terminal OR did not preserve three-state drive behavior"
    );

    Rp5c01RegisterFile held_clock;
    held_clock.initialize_zero();
    const auto first_held_pulse =
        held_clock.advance_oscillator_ticks(32'768U);
    const auto saturated_held_pulse =
        held_clock.advance_oscillator_ticks(32'768U);
    const auto resumed_hold = held_clock.write(0x0DU, 0x08U);
    const auto guarded_read = held_clock.read(0x00U);
    const auto guard_partial = held_clock.advance_oscillator_ticks(3U);
    const auto still_guarded_read = held_clock.read(0x00U);
    const auto guard_complete = held_clock.advance_oscillator_ticks(1U);
    const auto released_second = held_clock.read(0x00U);
    passed &= expect(
        first_held_pulse == Rp5c01RegisterStatus::ok
            && saturated_held_pulse == Rp5c01RegisterStatus::ok
            && resumed_hold.succeeded()
            && guarded_read.status
                == Rp5c01RegisterStatus::unsupported_operation
            && guard_partial == Rp5c01RegisterStatus::ok
            && still_guarded_read.status
                == Rp5c01RegisterStatus::unsupported_operation
            && guard_complete == Rp5c01RegisterStatus::ok
            && released_second.succeeded()
            && released_second.value == 1U
            && held_clock.clock_hold_pending() == false,
        "Clock Hold did not saturate, release, or guard clock reads"
    );

    Rp5c01RegisterFile adjusted_clock;
    constexpr std::array<std::uint8_t, 0x0DU> adjustment_time{
        9U, 2U,
        4U, 3U,
        2U, 1U,
        2U,
        5U, 1U,
        6U, 0U,
        4U, 2U,
    };
    const auto adjustment_configured = configure_clock(
        adjusted_clock,
        adjustment_time,
        1U,
        0U,
        false
    );
    const auto lower_half_adjusted = adjusted_clock.adjust_seconds();
    const auto lower_seconds = adjusted_clock.read(0x00U);
    const auto lower_minutes = adjusted_clock.read(0x02U);
    const auto set_upper_half = adjusted_clock.write(0x01U, 0x03U);
    const auto upper_half_adjusted = adjusted_clock.adjust_seconds();
    passed &= expect(
        adjustment_configured
            && lower_half_adjusted == Rp5c01RegisterStatus::ok
            && lower_seconds.value == 0U && lower_minutes.value == 4U
            && adjusted_clock.mode_register() == 0U
            && set_upper_half.succeeded()
            && upper_half_adjusted == Rp5c01RegisterStatus::ok
            && adjusted_clock.read(0x00U).value == 0U
            && adjusted_clock.read(0x01U).value == 0U
            && adjusted_clock.read(0x02U).value == 5U
            && adjusted_clock.read(0x03U).value == 3U
            && adjusted_clock.clock_hold_pending() == false,
        "ADJ threshold or Timer EN independence differs"
    );

    Rp5c01RegisterFile partial_adjustment;
    static_cast<void>(partial_adjustment.write(0x0DU, 0x00U));
    static_cast<void>(partial_adjustment.write(0x00U, 0x00U));
    static_cast<void>(partial_adjustment.write(0x01U, 0x03U));
    const auto rejected_adjustment = partial_adjustment.adjust_seconds();
    passed &= expect(
        rejected_adjustment == Rp5c01RegisterStatus::unknown_state
            && partial_adjustment.read(0x00U).value == 0U
            && partial_adjustment.read(0x01U).value == 3U,
        "ADJ partially changed an unknown minute carry"
    );

    Rp5c01RegisterFile held_adjustment;
    held_adjustment.initialize_zero();
    const auto adjustment_pulse =
        held_adjustment.advance_oscillator_ticks(32'768U);
    const auto pending_adjustment = held_adjustment.adjust_seconds();
    const auto adjustment_resume = held_adjustment.write(0x0DU, 0x08U);
    const auto guarded_adjustment = held_adjustment.adjust_seconds();
    const auto adjustment_guard_elapsed =
        held_adjustment.advance_oscillator_ticks(4U);
    const auto released_adjustment = held_adjustment.adjust_seconds();
    passed &= expect(
        adjustment_pulse == Rp5c01RegisterStatus::ok
            && pending_adjustment
                == Rp5c01RegisterStatus::unsupported_operation
            && adjustment_resume.succeeded()
            && guarded_adjustment
                == Rp5c01RegisterStatus::unsupported_operation
            && adjustment_guard_elapsed == Rp5c01RegisterStatus::ok
            && released_adjustment == Rp5c01RegisterStatus::ok
            && held_adjustment.read(0x00U).value == 0U,
        "ADJ accepted an unresolved Clock Hold interaction"
    );

    Rp5c01RegisterFile minute_clock;
    constexpr std::array<std::uint8_t, 0x0DU> minute_time{
        8U, 5U,
        4U, 3U,
        2U, 1U,
        2U,
        5U, 1U,
        6U, 0U,
        4U, 2U,
    };
    const auto minute_configured = configure_clock(
        minute_clock,
        minute_time,
        1U,
        0U
    );
    const auto alarm_bank_selected = minute_clock.write(0x0DU, 0x09U);
    const auto alarm_minute_units = minute_clock.write(0x02U, 0x05U);
    const auto alarm_minute_tens = minute_clock.write(0x03U, 0x03U);
    const auto alarm_before_minute =
        minute_clock.alarm_comparator_output();
    const auto second_59 = minute_clock.advance_one_second();
    const auto minute_35 = minute_clock.advance_one_second();
    const auto alarm_after_minute =
        minute_clock.alarm_comparator_output();
    const auto time_bank_selected = minute_clock.write(0x0DU, 0x08U);
    passed &= expect(
        minute_configured && alarm_bank_selected.succeeded()
            && alarm_minute_units.succeeded()
            && alarm_minute_tens.succeeded()
            && alarm_before_minute == false
            && second_59 == Rp5c01RegisterStatus::ok
            && minute_35 == Rp5c01RegisterStatus::ok
            && alarm_after_minute == true
            && time_bank_selected.succeeded()
            && minute_clock.read(0x00U).value == 0U
            && minute_clock.read(0x01U).value == 0U
            && minute_clock.read(0x02U).value == 5U
            && minute_clock.read(0x03U).value == 3U
            && minute_clock.read(0x04U).value == 2U
            && minute_clock.read(0x05U).value == 1U,
        "Second or minute decimal carry differs"
    );
    const auto subsecond_advanced =
        minute_clock.advance_oscillator_ticks(32'767U);
    const auto subsecond_unchanged = minute_clock.read(0x00U).value == 0U;
    const auto second_boundary = minute_clock.advance_oscillator_ticks(1U);
    const auto two_more_seconds =
        minute_clock.advance_oscillator_ticks(65'536U);
    passed &= expect(
        subsecond_advanced == Rp5c01RegisterStatus::ok
            && subsecond_unchanged
            && second_boundary == Rp5c01RegisterStatus::ok
            && two_more_seconds == Rp5c01RegisterStatus::ok
            && minute_clock.read(0x00U).value == 3U
            && minute_clock.read(0x01U).value == 0U,
        "32.768 kHz divider boundary or multi-second advance differs"
    );

    Rp5c01RegisterFile reset_divider_clock;
    constexpr std::array<std::uint8_t, 0x0DU> divider_time{
        0U, 0U,
        0U, 0U,
        0U, 0U,
        0U,
        1U, 0U,
        1U, 0U,
        0U, 0U,
    };
    const auto divider_configured = configure_clock(
        reset_divider_clock,
        divider_time,
        1U,
        0U
    );
    const auto divider_partial =
        reset_divider_clock.advance_oscillator_ticks(1'000U);
    const auto phase_reset_write = reset_divider_clock.write(0x0FU, 0x02U);
    const auto after_reset_partial =
        reset_divider_clock.advance_oscillator_ticks(31'768U);
    const auto reset_preserved_second =
        reset_divider_clock.read(0x00U).value == 0U;
    const auto after_reset_boundary =
        reset_divider_clock.advance_oscillator_ticks(1'000U);
    passed &= expect(
        divider_configured
            && divider_partial == Rp5c01RegisterStatus::ok
            && phase_reset_write.succeeded()
            && after_reset_partial == Rp5c01RegisterStatus::ok
            && reset_preserved_second
            && after_reset_boundary == Rp5c01RegisterStatus::ok
            && reset_divider_clock.read(0x00U).value == 1U,
        "Divider reset did not restart the 32.768 kHz phase"
    );

    Rp5c01RegisterFile atomic_divider_clock;
    static_cast<void>(atomic_divider_clock.write(0x0FU, 0x02U));
    static_cast<void>(atomic_divider_clock.write(0x0DU, 0x08U));
    static_cast<void>(atomic_divider_clock.write(0x00U, 0x08U));
    static_cast<void>(atomic_divider_clock.write(0x01U, 0x05U));
    const auto rejected_two_seconds =
        atomic_divider_clock.advance_oscillator_ticks(65'536U);
    static_cast<void>(atomic_divider_clock.write(0x02U, 0x00U));
    static_cast<void>(atomic_divider_clock.write(0x03U, 0x00U));
    const auto atomic_partial =
        atomic_divider_clock.advance_oscillator_ticks(32'767U);
    const auto atomic_second_unchanged =
        atomic_divider_clock.read(0x00U).value == 8U;
    const auto atomic_boundary =
        atomic_divider_clock.advance_oscillator_ticks(1U);
    passed &= expect(
        rejected_two_seconds == Rp5c01RegisterStatus::unknown_state
            && atomic_partial == Rp5c01RegisterStatus::ok
            && atomic_second_unchanged
            && atomic_boundary == Rp5c01RegisterStatus::ok
            && atomic_divider_clock.read(0x00U).value == 9U,
        "Failed multi-second advance changed counter or divider phase"
    );

    struct MonthBoundary {
        std::uint8_t month;
        std::uint8_t last_date;
        std::uint8_t next_month;
    };
    constexpr std::array month_boundaries{
        MonthBoundary{1U, 31U, 2U},
        MonthBoundary{2U, 28U, 3U},
        MonthBoundary{3U, 31U, 4U},
        MonthBoundary{4U, 30U, 5U},
        MonthBoundary{5U, 31U, 6U},
        MonthBoundary{6U, 30U, 7U},
        MonthBoundary{7U, 31U, 8U},
        MonthBoundary{8U, 31U, 9U},
        MonthBoundary{9U, 30U, 10U},
        MonthBoundary{10U, 31U, 11U},
        MonthBoundary{11U, 30U, 12U},
    };
    for (const auto boundary : month_boundaries) {
        std::array<std::uint8_t, 0x0DU> boundary_time{
            9U, 5U,
            9U, 5U,
            3U, 2U,
            3U,
            1U, 3U,
            1U, 0U,
            4U, 2U,
        };
        boundary_time[7U] = static_cast<std::uint8_t>(
            boundary.last_date % 10U
        );
        boundary_time[8U] = static_cast<std::uint8_t>(
            boundary.last_date / 10U
        );
        boundary_time[9U] = static_cast<std::uint8_t>(
            boundary.month % 10U
        );
        boundary_time[0x0AU] = static_cast<std::uint8_t>(
            boundary.month / 10U
        );
        boundary_time[0x0BU] = 4U;
        boundary_time[0x0CU] = 2U;

        Rp5c01RegisterFile boundary_clock;
        const auto boundary_configured = configure_clock(
            boundary_clock,
            boundary_time,
            1U,
            1U
        );
        const auto boundary_advanced =
            boundary_clock.advance_one_second();
        const auto next_month = static_cast<std::uint8_t>(
            boundary_clock.read(0x0AU).value.value_or(0U) * 10U
            + boundary_clock.read(0x09U).value.value_or(0U)
        );
        passed &= expect(
            boundary_configured
                && boundary_advanced == Rp5c01RegisterStatus::ok
                && boundary_clock.read(0x07U).value == 1U
                && boundary_clock.read(0x08U).value == 0U
                && next_month == boundary.next_month,
            "Month-length carry differs"
        );
    }

    Rp5c01RegisterFile year_clock;
    constexpr std::array<std::uint8_t, 0x0DU> year_end_time{
        9U, 5U,
        9U, 5U,
        3U, 2U,
        6U,
        1U, 3U,
        2U, 1U,
        9U, 9U,
    };
    const auto year_configured = configure_clock(
        year_clock,
        year_end_time,
        1U,
        3U
    );
    const auto year_advanced = year_clock.advance_one_second();
    const auto year_time_correct =
        year_clock.read(0x00U).value == 0U
        && year_clock.read(0x01U).value == 0U
        && year_clock.read(0x02U).value == 0U
        && year_clock.read(0x03U).value == 0U
        && year_clock.read(0x04U).value == 0U
        && year_clock.read(0x05U).value == 0U
        && year_clock.read(0x06U).value == 0U
        && year_clock.read(0x07U).value == 1U
        && year_clock.read(0x08U).value == 0U
        && year_clock.read(0x09U).value == 1U
        && year_clock.read(0x0AU).value == 0U
        && year_clock.read(0x0BU).value == 0U
        && year_clock.read(0x0CU).value == 0U;
    static_cast<void>(year_clock.write(0x0DU, 0x09U));
    const auto year_alarm_correct = year_clock.read(0x0AU).value == 1U
        && year_clock.read(0x0BU).value == 0U;
    passed &= expect(
        year_configured
            && year_advanced == Rp5c01RegisterStatus::ok
            && year_time_correct && year_alarm_correct,
        "24-hour year, week, or leap-year carry differs"
    );

    Rp5c01RegisterFile adjusted_year_clock;
    const auto adjusted_year_configured = configure_clock(
        adjusted_year_clock,
        year_end_time,
        1U,
        3U,
        false
    );
    const auto year_adjusted = adjusted_year_clock.adjust_seconds();
    passed &= expect(
        adjusted_year_configured
            && year_adjusted == Rp5c01RegisterStatus::ok
            && adjusted_year_clock.mode_register() == 0U
            && adjusted_year_clock.read(0x00U).value == 0U
            && adjusted_year_clock.read(0x01U).value == 0U
            && adjusted_year_clock.read(0x02U).value == 0U
            && adjusted_year_clock.read(0x03U).value == 0U
            && adjusted_year_clock.read(0x04U).value == 0U
            && adjusted_year_clock.read(0x05U).value == 0U
            && adjusted_year_clock.read(0x06U).value == 0U
            && adjusted_year_clock.read(0x07U).value == 1U
            && adjusted_year_clock.read(0x08U).value == 0U
            && adjusted_year_clock.read(0x09U).value == 1U
            && adjusted_year_clock.read(0x0AU).value == 0U
            && adjusted_year_clock.read(0x0BU).value == 0U
            && adjusted_year_clock.read(0x0CU).value == 0U,
        "ADJ did not reuse the atomic calendar carry"
    );

    Rp5c01RegisterFile unknown_independent_counters;
    bool unknown_counters_configured =
        unknown_independent_counters.write(0x0DU, 0x01U).succeeded();
    unknown_counters_configured &=
        unknown_independent_counters.write(0x0AU, 0x01U).succeeded();
    unknown_counters_configured &=
        unknown_independent_counters.write(0x0DU, 0x00U).succeeded();
    for (std::size_t address = 0U; address <= 0x0AU; ++address) {
        if (address == 6U) {
            continue;
        }
        unknown_counters_configured &= unknown_independent_counters.write(
            static_cast<std::uint8_t>(address),
            year_end_time[address]
        ).succeeded();
    }
    unknown_counters_configured &=
        unknown_independent_counters.write(0x0DU, 0x08U).succeeded();
    const auto unknown_counters_advanced =
        unknown_independent_counters.advance_one_second();
    const auto unknown_counters_time_correct =
        unknown_independent_counters.read(0x06U).status
            == Rp5c01RegisterStatus::unknown_state
        && unknown_independent_counters.read(0x07U).value == 1U
        && unknown_independent_counters.read(0x08U).value == 0U
        && unknown_independent_counters.read(0x09U).value == 1U
        && unknown_independent_counters.read(0x0AU).value == 0U
        && unknown_independent_counters.read(0x0BU).status
            == Rp5c01RegisterStatus::unknown_state
        && unknown_independent_counters.read(0x0CU).status
            == Rp5c01RegisterStatus::unknown_state;
    static_cast<void>(unknown_independent_counters.write(0x0DU, 0x09U));
    passed &= expect(
        unknown_counters_configured
            && unknown_counters_advanced == Rp5c01RegisterStatus::ok
            && unknown_counters_time_correct
            && unknown_independent_counters.read(0x0BU).status
                == Rp5c01RegisterStatus::unknown_state,
        "Unknown independent counters blocked a known calendar carry"
    );

    Rp5c01RegisterFile leap_clock;
    constexpr std::array<std::uint8_t, 0x0DU> leap_february_time{
        9U, 5U,
        9U, 5U,
        3U, 2U,
        2U,
        8U, 2U,
        2U, 0U,
        4U, 2U,
    };
    const auto leap_configured = configure_clock(
        leap_clock,
        leap_february_time,
        1U,
        0U
    );
    const auto leap_advanced = leap_clock.advance_one_second();
    passed &= expect(
        leap_configured
            && leap_advanced == Rp5c01RegisterStatus::ok
            && leap_clock.read(0x07U).value == 9U
            && leap_clock.read(0x08U).value == 2U
            && leap_clock.read(0x09U).value == 2U
            && leap_clock.read(0x0AU).value == 0U,
        "Leap-year February did not include day 29"
    );

    Rp5c01RegisterFile common_clock;
    const auto common_configured = configure_clock(
        common_clock,
        leap_february_time,
        1U,
        1U
    );
    const auto common_advanced = common_clock.advance_one_second();
    passed &= expect(
        common_configured
            && common_advanced == Rp5c01RegisterStatus::ok
            && common_clock.read(0x07U).value == 1U
            && common_clock.read(0x08U).value == 0U
            && common_clock.read(0x09U).value == 3U
            && common_clock.read(0x0AU).value == 0U,
        "Common-year February did not advance to March"
    );

    Rp5c01RegisterFile noon_clock;
    constexpr std::array<std::uint8_t, 0x0DU> morning_time{
        9U, 5U,
        9U, 5U,
        1U, 1U,
        4U,
        5U, 1U,
        6U, 0U,
        4U, 2U,
    };
    const auto noon_configured = configure_clock(
        noon_clock,
        morning_time,
        0U,
        0U
    );
    const auto noon_advanced = noon_clock.advance_one_second();
    passed &= expect(
        noon_configured
            && noon_advanced == Rp5c01RegisterStatus::ok
            && noon_clock.read(0x04U).value == 2U
            && noon_clock.read(0x05U).value == 3U
            && noon_clock.read(0x07U).value == 5U
            && noon_clock.read(0x08U).value == 1U,
        "12-hour AM-to-PM transition differs"
    );

    Rp5c01RegisterFile midnight_clock;
    auto evening_time = morning_time;
    evening_time[5U] = 3U;
    const auto midnight_configured = configure_clock(
        midnight_clock,
        evening_time,
        0U,
        0U
    );
    const auto midnight_advanced = midnight_clock.advance_one_second();
    passed &= expect(
        midnight_configured
            && midnight_advanced == Rp5c01RegisterStatus::ok
            && midnight_clock.read(0x04U).value == 2U
            && midnight_clock.read(0x05U).value == 1U
            && midnight_clock.read(0x06U).value == 5U
            && midnight_clock.read(0x07U).value == 6U
            && midnight_clock.read(0x08U).value == 1U,
        "12-hour PM-to-AM date transition differs"
    );

    Rp5c01RegisterFile partial_clock;
    static_cast<void>(partial_clock.write(0x0DU, 0x08U));
    static_cast<void>(partial_clock.write(0x00U, 0x09U));
    static_cast<void>(partial_clock.write(0x01U, 0x05U));
    passed &= expect(
        partial_clock.advance_one_second()
                == Rp5c01RegisterStatus::unknown_state
            && partial_clock.read(0x00U).value == 9U
            && partial_clock.read(0x01U).value == 5U,
        "Unknown minute carry partially changed the clock"
    );

    Rp5c01RegisterFile partial_calendar_clock;
    bool partial_calendar_configured =
        partial_calendar_clock.write(0x0DU, 0x01U).succeeded();
    partial_calendar_configured &=
        partial_calendar_clock.write(0x0AU, 0x01U).succeeded();
    partial_calendar_configured &=
        partial_calendar_clock.write(0x0DU, 0x00U).succeeded();
    constexpr std::array<std::uint8_t, 0x0BU> partial_calendar_time{
        9U, 5U,
        9U, 5U,
        3U, 2U,
        2U,
        5U, 1U,
        1U, 0U,
    };
    for (
        std::size_t address = 0U;
        address < partial_calendar_time.size();
        ++address
    ) {
        partial_calendar_configured &= partial_calendar_clock.write(
            static_cast<std::uint8_t>(address),
            partial_calendar_time[address]
        ).succeeded();
    }
    partial_calendar_configured &=
        partial_calendar_clock.write(0x0DU, 0x08U).succeeded();
    const auto partial_calendar_advanced =
        partial_calendar_clock.advance_one_second();
    passed &= expect(
        partial_calendar_configured
            && partial_calendar_advanced == Rp5c01RegisterStatus::ok
            && partial_calendar_clock.read(0x06U).value == 3U
            && partial_calendar_clock.read(0x07U).value == 6U
            && partial_calendar_clock.read(0x08U).value == 1U
            && partial_calendar_clock.read(0x0BU).status
                == Rp5c01RegisterStatus::unknown_state
            && partial_calendar_clock.read(0x0CU).status
                == Rp5c01RegisterStatus::unknown_state,
        "Ordinary date carry consumed unknown year or leap state"
    );

    Rp5c01RegisterFile unknown_leap_clock;
    auto unknown_leap_time = partial_calendar_time;
    unknown_leap_time[7U] = 8U;
    unknown_leap_time[8U] = 2U;
    unknown_leap_time[9U] = 2U;
    bool unknown_leap_configured =
        unknown_leap_clock.write(0x0DU, 0x01U).succeeded();
    unknown_leap_configured &=
        unknown_leap_clock.write(0x0AU, 0x01U).succeeded();
    unknown_leap_configured &=
        unknown_leap_clock.write(0x0DU, 0x00U).succeeded();
    for (
        std::size_t address = 0U;
        address < unknown_leap_time.size();
        ++address
    ) {
        unknown_leap_configured &= unknown_leap_clock.write(
            static_cast<std::uint8_t>(address),
            unknown_leap_time[address]
        ).succeeded();
    }
    unknown_leap_configured &=
        unknown_leap_clock.write(0x0DU, 0x08U).succeeded();
    const auto unknown_leap_advance =
        unknown_leap_clock.advance_one_second();
    passed &= expect(
        unknown_leap_configured
            && unknown_leap_advance
                == Rp5c01RegisterStatus::unknown_state
            && unknown_leap_clock.read(0x00U).value == 9U
            && unknown_leap_clock.read(0x01U).value == 5U
            && unknown_leap_clock.read(0x07U).value == 8U
            && unknown_leap_clock.read(0x08U).value == 2U,
        "February carry did not fail atomically on unknown leap state"
    );

    Rp5c01RegisterFile invalid_date_clock;
    auto invalid_date_time = year_end_time;
    invalid_date_time[7U] = 1U;
    invalid_date_time[8U] = 3U;
    invalid_date_time[9U] = 4U;
    invalid_date_time[0x0AU] = 0U;
    const auto invalid_date_configured = configure_clock(
        invalid_date_clock,
        invalid_date_time,
        1U,
        1U
    );
    const auto invalid_date_advance =
        invalid_date_clock.advance_one_second();
    const auto invalid_hold_stop = invalid_date_clock.write(0x0DU, 0x00U);
    const auto invalid_hold_pulse =
        invalid_date_clock.advance_oscillator_ticks(32'768U);
    const auto invalid_hold_resume = invalid_date_clock.write(0x0DU, 0x08U);
    passed &= expect(
        invalid_date_configured
            && invalid_date_advance
                == Rp5c01RegisterStatus::unsupported_operation
            && invalid_hold_stop.succeeded()
            && invalid_hold_pulse == Rp5c01RegisterStatus::ok
            && invalid_hold_resume.status
                == Rp5c01RegisterStatus::unsupported_operation
            && invalid_date_clock.mode_register() == 0U
            && invalid_date_clock.clock_hold_pending() == true
            && invalid_date_clock.read(0x00U).value == 9U
            && invalid_date_clock.read(0x01U).value == 5U
            && invalid_date_clock.read(0x02U).value == 9U
            && invalid_date_clock.read(0x03U).value == 5U
            && invalid_date_clock.read(0x07U).value == 1U
            && invalid_date_clock.read(0x08U).value == 3U,
        "Invalid calendar carry was not atomic and fail-closed"
    );

    return passed ? 0 : 1;
}
