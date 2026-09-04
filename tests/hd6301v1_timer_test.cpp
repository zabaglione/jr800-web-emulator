// SPDX-License-Identifier: MIT

#include <iostream>
#include <string_view>

#include "jr800/core/hd6301v1_timer.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    using jr800::core::Hd6301v1TimerInterruptSource;

    jr800::core::Hd6301v1Timer timer;
    const auto initial_control_status = timer.read8(0x0008U);
    const auto initial_counter_high = timer.read8(0x0009U);
    const auto initial_counter_low = timer.read8(0x000AU);
    const auto initial_output_compare_high = timer.read8(0x000BU);
    const auto initial_output_compare_low = timer.read8(0x000CU);
    bool passed = expect(
        timer.control_bits() == 0U
            && timer.status_bits() == 0U
            && timer.status_bits_known()
            && timer.free_running_counter() == 0U
            && !timer.free_running_counter_high_write_pending()
            && !timer.input_capture().has_value()
            && !timer.output_compare_level().has_value()
            && initial_control_status.handled
            && initial_control_status.value == 0U
            && initial_counter_high.handled
            && initial_counter_high.value == 0U
            && initial_counter_low.handled
            && initial_counter_low.value == 0U
            && initial_output_compare_high.handled
            && initial_output_compare_high.value == 0xFFU
            && initial_output_compare_low.handled
            && initial_output_compare_low.value == 0xFFU,
        "Timer registers did not begin in reset state"
    );

    passed &= expect(
        !timer.write8(0x0007U, 0xFFU).handled
            && !timer.read8(0x0007U).handled
            && timer.control_bits() == 0U,
        "Timer accepted an unrelated register address"
    );
    const auto first_control_write = timer.write8(0x0008U, 0xFFU);
    passed &= expect(
        first_control_write.handled
            && first_control_write.previous_value_known
            && first_control_write.previous_value == 0U
            && timer.control_bits() == 0x1FU,
        "TCSR write changed read-only status bits or lost control bits"
    );
    const auto second_control_write = timer.write8(0x0008U, 0xA0U);
    passed &= expect(
        second_control_write.handled
            && second_control_write.previous_value_known
            && second_control_write.previous_value == 0x1FU
            && timer.control_bits() == 0U,
        "TCSR control mask differs"
    );
    timer.advance_cycles(1U);
    passed &= expect(
        !timer.status_bits_known()
            && !timer.inspect8(0x0008U).value.has_value()
            && !timer.inspect8(0x000DU).value.has_value(),
        "Unknown capture input produced known timer status"
    );
    timer.set_input_capture_pin_state(false, true);
    timer.reset();

    passed &= expect(
        !timer.write8(0x000AU, 0xF3U).handled
            && timer.free_running_counter() == 0U
            && !timer.free_running_counter_high_write_pending(),
        "FRC low-byte write without a preceding high byte was guessed"
    );
    const auto counter_high_write = timer.write8(0x0009U, 0x5AU);
    passed &= expect(
        counter_high_write.handled
            && counter_high_write.previous_value_known
            && counter_high_write.previous_value == 0U
            && timer.free_running_counter() == 0xFFF8U
            && timer.free_running_counter_high_write_pending(),
        "FRC high-byte write did not apply the documented preset"
    );
    const auto counter_low_write = timer.write8(0x000AU, 0xF3U);
    passed &= expect(
        counter_low_write.handled
            && counter_low_write.previous_value_known
            && counter_low_write.previous_value == 0xF8U
            && timer.free_running_counter() == 0x5AF3U
            && !timer.free_running_counter_high_write_pending(),
        "FRC low-byte write did not commit the staged 16-bit value"
    );
    passed &= expect(
        !timer.write8(0x000AU, 0x42U).handled
            && timer.free_running_counter() == 0x5AF3U,
        "FRC accepted an unpaired second low-byte write"
    );

    timer.advance_cycles(0xA50DU);
    passed &= expect(
        timer.free_running_counter() == 0U
            && timer.read8(0x0009U).value == 0U
            && timer.read8(0x000AU).value == 0U,
        "FRC did not wrap after the exact E-cycle count"
    );
    timer.advance_cycles(0x1234U);
    passed &= expect(
        timer.free_running_counter() == 0x1234U
            && timer.read8(0x0009U).value == 0x12U
            && timer.read8(0x000AU).value == 0x34U,
        "FRC byte reads did not expose the advanced counter"
    );

    const auto output_compare_high_write = timer.write8(0x000BU, 0x12U);
    const auto output_compare_after_high = timer.read8(0x000BU);
    const auto output_compare_unchanged_low = timer.read8(0x000CU);
    passed &= expect(
        output_compare_high_write.handled
            && output_compare_high_write.previous_value_known
            && output_compare_high_write.previous_value == 0xFFU
            && output_compare_after_high.value == 0x12U
            && output_compare_unchanged_low.value == 0xFFU,
        "OCR high-byte write or previous value differs"
    );
    const auto output_compare_low_write = timer.write8(0x000CU, 0x34U);
    const auto output_compare_final_high = timer.read8(0x000BU);
    const auto output_compare_final_low = timer.read8(0x000CU);
    passed &= expect(
        output_compare_low_write.handled
            && output_compare_low_write.previous_value_known
            && output_compare_low_write.previous_value == 0xFFU
            && output_compare_final_high.value == 0x12U
            && output_compare_final_low.value == 0x34U,
        "OCR low-byte write or retained high byte differs"
    );

    timer.reset();
    static_cast<void>(timer.write8(0x0009U, 0xFFU));
    static_cast<void>(timer.write8(0x000AU, 0xFCU));
    timer.advance_cycles(2U);
    passed &= expect(
        timer.free_running_counter() == 0xFFFEU
            && timer.status_bits() == 0U,
        "Timer comparison was not inhibited after an FRC write"
    );
    timer.advance_cycles(1U);
    passed &= expect(
        timer.free_running_counter() == 0xFFFFU
            && timer.status_bits() == 0x40U
            && timer.output_compare_level() == false,
        "FRC-to-OCR match did not set OCF or transfer low OLVL"
    );
    timer.advance_cycles(1U);
    const auto inspected_flags = timer.inspect8(0x0008U);
    static_cast<void>(timer.read8(0x0009U));
    passed &= expect(
        timer.free_running_counter() == 0U
            && timer.status_bits() == 0x60U
            && inspected_flags.value == 0x60U,
        "FRC wrap or side-effect-free TCSR inspection differs"
    );
    const auto read_flags = timer.read8(0x0008U);
    static_cast<void>(timer.read8(0x000AU));
    passed &= expect(
        read_flags.value == 0x60U
            && timer.status_bits() == 0x60U,
        "TCSR read or non-clearing FRC-low read differs"
    );
    static_cast<void>(timer.read8(0x0009U));
    passed &= expect(
        timer.status_bits() == 0x40U,
        "TOF clear sequence did not require TCSR then FRC-high reads"
    );
    static_cast<void>(timer.write8(0x000CU, 0xFFU));
    passed &= expect(
        timer.status_bits() == 0U
            && timer.output_compare_level() == false,
        "OCF clear sequence changed the retained output level"
    );

    timer.reset();
    static_cast<void>(timer.write8(0x0008U, 0x01U));
    static_cast<void>(timer.write8(0x0009U, 0xFFU));
    static_cast<void>(timer.write8(0x000AU, 0xFCU));
    timer.advance_cycles(2U);
    passed &= expect(
        !timer.output_compare_level().has_value(),
        "Comparison-inhibit interval invented an output level"
    );
    timer.advance_cycles(1U);
    passed &= expect(
        timer.output_compare_level() == true,
        "Output compare did not transfer high OLVL"
    );
    static_cast<void>(timer.write8(0x0008U, 0x00U));
    timer.advance_cycles(1U);
    passed &= expect(
        timer.output_compare_level() == true,
        "TCSR write changed output before another compare match"
    );
    static_cast<void>(timer.write8(0x000BU, 0x00U));
    static_cast<void>(timer.write8(0x000CU, 0x03U));
    timer.advance_cycles(3U);
    passed &= expect(
        timer.output_compare_level() == false,
        "Following compare match did not transfer the new low OLVL"
    );

    timer.reset();
    timer.set_input_capture_pin_state(false, true);
    static_cast<void>(timer.write8(0x0008U, 0x02U));
    timer.advance_cycles(0x1234U);
    timer.set_input_capture_enabled(false);
    timer.set_input_capture_pin_state(true, true);
    timer.set_input_capture_pin_state(false, true);
    timer.set_input_capture_enabled(true);
    timer.set_input_capture_pin_state(true, true);
    const auto captured_high = timer.inspect8(0x000DU);
    const auto captured_low = timer.inspect8(0x000EU);
    static_cast<void>(timer.read8(0x000DU));
    passed &= expect(
        timer.input_capture() == 0x1234U
            && timer.status_bits() == 0x80U
            && captured_high.value == 0x12U
            && captured_low.value == 0x34U,
        "Qualified input edge did not capture the FRC and set ICF"
    );
    static_cast<void>(timer.read8(0x0008U));
    static_cast<void>(timer.read8(0x000EU));
    passed &= expect(
        timer.status_bits() == 0x80U,
        "ICR-low read cleared ICF"
    );
    static_cast<void>(timer.read8(0x000DU));
    passed &= expect(
        timer.status_bits() == 0U,
        "ICF clear sequence did not require TCSR then ICR-high reads"
    );

    timer.reset();
    passed &= expect(
        timer.interrupt_request().known
            && !timer.interrupt_request().asserted(),
        "Reset timer reported an interrupt request"
    );
    static_cast<void>(timer.write8(0x0008U, 0x0CU));
    static_cast<void>(timer.write8(0x0009U, 0xFFU));
    static_cast<void>(timer.write8(0x000AU, 0xFCU));
    timer.advance_cycles(3U);
    const auto output_compare_request = timer.interrupt_request();
    timer.advance_cycles(1U);
    const auto output_compare_overflow_request = timer.interrupt_request();
    passed &= expect(
        output_compare_request.asserted()
            && output_compare_request.source
                == Hd6301v1TimerInterruptSource::output_compare
            && output_compare_overflow_request.asserted()
            && output_compare_overflow_request.source
                == Hd6301v1TimerInterruptSource::output_compare,
        "Timer interrupt enable, assertion, or priority differs"
    );
    static_cast<void>(timer.read8(0x0008U));
    static_cast<void>(timer.write8(0x000CU, 0xFFU));
    const auto overflow_request = timer.interrupt_request();
    passed &= expect(
        overflow_request.asserted()
            && overflow_request.source
                == Hd6301v1TimerInterruptSource::overflow,
        "Cleared output compare did not expose pending overflow"
    );

    timer.reset();
    timer.set_input_capture_pin_state(false, true);
    static_cast<void>(timer.write8(0x0008U, 0x1AU));
    static_cast<void>(timer.write8(0x0009U, 0xFFU));
    static_cast<void>(timer.write8(0x000AU, 0xFCU));
    timer.advance_cycles(3U);
    timer.set_input_capture_pin_state(true, true);
    const auto capture_request = timer.interrupt_request();
    passed &= expect(
        capture_request.asserted()
            && capture_request.source
                == Hd6301v1TimerInterruptSource::input_capture,
        "Input capture did not take timer interrupt priority"
    );

    timer.reset();
    static_cast<void>(timer.write8(0x0008U, 0x18U));
    static_cast<void>(timer.write8(0x0009U, 0xFFU));
    static_cast<void>(timer.write8(0x000AU, 0xFCU));
    timer.set_input_capture_pin_state(false, false);
    timer.advance_cycles(3U);
    const auto unknown_capture_request = timer.interrupt_request();
    static_cast<void>(timer.write8(0x0008U, 0x08U));
    const auto disabled_capture_request = timer.interrupt_request();
    static_cast<void>(timer.write8(0x0008U, 0x00U));
    const auto disabled_interrupts_request = timer.interrupt_request();
    passed &= expect(
        !unknown_capture_request.known
            && !unknown_capture_request.asserted()
            && disabled_capture_request.asserted()
            && disabled_capture_request.source
                == Hd6301v1TimerInterruptSource::output_compare
            && disabled_interrupts_request.known
            && !disabled_interrupts_request.asserted(),
        "Unknown higher-priority capture request was guessed or bypassed"
    );
    timer.set_input_capture_pin_state(false, true);

    static_cast<void>(timer.write8(0x0008U, 0x15U));
    static_cast<void>(timer.write8(0x0009U, 0xA5U));
    timer.reset();
    timer.advance_cycles(1U);
    const auto reset_capture = timer.read8(0x000DU);
    passed &= expect(
        timer.control_bits() == 0U
            && timer.status_bits() == 0U
            && timer.status_bits_known()
            && timer.free_running_counter() == 1U
            && !timer.free_running_counter_high_write_pending()
            && !timer.input_capture().has_value()
            && !timer.output_compare_level().has_value()
            && !timer.write8(0x000AU, 0x42U).handled
            && timer.read8(0x0009U).value == 0U
            && timer.read8(0x000AU).value == 1U
            && timer.read8(0x000BU).value == 0xFFU
            && timer.read8(0x000CU).value == 0xFFU
            && reset_capture.handled
            && !reset_capture.value.has_value(),
        "Timer reset did not restore counter, compare, capture, or flags"
    );
    return passed ? 0 : 1;
}
