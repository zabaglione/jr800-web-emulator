// SPDX-License-Identifier: MIT

#include "jr8run_jr800.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "jr800/core/jr800_bus.hpp"
#include "jr800/core/jr800_machine.hpp"
#include "jr800/core/jr800_memory.hpp"
#include "jr800/formats/jr8rom.hpp"
#include "jr800/formats/linked_error.hpp"
#include "jr800/runtime/jr800_machine_runner.hpp"

namespace jr800::tools {
namespace {

struct KnownBits {
    std::uint8_t value{};
    std::uint8_t known_mask{};
};

struct KeyboardResponse {
    std::uint16_t address{};
    std::uint8_t value{};
};

struct CliOptions {
    std::filesystem::path rom_container;
    runtime::Jr800RunLimits limits;
    bool basic_boot_experiment{};
    std::optional<std::uint16_t> reset_stack_pointer;
    std::optional<std::uint16_t> reset_index_register;
    std::optional<std::uint8_t> reset_accumulator_a;
    std::optional<std::uint8_t> reset_accumulator_b;
    std::optional<KnownBits> reset_condition_code;
    std::optional<std::uint8_t> internal_ram_initial;
    std::optional<std::uint8_t> standard_ram_initial;
    std::optional<std::uint8_t> expansion_ram_initial;
    std::optional<std::uint8_t> lcd_unknown_data;
    std::optional<core::Jr800ExperimentalCalendarAddressSource>
        calendar_address_source;
    std::optional<core::Jr800ExperimentalCalendarUpperReadBits>
        calendar_upper_read_bits;
    std::optional<core::Jr800ExperimentalCalendarCpuCycleRatio>
        calendar_cpu_cycle_ratio;
    std::optional<KnownBits> port1_pins;
    std::optional<KnownBits> port2_pins;
    std::optional<bool> ram_standby_valid;
    std::optional<std::uint8_t> keyboard_window_value;
    std::vector<KeyboardResponse> keyboard_responses;
};

enum class RomContainerReadStatus : std::uint8_t {
    ok,
    io_error,
    invalid_container,
    incomplete_logical_rom,
};

struct RomContainerReadResult {
    RomContainerReadStatus status{RomContainerReadStatus::io_error};
    std::vector<std::uint8_t> bytes;
};

void print_usage(std::ostream& stream) {
    stream
        << "Usage: jr8run jr800 [--max-instructions <count>] "
           "[--max-suspended-cycles <count>]\n"
           "                     [--basic-boot-experiment]\n"
           "                     [--reset-sp <word>] [--reset-x <word>]\n"
           "                     [--reset-a <byte>] [--reset-b <byte>]\n"
           "                     [--reset-cc <value:known-mask>]\n"
           "                     [--internal-ram-initial <byte>]\n"
           "                     [--standard-ram-initial <byte>] "
           "[--expansion-ram-initial <byte>]\n"
           "                     [--lcd-unknown-data <byte>]\n"
           "                     [--calendar-address-source "
           "<a0-a3|a1-a4|a2-a5|a3-a6|a4-a7|a5-a8>]\n"
           "                     [--calendar-upper-read <zero|one>]\n"
           "                     [--calendar-cpu-cycle-ratio "
           "<e030-nominal-1.2288mhz>]\n"
           "                     [--port1-pins <value:known-mask>] "
           "[--port2-pins <value:known-mask>]\n"
           "                     [--ram-standby <valid|invalid>] "
           "[--keyboard-window-value <byte>]\n"
           "                     [--keyboard-response <address:byte>]...\n"
           "                     <rom.j8r>\n";
}

std::optional<std::uint64_t> parse_number(std::string_view text) {
    int base = 10;
    if (!text.empty() && text.front() == '$') {
        base = 16;
        text.remove_prefix(1U);
    } else if (text.size() > 2U && text[0] == '0'
               && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2U);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value,
        base
    );
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint8_t> parse_byte(std::string_view text) {
    const auto value = parse_number(text);
    if (!value.has_value() || *value > 0xFFU) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(*value);
}

std::optional<std::uint16_t> parse_word(std::string_view text) {
    const auto value = parse_number(text);
    if (!value.has_value() || *value > 0xFFFFU) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(*value);
}

std::optional<KnownBits> parse_known_bits(
    std::string_view text,
    std::uint8_t allowed_mask
) {
    const auto separator = text.find(':');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const auto value = parse_byte(text.substr(0U, separator));
    const auto known_mask = parse_byte(text.substr(separator + 1U));
    if (!value.has_value() || !known_mask.has_value()
        || (*value & static_cast<std::uint8_t>(~allowed_mask)) != 0U
        || (*known_mask & static_cast<std::uint8_t>(~allowed_mask)) != 0U) {
        return std::nullopt;
    }
    if ((*value & static_cast<std::uint8_t>(~*known_mask)) != 0U) {
        return std::nullopt;
    }
    return KnownBits{*value, *known_mask};
}

std::optional<KeyboardResponse> parse_keyboard_response(
    std::string_view text
) {
    const auto separator = text.find(':');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const auto address = parse_word(text.substr(0U, separator));
    const auto value = parse_byte(text.substr(separator + 1U));
    if (!address.has_value() || !value.has_value()
        || *address < 0x0C00U || *address > 0x0FFFU) {
        return std::nullopt;
    }
    return KeyboardResponse{*address, *value};
}

std::optional<core::Jr800ExperimentalCalendarAddressSource>
parse_calendar_address_source(std::string_view text) {
    using Source = core::Jr800ExperimentalCalendarAddressSource;
    if (text == "a0-a3") {
        return Source::cpu_a0_to_a3;
    }
    if (text == "a1-a4") {
        return Source::cpu_a1_to_a4;
    }
    if (text == "a2-a5") {
        return Source::cpu_a2_to_a5;
    }
    if (text == "a3-a6") {
        return Source::cpu_a3_to_a6;
    }
    if (text == "a4-a7") {
        return Source::cpu_a4_to_a7;
    }
    if (text == "a5-a8") {
        return Source::cpu_a5_to_a8;
    }
    return std::nullopt;
}

std::optional<core::Jr800ExperimentalCalendarUpperReadBits>
parse_calendar_upper_read_bits(std::string_view text) {
    using Upper = core::Jr800ExperimentalCalendarUpperReadBits;
    if (text == "zero") {
        return Upper::all_zero;
    }
    if (text == "one") {
        return Upper::all_one;
    }
    return std::nullopt;
}

std::optional<core::Jr800ExperimentalCalendarCpuCycleRatio>
parse_calendar_cpu_cycle_ratio(std::string_view text) {
    using Ratio = core::Jr800ExperimentalCalendarCpuCycleRatio;
    if (text == "e030-nominal-1.2288mhz") {
        return Ratio::e030_nominal_1_2288_mhz;
    }
    return std::nullopt;
}

std::optional<CliOptions> parse_options(int argc, char* argv[]) {
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto require_value = [&]() -> std::optional<std::string_view> {
            if (index + 1 >= argc) {
                std::cerr << "jr8run jr800: missing value for " << argument
                          << '\n';
                return std::nullopt;
            }
            ++index;
            return std::string_view{argv[index]};
        };

        if (argument == "--basic-boot-experiment") {
            options.basic_boot_experiment = true;
        } else if (argument == "--max-instructions") {
            const auto text = require_value();
            const auto value = text.has_value()
                ? parse_number(*text)
                : std::nullopt;
            if (!value.has_value() || *value == 0U) {
                std::cerr << "jr8run jr800: invalid instruction limit\n";
                return std::nullopt;
            }
            options.limits.instructions = *value;
        } else if (argument == "--max-suspended-cycles") {
            const auto text = require_value();
            const auto value = text.has_value()
                ? parse_number(*text)
                : std::nullopt;
            if (!value.has_value() || *value == 0U
                || *value > std::numeric_limits<std::uint32_t>::max()) {
                std::cerr << "jr8run jr800: invalid suspended-cycle limit\n";
                return std::nullopt;
            }
            options.limits.suspended_cycles =
                static_cast<std::uint32_t>(*value);
        } else if (argument == "--reset-sp" || argument == "--reset-x") {
            const auto text = require_value();
            const auto value = text.has_value()
                ? parse_word(*text)
                : std::nullopt;
            if (!value.has_value()) {
                std::cerr << "jr8run jr800: invalid word for " << argument
                          << '\n';
                return std::nullopt;
            }
            if (argument == "--reset-sp") {
                options.reset_stack_pointer = *value;
            } else {
                options.reset_index_register = *value;
            }
        } else if (argument == "--reset-a" || argument == "--reset-b"
                   || argument == "--internal-ram-initial"
                   || argument == "--standard-ram-initial"
                   || argument == "--expansion-ram-initial"
                   || argument == "--lcd-unknown-data"
                   || argument == "--keyboard-window-value") {
            const auto text = require_value();
            const auto value = text.has_value()
                ? parse_byte(*text)
                : std::nullopt;
            if (!value.has_value()) {
                std::cerr << "jr8run jr800: invalid byte for " << argument
                          << '\n';
                return std::nullopt;
            }
            if (argument == "--reset-a") {
                options.reset_accumulator_a = *value;
            } else if (argument == "--reset-b") {
                options.reset_accumulator_b = *value;
            } else if (argument == "--internal-ram-initial") {
                options.internal_ram_initial = *value;
            } else if (argument == "--standard-ram-initial") {
                options.standard_ram_initial = *value;
            } else if (argument == "--expansion-ram-initial") {
                options.expansion_ram_initial = *value;
            } else if (argument == "--lcd-unknown-data") {
                options.lcd_unknown_data = *value;
            } else {
                options.keyboard_window_value = *value;
            }
        } else if (argument == "--reset-cc") {
            const auto text = require_value();
            const auto condition_code = text.has_value()
                ? parse_known_bits(*text, 0x2FU)
                : std::nullopt;
            if (!condition_code.has_value()) {
                std::cerr << "jr8run jr800: invalid reset condition code\n";
                return std::nullopt;
            }
            options.reset_condition_code = *condition_code;
        } else if (argument == "--calendar-address-source") {
            const auto text = require_value();
            const auto source = text.has_value()
                ? parse_calendar_address_source(*text)
                : std::nullopt;
            if (!source.has_value()) {
                std::cerr << "jr8run jr800: invalid calendar address source\n";
                return std::nullopt;
            }
            options.calendar_address_source = *source;
        } else if (argument == "--calendar-upper-read") {
            const auto text = require_value();
            const auto upper = text.has_value()
                ? parse_calendar_upper_read_bits(*text)
                : std::nullopt;
            if (!upper.has_value()) {
                std::cerr << "jr8run jr800: invalid calendar upper read bits\n";
                return std::nullopt;
            }
            options.calendar_upper_read_bits = *upper;
        } else if (argument == "--calendar-cpu-cycle-ratio") {
            const auto text = require_value();
            const auto ratio = text.has_value()
                ? parse_calendar_cpu_cycle_ratio(*text)
                : std::nullopt;
            if (!ratio.has_value()) {
                std::cerr << "jr8run jr800: invalid calendar CPU-cycle ratio\n";
                return std::nullopt;
            }
            options.calendar_cpu_cycle_ratio = *ratio;
        } else if (argument == "--port1-pins"
                   || argument == "--port2-pins") {
            const auto text = require_value();
            const auto pins = text.has_value()
                ? parse_known_bits(
                    *text,
                    argument == "--port1-pins" ? 0xFFU : 0x1FU
                )
                : std::nullopt;
            if (!pins.has_value()) {
                std::cerr << "jr8run jr800: invalid pin state for " << argument
                          << '\n';
                return std::nullopt;
            }
            if (argument == "--port1-pins") {
                options.port1_pins = *pins;
            } else {
                options.port2_pins = *pins;
            }
        } else if (argument == "--ram-standby") {
            const auto text = require_value();
            if (!text.has_value()
                || (*text != "valid" && *text != "invalid")) {
                std::cerr << "jr8run jr800: invalid RAM standby state\n";
                return std::nullopt;
            }
            options.ram_standby_valid = *text == "valid";
        } else if (argument == "--keyboard-response") {
            const auto text = require_value();
            const auto response = text.has_value()
                ? parse_keyboard_response(*text)
                : std::nullopt;
            if (!response.has_value()) {
                std::cerr << "jr8run jr800: invalid keyboard response\n";
                return std::nullopt;
            }
            for (const auto& existing : options.keyboard_responses) {
                if (existing.address == response->address) {
                    std::cerr
                        << "jr8run jr800: duplicate keyboard response\n";
                    return std::nullopt;
                }
            }
            options.keyboard_responses.push_back(*response);
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8run jr800: unknown option: " << argument << '\n';
            return std::nullopt;
        } else if (!options.rom_container.empty()) {
            std::cerr << "jr8run jr800: exactly one JR8ROM input is required\n";
            return std::nullopt;
        } else {
            options.rom_container = argument;
        }
    }

