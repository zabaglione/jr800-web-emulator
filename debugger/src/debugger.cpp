// SPDX-License-Identifier: MIT

#include "jr800/debugger/debugger.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "jr800/disassembler/disassembler.hpp"
#include "jr800/formats/linked_error.hpp"
#include "jr800/isa/instruction_metadata.hpp"

namespace jr800::debugger {
namespace {

std::uint8_t access_trace_bit(core::AccessKind kind) noexcept {
    switch (kind) {
    case core::AccessKind::instruction_fetch:
        return static_cast<std::uint8_t>(AccessTraceMask::instruction_fetch);
    case core::AccessKind::data_read:
        return static_cast<std::uint8_t>(AccessTraceMask::data_read);
    case core::AccessKind::data_write:
        return static_cast<std::uint8_t>(AccessTraceMask::data_write);
    }
    return 0U;
}

template <typename Value>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity)
        : storage_(std::max<std::size_t>(capacity, 1U)) {}

    void push(Value value) noexcept {
        if (size_ < storage_.size()) {
            storage_[(begin_ + size_) % storage_.size()] = std::move(value);
            ++size_;
            return;
        }
        storage_[begin_] = std::move(value);
        begin_ = (begin_ + 1U) % storage_.size();
    }

    [[nodiscard]] std::vector<Value> values() const {
        std::vector<Value> result;
        result.reserve(size_);
        for (std::size_t index = 0; index < size_; ++index) {
            result.push_back(storage_[(begin_ + index) % storage_.size()]);
        }
        return result;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    void clear() noexcept {
        begin_ = 0U;
        size_ = 0U;
    }

private:
    std::vector<Value> storage_;
    std::size_t begin_{};
    std::size_t size_{};
};

constexpr std::uint8_t watchpoint_mask(MemoryWatchpointMode mode) noexcept {
    switch (mode) {
    case MemoryWatchpointMode::read:
        return 0x01U;
    case MemoryWatchpointMode::write:
        return 0x02U;
    case MemoryWatchpointMode::access:
        return 0x03U;
    }
    return 0U;
}

std::optional<MemoryWatchpointMode> watchpoint_mode(
    core::AccessKind kind
) noexcept {
    switch (kind) {
    case core::AccessKind::instruction_fetch:
        return std::nullopt;
    case core::AccessKind::data_read:
        return MemoryWatchpointMode::read;
    case core::AccessKind::data_write:
        return MemoryWatchpointMode::write;
    }
    return std::nullopt;
}

struct MemoryWatchpointTrigger {
    std::uint16_t address{};
    core::AccessKind access{core::AccessKind::data_read};
};

struct ExactSymbolMatch {
    const formats::jr8dbg::Symbol* symbol{};
    bool ambiguous{};
};

ExactSymbolMatch find_exact_symbol(
    const std::optional<formats::jr8dbg::DebugInfo>& debug_info,
    std::string_view symbol_name
) noexcept {
    if (!debug_info.has_value() || symbol_name.empty()) {
        return {};
    }
    const formats::jr8dbg::Symbol* match = nullptr;
    for (const auto& symbol : debug_info->symbols) {
        if (symbol.name != symbol_name) {
            continue;
        }
        if (match != nullptr) {
            return ExactSymbolMatch{nullptr, true};
        }
        match = &symbol;
    }
    return ExactSymbolMatch{match, false};
}

class DebugInfoExpressionSymbolResolver final
    : public ExpressionSymbolResolver {
public:
    explicit DebugInfoExpressionSymbolResolver(
        const std::optional<formats::jr8dbg::DebugInfo>& debug_info
    ) noexcept : debug_info_(debug_info) {}

    [[nodiscard]] ExpressionSymbolLookupResult resolve(
        std::string_view name
    ) const noexcept override {
        const auto match = find_exact_symbol(debug_info_, name);
        if (match.ambiguous) {
            return {ExpressionSymbolLookupStatus::ambiguous, 0U};
        }
        if (match.symbol == nullptr) {
            return {ExpressionSymbolLookupStatus::not_found, 0U};
        }
        return {
            ExpressionSymbolLookupStatus::found,
            match.symbol->value,
        };
    }

private:
    const std::optional<formats::jr8dbg::DebugInfo>& debug_info_;
};

}  // namespace

