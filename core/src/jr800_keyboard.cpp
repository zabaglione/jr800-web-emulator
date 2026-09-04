// SPDX-License-Identifier: MIT

#include "jr800/core/jr800_keyboard.hpp"

namespace jr800::core {
namespace {

struct KeyMapping {
    std::uint16_t address;
    std::uint8_t mask;
};

constexpr std::array<KeyMapping, static_cast<std::size_t>(Jr800Key::count)>
    key_mappings{{
        {0x0DFFU, 0x08U},
        {0x0DFFU, 0x10U},
        {0x0DFFU, 0x04U},
        {0x0F7FU, 0x40U},
        {0x0FEFU, 0x01U},
        {0x0FFBU, 0x02U},
        {0x0FEFU, 0x02U},
        {0x0F7FU, 0x01U},
        {0x0F7FU, 0x08U},
        {0x0F7FU, 0x20U},
        {0x0F7FU, 0x10U},
        {0x0FFEU, 0x01U},
        {0x0FFEU, 0x02U},
        {0x0FFEU, 0x04U},
        {0x0FFEU, 0x08U},
        {0x0FFEU, 0x10U},
        {0x0FFEU, 0x20U},
        {0x0FFEU, 0x40U},
        {0x0FFEU, 0x80U},
        {0x0F7FU, 0x80U},
        {0x0FF7U, 0x80U},
        {0x0FFBU, 0x01U},
        {0x0FFBU, 0x04U},
        {0x0FFBU, 0x08U},
        {0x0FFBU, 0x10U},
        {0x0FFBU, 0x20U},
        {0x0FFBU, 0x40U},
        {0x0FFBU, 0x80U},
        {0x0FF7U, 0x01U},
        {0x0FF7U, 0x02U},
        {0x0FF7U, 0x20U},
        {0x0FEFU, 0x04U},
        {0x0FEFU, 0x08U},
        {0x0FEFU, 0x10U},
        {0x0FEFU, 0x20U},
        {0x0FEFU, 0x40U},
        {0x0FEFU, 0x80U},
        {0x0FDFU, 0x01U},
        {0x0FDFU, 0x02U},
        {0x0FDFU, 0x04U},
        {0x0FDFU, 0x08U},
        {0x0FDFU, 0x10U},
        {0x0FDFU, 0x20U},
        {0x0FDFU, 0x40U},
        {0x0FDFU, 0x80U},
        {0x0FBFU, 0x01U},
        {0x0FBFU, 0x02U},
        {0x0FBFU, 0x04U},
        {0x0FBFU, 0x08U},
        {0x0FBFU, 0x10U},
        {0x0FBFU, 0x20U},
        {0x0FBFU, 0x40U},
        {0x0FBFU, 0x80U},
        {0x0F7FU, 0x02U},
        {0x0F7FU, 0x04U},
        {0x0FF7U, 0x04U},
        {0x0FF7U, 0x08U},
        {0x0FF7U, 0x10U},
        {0x0FF7U, 0x40U},
        {0x0EFFU, 0x01U},
        {0x0EFFU, 0x02U},
        {0x0EFFU, 0x04U},
        {0x0EFFU, 0x08U},
        {0x0EFFU, 0x10U},
        {0x0EFFU, 0x20U},
        {0x0EFFU, 0x40U},
        {0x0EFFU, 0x80U},
        {0x0DFFU, 0x01U},
        {0x0DFFU, 0x02U},
        {0x0FFDU, 0x01U},
        {0x0FFDU, 0x02U},
        {0x0FFDU, 0x04U},
        {0x0FFDU, 0x08U},
        {0x0FFDU, 0x10U},
        {0x0FFDU, 0x20U},
        {0x0FFDU, 0x40U},
        {0x0FFDU, 0x80U},
    }};

constexpr bool has_verified_idle_response(std::uint16_t address) noexcept {
    return address == 0x0DFFU || address == 0x0F7FU
        || address == 0x0FFEU;
}

}  // namespace

bool Jr800Keyboard::set_bus_response(
    std::uint16_t address,
    std::uint8_t value,
    bool known
) noexcept {
    if (address < base_address) {
        return false;
    }
    const auto index = static_cast<std::size_t>(address - base_address);
    if (index >= address_count) {
        return false;
    }
    values_[index] = value;
    known_[index] = known;
    return true;
}

bool Jr800Keyboard::set_key_state(
    Jr800Key key,
    bool pressed
) noexcept {
    const auto key_index = static_cast<std::size_t>(key);
    if (key_index >= key_mappings.size()) {
        return false;
    }
    const auto mapping = key_mappings[key_index];
    const auto address_index = static_cast<std::size_t>(
        mapping.address - base_address
    );
    if (pressed) {
        pressed_masks_[address_index] = static_cast<std::uint8_t>(
            pressed_masks_[address_index] | mapping.mask
        );
    } else {
        pressed_masks_[address_index] = static_cast<std::uint8_t>(
            pressed_masks_[address_index]
            & static_cast<std::uint8_t>(~mapping.mask)
        );
    }
    return true;
}

Jr800KeyboardReadResult Jr800Keyboard::read8(
    std::uint16_t address
) noexcept {
    const auto result = inspect8(address);
    if (!result.handled) {
        return result;
    }
    const auto index = static_cast<std::size_t>(address - base_address);
    ++read_attempts_;
    if (!read_addresses_[index]) {
        read_addresses_[index] = true;
        ++distinct_addresses_;
    }
    return result;
}

Jr800KeyboardReadResult Jr800Keyboard::inspect8(
    std::uint16_t address
) const noexcept {
    if (address < base_address) {
        return {};
    }
    const auto index = static_cast<std::size_t>(address - base_address);
    if (index >= address_count) {
        return {};
    }
    if (!known_[index] && !has_verified_idle_response(address)) {
        return {true, std::nullopt};
    }
    const auto pressed_mask = pressed_masks_[index];
    if (pressed_mask != 0U
        && (pressed_mask
            & static_cast<std::uint8_t>(pressed_mask - 1U)) != 0U) {
        return {true, std::nullopt};
    }
    const auto base_value = known_[index] ? values_[index] : 0xFFU;
    return {
        true,
        static_cast<std::uint8_t>(
            base_value & static_cast<std::uint8_t>(~pressed_mask)
        ),
    };
}

void Jr800Keyboard::clear_activity() noexcept {
    read_addresses_.fill(false);
    read_attempts_ = 0U;
    distinct_addresses_ = 0U;
}

Jr800KeyboardActivity Jr800Keyboard::activity() const noexcept {
    return {read_attempts_, distinct_addresses_};
}

}  // namespace jr800::core
