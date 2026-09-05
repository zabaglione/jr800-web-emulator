// SPDX-License-Identifier: MIT
#include "basic_program_loader.hpp"
#include "basic_rom.hpp"
#include "jr800/formats/basic_program.hpp"
#include "jr800/formats/linked_error.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace jr800::runtime {
namespace {
using core::Jr800Machine;
using formats::jr8app::ProgramKind;
constexpr std::uint16_t line_return = 0x80E3U;
constexpr std::uint16_t line_buffer = 0x2600U;
constexpr std::uint16_t tape_header_service = 0x2227U;
constexpr std::uint16_t tape_data_service = 0x222AU;
constexpr std::uint32_t instruction_limit = 1'000'000U;
// Text LOAD inserts lines through the ROM and costs more as the program grows.
constexpr std::uint32_t load_instruction_limit = 512'000'000U;

using basic_rom::byte;
using basic_rom::word;
using basic_rom::memory;
using basic_rom::supported_rom;
void step(Jr800Machine& machine) {
    if (machine.execution().step_instruction().fault != core::CpuFault::none) {
        throw std::runtime_error("BASIC execution failed");
    }
    if (machine.execution().cpu().state().execution_state != core::CpuExecutionState::active
        && machine.execution().advance_suspended_cycles(256U).bus_fault != core::BusFault::none) {
        throw std::runtime_error("BASIC suspension failed");
    }
}
bool console_wait(const Jr800Machine& machine) {
    const auto& state = machine.execution().cpu().state();
    if (state.execution_state != core::CpuExecutionState::sleeping) return false;
    const auto top = word(machine, 0x00BAU);
    if (top < 0x2011U || top >= 0x8000U || state.sp >= top) return false;
    const auto depth = top - state.sp;
    return (depth == 15 || depth == 17) && word(machine, static_cast<std::uint16_t>(top - 4U)) == line_return;
}
bool wait_for_console(Jr800Machine& machine) {
    for (std::uint32_t i = 0; i < instruction_limit; ++i) {
        if (console_wait(machine)) return true;
        step(machine);
    }
    return false;
}
void write(Jr800Machine& machine, std::uint16_t address, std::span<const std::uint8_t> bytes) {
    if (machine.host_load_ram(address, bytes) != core::Jr800MemoryStatus::ok) {
        throw std::runtime_error("BASIC transfer is outside RAM");
    }
}
bool command(Jr800Machine& machine, std::string_view text) {
    if (!console_wait(machine)) return false;
    for (std::uint8_t key = 0; key < static_cast<std::uint8_t>(core::Jr800Key::count); ++key) {
        static_cast<void>(machine.set_keyboard_key_state(static_cast<core::Jr800Key>(key), false));
    }
    static_cast<void>(machine.set_keyboard_key_state(core::Jr800Key::return_key, true));
    for (std::uint32_t i = 0; i < instruction_limit; ++i) {
        if (machine.execution().cpu().state().pc == line_return) {
            static_cast<void>(machine.set_keyboard_key_state(core::Jr800Key::return_key, false));
            std::vector<std::uint8_t> input(text.begin(), text.end());
            input.push_back(0U);
            write(machine, line_buffer, input);
            return true;
        }
        step(machine);
    }
    return false;
}
void finish_read(Jr800Machine& machine) {
    const auto cc = static_cast<std::uint8_t>((machine.execution().cpu().state().condition_code & 0xF4U) | 0x04U);
    if (!machine.host_return_from_subroutine(0U, cc)) throw std::runtime_error("Invalid BASIC service stack");
}
std::vector<std::uint8_t> current_program(const Jr800Machine& machine) {
    const auto page = byte(machine, 0x0082U);
    if (page >= 8U) throw std::runtime_error("Unsupported BASIC page");
    const auto table = static_cast<std::uint16_t>(0x2348U + page * 2U);
    const auto start = word(machine, table);
    const auto end = word(machine, static_cast<std::uint16_t>(table + 2U));
    if (start < 0x2000U || end <= start || end > 0x8000U) throw std::runtime_error("Invalid BASIC program bounds");
    return memory(machine, start, end - start);
}
}

LoadApplicationResult load_basic_program(Jr800Machine& machine,
    const formats::jr8app::Application& application, bool run_after_load) {
    try {
        if (!supported_rom(machine)) return LoadApplicationResult::unsupported_basic_rom;
        const bool text = application.kind == ProgramKind::basic_text;
        const auto lines = text ? formats::basic_text_line_numbers(application.basic_data)
            : formats::basic_binary_line_numbers(application.basic_data);
        auto candidate = machine.clone();
        if (!wait_for_console(*candidate) || !command(*candidate, "LOAD \"\"")) {
            return LoadApplicationResult::basic_not_ready;
        }
        const auto block_count = text ? (application.basic_data.size() + 1U) / 256U + 1U : 1U;
        std::size_t blocks_read = 0U;
        bool header_read = false;
        bool finished = false;
        // E-424/E-425: let the user's ROM perform LOAD, allocation and tokenization.
        // Only the two tape-transfer services are supplied by the host.
        for (std::uint32_t i = 0; i < load_instruction_limit; ++i) {
            const auto pc = candidate->execution().cpu().state().pc;
            if (pc == tape_header_service) {
                if (header_read) return LoadApplicationResult::basic_load_failed;
                std::array<std::uint8_t, 32> header{};
                header[0] = text ? 3U : 2U;
                std::copy(application.name.begin(), application.name.end(), header.begin() + 1);
                if (!text) {
                    header[18] = static_cast<std::uint8_t>(application.basic_data.size() >> 8U);
                    header[19] = static_cast<std::uint8_t>(application.basic_data.size());
                }
                write(*candidate, 0x22C0U, header);
                finish_read(*candidate);
                header_read = true;
            } else if (pc == tape_data_service) {
                if (!header_read || blocks_read >= block_count) return LoadApplicationResult::basic_load_failed;
                const auto length = word(*candidate, 0x22D2U);
                const auto destination = word(*candidate, 0x22D4U);
                if (text) {
                    if (length != 257U) return LoadApplicationResult::basic_load_failed;
                    std::array<std::uint8_t, 257> block{};
                    block[0] = blocks_read + 1U == block_count ? 1U : 0U;
                    for (std::size_t j = 0; j < 256U; ++j) {
                        const auto source = blocks_read * 256U + j;
                        block[j + 1U] = source < application.basic_data.size() ? application.basic_data[source]
                            : source == application.basic_data.size() ? 0x1AU : 0U;
                    }
                    write(*candidate, destination, block);
                } else {
                    if (length != application.basic_data.size()) return LoadApplicationResult::basic_load_failed;
                    write(*candidate, destination, application.basic_data);
                }
                finish_read(*candidate);
                ++blocks_read;
            } else {
                if (console_wait(*candidate)) { finished = true; break; }
                step(*candidate);
            }
        }
        if (!finished || !header_read || blocks_read != block_count) return LoadApplicationResult::basic_load_failed;
        const auto actual = current_program(*candidate);
        if (formats::basic_binary_line_numbers(actual) != lines
            || (!text && actual != application.basic_data)) return LoadApplicationResult::basic_load_failed;
        if (run_after_load && !command(*candidate, "RUN")) return LoadApplicationResult::basic_load_failed;
        machine.copy_state_from(*candidate);
        return LoadApplicationResult::loaded;
    } catch (const formats::linked::Error&) {
        return LoadApplicationResult::invalid_basic_program;
    } catch (const std::exception&) {
        return LoadApplicationResult::basic_load_failed;
    }
}
}
