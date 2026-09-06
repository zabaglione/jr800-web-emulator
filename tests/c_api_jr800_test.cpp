// SPDX-License-Identifier: MIT

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "jr800/formats/jr8rom.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/wasm/api.h"

namespace {

constexpr std::size_t logical_rom_size = 32U * 1024U;
constexpr std::uint32_t wav_sample_rate = 48'000U;
constexpr std::int16_t wav_amplitude = 12'000;

static_assert(JR800_CALENDAR_ALARM_TERMINAL_DISCONNECTED == 0);
static_assert(JR800_CALENDAR_ALARM_TERMINAL_UNKNOWN == 1);
static_assert(JR800_CALENDAR_ALARM_TERMINAL_RELEASED == 2);
static_assert(JR800_CALENDAR_ALARM_TERMINAL_PULL_LOW == 3);
static_assert(JR800_PORT2_TIMER_OUTPUT_UNAVAILABLE == 0);
static_assert(JR800_PORT2_TIMER_OUTPUT_DISABLED == 1);
static_assert(JR800_PORT2_TIMER_OUTPUT_UNKNOWN == 2);
static_assert(JR800_PORT2_TIMER_OUTPUT_LOW == 3);
static_assert(JR800_PORT2_TIMER_OUTPUT_HIGH == 4);

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

std::vector<std::uint8_t> make_rom(
    std::span<const std::uint8_t> program
) {
    std::vector<std::uint8_t> rom(logical_rom_size, 0x01U);
    std::copy(program.begin(), program.end(), rom.begin());
    rom[rom.size() - 2U] = 0x80U;
    rom[rom.size() - 1U] = 0x00U;
    return rom;
}

std::vector<std::uint8_t> make_jr8rom(
    const std::vector<std::uint8_t>& rom,
    bool split
) {
    jr800::formats::jr8rom::Image image;
    if (split) {
        const auto middle = rom.begin()
            + static_cast<std::ptrdiff_t>(rom.size() / 2U);
        image.segments = {
            {
                0xC000U,
                std::vector<std::uint8_t>(middle, rom.end()),
            },
            {
                0x8000U,
                std::vector<std::uint8_t>(rom.begin(), middle),
            },
        };
    } else {
        image.segments = {{0x8000U, rom}};
    }
    image.integrity_sha256 = jr800::formats::jr8rom::compute_integrity(image);
    return jr800::formats::jr8rom::write(image);
}

std::vector<std::uint8_t> make_jr8app(
    std::uint16_t address,
    std::uint16_t entry_point,
    std::span<const std::uint8_t> program,
    std::string_view target_profile = "hd6301v1"
) {
    jr800::formats::jr8app::Application application;
    application.target_profile = target_profile;
    application.entry_point = entry_point;
    application.segments = {{
        jr800::formats::jr8app::SegmentKind::data,
        address,
        static_cast<std::uint32_t>(program.size()),
        std::vector<std::uint8_t>{program.begin(), program.end()},
    }};
    application.integrity_sha256 =
        jr800::formats::jr8app::compute_integrity(application);
    return jr800::formats::jr8app::write(application);
}

void append_le16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_le32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    append_le16(bytes, static_cast<std::uint16_t>(value & 0xFFFFU));
    append_le16(bytes, static_cast<std::uint16_t>((value >> 16U) & 0xFFFFU));
}

std::uint16_t additive_sum(std::span<const std::uint8_t> bytes) {
    std::uint32_t sum = 0U;
    for (const auto byte : bytes) {
        sum += byte;
    }
    return static_cast<std::uint16_t>(sum & 0xFFFFU);
}

void append_wav_cycle(std::vector<std::int16_t>& samples, bool long_period) {
    const std::size_t half_period = long_period ? 21U : 11U;
    samples.insert(samples.end(), half_period, wav_amplitude);
    samples.insert(
        samples.end(),
        half_period,
        static_cast<std::int16_t>(-wav_amplitude)
    );
}

void append_wav_cycles(
    std::vector<std::int16_t>& samples,
    bool long_period,
    std::size_t count
) {
    for (std::size_t index = 0U; index < count; ++index) {
        append_wav_cycle(samples, long_period);
    }
}

void append_wav_byte(
    std::vector<std::int16_t>& samples,
    std::uint8_t value
) {
    for (int bit = 7; bit >= 0; --bit) {
        append_wav_cycle(samples, ((value >> bit) & 1U) != 0U);
    }
    append_wav_cycle(samples, true);
}