    if (options.rom_container.empty()) {
        std::cerr << "jr8run jr800: one JR8ROM input is required\n";
        return std::nullopt;
    }
    const auto has_explicit_machine_input =
        options.reset_stack_pointer.has_value()
        || options.reset_index_register.has_value()
        || options.reset_accumulator_a.has_value()
        || options.reset_accumulator_b.has_value()
        || options.reset_condition_code.has_value()
        || options.internal_ram_initial.has_value()
        || options.standard_ram_initial.has_value()
        || options.expansion_ram_initial.has_value()
        || options.lcd_unknown_data.has_value()
        || options.calendar_address_source.has_value()
        || options.calendar_upper_read_bits.has_value()
        || options.calendar_cpu_cycle_ratio.has_value()
        || options.port1_pins.has_value()
        || options.port2_pins.has_value()
        || options.ram_standby_valid.has_value()
        || options.keyboard_window_value.has_value()
        || !options.keyboard_responses.empty();
    if (options.basic_boot_experiment && has_explicit_machine_input) {
        std::cerr
            << "jr8run jr800: BASIC boot experiment cannot be combined "
               "with explicit machine inputs\n";
        return std::nullopt;
    }
    if (options.basic_boot_experiment) {
        options.internal_ram_initial = 0x00U;
        options.standard_ram_initial = 0x00U;
        options.expansion_ram_initial = 0x00U;
        options.lcd_unknown_data = 0x00U;
        options.calendar_address_source =
            core::Jr800ExperimentalCalendarAddressSource::cpu_a0_to_a3;
        options.calendar_upper_read_bits =
            core::Jr800ExperimentalCalendarUpperReadBits::all_zero;
        options.calendar_cpu_cycle_ratio =
            core::Jr800ExperimentalCalendarCpuCycleRatio::e030_nominal_1_2288_mhz;
        options.port1_pins = KnownBits{0xFFU, 0xFFU};
        options.port2_pins = KnownBits{0x1EU, 0x1FU};
        options.ram_standby_valid = false;
        options.keyboard_window_value = 0xFFU;
    }
    if (options.expansion_ram_initial.has_value()
        && !options.standard_ram_initial.has_value()) {
        std::cerr << "jr8run jr800: expansion RAM requires explicit standard RAM\n";
        return std::nullopt;
    }
    if (options.calendar_address_source.has_value()
        != options.calendar_upper_read_bits.has_value()) {
        std::cerr << "jr8run jr800: both calendar options are required\n";
        return std::nullopt;
    }
    if (options.calendar_cpu_cycle_ratio.has_value()
        && !options.calendar_address_source.has_value()) {
        std::cerr
            << "jr8run jr800: calendar CPU-cycle ratio requires calendar options\n";
        return std::nullopt;
    }
    return options;
}

RomContainerReadResult read_rom_container(
    const std::filesystem::path& path
) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        std::cerr << "jr8run jr800: cannot inspect JR8ROM input\n";
        return {RomContainerReadStatus::io_error, {}};
    }
    if (size > formats::jr8rom::maximum_encoded_size) {
        std::cerr << "jr8run jr800: JR8ROM input exceeds the size limit\n";
        return {RomContainerReadStatus::io_error, {}};
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "jr8run jr800: cannot open JR8ROM input\n";
        return {RomContainerReadStatus::io_error, {}};
    }
    std::vector<std::uint8_t> encoded(static_cast<std::size_t>(size));
    if (!encoded.empty()) {
        input.read(
            reinterpret_cast<char*>(encoded.data()),
            static_cast<std::streamsize>(encoded.size())
        );
    }
    if (!input) {
        std::cerr << "jr8run jr800: JR8ROM input read failed\n";
        return {RomContainerReadStatus::io_error, {}};
    }

    try {
        const auto image = formats::jr8rom::read(encoded);
        auto logical_rom = formats::jr8rom::extract_range(
            image,
            core::jr800_logical_rom_base,
            core::jr800_logical_rom_size
        );
        if (!logical_rom.has_value()) {
            std::cerr << "jr8run jr800: JR8ROM does not completely cover "
                         "logical ROM range $8000-$FFFF\n";
            return {RomContainerReadStatus::incomplete_logical_rom, {}};
        }
        return {RomContainerReadStatus::ok, std::move(*logical_rom)};
    } catch (const formats::linked::Error& format_error) {
        std::cerr << "jr8run jr800: invalid JR8ROM: " << format_error.what();
        if (format_error.byte_offset().has_value()) {
            std::cerr << " at byte " << *format_error.byte_offset();
        }
        std::cerr << '\n';
        return {RomContainerReadStatus::invalid_container, {}};
    }
}

