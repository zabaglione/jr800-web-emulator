// SPDX-License-Identifier: MIT
#include "jr800/formats/native_msave.hpp"
#include "jr800/runtime/program_save_capture.hpp"
#include "jr800/runtime/application_loader.hpp"
#include "jr800/core/synthetic_machine.hpp"
#include "jr800/debugger/debugger.hpp"
#include "jr800/wasm/api.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Observer final : jr800::core::MachineObserver {
    unsigned steps{}, accesses{}, detached{};
    void on_machine_detached(jr800::core::Machine&) noexcept override { ++detached; }
    void on_step_begin(const jr800::core::CpuState&) noexcept override { ++steps; }
    void on_step_end(const jr800::core::StepResult&, const jr800::core::CpuState&) noexcept override {}
    void on_bus_access(const jr800::core::BusAccessEvent&) noexcept override { ++accesses; }
};
void roundtrip(const jr800::formats::NativeMsaveFile& file) {
    using namespace jr800::formats;
    const auto expected = jr8app::write(native_program_application(file));
    const auto wav = encode_native_program_wav(file);
    const auto decoded = decode_native_program_wav(wav);
    require(decoded.file && decoded.issues.empty(), "Exported WAV could not be decoded");
    require(decoded.file->filename == file.filename && decoded.file->payload == file.payload
        && decoded.file->kind == file.kind, "Exported name, kind or payload changed");
    require(jr8app::write(native_program_application(*decoded.file)) == expected, "WAV and J8A exports differ");
    require(expected[8] == 0U && expected[9] == 1U, "J8A version changed");
    require(wav[22] == 1U && wav[24] == 0x80U && wav[25] == 0xBBU && wav[34] == 16U,
        "WAV must be mono 48 kHz PCM16");
}
}
int main() {
    using namespace jr800;
    using formats::jr8app::ProgramKind;
    try {
        formats::NativeMsaveFile file;
        file.filename = "BYTES"; file.start_address = 0x6000U; file.execution_address = 0x6001U;
        file.payload = {0x00,0x01,0xFF,0x80,0x7F,0xAA,0x55}; roundtrip(file);
        for (const std::uint16_t entry : {0U, 0x5FFFU}) {
            file.execution_address = entry; roundtrip(file);
            const auto application = formats::native_program_application(file);
            require(application.entry_point == entry, "Data-only entry was rewritten");
            core::Jr800ExperimentalMachineConfiguration configuration;
            configuration.memory = core::Jr800ExperimentalMemoryConfiguration{0x55U, 0x55U};
            core::Jr800Machine data_machine{configuration};
            const auto before = data_machine.execution().cpu().state();
            require(runtime::load_application(data_machine, application, true)
                == runtime::LoadApplicationResult::entry_point_not_loaded,
                "Data-only file executed");
            require(data_machine.execution().cpu().state().pc == before.pc,
                "Rejected execution changed CPU");
            require(data_machine.execution().inspect8(0x6000U).value == 0x55U,
                "Rejected execution changed RAM");
            require(runtime::load_application(data_machine, application, false)
                == runtime::LoadApplicationResult::loaded, "Data-only load rejected");
            for (std::size_t offset = 0; offset < file.payload.size(); ++offset) {
                require(data_machine.execution().inspect8(static_cast<std::uint16_t>(0x6000U + offset)).value
                    == file.payload[offset], "Data-only load changed payload");
            }
            core::SyntheticMachine synthetic;
            require(runtime::load_application(synthetic, application, 0x01FFU)
                == runtime::LoadApplicationResult::entry_point_not_loaded,
                "Synthetic data-only execution accepted");
        }
        file.execution_address = 0x6001U;
        file.filename = std::string{'K',static_cast<char>(0xB6U)}; roundtrip(file);
        file.kind = ProgramKind::basic_binary; file.start_address = 0x2800U;
        file.payload = {0,8,0,10,'A',':','B',0,0,0}; roundtrip(file);
        file.payload = {0,0}; roundtrip(file);
        file.kind = ProgramKind::basic_text; file.filename = "";
        file.payload.clear(); roundtrip(file);
        for (const std::size_t length : {254U,255U,256U,257U,656U}) {
            file.payload.clear(); unsigned line = 10U;
            while (file.payload.size() < length) {
                const auto remaining = length - file.payload.size();
                auto size = std::min<std::size_t>(remaining, 200U);
                if (remaining > size && remaining - size < 9U) size -= 9U;
                const auto prefix = std::to_string(line) + " REM ";
                file.payload.insert(file.payload.end(), prefix.begin(), prefix.end());
                file.payload.insert(file.payload.end(), size - prefix.size() - 1U, 'A');
                file.payload.push_back(13U); line += 10U;
            }
            roundtrip(file);
        }
        file.payload = {'1','0',' ','P','R','I','N','T',' ','"',0xC1,0xBA,0xBF,0x8A,0x82,0x8B,'"',13}; roundtrip(file);
        require(!formats::decode_native_program_blocks({}).file, "Missing blocks accepted");
        std::vector<std::vector<std::uint8_t>> blocks{std::vector<std::uint8_t>(32U),{0,0}};
        blocks[0][0] = 2U; blocks[0][19] = 2U; blocks[0][20] = 0x28U;
        require(formats::decode_native_program_blocks(blocks).file.has_value(), "Empty binary blocks rejected");
        blocks.push_back({0,0}); require(!formats::decode_native_program_blocks(blocks).file, "Extra block accepted");
        blocks.pop_back(); blocks[1].pop_back(); require(!formats::decode_native_program_blocks(blocks).file, "Truncated body accepted");
        core::SyntheticMachine host; auto& machine = host.execution();
        machine.initialize(isa::CpuProfile::hd6301v1, 0x1000U, 0x01FFU);
        host.bus().poke8(0x1000U, 0x01U); host.bus().poke8(0x1001U, 0x01U);
        Observer first, second;
        require(machine.add_observer(&first), "First observer rejected");
        debugger::Debugger debugger; require(debugger.attach(machine), "Debugger cannot share capture events");
        require(machine.add_observer(&second) && machine.add_observer(&first), "Observer coexistence failed");
        require(debugger.step().step.fault == core::CpuFault::none, "Observed step failed");
        require(first.steps == 1U && second.steps == 1U && first.accesses == second.accesses && first.accesses > 0U, "Observers did not receive identical events exactly once");
        machine.remove_observer(first);
        require(first.detached == 1U && debugger.machine() == &machine && machine.has_observer(&second), "Removal detached another observer");
        require(debugger.step().step.fault == core::CpuFault::none && second.steps == 2U && first.steps == 1U, "Remaining observer or debugger stopped receiving events");
        machine.clear_observers(); require(second.detached == 1U && debugger.machine() == nullptr, "Clearing observers left stale links");
        core::Jr800Machine unbooted; runtime::ProgramSaveCapture capture(unbooted);
        require(capture.state() == runtime::ProgramSaveState::unavailable && capture.files().empty(), "Unknown ROM capture enabled");
        jr800_machine* api = jr800_machine_create(); require(api != nullptr, "C API creation failed");
        jr800_program_saves_state saves{}; jr800_program_info info{}; std::uint32_t size = 0;
        require(jr800_machine_get_program_saves(api,&saves) == JR800_STATUS_OK && saves.state == 0U && saves.count == 0U, "Synthetic save state incorrect");
        require(jr800_machine_get_saved_program_info(api,0U,&info) == JR800_STATUS_NOT_FOUND, "Missing save metadata accepted");
        require(jr800_machine_export_saved_program(api,0U,3U,nullptr,0U,&size) == JR800_STATUS_INVALID_ARGUMENT, "Unknown export format accepted");
        require(jr800_machine_export_saved_program(api,0U,1U,nullptr,0U,&size) == JR800_STATUS_NOT_FOUND, "Missing save exported");
        require(jr800_machine_clear_program_saves(api) == JR800_STATUS_OK, "Empty clear failed");
        jr800_machine_destroy(api);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
