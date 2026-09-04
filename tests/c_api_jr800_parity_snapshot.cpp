// SPDX-License-Identifier: MIT

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "jr800/formats/jr8rom.hpp"
#include "jr800/wasm/api.h"

namespace {

struct Snapshot {
    jr800_machine_state state{};
    std::array<std::uint8_t, 2> memory{};
    jr800_keyboard_activity keyboard_activity{};
    jr800_disassembly disassembly{};
    std::string disassembly_text;
    std::uint32_t history_count{};
    std::uint32_t access_count{};
};

std::uint64_t combine(std::uint32_t low, std::uint32_t high) noexcept {
    return static_cast<std::uint64_t>(low)
        | (static_cast<std::uint64_t>(high) << 32U);
}

std::string_view profile_name(std::uint32_t profile) noexcept {
    return profile == JR800_PROFILE_HD6301V1 ? "hd6301v1" : "unknown";
}

std::string_view execution_state_name(std::uint32_t state) noexcept {
    switch (state) {
    case JR800_CPU_ACTIVE:
        return "active";
    case JR800_CPU_SLEEPING:
        return "sleeping";
    case JR800_CPU_WAITING_FOR_INTERRUPT:
        return "waiting-for-interrupt";
    default:
        return "unknown";
    }
}

std::string_view calendar_alarm_terminal_name(std::uint32_t state) noexcept {
    switch (state) {
    case JR800_CALENDAR_ALARM_TERMINAL_DISCONNECTED:
        return "disconnected";
    case JR800_CALENDAR_ALARM_TERMINAL_UNKNOWN:
        return "unknown";
    case JR800_CALENDAR_ALARM_TERMINAL_RELEASED:
        return "released";
    case JR800_CALENDAR_ALARM_TERMINAL_PULL_LOW:
        return "pull-low";
    default:
        return "invalid";
    }
}

std::string_view port2_timer_output_name(std::uint32_t state) noexcept {
    switch (state) {
    case JR800_PORT2_TIMER_OUTPUT_UNAVAILABLE:
        return "unavailable";
    case JR800_PORT2_TIMER_OUTPUT_DISABLED:
        return "disabled";
    case JR800_PORT2_TIMER_OUTPUT_UNKNOWN:
        return "unknown";
    case JR800_PORT2_TIMER_OUTPUT_LOW:
        return "low";
    case JR800_PORT2_TIMER_OUTPUT_HIGH:
        return "high";
    default:
        return "invalid";
    }
}

std::string_view stop_name(std::uint32_t reason) noexcept {
    return reason == JR800_STOP_STEP_COMPLETE ? "step-complete" : "unknown";
}

std::string_view fault_name(std::uint32_t fault) noexcept {
    return fault == JR800_FAULT_NONE ? "none" : "unknown";
}

std::string_view bus_fault_name(std::uint32_t fault) noexcept {
    return fault == JR800_BUS_FAULT_NONE ? "none" : "unknown";
}

std::string_view step_kind_name(std::uint32_t kind) noexcept {
    return kind == JR800_STEP_INSTRUCTION ? "instruction" : "unknown";
}

std::string_view interrupt_name(std::uint32_t source) noexcept {
    return source == JR800_INTERRUPT_NONE ? "none" : "unknown";
}

bool check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

std::vector<std::uint8_t> make_container() {
    std::vector<std::uint8_t> rom(32U * 1024U, 0x01U);
    rom[rom.size() - 2U] = 0x80U;
    rom[rom.size() - 1U] = 0x00U;
    const auto middle = rom.begin() + 16U * 1024U;

    jr800::formats::jr8rom::Image image;
    image.segments = {
        {0xC000U, {middle, rom.end()}},
        {0x8000U, {rom.begin(), middle}},
    };
    image.integrity_sha256 = jr800::formats::jr8rom::compute_integrity(image);
    return jr800::formats::jr8rom::write(image);
}

bool capture(jr800_machine* machine, Snapshot& snapshot) {
    if (jr800_machine_get_state(machine, &snapshot.state) != JR800_STATUS_OK
        || jr800_machine_read_memory(
            machine,
            0x8000U,
            snapshot.memory.data(),
            snapshot.memory.size()
        ) != JR800_STATUS_OK
        || jr800_machine_disassemble(
            machine,
            snapshot.state.pc,
            &snapshot.disassembly
        ) != JR800_STATUS_OK
        || jr800_machine_get_keyboard_activity(
            machine,
            &snapshot.keyboard_activity
        ) != JR800_STATUS_OK) {
        return false;
    }

    const auto text_size = jr800_machine_disassembly_text_size(
        machine,
        snapshot.state.pc
    );
    std::vector<char> text(text_size);
    if (text_size == 0U
        || jr800_machine_copy_disassembly_text(
            machine,
            snapshot.state.pc,
            text.data(),
            text_size
        ) != JR800_STATUS_OK) {
        return false;
    }
    snapshot.disassembly_text.assign(text.data());
    snapshot.history_count = jr800_machine_history_count(machine);

    const jr800_access_filter all_accesses{
        0U,
        0xFFFFU,
        JR800_ACCESS_TRACE_ALL,
    };
    return jr800_machine_access_count(
        machine,
        &all_accesses,
        &snapshot.access_count
    ) == JR800_STATUS_OK;
}

void print_state(const jr800_machine_state& state, std::string_view indent) {
    std::cout << indent << "{\n"
              << indent << "  \"abiVersion\": " << state.abi_version << ",\n"
              << indent << "  \"profile\": \"" << profile_name(state.profile)
              << "\",\n"
              << indent << "  \"pc\": " << state.pc << ",\n"
              << indent << "  \"sp\": " << state.sp << ",\n"
              << indent << "  \"x\": " << state.x << ",\n"
              << indent << "  \"a\": " << state.a << ",\n"
              << indent << "  \"b\": " << state.b << ",\n"
              << indent << "  \"conditionCode\": " << state.condition_code
              << ",\n"
              << indent << "  \"executionState\": \""
              << execution_state_name(state.execution_state) << "\",\n"
              << indent << "  \"cycleCount\": "
              << combine(state.cycle_count_low, state.cycle_count_high) << ",\n"
              << indent << "  \"registerKnownMask\": "
              << state.register_known_mask << ",\n"
              << indent << "  \"conditionCodeKnownMask\": "
              << state.condition_code_known_mask << ",\n"
              << indent << "  \"calendarAlarmTerminal\": \""
              << calendar_alarm_terminal_name(
                    state.calendar_alarm_terminal
                ) << "\",\n"
              << indent << "  \"port2TimerOutput\": \""
              << port2_timer_output_name(state.port2_timer_output) << "\",\n"
              << indent << "  \"lcdSubstitutedDataReadCount\": ";
    if (state.lcd_substituted_data_read_count_valid != 0U) {
        std::cout << combine(
            state.lcd_substituted_data_read_count_low,
            state.lcd_substituted_data_read_count_high
        );
    } else {
        std::cout << "null";
    }
    std::cout << '\n' << indent << '}';
}

void print_snapshot(const Snapshot& snapshot, std::string_view indent) {
    std::cout << indent << "{\n" << indent << "  \"state\": ";
    print_state(snapshot.state, std::string{indent} + "  ");
    std::cout << ",\n"
              << indent << "  \"memory\": ["
              << static_cast<unsigned int>(snapshot.memory[0]) << ", "
              << static_cast<unsigned int>(snapshot.memory[1]) << "],\n"
              << indent << "  \"historyCount\": " << snapshot.history_count
              << ",\n"
              << indent << "  \"accessCount\": " << snapshot.access_count
              << ",\n"
              << indent << "  \"keyboardActivity\": {\n"
              << indent << "    \"readAttempts\": "
              << combine(
                    snapshot.keyboard_activity.read_attempts_low,
                    snapshot.keyboard_activity.read_attempts_high
                ) << ",\n"
              << indent << "    \"distinctAddresses\": "
              << combine(
                    snapshot.keyboard_activity.distinct_addresses_low,
                    snapshot.keyboard_activity.distinct_addresses_high
                ) << "\n"
              << indent << "  },\n"
              << indent << "  \"disassembly\": {\n"
              << indent << "    \"address\": " << snapshot.disassembly.address
              << ",\n"
              << indent << "    \"bytes\": [" << snapshot.disassembly.byte0
              << ", " << snapshot.disassembly.byte1 << ", "
              << snapshot.disassembly.byte2 << "],\n"
              << indent << "    \"length\": " << snapshot.disassembly.length
              << ",\n"
              << indent << "    \"supported\": " << std::boolalpha
              << (snapshot.disassembly.supported != 0U) << ",\n"
              << indent << "    \"text\": "
              << std::quoted(snapshot.disassembly_text) << '\n'
              << indent << "  }\n"
              << indent << '}';
}

void print_stop(const jr800_stop_info& stop, std::string_view indent) {
    std::cout << indent << "{\n"
              << indent << "  \"reason\": \"" << stop_name(stop.reason)
              << "\",\n"
              << indent << "  \"fault\": \"" << fault_name(stop.fault)
              << "\",\n"
              << indent << "  \"instructionsExecuted\": "
              << combine(
                    stop.instructions_executed_low,
                    stop.instructions_executed_high
                ) << ",\n"
              << indent << "  \"pcBefore\": " << stop.pc_before << ",\n"
              << indent << "  \"pcAfter\": " << stop.pc_after << ",\n"
              << indent << "  \"bytes\": [" << stop.byte0 << ", "
              << stop.byte1 << ", " << stop.byte2 << "],\n"
              << indent << "  \"instructionLength\": "
              << stop.instruction_length << ",\n"
              << indent << "  \"bytesFetched\": " << stop.bytes_fetched
              << ",\n"
              << indent << "  \"cycles\": " << stop.cycles << ",\n"
              << indent << "  \"busFault\": \""
              << bus_fault_name(stop.bus_fault) << "\",\n"
              << indent << "  \"stepKind\": \""
              << step_kind_name(stop.step_kind) << "\",\n"
              << indent << "  \"interruptSource\": \""
              << interrupt_name(stop.interrupt_source) << "\"\n"
              << indent << '}';
}

}  // namespace

