// SPDX-License-Identifier: MIT
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "jr800/assembler/assembler.hpp"
#include "jr800/core/cpu.hpp"
#include "jr800/core/jr800_bus.hpp"
#include "jr800/linker/linker.hpp"
using namespace jr800::core;
void require(bool b,const std::string& m){if(!b)throw std::runtime_error(m);}
int main(int argc,char** argv){try {
 require(argc==2,"Usage: game_input_test source-root");
 const std::string root=argv[1];
 const std::string driver=R"(
.global entry
.global counts
.global gate_request
.global basic_check_break
.extern input_init
.extern input_poll
.extern input_take
.extern input_gate
.extern input_event
.section .text, code
entry:
    LDS #$5FFF
    JSR input_init
loop:
    TST gate_request
    BEQ scan
    CLR gate_request
    JSR input_gate
scan:
    JSR input_poll
    JSR input_take
    LDAA input_event
    LDX #counts
    LDAB #6
record:
    LSRA
    BCC next
    INC 0,X
next:
    INX
    DECB
    BNE record
    BRA loop
basic_check_break:
    RTS
.section .bss, bss
counts: .space 6
gate_request: .space 1
)";
 std::ifstream f(root+"/sdk/lib/game-input.s");require(f.good(),"Input source");
 const std::string source{std::istreambuf_iterator<char>(f),{}};
 std::vector<jr800::linker::InputObject> objects;
 for(const auto& s:std::array<jr800::assembler::Source,2>{{{"driver.s",driver},{"input.s",source}}}){
  auto a=jr800::assembler::assemble(s,{"hd6301v1","test"});require(a.succeeded(),"Assembly");objects.push_back({s.logical_path,std::move(a.output->object)});
 }
 auto script=jr800::linker::parse_script({"test.j8l","target hd6301v1\nentry entry\nregion RAM $2800 $1000\nplace .text RAM\nplace .bss RAM\n"});
 auto result=jr800::linker::link_objects(objects,*script.script,{"test"});require(result.succeeded(),"Link");
 auto sym=[&](const std::string& n){for(const auto& s:result.output->debug_info.symbols)if(s.name==n)return s.value;throw std::runtime_error(n);};
 Jr800ExperimentalMachineConfiguration config;config.internal_ram=Jr800ExperimentalInternalRamConfiguration{0};config.memory=Jr800ExperimentalMemoryConfiguration{0,std::nullopt};
 Jr800Bus bus(config);bus.reset_cpu_devices();
 for(unsigned a=0xc00;a<0x1000;++a)require(bus.set_keyboard_bus_response(a,255,true),"Keyboard window");
 for(const auto& s:result.output->application.segments)require((s.kind==jr800::formats::jr8app::SegmentKind::zero_fill?bus.host_fill_ram(s.address,s.logical_size,0):bus.host_load_ram(s.address,s.data))==Jr800MemoryStatus::ok,"Load");
 Cpu cpu;cpu.initialize(jr800::isa::CpuProfile::hd6301v1,result.output->application.entry_point,0x5fff);
 auto run=[&](std::uint64_t cycles){const auto end=cpu.state().cycle_count+cycles;while(cpu.state().cycle_count<end){bus.set_instruction_context(cpu.state().cycle_count,cpu.state().pc);require(cpu.step_instruction(bus).succeeded(),"CPU");}};
 auto key=[&](Jr800Key k,bool b){require(bus.set_keyboard_key_state(k,b),"Key");};
 auto count=[&](unsigned bit){auto r=bus.inspect8(sym("counts")+bit);require(r.succeeded(),"Count");return *r.value;};
 run(2000);
 const std::array<Jr800Key,4> digits{Jr800Key::keypad_8,Jr800Key::keypad_2,Jr800Key::keypad_4,Jr800Key::keypad_6};
 const std::array<Jr800Key,4> letters{Jr800Key::letter_w,Jr800Key::letter_s,Jr800Key::letter_a,Jr800Key::letter_d};
 for(unsigned i=0;i<4;++i){
  key(digits[i],true);key(letters[i],true);run(2000);require(count(i)==1,"Merged alias press");
  key(digits[i],false);run(2000);require(count(i)==1,"Alias release has no edge");
  key(letters[i],false);run(2000);
 }
 key(Jr800Key::keypad_6,true);run(360000);require(count(3)==2,"No early repeat");
 run(70000);require(count(3)==3,"Initial 20-tick repeat including timer wrap");
 run(120000);require(count(3)==4,"Six-tick repeat");key(Jr800Key::keypad_6,false);run(2000);
 key(Jr800Key::space,true);run(600000);require(count(4)==1,"Confirm does not repeat");
 const std::array<std::uint8_t,1> gate{1};require(bus.host_load_ram(sym("gate_request"),gate)==Jr800MemoryStatus::ok,"Gate request");run(2000);
 key(Jr800Key::letter_w,true);run(4000);require(count(0)==1,"Transition gate blocks held keys");
 key(Jr800Key::space,false);key(Jr800Key::letter_w,false);run(2000);
 key(Jr800Key::space,true);run(2000);key(Jr800Key::space,false);run(2000);require(count(4)==2,"New confirm after release");
 key(Jr800Key::return_key,true);run(600000);require(count(5)==1,"Cancel does not repeat");key(Jr800Key::return_key,false);run(2000);
 key(Jr800Key::keypad_8,true);key(Jr800Key::letter_s,true);run(2000);require(count(0)==1&&count(1)==1,"Opposite directions neutral");
 std::cout<<"PASS: aliases, short pulses, 20/6 tick repeats, timer wrap, confirm/cancel edges, release gate, opposite directions\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