std::string_view run_stop_name(runtime::Jr800RunStopReason reason) {
    using Reason = runtime::Jr800RunStopReason;
    switch (reason) {
    case Reason::instruction_limit:
        return "instruction-limit";
    case Reason::cpu_fault:
        return "cpu-fault";
    case Reason::suspended_cycle_limit:
        return "suspended-cycle-limit";
    case Reason::invalid_limits:
        return "invalid-limits";
    case Reason::machine_not_initialized:
        return "machine-not-initialized";
    }
    return "unknown";
}

std::string_view cpu_fault_name(core::CpuFault fault) {
    using Fault = core::CpuFault;
    switch (fault) {
    case Fault::none:
        return "none";
    case Fault::unsupported_opcode:
        return "unsupported-opcode";
    case Fault::unimplemented_operation:
        return "unimplemented-operation";
    case Fault::bus_access:
        return "bus-access";
    case Fault::unknown_state:
        return "unknown-state";
    case Fault::unknown_interrupt_request:
        return "unknown-interrupt-request";
    case Fault::bus_advance:
        return "bus-advance";
    }
    return "unknown";
}

std::string_view bus_fault_name(core::BusFault fault) {
    using Fault = core::BusFault;
    switch (fault) {
    case Fault::none:
        return "none";
    case Fault::backing_store_unavailable:
        return "backing-store-unavailable";
    case Fault::uninitialized_read:
        return "uninitialized-read";
    case Fault::unsupported_access:
        return "unsupported-access";
    case Fault::read_only_write:
        return "read-only-write";
    case Fault::device_state_unknown:
        return "device-state-unknown";
    case Fault::device_state_unsupported:
        return "device-state-unsupported";
    }
    return "unknown";
}

