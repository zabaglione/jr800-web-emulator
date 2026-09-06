// SPDX-License-Identifier: MIT

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "jr800/assembler/assembler.hpp"
#include "jr800/core/cpu.hpp"
#include "jr800/core/jr800_lcd.hpp"
#include "jr800/linker/linker.hpp"

namespace {

using namespace jr800::core;

constexpr std::uint16_t stack_top = 0x5FFFU;
constexpr std::size_t frame_size = 1536U;
constexpr const char* driver_prefix = R"(
.global entry
.global phase
.global span_count
.extern init
.extern clear
.extern framebuffer
.section .text, code
entry:
    LDS #$5FFF
    JSR init
    JSR clear
    LDAA #1
    STAA phase
    LDX #framebuffer
    LDAA #$5A
fill:
    STAA 0,X
    ADDA #37
    INX
    CPX #framebuffer + 1536
    BNE fill
)";

constexpr const char* full_render = R"(
.extern present
    JSR present
)";

constexpr const char* incremental_render = R"(
.extern present_begin
.extern present_next
    JSR present_begin
next_span:
    INC span_count
    JSR present_next
    BEQ present_done
    ; Input polling may clobber CPU registers between span transfers.
    LDAA #$A5
    LDAB #$5A
    LDX #$1234
    BRA next_span
present_done:
)";

constexpr const char* driver_suffix = R"(
    LDAA #2
    STAA phase
done:
    BRA done
.section .bss, bss
phase:
    .space 1
span_count:
    .space 1
)";

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string read_text(const char* path) {
    std::ifstream stream{path};
    require(stream.is_open(), std::string{"Cannot open "} + path);
    return {std::istreambuf_iterator<char>{stream}, {}};
}

std::uint16_t symbol_address(
    const jr800::linker::Output& output,
    const std::string& name
) {
    for (const auto& symbol : output.debug_info.symbols) {
        if (symbol.name == name) {
            return symbol.value;
        }
    }
    throw std::runtime_error("Missing linked symbol: " + name);
}

// This is a protocol fixture, not a model of JR-800 elapsed time. A pending
// operation reports BUSY for a chosen number of status reads. Only the next
// status read completes it; a premature write fails through the normal bus.
class BusyLcdBus final : public Bus {
public:
    explicit BusyLcdBus(unsigned busy_reads) : busy_reads_{busy_reads} {
        for (std::uint8_t index = 0; index < Jr800Lcd::controller_count; ++index) {
            require(lcd.set_controller_reset_line(index, true), "Reset failed");
            require(lcd.set_controller_reset_line(index, false), "Reset failed");
        }
    }

    BusFault advance_cycles(std::uint32_t) noexcept override {
        return BusFault::none;
    }

    InterruptRequest maskable_interrupt_request() const noexcept override {
        return {};
    }

    BusReadResult read8(std::uint16_t address, AccessKind kind) noexcept override {
        const auto decoded = decode_jr800_lcd_address(address);
        if (!decoded.handled) {
            notify_read(address, memory[address], kind);
            return {BusFault::none, memory[address]};
        }
        if (!decoded.selected()
            || decoded.selection->target != Jr800LcdRegister::control_status) {
            return {BusFault::unsupported_access, std::nullopt};
        }
        const auto index = decoded.selection->controller_index;
        if (pending_[index]) {
            if (remaining_[index] > 0U) {
                --remaining_[index];
                ++busy_status_reads[index];
            } else if (!complete(index)) {
                return {BusFault::device_state_unsupported, std::nullopt};
            }
        }
        const auto result = lcd.read8(address);
        if (!result.succeeded() || (result.known_mask & 0x80U) == 0U) {
            return {BusFault::device_state_unknown, std::nullopt};
        }
        // Only DB7 is part of the polling contract. Other pending status bits
        // can be unknown in Hd44102 and retain their deterministic test value.
        notify_read(address, result.value, kind);
        return {BusFault::none, result.value};
    }

    BusDiscardedReadResult read8_discard(std::uint16_t address) noexcept override {
        return {read8(address, AccessKind::data_read).fault};
    }

