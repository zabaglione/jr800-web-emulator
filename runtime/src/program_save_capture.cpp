// SPDX-License-Identifier: MIT
#include "jr800/runtime/program_save_capture.hpp"
#include "basic_rom.hpp"
#include <stdexcept>

namespace jr800::runtime {
ProgramSaveCapture::ProgramSaveCapture(core::Jr800Machine& machine) : machine_(&machine) {
    if (!machine.execution().add_observer(this)) throw std::runtime_error("Cannot observe program saves");
    reset();
}
void ProgramSaveCapture::reset() {
    pending_.reset(); blocks_.clear();
    enabled_ = false;
    try { enabled_ = machine_ != nullptr && basic_rom::supported_rom(*machine_); }
    catch (const std::exception&) { /* A machine without a complete ROM is unavailable. */ }
    state_ = enabled_ ? ProgramSaveState::idle : ProgramSaveState::unavailable;
}
void ProgramSaveCapture::clear() {
    files_.clear();
    reset();
}
void ProgramSaveCapture::fail(ProgramSaveState state) noexcept {
    pending_.reset(); blocks_.clear(); state_ = state;
}
void ProgramSaveCapture::on_machine_detached(core::Machine&) noexcept {
    machine_ = nullptr; enabled_ = false; fail(ProgramSaveState::unavailable);
}
void ProgramSaveCapture::finish_block(const core::CpuState& state) {
    if (!state.knowledge.knows(core::CpuRegister::accumulator_a)
        || (state.knowledge.condition_code & 5U) != 5U
        || state.a != 0U || (state.condition_code & 5U) != 4U) {
        fail(); return;
    }
    blocks_.push_back(std::move(pending_->bytes));
    pending_.reset();
    if (blocks_.size() == 1U) return;
    if (blocks_[0][0] == 3U && blocks_.back()[0] == 0U) return;
    auto decoded = formats::decode_native_program_blocks(blocks_);
    if (!decoded.file || !decoded.issues.empty()) { fail(); return; }
    // J8A and WAV are offered from the same validated snapshot.
    static_cast<void>(formats::native_program_application(*decoded.file));
    files_.push_back(std::move(*decoded.file));
    blocks_.clear(); state_ = ProgramSaveState::idle;
}
void ProgramSaveCapture::on_step_begin(const core::CpuState& state) noexcept {
    if (!enabled_) return;
    try {
        if (pending_ && state.pc == pending_->return_pc && state.sp == pending_->return_sp) {
            finish_block(state);
        }
        if (state.pc != 0x2221U && state.pc != 0x2224U) return;
        if (state.pc == 0x2221U) {
            pending_.reset(); blocks_.clear();
            if (files_.size() == capacity) { fail(ProgramSaveState::full); return; }
            state_ = ProgramSaveState::recording;
        } else if (state_ != ProgramSaveState::recording) return;
        if (pending_ || (state.pc == 0x2224U && blocks_.empty())) { fail(); return; }
        const auto length = state.pc == 0x2221U ? 32U : basic_rom::word(*machine_, 0x22D2U);
        const auto address = state.pc == 0x2221U ? 0x22C0U : basic_rom::word(*machine_, 0x22D4U);
        if (length == 0U || length > 32768U || address + length > 65536U || blocks_.size() >= 130U
            || !state.knowledge.knows(core::CpuRegister::stack_pointer) || state.sp > 0xFFFDU) {
            fail(); return;
        }
        auto bytes = basic_rom::memory(*machine_, static_cast<std::uint16_t>(address), length);
        if (state.pc == 0x2221U && (bytes[0] < 1U || bytes[0] > 3U)) { fail(); return; }
        if (state.pc == 0x2224U && blocks_[0][0] == 3U && (length != 257U || bytes[0] > 1U)) {
            fail(); return;
        }
        pending_ = PendingBlock{basic_rom::word(*machine_, static_cast<std::uint16_t>(state.sp + 1U)),
            static_cast<std::uint16_t>(state.sp + 2U), std::move(bytes)};
    } catch (const std::exception&) { fail(); }
}
void ProgramSaveCapture::on_step_end(const core::StepResult& result, const core::CpuState& state) noexcept {
    if (state_ != ProgramSaveState::recording) return;
    if (result.fault != core::CpuFault::none
        || (pending_ && state.sp > pending_->return_sp)) fail();
}
}
