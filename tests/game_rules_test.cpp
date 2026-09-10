// SPDX-License-Identifier: MIT
// Explicit in-memory unit fixtures supplement the full key-only game replays.
#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
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
 void call(const std::string& n,std::uint8_t b=0){const auto addr=symbols.at(n);const std::array<std::uint8_t,5> driver{0xc6,b,0xbd,static_cast<std::uint8_t>(addr>>8),static_cast<std::uint8_t>(addr)};require(bus.host_load_ram(0x2000,driver)==Jr800MemoryStatus::ok,"Driver");cpu.initialize(jr800::isa::CpuProfile::hd6301v1,0x2000,0x5fff);unsigned steps=0;while(cpu.state().pc!=0x2005&&steps++<300000){bus.set_instruction_context(cpu.state().cycle_count,cpu.state().pc);auto r=cpu.step_instruction(bus);require(r.succeeded(),"CPU at "+std::to_string(r.pc_before));}require(cpu.state().pc==0x2005&&cpu.state().sp==0x5fff,"Bounded routine and balanced stack");}
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
 {
  Fixture f(root,"line-four");
  for(unsigned piece=1;piece<=2;++piece)for(int y=0;y<6;++y)for(int x=0;x<7;++x)
   for(const auto& [dx,dy]:std::array<std::pair<int,int>,4>{{{1,0},{0,1},{1,1},{-1,1}}}){
    if(x+3*dx<0||x+3*dx>=7||y+3*dy>=6)continue;
    f.fill("board",42,0);
    for(int i=0;i<4;++i)f.put("board",piece,static_cast<unsigned>((y+i*dy)*7+x+i*dx));
    for(int i=0;i<4;++i){f.put("line_last",static_cast<std::uint8_t>((y+i*dy)*7+x+i*dx));f.call("line_score");require(f.get("line_longest")==4,"Every four-in-a-row window and last-piece position");}
   }
  f.fill("board",42,0);for(unsigned p:{5,6,7,8})f.put("board",1,p);f.put("line_last",6);f.call("line_score");require(f.get("line_longest")==2,"Rows do not wrap across the board edge");
  for(unsigned piece:{1,2}){f.fill("board",42,0);f.put("stage",1);for(unsigned p:{35,36,37})f.put("board",piece,p);f.call("four_cpu");require(f.get("board",38)==2,"CPU wins or blocks an immediate threat");}
  f.fill("board",42,0);f.put("stage",2);for(unsigned p:{28,29,30,35})f.put("board",1,p);for(unsigned p:{36,37})f.put("board",2,p);f.call("four_cpu");require(f.get("board",38)==0,"Hard CPU avoids supporting an immediate opponent win");
 }
 {
  Fixture f(root,"reversi-mini");
  for(unsigned piece=1;piece<=2;++piece)for(int y=0;y<6;++y)for(int x=0;x<6;++x)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
   if(dx==0&&dy==0)continue;
   for(int count=1;count<=4;++count){
    const int X=x+(count+1)*dx,Y=y+(count+1)*dy;if(X<0||X>=6||Y<0||Y>=6)continue;
    f.fill("board",36,0);f.put("rev_who",piece);f.put("rev_apply",0);
    for(int i=1;i<=count;++i)f.put("board",3-piece,static_cast<unsigned>((y+i*dy)*6+x+i*dx));
    f.put("board",piece,static_cast<unsigned>(Y*6+X));
    f.call("rev_measure",static_cast<std::uint8_t>(y*6+x));
    require(f.get("rev_total")==count,"Every bracketed ray is counted exactly");
    f.call("rev_place",static_cast<std::uint8_t>(y*6+x));require(f.get("board",static_cast<unsigned>(y*6+x))==piece,"Placed reversi disk");
    for(int i=1;i<=count;++i)require(f.get("board",static_cast<unsigned>((y+i*dy)*6+x+i*dx))==piece,"Every bracketed disk flips");
   }
  }
  f.fill("board",36,0);f.put("rev_who",1);f.put("rev_apply",0);f.put("cursor",5);f.put("board",2,6);f.put("board",2,7);f.put("board",1,8);f.call("rev_measure",5);require(f.get("rev_total")==0,"Reversi does not wrap rows");
 }
 {
  Fixture f(root,"five-stones");
  for(unsigned piece=1;piece<=2;++piece)for(int y=0;y<7;++y)for(int x=0;x<14;++x)
   for(const auto& [dx,dy]:std::array<std::pair<int,int>,4>{{{1,0},{0,1},{1,1},{-1,1}}}){
    if(x+4*dx<0||x+4*dx>=14||y+4*dy>=7)continue;
    f.fill("board",98,0);
    for(int i=0;i<5;++i)f.put("board",piece,static_cast<unsigned>((y+i*dy)*14+x+i*dx));
    for(int i=0;i<5;++i){f.put("line_last",static_cast<std::uint8_t>((y+i*dy)*14+x+i*dx));f.call("line_score");require(f.get("line_longest")==5,"All five-stone windows and last-piece positions");}
   }
  f.fill("board",98,0);for(unsigned p:{45,46,47})f.put("board",1,p);f.put("line_last",46);f.call("line_score");require(f.get("line_longest")==3&&f.get("line_open")==2,"Open three");
  f.put("board",2,44);f.call("line_score");require(f.get("line_open")==1,"Opponent closes one end");f.put("board",2,48);f.call("line_score");require(f.get("line_open")==0,"Both ends blocked");
  for(unsigned piece:{1,2}){f.fill("board",98,0);for(unsigned p:{42,43,44,45})f.put("board",piece,p);f.call("five_cpu");require(f.get("board",46)==2,"Five-stone CPU wins or blocks at an edge");}
 }
 {
  Fixture f(root,"hex-front");std::uint32_t seed=8018;
  const std::array<std::pair<int,int>,6> directions{{{0,-1},{0,1},{-1,0},{1,0},{1,-1},{-1,1}}};
  for(unsigned sample=0;sample<16;++sample){
   std::array<unsigned,36> cells{};
   for(unsigned p=0;p<36;++p){seed=seed*1664525U+1013904223U;cells[p]=sample<3?sample:(seed>>30)%3;f.put("board",static_cast<std::uint8_t>(cells[p]),p);}
   for(unsigned edge=0;edge<4;++edge){
    const unsigned who=edge<2?2:1;std::array<unsigned,36> distance{};distance.fill(255);
    for(unsigned p=0;p<36;++p)if((edge==0?p/6==0:edge==1?p/6==5:edge==2?p%6==0:p%6==5)&&cells[p]!=3-who)distance[p]=cells[p]==who?0:1;
    // Repeated edge relaxation is independent of the program's 0/1 deque traversal.
    for(unsigned pass=0;pass<36;++pass){bool changed=false;
     for(int p=0;p<36;++p)if(distance[static_cast<unsigned>(p)]<255)for(const auto& [dx,dy]:directions){const int x=p%6+dx,y=p/6+dy;if(x<0||x>=6||y<0||y>=6)continue;const auto q=static_cast<unsigned>(y*6+x);if(cells[q]==3-who)continue;const unsigned next=distance[static_cast<unsigned>(p)]+(cells[q]==who?0:1);if(next<distance[q]){distance[q]=next;changed=true;}}
     if(!changed)break;
    }
    f.put("hex_edge",edge);f.call("hex_path");for(unsigned p=0;p<36;++p)require(f.get("hex_maps",edge*36+p)==distance[p],"All four hex edge distance fields, including blocked and zero-cost cells");
   }
  }
  f.fill("board",36,0);f.put("cursor",0);f.put("board",1,5);f.call("hex_support");require(f.get("hex_adjacent")==0,"Relay support does not wrap an edge");f.put("board",1,1);f.put("board",1,6);f.call("hex_support");require(f.get("hex_adjacent")==2,"Two six-neighbour relay supports");
 }
 std::cout<<"PASS: laser cycles, twelve card effects, costs, shield/poison, factory contention/conversion/shipping, crater edges, connect-four windows and tactics, reversi rays, five-stone windows and open ends, hex distance fields\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
