// SPDX-License-Identifier: MIT

#include "jr800/core/hd44102.hpp"

#include <cstddef>

namespace jr800::core {
namespace {

constexpr std::uint8_t busy_mask = 0x80U;
constexpr std::uint8_t count_up_mask = 0x40U;
constexpr std::uint8_t display_off_mask = 0x20U;
constexpr std::uint8_t reset_mask = 0x10U;
constexpr std::uint8_t fixed_zero_mask = 0x0FU;
constexpr std::uint8_t set_display_off_instruction = 0x38U;
constexpr std::uint8_t set_display_on_instruction = 0x39U;
constexpr std::uint8_t set_count_down_instruction = 0x3AU;
constexpr std::uint8_t set_count_up_instruction = 0x3BU;
constexpr std::uint8_t control_payload_mask = 0x3FU;
constexpr std::uint8_t set_display_start_page_payload = 0x3EU;
constexpr std::uint8_t maximum_x_address = 3U;
constexpr std::uint8_t maximum_y_address = 49U;
constexpr std::uint8_t maximum_display_row = 31U;
constexpr std::size_t display_column_count = 50U;

std::size_t display_ram_index(
    std::uint8_t x,
    std::uint8_t y
) noexcept {
    return static_cast<std::size_t>(x) * display_column_count + y;
}

std::uint8_t next_y_address(std::uint8_t y, bool count_up) noexcept {
    if (count_up) {
        return static_cast<std::uint8_t>(
            y == maximum_y_address ? 0U : y + 1U
        );
    }
    return static_cast<std::uint8_t>(
        y == 0U ? maximum_y_address : y - 1U
    );
}

void encode_optional_flag(
    const std::optional<bool>& flag,
    std::uint8_t mask,
    Hd44102StatusRead& status
) noexcept {
    if (!flag.has_value()) {
        return;
    }
    status.known_mask = static_cast<std::uint8_t>(
        status.known_mask | mask
    );
    if (*flag) {
        status.value = static_cast<std::uint8_t>(status.value | mask);
    }
}

}  // namespace

void Hd44102::set_reset_line(bool asserted) noexcept {
    if (asserted) {
        reset_asserted_ = true;
        busy_ = true;
        count_up_ = true;
        display_off_ = true;
        display_start_page_.reset();
        x_address_.reset();
        y_address_.reset();
        output_register_.reset();
        display_ram_.fill(std::nullopt);
        pending_operation_ = PendingOperation::none;
        pending_value_ = 0U;
        pending_display_read_.reset();
        pending_display_write_.reset();
        return;
    }

    if (reset_asserted_ == true) {
        busy_ = false;
    }
    reset_asserted_ = false;
}

Hd44102StatusRead Hd44102::read_status() const noexcept {
    Hd44102StatusRead status{0U, fixed_zero_mask};
    encode_optional_flag(busy_, busy_mask, status);
    encode_optional_flag(count_up_, count_up_mask, status);
    encode_optional_flag(display_off_, display_off_mask, status);
    encode_optional_flag(reset_asserted_, reset_mask, status);
    return status;
}

Hd44102OperationStatus Hd44102::write_control(
    std::uint8_t instruction
) noexcept {
    if (!busy_.has_value() || !reset_asserted_.has_value()) {
        return Hd44102OperationStatus::unknown_state;
    }
    if (*busy_) {
        return Hd44102OperationStatus::busy;
    }
    if (*reset_asserted_) {
        return Hd44102OperationStatus::reset_asserted;
    }
    if (instruction == set_count_down_instruction
        || instruction == set_count_up_instruction) {
        pending_operation_ = PendingOperation::count_direction;
        pending_value_ = instruction == set_count_up_instruction ? 1U : 0U;
        count_up_.reset();
    } else if (instruction == set_display_off_instruction
               || instruction == set_display_on_instruction) {
        pending_operation_ = PendingOperation::display_on_off;
        pending_value_ = instruction == set_display_off_instruction ? 1U : 0U;
        display_off_.reset();
    } else if ((instruction & control_payload_mask)
               == set_display_start_page_payload) {
        pending_operation_ = PendingOperation::display_start_page;
        pending_value_ = static_cast<std::uint8_t>(instruction >> 6U);
        display_start_page_.reset();
    } else if ((instruction & control_payload_mask) <= maximum_y_address) {
        pending_operation_ = PendingOperation::set_address;
        pending_value_ = instruction;
        x_address_.reset();
        y_address_.reset();
    } else {
        return Hd44102OperationStatus::unsupported_instruction;
    }

    busy_ = true;
    return Hd44102OperationStatus::ok;
}

Hd44102OperationStatus Hd44102::write_display_data(
    std::uint8_t value
) noexcept {
    if (!busy_.has_value() || !reset_asserted_.has_value()) {
        return Hd44102OperationStatus::unknown_state;
    }
    if (*busy_) {
        return Hd44102OperationStatus::busy;
    }
    if (*reset_asserted_) {
        return Hd44102OperationStatus::reset_asserted;
    }
    if (!count_up_.has_value()
        || !x_address_.has_value()
        || !y_address_.has_value()) {
        return Hd44102OperationStatus::unknown_state;
    }

    const auto x = *x_address_;
    const auto y = *y_address_;
    const auto next_y = next_y_address(y, *count_up_);
    pending_operation_ = PendingOperation::write_display_data;
    pending_display_write_ = PendingDisplayWrite{x, y, next_y, value};
    display_ram_[display_ram_index(x, y)].reset();
    y_address_.reset();
    busy_ = true;
    return Hd44102OperationStatus::ok;
}

Hd44102DisplayDataRead Hd44102::read_display_data() noexcept {
    if (!busy_.has_value() || !reset_asserted_.has_value()) {
        return {Hd44102OperationStatus::unknown_state, std::nullopt};
    }
    if (*busy_) {
        return {Hd44102OperationStatus::busy, std::nullopt};
    }
    if (*reset_asserted_) {
        return {Hd44102OperationStatus::reset_asserted, std::nullopt};
    }
    if (!count_up_.has_value()
        || !x_address_.has_value()
        || !y_address_.has_value()) {
        return {Hd44102OperationStatus::unknown_state, std::nullopt};
    }

    const auto returned_value = output_register_;
    const auto x = *x_address_;
    const auto y = *y_address_;
    pending_operation_ = PendingOperation::read_display_data;
    pending_display_read_ = PendingDisplayRead{
        x,
        y,
        next_y_address(y, *count_up_),
    };
    output_register_.reset();
    y_address_.reset();
    busy_ = true;
    return {Hd44102OperationStatus::ok, returned_value};
}

Hd44102OperationStatus Hd44102::complete_busy_period() noexcept {
    if (!busy_.has_value() || !reset_asserted_.has_value()) {
        return Hd44102OperationStatus::unknown_state;
    }
    if (*reset_asserted_) {
        return Hd44102OperationStatus::reset_asserted;
    }
    if (!*busy_) {
        return Hd44102OperationStatus::no_pending_instruction;
    }
    if (pending_operation_ == PendingOperation::none) {
        return Hd44102OperationStatus::unknown_state;
    }

    switch (pending_operation_) {
    case PendingOperation::count_direction:
        count_up_ = pending_value_ != 0U;
        break;
    case PendingOperation::display_on_off:
        display_off_ = pending_value_ != 0U;
        break;
    case PendingOperation::display_start_page:
        display_start_page_ = pending_value_;
        break;
    case PendingOperation::set_address:
        x_address_ = static_cast<std::uint8_t>(pending_value_ >> 6U);
        y_address_ = static_cast<std::uint8_t>(
            pending_value_ & control_payload_mask
        );
        break;
    case PendingOperation::read_display_data:
        if (!pending_display_read_.has_value()) {
            return Hd44102OperationStatus::unknown_state;
        }
        output_register_ = display_ram_[display_ram_index(
            pending_display_read_->x,
            pending_display_read_->y
        )];
        y_address_ = pending_display_read_->next_y;
        pending_display_read_.reset();
        break;
    case PendingOperation::write_display_data:
        if (!pending_display_write_.has_value()) {
            return Hd44102OperationStatus::unknown_state;
        }
        display_ram_[display_ram_index(
            pending_display_write_->x,
            pending_display_write_->y
        )] = pending_display_write_->value;
        y_address_ = pending_display_write_->next_y;
        pending_display_write_.reset();
        break;
    case PendingOperation::none:
        return Hd44102OperationStatus::unknown_state;
    }
    pending_operation_ = PendingOperation::none;
    pending_value_ = 0U;
    busy_ = false;
    return Hd44102OperationStatus::ok;
}

std::optional<std::uint8_t> Hd44102::display_start_page() const noexcept {
    return display_start_page_;
}

std::optional<std::uint8_t> Hd44102::x_address() const noexcept {
    return x_address_;
}

std::optional<std::uint8_t> Hd44102::y_address() const noexcept {
    return y_address_;
}

std::optional<std::uint8_t> Hd44102::display_ram_value(
    std::uint8_t x,
    std::uint8_t y
) const noexcept {
    if (x > maximum_x_address || y > maximum_y_address) {
        return std::nullopt;
    }
    return display_ram_[display_ram_index(x, y)];
}

std::optional<bool> Hd44102::display_dot_1_32(
    std::uint8_t y,
    std::uint8_t row
) const noexcept {
    if (y > maximum_y_address || row > maximum_display_row) {
        return std::nullopt;
    }
    if (!display_off_.has_value()) {
        return std::nullopt;
    }
    if (*display_off_) {
        return false;
    }
    if (!display_start_page_.has_value()) {
        return std::nullopt;
    }

    const auto x = static_cast<std::uint8_t>(
        (*display_start_page_ + row / 8U) & maximum_x_address
    );
    const auto value = display_ram_[display_ram_index(x, y)];
    if (!value.has_value()) {
        return std::nullopt;
    }
    const auto bit = static_cast<std::uint8_t>(row & 0x07U);
    return ((*value >> bit) & 0x01U) != 0U;
}

}  // namespace jr800::core
