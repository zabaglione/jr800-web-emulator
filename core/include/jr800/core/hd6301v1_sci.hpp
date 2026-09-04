// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>

namespace jr800::core {

struct Hd6301v1SciReadResult {
    bool handled{};
    std::optional<std::uint8_t> value;
};

struct Hd6301v1SciWriteResult {
    bool handled{};
    std::uint8_t previous_value{};
    bool previous_value_known{};
};

struct Hd6301v1SciInterruptRequest {
    bool asserted{};
    bool known{true};
};

class Hd6301v1Sci final {
public:
    void reset() noexcept;
    void advance_cycles(std::uint32_t cycles) noexcept;
    void set_receive_pin_state(bool high, bool known) noexcept;

    [[nodiscard]] Hd6301v1SciReadResult read8(
        std::uint16_t address
    ) const noexcept;
    [[nodiscard]] Hd6301v1SciWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] std::uint8_t rate_mode_bits() const noexcept;
    [[nodiscard]] std::uint8_t control_bits() const noexcept;
    [[nodiscard]] Hd6301v1SciInterruptRequest interrupt_request()
        const noexcept;

private:
    static constexpr std::uint16_t rate_mode_address = 0x0010U;
    static constexpr std::uint16_t control_status_address = 0x0011U;
    static constexpr std::uint8_t writable_rate_mode_mask = 0x0FU;
    static constexpr std::uint8_t writable_control_mask = 0x1FU;
    static constexpr std::uint8_t wake_up_mask = 0x01U;
    static constexpr std::uint8_t receive_enable_mask = 0x08U;
    static constexpr std::uint8_t transmit_interrupt_enable_mask = 0x04U;
    static constexpr std::uint8_t receive_interrupt_enable_mask = 0x10U;
    static constexpr std::uint8_t transmit_data_register_empty_mask = 0x20U;
    static constexpr std::uint32_t minimum_receive_status_latency_cycles =
        160U;
    static constexpr std::uint32_t minimum_wakeup_clear_latency_cycles =
        160U;

    void schedule_status_uncertainty(std::uint32_t cycles) noexcept;

    std::uint8_t rate_mode_bits_{};
    std::uint8_t control_bits_{};
    bool receive_status_known_{true};
    std::uint32_t status_known_cycles_remaining_{};
    bool receive_pin_high_{};
    bool receive_pin_known_{};
};

}  // namespace jr800::core
