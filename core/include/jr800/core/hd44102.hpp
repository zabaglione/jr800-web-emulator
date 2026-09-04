// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace jr800::core {

enum class Hd44102OperationStatus : std::uint8_t {
    ok,
    busy,
    reset_asserted,
    unknown_state,
    unsupported_instruction,
    no_pending_instruction,
};

struct Hd44102DisplayDataRead {
    Hd44102OperationStatus status{Hd44102OperationStatus::unknown_state};
    std::optional<std::uint8_t> value;
};

struct Hd44102StatusRead {
    std::uint8_t value{};
    std::uint8_t known_mask{};

    [[nodiscard]] bool fully_known() const noexcept {
        return known_mask == 0xFFU;
    }
};

class Hd44102 final {
public:
    void set_reset_line(bool asserted) noexcept;

    [[nodiscard]] Hd44102StatusRead read_status() const noexcept;

    [[nodiscard]] Hd44102OperationStatus write_control(
        std::uint8_t instruction
    ) noexcept;

    [[nodiscard]] Hd44102OperationStatus write_display_data(
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] Hd44102DisplayDataRead read_display_data() noexcept;

    [[nodiscard]] Hd44102OperationStatus complete_busy_period() noexcept;

    [[nodiscard]] std::optional<std::uint8_t> display_start_page()
        const noexcept;
    [[nodiscard]] std::optional<std::uint8_t> x_address() const noexcept;
    [[nodiscard]] std::optional<std::uint8_t> y_address() const noexcept;
    [[nodiscard]] std::optional<std::uint8_t> display_ram_value(
        std::uint8_t x,
        std::uint8_t y
    ) const noexcept;
    [[nodiscard]] std::optional<bool> display_dot_1_32(
        std::uint8_t y,
        std::uint8_t row
    ) const noexcept;

private:
    struct PendingDisplayWrite {
        std::uint8_t x{};
        std::uint8_t y{};
        std::uint8_t next_y{};
        std::uint8_t value{};
    };

    struct PendingDisplayRead {
        std::uint8_t x{};
        std::uint8_t y{};
        std::uint8_t next_y{};
    };

    enum class PendingOperation : std::uint8_t {
        none,
        count_direction,
        display_on_off,
        display_start_page,
        set_address,
        read_display_data,
        write_display_data,
    };

    std::optional<bool> reset_asserted_;
    std::optional<bool> busy_;
    std::optional<bool> count_up_;
    std::optional<bool> display_off_;
    std::optional<std::uint8_t> display_start_page_;
    std::optional<std::uint8_t> x_address_;
    std::optional<std::uint8_t> y_address_;
    std::optional<std::uint8_t> output_register_;
    std::array<std::optional<std::uint8_t>, 200U> display_ram_{};
    PendingOperation pending_operation_{PendingOperation::none};
    std::uint8_t pending_value_{};
    std::optional<PendingDisplayRead> pending_display_read_;
    std::optional<PendingDisplayWrite> pending_display_write_;
};

}  // namespace jr800::core
