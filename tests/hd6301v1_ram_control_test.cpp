// SPDX-License-Identifier: MIT

#include <iostream>
#include <string_view>

#include "jr800/core/hd6301v1_ram_control.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    jr800::core::Hd6301v1RamControl control;
    bool passed = expect(
        control.ram_enabled()
            && !control.standby_power_valid().has_value(),
        "RAM control did not begin at its evidence boundary"
    );

    const auto initial = control.read8(0x0014U);
    const auto unrelated_read = control.read8(0x0013U);
    passed &= expect(
        initial.handled && !initial.value.has_value()
            && !unrelated_read.handled
            && !unrelated_read.value.has_value(),
        "Unknown standby status or unrelated read handling differs"
    );

    control.set_standby_power_valid(false, true);
    passed &= expect(
        control.read8(0x0014U).value == 0x7FU,
        "Cleared standby status or reset RAM-enable value differs"
    );
    control.set_standby_power_valid(true, true);
    passed &= expect(
        control.read8(0x0014U).value == 0xFFU,
        "Set standby status or unused-bit read value differs"
    );

    const auto unrelated_write = control.write8(0x0013U, 0x00U);
    passed &= expect(
        !unrelated_write.handled
            && control.read8(0x0014U).value == 0xFFU,
        "RAM control accepted an unrelated address"
    );

    const auto disable = control.write8(0x0014U, 0x00U);
    passed &= expect(
        disable.handled && disable.previous_value_known
            && disable.previous_value == 0xFFU
            && !control.ram_enabled()
            && control.standby_power_valid() == false
            && control.read8(0x0014U).value == 0x3FU,
        "RAM control writable bits or ignored bits differ"
    );

    control.reset();
    passed &= expect(
        control.ram_enabled()
            && control.standby_power_valid() == false
            && control.read8(0x0014U).value == 0x7FU,
        "CPU reset changed standby status or failed to enable RAM"
    );

    control.set_standby_power_valid(false, false);
    const auto unknown_previous = control.write8(0x0014U, 0xC0U);
    passed &= expect(
        unknown_previous.handled
            && !unknown_previous.previous_value_known
            && unknown_previous.previous_value == 0U
            && control.ram_enabled()
            && control.standby_power_valid() == true
            && control.read8(0x0014U).value == 0xFFU,
        "RAM control write did not establish unknown standby status"
    );

    return passed ? 0 : 1;
}
