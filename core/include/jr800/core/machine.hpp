// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include "jr800/core/bus.hpp"
#include "jr800/core/cpu.hpp"

namespace jr800::core {

class Machine;
class Jr800Machine;
class SyntheticMachine;

struct SuspendedCycleAdvanceResult {
    bool suspended{};
    std::uint32_t cycles_elapsed{};
    InterruptRequest interrupt_request{};
    BusFault bus_fault{BusFault::none};

    bool operator==(const SuspendedCycleAdvanceResult&) const = default;
};

class MachineObserver : public BusObserver {
public:
    MachineObserver() = default;
    ~MachineObserver() override;

    MachineObserver(const MachineObserver&) = delete;
    MachineObserver& operator=(const MachineObserver&) = delete;
    MachineObserver(MachineObserver&&) = delete;
    MachineObserver& operator=(MachineObserver&&) = delete;

    virtual void on_machine_detached(Machine& machine) noexcept = 0;
    virtual void on_step_begin(const CpuState& state) noexcept = 0;
    virtual void on_step_end(
        const StepResult& result,
        const CpuState& state
    ) noexcept = 0;

private:
    friend class Machine;
    Machine* machine_{};
};

class Machine final {
public:
    ~Machine();

    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;
    Machine(Machine&&) = delete;
    Machine& operator=(Machine&&) = delete;

    void reset() noexcept;
    void initialize(
        isa::CpuProfile profile,
        std::uint16_t program_counter,
        std::uint16_t stack_pointer
    ) noexcept;

    [[nodiscard]] StepResult step_instruction();
    [[nodiscard]] SuspendedCycleAdvanceResult advance_suspended_cycles(
        std::uint32_t cycle_limit
    ) noexcept;
    [[nodiscard]] BusReadResult inspect8(
        std::uint16_t address
    ) const noexcept;

    [[nodiscard]] bool set_observer(MachineObserver* observer) noexcept;
    [[nodiscard]] MachineObserver* observer() noexcept;
    [[nodiscard]] const MachineObserver* observer() const noexcept;

    [[nodiscard]] Cpu& cpu() noexcept;
    [[nodiscard]] const Cpu& cpu() const noexcept;

private:
    friend class Jr800Machine;
    friend class MachineObserver;
    friend class SyntheticMachine;

    explicit Machine(Bus& bus) noexcept;
    void initialize_known_state(
        isa::CpuProfile profile,
        CpuState state
    ) noexcept;
    void release_destroying_observer(MachineObserver& observer) noexcept;

    Cpu cpu_{};
    Bus& bus_;
    MachineObserver* observer_{};
};

}  // namespace jr800::core
