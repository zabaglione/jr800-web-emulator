// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "jr800/assembler/assembler.hpp"
#include "jr800/core/bus.hpp"
#include "jr800/core/synthetic_machine.hpp"
#include "jr800/debugger/debugger.hpp"
#include "jr800/linker/linker.hpp"
#include "jr800/runtime/application_loader.hpp"

namespace {

constexpr std::string_view kSource =
    ".section .text, code\n"
    ".global entry\n"
    ".global result\n"
    ".local loop\n"
    "entry:\n"
    "    LDAA #$42\n"
    "    STAA result\n"
    "    LDAA #$99\n"
    "    STAA result + 1\n"
    "    LDAA result\n"
    "loop:\n"
    "    BRA loop\n"
    ".section .bss, bss\n"
    "result:\n"
    "    .space 2\n";

constexpr std::string_view kScript =
    "target hd6301v1\n"
    "entry entry\n"
    "region ZP $0000 $0100\n"
    "region CODE $0200 $0100\n"
    "place .text CODE\n"
    "place .bss ZP\n";

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    using jr800::debugger::DebugInfoLoadResult;
    using jr800::debugger::MemoryWatchpointMode;
    using jr800::debugger::StopReason;
    using jr800::runtime::LoadApplicationResult;

    bool passed = true;
    const auto assembled = jr800::assembler::assemble(
        jr800::assembler::Source{"sample/main.s", std::string{kSource}},
        jr800::assembler::Options{"hd6301v1", "test-version"}
    );
    passed &= expect(assembled.succeeded(), "Vertical-slice source did not assemble");
    const auto script = jr800::linker::parse_script(
        jr800::linker::ScriptSource{"sample/memory.j8l", std::string{kScript}}
    );
    passed &= expect(script.succeeded(), "Vertical-slice link script was rejected");
    if (!assembled.succeeded() || !script.succeeded()) {
        return 1;
    }

    const auto linked = jr800::linker::link_objects(
        {jr800::linker::InputObject{"sample/main.jro", assembled.output->object}},
        *script.script,
        jr800::linker::Options{"test-version"}
    );
    passed &= expect(linked.succeeded(), "Vertical-slice object did not link");
    if (!linked.succeeded()) {
        return 1;
    }

    jr800::core::SyntheticMachine machine;
    passed &= expect(
        jr800::runtime::load_application(
            machine,
            linked.output->application,
            0x01FF
        ) == LoadApplicationResult::loaded,
        "JR8APP did not load into the synthetic machine"
    );

