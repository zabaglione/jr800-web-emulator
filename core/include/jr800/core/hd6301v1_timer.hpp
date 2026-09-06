// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>

namespace jr800::core {

struct Hd6301v1TimerReadResult {
    bool handled{};
    std::optional<std::uint8_t> value;
};

struct Hd6301v1TimerWriteResult {
    bool handled{};
    std::uint8_t previous_value{};
    bool previous_value_known{};
};

enum class Hd6301v1TimerInterruptSource : std::uint8_t {
    input_capture,
    output_compare,
    overflow,
};

struct Hd6301v1TimerInterruptRequest {
    std::optional<Hd6301v1TimerInterruptSource> source;
    bool known{true};

    [[nodiscard]] bool asserted() const noexcept {
        return known && source.has_value();
    }
};

class Hd6301v1Timer final {
public:
    void reset() noexcept;
    void advance_cycles(std::uint32_t cycles) noexcept;
    void set_input_capture_pin_state(bool value, bool known) noexcept;
    void set_input_capture_enabled(bool enabled) noexcept;

    [[nodiscard]] Hd6301v1TimerReadResult read8(
        std::uint16_t address
    ) noexcept;

    [[nodiscard]] Hd6301v1TimerReadResult inspect8(
        std::uint16_t address
    ) const noexcept;

    [[nodiscard]] Hd6301v1TimerWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] std::uint8_t control_bits() const noexcept;
    [[nodiscard]] std::uint8_t status_bits() const noexcept;
    [[nodiscard]] bool status_bits_known() const noexcept;
    [[nodiscard]] std::uint16_t free_running_counter() const noexcept;
    [[nodiscard]] bool free_running_counter_high_write_pending() const noexcept;
    [[nodiscard]] std::optional<std::uint16_t> input_capture() const noexcept;
    [[nodiscard]] std::optional<bool> output_compare_level() const noexcept;
    [[nodiscard]] Hd6301v1TimerInterruptRequest interrupt_request()
        const noexcept;

private:
    friend class MachineStateCodec;
    static constexpr std::uint16_t control_status_address = 0x0008U;
    static constexpr std::uint16_t free_running_counter_high_address = 0x0009U;
    static constexpr std::uint16_t free_running_counter_low_address = 0x000AU;
    static constexpr std::uint16_t free_running_counter_write_preset = 0xFFF8U;
    static constexpr std::uint16_t output_compare_high_address = 0x000BU;
    static constexpr std::uint16_t output_compare_low_address = 0x000CU;
    static constexpr std::uint16_t output_compare_reset_value = 0xFFFFU;
    static constexpr std::uint16_t input_capture_high_address = 0x000DU;
    static constexpr std::uint16_t input_capture_low_address = 0x000EU;
    static constexpr std::uint8_t writable_control_mask = 0x1FU;
    static constexpr std::uint8_t input_edge_mask = 0x02U;
    static constexpr std::uint8_t overflow_interrupt_enable_mask = 0x04U;
    static constexpr std::uint8_t output_compare_interrupt_enable_mask = 0x08U;
    static constexpr std::uint8_t input_capture_interrupt_enable_mask = 0x10U;
    static constexpr std::uint8_t overflow_flag_mask = 0x20U;
    static constexpr std::uint8_t output_compare_flag_mask = 0x40U;
    static constexpr std::uint8_t input_capture_flag_mask = 0x80U;
    static constexpr std::uint8_t comparison_inhibit_cycles_after_write = 2U;

    std::uint8_t control_bits_{};
    std::uint8_t status_bits_{};
    std::uint16_t free_running_counter_{};
    std::uint8_t pending_free_running_counter_high_{};
    bool free_running_counter_high_write_pending_{};
    std::uint16_t output_compare_{output_compare_reset_value};
    std::optional<bool> output_compare_level_;
    std::optional<std::uint16_t> input_capture_;
    bool overflow_clear_armed_{};
    bool output_compare_clear_armed_{};
    bool input_capture_clear_armed_{};
    bool input_capture_pin_value_{};
    bool input_capture_pin_known_{};
    bool input_capture_enabled_{true};
    bool input_capture_status_known_{true};
    std::uint8_t comparison_inhibit_cycles_{};
};

}  // namespace jr800::core
