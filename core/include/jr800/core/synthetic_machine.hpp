// SPDX-License-Identifier: MIT

#pragma once

#include "jr800/core/bus.hpp"
#include "jr800/core/machine.hpp"

namespace jr800::core {

class SyntheticMachine final {
public:
    SyntheticMachine() noexcept;

    SyntheticMachine(const SyntheticMachine&) = delete;
    SyntheticMachine& operator=(const SyntheticMachine&) = delete;
    SyntheticMachine(SyntheticMachine&&) = delete;
    SyntheticMachine& operator=(SyntheticMachine&&) = delete;

    [[nodiscard]] Machine& execution() noexcept;
    [[nodiscard]] const Machine& execution() const noexcept;
    [[nodiscard]] RamBus& bus() noexcept;
    [[nodiscard]] const RamBus& bus() const noexcept;

private:
    RamBus bus_{};
    Machine execution_;
};

}  // namespace jr800::core
