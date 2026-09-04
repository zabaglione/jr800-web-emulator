// SPDX-License-Identifier: MIT

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "jr800/core/jr800_lcd.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    using jr800::core::Jr800Lcd;
    using jr800::core::Jr800LcdAccessStatus;
    using jr800::core::Jr800LcdIndicator;
    using jr800::core::Jr800LcdIndicatorRamCoordinate;
    using jr800::core::Jr800LcdPanelCoordinate;
    using jr800::core::Jr800LcdRegister;
    using jr800::core::decode_jr800_lcd_address;
    using jr800::core::map_jr800_lcd_indicator_ram;
    using jr800::core::map_jr800_lcd_panel_coordinate;

    bool passed = true;
    std::size_t handled_count = 0U;
    std::size_t selected_count = 0U;
    std::size_t unsupported_select_count = 0U;

    for (std::uint32_t candidate = 0U; candidate <= 0xFFFFU; ++candidate) {
        const auto address = static_cast<std::uint16_t>(candidate);
        const auto decoded = decode_jr800_lcd_address(address);
        const auto in_window = address >= 0x0A00U && address <= 0x0BFFU;
        const auto select = static_cast<std::uint8_t>(address & 0x00FFU);
        const auto one_hot = select != 0U
            && (select & static_cast<std::uint8_t>(select - 1U)) == 0U;
        const auto expected_selected = in_window && one_hot;

        handled_count += decoded.handled ? 1U : 0U;
        selected_count += decoded.selected() ? 1U : 0U;
        unsupported_select_count +=
            decoded.handled && !decoded.selected() ? 1U : 0U;
        passed &= expect(
            decoded.handled == in_window
                && decoded.selected() == expected_selected,
            "LCD window or selection classification differs"
        );
        if (!expected_selected) {
            passed &= expect(
                !decoded.selection.has_value(),
                "Unsupported LCD address produced a selection"
            );
            continue;
        }

        const auto expected_controller = static_cast<std::uint8_t>(
            std::countr_zero(static_cast<unsigned>(select))
        );
        const auto expected_target = (address & 0x0100U) == 0U
            ? Jr800LcdRegister::control_status
            : Jr800LcdRegister::display_data;
        passed &= expect(
            decoded.selection
                == jr800::core::Jr800LcdSelection{
                    expected_controller,
                    expected_target,
                },
            "LCD controller or register selection differs"
        );
    }

    passed &= expect(
        handled_count == 512U
            && selected_count == 16U
            && unsupported_select_count == 496U,
        "LCD decode cardinality differs"
    );

    passed &= expect(
        map_jr800_lcd_panel_coordinate(0U, 0U)
                == Jr800LcdPanelCoordinate{0U, 45U, 0U}
            && map_jr800_lcd_panel_coordinate(45U, 31U)
                == Jr800LcdPanelCoordinate{0U, 0U, 31U}
            && map_jr800_lcd_panel_coordinate(46U, 0U)
                == Jr800LcdPanelCoordinate{1U, 49U, 0U}
            && map_jr800_lcd_panel_coordinate(145U, 31U)
                == Jr800LcdPanelCoordinate{2U, 0U, 31U}
            && map_jr800_lcd_panel_coordinate(146U, 0U)
                == Jr800LcdPanelCoordinate{3U, 49U, 0U}
            && map_jr800_lcd_panel_coordinate(191U, 31U)
                == Jr800LcdPanelCoordinate{3U, 4U, 31U}
            && map_jr800_lcd_panel_coordinate(0U, 32U)
                == Jr800LcdPanelCoordinate{4U, 4U, 0U}
            && map_jr800_lcd_panel_coordinate(45U, 63U)
                == Jr800LcdPanelCoordinate{4U, 49U, 31U}
            && map_jr800_lcd_panel_coordinate(46U, 32U)
                == Jr800LcdPanelCoordinate{5U, 0U, 0U}
            && map_jr800_lcd_panel_coordinate(146U, 32U)
                == Jr800LcdPanelCoordinate{7U, 0U, 0U}
            && map_jr800_lcd_panel_coordinate(191U, 63U)
                == Jr800LcdPanelCoordinate{7U, 45U, 31U}
            && !map_jr800_lcd_panel_coordinate(192U, 0U).has_value()
            && !map_jr800_lcd_panel_coordinate(0U, 64U).has_value(),
        "LCD panel boundary mapping differs"
    );

    std::array<bool, 8U * 50U * 32U> mapped_cells{};
    std::array<std::size_t, 8U> controller_dot_counts{};
    std::size_t mapped_dot_count = 0U;
    bool panel_mapping_is_injective = true;
    for (std::size_t row = 0U; row < Jr800Lcd::panel_height; ++row) {
        for (std::size_t column = 0U;
             column < Jr800Lcd::panel_width;
             ++column) {
            const auto coordinate = map_jr800_lcd_panel_coordinate(
                column,
                row
            );
            if (!coordinate.has_value()) {
                panel_mapping_is_injective = false;
                continue;
            }
            const auto index = static_cast<std::size_t>(
                coordinate->controller_index
            ) * 50U * 32U
                + static_cast<std::size_t>(coordinate->controller_row) * 50U
                + coordinate->controller_y;
            panel_mapping_is_injective &= !mapped_cells[index];
            mapped_cells[index] = true;
            ++controller_dot_counts[coordinate->controller_index];
            ++mapped_dot_count;
        }
    }
    bool panel_mapping_has_exact_coverage = true;
    const auto expected_panel_cell = [](
        std::size_t controller,
        std::size_t y
    ) noexcept {
        if (controller == 0U || controller == 7U) {
            return y <= 45U;
        }
        if (controller == 3U || controller == 4U) {
            return y >= 4U;
        }
        return true;
    };
    for (std::size_t controller = 0U; controller < 8U; ++controller) {
        for (std::size_t row = 0U; row < 32U; ++row) {
            for (std::size_t y = 0U; y < 50U; ++y) {
                const auto index = controller * 50U * 32U + row * 50U + y;
                panel_mapping_has_exact_coverage &=
                    mapped_cells[index]
                        == expected_panel_cell(controller, y);
            }
        }
    }
    passed &= expect(
        panel_mapping_is_injective
            && panel_mapping_has_exact_coverage
            && mapped_dot_count == 192U * 64U
            && controller_dot_counts
                == std::array<std::size_t, 8U>{
                    1472U,
                    1600U,
                    1600U,
                    1472U,
                    1472U,
                    1600U,
                    1600U,
                    1472U,
                },
        "LCD panel mapping cardinality or controller coverage differs"
    );

    const std::array<Jr800LcdIndicator, 16U> indicators{
        Jr800LcdIndicator::page_1,
        Jr800LcdIndicator::page_2,
        Jr800LcdIndicator::page_3,
        Jr800LcdIndicator::page_4,
        Jr800LcdIndicator::page_5,
        Jr800LcdIndicator::page_6,
        Jr800LcdIndicator::page_7,
        Jr800LcdIndicator::page_8,
        Jr800LcdIndicator::capital_lock,
        Jr800LcdIndicator::graphics_input,
        Jr800LcdIndicator::kana_input,
        Jr800LcdIndicator::insert_mode,
        Jr800LcdIndicator::control_mode,
        Jr800LcdIndicator::radian_mode,
        Jr800LcdIndicator::degree_mode,
        Jr800LcdIndicator::battery_warning,
    };
    const std::array<Jr800LcdIndicatorRamCoordinate, 16U>
        expected_indicator_coordinates{
            Jr800LcdIndicatorRamCoordinate{0U, 0U, 46U},
            Jr800LcdIndicatorRamCoordinate{0U, 1U, 46U},
            Jr800LcdIndicatorRamCoordinate{0U, 2U, 46U},
            Jr800LcdIndicatorRamCoordinate{0U, 3U, 46U},
            Jr800LcdIndicatorRamCoordinate{4U, 0U, 3U},
            Jr800LcdIndicatorRamCoordinate{4U, 1U, 3U},
            Jr800LcdIndicatorRamCoordinate{4U, 2U, 3U},
            Jr800LcdIndicatorRamCoordinate{4U, 3U, 3U},
            Jr800LcdIndicatorRamCoordinate{3U, 0U, 3U},
            Jr800LcdIndicatorRamCoordinate{3U, 1U, 3U},
            Jr800LcdIndicatorRamCoordinate{3U, 2U, 3U},
            Jr800LcdIndicatorRamCoordinate{3U, 3U, 3U},
            Jr800LcdIndicatorRamCoordinate{7U, 0U, 46U},
            Jr800LcdIndicatorRamCoordinate{7U, 1U, 46U},
            Jr800LcdIndicatorRamCoordinate{7U, 2U, 46U},
            Jr800LcdIndicatorRamCoordinate{7U, 3U, 46U},
        };
    std::array<bool, 8U * 4U * 50U> indicator_cells{};
    bool indicator_mapping_is_exact = true;
    for (std::size_t index = 0U; index < indicators.size(); ++index) {
        const auto coordinate = map_jr800_lcd_indicator_ram(
            indicators[index]
        );
        indicator_mapping_is_exact &=
            coordinate == expected_indicator_coordinates[index];
        if (!coordinate.has_value()) {
            continue;
        }
        const auto cell_index = static_cast<std::size_t>(
            coordinate->controller_index
        ) * 4U * 50U
            + static_cast<std::size_t>(coordinate->controller_x) * 50U
            + coordinate->controller_y;
        indicator_mapping_is_exact &= !indicator_cells[cell_index];
        indicator_cells[cell_index] = true;
        for (std::size_t bit = 0U; bit < 8U; ++bit) {
            const auto dot_index = static_cast<std::size_t>(
                coordinate->controller_index
            ) * 50U * 32U
                + (static_cast<std::size_t>(coordinate->controller_x) * 8U
                    + bit) * 50U
                + coordinate->controller_y;
            indicator_mapping_is_exact &= !mapped_cells[dot_index];
        }
    }
    passed &= expect(
        indicator_mapping_is_exact
            && !map_jr800_lcd_indicator_ram(
                static_cast<Jr800LcdIndicator>(16U)
            ).has_value(),
        "LCD indicator RAM mapping differs"
    );

    Jr800Lcd indicator_lcd;
    const std::array<Jr800LcdIndicator, 4U> written_indicators{
        Jr800LcdIndicator::page_1,
        Jr800LcdIndicator::page_5,
        Jr800LcdIndicator::capital_lock,
        Jr800LcdIndicator::control_mode,
    };
    const std::array<std::uint8_t, 4U> written_values{
        0x08U,
        0x10U,
        0x20U,
        0x40U,
    };
    bool indicator_values_written = true;
    for (std::size_t index = 0U;
         index < written_indicators.size();
         ++index) {
        const auto coordinate = map_jr800_lcd_indicator_ram(
            written_indicators[index]
        );
        if (!coordinate.has_value()) {
            indicator_values_written = false;
            continue;
        }
        const auto select = static_cast<std::uint16_t>(
            1U << coordinate->controller_index
        );
        const auto control = static_cast<std::uint16_t>(0x0A00U | select);
        const auto data = static_cast<std::uint16_t>(0x0B00U | select);
        const auto address = static_cast<std::uint8_t>(
            (coordinate->controller_x << 6U) | coordinate->controller_y
        );
        indicator_values_written &= indicator_lcd.set_controller_reset_line(
                coordinate->controller_index,
                true
            )
            && indicator_lcd.set_controller_reset_line(
                coordinate->controller_index,
                false
            )
            && indicator_lcd.write8(control, address).succeeded()
            && indicator_lcd.complete_busy_period(
                coordinate->controller_index
            ) == Jr800LcdAccessStatus::ok
            && indicator_lcd.write8(data, written_values[index]).succeeded()
            && indicator_lcd.complete_busy_period(
                coordinate->controller_index
            ) == Jr800LcdAccessStatus::ok;
    }
    bool indicator_values_read = indicator_values_written;
    for (std::size_t index = 0U;
         index < written_indicators.size();
         ++index) {
        indicator_values_read &= indicator_lcd.indicator_ram_value(
            written_indicators[index]
        ) == written_values[index];
    }
    passed &= expect(
        indicator_values_read
            && !indicator_lcd.indicator_ram_value(
                Jr800LcdIndicator::page_2
            ).has_value()
            && !indicator_lcd.indicator_ram_value(
                static_cast<Jr800LcdIndicator>(16U)
            ).has_value(),
        "LCD indicator RAM inspection guessed or lost storage"
    );

    Jr800Lcd panel_lcd;
    const std::array<std::uint8_t, 8U> visible_controller_y{
        45U,
        49U,
        49U,
        49U,
        4U,
        0U,
        0U,
        0U,
    };
    bool panel_dots_written = true;
    for (std::uint8_t controller = 0U;
         controller < Jr800Lcd::controller_count;
         ++controller) {
        const auto select = static_cast<std::uint16_t>(1U << controller);
        const auto control = static_cast<std::uint16_t>(0x0A00U | select);
        const auto data = static_cast<std::uint16_t>(0x0B00U | select);
        panel_dots_written &=
            panel_lcd.set_controller_reset_line(controller, true)
            && panel_lcd.set_controller_reset_line(controller, false)
            && panel_lcd.write8(control, 0x3EU).succeeded()
            && panel_lcd.complete_busy_period(controller)
                == Jr800LcdAccessStatus::ok
            && panel_lcd.write8(control, 0x39U).succeeded()
            && panel_lcd.complete_busy_period(controller)
                == Jr800LcdAccessStatus::ok
            && panel_lcd.write8(
                control,
                visible_controller_y[controller]
            ).succeeded()
            && panel_lcd.complete_busy_period(controller)
                == Jr800LcdAccessStatus::ok
            && panel_lcd.write8(data, 0x01U).succeeded()
            && panel_lcd.complete_busy_period(controller)
                == Jr800LcdAccessStatus::ok;
    }
    const std::array<std::size_t, 8U> visible_columns{
        0U,
        46U,
        96U,
        146U,
        0U,
        46U,
        96U,
        146U,
    };
    bool panel_dots_visible = panel_dots_written;
    for (std::uint8_t controller = 0U;
         controller < Jr800Lcd::controller_count;
         ++controller) {
        const auto panel_row = controller < 4U ? 0U : 32U;
        panel_dots_visible &= panel_lcd.display_panel_dot(
                visible_columns[controller],
                panel_row
            ) == true
            && panel_lcd.display_panel_dot(
                visible_columns[controller],
                panel_row + 1U
            ) == false;
    }
    passed &= expect(
        panel_dots_visible,
        "LCD composite did not expose mapped panel dots"
    );

    Jr800Lcd lcd;
    passed &= expect(
        !lcd.set_controller_reset_line(8U, true)
            && lcd.complete_busy_period(8U)
                == Jr800LcdAccessStatus::unsupported_select
            && !lcd.inspect_controller(8U).has_value()
            && !lcd.display_ram_value(8U, 0U, 0U).has_value()
            && !lcd.display_dot_1_32(8U, 0U, 0U).has_value()
            && !lcd.display_panel_dot(192U, 0U).has_value()
            && !lcd.display_panel_dot(0U, 64U).has_value(),
        "Out-of-range LCD controller was accepted"
    );

    const auto outside_read = lcd.read8(0x09FFU);
    const auto outside_write = lcd.write8(0x0C00U, 0x39U);
    const auto unsupported_read = lcd.read8(0x0A00U);
    const auto unsupported_write = lcd.write8(0x0B03U, 0xA5U);
    passed &= expect(
        outside_read.status == Jr800LcdAccessStatus::not_handled
            && !outside_read.selection.has_value()
            && outside_write.status == Jr800LcdAccessStatus::not_handled
            && !outside_write.selection.has_value()
            && unsupported_read.status
                == Jr800LcdAccessStatus::unsupported_select
            && !unsupported_read.selection.has_value()
            && unsupported_write.status
                == Jr800LcdAccessStatus::unsupported_select
            && !unsupported_write.selection.has_value(),
        "LCD composite did not preserve decode failure classes"
    );

    for (std::uint8_t controller = 0U;
         controller < Jr800Lcd::controller_count;
         ++controller) {
        const auto select = static_cast<std::uint16_t>(1U << controller);
        const auto control_address = static_cast<std::uint16_t>(
            0x0A00U | select
        );
        const auto initial = lcd.read8(control_address);
        passed &= expect(
            initial.succeeded()
                && initial.value == 0U
                && initial.known_mask == 0x0FU
                && !initial.fully_known()
                && initial.selection
                    == jr800::core::Jr800LcdSelection{
                        controller,
                        Jr800LcdRegister::control_status,
                    }
                && lcd.write8(control_address, 0x3AU).status
                    == Jr800LcdAccessStatus::unknown_state
                && lcd.complete_busy_period(controller)
                    == Jr800LcdAccessStatus::unknown_state,
            "LCD composite guessed an unreset controller state"
        );
        passed &= expect(
            lcd.set_controller_reset_line(controller, true),
            "LCD controller reset assertion was rejected"
        );
        const auto asserted = lcd.read8(control_address);
        passed &= expect(
            asserted.fully_known()
                && asserted.value == 0xF0U
                && lcd.complete_busy_period(controller)
                    == Jr800LcdAccessStatus::reset_asserted
                && lcd.set_controller_reset_line(controller, false),
            "LCD controller reset assertion state differs"
        );
        const auto released = lcd.read8(control_address);
        const auto released_state = lcd.inspect_controller(controller);
        passed &= expect(
            released.fully_known()
                && released.value == 0x60U
                && released_state.has_value()
                && released_state->status.value == 0x60U
                && released_state->status.fully_known()
                && !released_state->display_start_page.has_value()
                && !released_state->x_address.has_value()
                && !released_state->y_address.has_value()
                && lcd.display_dot_1_32(controller, 0U, 0U) == false,
            "LCD controller reset release state differs"
        );
    }

    const auto control_zero = static_cast<std::uint16_t>(0x0A01U);
    const auto control_one = static_cast<std::uint16_t>(0x0A02U);
    const auto data_zero = static_cast<std::uint16_t>(0x0B01U);
    const auto count_down = lcd.write8(control_zero, 0x3AU);
    const auto pending_status = lcd.read8(control_zero);
    passed &= expect(
        count_down.succeeded()
            && count_down.selection
                == jr800::core::Jr800LcdSelection{
                    0U,
                    Jr800LcdRegister::control_status,
                }
            && pending_status.succeeded()
            && pending_status.value == 0xA0U
            && pending_status.known_mask == 0xBFU
            && !pending_status.fully_known()
            && lcd.read8(control_one).fully_known()
            && lcd.read8(control_one).value == 0x60U
            && lcd.write8(control_zero, 0x39U).status
                == Jr800LcdAccessStatus::busy,
        "LCD busy state leaked across controllers or auto-completed"
    );
    passed &= expect(
        lcd.complete_busy_period(0U) == Jr800LcdAccessStatus::ok
            && lcd.read8(control_zero).fully_known()
            && lcd.read8(control_zero).value == 0x20U
            && lcd.complete_busy_period(0U)
                == Jr800LcdAccessStatus::no_pending_instruction,
        "LCD explicit busy completion differs"
    );

    passed &= expect(
        lcd.set_controller_reset_line(0U, true)
            && lcd.set_controller_reset_line(0U, false)
            && lcd.write8(control_zero, 0x00U).succeeded()
            && lcd.complete_busy_period(0U) == Jr800LcdAccessStatus::ok,
        "LCD display-data address setup differs"
    );
    const auto data_write = lcd.write8(data_zero, 0xA5U);
    passed &= expect(
        data_write.succeeded()
            && data_write.selection
                == jr800::core::Jr800LcdSelection{
                    0U,
                    Jr800LcdRegister::display_data,
                }
            && lcd.read8(data_zero).status == Jr800LcdAccessStatus::busy
            && lcd.complete_busy_period(0U) == Jr800LcdAccessStatus::ok
            && lcd.write8(control_zero, 0x00U).succeeded()
            && lcd.complete_busy_period(0U) == Jr800LcdAccessStatus::ok,
        "LCD display-data routing or busy ordering differs"
    );
    const auto dummy_read = lcd.read8(data_zero);
    passed &= expect(
        dummy_read.succeeded()
            && !dummy_read.fully_known()
            && dummy_read.known_mask == 0U
            && dummy_read.selection
                == jr800::core::Jr800LcdSelection{
                    0U,
                    Jr800LcdRegister::display_data,
                }
            && lcd.complete_busy_period(0U) == Jr800LcdAccessStatus::ok,
        "LCD dummy read did not preserve unknown output state"
    );
    const auto pipelined_read = lcd.read8(data_zero);
    passed &= expect(
        pipelined_read.fully_known()
            && pipelined_read.value == 0xA5U
            && lcd.complete_busy_period(0U) == Jr800LcdAccessStatus::ok
            && lcd.display_ram_value(0U, 0U, 0U) == 0xA5U
            && !lcd.display_ram_value(1U, 0U, 0U).has_value(),
        "LCD display-data read pipeline differs"
    );
    const auto data_state = lcd.inspect_controller(0U);
    passed &= expect(
        data_state.has_value()
            && data_state->x_address == 0U
            && data_state->y_address == 2U,
        "LCD controller inspection changed or lost address state"
    );

    passed &= expect(
        lcd.write8(0x0A04U, 0x32U).status
                == Jr800LcdAccessStatus::unsupported_instruction
            && lcd.complete_busy_period(2U)
                == Jr800LcdAccessStatus::no_pending_instruction
            && lcd.read8(0x0B08U).status
                == Jr800LcdAccessStatus::unknown_state,
        "LCD unsupported or unresolved device operation was accepted"
    );
    return passed ? 0 : 1;
}