bool is_valid_access_trace_filter(AccessTraceFilter filter) noexcept {
    const auto mask = static_cast<std::uint8_t>(filter.mask);
    const auto all = static_cast<std::uint8_t>(AccessTraceMask::all);
    return filter.first_address <= filter.last_address
        && mask != 0U
        && (mask & static_cast<std::uint8_t>(~all)) == 0U;
}

class Debugger::Impl {
public:
    Impl(std::size_t history_capacity, std::size_t access_capacity)
        : history(history_capacity), accesses(access_capacity) {}

    core::Machine* machine{};
    std::array<std::uint8_t, 65'536> execution_breakpoints{};
    std::unordered_map<std::uint16_t, std::unique_ptr<CompiledExpression>>
        breakpoint_conditions;
    std::unordered_map<std::uint32_t, std::unique_ptr<CompiledExpression>>
        expression_watches;
    std::unordered_map<std::uint32_t, formats::jr8dbg::Symbol>
        symbol_watches;
    std::array<std::uint8_t, 65'536> memory_watchpoints{};
    RingBuffer<ExecutionHistoryEntry> history;
    RingBuffer<core::BusAccessEvent> accesses;
    std::uint64_t history_sequence{};
    core::CpuState step_before{};
    std::uint64_t first_access_sequence{};
    std::uint32_t current_access_count{};
    std::optional<MemoryWatchpointTrigger> pending_watchpoint;
    std::optional<formats::jr8dbg::DebugInfo> debug_info;
};

Debugger::Debugger(std::size_t history_capacity, std::size_t access_capacity)
    : impl_(std::make_unique<Impl>(history_capacity, access_capacity)) {}

Debugger::~Debugger() {
    detach();
}

bool Debugger::attach(core::Machine& machine) noexcept {
    if (impl_->machine == &machine) {
        return true;
    }
    detach();
    if (!machine.add_observer(this)) {
        return false;
    }
    impl_->machine = &machine;
    return true;
}

void Debugger::detach() noexcept {
    if (impl_->machine != nullptr && impl_->machine->has_observer(this)) {
        impl_->machine->remove_observer(*this);
    }
    impl_->machine = nullptr;
    impl_->debug_info.reset();
    impl_->symbol_watches.clear();
}

core::Machine* Debugger::machine() noexcept {
    return impl_->machine;
}

const core::Machine* Debugger::machine() const noexcept {
    return impl_->machine;
}

void Debugger::set_execution_breakpoint(std::uint16_t address, bool enabled) noexcept {
    impl_->execution_breakpoints[address] = enabled ? 1U : 0U;
    impl_->breakpoint_conditions.erase(address);
}

ExpressionCompileDiagnostic Debugger::set_conditional_execution_breakpoint(
    std::uint16_t address,
    std::string_view condition
) {
    ExpressionCompileDiagnostic diagnostic;
    auto compiled = compile_expression(condition, diagnostic);
    if (compiled == nullptr) {
        return diagnostic;
    }
    impl_->breakpoint_conditions.insert_or_assign(address, std::move(compiled));
    impl_->execution_breakpoints[address] = 1U;
    return diagnostic;
}

ExpressionCompileDiagnostic Debugger::set_expression_watch(
    std::uint32_t watch_id,
    std::string_view expression
) {
    ExpressionCompileDiagnostic diagnostic;
    auto compiled = compile_expression(expression, diagnostic);
    if (compiled == nullptr) {
        return diagnostic;
    }
    impl_->expression_watches.insert_or_assign(watch_id, std::move(compiled));
    return diagnostic;
}

std::optional<ExpressionEvaluationResult> Debugger::evaluate_expression(
    const CompiledExpression& expression
) const noexcept {
    if (impl_->machine == nullptr) {
        return std::nullopt;
    }
    const DebugInfoExpressionSymbolResolver resolver{impl_->debug_info};
    return expression.evaluate(*impl_->machine, &resolver);
}

