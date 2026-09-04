// SPDX-License-Identifier: MIT

#include <iostream>
#include <string_view>

#include "jr800/core/hd44102.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    using jr800::core::Hd44102OperationStatus;

    jr800::core::Hd44102 controller;
    bool passed = true;

    const auto initial = controller.read_status();
    passed &= expect(
        initial.value == 0x00U
            && initial.known_mask == 0x0FU
            && !initial.fully_known()
            && controller.write_control(0x3AU)
                == Hd44102OperationStatus::unknown_state
            && controller.write_display_data(0xA5U)
                == Hd44102OperationStatus::unknown_state
            && controller.read_display_data().status
                == Hd44102OperationStatus::unknown_state
            && controller.complete_busy_period()
                == Hd44102OperationStatus::unknown_state
            && !controller.display_ram_value(0U, 0U).has_value()
            && !controller.display_ram_value(4U, 0U).has_value()
            && !controller.display_ram_value(0U, 50U).has_value()
            && !controller.display_dot_1_32(0U, 0U).has_value()
            && !controller.display_dot_1_32(50U, 0U).has_value()
            && !controller.display_dot_1_32(0U, 32U).has_value(),
        "Power-on state was guessed without an applied reset"
    );

    controller.set_reset_line(false);
    const auto unreset = controller.read_status();
    passed &= expect(
        unreset.value == 0x00U
            && unreset.known_mask == 0x1FU
            && !unreset.fully_known(),
        "Deasserting an unobserved reset invented retained state"
    );

    controller.set_reset_line(true);
    const auto asserted = controller.read_status();
    passed &= expect(
        asserted.value == 0xF0U
            && asserted.known_mask == 0xFFU
            && asserted.fully_known()
            && controller.write_control(0x3AU)
                == Hd44102OperationStatus::busy
            && controller.write_display_data(0xA5U)
                == Hd44102OperationStatus::busy
            && controller.read_display_data().status
                == Hd44102OperationStatus::busy
            && controller.display_dot_1_32(0U, 0U) == false
            && controller.complete_busy_period()
                == Hd44102OperationStatus::reset_asserted,
        "Asserted reset did not report busy, up, off, and reset"
    );

    controller.set_reset_line(false);
    const auto released = controller.read_status();
    passed &= expect(
        released.value == 0x60U
            && released.known_mask == 0xFFU
            && released.fully_known()
            && !controller.display_start_page().has_value()
            && !controller.x_address().has_value()
            && !controller.y_address().has_value()
            && controller.write_display_data(0xA5U)
                == Hd44102OperationStatus::unknown_state
            && controller.read_display_data().status
                == Hd44102OperationStatus::unknown_state
            && controller.display_dot_1_32(0U, 0U) == false,
        "Released reset did not retain up/off while clearing busy/reset"
    );

    passed &= expect(
        controller.write_control(0x32U)
                == Hd44102OperationStatus::unsupported_instruction
            && controller.read_status().value == 0x60U,
        "An unsupported control instruction changed controller state"
    );

    passed &= expect(
        controller.write_control(0x3AU) == Hd44102OperationStatus::ok,
        "Count-down instruction was not accepted"
    );
    const auto count_down_busy = controller.read_status();
    bool all_busy_writes_rejected = true;
    for (std::uint16_t value = 0U; value <= 0xFFU; ++value) {
        all_busy_writes_rejected &= controller.write_control(
            static_cast<std::uint8_t>(value)
        ) == Hd44102OperationStatus::busy;
    }
    passed &= expect(
        count_down_busy.value == 0xA0U
            && count_down_busy.known_mask == 0xBFU
            && !count_down_busy.fully_known()
            && all_busy_writes_rejected,
        "Busy count-down state or busy rejection differs"
    );
    passed &= expect(
        controller.complete_busy_period() == Hd44102OperationStatus::ok,
        "Count-down busy period did not complete"
    );
    const auto count_down = controller.read_status();
    passed &= expect(
        count_down.value == 0x20U
            && count_down.known_mask == 0xFFU
            && count_down.fully_known()
            && controller.complete_busy_period()
                == Hd44102OperationStatus::no_pending_instruction,
        "Count-down completion did not clear busy and direction"
    );

    passed &= expect(
        controller.write_control(0x3BU) == Hd44102OperationStatus::ok,
        "Count-up instruction was not accepted"
    );
    controller.set_reset_line(true);
    passed &= expect(
        controller.read_status().value == 0xF0U
            && controller.complete_busy_period()
                == Hd44102OperationStatus::reset_asserted,
        "Reset did not discard a pending direction instruction"
    );
    controller.set_reset_line(false);
    passed &= expect(
        controller.read_status().value == 0x60U
            && controller.complete_busy_period()
                == Hd44102OperationStatus::no_pending_instruction,
        "Reset release retained a discarded direction instruction"
    );

    passed &= expect(
        controller.write_control(0x39U) == Hd44102OperationStatus::ok
            && controller.read_status().value == 0xC0U
            && controller.read_status().known_mask == 0xDFU
            && !controller.display_dot_1_32(0U, 0U).has_value()
            && controller.write_display_data(0xA5U)
                == Hd44102OperationStatus::busy
            && controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && controller.read_status().value == 0x40U
            && controller.read_status().known_mask == 0xFFU
            && !controller.display_dot_1_32(0U, 0U).has_value(),
        "Display-on instruction did not commit through busy"
    );
    passed &= expect(
        controller.write_control(0x38U) == Hd44102OperationStatus::ok
            && controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && controller.read_status().value == 0x60U
            && controller.display_dot_1_32(0U, 0U) == false,
        "Display-off instruction did not restore the status bit"
    );
    passed &= expect(
        controller.write_control(0x39U) == Hd44102OperationStatus::ok,
        "Reset-cancellation display-on instruction was not accepted"
    );
    controller.set_reset_line(true);
    controller.set_reset_line(false);
    passed &= expect(
        controller.read_status().value == 0x60U
            && controller.complete_busy_period()
                == Hd44102OperationStatus::no_pending_instruction,
        "Reset did not cancel display-on and establish display-off"
    );

    for (std::uint16_t value = 0U; value <= 0xFFU; ++value) {
        jr800::core::Hd44102 candidate;
        candidate.set_reset_line(true);
        candidate.set_reset_line(false);
        const auto instruction = static_cast<std::uint8_t>(value);
        const auto result = candidate.write_control(instruction);
        const auto direction = instruction == 0x3AU || instruction == 0x3BU;
        const auto display_on_off = instruction == 0x38U
            || instruction == 0x39U;
        const auto start_page = (instruction & 0x3FU) == 0x3EU;
        const auto address = (instruction & 0x3FU) <= 49U;
        const auto supported = direction || display_on_off
            || start_page || address;
        passed &= expect(
            result == (supported
                ? Hd44102OperationStatus::ok
                : Hd44102OperationStatus::unsupported_instruction),
            "Control instruction acceptance set differs"
        );
        if (supported) {
            const auto busy_status = candidate.read_status();
            const auto expected_busy_value = direction
                ? 0xA0U
                : (display_on_off ? 0xC0U : 0xE0U);
            const auto expected_busy_mask = direction
                ? 0xBFU
                : (display_on_off ? 0xDFU : 0xFFU);
            const auto expected_completed_status = instruction == 0x3AU
                ? 0x20U
                : (instruction == 0x39U ? 0x40U : 0x60U);
            passed &= expect(
                busy_status.value == expected_busy_value
                    && busy_status.known_mask == expected_busy_mask
                    && !candidate.display_start_page().has_value()
                    && !candidate.x_address().has_value()
                    && !candidate.y_address().has_value()
                    && candidate.complete_busy_period()
                        == Hd44102OperationStatus::ok
                    && candidate.read_status().value
                        == expected_completed_status
                    && candidate.display_start_page()
                        == (start_page
                            ? std::optional<std::uint8_t>{
                                static_cast<std::uint8_t>(instruction >> 6U)
                            }
                            : std::nullopt)
                    && candidate.x_address()
                        == (address
                            ? std::optional<std::uint8_t>{
                                static_cast<std::uint8_t>(instruction >> 6U)
                            }
                            : std::nullopt)
                    && candidate.y_address()
                        == (address
                            ? std::optional<std::uint8_t>{
                                static_cast<std::uint8_t>(instruction & 0x3FU)
                            }
                            : std::nullopt),
                "Supported control instruction did not complete exactly"
            );
        } else {
            passed &= expect(
                candidate.read_status().value == 0x60U
                    && candidate.complete_busy_period()
                        == Hd44102OperationStatus::no_pending_instruction,
                "Unsupported control instruction started an operation"
            );
        }
    }

    controller.set_reset_line(true);
    controller.set_reset_line(false);
    passed &= expect(
        controller.write_control(0xFEU) == Hd44102OperationStatus::ok
            && controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && controller.display_start_page() == 3U,
        "Display start page 3 did not commit"
    );
    controller.set_reset_line(true);
    passed &= expect(
        !controller.display_start_page().has_value(),
        "Reset invented retention for the display start page"
    );

    controller.set_reset_line(false);
    passed &= expect(
        controller.write_control(0x3AU) == Hd44102OperationStatus::ok
            && controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && controller.write_control(0xBEU)
                == Hd44102OperationStatus::ok
            && controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && controller.read_status().value == 0x20U
            && controller.display_start_page() == 2U,
        "Address preconditions did not commit"
    );
    passed &= expect(
        controller.write_control(0xF1U) == Hd44102OperationStatus::ok
            && controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && controller.x_address() == 3U
            && controller.y_address() == 49U
            && controller.read_status().value == 0x20U
            && controller.display_start_page() == 2U,
        "Maximum X/Y address did not commit independently"
    );
    passed &= expect(
        controller.write_control(0x00U) == Hd44102OperationStatus::ok
            && !controller.x_address().has_value()
            && !controller.y_address().has_value()
            && controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && controller.x_address() == 0U
            && controller.y_address() == 0U,
        "X/Y address replacement or pending knownness differs"
    );
    controller.set_reset_line(true);
    passed &= expect(
        !controller.x_address().has_value()
            && !controller.y_address().has_value()
            && !controller.display_ram_value(0U, 0U).has_value(),
        "Reset invented X/Y address values"
    );

    jr800::core::Hd44102 display_data_controller;
    display_data_controller.set_reset_line(true);
    display_data_controller.set_reset_line(false);
    bool filled_all_cells = true;
    for (std::uint8_t x = 0U; x < 4U; ++x) {
        const auto address = static_cast<std::uint8_t>(x << 6U);
        filled_all_cells &= display_data_controller.write_control(address)
            == Hd44102OperationStatus::ok;
        filled_all_cells &= display_data_controller.complete_busy_period()
            == Hd44102OperationStatus::ok;
        for (std::uint8_t y = 0U; y < 50U; ++y) {
            const auto value = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(x) * 50U + y
            );
            filled_all_cells &= !display_data_controller
                .display_ram_value(x, y)
                .has_value();
            filled_all_cells &= display_data_controller
                .write_display_data(value) == Hd44102OperationStatus::ok;
            filled_all_cells &= display_data_controller.read_status().value
                == 0xE0U;
            filled_all_cells &= display_data_controller
                .read_status()
                .known_mask == 0xFFU;
            filled_all_cells &= display_data_controller.x_address() == x;
            filled_all_cells &= !display_data_controller
                .y_address()
                .has_value();
            filled_all_cells &= !display_data_controller
                .display_ram_value(x, y)
                .has_value();
            filled_all_cells &= display_data_controller
                .complete_busy_period() == Hd44102OperationStatus::ok;
            filled_all_cells &= display_data_controller
                .display_ram_value(x, y) == value;
            filled_all_cells &= display_data_controller.y_address()
                == static_cast<std::uint8_t>(y == 49U ? 0U : y + 1U);
        }
    }
    for (std::uint8_t x = 0U; x < 4U; ++x) {
        for (std::uint8_t y = 0U; y < 50U; ++y) {
            const auto value = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(x) * 50U + y
            );
            filled_all_cells &= display_data_controller
                .display_ram_value(x, y) == value;
        }
    }
    passed &= expect(
        filled_all_cells
            && display_data_controller.read_status().value == 0x60U
            && !display_data_controller.display_start_page().has_value(),
        "Count-up display writes did not fill exactly 4 by 50 bytes"
    );

    bool read_all_cells = true;
    for (std::uint8_t x = 0U; x < 4U; ++x) {
        const auto address = static_cast<std::uint8_t>(x << 6U);
        read_all_cells &= display_data_controller.write_control(address)
            == Hd44102OperationStatus::ok;
        read_all_cells &= display_data_controller.complete_busy_period()
            == Hd44102OperationStatus::ok;
        const auto dummy = display_data_controller.read_display_data();
        const auto expected_stale = x == 0U
            ? std::optional<std::uint8_t>{}
            : std::optional<std::uint8_t>{
                static_cast<std::uint8_t>(
                    static_cast<std::uint16_t>(x - 1U) * 50U
                )
            };
        read_all_cells &= dummy.status == Hd44102OperationStatus::ok;
        read_all_cells &= dummy.value == expected_stale;
        read_all_cells &= display_data_controller.read_status().value
            == 0xE0U;
        read_all_cells &= display_data_controller.read_status().known_mask
            == 0xFFU;
        read_all_cells &= display_data_controller.x_address() == x;
        read_all_cells &= !display_data_controller.y_address().has_value();
        read_all_cells &= display_data_controller.read_display_data().status
            == Hd44102OperationStatus::busy;
        read_all_cells &= display_data_controller.write_control(0x3BU)
            == Hd44102OperationStatus::busy;
        read_all_cells &= display_data_controller.write_display_data(0xFFU)
            == Hd44102OperationStatus::busy;
        read_all_cells &= display_data_controller.complete_busy_period()
            == Hd44102OperationStatus::ok;
        read_all_cells &= display_data_controller.y_address() == 1U;

        for (std::uint8_t y = 0U; y < 50U; ++y) {
            const auto result = display_data_controller.read_display_data();
            const auto expected_value = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(x) * 50U + y
            );
            read_all_cells &= result.status == Hd44102OperationStatus::ok;
            read_all_cells &= result.value == expected_value;
            read_all_cells &= !display_data_controller
                .y_address()
                .has_value();
            read_all_cells &= display_data_controller
                .display_ram_value(x, y) == expected_value;
            read_all_cells &= display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok;
            read_all_cells &= display_data_controller.y_address()
                == static_cast<std::uint8_t>((y + 2U) % 50U);
        }
    }
    for (std::uint8_t x = 0U; x < 4U; ++x) {
        for (std::uint8_t y = 0U; y < 50U; ++y) {
            const auto value = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(x) * 50U + y
            );
            read_all_cells &= display_data_controller
                .display_ram_value(x, y) == value;
        }
    }
    passed &= expect(
        read_all_cells,
        "Display reads did not implement the dummy-read pipeline exactly"
    );

    bool all_off_dots_forced_low = true;
    for (std::uint8_t y = 0U; y < 50U; ++y) {
        for (std::uint8_t row = 0U; row < 32U; ++row) {
            all_off_dots_forced_low &= display_data_controller
                .display_dot_1_32(y, row) == false;
        }
    }
    passed &= expect(
        all_off_dots_forced_low
            && !display_data_controller
                .display_dot_1_32(50U, 0U)
                .has_value()
            && !display_data_controller
                .display_dot_1_32(0U, 32U)
                .has_value(),
        "Display-off state did not force every valid logical dot low"
    );

    passed &= expect(
        display_data_controller.display_ram_value(1U, 25U) == 75U
            && display_data_controller.write_control(0x39U)
                == Hd44102OperationStatus::ok
            && display_data_controller.read_status().value == 0xC0U
            && display_data_controller.read_status().known_mask == 0xDFU
            && !display_data_controller
                .display_dot_1_32(25U, 12U)
                .has_value()
            && display_data_controller.display_ram_value(1U, 25U) == 75U
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.read_status().value == 0x40U
            && !display_data_controller
                .display_dot_1_32(25U, 12U)
                .has_value()
            && display_data_controller.display_ram_value(1U, 25U) == 75U
            && display_data_controller.write_control(0x38U)
                == Hd44102OperationStatus::ok
            && display_data_controller.display_ram_value(1U, 25U) == 75U
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.read_status().value == 0x60U
            && display_data_controller.display_dot_1_32(25U, 12U) == false
            && display_data_controller.display_ram_value(1U, 25U) == 75U,
        "Display on/off modified display RAM"
    );

    bool all_start_pages_mapped = true;
    for (std::uint8_t start_page = 0U; start_page < 4U; ++start_page) {
        const auto instruction = static_cast<std::uint8_t>(
            (start_page << 6U) | 0x3EU
        );
        all_start_pages_mapped &= display_data_controller
            .write_control(instruction) == Hd44102OperationStatus::ok;
        all_start_pages_mapped &= display_data_controller
            .display_dot_1_32(0U, 0U) == false;
        all_start_pages_mapped &= display_data_controller
            .complete_busy_period() == Hd44102OperationStatus::ok;
        all_start_pages_mapped &= display_data_controller
            .write_control(0x39U) == Hd44102OperationStatus::ok;
        all_start_pages_mapped &= !display_data_controller
            .display_dot_1_32(0U, 0U)
            .has_value();
        all_start_pages_mapped &= display_data_controller
            .complete_busy_period() == Hd44102OperationStatus::ok;

        for (std::uint8_t y = 0U; y < 50U; ++y) {
            for (std::uint8_t row = 0U; row < 32U; ++row) {
                const auto x = static_cast<std::uint8_t>(
                    (start_page + row / 8U) & 0x03U
                );
                const auto value = static_cast<std::uint8_t>(
                    static_cast<std::uint16_t>(x) * 50U + y
                );
                const auto expected = ((value >> (row & 0x07U)) & 0x01U)
                    != 0U;
                all_start_pages_mapped &= display_data_controller
                    .display_dot_1_32(y, row) == expected;
            }
        }

        all_start_pages_mapped &= display_data_controller
            .write_control(0x38U) == Hd44102OperationStatus::ok;
        all_start_pages_mapped &= !display_data_controller
            .display_dot_1_32(0U, 0U)
            .has_value();
        all_start_pages_mapped &= display_data_controller
            .complete_busy_period() == Hd44102OperationStatus::ok;
        all_start_pages_mapped &= display_data_controller
            .display_dot_1_32(0U, 0U) == false;
    }
    passed &= expect(
        all_start_pages_mapped,
        "Display start pages did not map all 50 by 32 logical dots"
    );

    jr800::core::Hd44102 dot_controller;
    dot_controller.set_reset_line(true);
    dot_controller.set_reset_line(false);
    bool dot_pattern_written = true;
    for (std::uint8_t x = 0U; x < 4U; ++x) {
        const auto address = static_cast<std::uint8_t>(x << 6U);
        dot_pattern_written &= dot_controller.write_control(address)
            == Hd44102OperationStatus::ok;
        dot_pattern_written &= dot_controller.complete_busy_period()
            == Hd44102OperationStatus::ok;
        dot_pattern_written &= dot_controller.write_display_data(
            static_cast<std::uint8_t>(1U << x)
        ) == Hd44102OperationStatus::ok;
        dot_pattern_written &= dot_controller.complete_busy_period()
            == Hd44102OperationStatus::ok;
    }
    passed &= expect(
        dot_pattern_written
            && dot_controller.write_control(0x7EU)
                == Hd44102OperationStatus::ok
            && dot_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && dot_controller.write_control(0x39U)
                == Hd44102OperationStatus::ok
            && dot_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && dot_controller.display_dot_1_32(0U, 0U) == false
            && dot_controller.display_dot_1_32(0U, 1U) == true
            && dot_controller.display_dot_1_32(0U, 9U) == false
            && dot_controller.display_dot_1_32(0U, 10U) == true
            && dot_controller.display_dot_1_32(0U, 19U) == true
            && dot_controller.display_dot_1_32(0U, 24U) == true
            && dot_controller.display_dot_1_32(0U, 25U) == false
            && !dot_controller.display_dot_1_32(1U, 1U).has_value(),
        "Independent one-bit pages did not rotate from start page one"
    );

    passed &= expect(
        dot_controller.write_control(0x40U)
                == Hd44102OperationStatus::ok
            && dot_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && dot_controller.write_display_data(0x00U)
                == Hd44102OperationStatus::ok
            && !dot_controller.display_dot_1_32(0U, 1U).has_value()
            && dot_controller.display_dot_1_32(0U, 10U) == true
            && dot_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && dot_controller.display_dot_1_32(0U, 1U) == false
            && dot_controller.display_dot_1_32(0U, 10U) == true,
        "Pending RAM write did not isolate logical-dot knownness"
    );
    passed &= expect(
        dot_controller.write_control(0xBEU)
                == Hd44102OperationStatus::ok
            && !dot_controller.display_dot_1_32(0U, 2U).has_value()
            && dot_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && dot_controller.display_dot_1_32(0U, 2U) == true
            && dot_controller.write_control(0x38U)
                == Hd44102OperationStatus::ok
            && !dot_controller.display_dot_1_32(0U, 2U).has_value()
            && dot_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && dot_controller.display_dot_1_32(0U, 2U) == false,
        "Pending page/display state did not control dot knownness"
    );

    passed &= expect(
        display_data_controller.write_control(0x3AU)
                == Hd44102OperationStatus::ok
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.write_control(0x80U)
                == Hd44102OperationStatus::ok
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok,
        "Count-down display-read preconditions did not commit"
    );
    const auto down_dummy = display_data_controller.read_display_data();
    passed &= expect(
        down_dummy.status == Hd44102OperationStatus::ok
            && down_dummy.value == 150U
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.y_address() == 49U,
        "Count-down dummy read did not wrap Y from zero to 49"
    );
    const auto down_first = display_data_controller.read_display_data();
    passed &= expect(
        down_first.status == Hd44102OperationStatus::ok
            && down_first.value == 100U
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.y_address() == 48U
            && display_data_controller.display_ram_value(2U, 0U) == 100U,
        "Count-down display read did not return RAM without modifying it"
    );

    passed &= expect(
        display_data_controller.write_control(0x3AU)
                == Hd44102OperationStatus::ok
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.write_control(0x80U)
                == Hd44102OperationStatus::ok
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.x_address() == 2U
            && display_data_controller.y_address() == 0U
            && display_data_controller.display_ram_value(2U, 0U) == 100U
            && display_data_controller.write_display_data(0xE7U)
                == Hd44102OperationStatus::ok
            && display_data_controller.read_status().value == 0xA0U
            && display_data_controller.x_address() == 2U
            && !display_data_controller.y_address().has_value()
            && !display_data_controller
                .display_ram_value(2U, 0U)
                .has_value()
            && display_data_controller.display_ram_value(2U, 1U) == 101U
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.display_ram_value(2U, 0U) == 0xE7U
            && display_data_controller.display_ram_value(2U, 1U) == 101U
            && display_data_controller.x_address() == 2U
            && display_data_controller.y_address() == 49U
            && display_data_controller.write_display_data(0xD6U)
                == Hd44102OperationStatus::ok
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.display_ram_value(2U, 49U) == 0xD6U
            && display_data_controller.y_address() == 48U,
        "Count-down display writes did not decrement and wrap Y only"
    );

    passed &= expect(
        display_data_controller.write_control(0x80U)
                == Hd44102OperationStatus::ok
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok,
        "Post-write output-register check address did not commit"
    );
    const auto post_write_dummy = display_data_controller.read_display_data();
    passed &= expect(
        post_write_dummy.status == Hd44102OperationStatus::ok
            && post_write_dummy.value == 149U
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.display_ram_value(2U, 0U) == 0xE7U
            && display_data_controller.display_ram_value(2U, 49U) == 0xD6U,
        "Display writes modified the output register or reads modified RAM"
    );

    bool all_count_down_addresses_advanced = true;
    for (std::uint8_t y = 0U; y < 50U; ++y) {
        const auto address = static_cast<std::uint8_t>(0xC0U | y);
        const auto value = static_cast<std::uint8_t>(0xA5U ^ y);
        all_count_down_addresses_advanced &= display_data_controller
            .write_control(address) == Hd44102OperationStatus::ok;
        all_count_down_addresses_advanced &= display_data_controller
            .complete_busy_period() == Hd44102OperationStatus::ok;
        all_count_down_addresses_advanced &= display_data_controller
            .write_display_data(value) == Hd44102OperationStatus::ok;
        all_count_down_addresses_advanced &= display_data_controller
            .complete_busy_period() == Hd44102OperationStatus::ok;
        all_count_down_addresses_advanced &= display_data_controller
            .display_ram_value(3U, y) == value;
        all_count_down_addresses_advanced &= display_data_controller
            .y_address()
                == static_cast<std::uint8_t>(y == 0U ? 49U : y - 1U);
    }
    passed &= expect(
        all_count_down_addresses_advanced,
        "Count-down display writes did not cover every Y address"
    );

    bool all_data_values_preserved = true;
    for (std::uint16_t value = 0U; value <= 0xFFU; ++value) {
        all_data_values_preserved &= display_data_controller
            .write_control(0x59U) == Hd44102OperationStatus::ok;
        all_data_values_preserved &= display_data_controller
            .complete_busy_period() == Hd44102OperationStatus::ok;
        all_data_values_preserved &= display_data_controller
            .write_display_data(static_cast<std::uint8_t>(value))
                == Hd44102OperationStatus::ok;
        all_data_values_preserved &= display_data_controller
            .complete_busy_period() == Hd44102OperationStatus::ok;
        all_data_values_preserved &= display_data_controller
            .display_ram_value(1U, 25U)
                == static_cast<std::uint8_t>(value);
        all_data_values_preserved &= display_data_controller.y_address()
            == 24U;
    }
    passed &= expect(
        all_data_values_preserved,
        "Display-data writes did not preserve every byte value"
    );

    passed &= expect(
        display_data_controller.write_control(0x4AU)
                == Hd44102OperationStatus::ok
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.write_display_data(0x5AU)
                == Hd44102OperationStatus::ok
            && display_data_controller.write_control(0x3BU)
                == Hd44102OperationStatus::busy
            && display_data_controller.read_display_data().status
                == Hd44102OperationStatus::busy,
        "A display-data operation did not establish the shared busy gate"
    );
    bool all_busy_data_writes_rejected = true;
    for (std::uint16_t value = 0U; value <= 0xFFU; ++value) {
        all_busy_data_writes_rejected &= display_data_controller
            .write_display_data(static_cast<std::uint8_t>(value))
                == Hd44102OperationStatus::busy;
    }
    passed &= expect(
        all_busy_data_writes_rejected
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.display_ram_value(1U, 10U) == 0x5AU
            && display_data_controller.y_address() == 9U,
        "Busy display-data writes changed the accepted operation"
    );

    passed &= expect(
        display_data_controller.write_control(0x00U)
                == Hd44102OperationStatus::ok
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::ok
            && display_data_controller.write_display_data(0xC3U)
                == Hd44102OperationStatus::ok,
        "Reset-cancellation display write was not staged"
    );
    display_data_controller.set_reset_line(true);
    bool reset_invalidated_ram = true;
    for (std::uint8_t x = 0U; x < 4U; ++x) {
        for (std::uint8_t y = 0U; y < 50U; ++y) {
            reset_invalidated_ram &= !display_data_controller
                .display_ram_value(x, y)
                .has_value();
        }
    }
    passed &= expect(
        reset_invalidated_ram
            && display_data_controller.display_dot_1_32(0U, 0U) == false
            && display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::reset_asserted,
        "Reset invented display-RAM retention or completed a pending write"
    );
    display_data_controller.set_reset_line(false);
    passed &= expect(
        display_data_controller.complete_busy_period()
                == Hd44102OperationStatus::no_pending_instruction
            && display_data_controller.write_display_data(0xC3U)
                == Hd44102OperationStatus::unknown_state,
        "Reset release retained a pending write or invented an address"
    );

    jr800::core::Hd44102 read_reset_controller;
    read_reset_controller.set_reset_line(true);
    read_reset_controller.set_reset_line(false);
    passed &= expect(
        read_reset_controller.write_control(0x00U)
                == Hd44102OperationStatus::ok
            && read_reset_controller.complete_busy_period()
                == Hd44102OperationStatus::ok,
        "Unknown-RAM display-read address did not commit"
    );
    const auto unknown_dummy = read_reset_controller.read_display_data();
    passed &= expect(
        unknown_dummy.status == Hd44102OperationStatus::ok
            && !unknown_dummy.value.has_value()
            && read_reset_controller.complete_busy_period()
                == Hd44102OperationStatus::ok,
        "Dummy read treated an unknown output byte as an operation error"
    );
    const auto unknown_data = read_reset_controller.read_display_data();
    passed &= expect(
        unknown_data.status == Hd44102OperationStatus::ok
            && !unknown_data.value.has_value(),
        "Unknown RAM data did not remain distinct from read status"
    );
    read_reset_controller.set_reset_line(true);
    read_reset_controller.set_reset_line(false);
    passed &= expect(
        read_reset_controller.complete_busy_period()
                == Hd44102OperationStatus::no_pending_instruction
            && read_reset_controller.read_display_data().status
                == Hd44102OperationStatus::unknown_state,
        "Reset retained a pending display read or invented an address"
    );

    controller.set_reset_line(false);
    const auto repeated_release = controller.read_status();
    passed &= expect(
        repeated_release.value == 0x60U
            && repeated_release.known_mask == 0xFFU,
        "Repeated reset release changed stable status"
    );

    controller.set_reset_line(true);
    const auto reasserted = controller.read_status();
    passed &= expect(
        reasserted.value == 0xF0U
            && reasserted.known_mask == 0xFFU,
        "Reasserted reset did not restore documented reset status"
    );

    return passed ? 0 : 1;
}
