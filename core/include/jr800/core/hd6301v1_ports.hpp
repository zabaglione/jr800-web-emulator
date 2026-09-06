// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>

namespace jr800::core {

struct Hd6301v1PortWriteResult {
    bool handled{};
    std::uint8_t previous_value{};
    bool previous_value_known{};
};

struct Hd6301v1PortReadResult {
    bool handled{};
    std::optional<std::uint8_t> value;
};

struct Hd6301v1Port2TimerOutputState {
    bool output_enabled{};
    std::optional<bool> level;

    bool operator==(const Hd6301v1Port2TimerOutputState&) const = default;
};

class Hd6301v1Ports final {
public:
    void reset() noexcept;

    void set_port1_pin_state(
        std::uint8_t value,
        std::uint8_t known_mask
    ) noexcept;
    void set_port2_pin_state(
        std::uint8_t value,
        std::uint8_t known_mask
    ) noexcept;

    [[nodiscard]] Hd6301v1PortReadResult read8(
        std::uint16_t address
    ) const noexcept;

    [[nodiscard]] Hd6301v1PortWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] std::uint8_t port1_data_direction() const noexcept;
    [[nodiscard]] std::optional<std::uint8_t> port1_data() const noexcept;
    [[nodiscard]] std::uint8_t port2_data_direction() const noexcept;
    [[nodiscard]] std::optional<std::uint8_t> port2_data_latch() const noexcept;
    [[nodiscard]] Hd6301v1Port2TimerOutputState port2_timer_output_state(
        std::optional<bool> timer_level
    ) const noexcept;
    [[nodiscard]] std::uint8_t port4_data_direction() const noexcept;

private:
    friend class MachineStateCodec;
    static constexpr std::uint16_t port1_data_direction_address = 0x0000U;
    static constexpr std::uint16_t port2_data_direction_address = 0x0001U;
    static constexpr std::uint16_t port1_data_address = 0x0002U;
    static constexpr std::uint16_t port2_data_address = 0x0003U;
    static constexpr std::uint16_t port4_data_direction_address = 0x0005U;
    static constexpr std::uint8_t port2_data_mask = 0x1FU;
    static constexpr std::uint8_t port2_data_direction_mask = 0x1FU;
    static constexpr std::uint8_t port2_mode_bits = 0xC0U;

    std::uint8_t port1_data_direction_{};
    std::optional<std::uint8_t> port1_data_;
    std::uint8_t port2_data_direction_{};
    std::optional<std::uint8_t> port2_data_latch_;
    std::uint8_t port4_data_direction_{};
    std::uint8_t port1_pin_value_{};
    std::uint8_t port1_pin_known_mask_{};
    std::uint8_t port2_pin_value_{};
    std::uint8_t port2_pin_known_mask_{};
};

}  // namespace jr800::core
