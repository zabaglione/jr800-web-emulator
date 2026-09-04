// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "jr800/core/machine.hpp"
#include "jr800/debugger/expression.hpp"
#include "jr800/formats/jr8dbg.hpp"
#include "jr800/formats/sha256.hpp"

namespace jr800::debugger {

enum class StopReason : std::uint8_t {
    step_complete,
    instruction_limit,
    execution_breakpoint,
    memory_watchpoint,
    cpu_fault,
    detached,
    sleeping,
    address_reached,
    step_out_complete,
    breakpoint_condition_error,
};

enum class MemoryWatchpointMode : std::uint8_t {
    read = 0x01,
    write = 0x02,
    access = 0x03,
};

enum class AccessTraceMask : std::uint8_t {
    instruction_fetch = 0x01,
    data_read = 0x02,
    data_write = 0x04,
    data = 0x06,
    all = 0x07,
};

struct AccessTraceFilter {
    std::uint16_t first_address{};
    std::uint16_t last_address{0xFFFFU};
    AccessTraceMask mask{AccessTraceMask::all};
};

[[nodiscard]] bool is_valid_access_trace_filter(
    AccessTraceFilter filter
) noexcept;

struct StopInfo {
    StopReason reason{StopReason::detached};
    core::StepResult step;
    std::uint16_t trigger_address{};
    std::uint64_t instructions_executed{};
    std::optional<core::AccessKind> trigger_access;
    std::optional<std::uint16_t> continuation_address;
    ExpressionEvaluationError condition_error{
        ExpressionEvaluationError::none
    };
};

struct StepOutState {
    bool continued{};
    std::uint64_t nesting_depth{};
};

struct StepOutResult {
    StopInfo stop;
    StepOutState state;
};

struct ExecutionHistoryEntry {
    std::uint64_t sequence{};
    std::uint64_t cycle_begin{};
    std::uint64_t first_access_sequence{};
    std::uint32_t access_count{};
    std::uint16_t pc_before{};
    std::uint16_t pc_after{};
    core::StepKind kind{core::StepKind::dormant};
    core::InterruptSource interrupt_source{core::InterruptSource::none};
    std::array<std::uint8_t, 3> bytes{};
    std::uint8_t instruction_length{};
    std::uint8_t bytes_fetched{};
    std::uint8_t cycles{};
    core::CpuFault fault{core::CpuFault::none};
    core::BusFault bus_fault{core::BusFault::none};
    std::uint16_t fault_address{};
    core::AccessKind fault_access{core::AccessKind::data_read};
    core::CpuStatePart state_fault{core::CpuStatePart::none};
    core::CpuState state_after{};
};

struct DisassembledInstruction {
    std::uint16_t address{};
    std::array<std::uint8_t, 3> bytes{};
    std::uint8_t length{};
    bool supported{};
    std::string text;
};

enum class DebugInfoLoadResult : std::uint8_t {
    loaded,
    detached,
    invalid_format,
    target_mismatch,
    integrity_mismatch,
};

enum class SymbolAddressStatus : std::uint8_t {
    found,
    not_found,
    ambiguous,
    not_address,
};

struct SymbolAddressResult {
    SymbolAddressStatus status{SymbolAddressStatus::not_found};
    std::uint16_t address{};

    [[nodiscard]] constexpr bool succeeded() const noexcept {
        return status == SymbolAddressStatus::found;
    }
};

enum class SymbolWatchRegistrationStatus : std::uint8_t {
    registered,
    not_found,
    ambiguous,
};

struct SymbolWatchRegistrationResult {
    SymbolWatchRegistrationStatus status{
        SymbolWatchRegistrationStatus::not_found
    };

    [[nodiscard]] constexpr bool succeeded() const noexcept {
        return status == SymbolWatchRegistrationStatus::registered;
    }
};

struct SymbolWatchValue {
    std::uint16_t value{};
    formats::jr8dbg::SymbolBinding binding{
        formats::jr8dbg::SymbolBinding::local
    };
    formats::jr8dbg::SymbolKind kind{
        formats::jr8dbg::SymbolKind::address
    };
    std::uint32_t size{};
    std::optional<std::uint32_t> source_file_index;

    bool operator==(const SymbolWatchValue&) const = default;
};

class Debugger final : private core::MachineObserver {
public:
    explicit Debugger(
        std::size_t history_capacity = 256,
        std::size_t access_capacity = 1024
    );
    ~Debugger() override;

    Debugger(const Debugger&) = delete;
    Debugger& operator=(const Debugger&) = delete;
    Debugger(Debugger&&) = delete;
    Debugger& operator=(Debugger&&) = delete;

