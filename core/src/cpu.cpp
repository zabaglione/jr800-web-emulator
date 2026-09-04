// SPDX-License-Identifier: MIT

#include "jr800/core/cpu.hpp"

#include <bit>
#include <cstdint>

#include "jr800/isa/instruction_metadata.hpp"

namespace jr800::core {
namespace {

constexpr std::uint8_t interrupt_entry_cycles = 12U;
constexpr std::uint8_t wai_interrupt_entry_cycles = 5U;
constexpr std::uint16_t software_interrupt_vector_address = 0xFFFAU;

CpuStatePart first_unknown_interrupt_frame_part(
    const CpuState& state
) noexcept {
    if (!state.knowledge.knows(CpuRegister::program_counter)) {
        return CpuStatePart::program_counter;
    }
    if (!state.knowledge.knows(CpuRegister::stack_pointer)) {
        return CpuStatePart::stack_pointer;
    }
    if (!state.knowledge.knows(CpuRegister::index_register)) {
        return CpuStatePart::index_register;
    }
    if (!state.knowledge.knows(CpuRegister::accumulator_a)) {
        return CpuStatePart::accumulator_a;
    }
    if (!state.knowledge.knows(CpuRegister::accumulator_b)) {
        return CpuStatePart::accumulator_b;
    }
    if (state.knowledge.condition_code != 0xFFU) {
        return CpuStatePart::condition_code;
    }
    return CpuStatePart::none;
}

bool write_interrupt_frame(
    Bus& bus,
    const CpuState& state,
    std::uint16_t return_pc,
    StepResult& result
) {
    const std::array<std::uint8_t, 7U> stack_values{
        static_cast<std::uint8_t>(return_pc & 0x00FFU),
        static_cast<std::uint8_t>(return_pc >> 8U),
        static_cast<std::uint8_t>(state.x & 0x00FFU),
        static_cast<std::uint8_t>(state.x >> 8U),
        state.a,
        state.b,
        state.condition_code,
    };
    for (std::uint16_t offset = 0U; offset < stack_values.size(); ++offset) {
        const auto address = static_cast<std::uint16_t>(state.sp - offset);
        const auto write = bus.write8(address, stack_values[offset]);
        if (!write.succeeded()) {
            result.fault = CpuFault::bus_access;
            result.bus_fault = write.fault;
            result.fault_address = address;
            result.fault_access = AccessKind::data_write;
            return false;
        }
    }
    return true;
}

bool read_interrupt_vector(
    Bus& bus,
    std::uint16_t vector_address,
    std::uint16_t& target,
    StepResult& result
) {
    const auto vector_msb = bus.read8(vector_address, AccessKind::data_read);
    if (!vector_msb.succeeded()) {
        result.fault = CpuFault::bus_access;
        result.bus_fault = vector_msb.fault;
        result.fault_address = vector_address;
        result.fault_access = AccessKind::data_read;
        return false;
    }
    const auto vector_lsb_address = static_cast<std::uint16_t>(
        vector_address + 1U
    );
    const auto vector_lsb = bus.read8(
        vector_lsb_address,
        AccessKind::data_read
    );
    if (!vector_lsb.succeeded()) {
        result.fault = CpuFault::bus_access;
        result.bus_fault = vector_lsb.fault;
        result.fault_address = vector_lsb_address;
        result.fault_access = AccessKind::data_read;
        return false;
    }
    target = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(*vector_msb.value) << 8U)
        | static_cast<std::uint16_t>(*vector_lsb.value)
    );
    return true;
}

std::uint16_t interrupt_vector_address(InterruptSource source) noexcept {
    switch (source) {
    case InterruptSource::timer_input_capture:
        return 0xFFF6U;
    case InterruptSource::timer_output_compare:
        return 0xFFF4U;
    case InterruptSource::timer_overflow:
        return 0xFFF2U;
    case InterruptSource::serial:
        return 0xFFF0U;
    case InterruptSource::none:
        break;
    }
    return 0U;
}

}  // namespace

void Cpu::reset() noexcept {
    state_ = {};
    state_.knowledge.registers = all_cpu_registers;
    state_.knowledge.condition_code = 0xFFU;
}

void Cpu::initialize(
    isa::CpuProfile profile,
    std::uint16_t program_counter,
    std::uint16_t stack_pointer
) noexcept {
    profile_ = profile;
    state_ = {};
    state_.pc = program_counter;
    state_.sp = stack_pointer;
    state_.knowledge.registers = all_cpu_registers;
    state_.knowledge.condition_code = 0xFFU;
}

void Cpu::initialize_known_state(
    isa::CpuProfile profile,
    CpuState state
) noexcept {
    profile_ = profile;
    state.condition_code = static_cast<std::uint8_t>(
        state.condition_code | fixed_condition_code_bits
    );
    state.knowledge.registers = static_cast<std::uint8_t>(
        state.knowledge.registers & all_cpu_registers
    );
    state.knowledge.condition_code = static_cast<std::uint8_t>(
        state.knowledge.condition_code | fixed_condition_code_bits
    );
    state_ = state;
}

StepResult Cpu::service_maskable_interrupt(
    Bus& bus,
    InterruptRequest request
) {
    StepResult result;
    result.pc_before = state_.pc;
    result.pc_after = state_.pc;
    result.interrupt_source = request.source;

    if (!request.known) {
        result.kind = StepKind::interrupt_entry;
        result.fault = CpuFault::unknown_interrupt_request;
        return result;
    }
    if (!request.asserted()) {
        return result;
    }

    result.kind = StepKind::interrupt_entry;
    if (profile_ != isa::CpuProfile::hd6301v1) {
        result.fault = CpuFault::unimplemented_operation;
        return result;
    }

    const auto interrupt_mask = condition_mask(ConditionCode::interrupt_mask);
    if ((state_.knowledge.condition_code & interrupt_mask) == 0U) {
        result.fault = CpuFault::unknown_state;
        result.state_fault = CpuStatePart::condition_code;
        return result;
    }
    if ((state_.condition_code & interrupt_mask) != 0U) {
        if (state_.execution_state == CpuExecutionState::sleeping) {
            state_.execution_state = CpuExecutionState::active;
            result.kind = StepKind::sleep_resume;
            result.step_completed = true;
        }
        return result;
    }

    const auto frame_already_stacked = state_.execution_state
        == CpuExecutionState::waiting_for_interrupt;
    if (!frame_already_stacked) {
        const auto unknown_part = first_unknown_interrupt_frame_part(state_);
        if (unknown_part != CpuStatePart::none) {
            result.fault = CpuFault::unknown_state;
            result.state_fault = unknown_part;
            return result;
        }
        if (!write_interrupt_frame(bus, state_, state_.pc, result)) {
            return result;
        }
    }

    std::uint16_t target{};
    if (!read_interrupt_vector(
            bus,
            interrupt_vector_address(request.source),
            target,
            result
        )) {
        return result;
    }

    if (!frame_already_stacked) {
        state_.sp = static_cast<std::uint16_t>(state_.sp - 7U);
    }
    state_.pc = target;
    state_.condition_code = static_cast<std::uint8_t>(
        state_.condition_code | interrupt_mask
    );
    state_.execution_state = CpuExecutionState::active;
    state_.maskable_interrupt_delay_cycles = 0U;
    const auto cycles = frame_already_stacked
        ? wai_interrupt_entry_cycles
        : interrupt_entry_cycles;
    state_.cycle_count += cycles;
    const auto advance_fault = bus.advance_cycles(cycles);

    result.pc_after = state_.pc;
    result.cycles = cycles;
    result.step_completed = true;
    if (advance_fault != BusFault::none) {
        result.fault = CpuFault::bus_advance;
        result.bus_fault = advance_fault;
    }
    return result;
}

