// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "jr800/core/synthetic_machine.hpp"
#include "jr800/core/jr800_machine.hpp"
#include "jr800/formats/jr8app.hpp"

namespace jr800::runtime {

enum class LoadApplicationResult : std::uint8_t {
    loaded,
    invalid_format,
    unknown_profile,
    unreviewed_profile,
    segment_out_of_range,
    target_mismatch,
};

[[nodiscard]] LoadApplicationResult load_application(
    core::SyntheticMachine& machine,
    const formats::jr8app::Application& application,
    std::uint16_t initial_stack_pointer
);

[[nodiscard]] LoadApplicationResult load_application(
    core::Jr800Machine& machine,
    const formats::jr8app::Application& application
);

}  // namespace jr800::runtime
