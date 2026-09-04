// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace jr800::core {

enum class Rp5c01RegisterStatus : std::uint8_t {
    ok,
    unknown_state,
    unsupported_operation,
    invalid_address,
};

struct Rp5c01RegisterReadResult {
    Rp5c01RegisterStatus status{Rp5c01RegisterStatus::invalid_address};
    std::optional<std::uint8_t> value;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == Rp5c01RegisterStatus::ok && value.has_value();
    }
};

struct Rp5c01RegisterWriteResult {
    Rp5c01RegisterStatus status{Rp5c01RegisterStatus::invalid_address};
    std::uint8_t previous_value{};
    bool previous_value_known{};

    [[nodiscard]] bool succeeded() const noexcept {
        return status == Rp5c01RegisterStatus::ok;
    }
};

class Rp5c01RegisterFile final {
public:
    // Explicit experiment state; not a modeled hardware reset operation.
    void initialize_zero() noexcept;

    [[nodiscard]] Rp5c01RegisterReadResult read(
        std::uint8_t address
    ) const noexcept;

    [[nodiscard]] Rp5c01RegisterWriteResult write(
        std::uint8_t address,
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] Rp5c01RegisterStatus advance_one_second() noexcept;
    [[nodiscard]] Rp5c01RegisterStatus advance_oscillator_ticks(
        std::uint32_t ticks
    ) noexcept;
    [[nodiscard]] Rp5c01RegisterStatus adjust_seconds() noexcept;

    [[nodiscard]] std::optional<std::uint8_t> mode_register() const noexcept;
    [[nodiscard]] std::optional<bool>
    clock_16hz_output_enabled() const noexcept;
    [[nodiscard]] std::optional<bool>
    clock_1hz_output_enabled() const noexcept;
    [[nodiscard]] std::optional<bool>
    divider_16hz_signal() const noexcept;
    [[nodiscard]] std::optional<bool>
    divider_1hz_signal() const noexcept;
    [[nodiscard]] std::optional<bool>
    clock_16hz_gate_output() const noexcept;
    [[nodiscard]] std::optional<bool>
    clock_1hz_gate_output() const noexcept;
    [[nodiscard]] std::optional<bool> clock_hold_pending() const noexcept;
    [[nodiscard]] std::optional<bool>
    alarm_comparator_output() const noexcept;
    [[nodiscard]] std::optional<bool>
    alarm_comparator_gate_output() const noexcept;
    [[nodiscard]] std::optional<bool>
    alarm_terminal_pull_low() const noexcept;

private:
    static constexpr std::uint8_t register_count = 0x10U;
    static constexpr std::uint8_t bank_register_count = 0x0DU;
    static constexpr std::uint8_t mode_register_address = 0x0DU;
    static constexpr std::uint8_t test_register_address = 0x0EU;
    static constexpr std::uint8_t reset_control_address = 0x0FU;
    static constexpr std::uint8_t first_alarm_register_address = 0x02U;
    static constexpr std::uint8_t last_alarm_register_address = 0x08U;
    static constexpr std::uint8_t data_mask = 0x0FU;
    static constexpr std::uint8_t mode_mask = 0x03U;
    static constexpr std::uint8_t clock_hold_safe_read_ticks = 4U;

    using RegisterBank = std::array<
        std::optional<std::uint8_t>,
        bank_register_count
    >;
    using AlarmComparisonState = std::array<std::optional<bool>, 4U>;

    [[nodiscard]] static std::uint8_t writable_mask(
        std::uint8_t mode,
        std::uint8_t address
    ) noexcept;
    [[nodiscard]] static std::uint8_t maximum_value(
        std::uint8_t mode,
        std::uint8_t address
    ) noexcept;
    [[nodiscard]] Rp5c01RegisterStatus
    advance_running_one_second() noexcept;

    [[nodiscard]] const RegisterBank& bank(std::uint8_t mode) const noexcept;
    [[nodiscard]] RegisterBank& bank(std::uint8_t mode) noexcept;

    RegisterBank time_registers_{};
    RegisterBank alarm_registers_{};
    AlarmComparisonState alarm_comparison_enabled_{};
    RegisterBank ram_block_10_{};
    RegisterBank ram_block_11_{};
    std::optional<std::uint8_t> mode_register_;
    std::optional<bool> clock_16hz_output_enabled_;
    std::optional<bool> clock_1hz_output_enabled_;
    std::optional<bool> clock_hold_pending_;
    std::uint8_t clock_hold_read_guard_ticks_{};
    std::optional<std::uint32_t> oscillator_divider_ticks_;
};

}  // namespace jr800::core