    BusReadResult inspect8(std::uint16_t address) const noexcept override {
        if (decode_jr800_lcd_address(address).handled) {
            return {BusFault::unsupported_access, std::nullopt};
        }
        return {BusFault::none, memory[address]};
    }

    BusWriteResult write8(std::uint16_t address, std::uint8_t value) noexcept override {
        if (!decode_jr800_lcd_address(address).handled) {
            const auto previous = memory[address];
            memory[address] = value;
            notify_write(address, value, previous);
            return {BusFault::none, previous, true};
        }
        const auto result = lcd.write8(address, value);
        if (!result.succeeded()) {
            if (result.status == Jr800LcdAccessStatus::busy) {
                ++rejected_busy_writes;
            }
            return {BusFault::device_state_unsupported, 0U, false};
        }
        const auto index = result.selection->controller_index;
        pending_[index] = true;
        remaining_[index] = busy_reads_;
        ++accepted_writes[index];
        if (busy_reads_ == 0U && !complete(index)) {
            return {BusFault::device_state_unsupported, 0U, false};
        }
        notify_write(address, value, std::nullopt);
        return {};
    }

    void settle_last_writes() {
        // present waits before writes; its last write may still be pending
        // when it returns. Complete only those final writes for pixel checks.
        for (std::uint8_t index = 0; index < Jr800Lcd::controller_count; ++index) {
            if (pending_[index]) {
                require(complete(index), "Final LCD completion failed");
            }
        }
    }

    std::array<std::uint8_t, 65536U> memory{};
    Jr800Lcd lcd;
    std::array<unsigned, Jr800Lcd::controller_count> busy_status_reads{};
    std::array<unsigned, Jr800Lcd::controller_count> accepted_writes{};
    unsigned rejected_busy_writes{};

private:
    bool complete(std::uint8_t index) noexcept {
        if (lcd.complete_busy_period(index) != Jr800LcdAccessStatus::ok) {
            return false;
        }
        pending_[index] = false;
        return true;
    }

    unsigned busy_reads_{};
    std::array<unsigned, Jr800Lcd::controller_count> remaining_{};
    std::array<bool, Jr800Lcd::controller_count> pending_{};
};

