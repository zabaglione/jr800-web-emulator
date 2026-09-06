// SPDX-License-Identifier: MIT
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "jr800/assembler/assembler.hpp"
#include "jr800/core/cpu.hpp"
#include "jr800/linker/linker.hpp"

using namespace jr800::core;
namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::string read_text(const char* path) {
    std::ifstream input{path};
    require(input.is_open(), "Cannot read test source");
    return {std::istreambuf_iterator<char>{input}, {}};
}
std::uint16_t symbol(const jr800::linker::Output& output, const std::string& name) {
    for (const auto& value : output.debug_info.symbols)
        if (value.name == name) return value.value;
    throw std::runtime_error("Missing symbol: " + name);
}
class Observer final : public BusObserver {
public:
    std::vector<std::uint16_t> services;
    bool touched_power = false;
    void on_bus_access(const BusAccessEvent& event) noexcept override {
        if (event.address == 0x0DFFU || event.address < 0x0020U) touched_power = true;
        if (event.kind == AccessKind::instruction_fetch
            && (event.address == 0xFEECU || event.address == 0xF127U))
            services.push_back(event.address);
    }
};
void run_to(Cpu& cpu, RamBus& bus, std::uint16_t pc) {
    for (unsigned i = 0; i < 10000U; ++i) {
        if (cpu.state().pc == pc) return;
        require(cpu.step_instruction(bus).succeeded(), "CPU failed in BASIC context code");
    }
    throw std::runtime_error("BASIC context code did not finish");
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Usage: lcd_sample_basic_return_test basic.s memory.j8l");
        const std::string harness = R"(
.global entry
.global poll
.global after_released
.extern basic_save
.extern basic_check_break
.section .text, code
entry:
    LDS #$5FFF
    JSR basic_save
poll:
    LDAA #$11
    LDAB #$22
    LDX #$3344
    JSR nested_poll
after_released:
    BRA poll
nested_poll:
    JSR basic_check_break
    RTS
)";
        std::vector<jr800::linker::InputObject> objects;
        for (const auto& source : std::array{
            jr800::assembler::Source{"basic-context-harness.s", harness},
            jr800::assembler::Source{argv[1], read_text(argv[1])}}) {
            auto assembled = jr800::assembler::assemble(source, {"hd6301v1", "test"});
            require(assembled.succeeded(), "Context fixture assembly failed");
            objects.push_back({source.logical_path, std::move(assembled.output->object)});
        }
        const auto script = jr800::linker::parse_script({argv[2], read_text(argv[2])});
        require(script.succeeded(), "Context fixture memory script failed");
        const auto linked = jr800::linker::link_objects(objects, *script.script, {"test"});
        require(linked.succeeded(), "Context fixture link failed");
        RamBus bus;
        for (const auto& segment : linked.output->application.segments)
            require(bus.load(segment.address, segment.data), "Context fixture load failed");
        // Original test stubs mark service order; they contain no firmware code.
        require(bus.load(0xFEECU, std::array<std::uint8_t, 4>{0x7C,0x25,0x00,0x39}), "Stub load failed");
        require(bus.load(0xF127U, std::array<std::uint8_t, 5>{0x7C,0x25,0x01,0x0E,0x39}), "Stub load failed");
        std::array<std::uint8_t, 128> original{};
        for (std::size_t i = 0; i < original.size(); ++i)
            original[i] = static_cast<std::uint8_t>(i * 37U + 11U);
        require(bus.load(0x0080U, original), "Context seed failed");
        bus.poke8(0x0F7FU, 0xFFU);
        bus.poke8(0x0DFFU, 0x7FU); // OFF must not be inspected by this module.
        Observer observer;
        require(bus.set_observer(&observer), "Cannot observe context fixture");
        Cpu cpu;
        cpu.initialize(jr800::isa::CpuProfile::hd6301v1,
                       linked.output->application.entry_point, 0x5FFFU);
        run_to(cpu, bus, symbol(*linked.output, "poll"));
        const auto saved = symbol(*linked.output, "basic_context");
        for (std::uint16_t i = 0; i < 128U; ++i)
            require(bus.peek8(saved + i) == original[i], "Incomplete BASIC context capture");
        require(bus.fill(0x0080U, 128U, 0xA5U), "Cannot overwrite context");
        for (const std::uint8_t released : {0xFFU, 0xBFU}) {
            bus.poke8(0x0F7FU, released);
            run_to(cpu, bus, symbol(*linked.output, "after_released"));
            const auto& state = cpu.state();
            require(state.sp == 0x5FFFU && state.a == 0x11U && state.b == 0x22U
                    && state.x == 0x3344U, "Released BREAK changed caller registers/stack");
            require(observer.services.empty(), "A non-BREAK key invoked BASIC services");
            require(cpu.step_instruction(bus).succeeded(), "Cannot leave polling boundary");
        }
        bus.poke8(0x0F7FU, 0x7FU);
        run_to(cpu, bus, 0x805EU);
        for (std::uint16_t i = 0; i < 128U; ++i)
            require(bus.peek8(0x0080U + i) == original[i], "Incomplete BASIC context restore");
        require(cpu.state().sp == 0x5FFFU, "Exit retained nested application stack frames");
        require(observer.services == std::vector<std::uint16_t>{0xFEECU,0xF127U},
                "BASIC display/input services were not called in order");
        require(!observer.touched_power, "BREAK context module accessed power control/input");
        std::cout << "BASIC context capture, BREAK exit and service boundary passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