bool Debugger::clear_expression_watch(std::uint32_t watch_id) noexcept {
    const auto watch = impl_->expression_watches.find(watch_id);
    if (watch == impl_->expression_watches.end()) {
        return false;
    }
    impl_->expression_watches.erase(watch);
    return true;
}

void Debugger::clear_expression_watches() noexcept {
    impl_->expression_watches.clear();
}

std::optional<ExpressionEvaluationResult>
Debugger::evaluate_expression_watch(std::uint32_t watch_id) const noexcept {
    if (impl_->machine == nullptr) {
        return std::nullopt;
    }
    const auto watch = impl_->expression_watches.find(watch_id);
    if (watch == impl_->expression_watches.end()) {
        return std::nullopt;
    }
    return evaluate_expression(*watch->second);
}

SymbolWatchRegistrationResult Debugger::set_symbol_watch(
    std::uint32_t watch_id,
    std::string_view symbol_name
) {
    const auto match = find_exact_symbol(impl_->debug_info, symbol_name);
    if (match.ambiguous) {
        return {SymbolWatchRegistrationStatus::ambiguous};
    }
    if (match.symbol == nullptr) {
        return {SymbolWatchRegistrationStatus::not_found};
    }
    impl_->symbol_watches.insert_or_assign(watch_id, *match.symbol);
    return {SymbolWatchRegistrationStatus::registered};
}

bool Debugger::clear_symbol_watch(std::uint32_t watch_id) noexcept {
    return impl_->symbol_watches.erase(watch_id) != 0U;
}

void Debugger::clear_symbol_watches() noexcept {
    impl_->symbol_watches.clear();
}

std::optional<SymbolWatchValue> Debugger::evaluate_symbol_watch(
    std::uint32_t watch_id
) const noexcept {
    const auto watch = impl_->symbol_watches.find(watch_id);
    if (watch == impl_->symbol_watches.end()) {
        return std::nullopt;
    }
    const auto& symbol = watch->second;
    return SymbolWatchValue{
        symbol.value,
        symbol.binding,
        symbol.kind,
        symbol.size,
        symbol.source_file_index,
    };
}

void Debugger::set_memory_watchpoint(
    std::uint16_t address,
    MemoryWatchpointMode mode,
    bool enabled
) noexcept {
    const auto mask = watchpoint_mask(mode);
    if (enabled) {
        impl_->memory_watchpoints[address] |= mask;
    } else {
        impl_->memory_watchpoints[address] &= static_cast<std::uint8_t>(~mask);
    }
}

void Debugger::clear_execution_breakpoints() noexcept {
    impl_->execution_breakpoints.fill(0U);
    impl_->breakpoint_conditions.clear();
}

void Debugger::clear_memory_watchpoints() noexcept {
    impl_->memory_watchpoints.fill(0U);
}

bool Debugger::has_execution_breakpoint(std::uint16_t address) const noexcept {
    return impl_->execution_breakpoints[address] != 0U;
}

std::optional<StopInfo> Debugger::execution_breakpoint_stop(
    std::uint16_t address,
    std::uint64_t instructions_executed
) const {
    if (!has_execution_breakpoint(address)) {
        return std::nullopt;
    }
    const auto condition = impl_->breakpoint_conditions.find(address);
    if (condition == impl_->breakpoint_conditions.end()) {
        core::StepResult step_result;
        step_result.pc_before = address;
        step_result.pc_after = address;
        return StopInfo{
            StopReason::execution_breakpoint,
            step_result,
            address,
            instructions_executed,
            std::nullopt,
            std::nullopt,
        };
    }

    const auto evaluation = evaluate_expression(*condition->second);
    if (!evaluation.has_value()
        || (evaluation->succeeded() && evaluation->value == 0U)) {
        return std::nullopt;
    }

    core::StepResult step_result;
    step_result.pc_before = address;
    step_result.pc_after = address;
    step_result.bus_fault = evaluation->bus_fault;
    step_result.fault_address = evaluation->fault_address;
    step_result.fault_access = core::AccessKind::data_read;
    step_result.state_fault = evaluation->state_fault;
    return StopInfo{
        evaluation->succeeded()
            ? StopReason::execution_breakpoint
            : StopReason::breakpoint_condition_error,
        step_result,
        address,
        instructions_executed,
        std::nullopt,
        std::nullopt,
        evaluation->error,
    };
}