    jr800::debugger::Debugger debugger{16, 64};
    passed &= expect(
        debugger.attach(machine.execution()),
        "Vertical-slice debugger did not attach"
    );
    passed &= expect(
        debugger.load_debug_info(
            linked.output->debug_info,
            linked.output->application.integrity_sha256
        ) == DebugInfoLoadResult::loaded,
        "JR8DBG did not bind to the loaded application"
    );
    passed &= expect(
        debugger.source_address("sample/main.s", 9U) == 0x0207U
            && !debugger.source_address("sample/missing.s", 9U).has_value()
            && !debugger.source_address("sample/main.s", 0U).has_value(),
        "Debugger source-line reverse lookup differs"
    );
    const auto entry_symbol = debugger.symbol_address("entry");
    const auto result_symbol = debugger.symbol_address("result");
    passed &= expect(
        entry_symbol.succeeded() && entry_symbol.address == 0x0200U
            && result_symbol.succeeded() && result_symbol.address == 0x0000U
            && debugger.symbol_address("ENTRY").status
                == jr800::debugger::SymbolAddressStatus::not_found
            && debugger.symbol_address("missing").status
                == jr800::debugger::SymbolAddressStatus::not_found,
        "Debugger exact symbol-address lookup differs"
    );
    const auto result_watch_registration = debugger.set_symbol_watch(
        5U,
        "result"
    );
    const auto result_watch = debugger.evaluate_symbol_watch(5U);
    passed &= expect(
        result_watch_registration.succeeded() && result_watch.has_value()
            && result_watch->value == 0x0000U
            && result_watch->binding
                == jr800::formats::jr8dbg::SymbolBinding::global
            && result_watch->kind
                == jr800::formats::jr8dbg::SymbolKind::address
            && result_watch->size == 0U
            && result_watch->source_file_index == 0U,
        "Debugger symbol watch lost linked symbol metadata"
    );
    passed &= expect(
        debugger.set_expression_watch(6U, "symbol(\"loop\")").succeeded(),
        "Debug-symbol expression watch was not compiled"
    );
    const auto linked_symbol_expression = debugger.evaluate_expression_watch(6U);
    passed &= expect(
        linked_symbol_expression.has_value()
            && linked_symbol_expression->succeeded()
            && linked_symbol_expression->value == 0x020DU,
        "Debug-symbol expression watch did not resolve the active JR8DBG"
    );
    debugger.set_execution_breakpoint(0x0205U, true);
    const auto interrupted_run_to = debugger.run_to(0x0207U, 100U);
    passed &= expect(
        interrupted_run_to.reason == StopReason::execution_breakpoint
            && interrupted_run_to.trigger_address == 0x0205U
            && interrupted_run_to.instructions_executed == 2U,
        "Run-to-address ignored an earlier execution breakpoint"
    );
    debugger.set_execution_breakpoint(0x0205U, false);
    debugger.set_execution_breakpoint(0x0207U, true);
    const auto boundary_run_to = debugger.run_to(0x0207U, 1U);
    passed &= expect(
        boundary_run_to.reason == StopReason::address_reached
            && boundary_run_to.trigger_address == 0x0207U
            && boundary_run_to.instructions_executed == 1U,
        "Run-to-address lost target priority at the instruction limit"
    );
    debugger.set_execution_breakpoint(0x0207U, false);
    passed &= expect(
        jr800::runtime::load_application(
            machine,
            linked.output->application,
            0x01FF
        ) == LoadApplicationResult::loaded,
        "Vertical-slice machine did not reset after run-to-address"
    );
    debugger.clear_history();
    debugger.set_memory_watchpoint(0x0001, MemoryWatchpointMode::write, true);
    const auto stop = debugger.run(100);
    passed &= expect(
        stop.reason == StopReason::memory_watchpoint
            && stop.trigger_address == 0x0001U
            && stop.trigger_access == jr800::core::AccessKind::data_write
            && stop.instructions_executed == 4U,
        "Write-mode memory watchpoint stop mismatch"
    );
    passed &= expect(
        machine.bus().peek8(0x0000) == 0x42U
            && machine.bus().peek8(0x0001) == 0x99U,
        "Vertical-slice RAM result mismatch"
    );
    passed &= expect(
        machine.execution().cpu().state().pc == 0x020AU
            && machine.execution().cpu().state().cycle_count == 12U,
        "Vertical-slice CPU state mismatch"
    );

    const auto history = debugger.history();
    passed &= expect(history.size() == 4U, "Vertical-slice history count mismatch");
    if (history.size() == 4U) {
        passed &= expect(
            history.back().pc_before == 0x0207U
                && history.back().pc_after == 0x020AU
                && history.back().bytes[0] == 0xB7U
                && history.back().access_count == 4U,
            "Watchpoint history entry mismatch"
        );
    }

    const auto accesses = debugger.memory_accesses();
    passed &= expect(accesses.size() == 12U, "Vertical-slice access count mismatch");
    if (!accesses.empty()) {
        const auto& write = accesses.back();
        passed &= expect(
            write.kind == jr800::core::AccessKind::data_write
                && write.instruction_pc == 0x0207U
                && write.address == 0x0001U
                && write.value == 0x99U
                && write.previous_value == 0U,
            "Watchpoint access record mismatch"
        );
    }
    const jr800::debugger::AccessTraceFilter write_filter{
        0x0001U,
        0x0001U,
        jr800::debugger::AccessTraceMask::data_write,
    };
    const auto filtered_accesses = debugger.memory_accesses(write_filter);
    passed &= expect(
        filtered_accesses.size() == 1U
            && debugger.memory_access_size(write_filter) == 1U
            && filtered_accesses.front() == accesses.back()
            && debugger.memory_access_size() == accesses.size(),
        "Access trace filter changed capture or selected the wrong record"
    );

