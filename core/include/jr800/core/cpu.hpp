// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>

#include "jr800/core/bus.hpp"
#include "jr800/isa/instruction_metadata.hpp"

namespace jr800::core {

inline constexpr std::uint8_t fixed_condition_code_bits = 0xC0U;

enum class CpuRegister : std::uint8_t {
    program_counter = 0x01,
    stack_pointer = 0x02,
    index_register = 0x04,
    accumulator_a = 0x08,
    accumulator_b = 0x10,
};

[[nodiscard]] constexpr std::uint8_t register_mask(
    CpuRegister cpu_register
) noexcept {
    return static_cast<std::uint8_t>(cpu_register);
}

inline constexpr std::uint8_t all_cpu_registers = 0x1FU;

struct CpuStateKnowledge {
    std::uint8_t registers{};
    std::uint8_t condition_code{};

    [[nodiscard]] constexpr bool knows(CpuRegister cpu_register) const noexcept {
        const auto mask = register_mask(cpu_register);
        return (registers & mask) == mask;
    }

    bool operator==(const CpuStateKnowledge&) const = default;
};

enum class CpuExecutionState : std::uint8_t {
    active,
    sleeping,
    waiting_for_interrupt,
};

struct CpuState {
    std::uint16_t pc{};
    std::uint16_t sp{};
    std::uint16_t x{};
    std::uint8_t a{};
    std::uint8_t b{};
    std::uint8_t condition_code{fixed_condition_code_bits};
    std::uint64_t cycle_count{};
    CpuExecutionState execution_state{CpuExecutionState::active};
    std::uint8_t maskable_interrupt_delay_cycles{};
    CpuStateKnowledge knowledge{};

    bool operator==(const CpuState&) const = default;
};

enum class ConditionCode : std::uint8_t {
    carry = 0x01,
    overflow = 0x02,
    zero = 0x04,
    negative = 0x08,
    interrupt_mask = 0x10,
    half_carry = 0x20,
};

[[nodiscard]] constexpr std::uint8_t condition_mask(ConditionCode flag) noexcept {
    return static_cast<std::uint8_t>(flag);
}

enum class CpuFault : std::uint8_t {
    none,
    unsupported_opcode,
    unimplemented_operation,
    bus_access,
    unknown_state,
    unknown_interrupt_request,
    bus_advance,
};

enum class StepKind : std::uint8_t {
    dormant,
    instruction,
    interrupt_entry,
    sleep_resume,
};

enum class CpuStatePart : std::uint8_t {
    none,
    program_counter,
    stack_pointer,
    index_register,
    accumulator_a,
    accumulator_b,
    condition_code,
};

struct StepResult {
    StepKind kind{StepKind::dormant};
    InterruptSource interrupt_source{InterruptSource::none};
    CpuFault fault{CpuFault::none};
    std::uint16_t pc_before{};
    std::uint16_t pc_after{};
    std::array<std::uint8_t, 3> bytes{};
    std::uint8_t instruction_length{};
    std::uint8_t bytes_fetched{};
    std::uint8_t cycles{};
    BusFault bus_fault{BusFault::none};
    std::uint16_t fault_address{};
    AccessKind fault_access{AccessKind::data_read};
    CpuStatePart state_fault{CpuStatePart::none};
    bool step_completed{};

    [[nodiscard]] bool succeeded() const noexcept {
        return fault == CpuFault::none && step_completed;
    }
};

class Cpu final {
public:
    void reset() noexcept;
    void initialize(
        isa::CpuProfile profile,
        std::uint16_t program_counter,
        std::uint16_t stack_pointer
    ) noexcept;

    [[nodiscard]] StepResult step_instruction(Bus& bus);

    [[nodiscard]] const CpuState& state() const noexcept;
    [[nodiscard]] isa::CpuProfile profile() const noexcept;

private:
    friend class Machine;

    void initialize_known_state(
        isa::CpuProfile profile,
        CpuState state
    ) noexcept;
    [[nodiscard]] StepResult service_maskable_interrupt(
        Bus& bus,
        InterruptRequest request
    );
    void set_nzv(std::uint8_t value) noexcept;
    void set_nzv16(std::uint16_t value) noexcept;
    void set_addition_flags8(
        std::uint8_t left,
        std::uint8_t right,
        std::uint8_t carry_in,
        std::uint8_t result
    ) noexcept;
    void set_addition_flags16(
        std::uint16_t left,
        std::uint16_t right,
        std::uint16_t result
    ) noexcept;
    void set_subtraction_flags8(
        std::uint8_t minuend,
        std::uint8_t subtrahend,
        std::uint8_t borrow,
        std::uint8_t result
    ) noexcept;
    void set_subtraction_flags16(
        std::uint16_t minuend,
        std::uint16_t subtrahend,
        std::uint16_t result
    ) noexcept;

    CpuState state_{};
    isa::CpuProfile profile_{isa::CpuProfile::jr800_unresolved};
};

}  // namespace jr800::core