std::string_view access_name(core::AccessKind access) {
    using Kind = core::AccessKind;
    switch (access) {
    case Kind::instruction_fetch:
        return "instruction-fetch";
    case Kind::data_read:
        return "data-read";
    case Kind::data_write:
        return "data-write";
    }
    return "unknown";
}

std::string_view region_name(core::Jr800MemoryRegion region) {
    using Region = core::Jr800MemoryRegion;
    switch (region) {
    case Region::cpu_internal_registers:
        return "cpu-internal-registers";
    case Region::reserved:
        return "reserved";
    case Region::cpu_internal_ram:
        return "cpu-internal-ram";
    case Region::calendar_clock:
        return "calendar-clock";
    case Region::lcd:
        return "lcd";
    case Region::keyboard:
        return "keyboard";
    case Region::standard_ram:
        return "standard-ram";
    case Region::expansion_ram:
        return "expansion-ram";
    case Region::standard_rom:
        return "standard-rom";
    case Region::expansion_rom:
        return "expansion-rom";
    case Region::cpu_internal_rom:
        return "cpu-internal-rom";
    }
    return "unknown";
}

std::string_view state_part_name(core::CpuStatePart part) {
    using Part = core::CpuStatePart;
    switch (part) {
    case Part::none:
        return "none";
    case Part::program_counter:
        return "program-counter";
    case Part::stack_pointer:
        return "stack-pointer";
    case Part::index_register:
        return "index-register";
    case Part::accumulator_a:
        return "accumulator-a";
    case Part::accumulator_b:
        return "accumulator-b";
    case Part::condition_code:
        return "condition-code";
    }
    return "unknown";
}