    const auto* source = debugger.source_at(stop.step.pc_before);
    passed &= expect(
        source != nullptr && source->source_file_index == 0U && source->line == 9U,
        "Watchpoint source lookup mismatch"
    );
    const auto result_symbols = debugger.symbols_at(0x0000);
    passed &= expect(
        std::any_of(result_symbols.begin(), result_symbols.end(), [](const auto* symbol) {
            return symbol->name == "result";
        }),
        "Result symbol lookup mismatch"
    );
    const auto disassembly = debugger.disassemble(stop.step.pc_before);
    passed &= expect(
        disassembly.has_value() && disassembly->text == "STAA $0001",
        "Stopped instruction disassembly mismatch"
    );

    debugger.set_memory_watchpoint(0x0001, MemoryWatchpointMode::write, false);
    passed &= expect(
        jr800::runtime::load_application(
            machine,
            linked.output->application,
            0x01FF
        ) == LoadApplicationResult::loaded,
        "Vertical-slice machine did not reload"
    );
    debugger.clear_history();
    debugger.set_memory_watchpoint(0x0000, MemoryWatchpointMode::read, true);
    const auto read_stop = debugger.run(100);
    passed &= expect(
        read_stop.reason == StopReason::memory_watchpoint
            && read_stop.trigger_address == 0x0000U
            && read_stop.trigger_access == jr800::core::AccessKind::data_read
            && read_stop.instructions_executed == 5U,
        "Read-mode memory watchpoint stop mismatch"
    );
    debugger.set_memory_watchpoint(0x0000, MemoryWatchpointMode::read, false);
    debugger.set_execution_breakpoint(read_stop.step.pc_after, true);
    const auto breakpoint = debugger.run(10);
    passed &= expect(
        breakpoint.reason == StopReason::execution_breakpoint
            && breakpoint.instructions_executed == 0U,
        "Loop execution breakpoint mismatch"
    );

    debugger.set_memory_watchpoint(0x1234, MemoryWatchpointMode::access, true);
    passed &= expect(
        debugger.has_memory_watchpoint(0x1234, MemoryWatchpointMode::read)
            && debugger.has_memory_watchpoint(0x1234, MemoryWatchpointMode::write)
            && debugger.has_memory_watchpoint(0x1234, MemoryWatchpointMode::access),
        "Access-mode memory watchpoint did not enable both modes"
    );
    debugger.set_memory_watchpoint(0x1234, MemoryWatchpointMode::read, false);
    passed &= expect(
        !debugger.has_memory_watchpoint(0x1234, MemoryWatchpointMode::read)
            && debugger.has_memory_watchpoint(0x1234, MemoryWatchpointMode::write)
            && !debugger.has_memory_watchpoint(
                0x1234,
                MemoryWatchpointMode::access
            ),
        "Memory watchpoint mode removal changed the wrong bit"
    );
    debugger.clear_memory_watchpoints();
    passed &= expect(
        !debugger.has_memory_watchpoint(0x1234, MemoryWatchpointMode::write),
        "Memory watchpoint clear failed"
    );