int main() {
    jr800_hardware_configuration configuration{};
    configuration.abi_version = JR800_WASM_ABI_VERSION;
    configuration.reset_stack_pointer_enabled = 1U;
    configuration.reset_stack_pointer_value = 0x2345U;
    configuration.reset_index_register_enabled = 1U;
    configuration.reset_index_register_value = 0x3456U;
    configuration.reset_accumulator_a_enabled = 1U;
    configuration.reset_accumulator_a_value = 0x67U;
    configuration.reset_accumulator_b_enabled = 1U;
    configuration.reset_accumulator_b_value = 0x89U;
    configuration.reset_condition_code_known_mask = 0x2FU;
    configuration.reset_condition_code_value = 0x25U;
    configuration.calendar_enabled = 1U;
    configuration.calendar_address_source = JR800_CALENDAR_CPU_A1_TO_A4;
    configuration.calendar_upper_read_bits = JR800_CALENDAR_UPPER_ONE;
    configuration.calendar_cpu_cycle_ratio =
        JR800_CALENDAR_CPU_CYCLE_RATIO_E030_NOMINAL_1_2288_MHZ;
    configuration.lcd_enabled = 1U;
    configuration.lcd_unknown_data_read_value = 0x3CU;
    auto* machine = jr800_machine_create_jr800(&configuration);
    if (!check(machine != nullptr, "JR-800 C ABI creation failed")) {
        return 1;
    }

    const auto container = make_container();
    Snapshot initial;
    jr800_stop_info stop{};
    Snapshot stepped;
    const auto passed = check(
        jr800_machine_load_jr8rom(
            machine,
            container.data(),
            static_cast<std::uint32_t>(container.size())
        ) == JR800_STATUS_OK,
        "JR8ROM C ABI load failed"
    ) && check(capture(machine, initial), "Initial C ABI snapshot failed")
        && check(
            jr800_machine_step(machine, &stop) == JR800_STATUS_OK,
            "C ABI step failed"
        )
        && check(capture(machine, stepped), "Stepped C ABI snapshot failed");
    if (!passed) {
        jr800_machine_destroy(machine);
        return 1;
    }

    std::cout << "{\n  \"initial\": ";
    print_snapshot(initial, "  ");
    std::cout << ",\n  \"step\": {\n    \"stop\": ";
    print_stop(stop, "    ");
    std::cout << ",\n    \"snapshot\": ";
    print_snapshot(stepped, "    ");
    std::cout << "\n  }\n}\n";

    jr800_machine_destroy(machine);
    return std::cout ? 0 : 1;
}