core::Jr800ExperimentalMachineConfiguration machine_configuration(
    const CliOptions& options
) {
    core::Jr800ExperimentalMachineConfiguration configuration;
    if (options.internal_ram_initial.has_value()) {
        configuration.internal_ram =
            core::Jr800ExperimentalInternalRamConfiguration{
                *options.internal_ram_initial,
            };
    }
    if (options.standard_ram_initial.has_value()) {
        configuration.memory = core::Jr800ExperimentalMemoryConfiguration{
            *options.standard_ram_initial,
            options.expansion_ram_initial,
        };
    }
    if (options.lcd_unknown_data.has_value()) {
        configuration.lcd = core::Jr800ExperimentalLcdConfiguration{
            *options.lcd_unknown_data,
        };
    }
    if (options.calendar_address_source.has_value()) {
        configuration.calendar = core::Jr800ExperimentalCalendarConfiguration{
            *options.calendar_address_source,
            *options.calendar_upper_read_bits,
            options.calendar_cpu_cycle_ratio,
        };
    }
    return configuration;
}

std::optional<bool> configured_flag(
    const std::optional<KnownBits>& condition_code,
    core::ConditionCode flag
) {
    const auto mask = core::condition_mask(flag);
    if (!condition_code.has_value()
        || (condition_code->known_mask & mask) == 0U) {
        return std::nullopt;
    }
    return (condition_code->value & mask) != 0U;
}