    debugger.clear_execution_breakpoints();
    passed &= expect(
        jr800::runtime::load_application(
            machine,
            linked.output->application,
            0x01FF
        ) == LoadApplicationResult::loaded,
        "Vertical-slice machine did not reload for conditional breakpoint"
    );
    debugger.clear_history();
    const auto false_condition = debugger.set_conditional_execution_breakpoint(
        0x020DU,
        "A == $99"
    );
    passed &= expect(
        false_condition.succeeded()
            && debugger.run(7U).reason == StopReason::instruction_limit,
        "False breakpoint condition stopped execution"
    );
    const auto true_condition = debugger.set_conditional_execution_breakpoint(
        0x020DU,
        "A == $42 && z == 0 && symbol(\"loop\") == $020D "
        "&& mem8[$0000] == $42 && (1 || mem8[$10000])"
    );
    const auto access_count_before_condition = debugger.memory_access_size();
    const auto conditional_stop = debugger.run(1U);
    passed &= expect(
        true_condition.succeeded()
            && conditional_stop.reason == StopReason::execution_breakpoint
            && conditional_stop.trigger_address == 0x020DU
            && conditional_stop.instructions_executed == 0U
            && conditional_stop.condition_error
                == jr800::debugger::ExpressionEvaluationError::none
            && debugger.memory_access_size() == access_count_before_condition,
        "True breakpoint condition or non-invasive memory evaluation differs"
    );
    const auto rejected_condition =
        debugger.set_conditional_execution_breakpoint(0x020DU, "A ==");
    const auto preserved_condition_stop = debugger.run(1U);
    passed &= expect(
        !rejected_condition.succeeded()
            && rejected_condition.error
                == jr800::debugger::ExpressionCompileError::invalid_syntax
            && preserved_condition_stop.reason
                == StopReason::execution_breakpoint,
        "Rejected condition replaced the active breakpoint"
    );
    const auto runtime_error_condition =
        debugger.set_conditional_execution_breakpoint(
            0x020DU,
            "mem8[$10000] != 0"
        );
    const auto condition_error_stop = debugger.run(1U);
    passed &= expect(
        runtime_error_condition.succeeded()
            && condition_error_stop.reason
                == StopReason::breakpoint_condition_error
            && condition_error_stop.condition_error
                == jr800::debugger::ExpressionEvaluationError::address_out_of_range
            && condition_error_stop.instructions_executed == 0U
            && debugger.memory_access_size() == access_count_before_condition,
        "Breakpoint condition evaluation error was not an explicit stop"
    );
    const auto target_before_condition = debugger.run_to(0x020DU, 1U);
    passed &= expect(
        target_before_condition.reason == StopReason::address_reached
            && target_before_condition.condition_error
                == jr800::debugger::ExpressionEvaluationError::none
            && target_before_condition.instructions_executed == 0U,
        "Run-to target did not take priority over a failing condition"
    );
    debugger.set_execution_breakpoint(0x020DU, false);

