// SPDX-License-Identifier: MIT

#include "jr800/core/machine.hpp"

namespace jr800::core {

Machine::Machine(Bus& bus) noexcept : bus_(bus) {}

MachineObserver::~MachineObserver() {
    if (machine_ != nullptr) {
        machine_->release_destroying_observer(*this);
    }
}

Machine::~Machine() {
    static_cast<void>(set_observer(nullptr));
}

void Machine::reset() noexcept {
    cpu_.reset();
}

void Machine::initialize(
    isa::CpuProfile profile,
    std::uint16_t program_counter,
    std::uint16_t stack_pointer
) noexcept {
    cpu_.initialize(profile, program_counter, stack_pointer);
}

void Machine::initialize_known_state(
    isa::CpuProfile profile,
    CpuState state
) noexcept {
    cpu_.initialize_known_state(profile, state);
}

StepResult Machine::step_instruction() {
    const auto execution_state = cpu_.state().execution_state;
    if (execution_state != CpuExecutionState::active) {
        const auto request = bus_.maskable_interrupt_request();
        if (request.known && !request.asserted()) {
            return cpu_.step_instruction(bus_);
        }
        const auto interrupt_mask = condition_mask(
            ConditionCode::interrupt_mask
        );
        if (execution_state == CpuExecutionState::waiting_for_interrupt
            && request.asserted()
            && (cpu_.state().knowledge.condition_code & interrupt_mask) != 0U
            && (cpu_.state().condition_code & interrupt_mask) != 0U) {
            return cpu_.step_instruction(bus_);
        }
        bus_.set_instruction_context(
            cpu_.state().cycle_count,
            cpu_.state().pc
        );
        if (observer_ != nullptr) {
            observer_->on_step_begin(cpu_.state());
        }
        const auto result = cpu_.service_maskable_interrupt(bus_, request);
        if (observer_ != nullptr) {
            observer_->on_step_end(result, cpu_.state());
        }
        return result;
    }
    bus_.set_instruction_context(cpu_.state().cycle_count, cpu_.state().pc);
    if (observer_ != nullptr) {
        observer_->on_step_begin(cpu_.state());
    }
    const auto result = cpu_.step_instruction(bus_);
    if (observer_ != nullptr) {
        observer_->on_step_end(result, cpu_.state());
    }
    return result;
}

SuspendedCycleAdvanceResult Machine::advance_suspended_cycles(
    std::uint32_t cycle_limit
) noexcept {
    SuspendedCycleAdvanceResult result;
    const auto execution_state = cpu_.state_.execution_state;
    if (execution_state == CpuExecutionState::active) {
        return result;
    }

    result.suspended = true;
    result.interrupt_request = bus_.maskable_interrupt_request();
    const auto is_request_boundary = [this, execution_state](
        InterruptRequest request
    ) noexcept {
        if (!request.known) {
            return true;
        }
        if (!request.asserted()) {
            return false;
        }
        if (execution_state == CpuExecutionState::sleeping) {
            return true;
        }
        const auto interrupt_mask = condition_mask(
            ConditionCode::interrupt_mask
        );
        if ((cpu_.state_.knowledge.condition_code & interrupt_mask) == 0U) {
            return true;
        }
        return (cpu_.state_.condition_code & interrupt_mask) == 0U;
    };
    while (result.cycles_elapsed < cycle_limit
           && !is_request_boundary(result.interrupt_request)) {
        result.bus_fault = bus_.advance_cycles(1U);
        if (result.bus_fault != BusFault::none) {
            break;
        }
        ++cpu_.state_.cycle_count;
        ++result.cycles_elapsed;
        result.interrupt_request = bus_.maskable_interrupt_request();
    }
    return result;
}

BusReadResult Machine::inspect8(std::uint16_t address) const noexcept {
    return bus_.inspect8(address);
}

bool Machine::set_observer(MachineObserver* observer) noexcept {
    if (observer_ == observer) {
        return true;
    }
    if (observer != nullptr && observer->machine_ != nullptr
        && observer->machine_ != this) {
        return false;
    }
    auto* const detached = observer_;
    if (!bus_.set_observer(observer)) {
        return false;
    }
    observer_ = observer;
    if (observer != nullptr) {
        observer->machine_ = this;
    }
    if (detached != nullptr) {
        detached->machine_ = nullptr;
        detached->on_machine_detached(*this);
    }
    return true;
}

void Machine::release_destroying_observer(MachineObserver& observer) noexcept {
    if (observer_ == &observer) {
        observer_ = nullptr;
        static_cast<void>(bus_.set_observer(nullptr));
    }
    observer.machine_ = nullptr;
}

MachineObserver* Machine::observer() noexcept {
    return observer_;
}

const MachineObserver* Machine::observer() const noexcept {
    return observer_;
}

Cpu& Machine::cpu() noexcept {
    return cpu_;
}

const Cpu& Machine::cpu() const noexcept {
    return cpu_;
}

}  // namespace jr800::core
