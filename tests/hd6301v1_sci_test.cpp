// SPDX-License-Identifier: MIT

#include <iostream>
#include <string_view>

#include "jr800/core/hd6301v1_sci.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    jr800::core::Hd6301v1Sci sci;
    const auto reset_read = sci.read8(0x0011U);
    const auto unrelated_read = sci.read8(0x0012U);
    bool passed = expect(
        sci.rate_mode_bits() == 0U && sci.control_bits() == 0U
            && reset_read.handled && reset_read.value == 0x20U
            && !unrelated_read.handled && !unrelated_read.value.has_value()
            && sci.interrupt_request().known
            && !sci.interrupt_request().asserted,
        "SCI registers did not begin in reset state"
    );

    const auto unrelated = sci.write8(0x0012U, 0xFFU);
    passed &= expect(
        !unrelated.handled
            && sci.rate_mode_bits() == 0U
            && sci.control_bits() == 0U,
        "SCI accepted an unrelated register address"
    );

    const auto all_bits = sci.write8(0x0010U, 0xFFU);
    passed &= expect(
        all_bits.handled
            && all_bits.previous_value_known
            && all_bits.previous_value == 0U
            && sci.rate_mode_bits() == 0x0FU,
        "RMCR write retained non-register bits or lost control bits"
    );

    const auto ignored_bits = sci.write8(0x0010U, 0xF0U);
    passed &= expect(
        ignored_bits.handled
            && ignored_bits.previous_value_known
            && ignored_bits.previous_value == 0x0FU
            && sci.rate_mode_bits() == 0U,
        "RMCR writable mask or previous value differs"
    );

    const auto all_control_bits = sci.write8(0x0011U, 0xFFU);
    const auto initial_receive_status = sci.read8(0x0011U);
    sci.advance_cycles(159U);
    const auto last_known_receive_status = sci.read8(0x0011U);
    sci.advance_cycles(1U);
    const auto unknown_receive_status = sci.read8(0x0011U);
    passed &= expect(
        all_control_bits.handled
            && all_control_bits.previous_value_known
            && all_control_bits.previous_value == 0x20U
            && sci.control_bits() == 0x1FU
            && initial_receive_status.value == 0x3FU
            && last_known_receive_status.value == 0x3FU
            && unknown_receive_status.handled
            && !unknown_receive_status.value.has_value()
            && sci.interrupt_request().known
            && sci.interrupt_request().asserted,
        "TRCSR write changed status bits or lost control bits"
    );

    const auto status_only = sci.write8(0x0011U, 0xE0U);
    passed &= expect(
        status_only.handled
            && !status_only.previous_value_known
            && status_only.previous_value == 0U
            && sci.control_bits() == 0U
            && !sci.read8(0x0011U).value.has_value()
            && sci.interrupt_request().known
            && !sci.interrupt_request().asserted,
        "TRCSR writable mask or receive-status knownness differs"
    );

    jr800::core::Hd6301v1Sci transmit_interrupt;
    const auto transmit_enable = transmit_interrupt.write8(0x0011U, 0x04U);
    passed &= expect(
        transmit_enable.handled && transmit_enable.previous_value_known
            && transmit_enable.previous_value == 0x20U
            && transmit_interrupt.read8(0x0011U).value == 0x24U
            && transmit_interrupt.interrupt_request().known
            && transmit_interrupt.interrupt_request().asserted,
        "Known reset TDRE did not produce the enabled transmit request"
    );

    jr800::core::Hd6301v1Sci disabled_receiver_interrupt;
    static_cast<void>(
        disabled_receiver_interrupt.write8(0x0011U, 0x10U)
    );
    passed &= expect(
        disabled_receiver_interrupt.read8(0x0011U).value == 0x30U
            && disabled_receiver_interrupt.interrupt_request().known
            && !disabled_receiver_interrupt.interrupt_request().asserted,
        "Disabled receiver produced an interrupt or unknown status"
    );

    jr800::core::Hd6301v1Sci enabled_receiver_interrupt;
    static_cast<void>(
        enabled_receiver_interrupt.write8(0x0011U, 0x18U)
    );
    passed &= expect(
        enabled_receiver_interrupt.read8(0x0011U).value == 0x38U
            && enabled_receiver_interrupt.interrupt_request().known
            && !enabled_receiver_interrupt.interrupt_request().asserted,
        "Receiver status changed before a byte could arrive"
    );
    enabled_receiver_interrupt.advance_cycles(160U);
    passed &= expect(
        !enabled_receiver_interrupt.read8(0x0011U).value.has_value()
            && !enabled_receiver_interrupt.interrupt_request().known
            && !enabled_receiver_interrupt.interrupt_request().asserted,
        "Potential receiver status was guessed without serial input"
    );

    jr800::core::Hd6301v1Sci idle_receiver;
    idle_receiver.set_receive_pin_state(true, true);
    static_cast<void>(idle_receiver.write8(0x0011U, 0x18U));
    idle_receiver.advance_cycles(4096U);
    passed &= expect(
        idle_receiver.read8(0x0011U).value == 0x38U
            && idle_receiver.interrupt_request().known
            && !idle_receiver.interrupt_request().asserted,
        "Known idle receive input changed SCI status"
    );
    idle_receiver.set_receive_pin_state(false, true);
    idle_receiver.advance_cycles(159U);
    passed &= expect(
        idle_receiver.read8(0x0011U).value == 0x38U,
        "Receive status changed before the minimum input window"
    );
    idle_receiver.advance_cycles(1U);
    passed &= expect(
        !idle_receiver.read8(0x0011U).value.has_value(),
        "Active receive input did not make unmodeled status unknown"
    );
    idle_receiver.set_receive_pin_state(true, true);
    idle_receiver.reset();
    static_cast<void>(idle_receiver.write8(0x0011U, 0x18U));
    idle_receiver.advance_cycles(4096U);
    passed &= expect(
        idle_receiver.read8(0x0011U).value == 0x38U,
        "SCI reset discarded the external receive pin state"
    );

    jr800::core::Hd6301v1Sci idle_wakeup;
    idle_wakeup.set_receive_pin_state(true, true);
    static_cast<void>(idle_wakeup.write8(0x0011U, 0x09U));
    idle_wakeup.advance_cycles(159U);
    passed &= expect(
        idle_wakeup.read8(0x0011U).value == 0x29U,
        "Wake-up state changed before ten fastest receive bits"
    );
    idle_wakeup.advance_cycles(1U);
    passed &= expect(
        !idle_wakeup.read8(0x0011U).value.has_value(),
        "Unmodeled hardware wake-up clear was guessed"
    );

    jr800::core::Hd6301v1Sci known_transmit_over_unknown_receive;
    static_cast<void>(
        known_transmit_over_unknown_receive.write8(0x0011U, 0x1CU)
    );
    known_transmit_over_unknown_receive.advance_cycles(160U);
    passed &= expect(
        !known_transmit_over_unknown_receive.read8(0x0011U)
            .value.has_value()
            && known_transmit_over_unknown_receive.interrupt_request().known
            && known_transmit_over_unknown_receive.interrupt_request()
                .asserted,
        "Known transmit request did not dominate unknown receive status"
    );

    static_cast<void>(sci.write8(0x0010U, 0x05U));
    static_cast<void>(sci.write8(0x0011U, 0x15U));
    sci.reset();
    passed &= expect(
        sci.rate_mode_bits() == 0U && sci.control_bits() == 0U
            && sci.read8(0x0011U).value == 0x20U,
        "SCI reset did not restore RMCR or TRCSR state"
    );

    return passed ? 0 : 1;
}