    [[nodiscard]] bool attach(core::Machine& machine) noexcept;
    void detach() noexcept;

    [[nodiscard]] core::Machine* machine() noexcept;
    [[nodiscard]] const core::Machine* machine() const noexcept;

    void set_execution_breakpoint(std::uint16_t address, bool enabled) noexcept;
    [[nodiscard]] ExpressionCompileDiagnostic
    set_conditional_execution_breakpoint(
        std::uint16_t address,
        std::string_view condition
    );
    [[nodiscard]] ExpressionCompileDiagnostic set_expression_watch(
        std::uint32_t watch_id,
        std::string_view expression
    );
    [[nodiscard]] std::optional<ExpressionEvaluationResult>
    evaluate_expression(const CompiledExpression& expression) const noexcept;
    [[nodiscard]] bool clear_expression_watch(
        std::uint32_t watch_id
    ) noexcept;
    void clear_expression_watches() noexcept;
    [[nodiscard]] std::optional<ExpressionEvaluationResult>
    evaluate_expression_watch(std::uint32_t watch_id) const noexcept;
    [[nodiscard]] SymbolWatchRegistrationResult set_symbol_watch(
        std::uint32_t watch_id,
        std::string_view symbol_name
    );
    [[nodiscard]] bool clear_symbol_watch(std::uint32_t watch_id) noexcept;
    void clear_symbol_watches() noexcept;
    [[nodiscard]] std::optional<SymbolWatchValue> evaluate_symbol_watch(
        std::uint32_t watch_id
    ) const noexcept;
    void set_memory_watchpoint(
        std::uint16_t address,
        MemoryWatchpointMode mode,
        bool enabled
    ) noexcept;
    void clear_execution_breakpoints() noexcept;
    void clear_memory_watchpoints() noexcept;
    [[nodiscard]] bool has_execution_breakpoint(std::uint16_t address) const noexcept;
    [[nodiscard]] bool has_memory_watchpoint(
        std::uint16_t address,
        MemoryWatchpointMode mode
    ) const noexcept;

    [[nodiscard]] StopInfo step();
    [[nodiscard]] StopInfo step_over(std::uint64_t instruction_limit);
    [[nodiscard]] StepOutResult step_out(
        std::uint64_t instruction_limit,
        StepOutState state = {}
    );
    [[nodiscard]] StopInfo run(std::uint64_t instruction_limit);
    [[nodiscard]] StopInfo run_to(
        std::uint16_t address,
        std::uint64_t instruction_limit
    );

    [[nodiscard]] std::vector<ExecutionHistoryEntry> history() const;
    [[nodiscard]] std::vector<core::BusAccessEvent> memory_accesses(
        AccessTraceFilter filter = {}
    ) const;
    [[nodiscard]] std::size_t history_size() const noexcept;
    [[nodiscard]] std::size_t memory_access_size(
        AccessTraceFilter filter = {}
    ) const;
    void clear_history() noexcept;

    [[nodiscard]] DebugInfoLoadResult load_debug_info(
        const formats::jr8dbg::DebugInfo& debug_info,
        const formats::Sha256Digest& application_integrity
    );
    void clear_debug_info() noexcept;
    [[nodiscard]] const formats::jr8dbg::LineMapping* source_at(
        std::uint16_t address
    ) const noexcept;
    [[nodiscard]] const formats::jr8dbg::SourceFile* source_file(
        std::uint32_t index
    ) const noexcept;
    [[nodiscard]] std::optional<std::uint16_t> source_address(
        std::string_view source_path,
        std::uint32_t line
    ) const noexcept;
    [[nodiscard]] SymbolAddressResult symbol_address(
        std::string_view symbol_name
    ) const noexcept;
    [[nodiscard]] std::vector<const formats::jr8dbg::Symbol*> symbols_at(
        std::uint16_t address
    ) const;
    [[nodiscard]] std::optional<DisassembledInstruction> disassemble(
        std::uint16_t address
    ) const;

private:
    [[nodiscard]] std::optional<StopInfo> execution_breakpoint_stop(
        std::uint16_t address,
        std::uint64_t instructions_executed
    ) const;
    [[nodiscard]] std::optional<std::uint16_t> step_over_target() const noexcept;
    [[nodiscard]] StopInfo run_until(
        std::optional<std::uint16_t> address,
        std::uint64_t instruction_limit
    );
    void on_machine_detached(core::Machine& machine) noexcept override;
    void on_step_begin(const core::CpuState& state) noexcept override;
    void on_step_end(
        const core::StepResult& result,
        const core::CpuState& state
    ) noexcept override;
    void on_bus_access(const core::BusAccessEvent& event) noexcept override;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace jr800::debugger
