// SPDX-License-Identifier: MIT
#pragma once
#include "jr800/runtime/application_loader.hpp"
namespace jr800::runtime {
[[nodiscard]] LoadApplicationResult load_basic_program(core::Jr800Machine& machine,
    const formats::jr8app::Application& application, bool run_after_load);
}
