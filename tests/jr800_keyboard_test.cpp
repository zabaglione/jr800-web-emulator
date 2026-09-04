// SPDX-License-Identifier: MIT

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "jr800/core/jr800_bus.hpp"
#include "jr800/core/jr800_keyboard.hpp"
#include "jr800/core/jr800_machine.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

class RecordingObserver final : public jr800::core::BusObserver {
public:
    void on_bus_access(
        const jr800::core::BusAccessEvent& event
    ) noexcept override {
        if (count < events.size()) {
            events[count] = event;
            ++count;
        }
    }

    std::array<jr800::core::BusAccessEvent, 8U> events{};
    std::size_t count{};
};

}  // namespace

int main() {
    using jr800::core::AccessKind;
    using jr800::core::BusFault;
    using jr800::core::Jr800Bus;
    using jr800::core::Jr800Key;
    using jr800::core::Jr800Keyboard;
    using jr800::core::Jr800Machine;

    bool passed = true;

    Jr800Keyboard keyboard;
    passed &= expect(
        !keyboard.read8(0x0BFFU).handled
            && !keyboard.read8(0x1000U).handled
            && !keyboard.set_bus_response(0x0BFFU, 0x11U, true)
            && !keyboard.set_bus_response(0x1000U, 0x22U, true)
            && keyboard.activity().read_attempts == 0U
            && keyboard.activity().distinct_addresses == 0U,
        "Keyboard input accepted an address outside its selection window"
    );

    bool complete_window_matches_verified_boundary = true;
    for (std::uint32_t address = 0x0C00U; address <= 0x0FFFU; ++address) {
        const auto read = keyboard.read8(static_cast<std::uint16_t>(address));
        const auto verified_idle = address == 0x0DFFU
            || address == 0x0F7FU || address == 0x0FFEU;
        if (!read.handled
            || (verified_idle && read.value != 0xFFU)
            || (!verified_idle && read.value.has_value())) {
            complete_window_matches_verified_boundary = false;
            break;
        }
    }
    passed &= expect(
        complete_window_matches_verified_boundary
            && keyboard.activity().read_attempts == 0x0400U
            && keyboard.activity().distinct_addresses == 0x0400U,
        "Keyboard selection window did not preserve verified idle responses"
    );
    keyboard.clear_activity();
    passed &= expect(
        keyboard.activity().read_attempts == 0U
            && keyboard.activity().distinct_addresses == 0U,
        "Keyboard activity clear retained aggregate counts"
    );

    passed &= expect(
        keyboard.set_bus_response(0x0C00U, 0x12U, true)
            && keyboard.set_bus_response(0x0FFFU, 0xA5U, true)
            && keyboard.read8(0x0C00U).value == 0x12U
            && keyboard.read8(0x0FFFU).value == 0xA5U
            && !keyboard.read8(0x0C01U).value.has_value(),
        "Keyboard raw responses aliased or changed boundary values"
    );
    passed &= expect(
        keyboard.set_bus_response(0x0C00U, 0xFFU, false)
            && keyboard.read8(0x0C00U).handled
            && !keyboard.read8(0x0C00U).value.has_value()
            && keyboard.activity().read_attempts == 5U
            && keyboard.activity().distinct_addresses == 3U,
        "Keyboard response could not return to unknown state"
    );

    struct KeyCase {
        Jr800Key key;
        std::uint16_t address;
        std::uint8_t mask;
    };
    constexpr std::array key_cases{
        KeyCase{Jr800Key::shift, 0x0DFFU, 0x08U},
        KeyCase{Jr800Key::control, 0x0DFFU, 0x10U},
        KeyCase{Jr800Key::menu, 0x0DFFU, 0x04U},
        KeyCase{Jr800Key::return_key, 0x0F7FU, 0x40U},
        KeyCase{Jr800Key::letter_x, 0x0F7FU, 0x01U},
        KeyCase{Jr800Key::keypad_insert_rub, 0x0F7FU, 0x08U},
        KeyCase{Jr800Key::keypad_vertical_arrows, 0x0F7FU, 0x20U},
        KeyCase{Jr800Key::keypad_horizontal_arrows, 0x0F7FU, 0x10U},
        KeyCase{Jr800Key::keypad_0, 0x0FFEU, 0x01U},
        KeyCase{Jr800Key::keypad_1, 0x0FFEU, 0x02U},
        KeyCase{Jr800Key::keypad_2, 0x0FFEU, 0x04U},
        KeyCase{Jr800Key::keypad_3, 0x0FFEU, 0x08U},
        KeyCase{Jr800Key::keypad_4, 0x0FFEU, 0x10U},
        KeyCase{Jr800Key::keypad_5, 0x0FFEU, 0x20U},
        KeyCase{Jr800Key::keypad_6, 0x0FFEU, 0x40U},
        KeyCase{Jr800Key::keypad_7, 0x0FFEU, 0x80U},
        KeyCase{Jr800Key::break_key, 0x0F7FU, 0x80U},
        KeyCase{Jr800Key::home_cls, 0x0FF7U, 0x80U},
        KeyCase{Jr800Key::main_0, 0x0FFBU, 0x01U},
        KeyCase{Jr800Key::space, 0x0FEFU, 0x01U},
        KeyCase{Jr800Key::main_1, 0x0FFBU, 0x02U},
        KeyCase{Jr800Key::main_2, 0x0FFBU, 0x04U},
        KeyCase{Jr800Key::main_3, 0x0FFBU, 0x08U},
        KeyCase{Jr800Key::main_4, 0x0FFBU, 0x10U},
        KeyCase{Jr800Key::main_5, 0x0FFBU, 0x20U},
        KeyCase{Jr800Key::main_6, 0x0FFBU, 0x40U},
        KeyCase{Jr800Key::main_7, 0x0FFBU, 0x80U},
        KeyCase{Jr800Key::main_8, 0x0FF7U, 0x01U},
        KeyCase{Jr800Key::main_9, 0x0FF7U, 0x02U},
        KeyCase{Jr800Key::main_caret, 0x0FF7U, 0x20U},
        KeyCase{Jr800Key::letter_a, 0x0FEFU, 0x02U},
        KeyCase{Jr800Key::letter_b, 0x0FEFU, 0x04U},
        KeyCase{Jr800Key::letter_c, 0x0FEFU, 0x08U},
        KeyCase{Jr800Key::letter_d, 0x0FEFU, 0x10U},
        KeyCase{Jr800Key::letter_e, 0x0FEFU, 0x20U},
        KeyCase{Jr800Key::letter_f, 0x0FEFU, 0x40U},
        KeyCase{Jr800Key::letter_g, 0x0FEFU, 0x80U},
        KeyCase{Jr800Key::letter_h, 0x0FDFU, 0x01U},
        KeyCase{Jr800Key::letter_i, 0x0FDFU, 0x02U},
        KeyCase{Jr800Key::letter_j, 0x0FDFU, 0x04U},
        KeyCase{Jr800Key::letter_k, 0x0FDFU, 0x08U},
        KeyCase{Jr800Key::letter_l, 0x0FDFU, 0x10U},
        KeyCase{Jr800Key::letter_m, 0x0FDFU, 0x20U},
        KeyCase{Jr800Key::letter_n, 0x0FDFU, 0x40U},
        KeyCase{Jr800Key::letter_o, 0x0FDFU, 0x80U},
        KeyCase{Jr800Key::letter_p, 0x0FBFU, 0x01U},
        KeyCase{Jr800Key::letter_q, 0x0FBFU, 0x02U},
        KeyCase{Jr800Key::letter_r, 0x0FBFU, 0x04U},
        KeyCase{Jr800Key::letter_s, 0x0FBFU, 0x08U},
        KeyCase{Jr800Key::letter_t, 0x0FBFU, 0x10U},
        KeyCase{Jr800Key::letter_u, 0x0FBFU, 0x20U},
        KeyCase{Jr800Key::letter_v, 0x0FBFU, 0x40U},
        KeyCase{Jr800Key::letter_w, 0x0FBFU, 0x80U},
        KeyCase{Jr800Key::letter_y, 0x0F7FU, 0x02U},
        KeyCase{Jr800Key::letter_z, 0x0F7FU, 0x04U},
        KeyCase{Jr800Key::colon, 0x0FF7U, 0x04U},
        KeyCase{Jr800Key::semicolon, 0x0FF7U, 0x08U},
        KeyCase{Jr800Key::comma, 0x0FF7U, 0x10U},
        KeyCase{Jr800Key::period, 0x0FF7U, 0x40U},
        KeyCase{Jr800Key::pf_1, 0x0EFFU, 0x01U},
        KeyCase{Jr800Key::pf_2, 0x0EFFU, 0x02U},
        KeyCase{Jr800Key::pf_3, 0x0EFFU, 0x04U},
        KeyCase{Jr800Key::pf_4, 0x0EFFU, 0x08U},
        KeyCase{Jr800Key::pf_5, 0x0EFFU, 0x10U},
        KeyCase{Jr800Key::pf_6, 0x0EFFU, 0x20U},
        KeyCase{Jr800Key::pf_7, 0x0EFFU, 0x40U},
        KeyCase{Jr800Key::pf_8, 0x0EFFU, 0x80U},
        KeyCase{Jr800Key::pf_9, 0x0DFFU, 0x01U},
        KeyCase{Jr800Key::pf_10, 0x0DFFU, 0x02U},
        KeyCase{Jr800Key::keypad_8, 0x0FFDU, 0x01U},
        KeyCase{Jr800Key::keypad_9, 0x0FFDU, 0x02U},
        KeyCase{Jr800Key::keypad_multiply, 0x0FFDU, 0x04U},
        KeyCase{Jr800Key::keypad_add, 0x0FFDU, 0x08U},
        KeyCase{Jr800Key::keypad_equal, 0x0FFDU, 0x10U},
        KeyCase{Jr800Key::keypad_subtract, 0x0FFDU, 0x20U},
        KeyCase{Jr800Key::keypad_decimal, 0x0FFDU, 0x40U},
        KeyCase{Jr800Key::keypad_divide, 0x0FFDU, 0x80U},
    };
    static_assert(
        key_cases.size() == static_cast<std::size_t>(Jr800Key::count)
    );
    Jr800Keyboard mapped_keyboard;
    for (const auto& key_case : key_cases) {
        const auto verified_idle = key_case.address == 0x0DFFU
            || key_case.address == 0x0F7FU
            || key_case.address == 0x0FFEU;
        const auto before = mapped_keyboard.inspect8(key_case.address);
        bool key_passed = before.value.has_value() == verified_idle
            && mapped_keyboard.set_key_state(key_case.key, true);
        if (!verified_idle) {
            key_passed &= !mapped_keyboard.inspect8(key_case.address)
                .value.has_value();
            key_passed &= mapped_keyboard.set_bus_response(
                key_case.address,
                0xFFU,
                true
            );
        }
        key_passed &= mapped_keyboard.inspect8(key_case.address).value
            == static_cast<std::uint8_t>(0xFFU & ~key_case.mask);
        key_passed &= mapped_keyboard.set_key_state(key_case.key, false)
            && mapped_keyboard.inspect8(key_case.address).value == 0xFFU;
        if (!verified_idle) {
            key_passed &= mapped_keyboard.set_bus_response(
                key_case.address,
                0x00U,
                false
            );
            key_passed &= !mapped_keyboard.inspect8(key_case.address)
                .value.has_value();
        }
        passed &= expect(
            key_passed,
            "JR-800 key mapping changed its qualified active-low response"
        );
    }
    passed &= expect(
        mapped_keyboard.set_bus_response(0x0FEFU, 0xFFU, true)
            && mapped_keyboard.set_key_state(Jr800Key::shift, true)
            && mapped_keyboard.set_key_state(Jr800Key::letter_a, true)
            && mapped_keyboard.inspect8(0x0DFFU).value == 0xF7U
            && mapped_keyboard.inspect8(0x0FEFU).value == 0xFDU
            && mapped_keyboard.set_key_state(Jr800Key::shift, false)
            && mapped_keyboard.set_key_state(Jr800Key::letter_a, false)
            && mapped_keyboard.set_bus_response(0x0FEFU, 0x00U, false),
        "Cross-selection ROM-expected modifier input changed"
    );
    passed &= expect(
        mapped_keyboard.set_key_state(Jr800Key::shift, true)
            && mapped_keyboard.set_key_state(Jr800Key::control, true)
            && !mapped_keyboard.inspect8(0x0DFFU).value.has_value()
            && mapped_keyboard.set_key_state(Jr800Key::control, false)
            && mapped_keyboard.inspect8(0x0DFFU).value == 0xF7U
            && mapped_keyboard.set_key_state(Jr800Key::letter_x, true)
            && mapped_keyboard.inspect8(0x0F7FU).value == 0xFEU
            && mapped_keyboard.set_key_state(Jr800Key::shift, false)
            && mapped_keyboard.set_key_state(Jr800Key::letter_x, false),
        "Unobserved same-address combination or observed cross-address keys changed"
    );
    passed &= expect(
        mapped_keyboard.set_bus_response(0x0DFFU, 0xAAU, true)
            && mapped_keyboard.set_key_state(Jr800Key::shift, true)
            && mapped_keyboard.inspect8(0x0DFFU).value == 0xA2U
            && mapped_keyboard.set_key_state(Jr800Key::shift, false)
            && mapped_keyboard.inspect8(0x0DFFU).value == 0xAAU
            && mapped_keyboard.set_bus_response(0x0DFFU, 0x00U, false)
            && mapped_keyboard.inspect8(0x0DFFU).value == 0xFFU
            && !mapped_keyboard.set_key_state(
                static_cast<Jr800Key>(0xFFU),
                true
            ),
        "Raw base response or verified-idle fallback changed key semantics"
    );

    Jr800Bus bus;
    RecordingObserver observer;
    passed &= expect(
        bus.set_observer(&observer),
        "Keyboard bus observer attach failed"
    );
    const auto unknown = bus.read8(0x0C23U, AccessKind::data_read);
    passed &= expect(
        unknown.fault == BusFault::uninitialized_read
            && !unknown.value.has_value() && observer.count == 0U
            && bus.keyboard_activity().read_attempts == 1U
            && bus.keyboard_activity().distinct_addresses == 1U,
        "Unspecified keyboard response produced a byte or trace event"
    );

    passed &= expect(
        bus.set_keyboard_bus_response(0x0C23U, 0x5AU, true),
        "Known keyboard bus response was rejected"
    );
    const auto read = bus.read8(0x0C23U, AccessKind::data_read);
    const auto count_before_inspection = observer.count;
    const auto inspected = bus.inspect8(0x0C23U);
    const auto discarded = bus.read8_discard(0x0C23U);
    const auto write = bus.write8(0x0C23U, 0xA5U);
    passed &= expect(
        read.succeeded() && read.value == 0x5AU
            && inspected.succeeded() && inspected.value == 0x5AU
            && count_before_inspection == 1U && observer.count == 2U
            && discarded.succeeded()
            && write.fault == BusFault::unsupported_access
            && bus.keyboard_activity().read_attempts == 3U
            && bus.keyboard_activity().distinct_addresses == 1U,
        "Keyboard read, inspection, discarded read, or write boundary differs"
    );
    passed &= expect(
        observer.events[0].kind == AccessKind::data_read
            && observer.events[0].address == 0x0C23U
            && observer.events[0].value == 0x5AU
            && observer.events[1].kind == AccessKind::data_read
            && observer.events[1].address == 0x0C23U
            && observer.events[1].value == 0x5AU,
        "Keyboard reads did not emit one structured event each"
    );

    bus.reset_cpu_devices();
    const auto after_cpu_reset = bus.read8(0x0C23U, AccessKind::data_read);
    passed &= expect(
        after_cpu_reset.succeeded() && after_cpu_reset.value == 0x5AU
            && observer.count == 3U
            && bus.keyboard_activity().read_attempts == 1U
            && bus.keyboard_activity().distinct_addresses == 1U,
        "CPU-device reset changed external keyboard input"
    );
    passed &= expect(
        bus.set_keyboard_bus_response(0x0C23U, 0x00U, false),
        "Keyboard response clear was rejected"
    );
    const auto cleared = bus.read8(0x0C23U, AccessKind::data_read);
    passed &= expect(
        cleared.fault == BusFault::uninitialized_read
            && !cleared.value.has_value() && observer.count == 3U
            && bus.keyboard_activity().read_attempts == 2U
            && bus.keyboard_activity().distinct_addresses == 1U,
        "Cleared keyboard response exposed a fallback value"
    );

    Jr800Machine machine;
    passed &= expect(
        !machine.set_keyboard_bus_response(0x1000U, 0x33U, true)
            && machine.set_keyboard_bus_response(0x0C80U, 0x7EU, true),
        "JR-800 machine keyboard input validation differs"
    );
    const auto machine_read = machine.execution().inspect8(0x0C80U);
    passed &= expect(
        machine_read.succeeded() && machine_read.value == 0x7EU
            && machine.keyboard_activity().read_attempts == 0U
            && machine.keyboard_activity().distinct_addresses == 0U,
        "JR-800 machine did not forward keyboard input"
    );
    passed &= expect(
        machine.set_keyboard_key_state(Jr800Key::letter_x, true)
            && machine.execution().inspect8(0x0F7FU).value == 0xFEU
            && machine.set_keyboard_key_state(Jr800Key::letter_x, false)
            && machine.execution().inspect8(0x0F7FU).value == 0xFFU,
        "JR-800 machine did not forward verified physical key state"
    );

    return passed ? 0 : 1;
}
