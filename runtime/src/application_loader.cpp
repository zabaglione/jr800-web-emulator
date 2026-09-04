// SPDX-License-Identifier: MIT

#include "jr800/runtime/application_loader.hpp"

#include <algorithm>

#include "jr800/formats/linked_error.hpp"
#include "jr800/isa/instruction_metadata.hpp"

namespace jr800::runtime {

LoadApplicationResult load_application(
    core::SyntheticMachine& machine,
    const formats::jr8app::Application& application,
    std::uint16_t initial_stack_pointer
) {
    try {
        static_cast<void>(formats::jr8app::write(application));
    } catch (const formats::linked::Error&) {
        return LoadApplicationResult::invalid_format;
    }
    const auto profile = isa::find_profile(application.target_profile);
    if (!profile.has_value()) {
        return LoadApplicationResult::unknown_profile;
    }
    const auto has_reviewed_instructions = std::any_of(
        isa::all_instructions().begin(),
        isa::all_instructions().end(),
        [&](const auto& instruction) {
            return isa::instruction_applies_to(instruction, *profile);
        }
    );
    if (!has_reviewed_instructions) {
        return LoadApplicationResult::unreviewed_profile;
    }

    machine.bus().clear();
    for (const auto& segment : application.segments) {
        const auto loaded = segment.kind == formats::jr8app::SegmentKind::data
            ? machine.bus().load(segment.address, segment.data)
            : machine.bus().fill(segment.address, segment.logical_size, 0U);
        if (!loaded) {
            return LoadApplicationResult::segment_out_of_range;
        }
    }
    machine.execution().initialize(
        *profile,
        application.entry_point,
        initial_stack_pointer
    );
    return LoadApplicationResult::loaded;
}

LoadApplicationResult load_application(
    core::Jr800Machine& machine,
    const formats::jr8app::Application& application
) {
    try {
        static_cast<void>(formats::jr8app::write(application));
    } catch (const formats::linked::Error&) {
        return LoadApplicationResult::invalid_format;
    }
    if (application.target_profile != "hd6301v1") {
        return LoadApplicationResult::target_mismatch;
    }

    for (const auto& segment : application.segments) {
        if (!machine.can_host_load_ram(
                segment.address,
                segment.logical_size
            )) {
            return LoadApplicationResult::segment_out_of_range;
        }
    }
    for (const auto& segment : application.segments) {
        const auto status = segment.kind == formats::jr8app::SegmentKind::data
            ? machine.host_load_ram(segment.address, segment.data)
            : machine.host_fill_ram(
                segment.address,
                segment.logical_size,
                0U
            );
        if (status != core::Jr800MemoryStatus::ok) {
            return LoadApplicationResult::segment_out_of_range;
        }
    }
    machine.host_start_program(application.entry_point);
    return LoadApplicationResult::loaded;
}

}  // namespace jr800::runtime
