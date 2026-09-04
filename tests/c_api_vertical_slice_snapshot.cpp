// SPDX-License-Identifier: MIT

#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "jr800/formats/jr8dbg.hpp"
#include "jr800/wasm/api.h"

namespace {

std::optional<std::vector<std::uint8_t>> read_hex(const char* path) {
    std::ifstream input(path);
    if (!input) {
        return std::nullopt;
    }
    const std::string text{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
    std::string digits;
    for (const unsigned char value : text) {
        if (std::isspace(value) != 0) {
            continue;
        }
        if (std::isxdigit(value) == 0) {
            return std::nullopt;
        }
        digits.push_back(static_cast<char>(value));
    }
    if (digits.empty() || digits.size() % 2U != 0U) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(digits.size() / 2U);
    for (std::size_t index = 0; index < digits.size(); index += 2U) {
        unsigned int value{};
        const auto* begin = digits.data() + index;
        const auto [end, error] = std::from_chars(begin, begin + 2U, value, 16);
        if (error != std::errc{} || end != begin + 2U) {
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>(value));
    }
    return bytes;
}

std::uint64_t combine(std::uint32_t low, std::uint32_t high) noexcept {
    return static_cast<std::uint64_t>(low)
        | (static_cast<std::uint64_t>(high) << 32U);
}

std::string_view profile_name(std::uint32_t profile) noexcept {
    switch (profile) {
    case JR800_PROFILE_MC6801:
        return "mc6801";
    case JR800_PROFILE_HD6301V1:
        return "hd6301v1";
    default:
        return "jr800_unresolved";
    }
}

std::string_view stop_name(std::uint32_t reason) noexcept {
    switch (reason) {
    case JR800_STOP_STEP_COMPLETE:
        return "step-complete";
    case JR800_STOP_INSTRUCTION_LIMIT:
        return "instruction-limit";
    case JR800_STOP_EXECUTION_BREAKPOINT:
        return "execution-breakpoint";
    case JR800_STOP_MEMORY_WATCHPOINT:
        return "memory-watchpoint";
    case JR800_STOP_CPU_FAULT:
        return "cpu-fault";
    case JR800_STOP_SLEEPING:
        return "sleeping";
    case JR800_STOP_ADDRESS_REACHED:
        return "address-reached";
    case JR800_STOP_STEP_OUT_COMPLETE:
        return "step-out-complete";
    default:
        return "detached";
    }
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

std::string_view fault_name(std::uint32_t fault) noexcept {
    switch (fault) {
    case JR800_FAULT_NONE:
        return "none";
    case JR800_FAULT_UNSUPPORTED_OPCODE:
        return "unsupported-opcode";
    case JR800_FAULT_UNIMPLEMENTED_OPERATION:
        return "unimplemented-operation";
    case JR800_FAULT_BUS_ACCESS:
        return "bus-access";
    case JR800_FAULT_UNKNOWN_STATE:
        return "unknown-state";
    case JR800_FAULT_UNKNOWN_INTERRUPT_REQUEST:
        return "unknown-interrupt-request";
    case JR800_FAULT_BUS_ADVANCE:
        return "bus-advance";
    default:
        return "unknown-fault";
    }
}

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: c_api_vertical_slice_snapshot <app.hex> <debug.hex>\n";
        return 2;
    }
    const auto application = read_hex(argv[1]);
    const auto debug_info = read_hex(argv[2]);
    if (!application.has_value() || !debug_info.has_value()) {
        std::cerr << "Fixture decode failed\n";
        return 2;
    }

    jr800_machine* machine = jr800_machine_create();
    if (machine == nullptr) {
        std::cerr << "Machine creation failed\n";
        return 1;
    }

    bool passed = true;
    passed &= check(
        jr800_machine_abi_version() == JR800_WASM_ABI_VERSION,
        "ABI version mismatch"
    );
    passed &= check(
        jr800_machine_load_application(
            machine,
            application->data(),
            static_cast<std::uint32_t>(application->size()),
            0x01FFU
        ) == JR800_STATUS_OK,
        "Application load failed"
    );
    constexpr char target_source_path[]{"main.s"};
    constexpr char target_symbol_name[]{"loop"};
    std::uint32_t source_address{0xA5A5A5A5U};
    std::uint32_t symbol_address{0xA5A5A5A5U};
    passed &= check(
        jr800_machine_source_address(
            machine,
            target_source_path,
            sizeof(target_source_path) - 1U,
            9U,
            &source_address
        ) == JR800_STATUS_NOT_FOUND
            && source_address == 0xA5A5A5A5U,
        "Source-address lookup guessed without debug information"
    );
    passed &= check(
        jr800_machine_symbol_address(
            machine,
            target_symbol_name,
            sizeof(target_symbol_name) - 1U,
            &symbol_address
        ) == JR800_STATUS_NOT_FOUND
            && symbol_address == 0xA5A5A5A5U,
        "Symbol-address lookup guessed without debug information"
    );
    passed &= check(
        jr800_machine_load_debug_info(
            machine,
            debug_info->data(),
            static_cast<std::uint32_t>(debug_info->size())
        ) == JR800_STATUS_OK,
        "Debug information load failed"
    );

    passed &= check(
        jr800_machine_source_address(
            nullptr,
            target_source_path,
            sizeof(target_source_path) - 1U,
            9U,
            &source_address
        ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_source_address(
                machine,
                nullptr,
                sizeof(target_source_path) - 1U,
                9U,
                &source_address
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_source_address(
                machine,
                target_source_path,
                0U,
                9U,
                &source_address
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_source_address(
                machine,
                target_source_path,
                sizeof(target_source_path) - 1U,
                0U,
                &source_address
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_source_address(
                machine,
                target_source_path,
                sizeof(target_source_path) - 1U,
                9U,
                nullptr
            ) == JR800_STATUS_INVALID_ARGUMENT
            && source_address == 0xA5A5A5A5U,
        "Invalid source-address lookup changed its output"
    );
    constexpr char missing_source_path[]{"missing.s"};
    constexpr char embedded_nul_path[]{'m', 'a', 'i', 'n', '\0', 's'};
    passed &= check(
        jr800_machine_source_address(
            machine,
            embedded_nul_path,
            sizeof(embedded_nul_path),
            9U,
            &source_address
        ) == JR800_STATUS_INVALID_ARGUMENT
            && source_address == 0xA5A5A5A5U
            && jr800_machine_source_address(
            machine,
            missing_source_path,
            sizeof(missing_source_path) - 1U,
            9U,
            &source_address
        ) == JR800_STATUS_NOT_FOUND
            && source_address == 0xA5A5A5A5U
            && jr800_machine_source_address(
                machine,
                target_source_path,
                sizeof(target_source_path) - 1U,
                9U,
                &source_address
            ) == JR800_STATUS_OK
            && source_address == 0x0207U,
        "Source-address reverse lookup failed or guessed a missing path"
    );
    constexpr char missing_symbol_name[]{"missing"};
    constexpr char embedded_nul_symbol[]{'l', 'o', '\0', 'o', 'p'};
    passed &= check(
        jr800_machine_symbol_address(
            nullptr,
            target_symbol_name,
            sizeof(target_symbol_name) - 1U,
            &symbol_address
        ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_symbol_address(
                machine,
                nullptr,
                sizeof(target_symbol_name) - 1U,
                &symbol_address
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_symbol_address(
                machine,
                target_symbol_name,
                0U,
                &symbol_address
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_symbol_address(
                machine,
                target_symbol_name,
                sizeof(target_symbol_name) - 1U,
                nullptr
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_symbol_address(
                machine,
                embedded_nul_symbol,
                sizeof(embedded_nul_symbol),
                &symbol_address
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_symbol_address(
                machine,
                missing_symbol_name,
                sizeof(missing_symbol_name) - 1U,
                &symbol_address
            ) == JR800_STATUS_NOT_FOUND
            && symbol_address == 0xA5A5A5A5U
            && jr800_machine_symbol_address(
                machine,
                target_symbol_name,
                sizeof(target_symbol_name) - 1U,
                &symbol_address
            ) == JR800_STATUS_OK
            && symbol_address == 0x020AU,
        "Symbol-address lookup failed, accepted invalid input, or guessed"
    );

    jr800_symbol_watch_result symbol_watch_result{
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
    };
    passed &= check(
        jr800_machine_set_symbol_watch(
            nullptr,
            41U,
            target_symbol_name,
            sizeof(target_symbol_name) - 1U
        ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_symbol_watch(
                machine,
                41U,
                nullptr,
                sizeof(target_symbol_name) - 1U
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_symbol_watch(
                machine,
                41U,
                target_symbol_name,
                0U
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_symbol_watch(
                machine,
                41U,
                embedded_nul_symbol,
                sizeof(embedded_nul_symbol)
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_evaluate_symbol_watch(
                nullptr,
                41U,
                &symbol_watch_result
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_evaluate_symbol_watch(machine, 41U, nullptr)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_clear_symbol_watch(nullptr, 41U)
                == JR800_STATUS_INVALID_ARGUMENT,
        "C ABI symbol-watch argument validation differs"
    );
    passed &= check(
        jr800_machine_set_symbol_watch(
            machine,
            41U,
            target_symbol_name,
            sizeof(target_symbol_name) - 1U
        ) == JR800_STATUS_OK
            && jr800_machine_evaluate_symbol_watch(
                machine,
                41U,
                &symbol_watch_result
            ) == JR800_STATUS_OK
            && symbol_watch_result.value == 0x020AU
            && symbol_watch_result.binding == JR800_SYMBOL_LOCAL
            && symbol_watch_result.kind == JR800_SYMBOL_ADDRESS
            && symbol_watch_result.size == 0U
            && symbol_watch_result.source_file_index_valid == 1U
            && symbol_watch_result.source_file_index == 0U,
        "C ABI symbol watch lost linked symbol metadata"
    );
    constexpr char loop_symbol_expression[]{"symbol(\"loop\")"};
    jr800_expression_watch_result symbol_expression_result{};
    passed &= check(
        jr800_machine_set_expression_watch(
            machine,
            44U,
            loop_symbol_expression,
            sizeof(loop_symbol_expression) - 1U
        ) == JR800_STATUS_OK
            && jr800_machine_evaluate_expression_watch(
                machine,
                44U,
                &symbol_expression_result
            ) == JR800_STATUS_OK
            && symbol_expression_result.error == JR800_EXPRESSION_OK
            && combine(
                symbol_expression_result.value_low,
                symbol_expression_result.value_high
            ) == 0x020AU,
        "C ABI expression did not resolve an exact JR8DBG symbol"
    );

    auto symbol_edge_debug_info = jr800::formats::jr8dbg::read(*debug_info);
    symbol_edge_debug_info.symbols.push_back(
        jr800::formats::jr8dbg::Symbol{
            "loop",
            jr800::formats::jr8dbg::SymbolBinding::local,
            jr800::formats::jr8dbg::SymbolKind::address,
            0x3000U,
            0U,
            0U,
        }
    );
    symbol_edge_debug_info.symbols.push_back(
        jr800::formats::jr8dbg::Symbol{
            "constant",
            jr800::formats::jr8dbg::SymbolBinding::local,
            jr800::formats::jr8dbg::SymbolKind::absolute,
            0x0042U,
            0U,
            {},
        }
    );
    const auto symbol_edge_bytes = jr800::formats::jr8dbg::write(
        symbol_edge_debug_info
    );
    passed &= check(
        jr800_machine_load_debug_info(
            machine,
            symbol_edge_bytes.data(),
            static_cast<std::uint32_t>(symbol_edge_bytes.size())
        ) == JR800_STATUS_OK,
        "Symbol edge-case debug information was rejected"
    );
    passed &= check(
        jr800_machine_evaluate_symbol_watch(
            machine,
            41U,
            &symbol_watch_result
        ) == JR800_STATUS_NOT_FOUND,
        "Successful JR8DBG replacement retained a stale C ABI symbol watch"
    );
    passed &= check(
        jr800_machine_evaluate_expression_watch(
            machine,
            44U,
            &symbol_expression_result
        ) == JR800_STATUS_OK
            && symbol_expression_result.error
                == JR800_EXPRESSION_AMBIGUOUS_SYMBOL,
        "C ABI expression guessed between duplicate JR8DBG symbols"
    );
    symbol_address = 0xA5A5A5A5U;
    passed &= check(
        jr800_machine_symbol_address(
            machine,
            target_symbol_name,
            sizeof(target_symbol_name) - 1U,
            &symbol_address
        ) == JR800_STATUS_AMBIGUOUS_SYMBOL
            && symbol_address == 0xA5A5A5A5U,
        "Ambiguous C ABI symbol lookup was guessed or changed its output"
    );
    constexpr char absolute_symbol_name[]{"constant"};
    symbol_address = 0xA5A5A5A5U;
    passed &= check(
        jr800_machine_symbol_address(
            machine,
            absolute_symbol_name,
            sizeof(absolute_symbol_name) - 1U,
            &symbol_address
        ) == JR800_STATUS_SYMBOL_NOT_ADDRESS
            && symbol_address == 0xA5A5A5A5U,
        "Absolute C ABI symbol lookup was accepted or changed its output"
    );
    passed &= check(
        jr800_machine_set_symbol_watch(
            machine,
            42U,
            absolute_symbol_name,
            sizeof(absolute_symbol_name) - 1U
        ) == JR800_STATUS_OK
            && jr800_machine_set_symbol_watch(
                machine,
                42U,
                target_symbol_name,
                sizeof(target_symbol_name) - 1U
            ) == JR800_STATUS_AMBIGUOUS_SYMBOL
            && jr800_machine_set_symbol_watch(
                machine,
                42U,
                missing_symbol_name,
                sizeof(missing_symbol_name) - 1U
            ) == JR800_STATUS_NOT_FOUND
            && jr800_machine_evaluate_symbol_watch(
                machine,
                42U,
                &symbol_watch_result
            ) == JR800_STATUS_OK
            && symbol_watch_result.value == 0x0042U
            && symbol_watch_result.binding == JR800_SYMBOL_LOCAL
            && symbol_watch_result.kind == JR800_SYMBOL_ABSOLUTE
            && symbol_watch_result.size == 0U
            && symbol_watch_result.source_file_index_valid == 0U
            && symbol_watch_result.source_file_index == 0U,
        "C ABI symbol watch guessed a name or rejected an absolute value"
    );
    constexpr char absolute_symbol_expression[]{"symbol(\"constant\") + 1"};
    passed &= check(
        jr800_machine_set_expression_watch(
            machine,
            45U,
            absolute_symbol_expression,
            sizeof(absolute_symbol_expression) - 1U
        ) == JR800_STATUS_OK
            && jr800_machine_evaluate_expression_watch(
                machine,
                45U,
                &symbol_expression_result
            ) == JR800_STATUS_OK
            && symbol_expression_result.error == JR800_EXPRESSION_OK
            && combine(
                symbol_expression_result.value_low,
                symbol_expression_result.value_high
            ) == 0x0043U,
        "C ABI expression rejected an absolute JR8DBG symbol"
    );
    passed &= check(
        jr800_machine_clear_symbol_watch(machine, 42U) == JR800_STATUS_OK
            && jr800_machine_clear_symbol_watch(machine, 42U)
                == JR800_STATUS_NOT_FOUND,
        "C ABI symbol-watch removal did not report missing state"
    );
    symbol_watch_result = {
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
    };
    passed &= check(
        jr800_machine_evaluate_symbol_watch(
            machine,
            42U,
            &symbol_watch_result
        ) == JR800_STATUS_NOT_FOUND
            && symbol_watch_result.value == 0xA5A5A5A5U
            && symbol_watch_result.binding == 0xA5A5A5A5U
            && symbol_watch_result.kind == 0xA5A5A5A5U
            && symbol_watch_result.size == 0xA5A5A5A5U
            && symbol_watch_result.source_file_index_valid == 0xA5A5A5A5U
            && symbol_watch_result.source_file_index == 0xA5A5A5A5U,
        "Missing C ABI symbol watch changed its output"
    );
    passed &= check(
        jr800_machine_load_debug_info(
            machine,
            debug_info->data(),
            static_cast<std::uint32_t>(debug_info->size())
        ) == JR800_STATUS_OK,
        "Original debug information could not be restored"
    );
    passed &= check(
        jr800_machine_set_symbol_watch(
            machine,
            43U,
            target_symbol_name,
            sizeof(target_symbol_name) - 1U
        ) == JR800_STATUS_OK,
        "Symbol watch could not be restored with the original JR8DBG"
    );
    passed &= check(
        jr800_machine_evaluate_expression_watch(
            machine,
            44U,
            &symbol_expression_result
        ) == JR800_STATUS_OK
            && symbol_expression_result.error == JR800_EXPRESSION_OK
            && combine(
                symbol_expression_result.value_low,
                symbol_expression_result.value_high
            ) == 0x020AU
            && jr800_machine_evaluate_expression_watch(
                machine,
                45U,
                &symbol_expression_result
            ) == JR800_STATUS_OK
            && symbol_expression_result.error
                == JR800_EXPRESSION_SYMBOL_NOT_FOUND,
        "C ABI symbol expressions did not track JR8DBG replacement"
    );

    jr800_stop_info run_to{};
    passed &= check(
        jr800_machine_run_to(nullptr, 0x0207U, 100U, &run_to)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_run_to(machine, 0x1'0000U, 100U, &run_to)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_run_to(machine, 0x0207U, 0U, &run_to)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_run_to(machine, 0x0207U, 100U, nullptr)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_run_to(machine, source_address, 100U, &run_to)
                == JR800_STATUS_OK
            && run_to.reason == JR800_STOP_ADDRESS_REACHED
            && run_to.trigger_address == 0x0207U
            && combine(
                run_to.instructions_executed_low,
                run_to.instructions_executed_high
            ) == 3U
            && run_to.pc_before == 0x0207U
            && run_to.pc_after == 0x0207U
            && run_to.trigger_access_valid == 0U,
        "Run-to-address stop failed"
    );
    passed &= check(
        jr800_machine_reset(machine) == JR800_STATUS_OK,
        "Reset after run-to-address failed"
    );

    auto mismatched_debug_info = *debug_info;
    const auto target_length =
        (static_cast<std::uint32_t>(mismatched_debug_info[16]) << 24U)
        | (static_cast<std::uint32_t>(mismatched_debug_info[17]) << 16U)
        | (static_cast<std::uint32_t>(mismatched_debug_info[18]) << 8U)
        | static_cast<std::uint32_t>(mismatched_debug_info[19]);
    const auto digest_offset = 20U + target_length;
    passed &= check(
        digest_offset < mismatched_debug_info.size(),
        "Debug fixture digest offset is invalid"
    );
    if (digest_offset < mismatched_debug_info.size()) {
        mismatched_debug_info[digest_offset] ^= 0xFFU;
        passed &= check(
            jr800_machine_load_debug_info(
                machine,
                mismatched_debug_info.data(),
                static_cast<std::uint32_t>(mismatched_debug_info.size())
            ) == JR800_STATUS_INTEGRITY_MISMATCH,
            "Mismatched debug information was accepted"
        );
    }
    const std::uint8_t invalid_debug_info[]{0x00U};
    passed &= check(
        jr800_machine_load_debug_info(machine, invalid_debug_info, 1U)
            == JR800_STATUS_INVALID_DEBUG_INFO,
        "Malformed debug information was accepted"
    );
    jr800_source_location retained_source{};
    passed &= check(
        jr800_machine_source_at(machine, 0x0200U, &retained_source)
            == JR800_STATUS_OK,
        "Rejected debug information replaced the last valid mapping"
    );
    passed &= check(
        jr800_machine_set_memory_watchpoint(
            machine,
            0x0001U,
            JR800_WATCHPOINT_WRITE,
            1U
        )
            == JR800_STATUS_OK,
        "Write-mode memory watchpoint setup failed"
    );

    jr800_stop_info stop{};
    passed &= check(
        jr800_machine_run(machine, 100U, &stop) == JR800_STATUS_OK,
        "Machine run failed"
    );
    jr800_machine_state state{};
    passed &= check(
        jr800_machine_get_state(machine, &state) == JR800_STATUS_OK,
        "State read failed"
    );

    std::uint8_t memory[2]{};
    passed &= check(
        jr800_machine_read_memory(machine, 0U, memory, 2U) == JR800_STATUS_OK,
        "Memory read failed"
    );

    const auto history_count = jr800_machine_history_count(machine);
    std::vector<jr800_history_entry> history(history_count);
    passed &= check(
        jr800_machine_copy_history(machine, history.data(), history_count)
            == history_count,
        "History copy failed"
    );
    const jr800_access_filter all_accesses{
        0U,
        0xFFFFU,
        JR800_ACCESS_TRACE_ALL,
    };
    const jr800_access_filter reversed_accesses{
        1U,
        0U,
        JR800_ACCESS_TRACE_ALL,
    };
    const jr800_access_filter empty_access_mask{0U, 0xFFFFU, 0U};
    const jr800_access_filter oversized_access_mask{0U, 0xFFFFU, 0x107U};
    const jr800_access_filter oversized_access_address{
        0U,
        0x1'0000U,
        JR800_ACCESS_TRACE_ALL,
    };
    std::uint32_t access_count{0xA5A5A5A5U};
    passed &= check(
        jr800_machine_access_count(nullptr, &all_accesses, &access_count)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_access_count(machine, nullptr, &access_count)
                == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_access_count(
                machine,
                &reversed_accesses,
                &access_count
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_access_count(
                machine,
                &empty_access_mask,
                &access_count
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_access_count(
                machine,
                &oversized_access_mask,
                &access_count
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_access_count(
                machine,
                &oversized_access_address,
                &access_count
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_access_count(machine, &all_accesses, nullptr)
                == JR800_STATUS_INVALID_ARGUMENT
            && access_count == 0xA5A5A5A5U,
        "Invalid access filter changed its output count"
    );
    passed &= check(
        jr800_machine_access_count(machine, &all_accesses, &access_count)
                == JR800_STATUS_OK
            && access_count == 12U,
        "Unfiltered access count failed"
    );
    std::vector<jr800_access_record> accesses(access_count);
    std::uint32_t copied_accesses{0xA5A5A5A5U};
    passed &= check(
        jr800_machine_copy_accesses(
            machine,
            &all_accesses,
            accesses.data(),
            access_count,
            &copied_accesses
        ) == JR800_STATUS_OK
            && copied_accesses == access_count,
        "Access copy failed"
    );
    passed &= check(
        history_count == 4U && history.back().pc_before == 0x0207U
            && history.back().access_count == 4U,
        "History content mismatch"
    );
    passed &= check(
        access_count == 12U
            && stop.reason == JR800_STOP_MEMORY_WATCHPOINT
            && stop.trigger_access_valid == 1U
            && stop.trigger_access == JR800_ACCESS_DATA_WRITE
            && accesses.back().kind == JR800_ACCESS_DATA_WRITE
            && accesses.back().address == 1U
            && accesses.back().value == 0x99U
            && accesses.back().value_known == 1U
            && accesses.back().previous_value == 0U
            && accesses.back().previous_value_known == 1U,
        "Access content mismatch"
    );
    jr800_access_record latest_access{};
    copied_accesses = 0xA5A5A5A5U;
    passed &= check(
        jr800_machine_copy_accesses(
            machine,
            &all_accesses,
            &latest_access,
            1U,
            &copied_accesses
        ) == JR800_STATUS_OK
            && copied_accesses == 1U
            && latest_access.sequence_low == accesses.back().sequence_low,
        "Bounded access copy did not retain the newest matching record"
    );
    jr800_access_record unchanged_access{};
    unchanged_access.address = 0xA5A5A5A5U;
    copied_accesses = 0xA5A5A5A5U;
    passed &= check(
        jr800_machine_copy_accesses(
            machine,
            &reversed_accesses,
            &unchanged_access,
            1U,
            &copied_accesses
        ) == JR800_STATUS_INVALID_ARGUMENT
            && copied_accesses == 0xA5A5A5A5U
            && unchanged_access.address == 0xA5A5A5A5U,
        "Invalid access copy changed caller output"
    );
    const jr800_access_filter result_write{
        1U,
        1U,
        JR800_ACCESS_TRACE_DATA_WRITE,
    };
    std::uint32_t filtered_count{};
    jr800_access_record filtered_access{};
    passed &= check(
        jr800_machine_access_count(machine, &result_write, &filtered_count)
                == JR800_STATUS_OK
            && filtered_count == 1U
            && jr800_machine_copy_accesses(
                machine,
                &result_write,
                &filtered_access,
                1U,
                &copied_accesses
            ) == JR800_STATUS_OK
            && copied_accesses == 1U
            && filtered_access.sequence_low == accesses.back().sequence_low
            && filtered_access.kind == JR800_ACCESS_DATA_WRITE
            && filtered_access.address == 1U
            && jr800_machine_access_count(machine, &all_accesses, &access_count)
                == JR800_STATUS_OK
            && access_count == 12U,
        "Access trace filter changed capture or selected the wrong record"
    );

    jr800_source_location source{};
    passed &= check(
        jr800_machine_source_at(machine, stop.pc_before, &source)
            == JR800_STATUS_OK,
        "Source lookup failed"
    );
    const auto path_size = jr800_machine_source_path_size(
        machine,
        source.source_file_index
    );
    std::vector<char> source_path(path_size);
    passed &= check(
        path_size != 0U
            && jr800_machine_copy_source_path(
                machine,
                source.source_file_index,
                source_path.data(),
                path_size
            ) == JR800_STATUS_OK,
        "Source path copy failed"
    );

    jr800_disassembly disassembly{};
    passed &= check(
        jr800_machine_disassemble(machine, stop.pc_before, &disassembly)
            == JR800_STATUS_OK,
        "Disassembly failed"
    );
    const auto disassembly_size = jr800_machine_disassembly_text_size(
        machine,
        stop.pc_before
    );
    std::vector<char> disassembly_text(disassembly_size);
    passed &= check(
        disassembly_size != 0U
            && jr800_machine_copy_disassembly_text(
                machine,
                stop.pc_before,
                disassembly_text.data(),
                disassembly_size
            ) == JR800_STATUS_OK,
        "Disassembly text copy failed"
    );

    if (!passed) {
        jr800_machine_destroy(machine);
        return 1;
    }

    std::cout
        << "{\n"
        << "  \"stop\": {\n"
        << "    \"reason\": \"" << stop_name(stop.reason) << "\",\n"
        << "    \"triggerAddress\": " << stop.trigger_address << ",\n"
        << "    \"triggerAccess\": \"data-write\",\n"
        << "    \"instructionsExecuted\": "
        << combine(stop.instructions_executed_low, stop.instructions_executed_high)
        << ",\n"
        << "    \"pcBefore\": " << stop.pc_before << ",\n"
        << "    \"pcAfter\": " << stop.pc_after << ",\n"
        << "    \"fault\": \"" << fault_name(stop.fault) << "\"\n"
        << "  },\n"
        << "  \"state\": {\n"
        << "    \"profile\": \"" << profile_name(state.profile) << "\",\n"
        << "    \"pc\": " << state.pc << ",\n"
        << "    \"sp\": " << state.sp << ",\n"
        << "    \"x\": " << state.x << ",\n"
        << "    \"a\": " << state.a << ",\n"
        << "    \"b\": " << state.b << ",\n"
        << "    \"conditionCode\": " << state.condition_code << ",\n"
        << "    \"executionState\": \""
        << execution_state_name(state.execution_state) << "\",\n"
        << "    \"cycleCount\": "
        << combine(state.cycle_count_low, state.cycle_count_high) << "\n"
        << "  },\n"
        << "  \"memory\": [" << static_cast<unsigned int>(memory[0]) << ", "
        << static_cast<unsigned int>(memory[1]) << "],\n"
        << "  \"historyCount\": " << history_count << ",\n"
        << "  \"accessCount\": " << access_count << ",\n"
        << "  \"source\": {\n"
        << "    \"path\": \"" << source_path.data() << "\",\n"
        << "    \"line\": " << source.line << ",\n"
        << "    \"column\": " << source.column << "\n"
        << "  },\n"
        << "  \"disassembly\": \"" << disassembly_text.data() << "\"\n"
        << "}\n";

    passed &= check(
        jr800_machine_reset(machine) == JR800_STATUS_OK
            && jr800_machine_history_count(machine) == 0U,
        "Reset failed"
    );
    passed &= check(
        jr800_machine_set_execution_breakpoint(machine, 0x0200U, 1U)
            == JR800_STATUS_OK,
        "Execution breakpoint setup failed"
    );
    jr800_stop_info breakpoint{};
    passed &= check(
        jr800_machine_run(machine, 10U, &breakpoint) == JR800_STATUS_OK
            && breakpoint.reason == JR800_STOP_EXECUTION_BREAKPOINT
            && combine(
                breakpoint.instructions_executed_low,
                breakpoint.instructions_executed_high
            ) == 0U,
        "Execution breakpoint stop failed"
    );

    constexpr char conditional_expression[] =
        "pc == $0200 && A == 0 && cycles == 0 "
        "&& symbol(\"loop\") == $020A";
    constexpr char malformed_expression[] = "A ==";
    constexpr std::array<char, 5U> embedded_nul_expression{
        '1', '\0', '|', '|', '0',
    };
    passed &= check(
        jr800_machine_set_execution_breakpoint(machine, 0x0200U, 0U)
                == JR800_STATUS_OK
            && jr800_machine_set_conditional_execution_breakpoint(
                machine,
                0x0200U,
                conditional_expression,
                static_cast<std::uint32_t>(
                    sizeof(conditional_expression) - 1U
                )
            ) == JR800_STATUS_OK
            && jr800_machine_set_conditional_execution_breakpoint(
                machine,
                0x0200U,
                malformed_expression,
                static_cast<std::uint32_t>(sizeof(malformed_expression) - 1U)
            ) == JR800_STATUS_INVALID_EXPRESSION
            && jr800_machine_set_conditional_execution_breakpoint(
                machine,
                0x0200U,
                embedded_nul_expression.data(),
                static_cast<std::uint32_t>(embedded_nul_expression.size())
            ) == JR800_STATUS_INVALID_EXPRESSION
            && jr800_machine_reset(machine) == JR800_STATUS_OK
            && jr800_machine_run(machine, 10U, &breakpoint)
                == JR800_STATUS_OK
            && breakpoint.reason == JR800_STOP_EXECUTION_BREAKPOINT
            && breakpoint.condition_error == JR800_EXPRESSION_OK
            && combine(
                breakpoint.instructions_executed_low,
                breakpoint.instructions_executed_high
            ) == 0U,
        "C ABI conditional breakpoint setup was not transactional"
    );

    constexpr char watch_expression[] = "PC";
    constexpr char failing_watch_expression[] = "mem8[$10000]";
    jr800_expression_watch_result watch_result{
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
    };
    passed &= check(
        jr800_machine_set_expression_watch(
            nullptr,
            42U,
            watch_expression,
            sizeof(watch_expression) - 1U
        ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_expression_watch(
                machine,
                42U,
                nullptr,
                sizeof(watch_expression) - 1U
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_set_expression_watch(
                machine,
                42U,
                watch_expression,
                0U
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_evaluate_expression_watch(
                nullptr,
                42U,
                &watch_result
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_evaluate_expression_watch(
                machine,
                42U,
                nullptr
            ) == JR800_STATUS_INVALID_ARGUMENT
            && jr800_machine_clear_expression_watch(nullptr, 42U)
                == JR800_STATUS_INVALID_ARGUMENT,
        "C ABI expression-watch argument validation differs"
    );
    passed &= check(
        jr800_machine_set_expression_watch(
            machine,
            42U,
            watch_expression,
            sizeof(watch_expression) - 1U
        ) == JR800_STATUS_OK
            && jr800_machine_evaluate_expression_watch(
                machine,
                42U,
                &watch_result
            ) == JR800_STATUS_OK
            && combine(watch_result.value_low, watch_result.value_high)
                == 0x0200U
            && watch_result.error == JR800_EXPRESSION_OK
            && watch_result.bus_fault == JR800_BUS_FAULT_NONE
            && watch_result.fault_address == 0U
            && watch_result.state_fault == JR800_STATE_PART_NONE
            && jr800_machine_set_expression_watch(
                machine,
                42U,
                malformed_expression,
                sizeof(malformed_expression) - 1U
            ) == JR800_STATUS_INVALID_EXPRESSION
            && jr800_machine_evaluate_expression_watch(
                machine,
                42U,
                &watch_result
            ) == JR800_STATUS_OK
            && combine(watch_result.value_low, watch_result.value_high)
                == 0x0200U,
        "C ABI expression-watch replacement was not transactional"
    );
    passed &= check(
        jr800_machine_set_expression_watch(
            machine,
            43U,
            failing_watch_expression,
            sizeof(failing_watch_expression) - 1U
        ) == JR800_STATUS_OK
            && jr800_machine_evaluate_expression_watch(
                machine,
                43U,
                &watch_result
            ) == JR800_STATUS_OK
            && watch_result.error
                == JR800_EXPRESSION_ADDRESS_OUT_OF_RANGE
            && jr800_machine_history_count(machine) == 0U,
        "C ABI expression-watch evaluation error was not structured"
    );
    passed &= check(
        jr800_machine_clear_expression_watch(machine, 42U)
                == JR800_STATUS_OK
            && jr800_machine_clear_expression_watch(machine, 42U)
                == JR800_STATUS_NOT_FOUND,
        "C ABI expression-watch removal did not report missing state"
    );
    watch_result = {
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
        0xA5A5A5A5U,
    };
    passed &= check(
        jr800_machine_evaluate_expression_watch(
            machine,
            42U,
            &watch_result
        ) == JR800_STATUS_NOT_FOUND
            && watch_result.value_low == 0xA5A5A5A5U
            && watch_result.value_high == 0xA5A5A5A5U
            && watch_result.error == 0xA5A5A5A5U
            && watch_result.bus_fault == 0xA5A5A5A5U
            && watch_result.fault_address == 0xA5A5A5A5U
            && watch_result.state_fault == 0xA5A5A5A5U,
        "Missing C ABI expression watch changed its output"
    );
    passed &= check(
        jr800_machine_load_application(
            machine,
            application->data(),
            static_cast<std::uint32_t>(application->size()),
            0x01FFU
        ) == JR800_STATUS_OK
            && jr800_machine_evaluate_expression_watch(
                machine,
                43U,
                &watch_result
            ) == JR800_STATUS_NOT_FOUND
            && jr800_machine_evaluate_symbol_watch(
                machine,
                43U,
                &symbol_watch_result
            ) == JR800_STATUS_NOT_FOUND,
        "Application load retained an obsolete debugger watch"
    );

    jr800_machine_destroy(machine);
    return passed ? 0 : 1;
}
