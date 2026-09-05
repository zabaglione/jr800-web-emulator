// SPDX-License-Identifier: MIT
#pragma once
#include "jr800/core/jr800_machine.hpp"
#include "jr800/formats/native_msave.hpp"
#include <optional>
#include <vector>

namespace jr800::runtime {
enum class ProgramSaveState : std::uint32_t { unavailable, idle, recording, failed, full };

// Read-only observer of E-427's ROM output services. It neither patches ROM
// nor changes CPU execution, tape timing, debugger events or device state.
class ProgramSaveCapture final : private core::MachineObserver {
public:
    explicit ProgramSaveCapture(core::Jr800Machine& machine);
    void reset();
    void clear();
    [[nodiscard]] ProgramSaveState state() const noexcept { return state_; }
    [[nodiscard]] const std::vector<formats::NativeMsaveFile>& files() const noexcept { return files_; }
    static constexpr std::size_t capacity = 32U;
private:
    void on_machine_detached(core::Machine&) noexcept override;
    void on_step_begin(const core::CpuState&) noexcept override;
    void on_step_end(const core::StepResult&, const core::CpuState&) noexcept override;
    void on_bus_access(const core::BusAccessEvent&) noexcept override {}
    void fail(ProgramSaveState state = ProgramSaveState::failed) noexcept;
    void finish_block(const core::CpuState&);
    core::Jr800Machine* machine_;
    bool enabled_{};
    ProgramSaveState state_{ProgramSaveState::unavailable};
    struct PendingBlock {
        std::uint16_t return_pc;
        std::uint16_t return_sp;
        std::vector<std::uint8_t> bytes;
    };
    std::optional<PendingBlock> pending_;
    std::vector<std::vector<std::uint8_t>> blocks_;
    std::vector<formats::NativeMsaveFile> files_;
};
}