bool Debugger::has_memory_watchpoint(
    std::uint16_t address,
    MemoryWatchpointMode mode
) const noexcept {
    const auto mask = watchpoint_mask(mode);
    return mask != 0U
        && (impl_->memory_watchpoints[address] & mask) == mask;
}

StopInfo Debugger::step() {
    if (impl_->machine == nullptr) {
        return StopInfo{
            StopReason::detached,
            {},
            0,
            0,
            std::nullopt,
            std::nullopt,
        };
    }
    const auto result = impl_->machine->step_instruction();
    const auto instructions_executed =
        result.kind == core::StepKind::instruction ? 1U : 0U;
    if (!result.step_completed
        && result.fault == core::CpuFault::none
        && impl_->machine->cpu().state().execution_state
            != core::CpuExecutionState::active) {
        return StopInfo{
            StopReason::sleeping,
            result,
            result.pc_after,
            0,
            std::nullopt,
            std::nullopt,
        };
    }
    if (!result.succeeded()) {
        const auto trigger_address = result.fault == core::CpuFault::bus_access
            ? result.fault_address
            : result.pc_before;
        return StopInfo{
            StopReason::cpu_fault,
            result,
            trigger_address,
            instructions_executed,
            std::nullopt,
            std::nullopt,
        };
    }
    if (impl_->pending_watchpoint.has_value()) {
        return StopInfo{
            StopReason::memory_watchpoint,
            result,
            impl_->pending_watchpoint->address,
            instructions_executed,
            impl_->pending_watchpoint->access,
            std::nullopt,
        };
    }
    if (impl_->machine->cpu().state().execution_state
        != core::CpuExecutionState::active) {
        return StopInfo{
            StopReason::sleeping,
            result,
            result.pc_after,
            instructions_executed,
            std::nullopt,
            std::nullopt,
        };
    }
    return StopInfo{
        StopReason::step_complete,
        result,
        result.pc_after,
        instructions_executed,
        std::nullopt,
        std::nullopt,
    };
}

std::optional<std::uint16_t> Debugger::step_over_target() const noexcept {
    if (impl_->machine == nullptr) {
        return std::nullopt;
    }
    const auto& state = impl_->machine->cpu().state();
    if (state.execution_state != core::CpuExecutionState::active
        || !state.knowledge.knows(core::CpuRegister::program_counter)) {
        return std::nullopt;
    }
    const auto opcode = impl_->machine->inspect8(state.pc);
    if (!opcode.succeeded()) {
        return std::nullopt;
    }
    const auto* instruction = isa::decode_instruction(
        impl_->machine->cpu().profile(),
        *opcode.value
    );
    if (instruction == nullptr || !isa::is_step_over_candidate(*instruction)) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(
        state.pc + instruction->instruction_length
    );
}

StopInfo Debugger::step_over(std::uint64_t instruction_limit) {
    if (impl_->machine == nullptr || instruction_limit == 0U) {
        return run(instruction_limit);
    }
    const auto initial_pc = impl_->machine->cpu().state().pc;
    const auto target = step_over_target();
    const auto first = step();
    if (!target.has_value()
        || first.reason != StopReason::step_complete
        || first.step.kind != core::StepKind::instruction
        || first.step.pc_before != initial_pc) {
        return first;
    }

    auto result = run_to(
        *target,
        instruction_limit - first.instructions_executed
    );
    result.instructions_executed += first.instructions_executed;
    if (result.reason == StopReason::instruction_limit
        || result.reason == StopReason::sleeping) {
        result.continuation_address = *target;
    }
    return result;
}