StepResult Cpu::step_instruction(Bus& bus) {
    StepResult result;
    result.pc_before = state_.pc;
    result.pc_after = state_.pc;
    if (state_.execution_state != CpuExecutionState::active) {
        return result;
    }
    const auto interrupt_request = bus.maskable_interrupt_request();
    const auto interrupt_mask = condition_mask(
        ConditionCode::interrupt_mask
    );
    const auto interrupt_mask_known =
        (state_.knowledge.condition_code & interrupt_mask) != 0U;
    const auto interrupt_mask_set =
        (state_.condition_code & interrupt_mask) != 0U;
    if (state_.maskable_interrupt_delay_cycles == 0U) {
        if (!interrupt_request.known) {
            if (!interrupt_mask_known || !interrupt_mask_set) {
                return service_maskable_interrupt(bus, interrupt_request);
            }
        } else if (interrupt_request.asserted()
                   && (!interrupt_mask_known || !interrupt_mask_set)) {
            return service_maskable_interrupt(bus, interrupt_request);
        }
    }
    const auto delay_before = state_.maskable_interrupt_delay_cycles;
    const auto condition_code_before = state_.condition_code;
    const auto condition_code_known_before =
        state_.knowledge.condition_code;
    result.kind = StepKind::instruction;
    const auto fail_bus_access = [&result](
        BusFault fault,
        std::uint16_t address,
        AccessKind access
    ) {
        result.fault = CpuFault::bus_access;
        result.bus_fault = fault;
        result.fault_address = address;
        result.fault_access = access;
        return result;
    };
    const auto fail_unknown_state = [&result](CpuStatePart part) {
        result.fault = CpuFault::unknown_state;
        result.state_fault = part;
        return result;
    };

    if (!state_.knowledge.knows(CpuRegister::program_counter)) {
        return fail_unknown_state(CpuStatePart::program_counter);
    }

    const auto opcode = bus.read8(state_.pc, AccessKind::instruction_fetch);
    if (!opcode.succeeded()) {
        return fail_bus_access(
            opcode.fault,
            state_.pc,
            AccessKind::instruction_fetch
        );
    }
    result.bytes[0] = *opcode.value;
    result.bytes_fetched = 1U;

    const auto* instruction = isa::decode_instruction(profile_, result.bytes[0]);
    if (instruction == nullptr) {
        result.fault = CpuFault::unsupported_opcode;
        result.instruction_length = 1U;
        return result;
    }
    result.instruction_length = instruction->instruction_length;

    switch (instruction->operation) {
    case isa::Operation::nop:
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    case isa::Operation::enter_sleep:
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        state_.execution_state = CpuExecutionState::sleeping;
        break;
    case isa::Operation::wait_for_interrupt: {
        const auto unknown_part = first_unknown_interrupt_frame_part(state_);
        if (unknown_part != CpuStatePart::none) {
            return fail_unknown_state(unknown_part);
        }
        const auto return_pc = static_cast<std::uint16_t>(state_.pc + 1U);
        if (!write_interrupt_frame(bus, state_, return_pc, result)) {
            return result;
        }
        state_.pc = return_pc;
        state_.sp = static_cast<std::uint16_t>(state_.sp - 7U);
        state_.execution_state = CpuExecutionState::waiting_for_interrupt;
        break;
    }
    case isa::Operation::software_interrupt: {
        const auto unknown_part = first_unknown_interrupt_frame_part(state_);
        if (unknown_part != CpuStatePart::none) {
            return fail_unknown_state(unknown_part);
        }
        const auto return_pc = static_cast<std::uint16_t>(state_.pc + 1U);
        if (!write_interrupt_frame(bus, state_, return_pc, result)) {
            return result;
        }
        std::uint16_t target{};
        if (!read_interrupt_vector(
                bus,
                software_interrupt_vector_address,
                target,
                result
            )) {
            return result;
        }
        state_.pc = target;
        state_.sp = static_cast<std::uint16_t>(state_.sp - 7U);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code
            | condition_mask(ConditionCode::interrupt_mask)
        );
        break;
    }
    case isa::Operation::transfer_accumulator_a_to_condition_code:
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        state_.condition_code = static_cast<std::uint8_t>(
            fixed_condition_code_bits | (state_.a & 0x3FU)
        );
        state_.knowledge.condition_code = 0xFFU;
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    case isa::Operation::transfer_condition_code_to_accumulator_a: {
        constexpr std::uint8_t documented_condition_code_bits = 0x3FU;
        if ((state_.knowledge.condition_code & documented_condition_code_bits)
            != documented_condition_code_bits) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }
        state_.a = state_.condition_code;
        state_.knowledge.registers = static_cast<std::uint8_t>(
            state_.knowledge.registers
            | register_mask(CpuRegister::accumulator_a)
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::increment_index_register:
    case isa::Operation::decrement_index_register: {
        if (!state_.knowledge.knows(CpuRegister::index_register)) {
            return fail_unknown_state(CpuStatePart::index_register);
        }
        if (instruction->operation == isa::Operation::increment_index_register) {
            state_.x = static_cast<std::uint16_t>(state_.x + 1U);
        } else {
            state_.x = static_cast<std::uint16_t>(state_.x - 1U);
        }
        const auto zero = condition_mask(ConditionCode::zero);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~zero
        );
        if (state_.x == 0U) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | zero
            );
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | zero
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::increment_stack_pointer:
    case isa::Operation::decrement_stack_pointer:
        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }
        if (instruction->operation == isa::Operation::increment_stack_pointer) {
            state_.sp = static_cast<std::uint16_t>(state_.sp + 1U);
        } else {
            state_.sp = static_cast<std::uint16_t>(state_.sp - 1U);
        }
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    case isa::Operation::transfer_stack_pointer_to_index_register:
        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }
        state_.x = static_cast<std::uint16_t>(state_.sp + 1U);
        state_.knowledge.registers = static_cast<std::uint8_t>(
            state_.knowledge.registers
            | register_mask(CpuRegister::index_register)
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    case isa::Operation::transfer_index_register_to_stack_pointer:
        if (!state_.knowledge.knows(CpuRegister::index_register)) {
            return fail_unknown_state(CpuStatePart::index_register);
        }
        state_.sp = static_cast<std::uint16_t>(state_.x - 1U);
        state_.knowledge.registers = static_cast<std::uint8_t>(
            state_.knowledge.registers
            | register_mask(CpuRegister::stack_pointer)
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    case isa::Operation::clear_overflow: {
        const auto overflow = condition_mask(ConditionCode::overflow);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~overflow
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | overflow
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::set_overflow: {
        const auto overflow = condition_mask(ConditionCode::overflow);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | overflow
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | overflow
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::clear_carry: {
        const auto carry = condition_mask(ConditionCode::carry);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::set_carry: {
        const auto carry = condition_mask(ConditionCode::carry);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | carry
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::clear_interrupt_mask: {
        const auto interrupt_mask = condition_mask(
            ConditionCode::interrupt_mask
        );
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~interrupt_mask
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | interrupt_mask
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::set_interrupt_mask: {
        const auto interrupt_mask = condition_mask(
            ConditionCode::interrupt_mask
        );
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | interrupt_mask
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | interrupt_mask
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::transfer_accumulator_a_to_b:
    case isa::Operation::transfer_accumulator_b_to_a: {
        const auto transfers_a_to_b = instruction->operation
            == isa::Operation::transfer_accumulator_a_to_b;
        const auto source_register = transfers_a_to_b
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto source_part = transfers_a_to_b
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(source_register)) {
            return fail_unknown_state(source_part);
        }
        const auto value = transfers_a_to_b ? state_.a : state_.b;
        if (transfers_a_to_b) {
            state_.b = value;
        } else {
            state_.a = value;
        }
        state_.knowledge.registers = static_cast<std::uint8_t>(
            state_.knowledge.registers
            | register_mask(
                transfers_a_to_b
                    ? CpuRegister::accumulator_b
                    : CpuRegister::accumulator_a
            )
        );
        set_nzv(value);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::exchange_double_accumulator_and_index_register: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        if (!state_.knowledge.knows(CpuRegister::index_register)) {
            return fail_unknown_state(CpuStatePart::index_register);
        }

        const auto old_x = state_.x;
        state_.x = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(state_.a) << 8U)
            | static_cast<std::uint16_t>(state_.b)
        );
        state_.a = static_cast<std::uint8_t>(old_x >> 8U);
        state_.b = static_cast<std::uint8_t>(old_x & 0x00FFU);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::decimal_adjust_accumulator_a: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        const auto half_carry = condition_mask(ConditionCode::half_carry);
        const auto carry = condition_mask(ConditionCode::carry);
        const auto required_flags = static_cast<std::uint8_t>(
            half_carry | carry
        );
        if ((state_.knowledge.condition_code & required_flags)
            != required_flags) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto high = static_cast<std::uint8_t>(state_.a >> 4U);
        const auto low = static_cast<std::uint8_t>(state_.a & 0x0FU);
        const auto old_half_carry = (state_.condition_code & half_carry) != 0U;
        const auto old_carry = (state_.condition_code & carry) != 0U;
        std::uint8_t correction{};
        bool carry_out{};
        if (!old_carry && high <= 0x09U && !old_half_carry
            && low <= 0x09U) {
            correction = 0x00U;
        } else if (!old_carry && high <= 0x08U && !old_half_carry
                   && low >= 0x0AU) {
            correction = 0x06U;
        } else if (!old_carry && high <= 0x09U && old_half_carry
                   && low <= 0x03U) {
            correction = 0x06U;
        } else if (!old_carry && high >= 0x0AU && !old_half_carry
                   && low <= 0x09U) {
            correction = 0x60U;
            carry_out = true;
        } else if (!old_carry && high >= 0x09U && !old_half_carry
                   && low >= 0x0AU) {
            correction = 0x66U;
            carry_out = true;
        } else if (!old_carry && high >= 0x0AU && old_half_carry
                   && low <= 0x03U) {
            correction = 0x66U;
            carry_out = true;
        } else if (old_carry && high <= 0x02U && !old_half_carry
                   && low <= 0x09U) {
            correction = 0x60U;
            carry_out = true;
        } else if (old_carry && high <= 0x02U && !old_half_carry
                   && low >= 0x0AU) {
            correction = 0x66U;
            carry_out = true;
        } else if (old_carry && high <= 0x03U && old_half_carry
                   && low <= 0x03U) {
            correction = 0x66U;
            carry_out = true;
        } else {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        const auto adjusted = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(state_.a)
            + static_cast<std::uint16_t>(correction)
        );
        const auto negative = condition_mask(ConditionCode::negative);
        const auto zero = condition_mask(ConditionCode::zero);
        const auto written = static_cast<std::uint8_t>(
            negative | zero | carry
        );
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~written
        );
        if ((adjusted & 0x80U) != 0U) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | negative
            );
        }
        if (adjusted == 0U) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | zero
            );
        }
        if (carry_out) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry
            );
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | written
        );
        state_.a = adjusted;
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::add_accumulator_b_to_index_register: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        if (!state_.knowledge.knows(CpuRegister::index_register)) {
            return fail_unknown_state(CpuStatePart::index_register);
        }
        state_.x = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(state_.x)
            + static_cast<std::uint32_t>(state_.b)
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::clear_accumulator_a:
    case isa::Operation::clear_accumulator_b: {
        const auto clear_a = instruction->operation
            == isa::Operation::clear_accumulator_a;
        auto& accumulator = clear_a ? state_.a : state_.b;
        const auto target_register = clear_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        accumulator = 0U;
        state_.knowledge.registers = static_cast<std::uint8_t>(
            state_.knowledge.registers
            | register_mask(target_register)
        );
        set_nzv(accumulator);
        const auto carry = condition_mask(ConditionCode::carry);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::negate_accumulator_a:
    case isa::Operation::negate_accumulator_b: {
        const auto negate_a = instruction->operation
            == isa::Operation::negate_accumulator_a;
        const auto target_register = negate_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        if (!state_.knowledge.knows(target_register)) {
            return fail_unknown_state(
                negate_a
                    ? CpuStatePart::accumulator_a
                    : CpuStatePart::accumulator_b
            );
        }
        auto& accumulator = negate_a ? state_.a : state_.b;
        const auto operand = accumulator;
        const auto negated = static_cast<std::uint8_t>(
            0U - static_cast<std::uint16_t>(operand)
        );
        accumulator = negated;
        set_subtraction_flags8(0U, operand, 0U, negated);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::complement_accumulator_a: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        state_.a = static_cast<std::uint8_t>(~state_.a);
        set_nzv(state_.a);
        const auto carry = condition_mask(ConditionCode::carry);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | carry
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::complement_accumulator_b: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        state_.b = static_cast<std::uint8_t>(~state_.b);
        set_nzv(state_.b);
        const auto carry = condition_mask(ConditionCode::carry);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | carry
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::logical_shift_right_double_accumulator: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        const auto operand = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(state_.a) << 8U)
            | static_cast<std::uint16_t>(state_.b)
        );
        const auto shifted = static_cast<std::uint16_t>(operand >> 1U);
        const auto carry = condition_mask(ConditionCode::carry);
        const auto overflow = condition_mask(ConditionCode::overflow);
        const auto carry_set = (operand & 0x0001U) != 0U;
        state_.a = static_cast<std::uint8_t>(shifted >> 8U);
        state_.b = static_cast<std::uint8_t>(shifted & 0x00FFU);
        set_nzv16(shifted);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        if (carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry | overflow
            );
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::arithmetic_shift_left_double_accumulator: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        const auto operand = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(state_.a) << 8U)
            | static_cast<std::uint16_t>(state_.b)
        );
        const auto shifted = static_cast<std::uint16_t>(operand << 1U);
        const auto carry = condition_mask(ConditionCode::carry);
        const auto overflow = condition_mask(ConditionCode::overflow);
        const auto carry_set = (operand & 0x8000U) != 0U;
        const auto negative_set = (shifted & 0x8000U) != 0U;
        state_.a = static_cast<std::uint8_t>(shifted >> 8U);
        state_.b = static_cast<std::uint8_t>(shifted & 0x00FFU);
        set_nzv16(shifted);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        if (carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry
            );
        }
        if (negative_set != carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | overflow
            );
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::logical_shift_right_accumulator_a:
    case isa::Operation::logical_shift_right_accumulator_b: {
        const auto shifts_a = instruction->operation
            == isa::Operation::logical_shift_right_accumulator_a;
        const auto accumulator_register = shifts_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(
                shifts_a
                    ? CpuStatePart::accumulator_a
                    : CpuStatePart::accumulator_b
            );
        }
        auto& accumulator = shifts_a ? state_.a : state_.b;
        const auto operand = accumulator;
        const auto carry = condition_mask(ConditionCode::carry);
        const auto overflow = condition_mask(ConditionCode::overflow);
        const auto carry_set = (operand & 0x01U) != 0U;
        accumulator = static_cast<std::uint8_t>(operand >> 1U);
        set_nzv(accumulator);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        if (carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry
            );
        }
        const auto negative_set = (accumulator & 0x80U) != 0U;
        if (negative_set != carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | overflow
            );
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::rotate_left_accumulator_a:
    case isa::Operation::rotate_left_accumulator_b: {
        const auto rotates_a = instruction->operation
            == isa::Operation::rotate_left_accumulator_a;
        const auto accumulator_register = rotates_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto accumulator_state_part = rotates_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(accumulator_state_part);
        }
        const auto carry = condition_mask(ConditionCode::carry);
        if ((state_.knowledge.condition_code & carry) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto operand = rotates_a ? state_.a : state_.b;
        const auto old_carry_set = (state_.condition_code & carry) != 0U;
        const auto new_carry_set = (operand & 0x80U) != 0U;
        const auto rotated = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(operand << 1U)
            | (old_carry_set ? 0x01U : 0U)
        );
        set_nzv(rotated);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        if (new_carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry
            );
        }
        const auto negative_set = (rotated & 0x80U) != 0U;
        if (negative_set != new_carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code
                | condition_mask(ConditionCode::overflow)
            );
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        if (rotates_a) {
            state_.a = rotated;
        } else {
            state_.b = rotated;
        }
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::rotate_right_accumulator_a:
    case isa::Operation::rotate_right_accumulator_b: {
        const auto rotates_a = instruction->operation
            == isa::Operation::rotate_right_accumulator_a;
        const auto accumulator_register = rotates_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(
                rotates_a
                    ? CpuStatePart::accumulator_a
                    : CpuStatePart::accumulator_b
            );
        }
        const auto carry = condition_mask(ConditionCode::carry);
        if ((state_.knowledge.condition_code & carry) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        auto& accumulator = rotates_a ? state_.a : state_.b;
        const auto operand = accumulator;
        const auto old_carry_set = (state_.condition_code & carry) != 0U;
        const auto new_carry_set = (operand & 0x01U) != 0U;
        accumulator = static_cast<std::uint8_t>(
            (operand >> 1U) | (old_carry_set ? 0x80U : 0U)
        );
        set_nzv(accumulator);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        if (new_carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry
            );
        }
        const auto negative_set = (accumulator & 0x80U) != 0U;
        if (negative_set != new_carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code
                | condition_mask(ConditionCode::overflow)
            );
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::arithmetic_shift_left_accumulator_a:
    case isa::Operation::arithmetic_shift_left_accumulator_b: {
        const auto shifts_a = instruction->operation
            == isa::Operation::arithmetic_shift_left_accumulator_a;
        const auto accumulator_register = shifts_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto accumulator_state_part = shifts_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(accumulator_state_part);
        }
        const auto operand = shifts_a ? state_.a : state_.b;
        const auto carry = condition_mask(ConditionCode::carry);
        const auto overflow = condition_mask(ConditionCode::overflow);
        const auto carry_set = (operand & 0x80U) != 0U;
        const auto shifted = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(operand) << 1U
        );
        set_nzv(shifted);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        if (carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry
            );
        }
        const auto negative_set = (shifted & 0x80U) != 0U;
        if (negative_set != carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | overflow
            );
        }
        if (shifts_a) {
            state_.a = shifted;
        } else {
            state_.b = shifted;
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::arithmetic_shift_right_accumulator_a:
    case isa::Operation::arithmetic_shift_right_accumulator_b: {
        const auto shifts_a = instruction->operation
            == isa::Operation::arithmetic_shift_right_accumulator_a;
        const auto accumulator_register = shifts_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto accumulator_state_part = shifts_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(accumulator_state_part);
        }
        const auto operand = shifts_a ? state_.a : state_.b;
        const auto carry = condition_mask(ConditionCode::carry);
        const auto overflow = condition_mask(ConditionCode::overflow);
        const auto carry_set = (operand & 0x01U) != 0U;
        const auto shifted = static_cast<std::uint8_t>(
            (operand >> 1U) | (operand & 0x80U)
        );
        set_nzv(shifted);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        if (carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry
            );
        }
        const auto negative_set = (shifted & 0x80U) != 0U;
        if (negative_set != carry_set) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | overflow
            );
        }
        if (shifts_a) {
            state_.a = shifted;
        } else {
            state_.b = shifted;
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::decrement_accumulator_a: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        const auto operand = state_.a;
        state_.a = static_cast<std::uint8_t>(state_.a - 1U);
        set_nzv(state_.a);
        if (operand == 0x80U) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code
                | condition_mask(ConditionCode::overflow)
            );
        }
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::decrement_accumulator_b: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        const auto operand = state_.b;
        state_.b = static_cast<std::uint8_t>(state_.b - 1U);
        set_nzv(state_.b);
        if (operand == 0x80U) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code
                | condition_mask(ConditionCode::overflow)
            );
        }
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::increment_accumulator_a:
    case isa::Operation::increment_accumulator_b: {
        const auto selects_a = instruction->operation
            == isa::Operation::increment_accumulator_a;
        const auto selected_register = selects_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto selected_state_part = selects_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(selected_register)) {
            return fail_unknown_state(selected_state_part);
        }
        auto& accumulator = selects_a ? state_.a : state_.b;
        const auto operand = accumulator;
        accumulator = static_cast<std::uint8_t>(accumulator + 1U);
        set_nzv(accumulator);
        if (operand == 0x7FU) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code
                | condition_mask(ConditionCode::overflow)
            );
        }
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::arithmetic_shift_right_memory:
    case isa::Operation::arithmetic_shift_left_memory:
    case isa::Operation::complement_memory:
    case isa::Operation::decrement_memory:
    case isa::Operation::logical_shift_right_memory:
    case isa::Operation::negate_memory:
    case isa::Operation::rotate_left_memory:
    case isa::Operation::rotate_right_memory:
    case isa::Operation::increment_memory: {
        const auto first_operand_address = static_cast<std::uint16_t>(
            state_.pc + 1U
        );
        const auto first_operand = bus.read8(
            first_operand_address,
            AccessKind::instruction_fetch
        );
        if (!first_operand.succeeded()) {
            return fail_bus_access(
                first_operand.fault,
                first_operand_address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *first_operand.value;
        result.bytes_fetched = 2U;

        std::uint16_t effective_address{};
        std::uint16_t pc_increment{};
        switch (instruction->addressing_mode) {
        case isa::AddressingMode::indexed8:
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            effective_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            pc_increment = 2U;
            break;
        case isa::AddressingMode::extended16: {
            const auto second_operand_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto second_operand = bus.read8(
                second_operand_address,
                AccessKind::instruction_fetch
            );
            if (!second_operand.succeeded()) {
                return fail_bus_access(
                    second_operand.fault,
                    second_operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *second_operand.value;
            result.bytes_fetched = 3U;
            effective_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            pc_increment = 3U;
            break;
        }
        default:
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        const auto carry = condition_mask(ConditionCode::carry);
        const auto reads_carry = instruction->operation
                == isa::Operation::rotate_left_memory
            || instruction->operation == isa::Operation::rotate_right_memory;
        if (reads_carry
            && (state_.knowledge.condition_code & carry) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }
        const auto old_carry_set =
            (state_.condition_code & carry) != 0U;
        const auto old_value = bus.read8(
            effective_address,
            AccessKind::data_read
        );
        if (!old_value.succeeded()) {
            return fail_bus_access(
                old_value.fault,
                effective_address,
                AccessKind::data_read
            );
        }
        std::uint8_t value{};
        switch (instruction->operation) {
        case isa::Operation::arithmetic_shift_left_memory:
            value = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(*old_value.value) << 1U
            );
            break;
        case isa::Operation::arithmetic_shift_right_memory:
            value = static_cast<std::uint8_t>(
                (*old_value.value >> 1U) | (*old_value.value & 0x80U)
            );
            break;
        case isa::Operation::complement_memory:
            value = static_cast<std::uint8_t>(~*old_value.value);
            break;
        case isa::Operation::decrement_memory:
            value = static_cast<std::uint8_t>(*old_value.value - 1U);
            break;
        case isa::Operation::logical_shift_right_memory:
            value = static_cast<std::uint8_t>(*old_value.value >> 1U);
            break;
        case isa::Operation::negate_memory:
            value = static_cast<std::uint8_t>(0U - *old_value.value);
            break;
        case isa::Operation::rotate_left_memory:
            value = static_cast<std::uint8_t>(
                (static_cast<std::uint16_t>(*old_value.value) << 1U)
                | (old_carry_set ? 0x01U : 0x00U)
            );
            break;
        case isa::Operation::rotate_right_memory:
            value = static_cast<std::uint8_t>(
                (*old_value.value >> 1U) | (old_carry_set ? 0x80U : 0U)
            );
            break;
        case isa::Operation::increment_memory:
            value = static_cast<std::uint8_t>(*old_value.value + 1U);
            break;
        default:
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        const auto write = bus.write8(effective_address, value);
        if (!write.succeeded()) {
            return fail_bus_access(
                write.fault,
                effective_address,
                AccessKind::data_write
            );
        }

        set_nzv(value);
        switch (instruction->operation) {
        case isa::Operation::arithmetic_shift_left_memory:
        case isa::Operation::rotate_left_memory: {
            const auto overflow = condition_mask(ConditionCode::overflow);
            const auto new_carry_set = (*old_value.value & 0x80U) != 0U;
            const auto negative_set = (value & 0x80U) != 0U;
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code & ~(carry | overflow)
            );
            if (new_carry_set) {
                state_.condition_code = static_cast<std::uint8_t>(
                    state_.condition_code | carry
                );
            }
            if (negative_set != new_carry_set) {
                state_.condition_code = static_cast<std::uint8_t>(
                    state_.condition_code | overflow
                );
            }
            state_.knowledge.condition_code = static_cast<std::uint8_t>(
                state_.knowledge.condition_code | carry
            );
            break;
        }
        case isa::Operation::complement_memory:
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry
            );
            state_.knowledge.condition_code = static_cast<std::uint8_t>(
                state_.knowledge.condition_code | carry
            );
            break;
        case isa::Operation::logical_shift_right_memory: {
            const auto overflow = condition_mask(ConditionCode::overflow);
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code & ~(carry | overflow)
            );
            if ((*old_value.value & 0x01U) != 0U) {
                state_.condition_code = static_cast<std::uint8_t>(
                    state_.condition_code | carry | overflow
                );
            }
            state_.knowledge.condition_code = static_cast<std::uint8_t>(
                state_.knowledge.condition_code | carry
            );
            break;
        }
        case isa::Operation::arithmetic_shift_right_memory:
        case isa::Operation::rotate_right_memory: {
            const auto overflow = condition_mask(ConditionCode::overflow);
            const auto new_carry_set = (*old_value.value & 0x01U) != 0U;
            const auto negative_set = (value & 0x80U) != 0U;
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code & ~(carry | overflow)
            );
            if (new_carry_set) {
                state_.condition_code = static_cast<std::uint8_t>(
                    state_.condition_code | carry
                );
            }
            if (negative_set != new_carry_set) {
                state_.condition_code = static_cast<std::uint8_t>(
                    state_.condition_code | overflow
                );
            }
            state_.knowledge.condition_code = static_cast<std::uint8_t>(
                state_.knowledge.condition_code | carry
            );
            break;
        }
        case isa::Operation::negate_memory:
            if (*old_value.value == 0x80U) {
                state_.condition_code = static_cast<std::uint8_t>(
                    state_.condition_code
                    | condition_mask(ConditionCode::overflow)
                );
            }
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code & ~carry
            );
            if (*old_value.value != 0U) {
                state_.condition_code = static_cast<std::uint8_t>(
                    state_.condition_code | carry
                );
            }
            state_.knowledge.condition_code = static_cast<std::uint8_t>(
                state_.knowledge.condition_code | carry
            );
            break;
        case isa::Operation::decrement_memory:
            if (*old_value.value == 0x80U) {
                state_.condition_code = static_cast<std::uint8_t>(
                    state_.condition_code
                    | condition_mask(ConditionCode::overflow)
                );
            }
            break;
        case isa::Operation::increment_memory:
            if (*old_value.value == 0x7FU) {
                state_.condition_code = static_cast<std::uint8_t>(
                    state_.condition_code
                    | condition_mask(ConditionCode::overflow)
                );
            }
            break;
        default:
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::multiply_unsigned_accumulators: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        const auto product = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(state_.a)
            * static_cast<std::uint16_t>(state_.b)
        );
        state_.a = static_cast<std::uint8_t>(product >> 8U);
        state_.b = static_cast<std::uint8_t>(product & 0x00FFU);
        state_.knowledge.registers = static_cast<std::uint8_t>(
            state_.knowledge.registers
            | register_mask(CpuRegister::accumulator_a)
            | register_mask(CpuRegister::accumulator_b)
        );

        const auto carry = condition_mask(ConditionCode::carry);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        if ((product & 0x0080U) != 0U) {
            state_.condition_code = static_cast<std::uint8_t>(
                state_.condition_code | carry
            );
        }
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::load_accumulator_a:
    case isa::Operation::load_accumulator_b: {
        std::uint8_t value{};
        std::uint16_t pc_increment{};
        switch (instruction->addressing_mode) {
        case isa::AddressingMode::immediate8: {
            const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
            const auto operand = bus.read8(
                address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            value = result.bytes[1];
            pc_increment = 2U;
            break;
        }
        case isa::AddressingMode::direct8:
        case isa::AddressingMode::indexed8: {
            const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
            const auto address_operand = bus.read8(
                address,
                AccessKind::instruction_fetch
            );
            if (!address_operand.succeeded()) {
                return fail_bus_access(
                    address_operand.fault,
                    address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_operand.value;
            result.bytes_fetched = 2U;

            std::uint16_t effective_address = result.bytes[1];
            if (instruction->addressing_mode == isa::AddressingMode::indexed8) {
                if (!state_.knowledge.knows(CpuRegister::index_register)) {
                    return fail_unknown_state(CpuStatePart::index_register);
                }
                effective_address = static_cast<std::uint16_t>(
                    state_.x + result.bytes[1]
                );
            }
            const auto operand = bus.read8(
                effective_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    effective_address,
                    AccessKind::data_read
                );
            }
            value = *operand.value;
            pc_increment = 2U;
            break;
        }
        case isa::AddressingMode::extended16: {
            const auto msb_address = static_cast<std::uint16_t>(state_.pc + 1U);
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(state_.pc + 2U);
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;

            const auto effective_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            const auto operand = bus.read8(
                effective_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    effective_address,
                    AccessKind::data_read
                );
            }
            value = *operand.value;
            pc_increment = 3U;
            break;
        }
        default:
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        if (instruction->operation == isa::Operation::load_accumulator_a) {
            state_.a = value;
            state_.knowledge.registers = static_cast<std::uint8_t>(
                state_.knowledge.registers
                | register_mask(CpuRegister::accumulator_a)
            );
        } else {
            state_.b = value;
            state_.knowledge.registers = static_cast<std::uint8_t>(
                state_.knowledge.registers
                | register_mask(CpuRegister::accumulator_b)
            );
        }
        set_nzv(value);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::load_double_accumulator:
    case isa::Operation::load_index_register:
    case isa::Operation::load_stack_pointer: {
        std::uint16_t value{};
        std::uint16_t pc_increment{};
        switch (instruction->addressing_mode) {
        case isa::AddressingMode::immediate16: {
            const auto msb_address = static_cast<std::uint16_t>(state_.pc + 1U);
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(state_.pc + 2U);
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;
            value = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            pc_increment = 3U;
            break;
        }
        case isa::AddressingMode::direct8:
        case isa::AddressingMode::indexed8:
        case isa::AddressingMode::extended16: {
            const auto addressed_stack_load =
                instruction->operation == isa::Operation::load_stack_pointer;
            const auto addressed_double_load = instruction->operation
                == isa::Operation::load_double_accumulator;
            if (instruction->operation != isa::Operation::load_index_register
                && !addressed_stack_load && !addressed_double_load) {
                result.fault = CpuFault::unimplemented_operation;
                return result;
            }
            const auto address_byte_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_byte = bus.read8(
                address_byte_address,
                AccessKind::instruction_fetch
            );
            if (!address_byte.succeeded()) {
                return fail_bus_access(
                    address_byte.fault,
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_byte.value;
            result.bytes_fetched = 2U;

            std::uint16_t effective_address = result.bytes[1];
            if (instruction->addressing_mode
                == isa::AddressingMode::extended16) {
                const auto low_address_byte_address =
                    static_cast<std::uint16_t>(state_.pc + 2U);
                const auto low_address_byte = bus.read8(
                    low_address_byte_address,
                    AccessKind::instruction_fetch
                );
                if (!low_address_byte.succeeded()) {
                    return fail_bus_access(
                        low_address_byte.fault,
                        low_address_byte_address,
                        AccessKind::instruction_fetch
                    );
                }
                result.bytes[2] = *low_address_byte.value;
                result.bytes_fetched = 3U;
                effective_address = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                    | static_cast<std::uint16_t>(result.bytes[2])
                );
                pc_increment = 3U;
            } else if (instruction->addressing_mode
                == isa::AddressingMode::indexed8) {
                if (!state_.knowledge.knows(CpuRegister::index_register)) {
                    return fail_unknown_state(CpuStatePart::index_register);
                }
                effective_address = static_cast<std::uint16_t>(
                    state_.x + result.bytes[1]
                );
                pc_increment = 2U;
            } else {
                pc_increment = 2U;
            }
            const auto high = bus.read8(
                effective_address,
                AccessKind::data_read
            );
            if (!high.succeeded()) {
                return fail_bus_access(
                    high.fault,
                    effective_address,
                    AccessKind::data_read
                );
            }
            const auto low_address = static_cast<std::uint16_t>(
                effective_address + 1U
            );
            const auto low = bus.read8(low_address, AccessKind::data_read);
            if (!low.succeeded()) {
                return fail_bus_access(
                    low.fault,
                    low_address,
                    AccessKind::data_read
                );
            }
            value = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(*high.value) << 8U)
                | static_cast<std::uint16_t>(*low.value)
            );
            break;
        }
        default:
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        if (instruction->operation
            == isa::Operation::load_double_accumulator) {
            state_.a = static_cast<std::uint8_t>(value >> 8U);
            state_.b = static_cast<std::uint8_t>(value & 0x00FFU);
            state_.knowledge.registers = static_cast<std::uint8_t>(
                state_.knowledge.registers
                | register_mask(CpuRegister::accumulator_a)
                | register_mask(CpuRegister::accumulator_b)
            );
        } else if (instruction->operation
            == isa::Operation::load_index_register) {
            state_.x = value;
            state_.knowledge.registers = static_cast<std::uint8_t>(
                state_.knowledge.registers
                | register_mask(CpuRegister::index_register)
            );
        } else {
            state_.sp = value;
            state_.knowledge.registers = static_cast<std::uint8_t>(
                state_.knowledge.registers
                | register_mask(CpuRegister::stack_pointer)
            );
        }
        set_nzv16(value);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::compare_index_register: {
        std::uint16_t operand{};
        std::uint16_t pc_increment{};
        if (instruction->addressing_mode
            == isa::AddressingMode::immediate16) {
            const auto msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;
            operand = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(*msb.value) << 8U)
                | static_cast<std::uint16_t>(*lsb.value)
            );
            pc_increment = 3U;
        } else {
            std::uint16_t effective_address{};
            if (instruction->addressing_mode
                == isa::AddressingMode::direct8) {
                const auto address_byte_address = static_cast<std::uint16_t>(
                    state_.pc + 1U
                );
                const auto address_byte = bus.read8(
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
                if (!address_byte.succeeded()) {
                    return fail_bus_access(
                        address_byte.fault,
                        address_byte_address,
                        AccessKind::instruction_fetch
                    );
                }
                result.bytes[1] = *address_byte.value;
                result.bytes_fetched = 2U;
                effective_address = *address_byte.value;
                pc_increment = 2U;
            } else if (instruction->addressing_mode
                == isa::AddressingMode::indexed8) {
                const auto displacement_address = static_cast<std::uint16_t>(
                    state_.pc + 1U
                );
                const auto displacement = bus.read8(
                    displacement_address,
                    AccessKind::instruction_fetch
                );
                if (!displacement.succeeded()) {
                    return fail_bus_access(
                        displacement.fault,
                        displacement_address,
                        AccessKind::instruction_fetch
                    );
                }
                result.bytes[1] = *displacement.value;
                result.bytes_fetched = 2U;
                if (!state_.knowledge.knows(CpuRegister::index_register)) {
                    return fail_unknown_state(CpuStatePart::index_register);
                }
                effective_address = static_cast<std::uint16_t>(
                    state_.x + *displacement.value
                );
                pc_increment = 2U;
            } else if (instruction->addressing_mode
                == isa::AddressingMode::extended16) {
                const auto address_msb_address = static_cast<std::uint16_t>(
                    state_.pc + 1U
                );
                const auto address_msb = bus.read8(
                    address_msb_address,
                    AccessKind::instruction_fetch
                );
                if (!address_msb.succeeded()) {
                    return fail_bus_access(
                        address_msb.fault,
                        address_msb_address,
                        AccessKind::instruction_fetch
                    );
                }
                result.bytes[1] = *address_msb.value;
                result.bytes_fetched = 2U;

                const auto address_lsb_address = static_cast<std::uint16_t>(
                    state_.pc + 2U
                );
                const auto address_lsb = bus.read8(
                    address_lsb_address,
                    AccessKind::instruction_fetch
                );
                if (!address_lsb.succeeded()) {
                    return fail_bus_access(
                        address_lsb.fault,
                        address_lsb_address,
                        AccessKind::instruction_fetch
                    );
                }
                result.bytes[2] = *address_lsb.value;
                result.bytes_fetched = 3U;
                effective_address = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(*address_msb.value) << 8U)
                    | static_cast<std::uint16_t>(*address_lsb.value)
                );
                pc_increment = 3U;
            } else {
                result.fault = CpuFault::unimplemented_operation;
                return result;
            }
            const auto high = bus.read8(
                effective_address,
                AccessKind::data_read
            );
            if (!high.succeeded()) {
                return fail_bus_access(
                    high.fault,
                    effective_address,
                    AccessKind::data_read
                );
            }
            const auto low_address = static_cast<std::uint16_t>(
                effective_address + 1U
            );
            const auto low = bus.read8(low_address, AccessKind::data_read);
            if (!low.succeeded()) {
                return fail_bus_access(
                    low.fault,
                    low_address,
                    AccessKind::data_read
                );
            }
            operand = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(*high.value) << 8U)
                | static_cast<std::uint16_t>(*low.value)
            );
        }

        if (!state_.knowledge.knows(CpuRegister::index_register)) {
            return fail_unknown_state(CpuStatePart::index_register);
        }
        const auto comparison = static_cast<std::uint16_t>(state_.x - operand);
        set_subtraction_flags16(state_.x, operand, comparison);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::add_accumulator_b_to_accumulator_a: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        const auto old_a = state_.a;
        const auto sum = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(old_a)
            + static_cast<std::uint16_t>(state_.b)
        );
        state_.a = sum;
        set_addition_flags8(old_a, state_.b, 0U, sum);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::add_to_accumulator_a:
    case isa::Operation::add_with_carry_accumulator_a:
    case isa::Operation::add_to_accumulator_b:
    case isa::Operation::add_with_carry_accumulator_b: {
        std::uint8_t operand_value{};
        std::uint16_t pc_increment = 2U;
        if (instruction->addressing_mode == isa::AddressingMode::immediate8) {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            operand_value = result.bytes[1];
        } else if (
            instruction->addressing_mode == isa::AddressingMode::direct8
        ) {
            const auto address_byte_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_byte = bus.read8(
                address_byte_address,
                AccessKind::instruction_fetch
            );
            if (!address_byte.succeeded()) {
                return fail_bus_access(
                    address_byte.fault,
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_byte.value;
            result.bytes_fetched = 2U;

            const auto operand_address = static_cast<std::uint16_t>(
                result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::indexed8
        ) {
            const auto displacement_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto displacement = bus.read8(
                displacement_address,
                AccessKind::instruction_fetch
            );
            if (!displacement.succeeded()) {
                return fail_bus_access(
                    displacement.fault,
                    displacement_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *displacement.value;
            result.bytes_fetched = 2U;

            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            const auto operand_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::extended16
        ) {
            const auto address_msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_msb = bus.read8(
                address_msb_address,
                AccessKind::instruction_fetch
            );
            if (!address_msb.succeeded()) {
                return fail_bus_access(
                    address_msb.fault,
                    address_msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_msb.value;
            result.bytes_fetched = 2U;

            const auto address_lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto address_lsb = bus.read8(
                address_lsb_address,
                AccessKind::instruction_fetch
            );
            if (!address_lsb.succeeded()) {
                return fail_bus_access(
                    address_lsb.fault,
                    address_lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *address_lsb.value;
            result.bytes_fetched = 3U;
            pc_increment = 3U;

            const auto operand_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        const auto adds_to_a = instruction->operation
                == isa::Operation::add_to_accumulator_a
            || instruction->operation
                == isa::Operation::add_with_carry_accumulator_a;
        const auto uses_carry = instruction->operation
                == isa::Operation::add_with_carry_accumulator_a
            || instruction->operation
                == isa::Operation::add_with_carry_accumulator_b;
        const auto accumulator_register = adds_to_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto accumulator_state_part = adds_to_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(accumulator_state_part);
        }
        const auto carry_mask = condition_mask(ConditionCode::carry);
        if (uses_carry
            && (state_.knowledge.condition_code & carry_mask) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }
        const auto carry_in = static_cast<std::uint8_t>(
            uses_carry && (state_.condition_code & carry_mask) != 0U
                ? 1U
                : 0U
        );
        const auto accumulator = adds_to_a ? state_.a : state_.b;
        const auto sum = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(accumulator)
            + static_cast<std::uint16_t>(operand_value)
            + static_cast<std::uint16_t>(carry_in)
        );
        if (adds_to_a) {
            state_.a = sum;
        } else {
            state_.b = sum;
        }
        set_addition_flags8(accumulator, operand_value, carry_in, sum);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::logical_and_accumulator_a:
    case isa::Operation::logical_and_accumulator_b: {
        std::uint8_t operand_value{};
        std::uint16_t pc_increment = 2U;
        if (instruction->addressing_mode == isa::AddressingMode::immediate8) {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            operand_value = result.bytes[1];
        } else if (
            instruction->addressing_mode == isa::AddressingMode::direct8
        ) {
            const auto address_byte_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_byte = bus.read8(
                address_byte_address,
                AccessKind::instruction_fetch
            );
            if (!address_byte.succeeded()) {
                return fail_bus_access(
                    address_byte.fault,
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_byte.value;
            result.bytes_fetched = 2U;

            const auto operand_address = static_cast<std::uint16_t>(
                result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::indexed8
        ) {
            const auto displacement_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto displacement = bus.read8(
                displacement_address,
                AccessKind::instruction_fetch
            );
            if (!displacement.succeeded()) {
                return fail_bus_access(
                    displacement.fault,
                    displacement_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *displacement.value;
            result.bytes_fetched = 2U;
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }

            const auto operand_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::extended16
        ) {
            const auto msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;

            const auto operand_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
            pc_increment = 3U;
        } else {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        const auto ands_a = instruction->operation
            == isa::Operation::logical_and_accumulator_a;
        const auto accumulator_register = ands_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto accumulator_state_part = ands_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(accumulator_state_part);
        }
        const auto value = static_cast<std::uint8_t>(
            (ands_a ? state_.a : state_.b) & operand_value
        );
        if (ands_a) {
            state_.a = value;
        } else {
            state_.b = value;
        }
        set_nzv(value);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::bit_test_accumulator_a:
    case isa::Operation::bit_test_accumulator_b: {
        std::uint8_t operand_value{};
        std::uint16_t pc_increment{2U};
        if (instruction->addressing_mode == isa::AddressingMode::immediate8) {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            operand_value = result.bytes[1];
        } else if (
            instruction->addressing_mode == isa::AddressingMode::direct8
        ) {
            const auto address_byte_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_byte = bus.read8(
                address_byte_address,
                AccessKind::instruction_fetch
            );
            if (!address_byte.succeeded()) {
                return fail_bus_access(
                    address_byte.fault,
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_byte.value;
            result.bytes_fetched = 2U;

            const auto operand_address = static_cast<std::uint16_t>(
                result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::indexed8
        ) {
            const auto displacement_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto displacement = bus.read8(
                displacement_address,
                AccessKind::instruction_fetch
            );
            if (!displacement.succeeded()) {
                return fail_bus_access(
                    displacement.fault,
                    displacement_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *displacement.value;
            result.bytes_fetched = 2U;

            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            const auto operand_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::extended16
        ) {
            const auto address_msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_msb = bus.read8(
                address_msb_address,
                AccessKind::instruction_fetch
            );
            if (!address_msb.succeeded()) {
                return fail_bus_access(
                    address_msb.fault,
                    address_msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_msb.value;
            result.bytes_fetched = 2U;

            const auto address_lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto address_lsb = bus.read8(
                address_lsb_address,
                AccessKind::instruction_fetch
            );
            if (!address_lsb.succeeded()) {
                return fail_bus_access(
                    address_lsb.fault,
                    address_lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *address_lsb.value;
            result.bytes_fetched = 3U;

            const auto operand_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | result.bytes[2]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
            pc_increment = 3U;
        } else {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        const auto tests_a = instruction->operation
            == isa::Operation::bit_test_accumulator_a;
        const auto accumulator_register = tests_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto accumulator_state_part = tests_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(accumulator_state_part);
        }
        set_nzv(static_cast<std::uint8_t>(
            (tests_a ? state_.a : state_.b) & operand_value
        ));
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::exclusive_or_accumulator_a:
    case isa::Operation::exclusive_or_accumulator_b:
    case isa::Operation::logical_or_accumulator_a:
    case isa::Operation::logical_or_accumulator_b: {
        const auto uses_exclusive_or =
            instruction->operation
                == isa::Operation::exclusive_or_accumulator_a
            || instruction->operation
                == isa::Operation::exclusive_or_accumulator_b;
        const auto targets_a = instruction->operation
                == isa::Operation::exclusive_or_accumulator_a
            || instruction->operation
                == isa::Operation::logical_or_accumulator_a;
        std::uint8_t operand_value{};
        std::uint16_t pc_increment{2U};
        if (
            instruction->addressing_mode == isa::AddressingMode::immediate8
        ) {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            operand_value = result.bytes[1];
        } else if (
            instruction->addressing_mode == isa::AddressingMode::direct8
        ) {
            const auto address_byte_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_byte = bus.read8(
                address_byte_address,
                AccessKind::instruction_fetch
            );
            if (!address_byte.succeeded()) {
                return fail_bus_access(
                    address_byte.fault,
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_byte.value;
            result.bytes_fetched = 2U;

            const auto operand_address = static_cast<std::uint16_t>(
                result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::indexed8
        ) {
            const auto displacement_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto displacement = bus.read8(
                displacement_address,
                AccessKind::instruction_fetch
            );
            if (!displacement.succeeded()) {
                return fail_bus_access(
                    displacement.fault,
                    displacement_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *displacement.value;
            result.bytes_fetched = 2U;
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }

            const auto operand_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::extended16
        ) {
            const auto msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;

            const auto operand_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
            pc_increment = 3U;
        } else {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        const auto accumulator_register = targets_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto accumulator_state_part = targets_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(accumulator_state_part);
        }
        const auto accumulator = targets_a ? state_.a : state_.b;
        const auto value = uses_exclusive_or
            ? static_cast<std::uint8_t>(accumulator ^ operand_value)
            : static_cast<std::uint8_t>(accumulator | operand_value);
        if (targets_a) {
            state_.a = value;
        } else {
            state_.b = value;
        }
        set_nzv(value);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::compare_accumulators: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        const auto comparison = static_cast<std::uint8_t>(
            state_.a - state_.b
        );
        set_subtraction_flags8(state_.a, state_.b, 0U, comparison);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::subtract_accumulator_b_from_accumulator_a: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        const auto old_a = state_.a;
        const auto subtraction = static_cast<std::uint8_t>(
            old_a - state_.b
        );
        state_.a = subtraction;
        set_subtraction_flags8(old_a, state_.b, 0U, subtraction);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::compare_accumulator_a:
    case isa::Operation::compare_accumulator_b: {
        std::uint8_t operand_value{};
        std::uint16_t pc_increment{2U};
        if (instruction->addressing_mode == isa::AddressingMode::immediate8) {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            operand_value = result.bytes[1];
        } else if (
            instruction->addressing_mode == isa::AddressingMode::direct8
        ) {
            const auto address_byte_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_byte = bus.read8(
                address_byte_address,
                AccessKind::instruction_fetch
            );
            if (!address_byte.succeeded()) {
                return fail_bus_access(
                    address_byte.fault,
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_byte.value;
            result.bytes_fetched = 2U;

            const auto operand_address = static_cast<std::uint16_t>(
                result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::indexed8
        ) {
            const auto displacement_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto displacement = bus.read8(
                displacement_address,
                AccessKind::instruction_fetch
            );
            if (!displacement.succeeded()) {
                return fail_bus_access(
                    displacement.fault,
                    displacement_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *displacement.value;
            result.bytes_fetched = 2U;
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }

            const auto operand_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::extended16
        ) {
            const auto msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;

            const auto operand_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
            pc_increment = 3U;
        } else {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        const auto compares_a = instruction->operation
            == isa::Operation::compare_accumulator_a;
        const auto accumulator_register = compares_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto accumulator_state_part = compares_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(accumulator_state_part);
        }
        const auto accumulator = compares_a ? state_.a : state_.b;
        const auto comparison = static_cast<std::uint8_t>(
            accumulator - operand_value
        );
        set_subtraction_flags8(
            accumulator,
            operand_value,
            0U,
            comparison
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::subtract_from_accumulator_a:
    case isa::Operation::subtract_from_accumulator_b: {
        std::uint8_t operand_value{};
        std::uint16_t pc_increment{2U};
        if (instruction->addressing_mode == isa::AddressingMode::immediate8) {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            operand_value = result.bytes[1];
        } else if (
            instruction->addressing_mode == isa::AddressingMode::direct8
        ) {
            const auto address_byte_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_byte = bus.read8(
                address_byte_address,
                AccessKind::instruction_fetch
            );
            if (!address_byte.succeeded()) {
                return fail_bus_access(
                    address_byte.fault,
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_byte.value;
            result.bytes_fetched = 2U;

            const auto operand_address = static_cast<std::uint16_t>(
                result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::indexed8
        ) {
            const auto displacement_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto displacement = bus.read8(
                displacement_address,
                AccessKind::instruction_fetch
            );
            if (!displacement.succeeded()) {
                return fail_bus_access(
                    displacement.fault,
                    displacement_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *displacement.value;
            result.bytes_fetched = 2U;
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }

            const auto operand_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (
            instruction->addressing_mode == isa::AddressingMode::extended16
        ) {
            const auto msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;

            const auto operand_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
            pc_increment = 3U;
        } else {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        const auto subtracts_a = instruction->operation
            == isa::Operation::subtract_from_accumulator_a;
        const auto accumulator_register = subtracts_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        const auto accumulator_state_part = subtracts_a
            ? CpuStatePart::accumulator_a
            : CpuStatePart::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(accumulator_state_part);
        }
        const auto accumulator = subtracts_a ? state_.a : state_.b;
        const auto subtraction = static_cast<std::uint8_t>(
            accumulator - operand_value
        );
        set_subtraction_flags8(
            accumulator,
            operand_value,
            0U,
            subtraction
        );
        if (subtracts_a) {
            state_.a = subtraction;
        } else {
            state_.b = subtraction;
        }
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::subtract_with_carry_accumulator_a:
    case isa::Operation::subtract_with_carry_accumulator_b: {
        const auto subtracts_a = instruction->operation
            == isa::Operation::subtract_with_carry_accumulator_a;
        std::uint8_t operand_value{};
        std::uint16_t pc_increment{2U};
        if (instruction->addressing_mode == isa::AddressingMode::immediate8) {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            operand_value = *operand.value;
        } else if (instruction->addressing_mode
                   == isa::AddressingMode::direct8) {
            const auto address_byte_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_byte = bus.read8(
                address_byte_address,
                AccessKind::instruction_fetch
            );
            if (!address_byte.succeeded()) {
                return fail_bus_access(
                    address_byte.fault,
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_byte.value;
            result.bytes_fetched = 2U;

            const auto operand_address = static_cast<std::uint16_t>(
                *address_byte.value
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (instruction->addressing_mode
                   == isa::AddressingMode::indexed8) {
            const auto displacement_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto displacement = bus.read8(
                displacement_address,
                AccessKind::instruction_fetch
            );
            if (!displacement.succeeded()) {
                return fail_bus_access(
                    displacement.fault,
                    displacement_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *displacement.value;
            result.bytes_fetched = 2U;

            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            const auto operand_address = static_cast<std::uint16_t>(
                state_.x + static_cast<std::uint16_t>(*displacement.value)
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
        } else if (instruction->addressing_mode
                   == isa::AddressingMode::extended16) {
            const auto msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;

            const auto operand_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::data_read
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::data_read
                );
            }
            operand_value = *operand.value;
            pc_increment = 3U;
        } else {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        const auto accumulator_register = subtracts_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        if (!state_.knowledge.knows(accumulator_register)) {
            return fail_unknown_state(
                subtracts_a
                    ? CpuStatePart::accumulator_a
                    : CpuStatePart::accumulator_b
            );
        }
        const auto carry = condition_mask(ConditionCode::carry);
        if ((state_.knowledge.condition_code & carry) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }
        const auto borrow = static_cast<std::uint8_t>(
            (state_.condition_code & carry) != 0U ? 1U : 0U
        );
        const auto accumulator = subtracts_a ? state_.a : state_.b;
        const auto subtraction = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(accumulator)
            - static_cast<std::uint16_t>(operand_value)
            - static_cast<std::uint16_t>(borrow)
        );
        set_subtraction_flags8(
            accumulator,
            operand_value,
            borrow,
            subtraction
        );
        if (subtracts_a) {
            state_.a = subtraction;
        } else {
            state_.b = subtraction;
        }
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::add_to_double_accumulator: {
        const auto immediate = instruction->addressing_mode
            == isa::AddressingMode::immediate16;
        const auto direct = instruction->addressing_mode
            == isa::AddressingMode::direct8;
        const auto indexed = instruction->addressing_mode
            == isa::AddressingMode::indexed8;
        const auto extended = instruction->addressing_mode
            == isa::AddressingMode::extended16;
        if (!immediate && !direct && !indexed && !extended) {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        std::uint16_t right{};
        std::uint16_t pc_increment{};
        if (immediate) {
            const auto high_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto high = bus.read8(
                high_address,
                AccessKind::instruction_fetch
            );
            if (!high.succeeded()) {
                return fail_bus_access(
                    high.fault,
                    high_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *high.value;
            result.bytes_fetched = 2U;

            const auto low_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto low = bus.read8(
                low_address,
                AccessKind::instruction_fetch
            );
            if (!low.succeeded()) {
                return fail_bus_access(
                    low.fault,
                    low_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *low.value;
            result.bytes_fetched = 3U;
            right = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            pc_increment = 3U;
        } else {
            const auto address_high_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_high = bus.read8(
                address_high_address,
                AccessKind::instruction_fetch
            );
            if (!address_high.succeeded()) {
                return fail_bus_access(
                    address_high.fault,
                    address_high_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_high.value;
            result.bytes_fetched = 2U;

            std::uint16_t effective_address{};
            if (extended) {
                const auto address_low_address = static_cast<std::uint16_t>(
                    state_.pc + 2U
                );
                const auto address_low = bus.read8(
                    address_low_address,
                    AccessKind::instruction_fetch
                );
                if (!address_low.succeeded()) {
                    return fail_bus_access(
                        address_low.fault,
                        address_low_address,
                        AccessKind::instruction_fetch
                    );
                }
                result.bytes[2] = *address_low.value;
                result.bytes_fetched = 3U;
                effective_address = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                    | static_cast<std::uint16_t>(result.bytes[2])
                );
                pc_increment = 3U;
            } else if (indexed) {
                if (!state_.knowledge.knows(CpuRegister::index_register)) {
                    return fail_unknown_state(CpuStatePart::index_register);
                }
                effective_address = static_cast<std::uint16_t>(
                    state_.x + result.bytes[1]
                );
                pc_increment = 2U;
            } else {
                effective_address = result.bytes[1];
                pc_increment = 2U;
            }
            const auto operand_high = bus.read8(
                effective_address,
                AccessKind::data_read
            );
            if (!operand_high.succeeded()) {
                return fail_bus_access(
                    operand_high.fault,
                    effective_address,
                    AccessKind::data_read
                );
            }
            const auto operand_low_address = static_cast<std::uint16_t>(
                effective_address + 1U
            );
            const auto operand_low = bus.read8(
                operand_low_address,
                AccessKind::data_read
            );
            if (!operand_low.succeeded()) {
                return fail_bus_access(
                    operand_low.fault,
                    operand_low_address,
                    AccessKind::data_read
                );
            }
            right = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(*operand_high.value) << 8U)
                | static_cast<std::uint16_t>(*operand_low.value)
            );
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }

        const auto left = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(state_.a) << 8U)
            | static_cast<std::uint16_t>(state_.b)
        );
        const auto sum = static_cast<std::uint16_t>(left + right);
        state_.a = static_cast<std::uint8_t>(sum >> 8U);
        state_.b = static_cast<std::uint8_t>(sum & 0x00FFU);
        state_.knowledge.registers = static_cast<std::uint8_t>(
            state_.knowledge.registers
            | register_mask(CpuRegister::accumulator_a)
            | register_mask(CpuRegister::accumulator_b)
        );
        set_addition_flags16(left, right, sum);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::subtract_from_double_accumulator: {
        const auto immediate = instruction->addressing_mode
            == isa::AddressingMode::immediate16;
        const auto direct = instruction->addressing_mode
            == isa::AddressingMode::direct8;
        const auto indexed = instruction->addressing_mode
            == isa::AddressingMode::indexed8;
        const auto extended = instruction->addressing_mode
            == isa::AddressingMode::extended16;
        if (!immediate && !direct && !indexed && !extended) {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        std::uint16_t subtrahend{};
        std::uint16_t pc_increment{};
        if (immediate) {
            const auto high_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto high = bus.read8(
                high_address,
                AccessKind::instruction_fetch
            );
            if (!high.succeeded()) {
                return fail_bus_access(
                    high.fault,
                    high_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *high.value;
            result.bytes_fetched = 2U;

            const auto low_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto low = bus.read8(
                low_address,
                AccessKind::instruction_fetch
            );
            if (!low.succeeded()) {
                return fail_bus_access(
                    low.fault,
                    low_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *low.value;
            result.bytes_fetched = 3U;
            subtrahend = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            pc_increment = 3U;
        } else {
            const auto address_byte_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto address_byte = bus.read8(
                address_byte_address,
                AccessKind::instruction_fetch
            );
            if (!address_byte.succeeded()) {
                return fail_bus_access(
                    address_byte.fault,
                    address_byte_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *address_byte.value;
            result.bytes_fetched = 2U;

            std::uint16_t effective_address{};
            if (extended) {
                const auto low_address = static_cast<std::uint16_t>(
                    state_.pc + 2U
                );
                const auto low = bus.read8(
                    low_address,
                    AccessKind::instruction_fetch
                );
                if (!low.succeeded()) {
                    return fail_bus_access(
                        low.fault,
                        low_address,
                        AccessKind::instruction_fetch
                    );
                }
                result.bytes[2] = *low.value;
                result.bytes_fetched = 3U;
                effective_address = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                    | static_cast<std::uint16_t>(result.bytes[2])
                );
                pc_increment = 3U;
            } else if (indexed) {
                if (!state_.knowledge.knows(CpuRegister::index_register)) {
                    return fail_unknown_state(CpuStatePart::index_register);
                }
                effective_address = static_cast<std::uint16_t>(
                    state_.x + result.bytes[1]
                );
                pc_increment = 2U;
            } else {
                effective_address = result.bytes[1];
                pc_increment = 2U;
            }
            const auto high = bus.read8(
                effective_address,
                AccessKind::data_read
            );
            if (!high.succeeded()) {
                return fail_bus_access(
                    high.fault,
                    effective_address,
                    AccessKind::data_read
                );
            }
            const auto low_address = static_cast<std::uint16_t>(
                effective_address + 1U
            );
            const auto low = bus.read8(low_address, AccessKind::data_read);
            if (!low.succeeded()) {
                return fail_bus_access(
                    low.fault,
                    low_address,
                    AccessKind::data_read
                );
            }
            subtrahend = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(*high.value) << 8U)
                | static_cast<std::uint16_t>(*low.value)
            );
        }

        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        const auto minuend = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(state_.a) << 8U)
            | static_cast<std::uint16_t>(state_.b)
        );
        const auto difference = static_cast<std::uint16_t>(
            minuend - subtrahend
        );
        state_.a = static_cast<std::uint8_t>(difference >> 8U);
        state_.b = static_cast<std::uint8_t>(difference & 0x00FFU);
        state_.knowledge.registers = static_cast<std::uint8_t>(
            state_.knowledge.registers
            | register_mask(CpuRegister::accumulator_a)
            | register_mask(CpuRegister::accumulator_b)
        );
        set_subtraction_flags16(minuend, subtrahend, difference);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::store_accumulator_a:
    case isa::Operation::store_accumulator_b: {
        std::uint16_t destination{};
        std::uint16_t pc_increment{};
        switch (instruction->addressing_mode) {
        case isa::AddressingMode::direct8: {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            destination = result.bytes[1];
            pc_increment = 2U;
            break;
        }
        case isa::AddressingMode::indexed8: {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            destination = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            pc_increment = 2U;
            break;
        }
        case isa::AddressingMode::extended16: {
            const auto msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;
            destination = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            pc_increment = 3U;
            break;
        }
        default:
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        const auto stores_b = instruction->operation
            == isa::Operation::store_accumulator_b;
        const auto source_register = stores_b
            ? CpuRegister::accumulator_b
            : CpuRegister::accumulator_a;
        const auto source_part = stores_b
            ? CpuStatePart::accumulator_b
            : CpuStatePart::accumulator_a;
        if (!state_.knowledge.knows(source_register)) {
            return fail_unknown_state(source_part);
        }
        const auto value = stores_b ? state_.b : state_.a;
        const auto write = bus.write8(destination, value);
        if (!write.succeeded()) {
            return fail_bus_access(
                write.fault,
                destination,
                AccessKind::data_write
            );
        }
        set_nzv(value);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::store_double_accumulator:
    case isa::Operation::store_index_register:
    case isa::Operation::store_stack_pointer: {
        const auto stores_double = instruction->operation
            == isa::Operation::store_double_accumulator;
        const auto stores_index = instruction->operation
            == isa::Operation::store_index_register;
        const auto direct = instruction->addressing_mode
            == isa::AddressingMode::direct8;
        const auto indexed = instruction->addressing_mode
            == isa::AddressingMode::indexed8;
        const auto extended = instruction->addressing_mode
            == isa::AddressingMode::extended16;
        if (!direct && !indexed && !extended) {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        const auto first_operand_address = static_cast<std::uint16_t>(
            state_.pc + 1U
        );
        const auto first_operand = bus.read8(
            first_operand_address,
            AccessKind::instruction_fetch
        );
        if (!first_operand.succeeded()) {
            return fail_bus_access(
                first_operand.fault,
                first_operand_address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *first_operand.value;
        result.bytes_fetched = 2U;

        std::uint16_t destination = result.bytes[1];
        std::uint16_t pc_increment = 2U;
        if (indexed) {
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            destination = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
        } else if (extended) {
            const auto second_operand_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto second_operand = bus.read8(
                second_operand_address,
                AccessKind::instruction_fetch
            );
            if (!second_operand.succeeded()) {
                return fail_bus_access(
                    second_operand.fault,
                    second_operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *second_operand.value;
            result.bytes_fetched = 3U;
            destination = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            pc_increment = 3U;
        }

        std::uint16_t value{};
        if (stores_double) {
            if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
                return fail_unknown_state(CpuStatePart::accumulator_a);
            }
            if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
                return fail_unknown_state(CpuStatePart::accumulator_b);
            }
            value = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(state_.a) << 8U)
                | static_cast<std::uint16_t>(state_.b)
            );
        } else if (stores_index) {
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            value = state_.x;
        } else {
            if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
                return fail_unknown_state(CpuStatePart::stack_pointer);
            }
            value = state_.sp;
        }

        const auto high = static_cast<std::uint8_t>(value >> 8U);
        const auto low = static_cast<std::uint8_t>(value & 0x00FFU);
        const auto high_write = bus.write8(destination, high);
        if (!high_write.succeeded()) {
            return fail_bus_access(
                high_write.fault,
                destination,
                AccessKind::data_write
            );
        }
        const auto low_destination = static_cast<std::uint16_t>(
            destination + 1U
        );
        const auto low_write = bus.write8(low_destination, low);
        if (!low_write.succeeded()) {
            return fail_bus_access(
                low_write.fault,
                low_destination,
                AccessKind::data_write
            );
        }

        set_nzv16(value);
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::clear_memory: {
        const auto first_operand_address = static_cast<std::uint16_t>(
            state_.pc + 1U
        );
        const auto first_operand = bus.read8(
            first_operand_address,
            AccessKind::instruction_fetch
        );
        if (!first_operand.succeeded()) {
            return fail_bus_access(
                first_operand.fault,
                first_operand_address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *first_operand.value;
        result.bytes_fetched = 2U;

        std::uint16_t effective_address{};
        std::uint16_t pc_increment{};
        switch (instruction->addressing_mode) {
        case isa::AddressingMode::indexed8:
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            effective_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            pc_increment = 2U;
            break;
        case isa::AddressingMode::extended16: {
            const auto second_operand_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto second_operand = bus.read8(
                second_operand_address,
                AccessKind::instruction_fetch
            );
            if (!second_operand.succeeded()) {
                return fail_bus_access(
                    second_operand.fault,
                    second_operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *second_operand.value;
            result.bytes_fetched = 3U;
            effective_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            pc_increment = 3U;
            break;
        }
        default:
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        const auto destination_read = bus.read8_discard(effective_address);
        if (!destination_read.succeeded()) {
            return fail_bus_access(
                destination_read.fault,
                effective_address,
                AccessKind::data_read
            );
        }
        const auto write = bus.write8(effective_address, 0U);
        if (!write.succeeded()) {
            return fail_bus_access(
                write.fault,
                effective_address,
                AccessKind::data_write
            );
        }

        set_nzv(0U);
        const auto carry = condition_mask(ConditionCode::carry);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + pc_increment);
        break;
    }
    case isa::Operation::and_immediate_with_memory:
    case isa::Operation::exclusive_or_immediate_with_memory:
    case isa::Operation::or_immediate_with_memory: {
        if (instruction->addressing_mode
                != isa::AddressingMode::immediate8_direct8
            && instruction->addressing_mode
                != isa::AddressingMode::immediate8_indexed8) {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        const auto immediate_address = static_cast<std::uint16_t>(
            state_.pc + 1U
        );
        const auto immediate = bus.read8(
            immediate_address,
            AccessKind::instruction_fetch
        );
        if (!immediate.succeeded()) {
            return fail_bus_access(
                immediate.fault,
                immediate_address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *immediate.value;
        result.bytes_fetched = 2U;

        const auto address_byte = static_cast<std::uint16_t>(state_.pc + 2U);
        const auto direct = bus.read8(
            address_byte,
            AccessKind::instruction_fetch
        );
        if (!direct.succeeded()) {
            return fail_bus_access(
                direct.fault,
                address_byte,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[2] = *direct.value;
        result.bytes_fetched = 3U;

        std::uint16_t effective_address = result.bytes[2];
        if (instruction->addressing_mode
            == isa::AddressingMode::immediate8_indexed8) {
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            effective_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[2]
            );
        }
        const auto old_value = bus.read8(
            effective_address,
            AccessKind::data_read
        );
        if (!old_value.succeeded()) {
            return fail_bus_access(
                old_value.fault,
                effective_address,
                AccessKind::data_read
            );
        }
        std::uint8_t value{};
        if (instruction->operation
            == isa::Operation::and_immediate_with_memory) {
            value = static_cast<std::uint8_t>(
                *old_value.value & result.bytes[1]
            );
        } else if (instruction->operation
                   == isa::Operation::exclusive_or_immediate_with_memory) {
            value = static_cast<std::uint8_t>(
                *old_value.value ^ result.bytes[1]
            );
        } else {
            value = static_cast<std::uint8_t>(
                *old_value.value | result.bytes[1]
            );
        }
        const auto write = bus.write8(effective_address, value);
        if (!write.succeeded()) {
            return fail_bus_access(
                write.fault,
                effective_address,
                AccessKind::data_write
            );
        }

        set_nzv(value);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 3U);
        break;
    }
    case isa::Operation::test_immediate_with_memory: {
        if (instruction->addressing_mode
                != isa::AddressingMode::immediate8_direct8
            && instruction->addressing_mode
                != isa::AddressingMode::immediate8_indexed8) {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        const auto immediate_address = static_cast<std::uint16_t>(
            state_.pc + 1U
        );
        const auto immediate = bus.read8(
            immediate_address,
            AccessKind::instruction_fetch
        );
        if (!immediate.succeeded()) {
            return fail_bus_access(
                immediate.fault,
                immediate_address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *immediate.value;
        result.bytes_fetched = 2U;

        const auto address_byte = static_cast<std::uint16_t>(
            state_.pc + 2U
        );
        const auto address = bus.read8(
            address_byte,
            AccessKind::instruction_fetch
        );
        if (!address.succeeded()) {
            return fail_bus_access(
                address.fault,
                address_byte,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[2] = *address.value;
        result.bytes_fetched = 3U;

        std::uint16_t effective_address = result.bytes[2];
        if (instruction->addressing_mode
            == isa::AddressingMode::immediate8_indexed8) {
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            effective_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[2]
            );
        }
        const auto value = bus.read8(effective_address, AccessKind::data_read);
        if (!value.succeeded()) {
            return fail_bus_access(
                value.fault,
                effective_address,
                AccessKind::data_read
            );
        }

        set_nzv(static_cast<std::uint8_t>(
            *value.value & result.bytes[1]
        ));
        state_.pc = static_cast<std::uint16_t>(state_.pc + 3U);
        break;
    }
    case isa::Operation::test_accumulator_a:
    case isa::Operation::test_accumulator_b: {
        if (instruction->addressing_mode != isa::AddressingMode::implied) {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        const auto tests_a = instruction->operation
            == isa::Operation::test_accumulator_a;
        const auto required_register = tests_a
            ? CpuRegister::accumulator_a
            : CpuRegister::accumulator_b;
        if (!state_.knowledge.knows(required_register)) {
            return fail_unknown_state(
                tests_a
                    ? CpuStatePart::accumulator_a
                    : CpuStatePart::accumulator_b
            );
        }

        set_nzv(tests_a ? state_.a : state_.b);
        const auto carry = condition_mask(ConditionCode::carry);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::test_memory: {
        if (instruction->addressing_mode != isa::AddressingMode::indexed8
            && instruction->addressing_mode
                != isa::AddressingMode::extended16) {
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        const auto operand_address = static_cast<std::uint16_t>(
            state_.pc + 1U
        );
        const auto operand = bus.read8(
            operand_address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                operand_address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;

        std::uint16_t effective_address{};
        std::uint16_t instruction_length{};
        if (instruction->addressing_mode == isa::AddressingMode::indexed8) {
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            effective_address = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            instruction_length = 2U;
        } else {
            const auto lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;
            effective_address = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            instruction_length = 3U;
        }
        const auto value = bus.read8(effective_address, AccessKind::data_read);
        if (!value.succeeded()) {
            return fail_bus_access(
                value.fault,
                effective_address,
                AccessKind::data_read
            );
        }

        set_nzv(*value.value);
        const auto carry = condition_mask(ConditionCode::carry);
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code & ~carry
        );
        state_.knowledge.condition_code = static_cast<std::uint8_t>(
            state_.knowledge.condition_code | carry
        );
        state_.pc = static_cast<std::uint16_t>(
            state_.pc + instruction_length
        );
        break;
    }
    case isa::Operation::branch_always: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;
        const auto displacement = std::bit_cast<std::int8_t>(result.bytes[1]);
        const auto next = static_cast<std::uint16_t>(state_.pc + 2U);
        state_.pc = static_cast<std::uint16_t>(
            static_cast<std::int32_t>(next) + displacement
        );
        break;
    }
    case isa::Operation::branch_never: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;
        state_.pc = static_cast<std::uint16_t>(state_.pc + 2U);
        break;
    }
    case isa::Operation::branch_if_higher:
    case isa::Operation::branch_if_lower_or_same: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;

        const auto condition = static_cast<std::uint8_t>(
            condition_mask(ConditionCode::carry)
            | condition_mask(ConditionCode::zero)
        );
        if ((state_.knowledge.condition_code & condition) != condition) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto next = static_cast<std::uint16_t>(state_.pc + 2U);
        const auto condition_set = (state_.condition_code & condition) != 0U;
        const auto branches_if_condition_set = instruction->operation
            == isa::Operation::branch_if_lower_or_same;
        if (condition_set == branches_if_condition_set) {
            const auto displacement = std::bit_cast<std::int8_t>(
                result.bytes[1]
            );
            state_.pc = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(next) + displacement
            );
        } else {
            state_.pc = next;
        }
        break;
    }
    case isa::Operation::branch_if_carry_clear: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;

        const auto carry = condition_mask(ConditionCode::carry);
        if ((state_.knowledge.condition_code & carry) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto next = static_cast<std::uint16_t>(state_.pc + 2U);
        if ((state_.condition_code & carry) == 0U) {
            const auto displacement = std::bit_cast<std::int8_t>(
                result.bytes[1]
            );
            state_.pc = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(next) + displacement
            );
        } else {
            state_.pc = next;
        }
        break;
    }
    case isa::Operation::branch_if_carry_set: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;

        const auto carry = condition_mask(ConditionCode::carry);
        if ((state_.knowledge.condition_code & carry) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto next = static_cast<std::uint16_t>(state_.pc + 2U);
        if ((state_.condition_code & carry) != 0U) {
            const auto displacement = std::bit_cast<std::int8_t>(
                result.bytes[1]
            );
            state_.pc = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(next) + displacement
            );
        } else {
            state_.pc = next;
        }
        break;
    }
    case isa::Operation::branch_if_not_equal: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;

        const auto zero = condition_mask(ConditionCode::zero);
        if ((state_.knowledge.condition_code & zero) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto next = static_cast<std::uint16_t>(state_.pc + 2U);
        if ((state_.condition_code & zero) == 0U) {
            const auto displacement = std::bit_cast<std::int8_t>(
                result.bytes[1]
            );
            state_.pc = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(next) + displacement
            );
        } else {
            state_.pc = next;
        }
        break;
    }
    case isa::Operation::branch_if_equal: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;

        const auto zero = condition_mask(ConditionCode::zero);
        if ((state_.knowledge.condition_code & zero) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto next = static_cast<std::uint16_t>(state_.pc + 2U);
        if ((state_.condition_code & zero) != 0U) {
            const auto displacement = std::bit_cast<std::int8_t>(
                result.bytes[1]
            );
            state_.pc = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(next) + displacement
            );
        } else {
            state_.pc = next;
        }
        break;
    }
    case isa::Operation::branch_if_overflow_clear:
    case isa::Operation::branch_if_overflow_set: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;

        const auto overflow = condition_mask(ConditionCode::overflow);
        if ((state_.knowledge.condition_code & overflow) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto next = static_cast<std::uint16_t>(state_.pc + 2U);
        const auto overflow_set = (state_.condition_code & overflow) != 0U;
        const auto branches_if_overflow_set = instruction->operation
            == isa::Operation::branch_if_overflow_set;
        if (overflow_set == branches_if_overflow_set) {
            const auto displacement = std::bit_cast<std::int8_t>(
                result.bytes[1]
            );
            state_.pc = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(next) + displacement
            );
        } else {
            state_.pc = next;
        }
        break;
    }
    case isa::Operation::branch_if_plus:
    case isa::Operation::branch_if_minus: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;

        const auto negative = condition_mask(ConditionCode::negative);
        if ((state_.knowledge.condition_code & negative) == 0U) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto next = static_cast<std::uint16_t>(state_.pc + 2U);
        const auto negative_set = (state_.condition_code & negative) != 0U;
        const auto branches_if_negative = instruction->operation
            == isa::Operation::branch_if_minus;
        if (negative_set == branches_if_negative) {
            const auto displacement = std::bit_cast<std::int8_t>(
                result.bytes[1]
            );
            state_.pc = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(next) + displacement
            );
        } else {
            state_.pc = next;
        }
        break;
    }
    case isa::Operation::branch_if_greater_or_equal:
    case isa::Operation::branch_if_less:
    case isa::Operation::branch_if_greater:
    case isa::Operation::branch_if_less_or_equal: {
        const auto address = static_cast<std::uint16_t>(state_.pc + 1U);
        const auto operand = bus.read8(
            address,
            AccessKind::instruction_fetch
        );
        if (!operand.succeeded()) {
            return fail_bus_access(
                operand.fault,
                address,
                AccessKind::instruction_fetch
            );
        }
        result.bytes[1] = *operand.value;
        result.bytes_fetched = 2U;

        const auto negative = condition_mask(ConditionCode::negative);
        const auto zero = condition_mask(ConditionCode::zero);
        const auto overflow = condition_mask(ConditionCode::overflow);
        const auto tests_zero = instruction->operation
                == isa::Operation::branch_if_greater
            || instruction->operation
                == isa::Operation::branch_if_less_or_equal;
        const auto required = static_cast<std::uint8_t>(
            negative | overflow | (tests_zero ? zero : 0U)
        );
        if ((state_.knowledge.condition_code & required) != required) {
            return fail_unknown_state(CpuStatePart::condition_code);
        }

        const auto next = static_cast<std::uint16_t>(state_.pc + 2U);
        const auto negative_set = (state_.condition_code & negative) != 0U;
        const auto zero_set = (state_.condition_code & zero) != 0U;
        const auto overflow_set = (state_.condition_code & overflow) != 0U;
        const auto is_less = negative_set != overflow_set;
        const auto is_less_or_equal = zero_set || is_less;
        const auto branch_taken = tests_zero
            ? is_less_or_equal == (instruction->operation
                == isa::Operation::branch_if_less_or_equal)
            : is_less == (instruction->operation
                == isa::Operation::branch_if_less);
        if (branch_taken) {
            const auto displacement = std::bit_cast<std::int8_t>(
                result.bytes[1]
            );
            state_.pc = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(next) + displacement
            );
        } else {
            state_.pc = next;
        }
        break;
    }
    case isa::Operation::jump: {
        switch (instruction->addressing_mode) {
        case isa::AddressingMode::indexed8: {
            const auto displacement_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto displacement = bus.read8(
                displacement_address,
                AccessKind::instruction_fetch
            );
            if (!displacement.succeeded()) {
                return fail_bus_access(
                    displacement.fault,
                    displacement_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *displacement.value;
            result.bytes_fetched = 2U;
            if (!state_.knowledge.knows(CpuRegister::index_register)) {
                return fail_unknown_state(CpuStatePart::index_register);
            }
            state_.pc = static_cast<std::uint16_t>(
                state_.x + result.bytes[1]
            );
            break;
        }
        case isa::AddressingMode::extended16: {
            const auto msb_address = static_cast<std::uint16_t>(state_.pc + 1U);
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(state_.pc + 2U);
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;
            state_.pc = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            break;
        }
        default:
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }
        break;
    }
    case isa::Operation::branch_to_subroutine: {
        std::uint16_t return_address{};
        std::uint16_t target{};
        switch (instruction->addressing_mode) {
        case isa::AddressingMode::direct8:
        case isa::AddressingMode::indexed8: {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            return_address = static_cast<std::uint16_t>(state_.pc + 2U);
            target = result.bytes[1];
            if (instruction->addressing_mode
                == isa::AddressingMode::indexed8) {
                if (!state_.knowledge.knows(CpuRegister::index_register)) {
                    return fail_unknown_state(CpuStatePart::index_register);
                }
                target = static_cast<std::uint16_t>(
                    state_.x + result.bytes[1]
                );
            }
            break;
        }
        case isa::AddressingMode::relative8: {
            const auto operand_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto operand = bus.read8(
                operand_address,
                AccessKind::instruction_fetch
            );
            if (!operand.succeeded()) {
                return fail_bus_access(
                    operand.fault,
                    operand_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *operand.value;
            result.bytes_fetched = 2U;
            return_address = static_cast<std::uint16_t>(state_.pc + 2U);
            const auto displacement = std::bit_cast<std::int8_t>(
                result.bytes[1]
            );
            target = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(return_address) + displacement
            );
            break;
        }
        case isa::AddressingMode::extended16: {
            const auto msb_address = static_cast<std::uint16_t>(
                state_.pc + 1U
            );
            const auto msb = bus.read8(
                msb_address,
                AccessKind::instruction_fetch
            );
            if (!msb.succeeded()) {
                return fail_bus_access(
                    msb.fault,
                    msb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[1] = *msb.value;
            result.bytes_fetched = 2U;

            const auto lsb_address = static_cast<std::uint16_t>(
                state_.pc + 2U
            );
            const auto lsb = bus.read8(
                lsb_address,
                AccessKind::instruction_fetch
            );
            if (!lsb.succeeded()) {
                return fail_bus_access(
                    lsb.fault,
                    lsb_address,
                    AccessKind::instruction_fetch
                );
            }
            result.bytes[2] = *lsb.value;
            result.bytes_fetched = 3U;
            return_address = static_cast<std::uint16_t>(state_.pc + 3U);
            target = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(result.bytes[1]) << 8U)
                | static_cast<std::uint16_t>(result.bytes[2])
            );
            break;
        }
        default:
            result.fault = CpuFault::unimplemented_operation;
            return result;
        }

        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }

        const auto low = static_cast<std::uint8_t>(return_address & 0x00FFU);
        const auto high = static_cast<std::uint8_t>(return_address >> 8U);
        const auto low_write = bus.write8(state_.sp, low);
        if (!low_write.succeeded()) {
            return fail_bus_access(
                low_write.fault,
                state_.sp,
                AccessKind::data_write
            );
        }
        const auto high_address = static_cast<std::uint16_t>(state_.sp - 1U);
        const auto high_write = bus.write8(high_address, high);
        if (!high_write.succeeded()) {
            return fail_bus_access(
                high_write.fault,
                high_address,
                AccessKind::data_write
            );
        }

        state_.sp = static_cast<std::uint16_t>(state_.sp - 2U);
        state_.pc = target;
        break;
    }
    case isa::Operation::push_index_register: {
        if (!state_.knowledge.knows(CpuRegister::index_register)) {
            return fail_unknown_state(CpuStatePart::index_register);
        }
        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }

        const auto low = static_cast<std::uint8_t>(state_.x & 0x00FFU);
        const auto high = static_cast<std::uint8_t>(state_.x >> 8U);
        const auto low_write = bus.write8(state_.sp, low);
        if (!low_write.succeeded()) {
            return fail_bus_access(
                low_write.fault,
                state_.sp,
                AccessKind::data_write
            );
        }
        const auto high_address = static_cast<std::uint16_t>(state_.sp - 1U);
        const auto high_write = bus.write8(high_address, high);
        if (!high_write.succeeded()) {
            return fail_bus_access(
                high_write.fault,
                high_address,
                AccessKind::data_write
            );
        }

        state_.sp = static_cast<std::uint16_t>(state_.sp - 2U);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::push_accumulator_a: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_a)) {
            return fail_unknown_state(CpuStatePart::accumulator_a);
        }
        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }

        const auto write = bus.write8(state_.sp, state_.a);
        if (!write.succeeded()) {
            return fail_bus_access(
                write.fault,
                state_.sp,
                AccessKind::data_write
            );
        }

        state_.sp = static_cast<std::uint16_t>(state_.sp - 1U);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::push_accumulator_b: {
        if (!state_.knowledge.knows(CpuRegister::accumulator_b)) {
            return fail_unknown_state(CpuStatePart::accumulator_b);
        }
        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }

        const auto write = bus.write8(state_.sp, state_.b);
        if (!write.succeeded()) {
            return fail_bus_access(
                write.fault,
                state_.sp,
                AccessKind::data_write
            );
        }

        state_.sp = static_cast<std::uint16_t>(state_.sp - 1U);
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::pull_accumulator_a:
    case isa::Operation::pull_accumulator_b: {
        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }

        const auto address = static_cast<std::uint16_t>(state_.sp + 1U);
        const auto value = bus.read8(address, AccessKind::data_read);
        if (!value.succeeded()) {
            return fail_bus_access(
                value.fault,
                address,
                AccessKind::data_read
            );
        }

        if (instruction->operation == isa::Operation::pull_accumulator_a) {
            state_.a = *value.value;
            state_.knowledge.registers = static_cast<std::uint8_t>(
                state_.knowledge.registers
                | register_mask(CpuRegister::accumulator_a)
            );
        } else {
            state_.b = *value.value;
            state_.knowledge.registers = static_cast<std::uint8_t>(
                state_.knowledge.registers
                | register_mask(CpuRegister::accumulator_b)
            );
        }
        state_.sp = address;
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::pull_index_register: {
        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }

        const auto high_address = static_cast<std::uint16_t>(state_.sp + 1U);
        const auto high = bus.read8(high_address, AccessKind::data_read);
        if (!high.succeeded()) {
            return fail_bus_access(
                high.fault,
                high_address,
                AccessKind::data_read
            );
        }
        const auto low_address = static_cast<std::uint16_t>(state_.sp + 2U);
        const auto low = bus.read8(low_address, AccessKind::data_read);
        if (!low.succeeded()) {
            return fail_bus_access(
                low.fault,
                low_address,
                AccessKind::data_read
            );
        }

        state_.x = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(*high.value) << 8U)
            | static_cast<std::uint16_t>(*low.value)
        );
        state_.knowledge.registers = static_cast<std::uint8_t>(
            state_.knowledge.registers
            | register_mask(CpuRegister::index_register)
        );
        state_.sp = low_address;
        state_.pc = static_cast<std::uint16_t>(state_.pc + 1U);
        break;
    }
    case isa::Operation::return_from_interrupt: {
        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }

        std::array<std::uint8_t, 7U> stacked{};
        for (std::uint16_t offset = 1U; offset <= stacked.size(); ++offset) {
            const auto address = static_cast<std::uint16_t>(
                state_.sp + offset
            );
            const auto value = bus.read8(address, AccessKind::data_read);
            if (!value.succeeded()) {
                return fail_bus_access(
                    value.fault,
                    address,
                    AccessKind::data_read
                );
            }
            stacked[offset - 1U] = *value.value;
        }

        state_.condition_code = static_cast<std::uint8_t>(
            fixed_condition_code_bits | stacked[0]
        );
        state_.b = stacked[1];
        state_.a = stacked[2];
        state_.x = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(stacked[3]) << 8U)
            | static_cast<std::uint16_t>(stacked[4])
        );
        state_.pc = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(stacked[5]) << 8U)
            | static_cast<std::uint16_t>(stacked[6])
        );
        state_.sp = static_cast<std::uint16_t>(state_.sp + stacked.size());
        state_.execution_state = CpuExecutionState::active;
        state_.knowledge.registers = all_cpu_registers;
        state_.knowledge.condition_code = 0xFFU;
        break;
    }
    case isa::Operation::return_from_subroutine: {
        if (!state_.knowledge.knows(CpuRegister::stack_pointer)) {
            return fail_unknown_state(CpuStatePart::stack_pointer);
        }
        const auto high_address = static_cast<std::uint16_t>(state_.sp + 1U);
        const auto high = bus.read8(high_address, AccessKind::data_read);
        if (!high.succeeded()) {
            return fail_bus_access(
                high.fault,
                high_address,
                AccessKind::data_read
            );
        }
        const auto low_address = static_cast<std::uint16_t>(state_.sp + 2U);
        const auto low = bus.read8(low_address, AccessKind::data_read);
        if (!low.succeeded()) {
            return fail_bus_access(
                low.fault,
                low_address,
                AccessKind::data_read
            );
        }

        state_.sp = low_address;
        state_.pc = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(*high.value) << 8U)
            | static_cast<std::uint16_t>(*low.value)
        );
        break;
    }
    }

    result.cycles = instruction->base_cycles;
    const auto releases_interrupt_mask =
        instruction->operation == isa::Operation::clear_interrupt_mask
        || instruction->operation
            == isa::Operation::transfer_accumulator_a_to_condition_code;
    const auto interrupt_was_masked =
        (condition_code_known_before & interrupt_mask) != 0U
        && (condition_code_before & interrupt_mask) != 0U;
    const auto interrupt_is_unmasked =
        (state_.knowledge.condition_code & interrupt_mask) != 0U
        && (state_.condition_code & interrupt_mask) == 0U;
    if (releases_interrupt_mask && interrupt_was_masked
        && interrupt_is_unmasked && interrupt_request.asserted()) {
        state_.maskable_interrupt_delay_cycles = 2U;
    } else if (result.cycles >= delay_before) {
        state_.maskable_interrupt_delay_cycles = 0U;
    } else {
        state_.maskable_interrupt_delay_cycles = static_cast<std::uint8_t>(
            delay_before - result.cycles
        );
    }
    state_.cycle_count += result.cycles;
    const auto advance_fault = bus.advance_cycles(result.cycles);
    result.pc_after = state_.pc;
    result.step_completed = true;
    if (advance_fault != BusFault::none) {
        result.fault = CpuFault::bus_advance;
        result.bus_fault = advance_fault;
    }
    return result;
}

const CpuState& Cpu::state() const noexcept {
    return state_;
}

isa::CpuProfile Cpu::profile() const noexcept {
    return profile_;
}

void Cpu::set_nzv(std::uint8_t value) noexcept {
    const auto changed = condition_mask(ConditionCode::negative)
        | condition_mask(ConditionCode::zero)
        | condition_mask(ConditionCode::overflow);
    state_.condition_code = static_cast<std::uint8_t>(state_.condition_code & ~changed);
    if ((value & 0x80U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | condition_mask(ConditionCode::negative)
        );
    }
    if (value == 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | condition_mask(ConditionCode::zero)
        );
    }
    state_.knowledge.condition_code = static_cast<std::uint8_t>(
        state_.knowledge.condition_code | changed
    );
}

void Cpu::set_nzv16(std::uint16_t value) noexcept {
    const auto changed = condition_mask(ConditionCode::negative)
        | condition_mask(ConditionCode::zero)
        | condition_mask(ConditionCode::overflow);
    state_.condition_code = static_cast<std::uint8_t>(
        state_.condition_code & ~changed
    );
    if ((value & 0x8000U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | condition_mask(ConditionCode::negative)
        );
    }
    if (value == 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | condition_mask(ConditionCode::zero)
        );
    }
    state_.knowledge.condition_code = static_cast<std::uint8_t>(
        state_.knowledge.condition_code | changed
    );
}

void Cpu::set_addition_flags8(
    std::uint8_t left,
    std::uint8_t right,
    std::uint8_t carry_in,
    std::uint8_t result
) noexcept {
    const auto half_carry = condition_mask(ConditionCode::half_carry);
    const auto negative = condition_mask(ConditionCode::negative);
    const auto zero = condition_mask(ConditionCode::zero);
    const auto overflow = condition_mask(ConditionCode::overflow);
    const auto carry = condition_mask(ConditionCode::carry);
    const auto changed = static_cast<std::uint8_t>(
        half_carry | negative | zero | overflow | carry
    );
    state_.condition_code = static_cast<std::uint8_t>(
        state_.condition_code & ~changed
    );
    const auto full_result = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(left)
        + static_cast<std::uint16_t>(right)
        + static_cast<std::uint16_t>(carry_in)
    );
    const auto low_nibble_result = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(left & 0x0FU)
        + static_cast<std::uint16_t>(right & 0x0FU)
        + static_cast<std::uint16_t>(carry_in)
    );
    if (low_nibble_result > 0x0FU) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | half_carry
        );
    }
    if ((result & 0x80U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | negative
        );
    }
    if (result == 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | zero
        );
    }
    if (((~(left ^ right)) & (left ^ result) & 0x80U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | overflow
        );
    }
    if (full_result > 0x00FFU) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | carry
        );
    }
    state_.knowledge.condition_code = static_cast<std::uint8_t>(
        state_.knowledge.condition_code | changed
    );
}

void Cpu::set_subtraction_flags8(
    std::uint8_t minuend,
    std::uint8_t subtrahend,
    std::uint8_t borrow,
    std::uint8_t result
) noexcept {
    const auto negative = condition_mask(ConditionCode::negative);
    const auto zero = condition_mask(ConditionCode::zero);
    const auto overflow = condition_mask(ConditionCode::overflow);
    const auto carry = condition_mask(ConditionCode::carry);
    const auto changed = static_cast<std::uint8_t>(
        negative | zero | overflow | carry
    );
    state_.condition_code = static_cast<std::uint8_t>(
        state_.condition_code & ~changed
    );
    if ((result & 0x80U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | negative
        );
    }
    if (result == 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | zero
        );
    }
    if (((minuend ^ subtrahend) & (minuend ^ result) & 0x80U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | overflow
        );
    }
    const auto full_subtrahend = static_cast<std::uint16_t>(subtrahend)
        + static_cast<std::uint16_t>(borrow);
    if (static_cast<std::uint16_t>(minuend) < full_subtrahend) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | carry
        );
    }
    state_.knowledge.condition_code = static_cast<std::uint8_t>(
        state_.knowledge.condition_code | changed
    );
}

void Cpu::set_addition_flags16(
    std::uint16_t left,
    std::uint16_t right,
    std::uint16_t result
) noexcept {
    const auto negative = condition_mask(ConditionCode::negative);
    const auto zero = condition_mask(ConditionCode::zero);
    const auto overflow = condition_mask(ConditionCode::overflow);
    const auto carry = condition_mask(ConditionCode::carry);
    const auto changed = static_cast<std::uint8_t>(
        negative | zero | overflow | carry
    );
    state_.condition_code = static_cast<std::uint8_t>(
        state_.condition_code & ~changed
    );
    if ((result & 0x8000U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | negative
        );
    }
    if (result == 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | zero
        );
    }
    if (((~(left ^ right)) & (left ^ result) & 0x8000U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | overflow
        );
    }
    const auto full_result = static_cast<std::uint32_t>(left)
        + static_cast<std::uint32_t>(right);
    if (full_result > 0x0000FFFFU) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | carry
        );
    }
    state_.knowledge.condition_code = static_cast<std::uint8_t>(
        state_.knowledge.condition_code | changed
    );
}

void Cpu::set_subtraction_flags16(
    std::uint16_t minuend,
    std::uint16_t subtrahend,
    std::uint16_t result
) noexcept {
    const auto negative = condition_mask(ConditionCode::negative);
    const auto zero = condition_mask(ConditionCode::zero);
    const auto overflow = condition_mask(ConditionCode::overflow);
    const auto carry = condition_mask(ConditionCode::carry);
    const auto changed = static_cast<std::uint8_t>(
        negative | zero | overflow | carry
    );
    state_.condition_code = static_cast<std::uint8_t>(
        state_.condition_code & ~changed
    );
    if ((result & 0x8000U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | negative
        );
    }
    if (result == 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | zero
        );
    }
    if (((minuend ^ subtrahend) & (minuend ^ result) & 0x8000U) != 0U) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | overflow
        );
    }
    if (minuend < subtrahend) {
        state_.condition_code = static_cast<std::uint8_t>(
            state_.condition_code | carry
        );
    }
    state_.knowledge.condition_code = static_cast<std::uint8_t>(
        state_.knowledge.condition_code | changed
    );
}

}  // namespace jr800::core