core::Jr800ExperimentalResetStateConfiguration reset_state_configuration(
    const CliOptions& options
) {
    return {
        .stack_pointer = options.reset_stack_pointer,
        .index_register = options.reset_index_register,
        .accumulator_a = options.reset_accumulator_a,
        .accumulator_b = options.reset_accumulator_b,
        .half_carry = configured_flag(
            options.reset_condition_code,
            core::ConditionCode::half_carry
        ),
        .negative = configured_flag(
            options.reset_condition_code,
            core::ConditionCode::negative
        ),
        .zero = configured_flag(
            options.reset_condition_code,
            core::ConditionCode::zero
        ),
        .overflow = configured_flag(
            options.reset_condition_code,
            core::ConditionCode::overflow
        ),
        .carry = configured_flag(
            options.reset_condition_code,
            core::ConditionCode::carry
        ),
    };
}

bool apply_inputs(core::Jr800Machine& machine, const CliOptions& options) {
    if (options.port1_pins.has_value()) {
        machine.set_port1_pin_state(
            options.port1_pins->value,
            options.port1_pins->known_mask
        );
    }
    if (options.port2_pins.has_value()) {
        machine.set_port2_pin_state(
            options.port2_pins->value,
            options.port2_pins->known_mask
        );
    }
    if (options.ram_standby_valid.has_value()) {
        machine.set_ram_standby_power_valid(
            *options.ram_standby_valid,
            true
        );
    }
    if (options.keyboard_window_value.has_value()) {
        for (std::uint32_t address = 0x0C00U; address <= 0x0FFFU; ++address) {
            if (!machine.set_keyboard_bus_response(
                    static_cast<std::uint16_t>(address),
                    *options.keyboard_window_value,
                    true
                )) {
                return false;
            }
        }
    }
    for (const auto& response : options.keyboard_responses) {
        if (!machine.set_keyboard_bus_response(
                response.address,
                response.value,
                true
            )) {
            return false;
        }
    }
    return true;
}