StepOutResult Debugger::step_out(
    std::uint64_t instruction_limit,
    StepOutState state
) {
    if (impl_->machine == nullptr) {
        return StepOutResult{step(), {}};
    }
    if (instruction_limit == 0U) {
        return StepOutResult{run(0U), state};
    }

    auto nesting_depth = state.nesting_depth;
    auto initial_step = !state.continued;
    std::uint64_t executed = 0U;
    while (executed < instruction_limit) {
        const auto& cpu_state = impl_->machine->cpu().state();
        if (!initial_step
            && cpu_state.execution_state == core::CpuExecutionState::active
            && cpu_state.knowledge.knows(core::CpuRegister::program_counter)) {
            const auto breakpoint = execution_breakpoint_stop(
                cpu_state.pc,
                executed
            );
            if (breakpoint.has_value()) {
                return StepOutResult{*breakpoint, {}};
            }
        }

        auto stop = step();
        if (stop.reason != StopReason::step_complete) {
            stop.instructions_executed += executed;
            const auto continuation = stop.reason == StopReason::sleeping
                ? StepOutState{!initial_step, nesting_depth}
                : StepOutState{};
            return StepOutResult{stop, continuation};
        }
        executed += stop.instructions_executed;

        if (initial_step) {
            if (stop.step.kind != core::StepKind::instruction) {
                stop.instructions_executed = executed;
                return StepOutResult{stop, {}};
            }
            initial_step = false;
        } else if (stop.step.kind == core::StepKind::interrupt_entry) {
            ++nesting_depth;
            continue;
        }
        if (stop.step.kind != core::StepKind::instruction) {
            continue;
        }

        const auto* instruction = isa::decode_instruction(
            impl_->machine->cpu().profile(),
            stop.step.bytes[0]
        );
        if (instruction == nullptr) {
            continue;
        }
        if (instruction->classification == isa::InstructionClass::call) {
            ++nesting_depth;
            continue;
        }
        if (instruction->classification != isa::InstructionClass::subroutine_return
            && instruction->classification
                != isa::InstructionClass::interrupt_return) {
            continue;
        }
        if (nesting_depth != 0U) {
            --nesting_depth;
            continue;
        }

        stop.reason = StopReason::step_out_complete;
        stop.trigger_address = stop.step.pc_after;
        stop.instructions_executed = executed;
        return StepOutResult{stop, {}};
    }

    const auto pc = impl_->machine->cpu().state().pc;
    core::StepResult step_result;
    step_result.pc_before = pc;
    step_result.pc_after = pc;
    return StepOutResult{
        StopInfo{
            StopReason::instruction_limit,
            step_result,
            pc,
            executed,
            std::nullopt,
            std::nullopt,
        },
        StepOutState{true, nesting_depth},
    };
}

StopInfo Debugger::run(std::uint64_t instruction_limit) {
    return run_until(std::nullopt, instruction_limit);
}

StopInfo Debugger::run_to(
    std::uint16_t address,
    std::uint64_t instruction_limit
) {
    return run_until(address, instruction_limit);
}

StopInfo Debugger::run_until(
    std::optional<std::uint16_t> address,
    std::uint64_t instruction_limit
) {
    if (impl_->machine == nullptr) {
        return StopInfo{
            StopReason::detached,
            {},
            0,
            0,
            std::nullopt,
            std::nullopt,
        };
    }
    std::uint64_t executed = 0U;
    while (true) {
        const auto& state = impl_->machine->cpu().state();
        const auto pc = state.pc;
        if (address.has_value()
            && state.execution_state == core::CpuExecutionState::active
            && state.knowledge.knows(core::CpuRegister::program_counter)
            && pc == *address) {
            core::StepResult step_result;
            step_result.pc_before = pc;
            step_result.pc_after = pc;
            return StopInfo{
                StopReason::address_reached,
                step_result,
                pc,
                executed,
                std::nullopt,
                std::nullopt,
            };
        }
        if (executed >= instruction_limit) {
            break;
        }
        if (state.execution_state == core::CpuExecutionState::active
            && state.knowledge.knows(core::CpuRegister::program_counter)) {
            const auto breakpoint = execution_breakpoint_stop(pc, executed);
            if (breakpoint.has_value()) {
                return *breakpoint;
            }
        }
        const auto stop = step();
        if (stop.reason != StopReason::step_complete) {
            auto result = stop;
            result.instructions_executed = executed
                + stop.instructions_executed;
            return result;
        }
        executed += stop.instructions_executed;
    }
    core::StepResult step_result;
    step_result.pc_before = impl_->machine->cpu().state().pc;
    step_result.pc_after = step_result.pc_before;
    return StopInfo{
        StopReason::instruction_limit,
        step_result,
        step_result.pc_before,
        executed,
        std::nullopt,
        std::nullopt,
    };
}