void append_wav_block(
    std::vector<std::int16_t>& samples,
    std::span<const std::uint8_t> bytes,
    std::size_t long_sync_cycles,
    std::size_t short_sync_cycles
) {
    append_wav_cycles(samples, false, 4'000U);
    append_wav_cycles(samples, true, long_sync_cycles);
    append_wav_cycles(samples, false, short_sync_cycles);
    append_wav_cycles(samples, true, 2U);
    for (const auto byte : bytes) {
        append_wav_byte(samples, byte);
    }
}

std::vector<std::uint8_t> make_native_program_wav(
    std::uint16_t address,
    std::uint16_t entry_point,
    std::span<const std::uint8_t> program
) {
    std::vector<std::uint8_t> header(34U, 0U);
    header[0] = 0x01U;
    constexpr std::string_view filename = "CAPIWAV";
    std::copy(filename.begin(), filename.end(), header.begin() + 1);
    const auto write_word = [&](std::size_t offset, std::uint16_t value) {
        header[offset] = static_cast<std::uint8_t>(value & 0xFFU);
        header[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    };
    write_word(17U, static_cast<std::uint16_t>(program.size()));
    write_word(19U, address);
    write_word(21U, entry_point);
    const auto header_sum = additive_sum(
        std::span<const std::uint8_t>{header}.first(32U)
    );
    header[32] = static_cast<std::uint8_t>(header_sum >> 8U);
    header[33] = static_cast<std::uint8_t>(header_sum & 0xFFU);

    std::vector<std::uint8_t> data(program.begin(), program.end());
    const auto data_sum = additive_sum(program);
    data.push_back(static_cast<std::uint8_t>(data_sum >> 8U));
    data.push_back(static_cast<std::uint8_t>(data_sum & 0xFFU));

    std::vector<std::int16_t> samples(wav_sample_rate / 10U, 0);
    append_wav_block(samples, header, 40U, 40U);
    samples.insert(samples.end(), wav_sample_rate / 500U, 0);
    append_wav_block(samples, data, 20U, 20U);
    samples.insert(samples.end(), wav_sample_rate / 10U, 0);

    const auto data_size = static_cast<std::uint32_t>(samples.size() * 2U);
    std::vector<std::uint8_t> wav;
    wav.reserve(44U + data_size);
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    append_le32(wav, 36U + data_size);
    wav.insert(wav.end(), {'W', 'A', 'V', 'E'});
    wav.insert(wav.end(), {'f', 'm', 't', ' '});
    append_le32(wav, 16U);
    append_le16(wav, 1U);
    append_le16(wav, 1U);
    append_le32(wav, wav_sample_rate);
    append_le32(wav, wav_sample_rate * 2U);
    append_le16(wav, 2U);
    append_le16(wav, 16U);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    append_le32(wav, data_size);
    for (const auto sample : samples) {
        append_le16(wav, static_cast<std::uint16_t>(sample));
    }
    return wav;
}

std::uint64_t combine(std::uint32_t low, std::uint32_t high) noexcept {
    return static_cast<std::uint64_t>(low)
        | (static_cast<std::uint64_t>(high) << 32U);
}

}  // namespace

int main() {
    bool passed = true;

    {
        jr800_hardware_configuration io_configuration{};
        io_configuration.abi_version = JR800_WASM_ABI_VERSION;
        io_configuration.ignore_unsupported_io = 2U;
        passed &= expect(jr800_machine_create_jr800(&io_configuration) == nullptr,
            "Invalid ignore-I/O boolean was accepted");
        io_configuration.ignore_unsupported_io = 1U;
        auto* io_machine = jr800_machine_create_jr800(&io_configuration);
        passed &= expect(io_machine != nullptr, "Ignore-I/O machine creation failed");
        if (io_machine != nullptr) {
            std::vector<std::uint8_t> io_rom(logical_rom_size, 0x01U);
            constexpr std::array<std::uint8_t, 8U> io_program{
                0x86U, 0x12U, 0xB7U, 0x03U, 0x00U, 0xB6U, 0x03U, 0x00U,
            };
            std::copy(io_program.begin(), io_program.end(), io_rom.begin());
            io_rom[logical_rom_size - 2U] = 0x80U;
            io_rom[logical_rom_size - 1U] = 0x00U;
            jr800_machine_state io_state{};
            jr800_stop_info io_stop{};
            passed &= expect(
                jr800_machine_load_logical_rom(io_machine, io_rom.data(),
                    static_cast<std::uint32_t>(io_rom.size())) == JR800_STATUS_OK
                    && jr800_machine_run(io_machine, 3U, &io_stop) == JR800_STATUS_OK
                    && io_stop.reason == JR800_STOP_INSTRUCTION_LIMIT
                    && jr800_machine_get_state(io_machine, &io_state) == JR800_STATUS_OK
                    && io_state.a == 0xFFU
                    && io_state.ignored_io_access_count_valid == 1U
                    && io_state.ignored_io_access_count_low == 2U
                    && io_state.ignored_io_access_count_high == 0U,
                "C ABI ignore-I/O execution or diagnostics differ");
            passed &= expect(jr800_machine_reset(io_machine) == JR800_STATUS_OK
                && jr800_machine_get_state(io_machine, &io_state) == JR800_STATUS_OK
                && io_state.ignored_io_access_count_valid == 1U
                && io_state.ignored_io_access_count_low == 0U,
                "C ABI reset retained ignored-I/O diagnostics");
            jr800_machine_destroy(io_machine);
        }
    }

    jr800_hardware_configuration invalid_configuration{};
    invalid_configuration.abi_version = JR800_WASM_ABI_VERSION;
    invalid_configuration.expansion_ram_enabled = 1U;
    passed &= expect(
        jr800_machine_create_jr800(nullptr) == nullptr
            && jr800_machine_create_jr800(&invalid_configuration) == nullptr,
        "Invalid JR-800 configuration was accepted"
    );
    invalid_configuration.expansion_ram_enabled = 0U;
    invalid_configuration.reset_index_register_value = 1U;
    passed &= expect(
        jr800_machine_create_jr800(&invalid_configuration) == nullptr,
        "Disabled reset register accepted a nonzero value"
    );
    invalid_configuration.reset_index_register_value = 0U;
    invalid_configuration.reset_condition_code_known_mask = 0x40U;
    passed &= expect(
        jr800_machine_create_jr800(&invalid_configuration) == nullptr,
        "Fixed condition-code bit was accepted as a reset override"
    );
    invalid_configuration.reset_condition_code_known_mask = 0x01U;
    invalid_configuration.reset_condition_code_value = 0x02U;
    passed &= expect(
        jr800_machine_create_jr800(&invalid_configuration) == nullptr,
        "Unknown reset condition-code value bit was accepted"
    );
    invalid_configuration.reset_condition_code_known_mask = 0U;
    invalid_configuration.reset_condition_code_value = 0U;
    invalid_configuration.internal_ram_initial_value = 1U;
    passed &= expect(
        jr800_machine_create_jr800(&invalid_configuration) == nullptr,
        "Disabled internal RAM accepted a nonzero initial value"
    );
    invalid_configuration.internal_ram_initial_value = 0U;
    invalid_configuration.port1_pin_value = 1U;
    passed &= expect(
        jr800_machine_create_jr800(&invalid_configuration) == nullptr,
        "Unknown pin bits were silently accepted"
    );
    invalid_configuration.port1_pin_value = 0U;
    invalid_configuration.calendar_cpu_cycle_ratio =
        JR800_CALENDAR_CPU_CYCLE_RATIO_E030_NOMINAL_1_2288_MHZ;
    passed &= expect(
        jr800_machine_create_jr800(&invalid_configuration) == nullptr,
        "Calendar CPU-cycle ratio was accepted without a calendar"
    );
    invalid_configuration.calendar_enabled = 1U;
    invalid_configuration.calendar_cpu_cycle_ratio = 2U;
    passed &= expect(
        jr800_machine_create_jr800(&invalid_configuration) == nullptr,
        "Unknown calendar CPU-cycle ratio was accepted"
    );

    jr800_hardware_configuration configuration{};
    configuration.abi_version = JR800_WASM_ABI_VERSION;
    jr800_machine* machine = jr800_machine_create_jr800(&configuration);
    jr800_machine* synthetic = jr800_machine_create();
    if (machine == nullptr || synthetic == nullptr) {
        jr800_machine_destroy(machine);
        jr800_machine_destroy(synthetic);
        std::cerr << "Machine creation failed\n";
        return 1;
    }

    const std::uint8_t byte{0x01U};
    constexpr std::array<std::uint8_t, 4U> ram_program{
        0x86U,
        0x42U,
        0x20U,
        0xFEU,
    };
    const auto ram_application = make_jr8app(
        0x2800U,
        0x2800U,
        ram_program
    );
    jr800_stop_info stop{};
    jr800_step_out_state step_out_state{};
    jr800_suspended_advance advance{};
    jr800_keyboard_activity keyboard_activity{
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
    };
    jr800_source_location source{};
    std::uint32_t source_address{};
    std::uint32_t symbol_address{};
    jr800_symbol_watch_result symbol_watch{};
    std::vector<std::uint8_t> panel(
        JR800_LCD_PANEL_DOT_COUNT,
        0xA5U
    );
    std::array<jr800_lcd_indicator_raw, JR800_LCD_INDICATOR_COUNT>
        indicators{};
    for (auto& indicator : indicators) {
        indicator = {0xA5A5A5A5U, 0xA5A5A5A5U};
    }
    passed &= expect(
        jr800_machine_step(machine, &stop) == JR800_STATUS_NO_ROM
            && jr800_machine_step_over(machine, 1U, &stop)
                == JR800_STATUS_NO_ROM
            && jr800_machine_step_out(
                machine,
                1U,
                &step_out_state,
                &stop
            ) == JR800_STATUS_NO_ROM
            && jr800_machine_run(machine, 1U, &stop) == JR800_STATUS_NO_ROM
            && jr800_machine_run_to(machine, 0x8000U, 1U, &stop)
                == JR800_STATUS_NO_ROM
            && jr800_machine_reset(machine) == JR800_STATUS_NO_ROM
            && jr800_machine_advance_suspended_cycles(machine, 1U, &advance)
                == JR800_STATUS_NO_ROM,
        "Unloaded JR-800 session did not report no-rom"
    );
    passed &= expect(
        jr800_machine_load_program(
            machine,
            ram_application.data(),
            static_cast<std::uint32_t>(ram_application.size())
        , 1U, nullptr) == JR800_STATUS_NO_ROM
            && jr800_machine_load_program(
                synthetic,
                ram_application.data(),
                static_cast<std::uint32_t>(ram_application.size())
            , 1U, nullptr) == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_load_program(nullptr, &byte, 1U, 1U, nullptr)
                == JR800_STATUS_INVALID_ARGUMENT,
        "RAM program load accepted an invalid session"
    );
    passed &= expect(
        jr800_machine_advance_calendar_oscillator_ticks(nullptr, 1U)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_advance_calendar_oscillator_ticks(
                synthetic,
                1U
            ) == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_advance_calendar_oscillator_ticks(machine, 1U)
                == JR800_STATUS_UNSUPPORTED_ACCESS,
        "Calendar oscillator advance did not reject an invalid target"
    );
    passed &= expect(
        jr800_machine_adjust_calendar_seconds(nullptr)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_adjust_calendar_seconds(synthetic)
                == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_adjust_calendar_seconds(machine)
                == JR800_STATUS_UNSUPPORTED_ACCESS,
        "Calendar adjustment did not reject an invalid target"
    );
    passed &= expect(
        jr800_machine_copy_lcd_panel(nullptr, panel.data(), panel.size())
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_copy_lcd_panel(
                machine,
                nullptr,
                panel.size()
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_copy_lcd_panel(
                machine,
                panel.data(),
                panel.size()
            ) == JR800_STATUS_NO_ROM
            && std::all_of(
                panel.begin(),
                panel.end(),
                [](std::uint8_t value) { return value == 0xA5U; }
            ),
        "Rejected unloaded LCD panel copy changed the destination"
    );
    passed &= expect(
        jr800_machine_copy_lcd_indicators(
            nullptr,
            indicators.data(),
            indicators.size()
        ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_copy_lcd_indicators(
                machine,
                nullptr,
                indicators.size()
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_copy_lcd_indicators(
                machine,
                indicators.data(),
                indicators.size()
            ) == JR800_STATUS_NO_ROM
            && std::all_of(
                indicators.begin(),
                indicators.end(),
                [](const jr800_lcd_indicator_raw& indicator) {
                    return indicator.value_known == 0xA5A5A5A5U
                        && indicator.value == 0xA5A5A5A5U;
                }
            ),
        "Rejected unloaded LCD indicator copy changed the destination"
    );
    passed &= expect(
        jr800_machine_get_keyboard_activity(nullptr, &keyboard_activity)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_get_keyboard_activity(machine, nullptr)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_get_keyboard_activity(
                machine,
                &keyboard_activity
            ) == JR800_STATUS_NO_ROM
            && keyboard_activity.read_attempts_low == 0xA5A5A5A5U
            && keyboard_activity.read_attempts_high == 0xA5A5A5A5U
            && keyboard_activity.distinct_addresses_low == 0xA5A5A5A5U
            && keyboard_activity.distinct_addresses_high == 0xA5A5A5A5U
            && jr800_machine_clear_keyboard_activity(nullptr)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_clear_keyboard_activity(machine)
                == JR800_STATUS_NO_ROM,
        "Rejected keyboard activity operation changed its destination"
    );
    passed &= expect(
        jr800_machine_load_application(machine, &byte, 1U, 0x01FFU)
                == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_load_debug_info(machine, &byte, 1U)
                == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_source_at(machine, 0x8000U, &source)
                == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_source_address(
                machine,
                "main.s",
                6U,
                1U,
                &source_address
            ) == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_symbol_address(
                machine,
                "entry",
                5U,
                &symbol_address
            ) == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_set_symbol_watch(machine, 1U, "entry", 5U)
                == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_evaluate_symbol_watch(
                machine,
                1U,
                &symbol_watch
            ) == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_clear_symbol_watch(machine, 1U)
                == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_copy_source_path(machine, 0U, nullptr, 0U)
                == JR800_STATUS_WRONG_MACHINE_KIND,
        "Application-only operations accepted a JR-800 session"
    );
    passed &= expect(
        jr800_machine_load_logical_rom(synthetic, &byte, 1U)
                == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_load_jr8rom(synthetic, &byte, 1U)
                == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_set_keyboard_bus_response(
                synthetic,
                0x0C00U,
                0U,
                0U
            ) == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_set_keyboard_key_state(
                synthetic,
                JR800_KEY_LETTER_X,
                0U
            ) == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_get_keyboard_activity(
                synthetic,
                &keyboard_activity
            ) == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_clear_keyboard_activity(synthetic)
                == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_copy_lcd_panel(
                synthetic,
                panel.data(),
                panel.size()
            ) == JR800_STATUS_WRONG_MACHINE_KIND
            && jr800_machine_copy_lcd_indicators(
                synthetic,
                indicators.data(),
                indicators.size()
            ) == JR800_STATUS_WRONG_MACHINE_KIND,
        "JR-800-only operations accepted a synthetic session"
    );

    std::array<std::uint8_t, 2U> retained{0xA5U, 0x5AU};
    passed &= expect(
        jr800_machine_read_memory(machine, 0x8000U, retained.data(), 1U)
                == JR800_STATUS_BACKING_STORE_UNAVAILABLE
            && retained == std::array<std::uint8_t, 2U>{0xA5U, 0x5AU},
        "Unavailable ROM read changed the destination"
    );

    const auto nop_rom = make_rom(std::array<std::uint8_t, 1U>{0x01U});
    const auto split_nop_container = make_jr8rom(nop_rom, true);
    auto incomplete_rom = nop_rom;
    incomplete_rom.pop_back();
    const auto incomplete_container = make_jr8rom(incomplete_rom, false);
    auto damaged_container = split_nop_container;
    damaged_container.back() ^= 0x01U;
    const std::vector<std::uint8_t> oversized_container(
        jr800::formats::jr8rom::maximum_encoded_size + 1U,
        0U
    );
    passed &= expect(
        jr800_machine_load_jr8rom(machine, nullptr, 0U)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_load_jr8rom(
                machine,
                nop_rom.data(),
                static_cast<std::uint32_t>(nop_rom.size())
            ) == JR800_STATUS_INVALID_JR8ROM
            && jr800_machine_load_jr8rom(
                machine,
                incomplete_container.data(),
                static_cast<std::uint32_t>(incomplete_container.size())
            ) == JR800_STATUS_INCOMPLETE_JR8ROM
            && jr800_machine_load_jr8rom(
                machine,
                damaged_container.data(),
                static_cast<std::uint32_t>(damaged_container.size())
            ) == JR800_STATUS_INTEGRITY_MISMATCH
            && jr800_machine_load_jr8rom(
                machine,
                oversized_container.data(),
                static_cast<std::uint32_t>(oversized_container.size())
            ) == JR800_STATUS_INVALID_JR8ROM,
        "Invalid JR8ROM inputs were accepted"
    );
    passed &= expect(
        jr800_machine_load_jr8rom(
            machine,
            split_nop_container.data(),
            static_cast<std::uint32_t>(split_nop_container.size())
        ) == JR800_STATUS_OK,
        "Split JR8ROM load failed"
    );
    passed &= expect(
        jr800_machine_load_logical_rom(
            machine,
            nop_rom.data(),
            static_cast<std::uint32_t>(nop_rom.size() - 1U)
        ) == JR800_STATUS_INVALID_LOGICAL_ROM,
        "Wrong-sized logical ROM was accepted"
    );
    passed &= expect(
        jr800_machine_load_logical_rom(
            machine,
            nop_rom.data(),
            static_cast<std::uint32_t>(nop_rom.size())
        ) == JR800_STATUS_OK,
        "Logical ROM load failed"
    );
    const auto wrong_target_application = make_jr8app(
        0x2800U,
        0x2800U,
        ram_program,
        "mc6801"
    );
    const auto rom_application = make_jr8app(
        0x8000U,
        0x8000U,
        ram_program
    );
    jr800_machine* program_machine = jr800_machine_create_jr800(
        &configuration
    );
    if (program_machine == nullptr) {
        jr800_machine_destroy(machine);
        jr800_machine_destroy(synthetic);
        std::cerr << "Program machine creation failed\n";
        return 1;
    }
    passed &= expect(
        jr800_machine_load_logical_rom(
            program_machine,
            nop_rom.data(),
            static_cast<std::uint32_t>(nop_rom.size())
        ) == JR800_STATUS_OK,
        "Program machine ROM load failed"
    );
    passed &= expect(
        jr800_machine_load_program(program_machine, &byte, 1U, 1U, nullptr)
                == JR800_STATUS_INVALID_APPLICATION
            && jr800_machine_load_program(
                program_machine,
                wrong_target_application.data(),
                static_cast<std::uint32_t>(wrong_target_application.size())
            , 1U, nullptr) == JR800_STATUS_TARGET_MISMATCH
            && jr800_machine_load_program(
                program_machine,
                rom_application.data(),
                static_cast<std::uint32_t>(rom_application.size())
            , 1U, nullptr) == JR800_STATUS_SEGMENT_OUT_OF_RANGE,
        "Invalid RAM program input was accepted"
    );
    passed &= expect(
        jr800_machine_load_program(
            program_machine,
            ram_application.data(),
            static_cast<std::uint32_t>(ram_application.size())
        , 1U, nullptr) == JR800_STATUS_OK,
        "RAM program load failed"
    );
    std::array<std::uint8_t, 4U> loaded_ram{};
    jr800_machine_state program_state{};
    passed &= expect(
        jr800_machine_read_memory(
            program_machine,
            0x2800U,
            loaded_ram.data(),
            loaded_ram.size()
        ) == JR800_STATUS_OK
            && loaded_ram == ram_program
            && jr800_machine_get_state(program_machine, &program_state)
                == JR800_STATUS_OK
            && program_state.pc == 0x2800U
            && jr800_machine_step(program_machine, &stop) == JR800_STATUS_OK
            && jr800_machine_get_state(program_machine, &program_state)
                == JR800_STATUS_OK
            && program_state.pc == 0x2802U
            && program_state.a == 0x42U,
        "Loaded RAM program did not start at its recorded entry point"
    );
    jr800::formats::jr8app::Application mixed_application;
    mixed_application.target_profile = "hd6301v1";
    mixed_application.entry_point = 0x2800U;
    mixed_application.segments = {
        {
            jr800::formats::jr8app::SegmentKind::data,
            0x2800U,
            1U,
            {0x99U},
        },
        {
            jr800::formats::jr8app::SegmentKind::data,
            0x8000U,
            1U,
            {0x01U},
        },
    };
    mixed_application.integrity_sha256 =
        jr800::formats::jr8app::compute_integrity(mixed_application);
    const auto mixed_program = jr800::formats::jr8app::write(
        mixed_application
    );
    const auto state_before_mixed_program = program_state;
    loaded_ram.fill(0U);
    passed &= expect(
        jr800_machine_load_program(
            program_machine,
            mixed_program.data(),
            static_cast<std::uint32_t>(mixed_program.size())
        , 1U, nullptr) == JR800_STATUS_SEGMENT_OUT_OF_RANGE
            && jr800_machine_read_memory(
                program_machine,
                0x2800U,
                loaded_ram.data(),
                loaded_ram.size()
            ) == JR800_STATUS_OK
            && loaded_ram == ram_program
            && jr800_machine_get_state(program_machine, &program_state)
                == JR800_STATUS_OK
            && program_state.pc == state_before_mixed_program.pc
            && program_state.a == state_before_mixed_program.a
            && program_state.cycle_count_low
                == state_before_mixed_program.cycle_count_low
            && program_state.cycle_count_high
                == state_before_mixed_program.cycle_count_high,
        "Rejected multi-segment RAM program partially changed the machine"
    );
    // Complete machine checkpoints are transactional and ROM-bound.
    std::uint32_t state_size{};
    passed &= expect(jr800_machine_export_state(program_machine, nullptr, 0, &state_size)
        == JR800_STATUS_OK && state_size > 68, "State size query failed");
    std::vector<std::uint8_t> saved_state(state_size, 0xA5);
    passed &= expect(jr800_machine_export_state(program_machine, saved_state.data(), state_size - 1, &state_size)
        == JR800_STATUS_BUFFER_TOO_SMALL && saved_state[0] == 0xA5, "Small state buffer modified");
    passed &= expect(jr800_machine_export_state(program_machine, saved_state.data(), state_size, &state_size)
        == JR800_STATUS_OK, "State export failed");
    auto damaged_state = saved_state; damaged_state.back() ^= 1;
    passed &= expect(jr800_machine_import_state(program_machine, damaged_state.data(), state_size)
        == JR800_STATUS_INVALID_APPLICATION, "Corrupt state accepted");
    damaged_state = saved_state; damaged_state[4] ^= 1;
    passed &= expect(jr800_machine_import_state(program_machine, damaged_state.data(), state_size)
        == JR800_STATUS_TARGET_MISMATCH, "Wrong ROM state accepted");
    passed &= expect(jr800_machine_import_state(program_machine, saved_state.data(), state_size)
        == JR800_STATUS_OK, "State restore failed");
    std::vector<std::uint8_t> state_again(state_size);
    passed &= expect(jr800_machine_export_state(program_machine, state_again.data(), state_size, &state_size)
        == JR800_STATUS_OK && state_again == saved_state, "State round trip changed bytes");
    passed &= expect(jr800_machine_get_state(program_machine, &program_state) == JR800_STATUS_OK
        && program_state.pc == state_before_mixed_program.pc
        && program_state.cycle_count_low == state_before_mixed_program.cycle_count_low,
        "State restore changed CPU position");
    const std::array<std::uint8_t, 4U> invalid_wav{'R', 'I', 'F', 'F'};
    jr800_native_program_wav_issue wav_issue{0xFFFFU, 0xFFFFU};
    passed &= expect(
        jr800_machine_load_native_program_wav(
            nullptr,
            invalid_wav.data(),
            static_cast<std::uint32_t>(invalid_wav.size()),
            &wav_issue
        , 1U, nullptr) == JR800_STATUS_INVALID_ARGUMENT
            && wav_issue.code == 0xFFFFU
            && jr800_machine_load_native_program_wav(
                synthetic,
                invalid_wav.data(),
                static_cast<std::uint32_t>(invalid_wav.size()),
                &wav_issue
            , 1U, nullptr) == JR800_STATUS_WRONG_MACHINE_KIND,
        "Invalid native program WAV arguments were accepted"
    );
    passed &= expect(
        jr800_machine_load_native_program_wav(
            program_machine,
            invalid_wav.data(),
            static_cast<std::uint32_t>(invalid_wav.size()),
            &wav_issue
        , 1U, nullptr) == JR800_STATUS_INVALID_NATIVE_PROGRAM_WAV
            && wav_issue.code == JR800_NATIVE_PROGRAM_WAV_ISSUE_INVALID_WAV
            && wav_issue.burst_index == 0U,
        "Invalid native program WAV did not report its decoder issue"
    );
    const auto program_wav = make_native_program_wav(
        0x2800U,
        0x2800U,
        ram_program
    );
    wav_issue = {0xFFFFU, 0xFFFFU};
    loaded_ram.fill(0U);
    passed &= expect(
        jr800_machine_load_native_program_wav(
            program_machine,
            program_wav.data(),
            static_cast<std::uint32_t>(program_wav.size()),
            &wav_issue
        , 1U, nullptr) == JR800_STATUS_OK
            && wav_issue.code == JR800_NATIVE_PROGRAM_WAV_ISSUE_NONE
            && wav_issue.burst_index == 0U
            && jr800_machine_read_memory(
                program_machine,
                0x2800U,
                loaded_ram.data(),
                loaded_ram.size()
            ) == JR800_STATUS_OK
            && loaded_ram == ram_program
            && jr800_machine_get_state(program_machine, &program_state)
                == JR800_STATUS_OK
            && program_state.pc == 0x2800U,
        "Native program WAV did not decode, load, and select its entry point"
    );
    jr800_machine_destroy(program_machine);
    passed &= expect(
        jr800_machine_copy_lcd_panel(
            machine,
            panel.data(),
            panel.size()
        ) == JR800_STATUS_UNSUPPORTED_ACCESS
            && std::all_of(
                panel.begin(),
                panel.end(),
                [](std::uint8_t value) { return value == 0xA5U; }
            ),
        "Disconnected LCD panel was exposed or changed the destination"
    );
    passed &= expect(
        jr800_machine_copy_lcd_indicators(
            machine,
            indicators.data(),
            indicators.size()
        ) == JR800_STATUS_UNSUPPORTED_ACCESS
            && std::all_of(
                indicators.begin(),
                indicators.end(),
                [](const jr800_lcd_indicator_raw& indicator) {
                    return indicator.value_known == 0xA5A5A5A5U
                        && indicator.value == 0xA5A5A5A5U;
                }
            ),
        "Disconnected LCD unexpectedly produced indicator values"
    );
    keyboard_activity = {};
    passed &= expect(
        jr800_machine_get_keyboard_activity(machine, &keyboard_activity)
                == JR800_STATUS_OK
            && combine(
                keyboard_activity.read_attempts_low,
                keyboard_activity.read_attempts_high
            ) == 0U
            && combine(
                keyboard_activity.distinct_addresses_low,
                keyboard_activity.distinct_addresses_high
            ) == 0U,
        "Reset keyboard activity was not empty"
    );

    jr800_machine_state state{};
    jr800_machine_state synthetic_state{};
    passed &= expect(
        jr800_machine_get_state(machine, &state) == JR800_STATUS_OK
            && jr800_machine_get_state(synthetic, &synthetic_state)
                == JR800_STATUS_OK
            && state.abi_version == JR800_WASM_ABI_VERSION
            && state.profile == JR800_PROFILE_HD6301V1
            && state.pc == 0x8000U
            && state.execution_state == JR800_CPU_ACTIVE
            && state.cycle_count_low == 0U
            && state.cycle_count_high == 0U
            && state.register_known_mask == JR800_REGISTER_PC
            && state.condition_code == 0xD0U
            && state.condition_code_known_mask == 0xD0U
            && state.calendar_alarm_terminal
                == JR800_CALENDAR_ALARM_TERMINAL_DISCONNECTED
            && state.port2_timer_output
                == JR800_PORT2_TIMER_OUTPUT_DISABLED
            && synthetic_state.port2_timer_output
                == JR800_PORT2_TIMER_OUTPUT_UNAVAILABLE
            && state.lcd_substituted_data_read_count_valid == 0U
            && state.lcd_substituted_data_read_count_low == 0U
            && state.lcd_substituted_data_read_count_high == 0U
            && synthetic_state.lcd_substituted_data_read_count_valid == 0U
            && synthetic_state.lcd_substituted_data_read_count_low == 0U
            && synthetic_state.lcd_substituted_data_read_count_high == 0U,
        "Reset state or knownness differs"
    );

    const auto timer_output_transport_succeeds = [](
        std::uint8_t olvl,
        std::uint32_t expected_final_state
    ) {
        jr800_hardware_configuration timer_configuration{};
        timer_configuration.abi_version = JR800_WASM_ABI_VERSION;
        jr800_machine* timer_machine =
            jr800_machine_create_jr800(&timer_configuration);
        if (timer_machine == nullptr) {
            return false;
        }

        const std::array<std::uint8_t, 13U> timer_program{
            0x86U, olvl,
            0x97U, 0x08U,
            0x86U, 0x02U,
            0x97U, 0x01U,
            0xCCU, 0xFFU, 0xFCU,
            0xDDU, 0x09U,
        };
        const auto timer_rom = make_rom(timer_program);
        jr800_machine_state disabled_state{};
        jr800_machine_state unknown_state{};
        jr800_machine_state final_state{};
        jr800_stop_info timer_stop{};
        const bool succeeded =
            jr800_machine_load_logical_rom(
                timer_machine,
                timer_rom.data(),
                static_cast<std::uint32_t>(timer_rom.size())
            ) == JR800_STATUS_OK
            && jr800_machine_get_state(timer_machine, &disabled_state)
                == JR800_STATUS_OK
            && disabled_state.port2_timer_output
                == JR800_PORT2_TIMER_OUTPUT_DISABLED
            && jr800_machine_run(timer_machine, 4U, &timer_stop)
                == JR800_STATUS_OK
            && jr800_machine_get_state(timer_machine, &unknown_state)
                == JR800_STATUS_OK
            && unknown_state.port2_timer_output
                == JR800_PORT2_TIMER_OUTPUT_UNKNOWN
            && jr800_machine_run(timer_machine, 2U, &timer_stop)
                == JR800_STATUS_OK
            && jr800_machine_get_state(timer_machine, &final_state)
                == JR800_STATUS_OK
            && final_state.port2_timer_output == expected_final_state;
        jr800_machine_destroy(timer_machine);
        return succeeded;
    };
    passed &= expect(
        timer_output_transport_succeeds(
            0x01U,
            JR800_PORT2_TIMER_OUTPUT_HIGH
        )
            && timer_output_transport_succeeds(
                0x00U,
                JR800_PORT2_TIMER_OUTPUT_LOW
            ),
        "Port 2 timer output state did not cross the C ABI boundary"
    );

    jr800_disassembly disassembly{};
    passed &= expect(
        jr800_machine_disassemble(machine, 0x8000U, &disassembly)
                == JR800_STATUS_OK
            && disassembly.address == 0x8000U
            && disassembly.byte0 == 0x01U
            && disassembly.byte1 == 0U
            && disassembly.byte2 == 0U
            && disassembly.length == 1U
            && disassembly.supported == 1U,
        "JR-800 disassembly record is not instruction-bounded"
    );
    constexpr char invalid_condition[] = "A ==";
    constexpr char unknown_condition[] = "A == 0";
    passed &= expect(
        jr800_machine_set_conditional_execution_breakpoint(
            nullptr,
            0x8000U,
            unknown_condition,
            static_cast<std::uint32_t>(sizeof(unknown_condition) - 1U)
        ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_conditional_execution_breakpoint(
                machine,
                0x8000U,
                nullptr,
                1U
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_conditional_execution_breakpoint(
                machine,
                0x8000U,
                unknown_condition,
                0U
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_conditional_execution_breakpoint(
                machine,
                0x1'0000U,
                unknown_condition,
                static_cast<std::uint32_t>(sizeof(unknown_condition) - 1U)
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_conditional_execution_breakpoint(
                machine,
                0x8000U,
                invalid_condition,
                static_cast<std::uint32_t>(sizeof(invalid_condition) - 1U)
            ) == JR800_STATUS_INVALID_EXPRESSION
            && jr800_machine_set_conditional_execution_breakpoint(
                machine,
                0x8000U,
                unknown_condition,
                static_cast<std::uint32_t>(sizeof(unknown_condition) - 1U)
            ) == JR800_STATUS_OK
            && jr800_machine_run(machine, 1U, &stop) == JR800_STATUS_OK
            && stop.reason == JR800_STOP_BREAKPOINT_CONDITION_ERROR
            && stop.condition_error == JR800_EXPRESSION_UNKNOWN_STATE
            && stop.state_fault == JR800_STATE_PART_ACCUMULATOR_A
            && combine(
                stop.instructions_executed_low,
                stop.instructions_executed_high
            ) == 0U
            && jr800_machine_history_count(machine) == 0U
            && jr800_machine_set_execution_breakpoint(
                machine,
                0x8000U,
                0U
            ) == JR800_STATUS_OK,
        "Conditional breakpoint did not preserve unknown JR-800 state"
    );
    constexpr char unreadable_memory_condition[] = "mem8[$1000] != 0";
    passed &= expect(
        jr800_machine_set_conditional_execution_breakpoint(
            machine,
            0x8000U,
            unreadable_memory_condition,
            static_cast<std::uint32_t>(
                sizeof(unreadable_memory_condition) - 1U
            )
        ) == JR800_STATUS_OK
            && jr800_machine_run(machine, 1U, &stop) == JR800_STATUS_OK
            && stop.reason == JR800_STOP_BREAKPOINT_CONDITION_ERROR
            && stop.condition_error == JR800_EXPRESSION_MEMORY_ACCESS
            && stop.bus_fault == JR800_BUS_FAULT_UNSUPPORTED_ACCESS
            && stop.condition_fault_address == 0x1000U
            && combine(
                stop.instructions_executed_low,
                stop.instructions_executed_high
            ) == 0U
            && jr800_machine_history_count(machine) == 0U
            && jr800_machine_set_execution_breakpoint(
                machine,
                0x8000U,
                0U
            ) == JR800_STATUS_OK,
        "Conditional breakpoint hid a non-invasive memory fault"
    );
    constexpr char register_watch[] = "A";
    constexpr char memory_watch[] = "mem8[$1000]";
    jr800_expression_watch_result watch_result{};
    passed &= expect(
        jr800_machine_set_expression_watch(
            machine,
            1U,
            register_watch,
            sizeof(register_watch) - 1U
        ) == JR800_STATUS_OK
            && jr800_machine_evaluate_expression_watch(
                machine,
                1U,
                &watch_result
            ) == JR800_STATUS_OK
            && watch_result.error == JR800_EXPRESSION_UNKNOWN_STATE
            && watch_result.state_fault
                == JR800_STATE_PART_ACCUMULATOR_A
            && watch_result.bus_fault == JR800_BUS_FAULT_NONE
            && jr800_machine_history_count(machine) == 0U,
        "Expression watch guessed an unknown JR-800 register"
    );
    passed &= expect(
        jr800_machine_set_expression_watch(
            machine,
            2U,
            memory_watch,
            sizeof(memory_watch) - 1U
        ) == JR800_STATUS_OK
            && jr800_machine_evaluate_expression_watch(
                machine,
                2U,
                &watch_result
            ) == JR800_STATUS_OK
            && watch_result.error == JR800_EXPRESSION_MEMORY_ACCESS
            && watch_result.bus_fault
                == JR800_BUS_FAULT_UNSUPPORTED_ACCESS
            && watch_result.fault_address == 0x1000U
            && watch_result.state_fault == JR800_STATE_PART_NONE
            && jr800_machine_history_count(machine) == 0U
            && jr800_machine_clear_expression_watch(machine, 1U)
                == JR800_STATUS_OK
            && jr800_machine_clear_expression_watch(machine, 2U)
                == JR800_STATUS_OK,
        "Expression watch hid a non-invasive JR-800 memory fault"
    );
    passed &= expect(
        jr800_machine_step(machine, &stop) == JR800_STATUS_OK
            && stop.reason == JR800_STOP_STEP_COMPLETE
            && stop.trigger_access_valid == 0U
            && stop.fault == JR800_FAULT_NONE
            && stop.bus_fault == JR800_BUS_FAULT_NONE
            && stop.step_kind == JR800_STEP_INSTRUCTION
            && stop.interrupt_source == JR800_INTERRUPT_NONE
            && stop.pc_before == 0x8000U
            && stop.pc_after == 0x8001U
            && stop.byte0 == 0x01U
            && stop.instruction_length == 1U
            && stop.bytes_fetched == 1U
            && stop.cycles == 1U
            && stop.condition_error == JR800_EXPRESSION_OK,
        "JR-800 C ABI step differs"
    );
    passed &= expect(
        jr800_machine_history_count(machine) == 1U,
        "JR-800 step did not create one history entry"
    );
    jr800_history_entry history{};
    passed &= expect(
        jr800_machine_copy_history(machine, &history, 1U) == 1U
            && history.pc_before == 0x8000U
            && history.state_pc == 0x8001U
            && history.step_kind == JR800_STEP_INSTRUCTION
            && history.bus_fault == JR800_BUS_FAULT_NONE
            && history.state_register_known_mask == JR800_REGISTER_PC
            && history.state_condition_code_known_mask == 0xD0U,
        "JR-800 history detail differs"
    );

    std::array<std::uint8_t, 2U> memory{};
    passed &= expect(
        jr800_machine_read_memory(machine, 0x8000U, memory.data(), 2U)
                == JR800_STATUS_OK
            && memory == std::array<std::uint8_t, 2U>{0x01U, 0x01U},
        "JR-800 ROM inspection differs"
    );
    passed &= expect(
        jr800_machine_read_memory(machine, 0x0C00U, memory.data(), 1U)
            == JR800_STATUS_UNINITIALIZED_READ,
        "Unknown keyboard response was guessed"
    );
    passed &= expect(
        jr800_machine_set_keyboard_bus_response(
            machine,
            0x0FFFU,
            0x3CU,
            1U
        ) == JR800_STATUS_OK,
        "Keyboard response setup failed"
    );
    retained = {0xA5U, 0x5AU};
    passed &= expect(
        jr800_machine_read_memory(machine, 0x0FFFU, retained.data(), 2U)
                == JR800_STATUS_UNSUPPORTED_ACCESS
            && retained == std::array<std::uint8_t, 2U>{0xA5U, 0x5AU},
        "Failed cross-region read partially changed the destination"
    );
    passed &= expect(
        jr800_machine_set_keyboard_bus_response(
            machine,
            0x0BFFU,
            0U,
            0U
        ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_keyboard_bus_response(
                machine,
                0x0C00U,
                1U,
                0U
            ) == JR800_STATUS_INVALID_ARGUMENT,
        "Invalid keyboard response was accepted"
    );
    passed &= expect(
        jr800_machine_set_keyboard_key_state(
            nullptr,
            JR800_KEY_LETTER_X,
            0U
        ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_keyboard_key_state(machine, 77U, 0U)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_keyboard_key_state(
                machine,
                JR800_KEY_LETTER_X,
                2U
            ) == JR800_STATUS_INVALID_ARGUMENT,
        "Invalid keyboard key state was accepted"
    );
    bool all_keyboard_keys_are_transportable = true;
    for (std::uint32_t key = JR800_KEY_SHIFT;
         key <= JR800_KEY_KEYPAD_DIVIDE;
         ++key) {
        if (jr800_machine_set_keyboard_key_state(machine, key, 1U)
                != JR800_STATUS_OK
            || jr800_machine_set_keyboard_key_state(machine, key, 0U)
                != JR800_STATUS_OK) {
            all_keyboard_keys_are_transportable = false;
            break;
        }
    }
    passed &= expect(
        all_keyboard_keys_are_transportable,
        "A modeled keyboard key was missing from the C transport"
    );
    passed &= expect(
        jr800_machine_set_keyboard_key_state(
            machine,
            JR800_KEY_LETTER_X,
            1U
        ) == JR800_STATUS_OK
            && jr800_machine_read_memory(
                machine,
                0x0F7FU,
                memory.data(),
                1U
            ) == JR800_STATUS_OK
            && memory[0] == 0xFEU
            && jr800_machine_set_keyboard_key_state(
                machine,
                JR800_KEY_LETTER_X,
                0U
            ) == JR800_STATUS_OK
            && jr800_machine_read_memory(
                machine,
                0x0F7FU,
                memory.data(),
                1U
            ) == JR800_STATUS_OK
            && memory[0] == 0xFFU,
        "Verified keyboard key state did not cross the C boundary"
    );
    passed &= expect(
        jr800_machine_set_keyboard_key_state(
            machine,
            JR800_KEY_LETTER_A,
            1U
        ) == JR800_STATUS_OK
            && jr800_machine_read_memory(
                machine,
                0x0FEFU,
                memory.data(),
                1U
            ) == JR800_STATUS_UNINITIALIZED_READ
            && jr800_machine_set_keyboard_bus_response(
                machine,
                0x0FEFU,
                0xFFU,
                1U
            ) == JR800_STATUS_OK
            && jr800_machine_read_memory(
                machine,
                0x0FEFU,
                memory.data(),
                1U
            ) == JR800_STATUS_OK
            && memory[0] == 0xFDU
            && jr800_machine_set_keyboard_key_state(
                machine,
                JR800_KEY_LETTER_A,
                0U
            ) == JR800_STATUS_OK
            && jr800_machine_set_keyboard_bus_response(
                machine,
                0x0FEFU,
                0U,
                0U
            ) == JR800_STATUS_OK,
        "Raw-qualified keyboard key weakened its C boundary"
    );

    passed &= expect(
        jr800_machine_set_keyboard_bus_response(
            machine,
            0x0FFDU,
            0xFFU,
            1U
        ) == JR800_STATUS_OK
            && jr800_machine_set_keyboard_key_state(
                machine,
                JR800_KEY_KEYPAD_DIVIDE,
                1U
            ) == JR800_STATUS_OK
            && jr800_machine_read_memory(
                machine,
                0x0FFDU,
                memory.data(),
                1U
            ) == JR800_STATUS_OK
            && memory[0] == 0x7FU
            && jr800_machine_set_keyboard_key_state(
                machine,
                JR800_KEY_KEYPAD_DIVIDE,
                0U
            ) == JR800_STATUS_OK
            && jr800_machine_set_keyboard_bus_response(
                machine,
                0x0FFDU,
                0U,
                0U
            ) == JR800_STATUS_OK,
        "Expanded keyboard key did not preserve its C mapping"
    );

    jr800_hardware_configuration keyboard_configuration{};
    keyboard_configuration.abi_version = JR800_WASM_ABI_VERSION;
    jr800_machine* keyboard_machine =
        jr800_machine_create_jr800(&keyboard_configuration);
    if (keyboard_machine == nullptr) {
        passed = false;
        std::cerr << "Keyboard activity machine creation failed\n";
    } else {
        constexpr std::array<std::uint8_t, 9U> keyboard_program{
            0xB6U, 0x0CU, 0x00U,
            0xB6U, 0x0CU, 0x01U,
            0xB6U, 0x0CU, 0x00U,
        };
        const auto keyboard_rom = make_rom(keyboard_program);
        keyboard_activity = {};
        passed &= expect(
            jr800_machine_load_logical_rom(
                keyboard_machine,
                keyboard_rom.data(),
                static_cast<std::uint32_t>(keyboard_rom.size())
            ) == JR800_STATUS_OK
                && jr800_machine_set_keyboard_bus_response(
                    keyboard_machine,
                    0x0C00U,
                    0xFFU,
                    1U
                ) == JR800_STATUS_OK
                && jr800_machine_set_keyboard_bus_response(
                    keyboard_machine,
                    0x0C01U,
                    0xFFU,
                    1U
                ) == JR800_STATUS_OK
                && jr800_machine_run(keyboard_machine, 3U, &stop)
                    == JR800_STATUS_OK
                && stop.reason == JR800_STOP_INSTRUCTION_LIMIT
                && jr800_machine_get_keyboard_activity(
                    keyboard_machine,
                    &keyboard_activity
                ) == JR800_STATUS_OK
                && combine(
                    keyboard_activity.read_attempts_low,
                    keyboard_activity.read_attempts_high
                ) == 3U
                && combine(
                    keyboard_activity.distinct_addresses_low,
                    keyboard_activity.distinct_addresses_high
                ) == 2U,
            "Keyboard activity C record differs"
        );
        passed &= expect(
            jr800_machine_clear_keyboard_activity(keyboard_machine)
                    == JR800_STATUS_OK
                && jr800_machine_get_keyboard_activity(
                    keyboard_machine,
                    &keyboard_activity
                ) == JR800_STATUS_OK
                && combine(
                    keyboard_activity.read_attempts_low,
                    keyboard_activity.read_attempts_high
                ) == 0U
                && combine(
                    keyboard_activity.distinct_addresses_low,
                    keyboard_activity.distinct_addresses_high
                ) == 0U,
            "Keyboard activity C clear failed"
        );
        jr800_machine_destroy(keyboard_machine);
    }

    passed &= expect(
        jr800_machine_step_over(nullptr, 1U, &stop)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_step_over(machine, 0U, &stop)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_step_over(machine, 1U, nullptr)
                == JR800_STATUS_INVALID_ARGUMENT,
        "Invalid step-over arguments were accepted"
    );
    passed &= expect(
        jr800_machine_step_out(nullptr, 1U, &step_out_state, &stop)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_step_out(machine, 0U, &step_out_state, &stop)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_step_out(machine, 1U, nullptr, &stop)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_step_out(machine, 1U, &step_out_state, nullptr)
                == JR800_STATUS_INVALID_ARGUMENT,
        "Invalid step-out pointers or limit were accepted"
    );
    step_out_state.continued = 2U;
    passed &= expect(
        jr800_machine_step_out(machine, 1U, &step_out_state, &stop)
            == JR800_STATUS_INVALID_ARGUMENT,
        "Invalid step-out continuation flag was accepted"
    );
    step_out_state = {0U, 1U, 0U};
    passed &= expect(
        jr800_machine_step_out(machine, 1U, &step_out_state, &stop)
            == JR800_STATUS_INVALID_ARGUMENT,
        "Fresh step-out state accepted a nonzero nesting depth"
    );
    step_out_state = {};

    passed &= expect(
        jr800_machine_set_memory_watchpoint(nullptr, 0x8000U, 1U, 1U)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_memory_watchpoint(
                machine,
                0x1'0000U,
                JR800_WATCHPOINT_READ,
                1U
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_memory_watchpoint(machine, 0x8000U, 0U, 1U)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_memory_watchpoint(machine, 0x8000U, 4U, 1U)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_memory_watchpoint(
                machine,
                0x8000U,
                JR800_WATCHPOINT_READ,
                2U
            ) == JR800_STATUS_INVALID_ARGUMENT,
        "Invalid memory watchpoint was accepted"
    );
    constexpr std::array<std::uint8_t, 3U> read_program{
        0xB6U, 0x80U, 0x00U,
    };
    const auto read_rom = make_rom(read_program);
    passed &= expect(
        jr800_machine_load_logical_rom(
            machine,
            read_rom.data(),
            static_cast<std::uint32_t>(read_rom.size())
        ) == JR800_STATUS_OK
            && jr800_machine_set_memory_watchpoint(
                machine,
                0x8000U,
                JR800_WATCHPOINT_READ,
                1U
            ) == JR800_STATUS_OK
            && jr800_machine_run(machine, 4U, &stop) == JR800_STATUS_OK
            && stop.reason == JR800_STOP_MEMORY_WATCHPOINT
            && stop.trigger_address == 0x8000U
            && stop.trigger_access_valid == 1U
            && stop.trigger_access == JR800_ACCESS_DATA_READ
            && combine(
                stop.instructions_executed_low,
                stop.instructions_executed_high
            ) == 1U,
        "Read-mode memory watchpoint did not stop after the data read"
    );

    jr800_hardware_configuration access_configuration{};
    access_configuration.abi_version = JR800_WASM_ABI_VERSION;
    access_configuration.reset_stack_pointer_enabled = 1U;
    access_configuration.reset_stack_pointer_value = 0x2345U;
    access_configuration.reset_index_register_enabled = 1U;
    access_configuration.reset_index_register_value = 0x3456U;
    access_configuration.reset_accumulator_a_enabled = 1U;
    access_configuration.reset_accumulator_a_value = 0x67U;
    access_configuration.reset_accumulator_b_enabled = 1U;
    access_configuration.reset_accumulator_b_value = 0x89U;
    access_configuration.reset_condition_code_known_mask = 0x2FU;
    access_configuration.reset_condition_code_value = 0x25U;
    access_configuration.internal_ram_enabled = 1U;
    access_configuration.internal_ram_initial_value = 0x5CU;
    access_configuration.standard_ram_enabled = 1U;
    access_configuration.standard_ram_initial_value = 0x41U;
    jr800_machine* access_machine =
        jr800_machine_create_jr800(&access_configuration);
    if (access_machine == nullptr) {
        passed = false;
        std::cerr << "Access-watch machine creation failed\n";
    } else {
        constexpr std::array<std::uint8_t, 3U> increment_program{
            0x7CU, 0x20U, 0x00U,
        };
        const auto increment_rom = make_rom(increment_program);
        std::uint8_t incremented{};
        std::uint8_t internal_ram_value{};
        jr800_machine_state configured_state{};
        passed &= expect(
            jr800_machine_load_logical_rom(
                access_machine,
                increment_rom.data(),
                static_cast<std::uint32_t>(increment_rom.size())
            ) == JR800_STATUS_OK
                && jr800_machine_get_state(
                    access_machine,
                    &configured_state
                ) == JR800_STATUS_OK
                && configured_state.sp == 0x2345U
                && configured_state.x == 0x3456U
                && configured_state.a == 0x67U
                && configured_state.b == 0x89U
                && configured_state.condition_code == 0xF5U
                && configured_state.register_known_mask
                    == (JR800_REGISTER_PC | JR800_REGISTER_SP
                        | JR800_REGISTER_X | JR800_REGISTER_A
                        | JR800_REGISTER_B)
                && configured_state.condition_code_known_mask == 0xFFU
                && jr800_machine_set_memory_watchpoint(
                    access_machine,
                    0x2000U,
                    JR800_WATCHPOINT_ACCESS,
                    1U
                ) == JR800_STATUS_OK
                && jr800_machine_run(access_machine, 4U, &stop)
                    == JR800_STATUS_OK
                && stop.reason == JR800_STOP_MEMORY_WATCHPOINT
                && stop.trigger_address == 0x2000U
                && stop.trigger_access_valid == 1U
                && stop.trigger_access == JR800_ACCESS_DATA_READ
                && jr800_machine_read_memory(
                    access_machine,
                    0x2000U,
                    &incremented,
                    1U
                ) == JR800_STATUS_OK
                && incremented == 0x42U
                && jr800_machine_read_memory(
                    access_machine,
                    0x0080U,
                    &internal_ram_value,
                    1U
                ) == JR800_STATUS_OK
                && internal_ram_value == 0x5CU
                && jr800_machine_reset(access_machine) == JR800_STATUS_OK
                && jr800_machine_read_memory(
                    access_machine, 0x2000U, &incremented, 1U
                ) == JR800_STATUS_OK
                && incremented == 0x41U
                && jr800_machine_get_state(
                    access_machine,
                    &configured_state
                ) == JR800_STATUS_OK
                && configured_state.sp == 0x2345U
                && configured_state.x == 0x3456U
                && configured_state.a == 0x67U
                && configured_state.b == 0x89U
                && configured_state.condition_code == 0xF5U
                && configured_state.condition_code_known_mask == 0xFFU,
            "Configured reset or access-mode watchpoint behavior changed"
        );
        jr800_machine_destroy(access_machine);
    }

    jr800_hardware_configuration step_over_configuration{};
    step_over_configuration.abi_version = JR800_WASM_ABI_VERSION;
    step_over_configuration.standard_ram_enabled = 1U;
    jr800_machine* step_over_machine =
        jr800_machine_create_jr800(&step_over_configuration);
    if (step_over_machine == nullptr) {
        passed = false;
        std::cerr << "Step-over machine creation failed\n";
    } else {
        constexpr std::array<std::uint8_t, 11U> step_over_program{
            0x8EU, 0x21U, 0xFFU,
            0x8DU, 0x03U,
            0x86U, 0x55U,
            0x01U,
            0x86U, 0x42U,
            0x39U,
        };
        const auto step_over_rom = make_rom(step_over_program);
        jr800_machine_state step_over_state{};
        passed &= expect(
            jr800_machine_load_logical_rom(
                step_over_machine,
                step_over_rom.data(),
                static_cast<std::uint32_t>(step_over_rom.size())
            ) == JR800_STATUS_OK
                && jr800_machine_step(step_over_machine, &stop)
                    == JR800_STATUS_OK
                && jr800_machine_set_execution_breakpoint(
                    step_over_machine,
                    0x8003U,
                    1U
                ) == JR800_STATUS_OK
                && jr800_machine_set_execution_breakpoint(
                    step_over_machine,
                    0x8005U,
                    1U
                ) == JR800_STATUS_OK
                && jr800_machine_step_over(step_over_machine, 10U, &stop)
                    == JR800_STATUS_OK
                && stop.reason == JR800_STOP_ADDRESS_REACHED
                && stop.trigger_address == 0x8005U
                && stop.continuation_address_valid == 0U
                && combine(
                    stop.instructions_executed_low,
                    stop.instructions_executed_high
                ) == 3U
                && jr800_machine_get_state(step_over_machine, &step_over_state)
                    == JR800_STATUS_OK
                && step_over_state.pc == 0x8005U
                && step_over_state.sp == 0x21FFU
                && step_over_state.a == 0x42U,
            "C ABI step-over did not reach the call return address"
        );
        passed &= expect(
            jr800_machine_reset(step_over_machine) == JR800_STATUS_OK
                && jr800_machine_step(step_over_machine, &stop)
                    == JR800_STATUS_OK
                && jr800_machine_step_over(step_over_machine, 1U, &stop)
                    == JR800_STATUS_OK
                && stop.reason == JR800_STOP_INSTRUCTION_LIMIT
                && stop.continuation_address_valid == 1U
                && stop.continuation_address == 0x8005U
                && combine(
                    stop.instructions_executed_low,
                    stop.instructions_executed_high
                ) == 1U,
            "C ABI bounded step-over lost its continuation address"
        );
        jr800_machine_destroy(step_over_machine);
    }

    jr800_hardware_configuration step_out_configuration{};
    step_out_configuration.abi_version = JR800_WASM_ABI_VERSION;
    step_out_configuration.standard_ram_enabled = 1U;
    jr800_machine* step_out_machine =
        jr800_machine_create_jr800(&step_out_configuration);
    if (step_out_machine == nullptr) {
        passed = false;
        std::cerr << "Step-out machine creation failed\n";
    } else {
        constexpr std::array<std::uint8_t, 23U> step_out_program{
            0x8EU, 0x21U, 0xFFU,
            0xBDU, 0x80U, 0x10U,
            0x01U, 0x01U, 0x01U, 0x01U,
            0x01U, 0x01U, 0x01U, 0x01U,
            0x01U, 0x01U,
            0x8DU, 0x03U,
            0x39U,
            0x01U, 0x01U,
            0x01U,
            0x39U,
        };
        const auto step_out_rom = make_rom(step_out_program);
        jr800_machine_state completed_state{};
        jr800_step_out_state continuation{};
        passed &= expect(
            jr800_machine_load_logical_rom(
                step_out_machine,
                step_out_rom.data(),
                static_cast<std::uint32_t>(step_out_rom.size())
            ) == JR800_STATUS_OK
                && jr800_machine_step(step_out_machine, &stop)
                    == JR800_STATUS_OK
                && jr800_machine_step(step_out_machine, &stop)
                    == JR800_STATUS_OK
                && jr800_machine_set_execution_breakpoint(
                    step_out_machine,
                    0x8010U,
                    1U
                ) == JR800_STATUS_OK
                && jr800_machine_set_execution_breakpoint(
                    step_out_machine,
                    0x8006U,
                    1U
                ) == JR800_STATUS_OK
                && jr800_machine_step_out(
                    step_out_machine,
                    2U,
                    &continuation,
                    &stop
                ) == JR800_STATUS_OK
                && stop.reason == JR800_STOP_INSTRUCTION_LIMIT
                && combine(
                    stop.instructions_executed_low,
                    stop.instructions_executed_high
                ) == 2U
                && continuation.continued == 1U
                && continuation.nesting_depth_low == 1U
                && continuation.nesting_depth_high == 0U,
            "C ABI bounded step-out lost its nested-call state"
        );
        passed &= expect(
            jr800_machine_step_out(
                step_out_machine,
                2U,
                &continuation,
                &stop
            ) == JR800_STATUS_OK
                && stop.reason == JR800_STOP_STEP_OUT_COMPLETE
                && stop.trigger_address == 0x8006U
                && combine(
                    stop.instructions_executed_low,
                    stop.instructions_executed_high
                ) == 2U
                && continuation.continued == 0U
                && continuation.nesting_depth_low == 0U
                && continuation.nesting_depth_high == 0U
                && jr800_machine_get_state(step_out_machine, &completed_state)
                    == JR800_STATUS_OK
                && completed_state.pc == 0x8006U
                && completed_state.sp == 0x21FFU,
            "C ABI continued step-out did not complete the original frame"
        );
        jr800_machine_destroy(step_out_machine);
    }

    jr800_hardware_configuration calendar_configuration{};
    calendar_configuration.abi_version = JR800_WASM_ABI_VERSION;
    calendar_configuration.calendar_enabled = 1U;
    jr800_machine* calendar_machine =
        jr800_machine_create_jr800(&calendar_configuration);
    if (calendar_machine == nullptr) {
        passed = false;
        std::cerr << "Calendar machine creation failed\n";
    } else {
        constexpr std::array<std::uint8_t, 5U> calendar_program{
            0x86U, 0x0CU,
            0xB7U, 0x06U, 0x0DU,
        };
        const auto calendar_rom = make_rom(calendar_program);
        std::uint8_t second{};
        jr800_machine_state initial_calendar_state{};
        jr800_machine_state active_calendar_state{};
        jr800_machine_state post_tick_calendar_state{};
        passed &= expect(
            jr800_machine_advance_calendar_oscillator_ticks(
                calendar_machine,
                0U
            ) == JR800_STATUS_OK
                && jr800_machine_get_state(
                    calendar_machine,
                    &initial_calendar_state
                ) == JR800_STATUS_OK
                && initial_calendar_state.calendar_alarm_terminal
                    == JR800_CALENDAR_ALARM_TERMINAL_RELEASED,
            "Initial calendar state did not cross the C ABI boundary"
        );
        passed &= expect(
            jr800_machine_load_logical_rom(
                    calendar_machine,
                    calendar_rom.data(),
                    static_cast<std::uint32_t>(calendar_rom.size())
                ) == JR800_STATUS_OK
                && jr800_machine_step(calendar_machine, &stop)
                    == JR800_STATUS_OK
                && jr800_machine_step(calendar_machine, &stop)
                    == JR800_STATUS_OK
                && jr800_machine_get_state(
                    calendar_machine,
                    &active_calendar_state
                ) == JR800_STATUS_OK
                && active_calendar_state.calendar_alarm_terminal
                    == JR800_CALENDAR_ALARM_TERMINAL_PULL_LOW,
            "Active calendar state did not cross the C ABI boundary"
        );
        passed &= expect(
            jr800_machine_advance_calendar_oscillator_ticks(
                    calendar_machine,
                    32'767U
                ) == JR800_STATUS_OK
                && jr800_machine_read_memory(
                    calendar_machine,
                    0x0600U,
                    &second,
                    1U
                ) == JR800_STATUS_OK
                && second == 0U
                && jr800_machine_advance_calendar_oscillator_ticks(
                    calendar_machine,
                    1U
                ) == JR800_STATUS_OK
                && jr800_machine_read_memory(
                    calendar_machine,
                    0x0600U,
                    &second,
                    1U
                ) == JR800_STATUS_OK
                && second == 1U
                && jr800_machine_get_state(
                    calendar_machine,
                    &post_tick_calendar_state
                ) == JR800_STATUS_OK
                && post_tick_calendar_state.calendar_alarm_terminal
                    == JR800_CALENDAR_ALARM_TERMINAL_PULL_LOW,
            "Post-tick calendar state did not cross the C ABI boundary"
        );
        jr800_machine_destroy(calendar_machine);
    }

    jr800_hardware_configuration calendar_adjust_configuration{};
    calendar_adjust_configuration.abi_version = JR800_WASM_ABI_VERSION;
    calendar_adjust_configuration.calendar_enabled = 1U;
    jr800_machine* calendar_adjust_machine =
        jr800_machine_create_jr800(&calendar_adjust_configuration);
    if (calendar_adjust_machine == nullptr) {
        passed = false;
        std::cerr << "Calendar adjustment machine creation failed\n";
    } else {
        constexpr std::array<std::uint8_t, 10U> calendar_adjust_program{
            0x86U, 0x00U,
            0xB7U, 0x06U, 0x00U,
            0x86U, 0x03U,
            0xB7U, 0x06U, 0x01U,
        };
        const auto calendar_adjust_rom = make_rom(calendar_adjust_program);
        bool ready_to_adjust = jr800_machine_load_logical_rom(
            calendar_adjust_machine,
            calendar_adjust_rom.data(),
            static_cast<std::uint32_t>(calendar_adjust_rom.size())
        ) == JR800_STATUS_OK;
        for (std::size_t instruction = 0U;
             instruction < 4U && ready_to_adjust;
             ++instruction) {
            ready_to_adjust = jr800_machine_step(
                calendar_adjust_machine,
                &stop
            ) == JR800_STATUS_OK;
        }
        std::array<std::uint8_t, 3U> before_adjust{};
        std::array<std::uint8_t, 3U> after_adjust{};
        jr800_machine_state before_adjust_state{};
        jr800_machine_state after_adjust_state{};
        ready_to_adjust = ready_to_adjust
            && jr800_machine_read_memory(
                calendar_adjust_machine,
                0x0600U,
                before_adjust.data(),
                before_adjust.size()
            ) == JR800_STATUS_OK
            && before_adjust
                == std::array<std::uint8_t, 3U>{0x00U, 0x03U, 0x00U}
            && jr800_machine_get_state(
                calendar_adjust_machine,
                &before_adjust_state
            ) == JR800_STATUS_OK;
        const auto history_before_adjust = jr800_machine_history_count(
            calendar_adjust_machine
        );
        const bool adjusted = ready_to_adjust
            && jr800_machine_adjust_calendar_seconds(
                calendar_adjust_machine
            ) == JR800_STATUS_OK
            && jr800_machine_read_memory(
                calendar_adjust_machine,
                0x0600U,
                after_adjust.data(),
                after_adjust.size()
            ) == JR800_STATUS_OK
            && after_adjust
                == std::array<std::uint8_t, 3U>{0x00U, 0x00U, 0x01U}
            && jr800_machine_get_state(
                calendar_adjust_machine,
                &after_adjust_state
            ) == JR800_STATUS_OK
            && after_adjust_state.pc == before_adjust_state.pc
            && after_adjust_state.cycle_count_low
                == before_adjust_state.cycle_count_low
            && after_adjust_state.cycle_count_high
                == before_adjust_state.cycle_count_high
            && jr800_machine_history_count(calendar_adjust_machine)
                == history_before_adjust;
        passed &= expect(
            adjusted,
            "Qualified calendar adjustment did not cross the C ABI boundary"
        );
        jr800_machine_destroy(calendar_adjust_machine);
    }

    jr800_hardware_configuration calendar_ratio_configuration{};
    calendar_ratio_configuration.abi_version = JR800_WASM_ABI_VERSION;
    calendar_ratio_configuration.calendar_enabled = 1U;
    calendar_ratio_configuration.calendar_cpu_cycle_ratio =
        JR800_CALENDAR_CPU_CYCLE_RATIO_E030_NOMINAL_1_2288_MHZ;
    jr800_machine* calendar_ratio_machine =
        jr800_machine_create_jr800(&calendar_ratio_configuration);
    if (calendar_ratio_machine == nullptr) {
        passed = false;
        std::cerr << "Calendar ratio machine creation failed\n";
    } else {
        constexpr std::array<std::uint8_t, 5U> calendar_program{
            0x86U, 0x08U,
            0xB7U, 0x06U, 0x0DU,
        };
        const auto calendar_rom = make_rom(calendar_program);
        std::uint8_t second{};
        bool ratio_progressed = jr800_machine_load_logical_rom(
            calendar_ratio_machine,
            calendar_rom.data(),
            static_cast<std::uint32_t>(calendar_rom.size())
        ) == JR800_STATUS_OK;
        std::uint32_t setup_cycles{};
        for (std::size_t instruction = 0U;
             instruction < 2U && ratio_progressed;
             ++instruction) {
            ratio_progressed = jr800_machine_step(
                calendar_ratio_machine,
                &stop
            ) == JR800_STATUS_OK;
            setup_cycles += stop.cycles;
        }
        ratio_progressed = ratio_progressed && setup_cycles == 6U
            && jr800_machine_advance_calendar_oscillator_ticks(
                calendar_ratio_machine,
                32'767U
            ) == JR800_STATUS_OK;
        for (std::size_t instruction = 0U;
             instruction < 31U && ratio_progressed;
             ++instruction) {
            ratio_progressed = jr800_machine_step(
                calendar_ratio_machine,
                &stop
            ) == JR800_STATUS_OK && stop.cycles == 1U;
        }
        ratio_progressed = ratio_progressed
            && jr800_machine_read_memory(
                calendar_ratio_machine,
                0x0600U,
                &second,
                1U
            ) == JR800_STATUS_OK
            && second == 0U
            && jr800_machine_step(calendar_ratio_machine, &stop)
                == JR800_STATUS_OK
            && stop.cycles == 1U
            && jr800_machine_read_memory(
                calendar_ratio_machine,
                0x0600U,
                &second,
                1U
            ) == JR800_STATUS_OK
            && second == 1U;
        passed &= expect(
            ratio_progressed,
            "Calendar CPU-cycle ratio did not cross the C ABI boundary"
        );
        jr800_machine_destroy(calendar_ratio_machine);
    }

    jr800_hardware_configuration lcd_configuration{};
    lcd_configuration.abi_version = JR800_WASM_ABI_VERSION;
    lcd_configuration.lcd_enabled = 1U;
    jr800_machine* lcd_machine =
        jr800_machine_create_jr800(&lcd_configuration);
    if (lcd_machine == nullptr) {
        passed = false;
        std::cerr << "LCD machine creation failed\n";
    } else {
        constexpr std::array<std::uint8_t, 38U> lcd_program{
            0x86U, 0x3EU,
            0xB7U, 0x0AU, 0x01U,
            0x86U, 0x39U,
            0xB7U, 0x0AU, 0x01U,
            0x86U, 0x00U,
            0xB7U, 0x0AU, 0x01U,
            0x86U, 0x01U,
            0xB7U, 0x0BU, 0x01U,
            0x86U, 0x00U,
            0xB7U, 0x0AU, 0x01U,
            0xB6U, 0x0BU, 0x01U,
            0x86U, 0x2EU,
            0xB7U, 0x0AU, 0x01U,
            0x86U, 0x80U,
            0xB7U, 0x0BU, 0x01U,
        };
        const auto lcd_rom = make_rom(lcd_program);
        jr800_machine_state lcd_initial_state{};
        jr800_machine_state lcd_final_state{};
        passed &= expect(
            jr800_machine_load_logical_rom(
                lcd_machine,
                lcd_rom.data(),
                static_cast<std::uint32_t>(lcd_rom.size())
            ) == JR800_STATUS_OK
                && jr800_machine_get_state(
                    lcd_machine,
                    &lcd_initial_state
                ) == JR800_STATUS_OK
                && lcd_initial_state.lcd_substituted_data_read_count_valid
                    == 1U
                && combine(
                    lcd_initial_state.lcd_substituted_data_read_count_low,
                    lcd_initial_state.lcd_substituted_data_read_count_high
                ) == 0U,
            "LCD machine ROM load or initial diagnostic failed"
        );
        panel.assign(panel.size(), 0xA5U);
        passed &= expect(
            jr800_machine_copy_lcd_panel(
                lcd_machine,
                panel.data(),
                JR800_LCD_PANEL_DOT_COUNT - 1U
            ) == JR800_STATUS_BUFFER_TOO_SMALL
                && std::all_of(
                    panel.begin(),
                    panel.end(),
                    [](std::uint8_t value) { return value == 0xA5U; }
                ),
            "Short LCD panel buffer was partially changed"
        );
        passed &= expect(
            jr800_machine_copy_lcd_panel(
                lcd_machine,
                panel.data(),
                panel.size()
            ) == JR800_STATUS_OK
                && std::all_of(
                    panel.begin(),
                    panel.end(),
                    [](std::uint8_t value) {
                        return value == JR800_LCD_DOT_OFF;
                    }
                ),
            "Reset LCD panel transport differs"
        );
        for (auto& indicator : indicators) {
            indicator = {0xA5A5A5A5U, 0xA5A5A5A5U};
        }
        passed &= expect(
            jr800_machine_copy_lcd_indicators(
                lcd_machine,
                indicators.data(),
                JR800_LCD_INDICATOR_COUNT - 1U
            ) == JR800_STATUS_BUFFER_TOO_SMALL
                && std::all_of(
                    indicators.begin(),
                    indicators.end(),
                    [](const jr800_lcd_indicator_raw& indicator) {
                        return indicator.value_known == 0xA5A5A5A5U
                            && indicator.value == 0xA5A5A5A5U;
                    }
                ),
            "Short LCD indicator buffer was partially changed"
        );
        passed &= expect(
            jr800_machine_copy_lcd_indicators(
                lcd_machine,
                indicators.data(),
                indicators.size()
            ) == JR800_STATUS_OK
                && std::all_of(
                    indicators.begin(),
                    indicators.end(),
                    [](const jr800_lcd_indicator_raw& indicator) {
                        return indicator.value_known == 0U
                            && indicator.value == 0U;
                    }
                ),
            "Reset LCD indicator raw values were guessed"
        );
        bool lcd_program_succeeded = true;
        for (std::size_t instruction = 0U;
             instruction < 11U;
             ++instruction) {
            lcd_program_succeeded &=
                jr800_machine_step(lcd_machine, &stop) == JR800_STATUS_OK;
        }
        passed &= expect(
            lcd_program_succeeded
                && jr800_machine_copy_lcd_panel(
                    lcd_machine,
                    panel.data(),
                    panel.size()
                ) == JR800_STATUS_OK
                && panel[0U] == JR800_LCD_DOT_UNKNOWN
                && panel[45U] == JR800_LCD_DOT_ON
                && panel[JR800_LCD_PANEL_WIDTH + 45U]
                    == JR800_LCD_DOT_OFF
                && jr800_machine_get_state(lcd_machine, &lcd_final_state)
                    == JR800_STATUS_OK
                && lcd_final_state.lcd_substituted_data_read_count_valid == 1U
                && combine(
                    lcd_final_state.lcd_substituted_data_read_count_low,
                    lcd_final_state.lcd_substituted_data_read_count_high
                ) == 1U,
            "LCD dots or substituted-read count did not cross the C ABI"
        );
        for (std::size_t instruction = 0U;
             instruction < 4U;
             ++instruction) {
            lcd_program_succeeded &=
                jr800_machine_step(lcd_machine, &stop) == JR800_STATUS_OK;
        }
        passed &= expect(
            lcd_program_succeeded
                && jr800_machine_copy_lcd_indicators(
                    lcd_machine,
                    indicators.data(),
                    indicators.size()
                ) == JR800_STATUS_OK
                && indicators[JR800_LCD_INDICATOR_PAGE_1].value_known == 1U
                && indicators[JR800_LCD_INDICATOR_PAGE_1].value == 0x80U
                && std::all_of(
                    indicators.begin() + 1,
                    indicators.end(),
                    [](const jr800_lcd_indicator_raw& indicator) {
                        return indicator.value_known == 0U
                            && indicator.value == 0U;
                    }
                ),
            "Raw LCD indicator values did not cross the C ABI"
        );
        jr800_machine_destroy(lcd_machine);
    }

    constexpr std::array<std::uint8_t, 13U> sleep_program{
        0x86U, 0x00U,
        0x97U, 0x0BU,
        0x86U, 0x20U,
        0x97U, 0x0CU,
        0x86U, 0x08U,
        0x97U, 0x08U,
        0x1AU,
    };
    const auto sleep_rom = make_rom(sleep_program);
    passed &= expect(
        jr800_machine_load_logical_rom(
            machine,
            sleep_rom.data(),
            static_cast<std::uint32_t>(sleep_rom.size())
        ) == JR800_STATUS_OK
            && jr800_machine_run(machine, 16U, &stop) == JR800_STATUS_OK
            && stop.reason == JR800_STOP_SLEEPING
            && combine(
                stop.instructions_executed_low,
                stop.instructions_executed_high
            ) == 7U,
        "JR-800 session did not enter bounded sleep"
    );
    passed &= expect(
        jr800_machine_advance_suspended_cycles(machine, 12U, &advance)
                == JR800_STATUS_OK
            && advance.suspended == 1U
            && advance.cycles_elapsed == 12U
            && advance.interrupt_known == 1U
            && advance.interrupt_source == JR800_INTERRUPT_NONE
            && advance.bus_fault == JR800_BUS_FAULT_NONE,
        "Bounded suspended-cycle advance differs"
    );
    passed &= expect(
        jr800_machine_advance_suspended_cycles(machine, 10U, &advance)
                == JR800_STATUS_OK
            && advance.suspended == 1U
            && advance.cycles_elapsed == 1U
            && advance.interrupt_known == 1U
            && advance.interrupt_source
                == JR800_INTERRUPT_TIMER_OUTPUT_COMPARE
            && advance.bus_fault == JR800_BUS_FAULT_NONE,
        "Suspended-cycle advance missed the timer boundary"
    );

    jr800_machine_destroy(synthetic);
    jr800_machine_destroy(machine);
    return passed ? 0 : 1;
}
