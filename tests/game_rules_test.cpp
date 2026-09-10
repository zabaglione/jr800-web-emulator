// SPDX-License-Identifier: MIT
// Explicit in-memory unit fixtures supplement the full key-only game replays.
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
#include "jr800/core/cpu.hpp"
#include "jr800/core/jr800_bus.hpp"
#include "jr800/formats/jr8app.hpp"
using namespace jr800::core;
void require(bool b,const std::string& m){if(!b)throw std::runtime_error(m);}
struct Fixture {
 Jr800Bus bus;Cpu cpu;std::map<std::string,unsigned> symbols;
 static Jr800ExperimentalMachineConfiguration config(){Jr800ExperimentalMachineConfiguration c;c.internal_ram=Jr800ExperimentalInternalRamConfiguration{0};c.memory=Jr800ExperimentalMemoryConfiguration{0,std::nullopt};return c;}
 Fixture(const std::string& root,const std::string& game):bus(config()){
  bus.reset_cpu_devices();for(unsigned a=0xc00;a<0x1000;++a)require(bus.set_keyboard_bus_response(a,255,true),"Keys");
  const std::string stem=root+"/build/games/"+game+"/"+game;
  std::ifstream file(stem+".j8a",std::ios::binary);require(file.good(),"Missing game build");
  const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
  const auto app=jr800::formats::jr8app::read(bytes);
  for(const auto& s:app.segments)require((s.kind==jr800::formats::jr8app::SegmentKind::zero_fill?bus.host_fill_ram(s.address,s.logical_size,0):bus.host_load_ram(s.address,s.data))==Jr800MemoryStatus::ok,"RAM");
  std::ifstream sym(stem+".sym");std::string line;std::regex pattern(R"(^ \$([0-9A-F]+) [GL] (\S+))");std::smatch match;
  while(std::getline(sym,line))if(std::regex_search(line,match,pattern))symbols[match[2]]=std::stoul(match[1],nullptr,16);
 }
 void put(const std::string& n,std::uint8_t v,unsigned offset=0){const std::array<std::uint8_t,1> a{v};require(bus.host_load_ram(symbols.at(n)+offset,a)==Jr800MemoryStatus::ok,"Fixture write");}
 std::uint8_t get(const std::string& n,unsigned offset=0){auto r=bus.inspect8(symbols.at(n)+offset);require(r.succeeded(),"Inspect");return *r.value;}
 void fill(const std::string& n,unsigned count,std::uint8_t v){require(bus.host_fill_ram(symbols.at(n),count,v)==Jr800MemoryStatus::ok,"Fill");}
 void call(const std::string& n){const auto addr=symbols.at(n);const std::array<std::uint8_t,3> driver{0xbd,static_cast<std::uint8_t>(addr>>8),static_cast<std::uint8_t>(addr)};require(bus.host_load_ram(0x2000,driver)==Jr800MemoryStatus::ok,"Driver");cpu.initialize(jr800::isa::CpuProfile::hd6301v1,0x2000,0x5fff);unsigned steps=0;while(cpu.state().pc!=0x2003&&steps++<300000){bus.set_instruction_context(cpu.state().cycle_count,cpu.state().pc);auto r=cpu.step_instruction(bus);require(r.succeeded(),"CPU at "+std::to_string(r.pc_before));}require(cpu.state().pc==0x2003&&cpu.state().sp==0x5fff,"Bounded routine and balanced stack");}
};
int main(int argc,char** argv){try{
 require(argc==2,"Usage: game_rules_test root");const std::string root=argv[1];
 {
  Fixture f(root,"mirror-link");f.fill("board",112,0);f.put("phase",2);f.put("source_count",1);f.put("source_cells",37);
  f.put("board",2,35);f.put("board",3,41);f.put("board",2,73);f.put("board",3,67);f.put("board",5,37);f.put("board",4,20);
  f.call("trace_light");require(f.get("lit_count")==0&&f.get("phase")==2,"Closed optical path stops at source");
  for(auto p:{35,41,67,73})require(f.get("visited",p)!=0,"All reflection corners reached");
  // A source record in an open cell exercises the repeated (cell,direction) guard.
  f.put("board",0,37);f.call("trace_light");require(f.get("visited",37)!=0,"Cycle guard returns without hanging");
 }
 {
  const std::array<std::array<unsigned,7>,12> stats{{{1,6,0,0,0,0,0},{1,0,6,0,0,0,0},{0,2,0,0,0,0,0},{2,12,0,0,0,0,0},{2,0,12,0,0,0,0},{1,0,0,5,0,0,0},{1,0,0,0,3,0,0},{0,0,0,0,0,0,1},{2,7,0,4,0,0,0},{3,20,0,0,0,0,0},{1,0,0,0,0,2,0},{1,4,4,0,0,0,0}}};
  for(unsigned id=0;id<12;++id){Fixture f(root,"circuit-deck");f.put("phase",2);f.put("hp",20);f.put("enemy_hp",200);f.put("energy",3);f.fill("hand",3,255);f.put("hand",id);f.put("input_event",16);f.call("game_update");const auto& c=stats[id];
   require(f.get("hand")==255&&f.get("discard_count")==1,"Card consumed once");
   require(f.get("energy")==3-c[0]+c[6]&&f.get("enemy_hp")==200-c[1]&&f.get("block")==c[2]&&f.get("hp")==20+c[3]&&f.get("poison")==c[4]&&f.get("strength")==c[5],"All twelve card effects");
  }
  Fixture f(root,"circuit-deck");f.put("enemy_hp",100);f.put("input_event",16);f.call("game_update");require(f.get("hand")==0&&f.get("discard_count")==0,"Insufficient energy preserves card");
  f.put("energy",3);f.put("enemy_block",4);f.call("game_update");require(f.get("enemy_hp")==98&&f.get("enemy_block")==0,"Shield absorbs damage");
  f.put("hp",40);f.put("enemy_hp",10);f.put("poison",3);f.put("intent",1);f.put("draw_count",8);f.fill("draw_pile",8,0);f.call("end_turn");require(f.get("enemy_hp")==7&&f.get("enemy_block")==6,"Poison and next enemy intention");
 }
 {
  Fixture f(root,"pocket-factory");f.fill("board",112,0);f.fill("items",112,0);f.put("target_a",100);f.put("target_b",100);f.put("board",6,35);f.put("board",6,36);f.put("board",8,37);f.put("items",1,35);f.put("items",2,37);f.call("simulate");
  require(f.get("items",35)==0&&f.get("items",36)==1&&f.get("items",37)==2,"Merge contention preserves both resources");
  f.call("simulate");require(f.get("items",36)==1&&f.get("items",37)==2,"Mutually blocked belts remain stable");
  f.fill("items",112,0);f.put("board",10,35);f.put("items",1,35);f.call("simulate");require(f.get("items",35)==3,"Press A converts ore A");
  f.put("board",11,35);f.put("items",2,35);f.call("simulate");require(f.get("items",35)==4,"Press B converts ore B");
  f.put("board",3,35);f.put("items",3,35);f.call("simulate");f.call("simulate");require(f.get("shipped_a")==1&&f.get("items",35)==0,"Exactly one shipment per consumed product");
 }
 {
  Fixture f(root,"arc-duel");f.fill("heights",192,50);f.put("health",100);f.put("cpu_health",100);f.put("impact_x",0);f.put("impact_y",60);f.call("explosion");require(f.get("heights")==62&&f.get("heights",191)==50,"Crater clips at left edge");
  f.put("impact_x",191);f.call("explosion");require(f.get("heights",191)==62,"Crater clips at right edge");for(unsigned i=0;i<192;++i)require(f.get("heights",i)<=62,"Terrain remains within screen");
 }
 std::cout<<"PASS: laser cycles, twelve card effects, costs, shield/poison, factory contention/conversion/shipping, crater edges\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