void run_case(
    const jr800::linker::Output& output,
    unsigned busy_reads,
    bool initially_busy,
    bool incremental
) {
    BusyLcdBus bus{busy_reads};
    for (const auto& segment : output.application.segments) {
        require(
            static_cast<std::size_t>(segment.address) + segment.logical_size
                <= bus.memory.size(),
            "Linked segment exceeds RAM"
        );
        std::copy(segment.data.begin(), segment.data.end(),
                  bus.memory.begin() + segment.address);
    }
    const auto framebuffer = symbol_address(output, "framebuffer");
    const auto phase = symbol_address(output, "phase");
    std::fill_n(bus.memory.begin() + framebuffer, frame_size, 0xA5U);
    if (initially_busy) {
        for (unsigned index = 0; index < Jr800Lcd::controller_count; ++index) {
            const auto address = static_cast<std::uint16_t>(0x0A00U | (1U << index));
            require(bus.write8(address, 0x3BU).succeeded(), "Initial operation failed");
        }
    }
    Cpu cpu;
    cpu.initialize(jr800::isa::CpuProfile::hd6301v1,
                   output.application.entry_point, stack_top);
    bool checked_clear = false;
    for (unsigned step = 0; step < 1000000U && bus.memory[phase] != 2U; ++step) {
        const auto result = cpu.step_instruction(bus);
        require(result.succeeded(),
                "CPU failed at PC " + std::to_string(result.pc_before)
                    + ", bus address " + std::to_string(result.fault_address)
                    + ", rejected BUSY writes "
                    + std::to_string(bus.rejected_busy_writes));
        if (bus.memory[phase] == 1U && !checked_clear) {
            require(cpu.state().sp == stack_top, "init/clear unbalanced SP");
            require(std::all_of(bus.memory.begin() + framebuffer,
                                bus.memory.begin() + framebuffer + frame_size,
                                [](auto value) { return value == 0U; }),
                    "clear did not erase the entire framebuffer");
            checked_clear = true;
        }
    }
    require(checked_clear && bus.memory[phase] == 2U, "Driver did not finish");
    require(cpu.state().sp == stack_top, "present unbalanced SP");
    require(bus.memory[symbol_address(output, "span_count")]
                == (incremental ? 32U : 0U),
            "Incremental transfer did not finish after exactly 32 spans");
    require(bus.rejected_busy_writes == 0U, "Write occurred while BUSY");
    bus.settle_last_writes();
    for (std::size_t index = 0; index < frame_size; ++index) {
        require(bus.memory[framebuffer + index]
                    == static_cast<std::uint8_t>(0x5AU + index * 37U),
                "present changed the framebuffer at " + std::to_string(index));
    }
    for (std::size_t row = 0; row < Jr800Lcd::panel_height; ++row) {
        for (std::size_t column = 0; column < Jr800Lcd::panel_width; ++column) {
            const auto offset = (row / 8U) * Jr800Lcd::panel_width + column;
            const bool expected = (bus.memory[framebuffer + offset]
                & (1U << (row % 8U))) != 0U;
            require(bus.lcd.display_panel_dot(column, row) == expected,
                    "LCD pixel mismatch at " + std::to_string(column)
                        + "," + std::to_string(row));
        }
    }
    for (std::uint8_t controller = 0; controller < Jr800Lcd::controller_count;
         ++controller) {
        require(bus.accepted_writes[controller] > 200U, "Controller was skipped");
        require(bus.busy_status_reads[controller]
                    == busy_reads * (bus.accepted_writes[controller] - 1U),
                "Polling did not cover every pending controller operation");
        for (std::uint8_t page = 0; page < 4U; ++page) {
            for (std::uint8_t column = 0; column < 50U; ++column) {
                require(bus.lcd.display_ram_value(controller, page, column).has_value(),
                        "init left LCD RAM uninitialized");
            }
        }
    }
    std::cout << "PASS busy_reads=" << busy_reads
              << " initially_busy=" << initially_busy
              << " incremental=" << incremental
              << " pixels=12288 framebuffer=1536 SP=24575\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 4 || (argc == 5 && std::string{argv[4]} == "incremental"),
                "Usage: lcd_sample_busy_test display.s font.s memory.j8l [incremental]");
        const bool incremental = argc == 5;
        const std::string driver = std::string{driver_prefix}
            + (incremental ? incremental_render : full_render) + driver_suffix;
        std::vector<jr800::linker::InputObject> objects;
        const std::array sources{
            jr800::assembler::Source{"lcd-busy-driver.s", driver},
            jr800::assembler::Source{argv[1], read_text(argv[1])},
            jr800::assembler::Source{argv[2], read_text(argv[2])},
        };
        for (const auto& source : sources) {
            auto result = jr800::assembler::assemble(source, {"hd6301v1", "test"});
            for (const auto& diagnostic : result.diagnostics) {
                std::cerr << diagnostic.path << ':' << diagnostic.line << ": "
                          << diagnostic.message << '\n';
            }
            require(result.succeeded(), "Assembly failed: " + source.logical_path);
            objects.push_back({source.logical_path, std::move(result.output->object)});
        }
        const auto script = jr800::linker::parse_script({argv[3], read_text(argv[3])});
        require(script.succeeded(), "LCD memory script failed to parse");
        const auto linked = jr800::linker::link_objects(objects, *script.script, {"test"});
        for (const auto& diagnostic : linked.diagnostics) {
            std::cerr << diagnostic.message << '\n';
        }
        require(linked.succeeded(), "LCD driver failed to link");
        bool passed = true;
        for (const unsigned busy_reads : {0U, 1U, 8U}) {
            for (const bool initially_busy : {false, true}) {
                try {
                    run_case(*linked.output, busy_reads, initially_busy, incremental);
                } catch (const std::exception& error) {
                    std::cerr << "FAIL busy_reads=" << busy_reads
                              << " initially_busy=" << initially_busy << ": "
                              << error.what() << '\n';
                    passed = false;
                }
            }
        }
        return passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
