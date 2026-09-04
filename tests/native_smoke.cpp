// SPDX-License-Identifier: MIT

#include <cstdint>
#include <iostream>
#include <vector>

#include "jr800/core/synthetic_machine.hpp"
#include "jr800/debugger/debugger.hpp"
#include "jr800/formats/api.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/wasm/api.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    jr800::core::SyntheticMachine machine;
    machine.execution().reset();

    jr800::debugger::Debugger debugger;
    const auto attached = debugger.attach(machine.execution());

    bool passed = true;
    passed &= expect(
        machine.execution().cpu().state().cycle_count == 0
            && machine.execution().cpu().state().knowledge.registers
                == jr800::core::all_cpu_registers
            && machine.execution().cpu().state().knowledge.condition_code
                == 0xFFU,
        "CPU reset smoke failed"
    );
    passed &= expect(
        attached && debugger.machine() == &machine.execution(),
        "Debugger attach smoke failed"
    );
    passed &= expect(
        jr800::formats::api_version.major == 0,
        "Formats API version smoke failed"
    );

    jr800_machine* handle = jr800_machine_create();
    passed &= expect(handle != nullptr, "C API create smoke failed");

    jr800_machine_state state{};
    passed &= expect(
        jr800_machine_get_state(handle, &state) == JR800_STATUS_OK,
        "C API state smoke failed"
    );
    passed &= expect(
        state.abi_version == JR800_WASM_ABI_VERSION,
        "C API ABI version smoke failed"
    );
    passed &= expect(
        state.execution_state == JR800_CPU_ACTIVE
            && state.cycle_count_low == 0
            && state.cycle_count_high == 0,
        "C API cycle state smoke failed"
    );

    jr800::formats::jr8app::Application sleep_application;
    sleep_application.target_profile = "hd6301v1";
    sleep_application.entry_point = 0x0200U;
    sleep_application.segments.push_back(jr800::formats::jr8app::Segment{
        jr800::formats::jr8app::SegmentKind::data,
        0x0200U,
        2U,
        std::vector<std::uint8_t>{0x1AU, 0x01U},
    });
    sleep_application.integrity_sha256 =
        jr800::formats::jr8app::compute_integrity(sleep_application);
    const auto sleep_bytes = jr800::formats::jr8app::write(sleep_application);
    passed &= expect(
        jr800_machine_load_application(
            handle,
            sleep_bytes.data(),
            static_cast<std::uint32_t>(sleep_bytes.size()),
            0x01FFU
        ) == JR800_STATUS_OK,
        "C API sleep application load failed"
    );

    jr800_stop_info sleep_stop{};
    passed &= expect(
        jr800_machine_step(handle, &sleep_stop) == JR800_STATUS_OK
            && sleep_stop.reason == JR800_STOP_SLEEPING
            && sleep_stop.fault == JR800_FAULT_NONE
            && sleep_stop.instructions_executed_low == 1U
            && sleep_stop.instructions_executed_high == 0U
            && sleep_stop.pc_before == 0x0200U
            && sleep_stop.pc_after == 0x0201U
            && sleep_stop.byte0 == 0x1AU
            && sleep_stop.bytes_fetched == 1U
            && sleep_stop.cycles == 4U,
        "C API SLP stop record differs"
    );
    passed &= expect(
        jr800_machine_get_state(handle, &state) == JR800_STATUS_OK
            && state.execution_state == JR800_CPU_SLEEPING
            && state.pc == 0x0201U
            && state.cycle_count_low == 4U
            && state.cycle_count_high == 0U,
        "C API sleeping state differs"
    );

    jr800_stop_info dormant_stop{};
    passed &= expect(
        jr800_machine_run(handle, 10U, &dormant_stop) == JR800_STATUS_OK
            && dormant_stop.reason == JR800_STOP_SLEEPING
            && dormant_stop.fault == JR800_FAULT_NONE
            && dormant_stop.instructions_executed_low == 0U
            && dormant_stop.instructions_executed_high == 0U
            && dormant_stop.pc_before == 0x0201U
            && dormant_stop.pc_after == 0x0201U
            && dormant_stop.bytes_fetched == 0U
            && dormant_stop.cycles == 0U
            && jr800_machine_history_count(handle) == 1U,
        "C API sleeping run made progress"
    );

    jr800_history_entry sleep_history{};
    passed &= expect(
        jr800_machine_copy_history(handle, &sleep_history, 1U) == 1U
            && sleep_history.state_execution_state == JR800_CPU_SLEEPING
            && sleep_history.state_pc == 0x0201U
            && sleep_history.state_cycle_count_low == 4U,
        "C API sleep history state differs"
    );
    passed &= expect(
        jr800_machine_reset(handle) == JR800_STATUS_OK
            && jr800_machine_get_state(handle, &state) == JR800_STATUS_OK
            && state.execution_state == JR800_CPU_ACTIVE
            && state.pc == 0x0200U,
        "C API reset did not leave sleep state"
    );

    jr800::formats::jr8app::Application wait_application;
    wait_application.target_profile = "hd6301v1";
    wait_application.entry_point = 0x0200U;
    wait_application.segments.push_back(jr800::formats::jr8app::Segment{
        jr800::formats::jr8app::SegmentKind::data,
        0x0200U,
        1U,
        std::vector<std::uint8_t>{0x3EU},
    });
    wait_application.integrity_sha256 =
        jr800::formats::jr8app::compute_integrity(wait_application);
    const auto wait_bytes = jr800::formats::jr8app::write(wait_application);
    passed &= expect(
        jr800_machine_load_application(
            handle,
            wait_bytes.data(),
            static_cast<std::uint32_t>(wait_bytes.size()),
            0x01FFU
        ) == JR800_STATUS_OK,
        "C API wait application load failed"
    );
    jr800_stop_info wait_stop{};
    passed &= expect(
        jr800_machine_step(handle, &wait_stop) == JR800_STATUS_OK
            && wait_stop.reason == JR800_STOP_SLEEPING
            && wait_stop.fault == JR800_FAULT_NONE
            && wait_stop.instructions_executed_low == 1U
            && wait_stop.pc_before == 0x0200U
            && wait_stop.pc_after == 0x0201U
            && wait_stop.byte0 == 0x3EU
            && wait_stop.bytes_fetched == 1U
            && wait_stop.cycles == 9U,
        "C API WAI stop record differs"
    );
    passed &= expect(
        jr800_machine_get_state(handle, &state) == JR800_STATUS_OK
            && state.execution_state == JR800_CPU_WAITING_FOR_INTERRUPT
            && state.pc == 0x0201U
            && state.sp == 0x01F8U
            && state.cycle_count_low == 9U,
        "C API WAI state differs"
    );
    jr800_history_entry wait_history{};
    passed &= expect(
        jr800_machine_copy_history(handle, &wait_history, 1U) == 1U
            && wait_history.state_execution_state
                == JR800_CPU_WAITING_FOR_INTERRUPT
            && wait_history.state_pc == 0x0201U
            && wait_history.state_cycle_count_low == 9U,
        "C API WAI history state differs"
    );
    jr800_machine_destroy(handle);

    return passed ? 0 : 1;
}