    auto wrong_integrity = linked.output->application.integrity_sha256;
    wrong_integrity.front() ^= 0xFFU;
    passed &= expect(
        debugger.load_debug_info(linked.output->debug_info, wrong_integrity)
            == DebugInfoLoadResult::integrity_mismatch,
        "Mismatched JR8DBG/application binding was accepted"
    );
    passed &= expect(
        debugger.source_at(0x0207) == source,
        "Rejected JR8DBG data replaced the last valid source mapping"
    );
    passed &= expect(
        debugger.evaluate_symbol_watch(5U) == result_watch,
        "Rejected JR8DBG data changed an existing symbol watch"
    );
    const auto retained_symbol_expression = debugger.evaluate_expression_watch(6U);
    passed &= expect(
        retained_symbol_expression.has_value()
            && retained_symbol_expression->succeeded()
            && retained_symbol_expression->value == 0x020DU,
        "Rejected JR8DBG data changed symbol expression resolution"
    );
    auto ambiguous_debug_info = linked.output->debug_info;
    ambiguous_debug_info.symbols.push_back(
        jr800::formats::jr8dbg::Symbol{
            "loop",
            jr800::formats::jr8dbg::SymbolBinding::local,
            jr800::formats::jr8dbg::SymbolKind::address,
            0x3000U,
            0U,
            0U,
        }
    );
    ambiguous_debug_info.symbols.push_back(
        jr800::formats::jr8dbg::Symbol{
            "constant",
            jr800::formats::jr8dbg::SymbolBinding::local,
            jr800::formats::jr8dbg::SymbolKind::absolute,
            0x0042U,
            0U,
            {},
        }
    );
    passed &= expect(
        debugger.load_debug_info(
            ambiguous_debug_info,
            linked.output->application.integrity_sha256
        ) == DebugInfoLoadResult::loaded,
        "Valid duplicate-local symbol fixture was rejected"
    );
    passed &= expect(
        !debugger.evaluate_symbol_watch(5U).has_value(),
        "Successful JR8DBG replacement retained a stale symbol watch"
    );
    const auto ambiguous_symbol_expression = debugger.evaluate_expression_watch(6U);
    passed &= expect(
        ambiguous_symbol_expression.has_value()
            && ambiguous_symbol_expression->error
                == jr800::debugger::ExpressionEvaluationError::ambiguous_symbol,
        "Debug-symbol expression guessed between duplicate local names"
    );
    passed &= expect(
        debugger.symbol_address("loop").status
                == jr800::debugger::SymbolAddressStatus::ambiguous
            && debugger.symbol_address("constant").status
                == jr800::debugger::SymbolAddressStatus::not_address,
        "Debugger guessed an ambiguous or non-address symbol"
    );
    const auto absolute_watch = debugger.set_symbol_watch(7U, "constant");
    const auto ambiguous_replacement = debugger.set_symbol_watch(7U, "loop");
    const auto preserved_absolute_watch = debugger.evaluate_symbol_watch(7U);
    passed &= expect(
        absolute_watch.succeeded()
            && ambiguous_replacement.status
                == jr800::debugger::SymbolWatchRegistrationStatus::ambiguous
            && preserved_absolute_watch.has_value()
            && preserved_absolute_watch->value == 0x0042U
            && preserved_absolute_watch->kind
                == jr800::formats::jr8dbg::SymbolKind::absolute
            && preserved_absolute_watch->binding
                == jr800::formats::jr8dbg::SymbolBinding::local
            && preserved_absolute_watch->size == 0U
            && !preserved_absolute_watch->source_file_index.has_value(),
        "Symbol watch rejected an absolute value or replaced it ambiguously"
    );
    passed &= expect(
        debugger.clear_symbol_watch(7U)
            && !debugger.clear_symbol_watch(7U)
            && !debugger.evaluate_symbol_watch(7U).has_value(),
        "Symbol-watch removal did not report missing state"
    );
    passed &= expect(
        debugger.set_symbol_watch(8U, "constant").succeeded()
            && debugger.set_symbol_watch(9U, "constant").succeeded(),
        "Symbol-watch clear-all fixture could not be registered"
    );
    debugger.clear_symbol_watches();
    passed &= expect(
        !debugger.evaluate_symbol_watch(8U).has_value()
            && !debugger.evaluate_symbol_watch(9U).has_value(),
        "Symbol-watch clear-all retained a watch"
    );
    passed &= expect(
        debugger.set_expression_watch(
            10U,
            "symbol(\"constant\") + 1"
        ).succeeded(),
        "Absolute-symbol expression watch was not compiled"
    );
    const auto absolute_symbol_expression = debugger.evaluate_expression_watch(10U);
    passed &= expect(
        absolute_symbol_expression.has_value()
            && absolute_symbol_expression->succeeded()
            && absolute_symbol_expression->value == 0x0043U,
        "Absolute JR8DBG symbol was not usable in an expression"
    );
    passed &= expect(
        debugger.load_debug_info(
            linked.output->debug_info,
            linked.output->application.integrity_sha256
        ) == DebugInfoLoadResult::loaded,
        "Original JR8DBG could not be restored after symbol-expression tests"
    );
    const auto restored_symbol_expression = debugger.evaluate_expression_watch(6U);
    const auto missing_absolute_expression = debugger.evaluate_expression_watch(10U);
    passed &= expect(
        restored_symbol_expression.has_value()
            && restored_symbol_expression->succeeded()
            && restored_symbol_expression->value == 0x020DU
            && missing_absolute_expression.has_value()
            && missing_absolute_expression->error
                == jr800::debugger::ExpressionEvaluationError::symbol_not_found,
        "Symbol expressions did not track successful JR8DBG replacement"
    );

