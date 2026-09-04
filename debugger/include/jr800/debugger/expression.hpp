// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "jr800/core/machine.hpp"

namespace jr800::debugger {

enum class ExpressionCompileError : std::uint8_t {
    none,
    empty,
    too_long,
    invalid_token,
    invalid_syntax,
    unknown_identifier,
    too_complex,
};

struct ExpressionCompileDiagnostic {
    ExpressionCompileError error{ExpressionCompileError::none};
    std::size_t offset{};

    [[nodiscard]] constexpr bool succeeded() const noexcept {
        return error == ExpressionCompileError::none;
    }
};

enum class ExpressionEvaluationError : std::uint8_t {
    none,
    unknown_state,
    memory_access,
    division_by_zero,
    invalid_shift,
    address_out_of_range,
    symbol_not_found,
    ambiguous_symbol,
};

enum class ExpressionSymbolLookupStatus : std::uint8_t {
    found,
    not_found,
    ambiguous,
};

struct ExpressionSymbolLookupResult {
    ExpressionSymbolLookupStatus status{ExpressionSymbolLookupStatus::not_found};
    std::uint64_t value{};
};

class ExpressionSymbolResolver {
public:
    virtual ~ExpressionSymbolResolver() = default;

    [[nodiscard]] virtual ExpressionSymbolLookupResult resolve(
        std::string_view name
    ) const noexcept = 0;
};

struct ExpressionEvaluationResult {
    std::uint64_t value{};
    ExpressionEvaluationError error{ExpressionEvaluationError::none};
    core::BusFault bus_fault{core::BusFault::none};
    std::uint16_t fault_address{};
    core::CpuStatePart state_fault{core::CpuStatePart::none};

    [[nodiscard]] constexpr bool succeeded() const noexcept {
        return error == ExpressionEvaluationError::none;
    }
};

class CompiledExpression final {
public:
    ~CompiledExpression();

    CompiledExpression(const CompiledExpression&) = delete;
    CompiledExpression& operator=(const CompiledExpression&) = delete;
    CompiledExpression(CompiledExpression&&) noexcept;
    CompiledExpression& operator=(CompiledExpression&&) noexcept;

    [[nodiscard]] ExpressionEvaluationResult evaluate(
        const core::Machine& machine,
        const ExpressionSymbolResolver* symbol_resolver = nullptr
    ) const noexcept;

private:
    class Impl;

    explicit CompiledExpression(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;

    friend std::unique_ptr<CompiledExpression> compile_expression(
        std::string_view,
        ExpressionCompileDiagnostic&
    );
};

[[nodiscard]] std::unique_ptr<CompiledExpression> compile_expression(
    std::string_view text,
    ExpressionCompileDiagnostic& diagnostic
);

}  // namespace jr800::debugger
