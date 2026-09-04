// SPDX-License-Identifier: MIT

#include "jr800/core/synthetic_machine.hpp"

namespace jr800::core {

SyntheticMachine::SyntheticMachine() noexcept : execution_(bus_) {
    execution_.reset();
}

Machine& SyntheticMachine::execution() noexcept {
    return execution_;
}

const Machine& SyntheticMachine::execution() const noexcept {
    return execution_;
}

RamBus& SyntheticMachine::bus() noexcept {
    return bus_;
}

const RamBus& SyntheticMachine::bus() const noexcept {
    return bus_;
}

}  // namespace jr800::core
