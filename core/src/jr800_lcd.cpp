// SPDX-License-Identifier: MIT

#include "jr800/core/jr800_lcd.hpp"

#include <bit>

namespace jr800::core {
namespace {

constexpr std::uint16_t base_address = 0x0A00U;
constexpr std::uint16_t final_address = 0x0BFFU;
constexpr std::uint16_t display_data_mask = 0x0100U;
constexpr std::size_t controller_column_count = 50U;
constexpr std::size_t controller_row_count = 32U;
constexpr std::uint8_t maximum_controller_y = 49U;
constexpr std::size_t panel_left_margin = 4U;
constexpr std::uint8_t indicator_count = 16U;

Jr800LcdAccessStatus access_status(
    Hd44102OperationStatus status
) noexcept {
    switch (status) {
    case Hd44102OperationStatus::ok:
        return Jr800LcdAccessStatus::ok;
    case Hd44102OperationStatus::busy:
        return Jr800LcdAccessStatus::busy;
    case Hd44102OperationStatus::reset_asserted:
        return Jr800LcdAccessStatus::reset_asserted;
    case Hd44102OperationStatus::unknown_state:
        return Jr800LcdAccessStatus::unknown_state;
    case Hd44102OperationStatus::unsupported_instruction:
        return Jr800LcdAccessStatus::unsupported_instruction;
    case Hd44102OperationStatus::no_pending_instruction:
        return Jr800LcdAccessStatus::no_pending_instruction;
    }
    return Jr800LcdAccessStatus::unknown_state;
}

}  // namespace

Jr800LcdDecodeResult decode_jr800_lcd_address(
    std::uint16_t address
) noexcept {
    if (address < base_address || address > final_address) {
        return {};
    }

    const auto select = static_cast<std::uint8_t>(address & 0x00FFU);
    if (select == 0U
        || (select & static_cast<std::uint8_t>(select - 1U)) != 0U) {
        return {true, std::nullopt};
    }

    const auto controller_index = static_cast<std::uint8_t>(
        std::countr_zero(static_cast<unsigned>(select))
    );
    const auto target = (address & display_data_mask) == 0U
        ? Jr800LcdRegister::control_status
        : Jr800LcdRegister::display_data;
    return {true, Jr800LcdSelection{controller_index, target}};
}

std::optional<Jr800LcdPanelCoordinate> map_jr800_lcd_panel_coordinate(
    std::size_t column,
    std::size_t row
) noexcept {
    if (column >= Jr800Lcd::panel_width
        || row >= Jr800Lcd::panel_height) {
        return std::nullopt;
    }

    const auto expanded_column = column + panel_left_margin;
    const auto horizontal_index = static_cast<std::uint8_t>(
        expanded_column / controller_column_count
    );
    const auto local_column = static_cast<std::uint8_t>(
        expanded_column % controller_column_count
    );
    const auto lower_half = row >= controller_row_count;
    const auto controller_index = lower_half
        ? static_cast<std::uint8_t>(4U + horizontal_index)
        : horizontal_index;
    const auto controller_y = lower_half
        ? local_column
        : static_cast<std::uint8_t>(maximum_controller_y - local_column);
    const auto controller_row = static_cast<std::uint8_t>(
        row % controller_row_count
    );
    return Jr800LcdPanelCoordinate{
        controller_index,
        controller_y,
        controller_row,
    };
}

std::optional<Jr800LcdIndicatorRamCoordinate>
map_jr800_lcd_indicator_ram(Jr800LcdIndicator indicator) noexcept {
    const auto index = static_cast<std::uint8_t>(indicator);
    if (index >= indicator_count) {
        return std::nullopt;
    }
    if (index < 4U) {
        return Jr800LcdIndicatorRamCoordinate{0U, index, 46U};
    }
    if (index < 8U) {
        return Jr800LcdIndicatorRamCoordinate{
            4U,
            static_cast<std::uint8_t>(index - 4U),
            3U,
        };
    }
    if (index < 12U) {
        return Jr800LcdIndicatorRamCoordinate{
            3U,
            static_cast<std::uint8_t>(index - 8U),
            3U,
        };
    }
    return Jr800LcdIndicatorRamCoordinate{
        7U,
        static_cast<std::uint8_t>(index - 12U),
        46U,
    };
}

bool Jr800Lcd::set_controller_reset_line(
    std::uint8_t controller_index,
    bool asserted
) noexcept {
    if (controller_index >= controllers_.size()) {
        return false;
    }
    controllers_[controller_index].set_reset_line(asserted);
    return true;
}

Jr800LcdReadResult Jr800Lcd::read8(std::uint16_t address) noexcept {
    const auto decoded = decode_jr800_lcd_address(address);
    if (!decoded.handled) {
        return {};
    }
    if (!decoded.selection.has_value()) {
        return {
            Jr800LcdAccessStatus::unsupported_select,
            std::nullopt,
            0U,
            0U,
        };
    }

    const auto selection = *decoded.selection;
    auto& controller = controllers_[selection.controller_index];
    if (selection.target == Jr800LcdRegister::control_status) {
        const auto status = controller.read_status();
        return {
            Jr800LcdAccessStatus::ok,
            selection,
            status.value,
            status.known_mask,
        };
    }

    const auto read = controller.read_display_data();
    return {
        access_status(read.status),
        selection,
        read.value.value_or(0U),
        static_cast<std::uint8_t>(read.value.has_value() ? 0xFFU : 0U),
    };
}

Jr800LcdWriteResult Jr800Lcd::write8(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    const auto decoded = decode_jr800_lcd_address(address);
    if (!decoded.handled) {
        return {};
    }
    if (!decoded.selection.has_value()) {
        return {Jr800LcdAccessStatus::unsupported_select, std::nullopt};
    }

    const auto selection = *decoded.selection;
    auto& controller = controllers_[selection.controller_index];
    const auto operation = selection.target == Jr800LcdRegister::control_status
        ? controller.write_control(value)
        : controller.write_display_data(value);
    return {access_status(operation), selection};
}

Jr800LcdAccessStatus Jr800Lcd::complete_busy_period(
    std::uint8_t controller_index
) noexcept {
    if (controller_index >= controllers_.size()) {
        return Jr800LcdAccessStatus::unsupported_select;
    }
    return access_status(
        controllers_[controller_index].complete_busy_period()
    );
}

std::optional<Jr800LcdControllerState> Jr800Lcd::inspect_controller(
    std::uint8_t controller_index
) const noexcept {
    if (controller_index >= controllers_.size()) {
        return std::nullopt;
    }
    const auto& controller = controllers_[controller_index];
    return Jr800LcdControllerState{
        controller.read_status(),
        controller.display_start_page(),
        controller.x_address(),
        controller.y_address(),
    };
}

std::optional<std::uint8_t> Jr800Lcd::display_ram_value(
    std::uint8_t controller_index,
    std::uint8_t x,
    std::uint8_t y
) const noexcept {
    if (controller_index >= controllers_.size()) {
        return std::nullopt;
    }
    return controllers_[controller_index].display_ram_value(x, y);
}

std::optional<bool> Jr800Lcd::display_dot_1_32(
    std::uint8_t controller_index,
    std::uint8_t y,
    std::uint8_t row
) const noexcept {
    if (controller_index >= controllers_.size()) {
        return std::nullopt;
    }
    return controllers_[controller_index].display_dot_1_32(y, row);
}

std::optional<bool> Jr800Lcd::display_panel_dot(
    std::size_t column,
    std::size_t row
) const noexcept {
    const auto coordinate = map_jr800_lcd_panel_coordinate(column, row);
    if (!coordinate.has_value()) {
        return std::nullopt;
    }
    return display_dot_1_32(
        coordinate->controller_index,
        coordinate->controller_y,
        coordinate->controller_row
    );
}

std::optional<std::uint8_t> Jr800Lcd::indicator_ram_value(
    Jr800LcdIndicator indicator
) const noexcept {
    const auto coordinate = map_jr800_lcd_indicator_ram(indicator);
    if (!coordinate.has_value()) {
        return std::nullopt;
    }
    return display_ram_value(
        coordinate->controller_index,
        coordinate->controller_x,
        coordinate->controller_y
    );
}

}  // namespace jr800::core
