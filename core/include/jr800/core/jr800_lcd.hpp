// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "jr800/core/hd44102.hpp"

namespace jr800::core {

enum class Jr800LcdRegister : std::uint8_t {
    control_status,
    display_data,
};

struct Jr800LcdSelection {
    std::uint8_t controller_index{};
    Jr800LcdRegister target{Jr800LcdRegister::control_status};

    bool operator==(const Jr800LcdSelection&) const = default;
};

struct Jr800LcdDecodeResult {
    bool handled{};
    std::optional<Jr800LcdSelection> selection;

    [[nodiscard]] bool selected() const noexcept {
        return handled && selection.has_value();
    }

    bool operator==(const Jr800LcdDecodeResult&) const = default;
};

enum class Jr800LcdAccessStatus : std::uint8_t {
    not_handled,
    unsupported_select,
    ok,
    busy,
    reset_asserted,
    unknown_state,
    unsupported_instruction,
    no_pending_instruction,
};

struct Jr800LcdReadResult {
    Jr800LcdAccessStatus status{Jr800LcdAccessStatus::not_handled};
    std::optional<Jr800LcdSelection> selection;
    std::uint8_t value{};
    std::uint8_t known_mask{};

    [[nodiscard]] bool succeeded() const noexcept {
        return status == Jr800LcdAccessStatus::ok;
    }

    [[nodiscard]] bool fully_known() const noexcept {
        return succeeded() && known_mask == 0xFFU;
    }

    bool operator==(const Jr800LcdReadResult&) const = default;
};

struct Jr800LcdWriteResult {
    Jr800LcdAccessStatus status{Jr800LcdAccessStatus::not_handled};
    std::optional<Jr800LcdSelection> selection;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == Jr800LcdAccessStatus::ok;
    }

    bool operator==(const Jr800LcdWriteResult&) const = default;
};

struct Jr800LcdControllerState {
    Hd44102StatusRead status;
    std::optional<std::uint8_t> display_start_page;
    std::optional<std::uint8_t> x_address;
    std::optional<std::uint8_t> y_address;
};

struct Jr800LcdPanelCoordinate {
    std::uint8_t controller_index{};
    std::uint8_t controller_y{};
    std::uint8_t controller_row{};

    bool operator==(const Jr800LcdPanelCoordinate&) const = default;
};

enum class Jr800LcdIndicator : std::uint8_t {
    page_1,
    page_2,
    page_3,
    page_4,
    page_5,
    page_6,
    page_7,
    page_8,
    capital_lock,
    graphics_input,
    kana_input,
    insert_mode,
    control_mode,
    radian_mode,
    degree_mode,
    battery_warning,
};

struct Jr800LcdIndicatorRamCoordinate {
    std::uint8_t controller_index{};
    std::uint8_t controller_x{};
    std::uint8_t controller_y{};

    bool operator==(const Jr800LcdIndicatorRamCoordinate&) const = default;
};

// Provisional logical decode only. Jr800Bus remains fail-closed for this range.
[[nodiscard]] Jr800LcdDecodeResult decode_jr800_lcd_address(
    std::uint16_t address
) noexcept;

// Provisional physical composition only. Jr800Bus remains disconnected.
[[nodiscard]] std::optional<Jr800LcdPanelCoordinate>
map_jr800_lcd_panel_coordinate(
    std::size_t column,
    std::size_t row
) noexcept;

// Provisional annunciator storage mapping.
// Active-drive semantics are unstaged.
[[nodiscard]] std::optional<Jr800LcdIndicatorRamCoordinate>
map_jr800_lcd_indicator_ram(
    Jr800LcdIndicator indicator
) noexcept;

// Composite device only. Board reset and busy timing remain caller-owned.
class Jr800Lcd final {
public:
    static constexpr std::size_t controller_count = 8U;
    static constexpr std::size_t panel_width = 192U;
    static constexpr std::size_t panel_height = 64U;

    [[nodiscard]] bool set_controller_reset_line(
        std::uint8_t controller_index,
        bool asserted
    ) noexcept;

    [[nodiscard]] Jr800LcdReadResult read8(
        std::uint16_t address
    ) noexcept;

    [[nodiscard]] Jr800LcdWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] Jr800LcdAccessStatus complete_busy_period(
        std::uint8_t controller_index
    ) noexcept;

    [[nodiscard]] std::optional<Jr800LcdControllerState> inspect_controller(
        std::uint8_t controller_index
    ) const noexcept;

    [[nodiscard]] std::optional<std::uint8_t> display_ram_value(
        std::uint8_t controller_index,
        std::uint8_t x,
        std::uint8_t y
    ) const noexcept;

    [[nodiscard]] std::optional<bool> display_dot_1_32(
        std::uint8_t controller_index,
        std::uint8_t y,
        std::uint8_t row
    ) const noexcept;

    [[nodiscard]] std::optional<bool> display_panel_dot(
        std::size_t column,
        std::size_t row
    ) const noexcept;

    [[nodiscard]] std::optional<std::uint8_t> indicator_ram_value(
        Jr800LcdIndicator indicator
    ) const noexcept;

private:
    friend class MachineStateCodec;
    std::array<Hd44102, controller_count> controllers_{};
};

}  // namespace jr800::core