    jr800::core::SyntheticMachine step_over_machine;
    const std::vector<std::uint8_t> step_over_program{
        0x8DU, 0x03U,
        0x86U, 0x55U,
        0x01U,
        0x86U, 0x42U,
        0x39U,
    };
    passed &= expect(
        step_over_machine.bus().load(0x3000U, step_over_program),
        "Step-over program did not fit synthetic RAM"
    );
    step_over_machine.execution().initialize(
        jr800::isa::CpuProfile::hd6301v1,
        0x3000U,
        0x01FFU
    );
    jr800::debugger::Debugger step_over_debugger{16U, 64U};
    passed &= expect(
        step_over_debugger.attach(step_over_machine.execution()),
        "Step-over debugger did not attach"
    );
    step_over_debugger.set_execution_breakpoint(0x3000U, true);
    step_over_debugger.set_execution_breakpoint(0x3002U, true);
    const auto completed_step_over = step_over_debugger.step_over(10U);
    passed &= expect(
        completed_step_over.reason == StopReason::address_reached
            && completed_step_over.trigger_address == 0x3002U
            && completed_step_over.instructions_executed == 3U
            && !completed_step_over.continuation_address.has_value()
            && step_over_machine.execution().cpu().state().pc == 0x3002U
            && step_over_machine.execution().cpu().state().sp == 0x01FFU
            && step_over_machine.execution().cpu().state().a == 0x42U,
        "Step-over did not bypass the current call and stop at its return address"
    );
    const auto linear_step_over = step_over_debugger.step_over(10U);
    passed &= expect(
        linear_step_over.reason == StopReason::step_complete
            && linear_step_over.instructions_executed == 1U
            && !linear_step_over.continuation_address.has_value()
            && step_over_machine.execution().cpu().state().pc == 0x3004U
            && step_over_machine.execution().cpu().state().a == 0x55U,
        "Step-over did not fall back to one step for a non-call instruction"
    );

    step_over_machine.execution().initialize(
        jr800::isa::CpuProfile::hd6301v1,
        0x3000U,
        0x01FFU
    );
    const auto bounded_step_over = step_over_debugger.step_over(1U);
    passed &= expect(
        bounded_step_over.reason == StopReason::instruction_limit
            && bounded_step_over.instructions_executed == 1U
            && bounded_step_over.continuation_address == 0x3002U
            && step_over_machine.execution().cpu().state().pc == 0x3005U,
        "Bounded step-over did not expose its reusable continuation address"
    );
    const auto continued_step_over = step_over_debugger.run_to(
        bounded_step_over.continuation_address.value_or(0U),
        10U
    );
    passed &= expect(
        continued_step_over.reason == StopReason::address_reached
            && continued_step_over.instructions_executed == 2U
            && step_over_machine.execution().cpu().state().pc == 0x3002U,
        "Step-over continuation did not reach the return address"
    );

    step_over_machine.execution().initialize(
        jr800::isa::CpuProfile::hd6301v1,
        0x3000U,
        0x01FFU
    );
    step_over_debugger.set_execution_breakpoint(0x3005U, true);
    const auto interrupted_step_over = step_over_debugger.step_over(10U);
    passed &= expect(
        interrupted_step_over.reason == StopReason::execution_breakpoint
            && interrupted_step_over.trigger_address == 0x3005U
            && interrupted_step_over.instructions_executed == 1U
            && !interrupted_step_over.continuation_address.has_value(),
        "Step-over hid a breakpoint inside the called routine"
    );
    step_over_debugger.clear_execution_breakpoints();
    step_over_debugger.set_memory_watchpoint(
        0x01FFU,
        MemoryWatchpointMode::write,
        true
    );
    step_over_machine.execution().initialize(
        jr800::isa::CpuProfile::hd6301v1,
        0x3000U,
        0x01FFU
    );
    const auto watched_step_over = step_over_debugger.step_over(10U);
    passed &= expect(
        watched_step_over.reason == StopReason::memory_watchpoint
            && watched_step_over.trigger_address == 0x01FFU
            && watched_step_over.trigger_access
                == jr800::core::AccessKind::data_write
            && watched_step_over.instructions_executed == 1U
            && !watched_step_over.continuation_address.has_value(),
        "Step-over hid a watchpoint in the call instruction"
    );
    step_over_debugger.detach();
    passed &= expect(
        step_over_debugger.step_over(10U).reason == StopReason::detached,
        "Detached step-over did not fail visibly"
    );

