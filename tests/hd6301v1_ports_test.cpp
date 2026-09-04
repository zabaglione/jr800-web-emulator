// SPDX-License-Identifier: MIT

#include <iostream>
#include <string_view>

#include "jr800/core/hd6301v1_ports.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    jr800::core::Hd6301v1Ports ports;
    bool passed = expect(
        ports.port1_data_direction() == 0U
            && !ports.port1_data().has_value()
            && ports.port2_data_direction() == 0U
            && !ports.port2_data_latch().has_value()
            && !ports.port2_timer_output_state(true).output_enabled
            && !ports.port2_timer_output_state(true).level.has_value()
            && ports.port4_data_direction() == 0U,
        "Port registers did not begin in their documented reset states"
    );

    const auto initial_port1_read = ports.read8(0x0002U);
    const auto initial_port2_read = ports.read8(0x0003U);
    const auto unrelated_read = ports.read8(0x0004U);
    passed &= expect(
        initial_port1_read.handled && !initial_port1_read.value.has_value()
            && initial_port2_read.handled
            && !initial_port2_read.value.has_value()
            && !unrelated_read.handled
            && !unrelated_read.value.has_value(),
        "Initial pin state or unrelated read handling differs"
    );

    ports.set_port1_pin_state(0xC3U, 0x0FU);
    const auto partial_port1_read = ports.read8(0x0002U);
    passed &= expect(
        partial_port1_read.handled
            && !partial_port1_read.value.has_value(),
        "Partially known Port 1 pins produced a complete byte"
    );
    ports.set_port1_pin_state(0xC3U, 0xFFU);
    passed &= expect(
        ports.read8(0x0002U).value == 0xC3U,
        "Known Port 1 pin state was not readable"
    );
    ports.set_port2_pin_state(0x12U, 0x0FU);
    const auto partial_port2_read = ports.read8(0x0003U);
    passed &= expect(
        partial_port2_read.handled
            && !partial_port2_read.value.has_value(),
        "Partially known Port 2 pins produced a complete byte"
    );
    ports.set_port2_pin_state(0x12U, 0x1FU);
    passed &= expect(
        ports.read8(0x0003U).value == 0xD2U,
        "Known Port 2 pin state did not include the mode-6 bits"
    );

    const auto unrelated = ports.write8(0x0004U, 0xFFU);
    passed &= expect(
        !unrelated.handled
            && ports.port1_data_direction() == 0U
            && !ports.port1_data().has_value()
            && ports.port2_data_direction() == 0U
            && !ports.port2_data_latch().has_value()
            && ports.port4_data_direction() == 0U,
        "Port registers accepted an unrelated address"
    );
    const auto port1_direction_write = ports.write8(0x0000U, 0x3CU);
    passed &= expect(
        port1_direction_write.handled
            && port1_direction_write.previous_value_known
            && port1_direction_write.previous_value == 0U
            && ports.port1_data_direction() == 0x3CU,
        "Port 1 direction register lost writable bits"
    );
    const auto port2_direction_write = ports.write8(0x0001U, 0xFFU);
    const auto unknown_timer_output =
        ports.port2_timer_output_state(std::nullopt);
    const auto low_timer_output = ports.port2_timer_output_state(false);
    const auto high_timer_output = ports.port2_timer_output_state(true);
    passed &= expect(
        port2_direction_write.handled
            && port2_direction_write.previous_value_known
            && port2_direction_write.previous_value == 0U
            && ports.port2_data_direction() == 0x1FU
            && unknown_timer_output.output_enabled
            && !unknown_timer_output.level.has_value()
            && low_timer_output.output_enabled
            && low_timer_output.level == false
            && high_timer_output.output_enabled
            && high_timer_output.level == true,
        "Port 2 direction or timer-output selection differs"
    );
    const auto first_port1_write = ports.write8(0x0002U, 0x5AU);
    const auto second_port1_write = ports.write8(0x0002U, 0xA5U);
    passed &= expect(
        first_port1_write.handled
            && !first_port1_write.previous_value_known
            && second_port1_write.handled
            && second_port1_write.previous_value_known
            && second_port1_write.previous_value == 0x5AU
            && ports.port1_data() == 0xA5U
            && ports.read8(0x0002U).value == 0xC3U,
        "Port 1 data write knownness differs"
    );
    const auto first_port2_write = ports.write8(0x0003U, 0x2AU);
    const auto second_port2_write = ports.write8(0x0003U, 0xF5U);
    passed &= expect(
        first_port2_write.handled
            && !first_port2_write.previous_value_known
            && second_port2_write.handled
            && second_port2_write.previous_value_known
            && second_port2_write.previous_value == 0xCAU
            && ports.port2_data_latch() == 0x15U
            && ports.read8(0x0003U).value == 0xD2U,
        "Port 2 data latch, read-only mode bits, or pin read differs"
    );
    const auto port4_write = ports.write8(0x0005U, 0xA5U);
    passed &= expect(
        port4_write.handled
            && port4_write.previous_value_known
            && port4_write.previous_value == 0U
            && ports.port4_data_direction() == 0xA5U,
        "Port 4 direction register lost writable bits"
    );

    ports.reset();
    passed &= expect(
        ports.port1_data_direction() == 0U
            && !ports.port1_data().has_value()
            && ports.port2_data_direction() == 0U
            && !ports.port2_data_latch().has_value()
            && !ports.port2_timer_output_state(false).output_enabled
            && !ports.port2_timer_output_state(false).level.has_value()
            && ports.port4_data_direction() == 0U
            && ports.read8(0x0002U).value == 0xC3U
            && ports.read8(0x0003U).value == 0xD2U,
        "Port reset did not restore documented state"
    );

    ports.set_port1_pin_state(0x00U, 0x00U);
    ports.set_port2_pin_state(0x00U, 0x00U);
    passed &= expect(
        !ports.read8(0x0002U).value.has_value()
            && !ports.read8(0x0003U).value.has_value(),
        "Cleared port pin evidence retained a readable value"
    );
    return passed ? 0 : 1;
}
