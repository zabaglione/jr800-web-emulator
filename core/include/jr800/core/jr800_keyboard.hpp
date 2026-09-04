// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace jr800::core {

struct Jr800KeyboardReadResult {
    bool handled{};
    std::optional<std::uint8_t> value;
};

struct Jr800KeyboardActivity {
    std::uint64_t read_attempts{};
    std::uint64_t distinct_addresses{};

    bool operator==(const Jr800KeyboardActivity&) const = default;
};

enum class Jr800Key : std::uint8_t {
    shift,
    control,
    menu,
    return_key,
    space,
    main_1,
    letter_a,
    letter_x,
    keypad_insert_rub,
    keypad_vertical_arrows,
    keypad_horizontal_arrows,
    keypad_0,
    keypad_1,
    keypad_2,
    keypad_3,
    keypad_4,
    keypad_5,
    keypad_6,
    keypad_7,
    break_key,
    home_cls,
    main_0,
    main_2,
    main_3,
    main_4,
    main_5,
    main_6,
    main_7,
    main_8,
    main_9,
    main_caret,
    letter_b,
    letter_c,
    letter_d,
    letter_e,
    letter_f,
    letter_g,
    letter_h,
    letter_i,
    letter_j,
    letter_k,
    letter_l,
    letter_m,
    letter_n,
    letter_o,
    letter_p,
    letter_q,
    letter_r,
    letter_s,
    letter_t,
    letter_u,
    letter_v,
    letter_w,
    letter_y,
    letter_z,
    colon,
    semicolon,
    comma,
    period,
    pf_1,
    pf_2,
    pf_3,
    pf_4,
    pf_5,
    pf_6,
    pf_7,
    pf_8,
    pf_9,
    pf_10,
    keypad_8,
    keypad_9,
    keypad_multiply,
    keypad_add,
    keypad_equal,
    keypad_subtract,
    keypad_decimal,
    keypad_divide,
    count,
};

class Jr800Keyboard final {
public:
    [[nodiscard]] bool set_bus_response(
        std::uint16_t address,
        std::uint8_t value,
        bool known
    ) noexcept;
    [[nodiscard]] bool set_key_state(
        Jr800Key key,
        bool pressed
    ) noexcept;

    [[nodiscard]] Jr800KeyboardReadResult read8(
        std::uint16_t address
    ) noexcept;
    [[nodiscard]] Jr800KeyboardReadResult inspect8(
        std::uint16_t address
    ) const noexcept;

    void clear_activity() noexcept;
    [[nodiscard]] Jr800KeyboardActivity activity() const noexcept;

private:
    static constexpr std::uint16_t base_address = 0x0C00U;
    static constexpr std::size_t address_count = 0x0400U;

    std::array<std::uint8_t, address_count> values_{};
    std::array<bool, address_count> known_{};
    std::array<std::uint8_t, address_count> pressed_masks_{};
    std::array<bool, address_count> read_addresses_{};
    std::uint64_t read_attempts_{};
    std::uint64_t distinct_addresses_{};
};

}  // namespace jr800::core
