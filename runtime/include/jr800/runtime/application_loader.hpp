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
    entry_point_not_loaded,
    target_mismatch,
    unsupported_basic_rom,
    basic_not_ready,
    invalid_basic_program,
    basic_load_failed,
};

[[nodiscard]] LoadApplicationResult load_application(
    core::SyntheticMachine& machine,
    const formats::jr8app::Application& application,
    std::uint16_t initial_stack_pointer
);

[[nodiscard]] LoadApplicationResult load_application(
    core::Jr800Machine& machine,
    const formats::jr8app::Application& application,
    bool run_after_load = true
);

}  // namespace jr800::runtime
