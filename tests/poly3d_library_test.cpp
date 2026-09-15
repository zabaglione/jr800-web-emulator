// SPDX-License-Identifier: MIT
// Execute the assembly API, checking pixels against direct intersections.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::string file(const std::string& path) {
    std::ifstream stream(path);
    check(stream.is_open(), path);
    return {std::istreambuf_iterator<char>(stream), {}};
}
struct Fixture {
    RamBus bus;
    Cpu cpu;
    jr800::linker::Output linked;
    explicit Fixture(const std::string& root) {
        std::vector<jr800::linker::InputObject> objects;
        auto add = [&](const std::string& path, const std::string& source) {
            auto result = jr800::assembler::assemble({path, source}, {"hd6301v1", "test"});
            for (const auto& diagnostic : result.diagnostics) std::cerr << diagnostic.message << '\n';
            check(result.succeeded(), "Assembly failed: " + path);
            objects.push_back({path, std::move(result.output->object)});
        };
        add("fixture.s", ".global entry\n.global framebuffer\n.global p3_poll\n.global spans\n.global spans_end\n"
            ".section .text, code\nentry: BRA entry\np3_poll: RTS\n.section .data, data\nspans: .byte 0\nspans_end:\n"
            ".section .frame, bss\nframebuffer: .space 1536\n");
        for (const auto* name : {"poly3d/render.s", "poly3d/models.s", "lcd/dirty.s"}) {
            const auto path = root + "/sdk/lib/" + name;
            add(path, file(path));
        }
        const auto script_path = root + "/sdk/examples/lcd/common/poly3d-memory.j8l";
        auto script = jr800::linker::parse_script({script_path, file(script_path)});
        check(script.succeeded(), "Link script");
        auto result = jr800::linker::link_objects(objects, *script.script, {"test"});
        for (const auto& diagnostic : result.diagnostics) std::cerr << diagnostic.message << '\n';
        check(result.succeeded(), "Link failed");
        linked = std::move(*result.output);
        for (const auto& segment : linked.application.segments) check(bus.load(segment.address, segment.data), "Load");
    }
    std::uint16_t sym(const std::string& name) const {
        for (const auto& symbol : linked.debug_info.symbols) if (symbol.name == name) return symbol.value;
        throw std::runtime_error("Missing symbol: " + name);
    }
    void put(const std::string& name, unsigned value) { bus.poke8(sym(name), static_cast<std::uint8_t>(value)); }
    int byte(unsigned address) const { return bus.peek8(static_cast<std::uint16_t>(address)); }
    int signed_byte(unsigned address) const { const auto value = byte(address); return value < 128 ? value : value - 256; }
    unsigned word(unsigned address) const { return static_cast<unsigned>(byte(address)*256 + byte(address+1)); }
    std::uint64_t call(const std::string& name, std::uint16_t x = 0) {
        const auto address = sym(name);
        const std::array<std::uint8_t, 6> driver{0xCE, static_cast<std::uint8_t>(x>>8), static_cast<std::uint8_t>(x),
            0xBD, static_cast<std::uint8_t>(address>>8), static_cast<std::uint8_t>(address)};
        check(bus.load(0x2000, driver), "Driver");
        cpu.initialize(jr800::isa::CpuProfile::hd6301v1, 0x2000, 0x5fff);
        for (unsigned count = 0; count < 500000 && cpu.state().pc != 0x2006; ++count) {
            bus.set_instruction_context(cpu.state().cycle_count, cpu.state().pc);
            const auto step = cpu.step_instruction(bus);
            check(step.succeeded(), "CPU fault at " + std::to_string(step.pc_before));
        }
        check(cpu.state().pc == 0x2006 && cpu.state().sp == 0x5fff, "Return/stack: " + name);
        return cpu.state().cycle_count;
    }
};
using Vec = std::array<int, 3>;
using Image = std::array<std::uint8_t, 1536>;
struct Mesh { std::vector<Vec> vertices; std::vector<std::array<int, 7>> faces; };
Mesh mesh(const Fixture& f, unsigned address) {
    Mesh result;
    const auto vertices = f.word(address+2), faces = f.word(address+4);
    for (int i = 0; i < f.byte(address); ++i) {
        result.vertices.push_back({f.signed_byte(vertices+i*3), f.signed_byte(vertices+i*3+1), f.signed_byte(vertices+i*3+2)});
    }
    for (int i = 0; i < f.byte(address+1); ++i) {
        result.faces.push_back({f.byte(faces+i*7), f.byte(faces+i*7+1), f.byte(faces+i*7+2),
            f.signed_byte(faces+i*7+3), f.signed_byte(faces+i*7+4), f.signed_byte(faces+i*7+5), f.byte(faces+i*7+6)});
    }
    return result;
}
int product(int a, int b) { return static_cast<int>(std::floor(static_cast<double>(a)*b/128)); }
int sine(int angle) { return static_cast<int>(std::round(127*std::sin((angle&63)*std::acos(-1.0)/32))); }
Vec rotate(Vec v, int yaw, int pitch) {
    const int x = product(v[0], sine(yaw+16)) + product(v[2], sine(yaw));
    const int z = product(v[2], sine(yaw+16)) - product(v[0], sine(yaw));
    return {x, product(v[1], sine(pitch+16))-product(z, sine(pitch)),
            product(v[1], sine(pitch))+product(z, sine(pitch+16))};
}
void pixel(Image& image, int x, int y, bool black) {
    auto& byte = image[(y/8)*192+x];
    const auto mask = static_cast<std::uint8_t>(1U << (y%8));
    byte = static_cast<std::uint8_t>((byte & ~mask) | (black ? mask : 0));
}
void triangle(Image& image, const std::array<Vec, 3>& points, int shade, int mode, int top, int bottom) {
    const auto min_y = std::min({points[0][1], points[1][1], points[2][1]});
    const auto max_y = std::max({points[0][1], points[1][1], points[2][1]});
    const std::array<int, 8> patterns{3, 3, 3, 1, 1, 2, 1, 0};
    for (int y = std::max(top, min_y); y <= std::min(bottom, max_y); ++y) {
        std::vector<int> intersections;
        for (unsigned edge = 0; edge < 3; ++edge) {
            auto a = points[edge], b = points[(edge+1)%3];
            if (a[1] > b[1]) std::swap(a, b);
            if (y < a[1] || y > b[1]) continue;
            if (a[1] == b[1]) intersections.insert(intersections.end(), {a[0], b[0]});
            else intersections.push_back(a[0] + (b[0] < a[0] ? -1 : 1)*
                (std::abs(b[0]-a[0])*(y-a[1])/(b[1]-a[1])));
        }
        if (intersections.empty()) continue;
        const int left = std::max(0, *std::min_element(intersections.begin(), intersections.end()));
        const int right = std::min(191, *std::max_element(intersections.begin(), intersections.end()));
        for (int x = left; x <= right; ++x) {
            const bool outline = y == min_y || y == max_y || x == left || x == right;
            if (mode == 0 || outline) pixel(image, x, y, outline || ((patterns[shade+(y&1)]>>(x&1))&1));
        }
    }
}
unsigned render(Image& image, const Mesh& m, int yaw, int pitch, int scale, int x, int y, int mode, int top, int bottom) {
    std::vector<Vec> projected;
    for (const auto& vertex : m.vertices) {
        const auto v = rotate(vertex, yaw, pitch);
        projected.push_back({x+product(v[0], scale), y-product(v[1], scale), 0});
    }
    unsigned visible = 0;
    for (const auto& face : m.faces) {
        const auto normal = rotate({face[3], face[4], face[5]}, yaw, pitch);
        if (normal[2] <= 0) continue;
        ++visible;
        const auto light = product(normal[0], -48)+product(normal[1], 88)+product(normal[2], 72);
        const int shade = face[6] == 255 ? (light < 0 ? 0 : light < 24 ? 2 : light < 60 ? 4 : 6) : face[6];
        triangle(image, {projected[face[0]], projected[face[1]], projected[face[2]]}, shade, mode, top, bottom);
    }
    return visible;
}
void compare(const Fixture& f, const Image& expected, const std::string& context) {
    for (unsigned i = 0; i < expected.size(); ++i) {
        check(f.byte(f.sym("framebuffer")+i) == expected[i], context + " pixel byte=" + std::to_string(i) +
              " got=" + std::to_string(f.byte(f.sym("framebuffer")+i)) + " expected=" + std::to_string(expected[i]));
    }
    check(f.byte(0x4bff) == 0xA5 && f.byte(0x5200) == 0x5A, "Framebuffer guard");
    check(f.byte(f.sym("p3_state_end")+40) == 0x69, "Workspace guard");
    for (unsigned i = 0; i < 32; ++i) check(f.byte(0x5e00+i) == 0x96, "Stack reserve guard");
}
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Usage: poly3d_library_test <root>");
        Fixture f(argv[1]);
        f.bus.poke8(0x4bff, 0xA5); f.bus.poke8(0x5200, 0x5A);
        f.bus.poke8(f.sym("p3_state_end")+40, 0x69);
        check(f.bus.fill(0x5e00, 32, 0x96), "Stack guard setup");
        f.call("p3_init"); f.call("dirty_reset");
        std::uint64_t maximum = 0;
        unsigned cases = 0;
        for (const auto* name : {"tetra", "octa", "cube"}) {
            const auto model = f.sym(std::string("p3_model_")+name);
            const auto m = mesh(f, model);
            for (int yaw = 0; yaw < 64; ++yaw) {
                const std::vector<int> pitches = yaw%8 == 0 ? std::vector<int>{0, 4, 16, 31, 48, 63} : std::vector<int>{4};
                for (const int pitch : pitches) for (const int scale : {16, 64, 128}) {
                    const std::vector<std::array<int, 2>> origins = yaw%8 == 0 ?
                        std::vector<std::array<int, 2>>{{96,32}, {0,0}, {191,63}, {0,63}, {191,0}} :
                        std::vector<std::array<int, 2>>{{96,32}};
                    for (const auto& origin : origins) for (const int mode : {0, 1}) {
                        f.call("p3_begin"); f.put("p3_yaw", yaw); f.put("p3_pitch", pitch);
                        f.put("p3_scale", scale); f.put("p3_x", origin[0]); f.put("p3_y", origin[1]); f.put("p3_mode", mode);
                        Image expected{};
                        const auto visible = render(expected, m, yaw, pitch, scale, origin[0], origin[1], mode, 0, 63);
                        maximum = std::max(maximum, f.call("p3_draw", model));
                        check(f.byte(f.sym("p3_status")) == 0, "Valid model status");
                        check(f.byte(f.sym("p3_visible_count")) == static_cast<int>(visible), "Culling");
                        compare(f, expected, std::string(name)+" yaw="+std::to_string(yaw)+" pitch="+std::to_string(pitch)+
                                " scale="+std::to_string(scale)+" x="+std::to_string(origin[0])+" y="+std::to_string(origin[1])+" mode="+std::to_string(mode));
                        ++cases;
                    }
                }
            }
        }
        // Near objects must overwrite white dither pixels of farther objects.
        f.call("p3_begin"); f.put("p3_x", 96); f.put("p3_y", 32); f.put("p3_yaw", 9); f.put("p3_pitch", 5);
        f.put("p3_scale", 128); f.put("p3_mode", 0);
        Image expected{};
        for (const auto* name : {"cube", "tetra"}) {
            const auto model = f.sym(std::string("p3_model_")+name);
            render(expected, mesh(f, model), 9, 5, 128, 96, 32, 0, 0, 63);
            f.call("p3_draw", model);
        }
        compare(f, expected, "Opaque object composition");
        // No writes for invalid descriptors or viewport/scale parameters.
        const auto model = f.sym("p3_model_tetra");
        for (const auto& item : std::vector<std::pair<std::string, unsigned>>{
                 {"p3_scale", 0}, {"p3_scale", 129}, {"p3_x", 192}, {"p3_y", 64}, {"p3_mode", 2},
                 {"p3_clip_top", 1}, {"p3_clip_bottom", 64}, {"p3_clip_top", 64}}) {
            f.call("p3_init"); f.put(item.first, item.second);
            f.call("p3_draw", model);
            check(f.byte(f.sym("p3_status")) == 1 && f.byte(f.sym("p3_error")) == 1, "Invalid API status");
            compare(f, expected, "Invalid API must not draw");
        }
        f.call("p3_init");
        for (const unsigned offset : {0U, 1U}) {
            const auto saved = f.byte(model+offset);
            f.bus.poke8(model+offset, offset == 0 ? 9 : 13);
            f.call("p3_draw", model);
            check(f.byte(f.sym("p3_status")) == 1, "Invalid model count");
            compare(f, expected, "Invalid count must not draw");
            f.bus.poke8(model+offset, static_cast<std::uint8_t>(saved));
        }
        for (const auto address : {f.word(model+2), f.word(model+4)}) {
            const auto saved = f.byte(address);
            f.bus.poke8(address, 25);
            f.call("p3_draw", model);
            check(f.byte(f.sym("p3_status")) == 1, "Invalid vertex or index");
            compare(f, expected, "Invalid mesh must not draw");
            f.bus.poke8(address, static_cast<std::uint8_t>(saved));
        }
        // A line is checked over every endpoint direction and LCD seam.
        for (int x : {0, 45, 46, 95, 96, 145, 146, 191}) for (int y : {0, 7, 8, 31, 32, 55, 63}) {
            check(f.bus.fill(f.sym("framebuffer"), 1536, 0), "Clear line fixture");
            f.call("p3_init");
            const std::array<std::uint8_t, 4> line{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y),
                static_cast<std::uint8_t>(191-x), static_cast<std::uint8_t>(63-y)};
            check(f.bus.load(0x2100, line), "Line descriptor");
            expected.fill(0);
            triangle(expected, {{{x,y,0}, {191-x,63-y,0}, {191-x,63-y,0}}}, 0, 0, 0, 63);
            f.call("p3_line", 0x2100);
            compare(f, expected, "Clipped line");
        }
        // Whole-byte line paths must preserve unrelated bits, clip at band
        // boundaries, mark dirty ranges, and erase cleanly on the next frame.
        unsigned line_cases = 0;
        for (int top : {0, 8, 16}) for (int bottom : {31, 55, 63})
            for (int a : {0, 7, 8, 15, 31, 32, 55, 63}) for (int b : {0, 7, 8, 31, 55, 63})
                for (int orientation = 0; orientation < 2; ++orientation) {
                    f.call("p3_init"); f.call("dirty_reset");
                    f.put("p3_clip_top", top); f.put("p3_clip_bottom", bottom);
                    expected.fill(0);
                    std::fill_n(expected.begin(), top*24, 0xA5);
                    std::fill(expected.begin()+(bottom+1)*24, expected.end(), 0x5A);
                    check(f.bus.load(f.sym("framebuffer"), expected), "Clipped framebuffer setup");
                    const auto background = expected;
                    const std::array<std::uint8_t, 4> line = orientation == 0 ?
                        std::array<std::uint8_t, 4>{46, static_cast<std::uint8_t>(a), 46, static_cast<std::uint8_t>(b)} :
                        std::array<std::uint8_t, 4>{static_cast<std::uint8_t>(3*a), static_cast<std::uint8_t>(b),
                            static_cast<std::uint8_t>(3*(63-a)), static_cast<std::uint8_t>(b)};
                    check(f.bus.load(0x2100, line), "Axis line descriptor");
                    triangle(expected, {{{line[0],line[1],0}, {line[2],line[3],0}, {line[2],line[3],0}}}, 0, 0, top, bottom);
                    f.call("p3_line", 0x2100);
                    compare(f, expected, "Axis line and protected HUD");
                    f.call("p3_begin");
                    compare(f, background, "Axis line erasure and protected HUD");
                    ++line_cases;
                }
        std::cout << "PASS mesh_cases=" << cases << " max_draw_cycles=" << maximum
                  << " axis_line_cases=" << line_cases << " workspace_bytes="
                  << f.sym("p3_state_end")-f.sym("p3_x") << " framebuffer_bytes=1536\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
