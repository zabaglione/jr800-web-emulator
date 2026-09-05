// SPDX-License-Identifier: MIT

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>
#include <vector>

#include "jr800/core/rp5c01_register_file.hpp"
#include "jr800/wasm/api.h"

namespace {
bool expect(bool condition, std::string_view message) {
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}
unsigned decimal(const jr800::core::Rp5c01RegisterFile& rtc, std::uint8_t at) {
    return rtc.read(at).value.value_or(255U)
        + 10U * rtc.read(static_cast<std::uint8_t>(at + 1U)).value.value_or(255U);
}
}

int main() {
    using jr800::core::CalendarDateTime;
    using jr800::core::Rp5c01RegisterFile;
    using jr800::core::Rp5c01RegisterStatus;
    bool passed = true;
    Rp5c01RegisterFile rtc;
    rtc.initialize_zero();
    passed &= expect(rtc.set_datetime({2026U, 9U, 5U, 12U, 34U, 56U})
        && rtc.read(6U).value == 6U && decimal(rtc, 11U) == 26U
        && decimal(rtc, 9U) == 9U && decimal(rtc, 7U) == 5U
        && decimal(rtc, 4U) == 12U && decimal(rtc, 2U) == 34U
        && decimal(rtc, 0U) == 56U, "Local date/time or Saturday differs");
    passed &= expect(rtc.write(13U, 9U).succeeded()
        && rtc.read(10U).value == 1U && rtc.read(11U).value == 2U,
        "24-hour selection or independent leap counter differs");
    passed &= expect(rtc.write(13U, 8U).succeeded(), "Time bank unavailable");
    passed &= expect(rtc.advance_oscillator_ticks(32'767U) == Rp5c01RegisterStatus::ok
        && decimal(rtc, 0U) == 56U
        && rtc.advance_oscillator_ticks(1U) == Rp5c01RegisterStatus::ok
        && decimal(rtc, 0U) == 57U, "Seed did not start a full new second");

    for (std::uint16_t year = 2000U; year <= 2099U; ++year) {
        const bool leap = year % 4U == 0U;
        passed &= expect(rtc.set_datetime({year, 2U, 28U, 23U, 59U, 59U})
            && rtc.advance_one_second() == Rp5c01RegisterStatus::ok
            && decimal(rtc, 7U) == (leap ? 29U : 1U)
            && decimal(rtc, 9U) == (leap ? 2U : 3U),
            "February carry differs within 2000-2099");
    }
    passed &= expect(rtc.set_datetime({2027U, 12U, 31U, 23U, 59U, 59U})
        && rtc.advance_one_second() == Rp5c01RegisterStatus::ok
        && decimal(rtc, 11U) == 28U && decimal(rtc, 9U) == 1U
        && decimal(rtc, 7U) == 1U && rtc.read(6U).value == 6U
        && rtc.write(13U, 9U).succeeded() && rtc.read(11U).value == 0U,
        "New year, weekday and leap counter did not carry together");

    passed &= expect(rtc.set_datetime({2026U, 9U, 5U, 12U, 34U, 56U}), "Reseed failed");
    for (const auto invalid : std::array{
            CalendarDateTime{1999U, 12U, 31U, 0U, 0U, 0U},
            CalendarDateTime{2100U, 1U, 1U, 0U, 0U, 0U},
            CalendarDateTime{2026U, 2U, 29U, 0U, 0U, 0U},
            CalendarDateTime{2026U, 0U, 5U, 0U, 0U, 0U},
            CalendarDateTime{2026U, 9U, 5U, 24U, 0U, 0U},
            CalendarDateTime{2026U, 9U, 5U, 0U, 60U, 0U},
            CalendarDateTime{2026U, 9U, 5U, 0U, 0U, 60U}}) {
        passed &= expect(!rtc.set_datetime(invalid)
            && decimal(rtc, 0U) == 56U && decimal(rtc, 4U) == 12U
            && decimal(rtc, 9U) == 9U && rtc.read(6U).value == 6U,
            "Invalid host date changed clock state");
    }
    // DATE$ writes only the date digits; DAY$ and leap state remain independent.
    passed &= expect(rtc.write(6U, 1U).succeeded()
        && rtc.write(11U, 8U).succeeded() && rtc.read(6U).value == 1U
        && rtc.write(13U, 9U).succeeded() && rtc.read(11U).value == 2U,
        "Standard weekday or leap semantics were replaced by automatic correction");

    const jr800_calendar_datetime input{2026U, 9U, 5U, 12U, 34U, 56U};
    jr800_hardware_configuration config{};
    config.abi_version = JR800_WASM_ABI_VERSION;
    auto* detached = jr800_machine_create_jr800(&config);
    auto* synthetic = jr800_machine_create();
    passed &= expect(jr800_machine_set_calendar_datetime(nullptr, &input) == JR800_STATUS_INVALID_ARGUMENT
        && jr800_machine_set_calendar_datetime(detached, nullptr) == JR800_STATUS_INVALID_ARGUMENT
        && jr800_machine_set_calendar_datetime(detached, &input) == JR800_STATUS_UNSUPPORTED_ACCESS
        && jr800_machine_set_calendar_datetime(synthetic, &input) == JR800_STATUS_WRONG_MACHINE_KIND,
        "C ABI accepted a missing or wrong calendar session");
    jr800_machine_destroy(detached);
    jr800_machine_destroy(synthetic);
    config.calendar_enabled = 1U;
    auto* machine = jr800_machine_create_jr800(&config);
    std::vector<std::uint8_t> rom(32'768U, 1U);  // Project-authored NOP fixture.
    rom[32'766U] = 0x80U;
    rom[32'767U] = 0U;
    jr800_machine_state before{}, after{};
    passed &= expect(machine != nullptr
        && jr800_machine_load_logical_rom(machine, rom.data(), 32'768U) == JR800_STATUS_OK
        && jr800_machine_get_state(machine, &before) == JR800_STATUS_OK
        && jr800_machine_set_calendar_datetime(machine, &input) == JR800_STATUS_OK
        && jr800_machine_get_state(machine, &after) == JR800_STATUS_OK
        && std::memcmp(&before, &after, sizeof(before)) == 0,
        "C ABI calendar initialization changed CPU or machine diagnostic state");
    jr800_machine_destroy(machine);
    return passed ? 0 : 1;
}
