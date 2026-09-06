// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>

namespace jr800::core {

struct Hd6301v1RamControlReadResult {
    bool handled{};
    std::optional<std::uint8_t> value;
};

struct Hd6301v1RamControlWriteResult {
    bool handled{};
    std::uint8_t previous_value{};
    bool previous_value_known{};
};

class Hd6301v1RamControl final {
public:
    void reset() noexcept;
    void set_standby_power_valid(bool value, bool known) noexcept;

    [[nodiscard]] Hd6301v1RamControlReadResult read8(
        std::uint16_t address
    ) const noexcept;
    [[nodiscard]] Hd6301v1RamControlWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] bool ram_enabled() const noexcept;
    [[nodiscard]] std::optional<bool> standby_power_valid() const noexcept;

private:
    friend class MachineStateCodec;
    static constexpr std::uint16_t register_address = 0x0014U;
    static constexpr std::uint8_t standby_mask = 0x80U;
    static constexpr std::uint8_t ram_enable_mask = 0x40U;
    static constexpr std::uint8_t unused_read_mask = 0x3FU;

    std::optional<bool> standby_power_valid_;
    bool ram_enabled_{true};
};

}  // namespace jr800::core