std::vector<ExecutionHistoryEntry> Debugger::history() const {
    return impl_->history.values();
}

std::vector<core::BusAccessEvent> Debugger::memory_accesses(
    AccessTraceFilter filter
) const {
    if (!is_valid_access_trace_filter(filter)) {
        throw std::invalid_argument{"invalid access trace filter"};
    }
    auto result = impl_->accesses.values();
    const auto mask = static_cast<std::uint8_t>(filter.mask);
    std::erase_if(result, [&](const core::BusAccessEvent& event) {
        return event.address < filter.first_address
            || event.address > filter.last_address
            || (mask & access_trace_bit(event.kind)) == 0U;
    });
    return result;
}

std::size_t Debugger::history_size() const noexcept {
    return impl_->history.size();
}

std::size_t Debugger::memory_access_size(AccessTraceFilter filter) const {
    return memory_accesses(filter).size();
}

void Debugger::clear_history() noexcept {
    impl_->history.clear();
    impl_->accesses.clear();
    impl_->history_sequence = 0U;
}

DebugInfoLoadResult Debugger::load_debug_info(
    const formats::jr8dbg::DebugInfo& debug_info,
    const formats::Sha256Digest& application_integrity
) {
    if (impl_->machine == nullptr) {
        return DebugInfoLoadResult::detached;
    }
    std::optional<formats::jr8dbg::DebugInfo> candidate{debug_info};
    try {
        static_cast<void>(formats::jr8dbg::write(*candidate));
    } catch (const formats::linked::Error&) {
        return DebugInfoLoadResult::invalid_format;
    }
    if (debug_info.target_profile != isa::profile_name(impl_->machine->cpu().profile())) {
        return DebugInfoLoadResult::target_mismatch;
    }
    if (debug_info.application_integrity_sha256 != application_integrity) {
        return DebugInfoLoadResult::integrity_mismatch;
    }
    impl_->symbol_watches.clear();
    impl_->debug_info.swap(candidate);
    return DebugInfoLoadResult::loaded;
}

void Debugger::clear_debug_info() noexcept {
    impl_->debug_info.reset();
    impl_->symbol_watches.clear();
}

const formats::jr8dbg::LineMapping* Debugger::source_at(
    std::uint16_t address
) const noexcept {
    if (!impl_->debug_info.has_value()) {
        return nullptr;
    }
    return formats::jr8dbg::find_line(*impl_->debug_info, address);
}

const formats::jr8dbg::SourceFile* Debugger::source_file(
    std::uint32_t index
) const noexcept {
    if (!impl_->debug_info.has_value()
        || index >= impl_->debug_info->source_files.size()) {
        return nullptr;
    }
    return &impl_->debug_info->source_files[index];
}

std::optional<std::uint16_t> Debugger::source_address(
    std::string_view source_path,
    std::uint32_t line
) const noexcept {
    if (!impl_->debug_info.has_value()) {
        return std::nullopt;
    }
    const auto* mapping = formats::jr8dbg::find_source_line(
        *impl_->debug_info,
        source_path,
        line
    );
    return mapping == nullptr
        ? std::nullopt
        : std::optional<std::uint16_t>{mapping->address};
}

SymbolAddressResult Debugger::symbol_address(
    std::string_view symbol_name
) const noexcept {
    const auto match = find_exact_symbol(impl_->debug_info, symbol_name);
    if (match.ambiguous) {
        return SymbolAddressResult{SymbolAddressStatus::ambiguous, 0U};
    }
    if (match.symbol == nullptr) {
        return {};
    }
    if (match.symbol->kind != formats::jr8dbg::SymbolKind::address) {
        return SymbolAddressResult{SymbolAddressStatus::not_address, 0U};
    }
    return SymbolAddressResult{
        SymbolAddressStatus::found,
        match.symbol->value,
    };
}