    jr800::core::SyntheticMachine step_out_machine;
    const std::vector<std::uint8_t> step_out_program{
        0x36U,
        0x8DU, 0x03U,
        0x32U,
        0x39U,
        0x01U,
        0x01U,
        0x39U,
    };
    passed &= expect(
        step_out_machine.bus().load(0x4000U, step_out_program),
        "Step-out program did not fit synthetic RAM"
    );
    step_out_machine.bus().poke8(0x01FEU, 0x50U);
    step_out_machine.bus().poke8(0x01FFU, 0x00U);
    step_out_machine.execution().initialize(
        jr800::isa::CpuProfile::hd6301v1,
        0x4000U,
        0x01FDU
    );
    jr800::debugger::Debugger step_out_debugger{16U, 64U};
    passed &= expect(
        step_out_debugger.attach(step_out_machine.execution()),
        "Step-out debugger did not attach"
    );
    step_out_debugger.set_execution_breakpoint(0x4000U, true);
    step_out_debugger.set_execution_breakpoint(0x5000U, true);
    const auto completed_step_out = step_out_debugger.step_out(10U);
    passed &= expect(
        completed_step_out.stop.reason == StopReason::step_out_complete
            && completed_step_out.stop.trigger_address == 0x5000U
            && completed_step_out.stop.instructions_executed == 6U
            && !completed_step_out.state.continued
            && completed_step_out.state.nesting_depth == 0U
            && step_out_machine.execution().cpu().state().pc == 0x5000U
            && step_out_machine.execution().cpu().state().sp == 0x01FFU,
        "Step-out did not track the nested call and stack-local byte"
    );

    step_out_machine.execution().initialize(
        jr800::isa::CpuProfile::hd6301v1,
        0x4000U,
        0x01FDU
    );
    const auto bounded_step_out = step_out_debugger.step_out(3U);
    passed &= expect(
        bounded_step_out.stop.reason == StopReason::instruction_limit
            && bounded_step_out.stop.instructions_executed == 3U
            && bounded_step_out.state.continued
            && bounded_step_out.state.nesting_depth == 1U
            && step_out_machine.execution().cpu().state().pc == 0x4007U,
        "Bounded step-out did not preserve its nested-call state"
    );
    const auto continued_step_out = step_out_debugger.step_out(
        3U,
        bounded_step_out.state
    );
    passed &= expect(
        continued_step_out.stop.reason == StopReason::step_out_complete
            && continued_step_out.stop.trigger_address == 0x5000U
            && continued_step_out.stop.instructions_executed == 3U
            && !continued_step_out.state.continued
            && continued_step_out.state.nesting_depth == 0U,
        "Continued step-out did not complete the original frame"
    );

    step_out_machine.execution().initialize(
        jr800::isa::CpuProfile::hd6301v1,
        0x4000U,
        0x01FDU
    );
    const auto nested_condition =
        step_out_debugger.set_conditional_execution_breakpoint(
            0x4006U,
            "pc == $4006"
        );
    const auto interrupted_step_out = step_out_debugger.step_out(10U);
    passed &= expect(
        nested_condition.succeeded()
            && interrupted_step_out.stop.reason
                == StopReason::execution_breakpoint
            && interrupted_step_out.stop.trigger_address == 0x4006U
            && interrupted_step_out.stop.instructions_executed == 2U
            && interrupted_step_out.stop.condition_error
                == jr800::debugger::ExpressionEvaluationError::none
            && !interrupted_step_out.state.continued,
        "Step-out hid a conditional breakpoint inside a nested call"
    );
    step_out_debugger.detach();
    passed &= expect(
        step_out_debugger.step_out(10U).stop.reason == StopReason::detached,
        "Detached step-out did not fail visibly"
    );

    return passed ? 0 : 1;
}
