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
void check(bool v, const std::string& s) { if (!v) throw std::runtime_error(s); }
struct Trace : BusObserver {
    std::vector<BusAccessEvent> writes;
    void on_bus_access(const BusAccessEvent& e) noexcept override {
        if (e.kind == AccessKind::data_write && e.address == 2) writes.push_back(e);
    }
};
struct Fixture {
    RamBus bus;
    Cpu cpu;
    jr800::linker::Output linked;
    explicit Fixture(const std::string& root) {
        std::vector<jr800::linker::InputObject> objects;
        auto add = [&](const std::string& path, const std::string& text) {
            auto a = jr800::assembler::assemble({path,text},{"hd6301v1","test"});
            for (const auto& d:a.diagnostics) std::cerr<<d.message<<'\n';
            check(a.succeeded(),"assembly: "+path);
            objects.push_back({path,std::move(a.output->object)});
        };
        add("fixture.s", ".global entry\n.global framebuffer\n.section .text, code\nentry: BRA entry\n.section .frame, bss\nframebuffer: .space 1536\n");
        for (const auto* name:{"keys","sprite","scroll","sound"}) {
            const auto path=root+"/sdk/lib/"+name+".s";
            std::ifstream f(path); check(f.is_open(),path);
            add(path,{std::istreambuf_iterator<char>(f),{}});
        }
        auto script=jr800::linker::parse_script({"test.j8l","target hd6301v1\nentry entry\nregion CODE $2800 $1800\nregion FRAME $4000 $0600\nregion STATE $4B00 $0100\nplace .text CODE\nplace .frame FRAME\nplace .bss STATE\n"});
        check(script.succeeded(),"script");
        auto result=jr800::linker::link_objects(objects,*script.script,{"test"});
        for(const auto& d:result.diagnostics) std::cerr<<d.message<<'\n';
        check(result.succeeded(),"link"); linked=std::move(*result.output);
        for(const auto& s:linked.application.segments) check(bus.load(s.address,s.data),"load");
    }
    std::uint16_t sym(const std::string& name) const {
        for(const auto& s:linked.debug_info.symbols) if(s.name==name) return s.value;
        throw std::runtime_error("symbol: "+name);
    }
    void put(const std::string& name,std::uint8_t value) { bus.poke8(sym(name),value); }
    std::uint64_t call(const std::string& name,std::uint16_t x=0,std::uint8_t a=0,std::uint8_t b=0) {
        const auto address=sym(name);
        const std::array<std::uint8_t,10> jsr{0xCE,static_cast<std::uint8_t>(x>>8),static_cast<std::uint8_t>(x),0x86,a,0xC6,b,0xBD,static_cast<std::uint8_t>(address>>8),static_cast<std::uint8_t>(address)};
        check(bus.load(0x2000,jsr),"driver");
        cpu.initialize(jr800::isa::CpuProfile::hd6301v1,0x2000,0x5fff);
        for(unsigned i=0;i<2000000 && cpu.state().pc!=0x200A;i++) {
            bus.set_instruction_context(cpu.state().cycle_count,cpu.state().pc);
            const auto step=cpu.step_instruction(bus);
            check(step.succeeded(),"CPU fault at "+std::to_string(step.pc_before));
        }
        check(cpu.state().pc==0x200A && cpu.state().sp==0x5fff,"return/stack");
        return cpu.state().cycle_count;
    }
};
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Usage: sdk_library_test <root>"); Fixture f(argv[1]);
        // Every old/new mask pair, including chords, held input and release.
        for(unsigned old=0;old<256;old++) for(unsigned next=0;next<256;next++) {
            f.bus.poke8(0x4700,old); f.call("keys_update",0x4700,next);
            check(f.bus.peek8(0x4700)==next && f.bus.peek8(0x4701)==(next & ~old & 255)
                && f.bus.peek8(0x4702)==(old & ~next & 255),"key edges");
        }
        f.call("keys_reset",0x4700);
        check(f.bus.peek8(0x4700)==0 && f.bus.peek8(0x4701)==0 && f.bus.peek8(0x4702)==0,"key reset");
        f.call("keys_latch",0x4700,0x30);
        f.call("keys_latch",0x4700,0x10);
        f.call("keys_latch",0x4700,0x10);
        check(f.bus.peek8(0x4700)==0x10 && f.bus.peek8(0x4701)==0x30 && f.bus.peek8(0x4702)==0x20,"latched edges");
        f.call("keys_ack",0x4700);
        check(f.bus.peek8(0x4700)==0x10 && f.bus.peek8(0x4701)==0 && f.bus.peek8(0x4702)==0,"ack preserves held");
        std::uint64_t sprite_max=0;
        for(unsigned mode:{0U,1U}) for(unsigned y=0;y<65;y++)
        for(unsigned x:{0U,1U,183U,188U,191U,192U,255U}) for(unsigned width:{0U,1U,8U,16U,192U,193U}) {
            check(f.bus.fill(0x3fff,1538,0xA5),"fill");
            std::array<std::uint8_t,1536> expected; expected.fill(0xA5);
            for(unsigned c=0;c<192;c++) f.bus.poke8(0x4800+c,static_cast<std::uint8_t>(c*37+19));
            if(x<192 && y<64 && width>0 && width<=192) {
                for(unsigned c=0;c<width && x+c<192;c++) for(unsigned r=0;r<8 && y+r<64;r++) {
                    if(((c*37+19)&(1U<<r))!=0) {
                        auto& pixel=expected[((y+r)/8)*192+x+c];
                        const auto bit=static_cast<std::uint8_t>(1U<<((y+r)%8));
                        if(mode==0) pixel|=bit; else pixel^=bit;
                    }
                }
            }
            f.put("sprite_mode",mode); f.put("sprite_width",width);
            sprite_max=std::max(sprite_max,f.call("sprite8",0x4800,x,y));
            for(unsigned i=0;i<1536;i++) check(f.bus.peek8(0x4000+i)==expected[i],"sprite pixel");
            check(f.bus.peek8(0x3fff)==0xA5 && f.bus.peek8(0x4600)==0xA5,"sprite bounds");
        }
        // Moving overlapping memory in both directions, with a pixel oracle.
        for(const auto* name:{"scroll_left","scroll_right"}) for(unsigned n=0;n<256;n++) {
            std::array<std::uint8_t,192> original;
            for(unsigned i=0;i<192;i++) {original[i]=static_cast<std::uint8_t>(i*17+3);f.bus.poke8(0x4100+i,original[i]);}
            f.bus.poke8(0x40ff,0xA5);f.bus.poke8(0x41c0,0x5A);
            const auto cycles=f.call(name,0x4100,n);
            for(unsigned i=0;i<192;i++) {
                unsigned expected=original[i];
                if(n>0 && n<192) expected=std::string(name)=="scroll_left" ? (i+n<192?original[i+n]:0) : (i>=n?original[i-n]:0);
                check(f.bus.peek8(0x4100+i)==expected,"scroll data");
            }
            check(f.bus.peek8(0x40ff)==0xA5 && f.bus.peek8(0x41c0)==0x5A,"scroll bounds");
            if(n==1 || n==8) std::cout<<name<<" pixels="<<n<<" cycles="<<cycles<<'\n';
        }
        f.put("sprite_mode",0);f.put("sprite_width",8);
        for(unsigned y:{0U,7U}) {
            std::cout<<"sprite8 width=8 y="<<y<<" cycles="<<f.call("sprite8",0x4800,24,y)<<'\n';
        }
        Trace trace;check(f.bus.set_observer(&trace),"observer");
        for(unsigned baseline:{0U,0xEFU,0xA5U}) for(unsigned n:{0U,1U,2U,340U}) for(unsigned count:{0U,1U,3U,256U}) {
            trace.writes.clear(); f.put("sound_port",baseline);
            f.call("sound_tone",n,count>>8,count&255);
            check(trace.writes.size()==(n==0?0:count*2),"tone count");
            for(unsigned i=0;i<trace.writes.size();i++) {
                check(trace.writes[i].value==((baseline&0xEF)|((i%2==0)?16:0)),"tone shadow");
                if(i>0) {
                    const auto delta=trace.writes[i].instruction_cycle-trace.writes[i-1].instruction_cycle;
                    check(delta==4*n+(i%2==1?12:30),"tone cycle: "+std::to_string(delta)+" n="+std::to_string(n)+" i="+std::to_string(i));
                }
            }
        }
        // Two tones with an intervening rest; terminator must not overrun.
        const std::array<std::uint8_t,16> sequence{0,2,0,3, 0,0,0,20, 0,4,0,2, 0,0,0,0};
        check(f.bus.load(0x4900,sequence),"sequence");
        f.put("sound_port",0xA5); trace.writes.clear();
        f.bus.poke8(0,0x56);
        f.call("sound_play",0x4900);
        check(trace.writes.size()==10 && f.bus.peek8(2)==0xA5,"sequence end");
        check(f.bus.peek8(0)==0x56,"library must not replace caller DDR");
        check(trace.writes[6].instruction_cycle-trace.writes[5].instruction_cycle>80,"rest");
        trace.writes.clear();f.call("sound_play",0x490C);
        check(trace.writes.empty(),"empty sequence");
        check(f.bus.set_observer(nullptr),"detach");
        std::cout<<"PASS keys=65536 sprite_max_cycles="<<sprite_max<<" scroll=512 tone=48\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
