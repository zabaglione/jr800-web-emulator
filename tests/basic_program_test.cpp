// SPDX-License-Identifier: MIT
#include "jr800/formats/basic_program.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/linked_error.hpp"
#include "jr800/core/jr800_machine.hpp"
#include "jr800/debugger/debugger.hpp"
#include "jr800/runtime/application_loader.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void reject(F&& action) {
    try { action(); } catch (const jr800::formats::linked::Error&) { return; }
    throw std::runtime_error("Invalid BASIC input accepted");
}
}
int main() {
    using namespace jr800;
    using formats::jr8app::ProgramKind;
    try {
        formats::jr8app::Application text;
        text.target_profile = "hd6301v1";
        text.kind = ProgramKind::basic_text;
        text.name = {'T', 0xB6U, 0x80U};
        text.basic_data = {'1','0',' ','P','R','I','N','T',' ','"',0x80U,0xB6U,'"',13};
        text.integrity_sha256 = formats::jr8app::compute_integrity(text);
        const auto encoded = formats::jr8app::write(text);
        require(formats::jr8app::read(encoded) == text, "BASIC bytes did not survive J8A round trip");
        require(encoded[9] == 1U, "J8A version must be 1");
        auto unsupported = encoded; unsupported[9] = 2U;
        reject([&]{static_cast<void>(formats::jr8app::read(unsupported));});
        auto tampered = encoded; tampered[12] = 3U;
        reject([&]{static_cast<void>(formats::jr8app::read(tampered));});
        tampered = encoded; tampered[26] ^= 1U;
        reject([&]{static_cast<void>(formats::jr8app::read(tampered));});
        for (std::size_t n = 0; n < encoded.size(); ++n) {
            reject([&]{static_cast<void>(formats::jr8app::read(std::span(encoded).first(n)));});
        }
        const std::vector<std::vector<std::uint8_t>> bad_text = {
            {'R','U','N',13}, {'1','0',13}, {'0',' ','E','N','D',13},
            {'1','0',' ','E','N','D'}, {'1','0',' ',0U,13},
            {'1','0',' ','E','N','D',13,'1','0',' ','E','N','D',13},
        };
        for (const auto& bytes : bad_text) reject([&]{static_cast<void>(formats::basic_text_line_numbers(bytes));});
        require(formats::basic_text_line_numbers({}).empty(), "Empty BASIC text refused");
        const std::vector<std::uint8_t> empty{0,0};
        require(formats::basic_binary_line_numbers(empty).empty(), "Empty BASIC binary refused");
        // Project-authored native line records; no ROM table or instruction bytes.
        const std::vector<std::uint8_t> native{0,8,0,10,'A',':','B',0,0,0};
        require(formats::basic_binary_line_numbers(native) == std::vector<std::uint16_t>{10}, "Native line lengths decoded incorrectly");
        for (const auto& bytes : std::vector<std::vector<std::uint8_t>>{{0},{0,0,0},{0,9,0,10,'A',0,0,0},{0,4,0,10,0,0}}) {
            reject([&]{static_cast<void>(formats::basic_binary_line_numbers(bytes));});
        }
        auto invalid = text; invalid.entry_point = 0x2800U;
        reject([&]{static_cast<void>(formats::jr8app::compute_integrity(invalid));});

        core::Jr800ExperimentalMachineConfiguration configuration;
        configuration.internal_ram = core::Jr800ExperimentalInternalRamConfiguration{0U};
        configuration.memory = core::Jr800ExperimentalMemoryConfiguration{0U, 0U};
        configuration.calendar = core::Jr800ExperimentalCalendarConfiguration{};
        core::Jr800Machine machine(configuration);
        std::array<std::uint8_t,32768> rom{};
        rom[0] = 0x01U; rom[1] = 0x20U; rom[2] = 0xFDU;
        rom[32766] = 0x80U;
        require(machine.load_logical_rom(rom) == core::Jr800MemoryStatus::ok, "Synthetic ROM failed");
        require(machine.initialize_from_reset_entry().succeeded(), "Reset failed");
        machine.set_port1_pin_state(255U,255U);
        machine.set_port2_pin_state(30U,31U);
        require(machine.set_calendar_datetime({2026,9,5,12,30,0}) == core::Jr800CalendarOperationStatus::ok, "Clock failed");
        debugger::Debugger debugger;
        require(debugger.attach(machine.execution()), "Debugger attachment failed");
        auto candidate = machine.clone();
        require(!candidate->execution().has_observers() && machine.execution().has_observers(), "Clone copied an observer");
        const auto before = machine.execution().cpu().state();
        require(candidate->host_fill_ram(0x2000U,16U,0x42U) == core::Jr800MemoryStatus::ok, "Candidate RAM write failed");
        require(machine.execution().inspect8(0x2000U).value == 0U, "Candidate mutated original RAM");
        require(formats::jr8app::read(encoded) == text, "Input changed");
        require(runtime::load_application(machine,text,false) == runtime::LoadApplicationResult::unsupported_basic_rom, "Unrecognized ROM accepted");
        require(machine.execution().cpu().state() == before, "Failed import changed CPU");
        machine.copy_state_from(*candidate);
        require(machine.execution().has_observers(), "Commit detached debugger");
        for (unsigned n = 0; n < 100U; ++n) {
            require(machine.execution().step_instruction().fault == core::CpuFault::none, "Committed machine failed");
            require(candidate->execution().step_instruction().fault == core::CpuFault::none, "Candidate failed");
            require(machine.execution().cpu().state() == candidate->execution().cpu().state(), "Clone lost deterministic execution");
        }
        for (unsigned address = 0; address < 65536U; ++address) {
            const auto a = machine.execution().inspect8(static_cast<std::uint16_t>(address));
            const auto b = candidate->execution().inspect8(static_cast<std::uint16_t>(address));
            require(a.fault == b.fault && a.value == b.value, "Clone lost bus/device state");
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