std::vector<const formats::jr8dbg::Symbol*> Debugger::symbols_at(
    std::uint16_t address
) const {
    if (!impl_->debug_info.has_value()) {
        return {};
    }
    return formats::jr8dbg::find_symbols(*impl_->debug_info, address);
}

std::optional<DisassembledInstruction> Debugger::disassemble(
    std::uint16_t address
) const {
    if (impl_->machine == nullptr) {
        return std::nullopt;
    }
    DisassembledInstruction result;
    result.address = address;

    const auto read_bytes = [&](std::size_t begin, std::size_t end) {
        for (auto index = begin; index < end; ++index) {
            const auto inspected = impl_->machine->inspect8(
                static_cast<std::uint16_t>(address + index)
            );
            if (!inspected.succeeded()) {
                return false;
            }
            result.bytes[index] = *inspected.value;
        }
        return true;
    };
    if (!read_bytes(0U, 1U)) {
        return std::nullopt;
    }

    auto decoded = disassembler::disassemble_one(
        impl_->machine->cpu().profile(),
        address,
        std::span<const std::uint8_t>{result.bytes}.first(1U)
    );
    if (decoded.status == disassembler::Status::truncated_instruction) {
        if (decoded.instruction == nullptr
            || decoded.instruction->instruction_length > result.bytes.size()) {
            return std::nullopt;
        }
        const auto instruction_length = static_cast<std::size_t>(
            decoded.instruction->instruction_length
        );
        if (!read_bytes(1U, instruction_length)) {
            return std::nullopt;
        }
        decoded = disassembler::disassemble_one(
            impl_->machine->cpu().profile(),
            address,
            std::span<const std::uint8_t>{result.bytes}.first(instruction_length)
        );
    }
    if (decoded.status != disassembler::Status::decoded
        && decoded.status != disassembler::Status::unknown_opcode) {
        return std::nullopt;
    }

    result.length = static_cast<std::uint8_t>(decoded.consumed_bytes);
    result.supported = decoded.status == disassembler::Status::decoded;
    result.text = std::move(decoded.text);
    return result;
}

void Debugger::on_machine_detached(core::Machine& machine) noexcept {
    if (impl_->machine == &machine) {
        impl_->machine = nullptr;
        impl_->debug_info.reset();
    }
}

void Debugger::on_step_begin(const core::CpuState& state) noexcept {
    impl_->step_before = state;
    impl_->first_access_sequence = 0U;
    impl_->current_access_count = 0U;
    impl_->pending_watchpoint.reset();
}

void Debugger::on_step_end(
    const core::StepResult& result,
    const core::CpuState& state
) noexcept {
    ++impl_->history_sequence;
    impl_->history.push(ExecutionHistoryEntry{
        impl_->history_sequence,
        impl_->step_before.cycle_count,
        impl_->first_access_sequence,
        impl_->current_access_count,
        result.pc_before,
        result.pc_after,
        result.kind,
        result.interrupt_source,
        result.bytes,
        result.instruction_length,
        result.bytes_fetched,
        result.cycles,
        result.fault,
        result.bus_fault,
        result.fault_address,
        result.fault_access,
        result.state_fault,
        state,
    });
}

void Debugger::on_bus_access(const core::BusAccessEvent& event) noexcept {
    if (impl_->current_access_count == 0U) {
        impl_->first_access_sequence = event.sequence;
    }
    ++impl_->current_access_count;
    impl_->accesses.push(event);
    const auto mode = watchpoint_mode(event.kind);
    if (!impl_->pending_watchpoint.has_value()
        && mode.has_value()
        && has_memory_watchpoint(event.address, *mode)) {
        impl_->pending_watchpoint = MemoryWatchpointTrigger{
            event.address,
            event.kind,
        };
    }
}

}  // namespace jr800::debugger