void print_summary(
    const runtime::Jr800RunSummary& summary
) {
    const auto calendar_alarm_terminal = [&]() -> std::string_view {
        if (!summary.calendar_alarm_terminal.connected) {
            return "disconnected";
        }
        if (!summary.calendar_alarm_terminal.pull_low.has_value()) {
            return "unknown";
        }
        return *summary.calendar_alarm_terminal.pull_low
            ? "pull-low"
            : "released";
    }();
    const auto port2_timer_output = [&]() -> std::string_view {
        if (!summary.port2_timer_output.output_enabled) {
            return "disabled";
        }
        if (!summary.port2_timer_output.level.has_value()) {
            return "unknown";
        }
        return *summary.port2_timer_output.level ? "high" : "low";
    }();
    const auto port2_timer_output_observed = [&]() {
        std::string result;
        const auto append = [&result](std::string_view state) {
            if (!result.empty()) {
                result += ',';
            }
            result += state;
        };
        if (summary.port2_timer_output_coverage.disabled) {
            append("disabled");
        }
        if (summary.port2_timer_output_coverage.unknown) {
            append("unknown");
        }
        if (summary.port2_timer_output_coverage.low) {
            append("low");
        }
        if (summary.port2_timer_output_coverage.high) {
            append("high");
        }
        return result;
    }();
    std::cout << "stop=" << run_stop_name(summary.stop_reason)
              << " instructions=" << summary.instructions_completed
              << " execution-cycles=" << summary.execution_cycles
              << " suspended-cycles=" << summary.suspended_cycles
              << " interrupt-entries=" << summary.interrupt_entries
              << " timer-input-capture-interrupts="
              << summary.timer_input_capture_interrupts
              << " timer-output-compare-interrupts="
              << summary.timer_output_compare_interrupts
              << " timer-overflow-interrupts="
              << summary.timer_overflow_interrupts
              << " serial-interrupts=" << summary.serial_interrupts
              << " instructions-after-last-interrupt="
              << summary.instructions_after_last_interrupt
              << " sleep-entries=" << summary.sleep_entries
              << " wait-entries=" << summary.wait_entries
              << " sleep-resumes=" << summary.sleep_resumes
              << " keyboard-read-attempts="
              << summary.keyboard_activity.read_attempts
              << " keyboard-distinct-addresses="
              << summary.keyboard_activity.distinct_addresses
              << " calendar-alarm-terminal=" << calendar_alarm_terminal
              << " port2-timer-output=" << port2_timer_output
              << " port2-timer-output-observed="
              << port2_timer_output_observed
              << " cpu-fault=" << cpu_fault_name(summary.cpu_fault)
              << " bus-fault=" << bus_fault_name(summary.bus_fault)
              << " state-fault=" << state_part_name(summary.state_fault);
    if (summary.fault_access.has_value()) {
        std::cout << " fault-access=" << access_name(*summary.fault_access);
    }
    if (summary.fault_region.has_value()) {
        std::cout << " fault-region=" << region_name(*summary.fault_region);
    }
    if (summary.last_interrupt_target_region.has_value()) {
        std::cout << " last-interrupt-target-region="
                  << region_name(*summary.last_interrupt_target_region);
    }
    if (summary.lcd_substituted_data_reads.has_value()) {
        std::cout << " lcd-substituted-reads="
                  << *summary.lcd_substituted_data_reads;
    }
    if (summary.lcd_panel.has_value()) {
        std::cout << " lcd-unknown-dots=" << summary.lcd_panel->unknown_dots
                  << " lcd-off-dots=" << summary.lcd_panel->off_dots
                  << " lcd-on-dots=" << summary.lcd_panel->on_dots;
    }
    std::cout << '\n';
}

}  // namespace

int run_jr800_command(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }
    const auto options = parse_options(argc, argv);
    if (!options.has_value()) {
        print_usage(std::cerr);
        return 2;
    }

    const auto rom = read_rom_container(options->rom_container);
    if (rom.status != RomContainerReadStatus::ok) {
        return rom.status == RomContainerReadStatus::io_error ? 2 : 1;
    }

    core::Jr800Machine machine{
        machine_configuration(*options),
        reset_state_configuration(*options),
    };
    if (!apply_inputs(machine, *options)) {
        std::cerr << "jr8run jr800: host input configuration failed\n";
        return 1;
    }
    if (machine.load_logical_rom(rom.bytes) != core::Jr800MemoryStatus::ok) {
        std::cerr << "jr8run jr800: logical ROM load failed\n";
        return 1;
    }
    const auto reset = machine.initialize_from_reset_entry();
    if (!reset.succeeded()) {
        std::cerr << "jr8run jr800: reset entry failed with "
                  << bus_fault_name(reset.fault) << '\n';
        return 1;
    }

    const auto summary = runtime::run_jr800_machine(machine, options->limits);
    print_summary(summary);
    return summary.completed_limit() ? 0 : 1;
}

}  // namespace jr800::tools
