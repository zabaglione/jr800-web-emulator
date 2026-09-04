// SPDX-License-Identifier: MIT

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "jr800/core/synthetic_machine.hpp"
#include "jr800/debugger/debugger.hpp"
#include "jr800/debugger/expression.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

std::unique_ptr<jr800::debugger::CompiledExpression> compile(
    std::string_view text,
    jr800::debugger::ExpressionCompileDiagnostic& diagnostic
) {
    return jr800::debugger::compile_expression(text, diagnostic);
}

class TestSymbolResolver final
    : public jr800::debugger::ExpressionSymbolResolver {
public:
    [[nodiscard]] jr800::debugger::ExpressionSymbolLookupResult resolve(
        std::string_view name
    ) const noexcept override {
        using jr800::debugger::ExpressionSymbolLookupStatus;
        if (name == "Target.Name") {
            return {ExpressionSymbolLookupStatus::found, 0x0020U};
        }
        if (name == "duplicate") {
            return {ExpressionSymbolLookupStatus::ambiguous, 0U};
        }
        return {ExpressionSymbolLookupStatus::not_found, 0U};
    }
};

}  // namespace

int main() {
    using jr800::debugger::ExpressionCompileError;
    using jr800::debugger::ExpressionEvaluationError;

    bool passed = true;
    jr800::core::SyntheticMachine machine;
    machine.execution().initialize(
        jr800::isa::CpuProfile::hd6301v1,
        0x1234U,
        0xABCDU
    );
    machine.bus().poke8(0x0020U, 0x5AU);

    jr800::debugger::ExpressionCompileDiagnostic diagnostic;
    auto expression = compile(
        "PC == $1234 && sp == $abcd && cycles == 0 "
        "&& mem8[$20] + 1 == $5b",
        diagnostic
    );
    const auto result = expression == nullptr
        ? jr800::debugger::ExpressionEvaluationResult{}
        : expression->evaluate(machine.execution());
    passed &= expect(
        diagnostic.succeeded() && expression != nullptr
            && result.succeeded() && result.value == 1U,
        "Register, cycle, or memory expression differs"
    );

    jr800::debugger::Debugger expression_debugger;
    passed &= expect(
        expression != nullptr
            && !expression_debugger.evaluate_expression(*expression).has_value(),
        "Detached debugger evaluated a standalone expression"
    );
    passed &= expect(
        expression_debugger.attach(machine.execution()),
        "Expression debugger did not attach"
    );
    const auto debugger_result = expression == nullptr
        ? std::optional<jr800::debugger::ExpressionEvaluationResult>{}
        : expression_debugger.evaluate_expression(*expression);
    passed &= expect(
        debugger_result.has_value() && debugger_result->succeeded()
            && debugger_result->value == 1U,
        "Attached debugger did not evaluate a standalone expression"
    );
    expression_debugger.detach();

    expression = compile(
        "1 + 2 * 3 == 7 && (8 >> 2) == 2 && (~0 & $ff) == $ff",
        diagnostic
    );
    passed &= expect(
        expression != nullptr
            && expression->evaluate(machine.execution()).value == 1U,
        "Expression precedence differs"
    );

    expression = compile(
        "0 && mem8[$10000] || 1 || (1 / 0)",
        diagnostic
    );
    const auto short_circuit = expression == nullptr
        ? jr800::debugger::ExpressionEvaluationResult{}
        : expression->evaluate(machine.execution());
    passed &= expect(
        expression != nullptr && short_circuit.succeeded()
            && short_circuit.value == 1U,
        "Logical operators did not short-circuit"
    );

    expression = compile("1 / 0", diagnostic);
    passed &= expect(
        expression != nullptr
            && expression->evaluate(machine.execution()).error
                == ExpressionEvaluationError::division_by_zero,
        "Division by zero was not reported"
    );
    expression = compile("1 << 64", diagnostic);
    passed &= expect(
        expression != nullptr
            && expression->evaluate(machine.execution()).error
                == ExpressionEvaluationError::invalid_shift,
        "Invalid shift was not reported"
    );
    expression = compile("mem8[$10000]", diagnostic);
    passed &= expect(
        expression != nullptr
            && expression->evaluate(machine.execution()).error
                == ExpressionEvaluationError::address_out_of_range,
        "Out-of-range memory expression was not reported"
    );

    const TestSymbolResolver symbol_resolver;
    expression = compile("mem8[symbol(\"Target.Name\")]", diagnostic);
    const auto symbol_result = expression == nullptr
        ? jr800::debugger::ExpressionEvaluationResult{}
        : expression->evaluate(machine.execution(), &symbol_resolver);
    passed &= expect(
        expression != nullptr && symbol_result.succeeded()
            && symbol_result.value == 0x5AU,
        "Exact debug symbol did not participate in an expression"
    );
    expression = compile("symbol(\"target.name\")", diagnostic);
    passed &= expect(
        expression != nullptr
            && expression->evaluate(machine.execution(), &symbol_resolver).error
                == ExpressionEvaluationError::symbol_not_found,
        "Debug-symbol expression lookup was not case-sensitive"
    );
    expression = compile("symbol(\"duplicate\")", diagnostic);
    passed &= expect(
        expression != nullptr
            && expression->evaluate(machine.execution(), &symbol_resolver).error
                == ExpressionEvaluationError::ambiguous_symbol,
        "Ambiguous debug-symbol expression was guessed"
    );
    expression = compile("symbol(\"Target.Name\")", diagnostic);
    passed &= expect(
        expression != nullptr
            && expression->evaluate(machine.execution()).error
                == ExpressionEvaluationError::symbol_not_found,
        "Missing expression symbol resolver was not explicit"
    );
    expression = compile("1 || symbol(\"missing\")", diagnostic);
    const auto short_circuit_symbol_result = expression == nullptr
        ? jr800::debugger::ExpressionEvaluationResult{}
        : expression->evaluate(machine.execution());
    passed &= expect(
        expression != nullptr && short_circuit_symbol_result.succeeded()
            && short_circuit_symbol_result.value == 1U,
        "Short-circuit evaluation resolved an unreachable debug symbol"
    );
    expression = compile("symbol(\"unterminated)", diagnostic);
    passed &= expect(
        expression == nullptr
            && diagnostic.error == ExpressionCompileError::invalid_token,
        "Unterminated debug-symbol name was accepted"
    );

    expression = compile("", diagnostic);
    passed &= expect(
        expression == nullptr && diagnostic.error == ExpressionCompileError::empty,
        "Empty expression was accepted"
    );
    expression = compile("unknown_name", diagnostic);
    passed &= expect(
        expression == nullptr
            && diagnostic.error == ExpressionCompileError::unknown_identifier
            && diagnostic.offset == 0U,
        "Unknown expression identifier was accepted"
    );
    expression = compile("A ==", diagnostic);
    passed &= expect(
        expression == nullptr
            && diagnostic.error == ExpressionCompileError::invalid_syntax
            && diagnostic.offset == 4U,
        "Incomplete expression diagnostic differs"
    );
    const std::string embedded_nul{"1\0 || 0", 7U};
    expression = compile(embedded_nul, diagnostic);
    passed &= expect(
        expression == nullptr
            && diagnostic.error == ExpressionCompileError::invalid_token
            && diagnostic.offset == 1U,
        "Embedded NUL expression was accepted"
    );
    expression = compile(std::string(257U, '1'), diagnostic);
    passed &= expect(
        expression == nullptr
            && diagnostic.error == ExpressionCompileError::too_long,
        "Oversized expression was accepted"
    );
    std::string wide_expression{"1"};
    for (std::size_t index = 0; index < 64U; ++index) {
        wide_expression += "+1";
    }
    expression = compile(wide_expression, diagnostic);
    passed &= expect(
        expression == nullptr
            && diagnostic.error == ExpressionCompileError::too_complex,
        "Wide expression exceeded the node limit without an error"
    );
    const std::string deep_expression = std::string(34U, '(') + "1"
        + std::string(34U, ')');
    expression = compile(deep_expression, diagnostic);
    passed &= expect(
        expression == nullptr
            && diagnostic.error == ExpressionCompileError::too_complex,
        "Deep expression exceeded the parser limit without an error"
    );

    jr800::debugger::Debugger debugger;
    passed &= expect(
        debugger.attach(machine.execution()),
        "Debugger could not attach for expression-watch tests"
    );
    const auto registered = debugger.set_expression_watch(
        7U,
        "PC + mem8[$20]"
    );
    const auto watched = debugger.evaluate_expression_watch(7U);
    passed &= expect(
        registered.succeeded() && watched.has_value()
            && watched->succeeded() && watched->value == 0x128EU
            && debugger.history_size() == 0U
            && debugger.memory_access_size() == 0U,
        "Expression watch did not evaluate without trace side effects"
    );
    const auto rejected_replacement = debugger.set_expression_watch(7U, "A ==");
    const auto preserved = debugger.evaluate_expression_watch(7U);
    passed &= expect(
        !rejected_replacement.succeeded() && preserved.has_value()
            && preserved->succeeded() && preserved->value == 0x128EU,
        "Invalid expression-watch replacement changed the stored watch"
    );
    passed &= expect(
        debugger.set_expression_watch(9U, "mem8[$10000]").succeeded(),
        "Evaluation-error expression watch was not registered"
    );
    const auto failed_evaluation = debugger.evaluate_expression_watch(9U);
    passed &= expect(
        failed_evaluation.has_value()
            && failed_evaluation->error
                == ExpressionEvaluationError::address_out_of_range,
        "Expression-watch evaluation error was not retained"
    );
    passed &= expect(
        debugger.clear_expression_watch(7U)
            && !debugger.clear_expression_watch(7U)
            && !debugger.evaluate_expression_watch(7U).has_value(),
        "Expression-watch removal did not report missing state"
    );
    debugger.clear_expression_watches();
    passed &= expect(
        !debugger.evaluate_expression_watch(9U).has_value(),
        "Expression-watch clear-all retained a watch"
    );

    return passed ? 0 : 1;
}
