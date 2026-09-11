// SPDX-License-Identifier: MIT
// Explicit in-memory unit fixtures supplement the full key-only game replays.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
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
 void call(const std::string& n,std::uint8_t b=0,std::uint8_t a=0,unsigned x=0){const auto addr=symbols.at(n);const std::array<std::uint8_t,10> driver{0xce,static_cast<std::uint8_t>(x>>8),static_cast<std::uint8_t>(x),0x86,a,0xc6,b,0xbd,static_cast<std::uint8_t>(addr>>8),static_cast<std::uint8_t>(addr)};require(bus.host_load_ram(0x5e00,driver)==Jr800MemoryStatus::ok,"Driver");cpu.initialize(jr800::isa::CpuProfile::hd6301v1,0x5e00,0x5fff);unsigned steps=0;while(cpu.state().pc!=0x5e0a&&steps++<300000){bus.set_instruction_context(cpu.state().cycle_count,cpu.state().pc);auto r=cpu.step_instruction(bus);require(r.succeeded(),"CPU at "+std::to_string(r.pc_before));}require(cpu.state().pc==0x5e0a&&cpu.state().sp==0x5fff,"Bounded routine and balanced stack");}
};
int main(int argc,char** argv){try{
 require(argc==2,"Usage: game_rules_test root");const std::string root=argv[1];
 // Render the same numbers through four typefaces, at controller boundaries.
 // Expected strokes derive from the SDK's unscaled glyphs, not the HUD cache.
 {
  Fixture f(root,"box-shift");const auto desc=f.symbols.at("hud_field_0");
  const auto cache=(unsigned(f.get("hud_field_0",6))<<8)|f.get("hud_field_0",7);
  for(unsigned font=0;font<4;++font)for(unsigned inverse:{0U,128U})for(unsigned x:{0U,45U,94U,137U}){
   f.put("hud_field_0",x);f.put("hud_field_0",3,1);f.put("hud_field_0",5,2);f.put("hud_field_0",font,3);f.put("hud_field_0",inverse,4);
   require(f.bus.host_fill_ram(cache,3,0)==Jr800MemoryStatus::ok,"Reset numeric field cache");
   f.fill("framebuffer",1536,165);
   for(unsigned value:{65535U,10000U,999U,10U,9U,0U}){
    std::array<unsigned,1536> before{};for(unsigned i=0;i<1536;++i)before[i]=f.get("framebuffer",i);
    f.call("hud_number",value&255,value>>8,desc);
    auto text=std::to_string(value);text=std::string(5-text.size(),' ')+text;
    const unsigned sx=font==3?2:1,sy=font>=2?2:1,fw=font==0?4:font==3?11:6;
    for(unsigned band=0;band<8;++band)for(unsigned col=0;col<192;++col){
     unsigned expected=before[band*192+col];
     if(band>=3&&band<3+sy&&col>=x&&col<x+5*fw){
      const unsigned index=(col-x)/fw,gx=(col-x)%fw;expected=0;
      if(font==0){expected=f.get("hud_font_tiny",(text[index]-32)*4+gx);}
      else if(gx<5*sx){const unsigned glyph=f.get("font",(text[index]-32)*5+gx/sx);for(unsigned bit=0;bit<8;++bit){const unsigned gy=((band-3)*8+bit)/sy;if(gy<7&&(glyph&(1U<<gy)))expected|=1U<<bit;}}
      if(inverse)expected^=255;
     }
     require(f.get("framebuffer",band*192+col)==expected,"HUD numeric strokes, erasure and controller boundaries");
    }
    f.fill("dirty_min",8,192);f.fill("dirty_max",8,0);f.call("hud_number",value&255,value>>8,desc);
    for(unsigned i=0;i<8;++i)require(f.get("dirty_min",i)==192,"Unchanged numeric value marks no LCD transfer");
   }
  }
 }

 {
  Fixture f(root,"box-shift");f.fill("framebuffer",1536,165);f.call("result_draw",0,0,f.symbols.at("result_win"));
  const std::array<std::string,4> lines{{"+------------------+","|      CLEAR       |","| SPACE: CONTINUE  |","+------------------+"}};
  for(unsigned band=0;band<8;++band)for(unsigned x=0;x<192;++x){unsigned expected=165;if(band>=2&&band<=5&&x>=36&&x<156){const unsigned col=(x-36)%6;expected=col==5?0:f.get("font",(lines[band-2][(x-36)/6]-32)*5+col);}require(f.get("framebuffer",band*192+x)==expected,"Result dialog is opaque and preserves every pixel outside its rectangle");}
 }
 {
  Fixture f(root,"micro-rogue");const auto desc=f.symbols.at("hud_field_0");f.put("hud_field_0",45);f.put("hud_field_0",3,1);f.put("hud_field_0",54,2);f.put("hud_field_0",4,3);f.put("hud_field_0",0,4);f.put("hud_field_0",24,5);
  for(unsigned value:{0U,1U,12U,24U,25U,256U}){f.call("hud_number",value&255,value>>8,desc);const unsigned filled=52*std::min(value,24U)/24;for(unsigned x=0;x<54;++x){const unsigned expected=x==0||x==53?63:(x<=filled?63:33);require(f.get("framebuffer",3*192+45+x)==expected,"Health gauge saturates, scales and erases exactly");}}
 }
 // HUD initialization preserves each game's logical 128-pixel play viewport.
 for(const auto& game:{"box-shift","mirror-link","micro-rogue","beat-step","balance-dock","pocket-factory"}){
  Fixture f(root,game);f.put("seed",1);f.call("game_start");f.fill("framebuffer",1536,165);f.put("hud_ready",0);f.call("visual_hud");
  const unsigned origin=f.get("view_origin");for(unsigned band=1;band<8;++band)for(unsigned x=origin;x<origin+128;++x)require(f.get("framebuffer",band*192+x)==165,"HUD preserves shifted play viewport");
 }

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
 {
  Fixture f(root,"pawn-race");
  const std::array<int,3> dx{{0,-1,1}};
  for(unsigned side=1;side<=2;++side)for(int from=0;from<36;++from)for(unsigned dir=0;dir<3;++dir)for(unsigned target=0;target<3;++target){
   f.fill("board",36,0);f.put("board",side,static_cast<unsigned>(from));f.put("pawn_side",side);f.put("pawn_from",static_cast<std::uint8_t>(from));f.put("pawn_dir",dir);
   const int x=from%6+dx[dir],y=from/6+(side==1?-1:1),to=y*6+x;const bool inside=x>=0&&x<6&&y>=0&&y<6;
   if(inside)f.put("board",target,static_cast<unsigned>(to));
   const bool valid=inside&&target!=side&&(dir!=0||target==0);
   f.call("pawn_valid");require((f.cpu.state().a!=0)==valid,"Pawn forward/diagonal legality at every edge and destination occupancy");
   if(inside)require(f.get("pawn_to")==to,"Pawn destination does not wrap rows");
  }
  f.fill("board",36,0);f.put("pawn_side",1);f.put("pawn_from",24);f.put("pawn_dir",0);f.call("pawn_valid");require(f.cpu.state().a==0,"Empty source cannot move");
  f.put("board",2,24);f.call("pawn_valid");require(f.cpu.state().a==0,"Opponent pawn cannot be selected");
 }
 {
  Fixture f(root,"dot-claim");std::array<std::pair<int,int>,31> edges{};unsigned ei=0;
  for(int y=0;y<7;++y)for(int x=0;x<9;++x)if((x+y)%2)edges[ei++]={x,y};
  std::array<std::array<unsigned,4>,12> boxes{};unsigned bi=0;
  for(int y=1;y<7;y+=2)for(int x=1;x<9;x+=2){
   unsigned j=0;for(const auto& point:std::array<std::pair<int,int>,4>{{{x,y-1},{x,y+1},{x-1,y},{x+1,y}}}){
    boxes[bi][j++]=static_cast<unsigned>(std::find(edges.begin(),edges.end(),point)-edges.begin());
   }++bi;
  }
  for(unsigned who=1;who<=2;++who)for(unsigned edge=0;edge<31;++edge)for(unsigned mask=0;mask<16;++mask){
   f.fill("board",43,0);f.put("dot_edge",edge);f.put("dot_side",who);
   for(unsigned box=0;box<12;++box)if(std::find(boxes[box].begin(),boxes[box].end(),edge)!=boxes[box].end())for(unsigned j=0;j<4;++j)if(mask&(1U<<j))f.put("board",3-who,boxes[box][j]);
   f.put("board",0,edge);f.call("dot_claim");unsigned count=0;
   for(unsigned box=0;box<12;++box){bool full=true;for(auto e:boxes[box])full=full&&f.get("board",e)!=0;
    const bool adjacent=std::find(boxes[box].begin(),boxes[box].end(),edge)!=boxes[box].end();const unsigned expected=adjacent&&full?who:0;
    require(f.get("board",31+box)==expected,"Dot edge closes exactly its adjacent complete boxes");if(expected)++count;
   }
   require(f.get("dot_captured")==count,"One edge may claim two boxes");
  }
 }
 {
  Fixture f(root,"pipe-weave");std::uint32_t seed=1919;
  const std::array<std::array<int,4>,4> dirs{{{{0,-1,1,2}},{{0,1,2,1}},{{-1,0,4,8}},{{1,0,8,4}}}};
  for(unsigned sample=0;sample<32;++sample){
   std::array<unsigned,36> cells{};std::array<bool,36> wet{};wet[0]=true;unsigned leaks=0;
   for(unsigned p=0;p<36;++p){seed=seed*1664525U+1013904223U;cells[p]=sample==0?0:sample==1?15:(seed>>28);f.put("board",static_cast<std::uint8_t>(cells[p]),p);}
   for(int p=0;p<36;++p)for(const auto& d:dirs)if(cells[static_cast<unsigned>(p)]&static_cast<unsigned>(d[2])){
    const int x=p%6+d[0],y=p/6+d[1];if(x<0||x>=6||y<0||y>=6||(cells[static_cast<unsigned>(y*6+x)]&static_cast<unsigned>(d[3]))==0)++leaks;
   }
   for(unsigned pass=0;pass<36;++pass)for(int p=0;p<36;++p)if(wet[static_cast<unsigned>(p)])for(const auto& d:dirs){
    const int x=p%6+d[0],y=p/6+d[1];if(x>=0&&x<6&&y>=0&&y<6&&(cells[static_cast<unsigned>(p)]&static_cast<unsigned>(d[2]))&&(cells[static_cast<unsigned>(y*6+x)]&static_cast<unsigned>(d[3])))wet[static_cast<unsigned>(y*6+x)]=true;
   }
   f.put("phase",2);f.call("pipe_trace");require(f.get("pipe_leaks")==leaks,"Pipe leak counts include every unmatched port and screen edge");
   unsigned dry=0;for(unsigned p=0;p<36;++p){require((f.get("pipe_wet",p)!=0)==wet[p],"Pipe wet set terminates on cyclic and disconnected networks");if(!wet[p])++dry;}
   require(f.get("grid_stat")==dry,"Dry pipe count");
  }
 }
 {
  Fixture f(root,"number-rail");
  for(unsigned code=0;code<625;++code){
   std::vector<unsigned> compact;unsigned digits=code;
   for(unsigned i=0;i<4;++i){const unsigned v=digits%5;digits/=5;f.put("rail_row",v,i);if(v)compact.push_back(v);}
   std::array<unsigned,4> expected{};unsigned out=0,points=0;
   for(unsigned i=0;i<compact.size();++i){unsigned v=compact[i];if(i+1<compact.size()&&v==compact[i+1]){++v;++i;points+=1U<<v;}expected[out++]=v;}
   f.put("rail_delta",0);f.put("rail_delta",0,1);f.call("rail_merge");
   for(unsigned i=0;i<4;++i)require(f.get("rail_row",i)==expected[i],"Every short number row compresses and merges each tile once");
   require(f.get("rail_delta")*256U+f.get("rail_delta",1)==points,"Merge points equal the new tile values");
  }
  for(unsigned p=0;p<16;++p)f.put("board",static_cast<std::uint8_t>(1+(p/4+p%4)%2),p);
  f.put("phase",2);f.put("stage",2);f.call("rail_status");require(f.get("phase")==5,"No empty square and no equal neighbours ends the game");
  f.put("phase",2);f.put("board",0,0);f.call("rail_status");require(f.get("phase")==2,"An empty square keeps the game playable");
  f.put("board",11,0);f.call("rail_status");require(f.get("phase")==4,"2048 reaches the final goal");
  for(unsigned p=0;p<16;++p)f.put("board",static_cast<std::uint8_t>(1+p%4),p);
  f.put("phase",2);f.put("rail_direction",2);f.put("seed",93);f.put("undo_valid",1);f.fill("snapshot_board",112,165);f.put("moves",17);f.put("rail_score",123,1);f.call("rail_slide");
  require(f.get("moves")==17&&f.get("seed")==93&&f.get("rail_score",1)==123&&f.get("undo_valid")==1,"Invalid slide preserves counters, RNG and undo");
  for(unsigned p=0;p<112;++p)require(f.get("snapshot_board",p)==165,"Invalid slide cannot overwrite the undo board");
  f.fill("board",16,0);f.put("board",1,0);f.put("board",1,1);f.put("rail_score",255);f.put("rail_score",254,1);f.call("rail_slide");
  require(f.get("rail_score")==255&&f.get("rail_score",1)==255,"Five-digit score saturates without wrapping");
 }
 {
  Fixture f(root,"mine-field");
  for(unsigned count:{10,15,20})for(unsigned start:{0,13,49,84,97})for(unsigned seed:{1,93}){
   f.fill("board",98,0);f.put("cursor",start);f.put("mine_count",count);f.put("seed",seed);f.call("mine_generate");unsigned actual=0;
   for(int p=0;p<98;++p){const auto cell=f.get("board",static_cast<unsigned>(p));if(cell&16)++actual;
    unsigned adjacent=0;for(int q=0;q<98;++q)if(q!=p&&std::abs(p%14-q%14)<=1&&std::abs(p/14-q/14)<=1&&(f.get("board",static_cast<unsigned>(q))&16))++adjacent;
    if(!(cell&16))require((cell&15)==adjacent,"Mine number equals all eight adjacent mines");
    if(std::abs(p%14-static_cast<int>(start%14))<=1&&std::abs(p/14-static_cast<int>(start/14))<=1)require((cell&16)==0,"Every first click and its neighbours are safe, including corners");
    require((cell&128)==0,"Temporary generation markers are cleared");
   }require(actual==count,"Mine generation places exactly the selected count");
  }
  f.fill("board",98,0);f.put("phase",2);f.put("grid_stat",98);f.call("mine_reveal",49);require(f.get("grid_stat")==0,"A zero-region flood visits each of 98 cells exactly once");
  for(unsigned p=0;p<98;++p)require(f.get("board",p)==32,"Entire zero field is revealed");
  f.fill("board",98,0);f.put("phase",2);f.put("grid_stat",97);f.put("board",80,0);f.call("mine_reveal",0);require(f.get("phase")==2&&f.get("grid_stat")==97,"Flags prevent reveal even on a mine");
  f.put("board",16,0);f.call("mine_reveal",0);require(f.get("phase")==5&&f.get("mine_blast")==0,"Opening an unflagged mine records the hit");
 }
 {
  Fixture f(root,"loop-trace");f.fill("board",36,0);f.put("board",5,0);f.put("board",3,1);f.put("cursor",0);f.put("loop_marks",1,0);f.put("loop_next",1);f.put("input_event",8);f.put("phase",2);f.put("grid_stat",1);f.call("game_update");
  require(f.get("cursor")==0&&f.get("moves")==0,"Checkpoint B cannot be entered before A");
  f.put("board",2,1);f.call("game_update");require(f.get("cursor")==1&&f.get("loop_next")==2,"Checkpoint A advances the required order");
  f.call("loop_undo");require(f.get("cursor")==0&&f.get("loop_next")==1&&f.get("loop_paths",0)==0&&f.get("loop_paths",1)==0,"Undo restores checkpoint order and both ends of the line");
  f.put("input_event",16);f.put("grid_stat",0);f.put("loop_next",4);f.put("cursor",35);f.call("game_update");require(f.get("phase")==2,"A non-adjacent final node cannot close a loop");
  f.put("cursor",1);f.call("game_update");require(f.get("phase")==4&&f.get("loop_paths",0)==8&&f.get("loop_paths",1)==4,"Closing connects both endpoints to the start");
 }
 {
  Fixture f(root,"ace-stack");
  for(unsigned row=0;row<7;++row)for(unsigned col=0;col<=row;++col)for(unsigned mask=0;mask<4;++mask)for(unsigned rank=1;rank<=13;++rank){
   const unsigned card=row*(row+1)/2+col;f.fill("board",52,0);f.put("board",rank,card);
   if(row<6){const unsigned child=(row+1)*(row+2)/2+col;if(mask&1)f.put("board",2,child);if(mask&2)f.put("board",3,child+1);}
   f.call("ace_available",card);require(f.cpu.state().a==((row==6||mask==0)?rank:0),"Pyramid availability requires both covering cards to be removed");
  }
  f.fill("board",52,0);f.put("board",5,21);f.put("board",7,22);f.put("grid_stat",2);f.put("phase",2);f.put("cursor",21);f.put("input_event",16);f.call("game_update");require(f.get("selection_active")==1,"First card is selected");
  f.put("cursor",22);f.call("game_update");require(f.get("moves")==0&&f.get("board",21)==5&&f.get("board",22)==7,"A pair must total thirteen");
  f.put("board",8,22);f.call("game_update");require(f.get("phase")==4&&f.get("grid_stat")==0,"A valid pair removes both cards and clears the pyramid");
  f.fill("board",52,0);f.put("board",1,21);f.put("grid_stat",1);f.put("ace_stock_pos",24);f.put("ace_waste_count",0);f.put("phase",2);f.call("ace_check");require(f.get("phase")==5,"An exhausted stock without any legal pair ends the deal");
  f.put("board",12,28);f.put("ace_waste_count",1);f.put("phase",2);f.call("ace_check");require(f.get("phase")==2,"The top waste card can keep a tableau pair available");
 }
 {
  Fixture f(root,"suit-run");
  for(unsigned waste=1;waste<=13;++waste)for(unsigned rank=1;rank<=13;++rank){
   f.fill("board",35,0);f.put("board",rank,28);f.put("suit_waste",waste);f.put("suit_stock_pos",0);f.put("suit_chain",0);f.put("suit_score",0);f.put("suit_score",0,1);f.put("moves",0);f.put("grid_stat",1);f.put("cursor",28);f.put("input_event",16);f.put("phase",2);f.call("game_update");
   const int delta=std::abs(static_cast<int>(waste)-static_cast<int>(rank));const bool valid=delta==1||delta==12;
   require((f.get("phase")==4)==valid,"Golf accepts exactly adjacent ranks, including A/K wrapping");
   require(f.get("suit_score",1)==(valid?1:0),"Only a removed card scores");
  }
  for(unsigned card=0;card<35;++card)for(unsigned covered=0;covered<2;++covered){
   f.fill("board",35,0);f.put("board",7,card);if(card<28&&covered)f.put("board",8,card+7);f.call("suit_available",card);
   require(f.cpu.state().a==((card>=28||!covered)?7:0),"Only the exposed end of a golf column is available");
  }
  f.fill("board",35,0);f.put("board",7,28);f.put("suit_waste",2);f.put("suit_stock_pos",16);f.put("grid_stat",1);f.put("phase",2);f.call("suit_check");require(f.get("phase")==5,"An exhausted golf stock with no adjacent rank fails");
 }
 {
  Fixture f(root,"dice-hold");
  for(unsigned code=0;code<7776;++code){
   unsigned digits=code,sum=0;std::array<unsigned,7> count{};std::array<unsigned,13> score{};
   for(unsigned i=0;i<5;++i){const unsigned face=digits%6+1;digits/=6;f.put("board",face,i);++count[face];sum+=face;}
   unsigned longest=0,run=0;bool two=false,three=false;for(unsigned n=1;n<=6;++n){score[n-1]=n*count[n];if(count[n])++run;else run=0;longest=std::max(longest,run);two=two||count[n]==2;three=three||count[n]==3;}
   const unsigned max=*std::max_element(count.begin(),count.end());score[6]=max>=3?sum:0;score[7]=max>=4?sum:0;score[8]=two&&three?25:0;score[9]=longest>=4?30:0;score[10]=longest>=5?40:0;score[11]=max==5?50:0;score[12]=sum;
   f.call("dice_evaluate");for(unsigned c=0;c<13;++c)require(f.get("dice_scores",c)==score[c],"All 7776 ordered dice throws match thirteen independent category scores");
  }
  f.fill("dice_used",13,0);f.put("dice_upper",60);f.put("dice_total",0);f.put("dice_total",60,1);f.put("dice_bonus",0);f.put("dice_round",12);f.put("dice_category",0);f.put("dice_scores",3,0);f.put("phase",2);f.put("stage",0);f.call("dice_commit");require(f.get("dice_total",1)==98&&f.get("dice_bonus")==35,"The upper bonus is granted once at sixty-three");
  f.call("dice_commit");require(f.get("dice_total",1)==98&&f.get("dice_round")==13,"An already filled score row is inert");
 }
 {
  Fixture f(root,"push-luck");
  for(unsigned stage=0;stage<3;++stage)for(unsigned player:{0,79,80,99})for(unsigned cpu:{0,70,95})for(unsigned pot=0;pot<40;++pot)for(unsigned rolls:{0,5,6}){
   f.put("stage",stage);f.put("luck_scores",player);f.put("luck_scores",cpu,1);f.put("luck_pot",pot);f.put("luck_rolls",rolls);f.call("luck_decide");
   const unsigned limit=stage==2&&player>=80?32:std::array<unsigned,3>{12,18,24}[stage];const bool stop=pot&&(cpu+pot>=100||rolls>=6||pot>=limit);
   require((f.cpu.state().a!=0)==stop,"CPU banks using difficulty, goal and turn-risk thresholds");
  }
  for(unsigned side=0;side<2;++side){f.put("luck_side",side);f.put("luck_scores",95,side);f.put("luck_pot",7);f.put("phase",2);f.call("luck_bank");require(f.get("phase")==4+side&&f.get("luck_scores",side)==102&&f.get("luck_pot")==0,"A bank of at least one hundred ends the match for the correct side");}
  f.put("luck_side",0);f.put("luck_scores",250);f.put("luck_pot",20);f.put("phase",2);f.call("luck_bank");require(f.get("luck_scores")==255,"A large bank saturates instead of wrapping");
  f.put("luck_side",0);f.put("luck_pot",0);f.put("phase",2);f.call("luck_bank");require(f.get("luck_side")==0&&f.get("phase")==2,"Banking an empty pot does not skip a turn");
 }
 {
  Fixture f(root,"wall-break");
  for(unsigned row=0;row<3;++row)for(unsigned col=0;col<8;++col)for(unsigned life:{1,2}){
   f.fill("wall_board",24,0);f.put("wall_board",life,row*8+col);f.put("wall_left",1);f.put("wall_score",0);f.put("wall_score",0,1);f.put("phase",2);f.put("wall_x",col*16+7);f.put("wall_y",row*8+15);f.put("wall_dx",1);f.put("wall_dy",255);f.put("wall_paddle",52);f.call("wall_step");
   require(f.get("wall_board",row*8+col)==life-1,"A vertical contact reduces one brick hit point");require(f.get("wall_score",1)==10,"Every brick contact scores ten");require(f.get("wall_dy")==1,"Brick contact reverses vertical velocity");
  }
  for(unsigned x:{0,7,8,120,127,128,250})for(unsigned y:{0,7,8,15,16,63,64})for(unsigned w:{0,1,2,22,255})for(unsigned h:{0,1,2,255}){
   f.fill("tile_cache",112,0);f.put("clear_chunks",165);f.put("scene_w",w);f.put("scene_h",h);f.call("scene_rect",y,x);
   for(unsigned p=0;p<112;++p){const unsigned tx=(p%16)*8,ty=(p/16+1)*8;const bool touched=w&&h&&tx<x+w&&tx+8>x&&ty<y+h&&ty+8>y;
    require(f.get("tile_cache",p)==(touched?255:0),"Moving-object invalidation clips exactly to intersecting playfield tiles");}
   require(f.get("clear_chunks")==165,"Rectangle invalidation cannot overwrite the next state byte");
  }
  for(unsigned x:{0,47,48,95,96,127,128})for(unsigned y:{7,8,15,16,63,64}){
   f.fill("framebuffer",1536,0);f.call("scene_pixel",y,x);
   for(unsigned i=0;i<1536;++i){const unsigned expected=x<128&&y>=8&&y<64&&i==(y/8)*192+x+f.get("view_origin")?(1U<<(y%8)):0;require(f.get("framebuffer",i)==expected,"Sprite pixels clip at the top, bottom and HUD without crossing LCD bands");}
  }
  f.fill("wall_board",24,0);f.put("wall_x",1);f.put("wall_y",40);f.put("wall_dx",255);f.put("wall_dy",1);f.call("wall_step");require(f.get("wall_x")==1&&f.get("wall_dx")==1,"Left edge reflects without leaving the playfield");
  f.put("wall_x",125);f.put("wall_dx",1);f.call("wall_step");require(f.get("wall_x")==125&&f.get("wall_dx")==255,"Right edge reflects without entering the HUD");
  for(unsigned x=39;x<65;++x){f.put("wall_x",x);f.put("wall_y",58);f.put("wall_dx",0);f.put("wall_dy",1);f.put("wall_paddle",40);f.call("wall_step");const bool catchBall=x+1>=40&&x+1<62;require((f.get("wall_dy")==255)==catchBall,"Paddle collision includes exactly its horizontal span");}
 }
 {
  Fixture f(root,"tail-trail");
  for(unsigned cell=0;cell<98;++cell)for(unsigned direction=0;direction<4;++direction){
   f.fill("board",98,0);f.put("board",1,cell);f.put("tail_body",cell);f.put("tail_length",1);f.put("tail_queued",direction);f.put("cursor",cell);f.put("tail_food",255);f.put("phase",2);
   const int x=static_cast<int>(cell%14)+(direction==2?-1:direction==3?1:0),y=static_cast<int>(cell/14)+(direction==0?-1:direction==1?1:0);const bool valid=x>=0&&x<14&&y>=0&&y<7;
   // Length two exercises normal shifts while keeping a distant vacating tail.
   f.put("tail_length",2);f.put("tail_body",(cell+49)%98,1);f.put("board",1,(cell+49)%98);f.call("tail_move");
   require((f.get("phase")==2)==valid,"Every snake direction respects rectangular board boundaries");if(valid)require(f.get("cursor")==static_cast<unsigned>(y*14+x),"Snake advances exactly one cell");
  }
  f.fill("board",98,0);for(unsigned i=0;i<4;++i){const unsigned p=std::array<unsigned,4>{15,14,28,29}[i];f.put("tail_body",p,i);f.put("board",1,p);}f.put("tail_length",4);f.put("cursor",15);f.put("tail_queued",1);f.put("tail_food",50);f.put("phase",2);f.call("tail_move");require(f.get("cursor")==29&&f.get("phase")==2,"A non-growing snake may enter its vacating tail cell");
  f.fill("board",98,0);f.put("cursor",15);f.put("tail_body",15);f.put("tail_body",16,1);f.put("tail_body",30,2);f.put("tail_length",3);f.put("board",1,15);f.put("board",1,16);f.put("board",1,30);f.put("tail_queued",3);f.put("phase",2);f.call("tail_move");require(f.get("phase")==5,"Entering the body before its tail is fatal");
 }
 {
  Fixture f(root,"maze-chase");
  for(unsigned sample=0;sample<12;++sample){f.put("stage",sample);f.call("game_start");for(unsigned start:{16,28,52,76,88}){
   f.put("cursor",start);f.call("maze_trace");std::array<unsigned,105> dist{};dist.fill(255);std::vector<unsigned> queue{start};dist[start]=0;
   for(unsigned i=0;i<queue.size();++i){const int p=static_cast<int>(queue[i]);for(int d:{-15,15,-1,1}){const int q=p+d;if(q<0||q>=105||(d==-1&&p%15==0)||(d==1&&p%15==14))continue;const auto cell=static_cast<unsigned>(q);if(f.get("board",cell)!=1&&dist[cell]==255){dist[cell]=dist[queue[i]]+1;queue.push_back(cell);}}}
   for(unsigned p=0;p<105;++p)require(f.get("maze_distance",p)==dist[p],"Ghost distance fields match independent BFS for every maze and power junction");
  }}
  f.put("stage",0);f.call("game_start");f.put("cursor",52);f.put("maze_ghosts",52);f.put("maze_ghosts",88,1);f.put("maze_power",1);f.put("maze_running",1);f.put("maze_score",0);f.put("maze_score",0,1);f.call("maze_contact");require(f.get("maze_score",1)==50&&f.get("maze_sleep")==2&&f.get("maze_ghosts")==88,"Powered contact captures one ghost and starts a respawn delay");
  f.put("maze_sleep",0);f.put("maze_ghosts",52);f.put("maze_power",0);f.put("maze_lives",2);f.call("maze_contact");require(f.get("maze_lives")==1&&f.get("cursor")==16&&f.get("maze_running")==0,"Unpowered contact loses one life and waits for restart");
  f.put("cursor",52);f.put("maze_ghosts",52);f.put("maze_power",0);f.call("maze_contact");require(f.get("phase")==5,"The last life ends the chase");
 }
 {
  Fixture f(root,"river-hop");
  for(unsigned lane=0;lane<4;++lane)for(unsigned cell=0;cell<14;++cell){
   f.fill("river_lanes",56,0);f.put("river_lanes",1,lane*14+cell);f.fill("river_periods",4,3);f.fill("river_timers",4,3);f.put("river_timers",1,lane);f.put("river_time",100);f.put("river_lives",5);f.put("cursor",90);f.put("phase",2);f.call("river_world");
   const unsigned next=(cell+((lane==0||lane==3)?1:13))%14;for(unsigned x=0;x<14;++x)require(f.get("river_lanes",lane*14+x)==(x==next?1:0),"Traffic and logs rotate one cell in their own direction");
  }
  for(unsigned lane=0;lane<2;++lane)for(unsigned x=0;x<14;++x){
   f.fill("river_lanes",56,1);f.fill("river_periods",4,3);f.fill("river_timers",4,3);f.put("river_timers",1,lane);f.put("river_time",100);f.put("river_lives",5);f.put("cursor",(lane+1)*14+x);f.put("phase",2);f.call("river_world");const bool falls=lane==0?x==13:x==0;
   require(f.get("river_lives")==static_cast<unsigned>(falls?4:5),"A rider falls only when the log carries it past a screen edge");if(!falls)require(f.get("cursor")==(lane+1)*14+x+(lane==0?1:-1),"A floating log carries its rider");
  }
  for(unsigned x=0;x<14;++x){f.fill("river_homes",5,0);f.put("grid_stat",0);f.put("river_lives",5);f.put("river_time",70);f.put("river_score",0);f.put("river_score",0,1);f.put("cursor",x);f.put("phase",2);f.call("river_check");const bool home=x==1||x==4||x==7||x==10||x==12;require(f.get("grid_stat")==static_cast<unsigned>(home?1:0),"Only one of the five home bays accepts a landing");if(home){require(f.get("river_score",1)==170,"New homes score their remaining time once");f.put("cursor",x);f.call("river_check");require(f.get("grid_stat")==1&&f.get("river_lives")==4,"A filled bay cannot score twice");}}
  for(unsigned row:{1,2,4,5})for(unsigned occupied:{0,1}){f.fill("river_lanes",56,occupied);f.put("cursor",row*14+6);f.put("river_lives",5);f.put("phase",2);f.call("river_check");const bool safe=row<3?occupied!=0:occupied==0;require(f.get("river_lives")==static_cast<unsigned>(safe?5:4),"Water requires a log while roads require an empty cell");}
 }
 {
  Fixture f(root,"tower-leap");
  for(unsigned level:{0,1,4,8,11})for(unsigned dx=0;dx<32;++dx)for(unsigned fall=1;fall<=7;++fall){
   f.fill("tower_platforms",12,12);f.put("tower_platforms",5,level);f.put("tower_x",32+dx);f.put("tower_height",level*16+3);f.put("tower_velocity",256-fall);f.put("tower_camera",0);f.put("tower_lives",3);f.put("tower_highest",0);f.put("tower_checkpoint",0);f.put("tower_grounded",0);f.put("input_held",0);f.put("phase",2);f.call("tower_step");
   const bool lands=fall>=3&&32+dx+3>=40&&32+dx<64;
   require((f.get("tower_grounded")!=0)==(lands||(level==0&&fall>3)),"Descending feet cross only overlapping platform tops, with underflow falling back to the checkpoint");
   if(lands){require(f.get("tower_height")==level*16&&f.get("tower_velocity")==0,"A landing clamps to the platform height");require(f.get("tower_highest")==level&&f.get("tower_checkpoint")==static_cast<unsigned>(level&~3U),"New highest landings update checkpoint floors");require((f.get("phase")==4)==(level==11),"Only the top floor clears the tower");}
  }
  f.put("stage",0);f.call("game_start");f.put("tower_x",50);f.put("tower_height",15);f.put("tower_velocity",3);f.put("tower_grounded",0);f.put("tower_camera",0);f.put("input_held",0);f.call("tower_step");require(f.get("tower_height")==18&&f.get("tower_grounded")==0,"Ascending jumps pass through platform undersides");
  for(unsigned cp:{0,4,8}){f.put("tower_checkpoint",cp);f.put("tower_lives",2);f.call("tower_die");require(f.get("tower_lives")==1&&f.get("tower_height")==cp*16&&f.get("tower_grounded")==1,"A life restores the chosen checkpoint");f.call("tower_die");require(f.get("phase")==5,"The last fall produces failure");}
 }
 {
  Fixture f(root,"bomb-vault");
  for(unsigned stage=0;stage<12;++stage){f.put("stage",stage);f.call("game_start");unsigned keys=0;for(unsigned p=0;p<105;++p)keys+=f.get("board",p)==4;require(keys==3&&f.get("board",88)==3,"Every vault has three key crates and a separate exit");}
  for(unsigned origin:{16,22,28,46,52,58,76,82,88})for(int direction:{-15,15,-1,1})for(unsigned distance=1;distance<=3;++distance)for(unsigned crate:{2,4}){
   for(unsigned p=0;p<105;++p)f.put("board",p/15==0||p/15==6||p%15==0||p%15==14?1:0,p);
   const int target=static_cast<int>(origin)+direction*static_cast<int>(distance);if(target<=0||target>=104||target/15==0||target/15==6||target%15==0||target%15==14)continue;
   f.put("board",crate,static_cast<unsigned>(target));f.fill("bomb_flames",105,0);f.put("bomb_cell",origin);f.put("bomb_score",0);f.put("bomb_score",0,1);std::array<unsigned,105> expected{};expected[origin]=1;unsigned destroyed=0;
   for(int d:{-15,15,-1,1})for(unsigned n=1;n<=3;++n){const int q=static_cast<int>(origin)+d*static_cast<int>(n);const auto p=static_cast<unsigned>(q);if(f.get("board",p)==1)break;expected[p]=1;if(f.get("board",p)==crate){++destroyed;break;}}
   f.call("bomb_explode");for(unsigned p=0;p<105;++p)require(f.get("bomb_flames",p)==expected[p],"Blast rays stop at the first wall or crate, including screen edges");
   require(f.get("board",static_cast<unsigned>(target))==(destroyed?(crate==4?5:0):crate),"Crates reveal keys without burning them away");require(f.get("bomb_score",1)==destroyed*30&&f.get("bomb_flame_time")==4&&f.get("bomb_cell")==255,"One blast scores each crate once and enters its bounded flame interval");
  }
  f.put("stage",0);f.call("game_start");f.put("bomb_flames",1,49);f.call("bomb_contact");require(f.get("bomb_enemies")==255&&f.get("bomb_score",1)==100,"Flames defeat a patroller once");f.call("bomb_contact");require(f.get("bomb_score",1)==100,"A removed enemy cannot score again");
  f.put("cursor",88);f.put("grid_stat",1);f.put("phase",2);f.call("bomb_contact");require(f.get("phase")==2,"The exit stays locked while a key remains");f.put("grid_stat",0);f.call("bomb_contact");require(f.get("phase")==4,"Collecting every key unlocks the exit");
 }
 {
  Fixture f(root,"grid-claim");const std::array<unsigned,4> opposite{1,0,3,2};std::uint32_t rng=0x33112244U;
  const auto next=[](unsigned p,unsigned d){const int x=static_cast<int>(p%16)+(d==2?-1:d==3?1:0),y=static_cast<int>(p/16)+(d==0?-1:d==1?1:0);return x>=0&&x<16&&y>=0&&y<7?static_cast<unsigned>(y*16+x):255U;};
  for(unsigned sample=0;sample<420;++sample){std::array<unsigned,112> board{};for(unsigned p=0;p<112;++p){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;board[p]=rng%5==0?1:0;f.put("board",board[p],p);}const unsigned enemy=sample%112,dir=sample%4,stage=sample%3,reserved=(sample*13)%113;f.put("claim_enemy",enemy);f.put("claim_cpu_dir",dir);f.put("stage",stage);f.put("claim_player_next",reserved);
   unsigned best=255,bestDir=dir,bestScore=0;
   for(unsigned d=0;d<4;++d){const auto n=next(enemy,d);if(d==opposite[dir]||n==255||board[n])continue;unsigned score=1;
    if(stage==1){score=0;for(unsigned turn=0;turn<4;++turn){const auto q=next(n,turn);if(q!=255&&!board[q])score+=4;}}
    if(stage==2){std::array<bool,112> seen{};std::vector<unsigned> queue;if(n!=reserved){queue.push_back(n);seen[n]=true;}for(unsigned i=0;i<queue.size();++i)for(unsigned turn=0;turn<4;++turn){const auto q=next(queue[i],turn);if(q!=255&&q!=reserved&&!board[q]&&!seen[q]){seen[q]=true;queue.push_back(q);}}score=static_cast<unsigned>(queue.size())*2;}
    score+=d==dir?1:0;if(best==255||score>bestScore){best=n;bestDir=d;bestScore=score;}
   }
   f.call("claim_ai");require(f.cpu.state().b==best&&f.get("claim_cpu_dir")==bestDir,"All three trail-racing CPU policies match independent legal-turn and flood-fill evaluation");for(unsigned p=0;p<112;++p)require(f.get("board",p)==board[p],"CPU lookahead does not alter the arena");
  }
  for(unsigned kind=0;kind<4;++kind){f.fill("board",112,3);f.put("cursor",50);f.put("claim_enemy",52);f.put("claim_queued",3);f.put("claim_cpu_dir",2);f.put("claim_wins",0);f.put("claim_losses",0);f.put("claim_mode",1);f.put("phase",2);f.put("stage",0);if(kind==0||kind==2)f.put("board",0,51);if(kind==1){f.put("claim_enemy",60);f.put("board",0,59);}if(kind==2)f.put("claim_enemy",60);f.call("claim_step");require(f.get("claim_mode")==std::array<unsigned,4>{4,3,2,4}[kind],"Same-cell contact and simultaneous crashes draw, while one-sided crashes award only the opponent");}
 }
 {
  Fixture f(root,"gravity-run");
  for(unsigned mask=0;mask<32;++mask)for(unsigned row=1;row<=5;++row)for(int direction:{-1,1}){
   f.put("stage",0);f.call("game_start");f.fill("gravity_course",80,0);f.put("gravity_course",mask,4);f.put("gravity_row",row);f.put("gravity_direction",direction<0?255:1);f.put("phase",2);f.call("gravity_world");const int y=std::clamp(static_cast<int>(row)+direction,1,5);const bool hit=(mask&(1U<<static_cast<unsigned>(y-1)))!=0;
   require(f.get("gravity_lives")==static_cast<unsigned>(hit?2:3)&&f.get("gravity_row")==static_cast<unsigned>(hit?5:y),"Every five-row obstacle mask uses the clamped gravity destination for contact");require(f.get("gravity_distance")==static_cast<unsigned>(hit?0:1),"A collision returns to the active checkpoint");
  }
  f.put("stage",0);f.call("game_start");f.fill("gravity_course",80,0);f.put("gravity_course",4U<<5U,4);f.put("gravity_row",3);f.put("gravity_direction",1);f.call("gravity_world");require(f.get("gravity_stars")==1&&f.get("gravity_score",1)==10,"A star scores exactly ten points");f.put("gravity_distance",0);f.put("gravity_row",3);f.call("gravity_world");require(f.get("gravity_stars")==1&&f.get("gravity_score",1)==10,"A collected star cannot score twice after revisiting");
  f.put("gravity_distance",31);f.call("gravity_world");require(f.get("gravity_checkpoint")==32,"Crossing the halfway mark activates the checkpoint");f.put("gravity_course",31,36);f.call("gravity_world");require(f.get("gravity_distance")==32&&f.get("gravity_lives")==2,"A later collision respawns at halfway");f.put("gravity_distance",63);f.put("phase",2);f.call("gravity_world");require(f.get("phase")==4&&f.get("gravity_score",1)==110,"The final column awards the completion bonus once");
 }
 {
  Fixture f(root,"star-patrol");
  for(unsigned i=0;i<20;++i)for(unsigned hp:{1,2})for(unsigned offset:{0,6})for(unsigned base:{0,4}){
   f.put("stage",0);f.call("game_start");f.fill("star_enemies",20,0);f.put("star_enemies",hp,i);f.put("star_left",1);f.put("star_offset",offset);f.put("star_base",base);f.call("star_view");f.put("star_shot",(base+i/10)*16+offset+i%10);f.put("phase",2);f.call("star_hit");require(f.get("star_enemies",i)==hp-1&&f.get("star_score",1)==10&&f.get("star_shot")==255,"One shot damages exactly one alien before any overlapping shield");require((f.get("phase")==4)==(hp==1),"The final alien must lose all armor before clearance");
  }
  for(unsigned col=0;col<16;++col)for(unsigned hp:{1,2}){f.put("stage",0);f.call("game_start");f.put("star_shields",hp,col);f.put("star_shot",80+col);f.call("star_hit");require(f.get("star_shields",col)==hp-1&&f.get("star_shot")==255,"Player shots chip a shield once");f.put("star_shields",hp,col);f.put("star_bolt",80+col);f.call("star_contact");require(f.get("star_shields",col)==hp-1&&f.get("star_bolt")==255,"Enemy bolts use the same shield durability");}
  f.put("stage",0);f.call("game_start");f.put("star_running",1);f.put("star_shot",52);f.put("star_bolt",36);f.put("phase",2);f.call("star_world");require(f.get("star_shot")==255&&f.get("star_bolt")==255,"Opposite projectiles cancel when their movement segments cross");
  f.put("star_bolt",88);f.put("cursor",104);f.put("star_lives",1);f.put("star_march_clock",7);f.put("star_fire_clock",7);f.put("phase",2);f.call("star_world");require(f.get("phase")==5&&f.get("star_march_clock")==7&&f.get("star_fire_clock")==7,"A terminal hit stops the world before further marching or firing");
  f.call("game_start");f.put("star_running",1);f.put("star_base",4);f.put("star_offset",6);f.put("star_bounces",1);f.put("star_march_clock",7);f.put("phase",2);f.call("star_world");require(f.get("phase")==5&&f.get("star_base")==5,"Invasion of the bottom row ends the patrol even with lives left");
 }
 {
  Fixture f(root,"orbit-guard");
  for(unsigned aim=0;aim<8;++aim)for(unsigned code=0;code<27;++code){f.fill("orbit_enemies",24,0);unsigned n=code,count=0,nearest=255;std::array<unsigned,3> hp{};for(unsigned r=0;r<3;++r){hp[r]=n%3;n/=3;f.put("orbit_enemies",hp[r],r*8+aim);if(hp[r]){++count;if(nearest==255)nearest=r;}}f.put("orbit_aim",aim);f.put("orbit_left",count+1);f.put("orbit_core",5);f.put("orbit_score",0);f.put("orbit_score",0,1);f.put("phase",2);f.call("orbit_shoot");for(unsigned r=0;r<3;++r)require(f.get("orbit_enemies",r*8+aim)==hp[r]-(r==nearest?1:0),"A radial shot damages only the nearest occupied ring");require(f.get("orbit_score",1)==static_cast<unsigned>(nearest==255?0:10),"Only a successful radial hit scores");}
  for(unsigned stage:{0,8})for(unsigned slot=0;slot<24;++slot)for(unsigned hp:{1,2}){f.fill("orbit_enemies",24,0);f.put("orbit_enemies",hp,slot);f.put("orbit_turn",1);f.put("orbit_core",5);f.put("orbit_left",2);f.put("orbit_position",12);f.put("orbit_total",12);f.put("orbit_flash",0);f.put("stage",stage);f.put("phase",2);f.call("orbit_world");const unsigned dest=slot>=8?((slot-8)/8)*8+(slot%8+(stage==8?1:0))%8:255;for(unsigned p=0;p<24;++p)require(f.get("orbit_enemies",p)==(p==dest?hp:0),"Orbital advances preserve armor and rotate all approaching sectors consistently");require(f.get("orbit_core")==static_cast<unsigned>(slot<8?4:5),"Only an inner-ring crossing damages the core");}
  f.fill("orbit_enemies",24,2);f.put("orbit_left",24);f.put("orbit_core",5);f.put("orbit_pulse",1);f.put("orbit_score",0);f.put("orbit_score",0,1);f.put("phase",2);f.call("orbit_burst");require(f.get("phase")==4&&f.get("orbit_left")==0&&f.get("orbit_score")==1&&f.get("orbit_score",1)==224,"The one-use pulse clears active enemies and scores remaining armor");f.call("orbit_burst");require(f.get("orbit_score",1)==224,"A spent pulse cannot score again");
 }
 {
  Fixture f(root,"target-range");
  for(unsigned cursor=0;cursor<9;++cursor)for(unsigned target=0;target<9;++target)for(unsigned remaining:{1,8,32,144}){f.fill("board",9,0);f.put("cursor",cursor);f.put("range_target",target);f.put("range_remaining",remaining);f.put("range_misses",0);f.put("range_score",0);f.put("range_score",0,1);f.put("phase",2);f.call("range_shoot");const bool hit=cursor==target;require(f.get("range_score",1)==(hit?10+remaining/8:0)&&f.get("range_misses")==static_cast<unsigned>(hit?0:1),"Target hits use remaining-time bonuses while all other panels count as mistakes");require(f.get("range_result")==static_cast<unsigned>(hit?1:4)&&f.get("range_remaining")==12,"Each decision enters its bounded feedback interval");}
  for(unsigned target:{2,255}){f.put("range_target",target);f.put("range_misses",0);f.put("range_score",0);f.put("range_score",0,1);f.put("phase",2);f.call("range_expire");require(f.get("range_misses")==static_cast<unsigned>(target==255?0:1)&&f.get("range_score",1)==static_cast<unsigned>(target==255?10:0),"Waiting is correct only when no real target is present");}
  f.put("cursor",2);f.put("range_target",2);f.put("range_mode",1);f.put("range_trial_pending",0);f.put("resume_pending",0);f.put("range_remaining",12);f.put("range_clock",0);f.put("input_ticks",12);f.put("input_event",16);f.put("range_misses",0);f.put("range_score",0);f.put("range_score",0,1);f.put("phase",2);f.call("game_update");require(f.get("range_result")==3&&f.get("range_score",1)==0,"An action at the exact deadline is rejected before shot processing");
  f.put("range_mode",1);f.put("range_trial_pending",1);f.put("range_remaining",144);f.put("range_clock",0);f.put("input_ticks",200);f.put("input_event",0);f.call("game_update");require(f.get("range_remaining")==144&&f.get("range_clock")==200&&f.get("range_trial_pending")==0,"A newly rendered target starts with its full reaction window");
  for(unsigned stage=0;stage<3;++stage)for(int delta:{-1,0}){const unsigned goal=std::array<unsigned,3>{180,220,250}[stage],score=static_cast<unsigned>(static_cast<int>(goal)+delta);f.put("stage",stage);f.put("range_mode",2);f.put("range_round",19);f.put("range_remaining",1);f.put("range_clock",0);f.put("input_ticks",1);f.put("range_score",score>>8U);f.put("range_score",score&255U,1);f.put("phase",2);f.call("game_update");require(f.get("phase")==static_cast<unsigned>(delta<0?5:4),"The twentieth trial applies the selected score threshold");}
 }
 {
  Fixture f(root,"ricochet-ops");const std::array<std::pair<int,int>,8> directions{{{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}}};
  for(unsigned mirror:{2,3})for(unsigned p=0;p<48;++p)for(unsigned d=0;d<8;++d){f.fill("board",48,0);f.fill("rico_visited",384,0);f.fill("rico_trail",48,0);f.put("rico_bullet",p);f.put("rico_direction",d);f.put("cursor",41);f.put("rico_active",1);f.put("rico_ammo",2);f.put("phase",2);const auto [dx,dy]=directions[d];const int x=static_cast<int>(p%8)+dx,y=static_cast<int>(p/8)+dy;const bool valid=x>=0&&x<8&&y>=0&&y<6&&y*8+x!=41;const unsigned q=valid?static_cast<unsigned>(y*8+x):0;if(valid)f.put("board",mirror,q);f.call("rico_step");require(f.get("rico_active")==static_cast<unsigned>(valid),"Every ray direction clips screen edges and the launch cell");if(valid){const auto reflected=mirror==2?std::pair{-dy,-dx}:std::pair{dy,dx};const auto found=std::find(directions.begin(),directions.end(),reflected);require(f.get("rico_direction")==static_cast<unsigned>(std::distance(directions.begin(),found))&&f.get("rico_bullet")==q,"Mirror turns match independent vector reflection");require(f.get("rico_visited",q*8+d)==1&&f.get("rico_trail",q)==1,"Cycle state records entering direction and visible trail separately");}}
  f.fill("board",48,0);f.fill("rico_visited",384,0);f.fill("rico_trail",48,0);for(const auto& [p,v]:std::array<std::pair<unsigned,unsigned>,4>{{{9,2},{11,3},{27,2},{25,3}}})f.put("board",v,p);f.put("rico_bullet",17);f.put("rico_direction",0);f.put("cursor",41);f.put("rico_active",1);f.put("rico_ammo",2);f.put("phase",2);unsigned count=0;while(f.get("rico_active")&&count++<385)f.call("rico_step");require(count<385&&f.get("phase")==2,"A closed four-mirror orbit terminates on a repeated cell and direction");
  f.fill("board",48,0);f.fill("rico_visited",384,0);f.fill("rico_trail",48,0);for(unsigned p:{9,10,11})f.put("board",4,p);f.put("rico_bullet",8);f.put("rico_direction",2);f.put("rico_active",1);f.put("rico_left",3);f.put("rico_score",0);f.put("rico_score",0,1);f.put("phase",2);for(unsigned i=0;i<3;++i)f.call("rico_step");require(f.get("phase")==4&&f.get("rico_score")==1&&f.get("rico_score",1)==44&&f.get("rico_left")==0,"One shot can pass through three targets and score each exactly once");
 }
 {
  Fixture f(root,"relay-quest");const std::array<int,4> delta{-14,14,-1,1};
  for(unsigned mask=0;mask<16;++mask)for(unsigned hp=1;hp<=9;++hp){f.fill("board",98,0);f.put("cursor",45);for(unsigned d=0;d<4;++d)if(mask&(1U<<d))f.put("board",9,static_cast<unsigned>(45+delta[d]));f.put("quest_hp",hp);f.put("moves",254);f.put("phase",2);f.call("quest_counter");const unsigned count=static_cast<unsigned>((mask&1)+((mask>>1)&1)+((mask>>2)&1)+((mask>>3)&1));require(f.get("quest_hp")==static_cast<unsigned>(hp>count?hp-count:0)&&f.get("phase")==static_cast<unsigned>(hp>count?2:5),"Every adjacent guard counters once without health underflow");require(f.get("moves")==255,"Turn display saturates");}
  for(unsigned hp=1;hp<=9;++hp)for(unsigned pot=0;pot<3;++pot){f.fill("board",98,0);f.put("cursor",45);f.put("quest_hp",hp);f.put("quest_pot",pot);f.put("moves",0);f.put("phase",2);f.call("quest_heal");const bool used=pot&&hp<9;require(f.get("quest_hp")==static_cast<unsigned>(used?std::min(9U,hp+3):hp)&&f.get("quest_pot")==pot-static_cast<unsigned>(used)&&f.get("moves")==static_cast<unsigned>(used),"Medicine consumes exactly one turn and bottle only when useful");}
  for(unsigned direction=0;direction<4;++direction)for(unsigned enemy=7;enemy<=9;++enemy){f.fill("board",98,0);const unsigned p=static_cast<unsigned>(45+delta[direction]);f.put("board",enemy,p);f.put("cursor",45);f.put("quest_aim",direction);f.put("quest_hp",9);f.put("quest_score",0);f.put("quest_score",0,1);f.put("phase",2);f.call("quest_attack");require(f.get("board",p)==static_cast<unsigned>(enemy==9?7:0)&&f.get("quest_hp")==static_cast<unsigned>(enemy==9?8:9)&&f.get("quest_score",1)==static_cast<unsigned>(enemy==9?0:25),"A melee kill prevents that enemy counter while wounded guards retaliate");}
  for(unsigned keys:{0,1}){f.fill("board",98,0);f.put("board",3,46);f.put("cursor",45);f.put("quest_keys",keys);f.put("quest_hp",9);f.put("input_event",8);f.put("phase",2);f.call("game_update");require(f.get("cursor")==static_cast<unsigned>(keys?46:45)&&f.get("quest_keys")==0&&f.get("board",46)==static_cast<unsigned>(keys?0:3),"A locked door consumes a key once and otherwise blocks movement");}
  for(unsigned left:{0,1}){f.fill("board",98,0);f.put("board",6,46);f.put("cursor",45);f.put("quest_left",left);f.put("quest_hp",7);f.put("quest_score",0);f.put("quest_score",0,1);f.put("input_event",8);f.put("phase",2);f.call("game_update");require(f.get("phase")==static_cast<unsigned>(left?2:4)&&f.get("quest_score",1)==static_cast<unsigned>(left?0:70),"The exit requires both relays and awards remaining-health points once");}
 }
 {
  Fixture f(root,"echo-cavern");std::uint32_t random=0x400040;
  for(unsigned trial=0;trial<180;++trial){std::array<unsigned,98> board{},known{};for(unsigned p=0;p<98;++p){random=random*1664525U+1013904223U;board[p]=random%4==0?1:0;f.put("board",board[p],p);}const unsigned start=trial%98;board[start]=0;f.put("board",0,start);f.fill("echo_known",98,0);f.put("echo_mapped",0);f.put("cursor",start);std::vector<std::pair<unsigned,unsigned>> queue{{start,0}};known[start]=1;for(unsigned n=0;n<queue.size();++n){const auto [p,d]=queue[n];for(const auto& [dx,dy]:std::array<std::pair<int,int>,4>{{{0,-1},{0,1},{-1,0},{1,0}}}){const int x=static_cast<int>(p%14)+dx,y=static_cast<int>(p/14)+dy;if(x<0||x>=14||y<0||y>=7)continue;const unsigned q=static_cast<unsigned>(y*14+x);if(known[q])continue;known[q]=1;if(d+1<3&&board[q]!=1)queue.emplace_back(q,d+1);}}f.call("echo_ping");unsigned count=0;for(unsigned p=0;p<98;++p){require(f.get("echo_known",p)==known[p],"Sonar follows bounded shortest paths and cannot cross absorbing walls");count+=known[p];}require(f.get("echo_mapped")==count,"Mapped count includes each newly discovered cell once");f.call("echo_ping");require(f.get("echo_mapped")==count,"Repeated sonar cannot inflate discovery count");}
  for(unsigned air=1;air<=99;++air)for(unsigned tile:{0,2,3,4,5}){f.fill("board",98,0);f.fill("echo_known",98,0);f.put("board",tile,46);f.put("cursor",45);f.put("echo_air",air);f.put("echo_left",0);f.put("echo_score",0);f.put("echo_score",0,1);f.put("phase",2);f.put("input_event",8);f.call("game_update");const unsigned cost=tile==4?3:1;const bool alive=air>cost;const unsigned remaining=alive?(tile==3?std::min(99U,air-cost+24):air-cost):0;require(f.get("echo_air")==remaining&&f.get("phase")==static_cast<unsigned>(!alive?5:tile==5?4:2),"Air is charged before pickups or exit; tanks cap at 99 and spikes cost three");if(tile==3)require(f.get("board",46)==static_cast<unsigned>(alive?0:3),"A tank cannot revive a player whose air already reached zero");}
  for(unsigned air:{1,2,3}){f.put("echo_air",air);f.put("phase",2);f.call("echo_sonar");require(f.get("echo_air")==static_cast<unsigned>(air>2?air-2:0)&&f.get("phase")==static_cast<unsigned>(air>2?2:5),"Sonar uses two air and cannot run on an empty reserve");}
 }
 {
  Fixture f(root,"micro-rogue");const std::array<int,4> delta{-14,14,-1,1};std::uint32_t random=0x410041;
  for(unsigned trial=0;trial<240;++trial){std::array<unsigned,98> board{},distance;distance.fill(255);std::vector<unsigned> floorCells;for(unsigned p=0;p<98;++p){random=random*1664525U+1013904223U;board[p]=p/14==0||p/14==6||p%14==0||p%14==13||random%7==0?1:0;if(!board[p])floorCells.push_back(p);f.put("board",board[p],p);}const unsigned player=floorCells[trial%floorCells.size()];std::array<unsigned,3> positions{},health{};for(unsigned actor=0;actor<3;++actor){positions[actor]=floorCells[(trial+actor+1)%floorCells.size()];health[actor]=actor==2&&trial%3==0?0:5;f.put("rogue_positions",positions[actor],actor);f.put("rogue_health",health[actor],actor);}f.put("cursor",player);f.put("rogue_floor",trial%5);f.put("rogue_armor",trial%3);f.put("rogue_hp",24);f.put("phase",2);std::vector<unsigned> queue{player};distance[player]=0;for(unsigned n=0;n<queue.size();++n)for(int d:delta){const unsigned q=static_cast<unsigned>(static_cast<int>(queue[n])+d);if(!board[q]&&distance[q]==255){distance[q]=distance[queue[n]]+1;queue.push_back(q);}}unsigned hp=24;for(unsigned actor=0;actor<3;++actor){if(!health[actor]||distance[positions[actor]]>=7)continue;if(distance[positions[actor]]==1){const unsigned damage=std::max(1,static_cast<int>(trial%5==4&&actor==0?5:2+(trial%5)/2)-static_cast<int>(trial%3));hp=hp>damage?hp-damage:0;if(!hp)break;}else{unsigned best=positions[actor],bestDistance=distance[best];for(int d:delta){const unsigned q=static_cast<unsigned>(static_cast<int>(positions[actor])+d);bool occupied=false;for(unsigned j=0;j<3;++j)occupied|=health[j]&&positions[j]==q;if(distance[q]<bestDistance&&!occupied){best=q;bestDistance=distance[q];}}positions[actor]=best;}}f.call("rogue_enemies");for(unsigned p=0;p<98;++p)require(f.get("rogue_distance",p)==distance[p],"Dungeon distance fields match independent breadth-first paths");for(unsigned actor=0;actor<3;++actor)require(f.get("rogue_positions",actor)==positions[actor],"Guards pursue in order without overlap or movement-turn attacks");require(f.get("rogue_hp")==hp&&f.get("phase")==static_cast<unsigned>(hp?2:5),"Armor and the final-floor strong enemy apply bounded damage");}
  for(unsigned enemyHp=1;enemyHp<=12;++enemyHp)for(unsigned sword=1;sword<=5;++sword){f.fill("board",98,1);f.put("board",0,45);f.put("board",0,46);f.fill("rogue_health",3,0);f.put("rogue_health",enemyHp);f.put("rogue_positions",46);f.put("cursor",45);f.put("rogue_sword",sword);f.put("rogue_hp",24);f.put("rogue_armor",0);f.put("rogue_floor",0);f.put("rogue_left",1);f.put("rogue_score",0);f.put("rogue_score",0,1);f.put("input_event",8);f.put("phase",2);f.call("game_update");const bool killed=enemyHp<=sword;require(f.get("rogue_health")==static_cast<unsigned>(killed?0:enemyHp-sword)&&f.get("rogue_hp")==static_cast<unsigned>(killed?24:22)&&f.get("rogue_score",1)==static_cast<unsigned>(killed?20:0),"Bump attacks damage once and killed enemies never retaliate");}
  for(unsigned wanted=0;wanted<12;++wanted){unsigned seed=1;while((((seed>>1)^((seed&1)?0xb8:0))%12)!=wanted)++seed;f.put("seed",seed);f.put("stage",2);f.put("rogue_floor",4);f.call("rogue_load_room");require(f.get("rogue_room")==wanted&&f.get("rogue_health")==12&&f.get("rogue_health",1)==7&&f.get("rogue_left")==3,"All twelve templates load their enemy spawns and selected-difficulty strong enemy");f.call("rogue_trace");for(unsigned p=0;p<98;++p)if(f.get("board",p)!=1)require(f.get("rogue_distance",p)!=255,"All items and exits remain connected in every template");}
 }
 {
  Fixture f(root,"signal-ghost");const std::array<std::pair<int,int>,8> vectors{{{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}}};std::uint32_t random=0x420042;
  for(unsigned trial=0;trial<320;++trial){std::array<unsigned,98> board{},vision{};const std::array<unsigned,3> cameras{trial%98,(trial+33)%98,(trial+65)%98},bases{trial%4,(trial/3)%4,(trial/7)%4};const unsigned tick=trial%4,disabled=(trial/4)%4;for(unsigned p=0;p<98;++p){random=random*1664525U+1013904223U;board[p]=random%4==0?1:0;f.put("board",board[p],p);}for(unsigned actor=0;actor<3;++actor){board[cameras[actor]]=0;f.put("board",0,cameras[actor]);f.put("signal_cameras",cameras[actor],actor);f.put("signal_bases",bases[actor],actor);}for(unsigned actor=0;actor<3;++actor){if(disabled&(actor==1?2:1))continue;for(int ray=-1;ray<=1;++ray){const auto [dx,dy]=vectors[(static_cast<int>((bases[actor]+tick)*2)+ray+8)%8];int x=static_cast<int>(cameras[actor]%14),y=static_cast<int>(cameras[actor]/14);for(unsigned n=0;n<4;++n){x+=dx;y+=dy;if(x<0||x>=14||y<0||y>=7)break;const unsigned q=static_cast<unsigned>(y*14+x);if(board[q]==1||std::find(cameras.begin(),cameras.end(),q)!=cameras.end())break;vision[q]=1;}}}f.put("signal_tick",tick);f.put("signal_disabled",disabled);f.call("signal_trace");for(unsigned p=0;p<98;++p)require(f.get("signal_vision",p)==vision[p],"Rotating three-ray cameras clip edges and stop at walls or other cameras for every network mask");}
  f.fill("board",98,0);f.put("board",2,45);f.put("cursor",45);f.put("signal_cameras",43);f.put("signal_cameras",80,1);f.put("signal_cameras",30,2);f.put("signal_bases",0);f.put("signal_bases",0,1);f.put("signal_bases",1,2);f.put("signal_tick",0);f.put("signal_disabled",2);f.put("grid_stat",1);f.put("signal_score",0);f.put("signal_score",0,1);f.put("input_event",16);f.put("phase",2);f.call("game_update");require(f.get("signal_disabled")==3&&f.get("grid_stat")==0&&f.get("signal_score",1)==100&&f.get("phase")==2,"A terminal removes its entire network before the rotated view checks the player");f.call("game_update");require(f.get("signal_score",1)==100&&f.get("grid_stat")==0,"Disabled terminals cannot score or decrement links twice");
  for(unsigned moves:{0,99,100,254,255}){f.fill("board",98,0);f.put("board",4,45);f.put("cursor",45);f.put("signal_disabled",3);f.put("signal_score",0);f.put("signal_score",200,1);f.put("moves",moves);f.put("phase",2);f.call("signal_turn");const unsigned turns=std::min(255U,moves+1),score=200+(turns<100?100-turns:0);require(f.get("phase")==4&&f.get("moves")==turns&&f.get("signal_score")==score/256&&f.get("signal_score",1)==score%256,"Stealth completion bonus saturates safely across the step counter limit");}
 }
 {
  Fixture f(root,"rail-dispatch");const std::array<unsigned,27> next{1,2,3,4,10,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,255,22,23,24,25,26,255};std::uint32_t random=0x430043;
  for(unsigned trial=0;trial<480;++trial){std::array<unsigned,4> position{},target{},proposed{};for(unsigned a=0;a<4;++a){do{random=random*1664525U+1013904223U;position[a]=random%27;}while(position[a]==20||position[a]==26||std::find(position.begin(),position.begin()+a,position[a])!=position.begin()+a);if(a==3&&trial%2)position[a]=255;target[a]=(trial+a)%2;f.put("dispatch_positions",position[a],a);f.put("dispatch_targets",target[a],a);}const unsigned gateA=trial%2,gateB=(trial/2)%2,route=(trial/4)%2;for(unsigned a=0;a<4;++a){const auto p=position[a];proposed[a]=p==255||(p==2&&!gateA)||(p==7&&!gateB)?p:p==14?(route?21:15):next[p];}for(unsigned pass=0;pass<4;++pass)for(unsigned a=0;a<4;++a)if(proposed[a]!=255&&proposed[a]!=position[a])for(unsigned b=0;b<4;++b)if(a!=b&&position[b]==proposed[a]&&proposed[b]==position[b]){proposed[a]=position[a];break;}unsigned reason=0,done=0;for(unsigned a=0;a<4;++a)for(unsigned b=a+1;b<4;++b)if(proposed[a]!=255&&proposed[b]!=255&&(proposed[a]==proposed[b]||(proposed[a]==position[b]&&proposed[b]==position[a])))reason=1;if(!reason)for(unsigned a=0;a<4;++a){position[a]=proposed[a];if(position[a]==20||position[a]==26){if(target[a]!=static_cast<unsigned>(position[a]==26)){reason=2;break;}position[a]=255;++done;}}f.put("dispatch_gates",gateA);f.put("dispatch_gates",gateB,1);f.put("dispatch_switch",route);f.put("dispatch_total",99);f.put("dispatch_next_train",99);f.put("dispatch_time",0);f.put("dispatch_done",0);f.put("dispatch_reason",0);f.put("dispatch_running",1);f.put("phase",2);f.call("dispatch_world");for(unsigned a=0;a<4;++a)require(f.get("dispatch_positions",a)==position[a],"Signals, fixed-point queue blocking and routing preserve train positions");require(f.get("dispatch_reason")==reason&&f.get("dispatch_done")==done&&f.get("phase")==static_cast<unsigned>(reason?5:2),"Merge collisions and wrong destinations fail before invalid further deliveries");}
  f.fill("dispatch_positions",4,255);f.put("dispatch_positions",0);f.put("dispatch_positions",1,1);f.put("dispatch_positions",2,2);f.put("dispatch_gates",0);f.put("dispatch_time",0);f.put("dispatch_done",0);f.put("dispatch_reason",0);f.put("dispatch_next_train",99);f.put("dispatch_total",99);f.put("phase",2);f.call("dispatch_world");require(f.get("dispatch_positions")==0&&f.get("dispatch_positions",1)==1&&f.get("dispatch_positions",2)==2&&f.get("phase")==2,"A stopped signal backs up a multi-train queue without a rear collision");
  f.fill("dispatch_positions",4,255);f.put("dispatch_positions",4);f.put("dispatch_positions",9,1);f.put("dispatch_reason",0);f.put("phase",2);f.call("dispatch_world");require(f.get("dispatch_reason")==1&&f.get("dispatch_positions")==4&&f.get("dispatch_positions",1)==9,"Simultaneous arrivals at the shared merge crash instead of choosing a winner");
  f.fill("dispatch_positions",4,255);f.put("dispatch_positions",19);f.put("dispatch_positions",25,1);f.put("dispatch_targets",0);f.put("dispatch_targets",1,1);f.put("dispatch_done",0);f.put("dispatch_total",2);f.put("dispatch_next_train",2);f.put("dispatch_time",179);f.put("phase",2);f.call("dispatch_world");require(f.get("dispatch_done")==2&&f.get("dispatch_positions")==255&&f.get("dispatch_positions",1)==255&&f.get("phase")==4,"Two correct arrivals at the deadline complete before timeout and are removed once");
 }
 {
  Fixture f(root,"orchard-days");const std::array<unsigned,3> duration{0,3,5};const unsigned weather=f.symbols.at("orchard_levels")+2;f.put("orchard_weather",weather>>8);f.put("orchard_weather",weather&255,1);
  for(unsigned crop=1;crop<=2;++crop)for(unsigned growth=0;growth<=duration[crop];++growth)for(unsigned wet:{0,1})for(unsigned dry:{0,1})for(unsigned rain:{0,1}){f.fill("orchard_crops",72,0);f.put("orchard_crops",crop);f.put("orchard_growth",growth);f.put("orchard_wet",wet);f.put("orchard_dry",dry);f.put("orchard_day",0);f.put("orchard_limit",8);f.put("orchard_rain",rain);f.put("phase",2);unsigned C=crop,G=growth,D=dry;if(G<duration[C]){if(wet||rain){++G;D=0;}else if(++D==2){C=G=D=0;}}f.call("orchard_night");require(f.get("orchard_crops")==C&&f.get("orchard_growth")==G&&f.get("orchard_wet")==0&&f.get("orchard_dry")==D&&f.get("orchard_actions")==3&&f.get("orchard_day")==1,"Night growth, rainy recovery, dry-day death and mature crops obey the complete state matrix");}
  for(unsigned seed:{1,2})for(unsigned coins=0;coins<=4;++coins)for(unsigned actions:{0,1}){f.fill("orchard_crops",72,0);f.put("cursor",0);f.put("orchard_seed",seed);f.put("orchard_coins",0);f.put("orchard_coins",coins,1);f.put("orchard_actions",actions);f.call("orchard_work");const unsigned cost=seed==1?2:3;const bool planted=actions&&coins>=cost;require(f.get("orchard_crops")==static_cast<unsigned>(planted?seed:0)&&f.get("orchard_coins",1)==coins-(planted?cost:0)&&f.get("orchard_actions")==actions-static_cast<unsigned>(planted),"Planting never underflows money or spends unavailable work");}
  f.fill("orchard_crops",72,0);f.put("orchard_crops",2);f.put("orchard_growth",5);f.put("cursor",0);f.put("orchard_coins",0);f.put("orchard_coins",250,1);f.put("orchard_goal",60);f.put("orchard_actions",1);f.put("phase",2);f.call("orchard_work");require(f.get("orchard_coins")==1&&f.get("orchard_coins",1)==6&&f.get("orchard_crops")==0&&f.get("phase")==4,"Harvest money carries across the byte boundary and clears only after payment");
  f.put("orchard_day",7);f.put("orchard_limit",8);f.put("orchard_crops",1);f.put("orchard_growth",2);f.put("orchard_wet",1);f.put("phase",2);f.call("orchard_night");require(f.get("phase")==5&&f.get("orchard_day")==7&&f.get("orchard_growth")==2,"Ending the final day cannot grant an extra growing or harvesting day");
 }
 {
  Fixture f(root,"market-harbor");const std::array<std::array<int,3>,4> base{{{4,14,18},{10,5,15},{8,12,6},{12,16,22}}};const std::array<std::array<int,8>,3> season{{{0,-2,1,2,-1,0,1,-1},{0,2,-1,3,-2,1,-3,0},{0,-3,2,4,-2,1,-4,3}}};
  for(unsigned port=0;port<4;++port)for(unsigned good=0;good<3;++good)for(unsigned day=0;day<24;++day)for(unsigned offset=0;offset<8;++offset){f.put("market_calc_day",day);f.put("market_offset",offset);f.call("market_price",good,port);require(f.cpu.state().a==std::max(1,base[port][good]+season[good][(day+offset)%8]),"Every port and seasonal phase uses bounded independent integer prices");}
  for(unsigned price=1;price<=26;++price)for(unsigned cash:{0,1,25,255,260}){f.fill("market_cargo",3,0);f.put("market_prices",price);f.put("market_good",0);f.put("market_mode",0);f.put("market_load",0);f.put("market_capacity",6);f.put("market_cash",cash>>8);f.put("market_cash",cash&255,1);f.put("market_goal",3);f.put("market_goal",232,1);f.put("phase",2);f.call("market_trade");const bool bought=cash>=price;require(f.get("market_cargo")==static_cast<unsigned>(bought)&&f.get("market_load")==static_cast<unsigned>(bought)&&f.get("market_cash")*256U+f.get("market_cash",1)==cash-(bought?price:0),"A purchase checks funds before subtracting across byte boundaries");if(bought){f.put("market_mode",1);f.call("market_trade");require(f.get("market_load")==0&&f.get("market_cash")*256U+f.get("market_cash",1)==cash,"An immediate same-price resale conserves total cash and cargo");}}
  for(unsigned port=0;port<4;++port)for(unsigned dest=0;dest<4;++dest){f.put("market_port",port);f.call("market_distance",0,dest);const unsigned clockwise=(dest+4-port)%4,counter=(port+4-dest)%4;require(f.cpu.state().a==std::min(clockwise,counter),"Voyages use the shorter route around the four-port ring");}
  f.put("market_port",0);f.put("market_destination",2);f.put("market_day",0);f.put("market_limit",8);f.put("market_cash",0);f.put("market_cash",3,1);f.put("market_load",1);f.put("selection_active",1);f.put("market_message",0);f.put("phase",2);f.call("market_sail");require(f.get("market_message")==1&&f.get("market_cash",1)==3&&f.get("market_day")==0&&f.get("market_port")==0&&f.get("selection_active")==1,"Insufficient fare preserves the voyage and all economic state");
  f.put("market_destination",1);f.put("market_load",0);f.put("market_offset",0);f.put("phase",2);f.call("market_sail");require(f.get("market_cash",1)==1&&f.get("market_day")==1&&f.get("market_port")==1&&f.get("market_reason")==2&&f.get("phase")==5,"An empty ship with less than the minimum next fare fails after arrival");
  f.put("market_port",0);f.put("market_destination",2);f.put("market_day",7);f.put("market_limit",8);f.put("market_cash",0);f.put("market_cash",40,1);f.put("selection_active",1);f.put("phase",2);f.call("market_sail");require(f.get("market_reason")==1&&f.get("phase")==5&&f.get("market_cash",1)==40&&f.get("market_day")==7&&f.get("market_port")==0,"Over-deadline confirmation fails before spending fare or changing the port");
 }
 {
  Fixture f(root,"wind-putt");const auto putWord=[&](const std::string& n,int v){f.put(n,static_cast<unsigned>(v)>>8);f.put(n,v&255,1);};const auto word=[&](const std::string& n){return f.get(n)*256+f.get(n,1);};
  std::array<unsigned,112> board{};for(unsigned p=0;p<112;++p){board[p]=(p%16==0||p%16==15||p/16==0||p/16==6||(p%16==7&&p/16!=3))?1:0;f.put("putt_board",board[p],p);}const auto blocked=[&](int x,int y){if(x<1||x>125||y<9||y>61)return true;for(const auto& [dx,dy]:std::array<std::pair<int,int>,4>{{{0,0},{1,0},{0,1},{1,1}}})if(board[((y+dy-8)/8)*16+(x+dx)/8]==1)return true;return false;};
  for(unsigned y=8;y<64;++y)for(unsigned x=0;x<128;++x){f.call("putt_blocked",y,x);require(f.cpu.state().a==static_cast<unsigned>(blocked(x,y)),"Every pixel and tile seam checks all four corners of the ball");}
  const auto half=[](int v){return v>=0?v/2:-((-v+1)/2);};std::uint32_t random=0x460046;
  for(unsigned trial=0;trial<600;++trial){random=random*1664525U+1013904223U;int x=(10+random%108)*256+(random>>16)%256,y=(17+(random>>8)%37)*256+(random>>24);int vx=static_cast<int>(random%577)-288,vy=static_cast<int>((random>>12)%513)-256;const unsigned left=trial%3==0?8:trial%3==1?1:23;putWord("putt_x",x);putWord("putt_y",y);putWord("putt_vx",vx);putWord("putt_vy",vy);f.put("putt_left",left);f.put("putt_shots",trial%12+1);f.put("putt_cup_x",250);f.put("putt_cup_y",250);f.put("phase",2);if(left==8){vx=half(vx);vy=half(vy);}int q=x+vx;if(blocked(q>>8,y>>8))vx=-vx;else x=q;q=y+vy;if(blocked(x>>8,q>>8))vy=-vy;else y=q;if(left==1){x&=65280;y&=65280;}f.call("putt_step");require(word("putt_x")==x&&word("putt_y")==y&&word("putt_vx")==static_cast<int>(static_cast<unsigned>(vx)&65535)&&word("putt_vy")==static_cast<int>(static_cast<unsigned>(vy)&65535)&&f.get("putt_left")==left-1,"Signed fixed-point movement, axis reflections and final slowdown match an independent model");require(f.get("phase")==static_cast<unsigned>(left==1&&trial%12==11?5:2),"The final unsuccessful twelfth stroke fails after motion ends");}
  for(unsigned aim=0;aim<8;++aim)for(int wind:{-1,0,1})for(unsigned power=1;power<=8;++power){const std::array<std::pair<int,int>,8> vector{{{0,-256},{181,-181},{256,0},{181,181},{0,256},{-181,181},{-256,0},{-181,-181}}};f.put("putt_left",0);f.put("putt_aim",aim);f.put("putt_power",power);f.put("putt_wind",wind&255);f.put("putt_shots",0);f.call("putt_launch");require(word("putt_vx")==static_cast<int>(static_cast<unsigned>(vector[aim].first+wind*32)&65535)&&word("putt_vy")==static_cast<int>(static_cast<unsigned>(vector[aim].second)&65535)&&f.get("putt_left")==power*8&&f.get("putt_shots")==1,"Every aim, crosswind and power creates one bounded stroke");}
  f.fill("putt_board",112,0);putWord("putt_x",40*256);putWord("putt_y",32*256);putWord("putt_vx",0);putWord("putt_vy",0);f.put("putt_cup_x",40);f.put("putt_cup_y",32);f.put("putt_left",1);f.put("putt_shots",12);f.put("phase",2);f.call("putt_step");require(f.get("phase")==4&&f.get("putt_left")==0,"Cup capture on the last available stroke precedes failure");
 }
 {
  Fixture f(root,"rally-return");const auto deflect=[](unsigned p){return p<3?-3:p<6?-1:p<10?1:3;};
  for(unsigned impact=0;impact<=12;++impact){f.call("rally_deflect",0,impact);require(f.cpu.state().a==static_cast<unsigned>(deflect(impact)&255),"Every paddle impact zone has the specified return angle");}
  for(unsigned paddle=9;paddle<=51;++paddle)for(unsigned y=9;y<=61;++y)for(int spin:{-1,0,1})for(unsigned side:{0,1}){f.put("rally_x",side?114:10);f.put("rally_y",y);f.put("rally_dx",side?2:254);f.put("rally_dy",0);f.put("rally_player",paddle);f.put("rally_cpu",paddle);f.put("rally_spin",spin&255);f.put("rally_you",0);f.put("rally_them",0);f.put("phase",2);f.call("rally_motion");const int r=static_cast<int>(y+1)-static_cast<int>(paddle);const bool hit=r>=0&&r<=12;require(f.get("rally_x")==static_cast<unsigned>(hit?(side?114:10):(side?116:8))&&f.get("rally_dx")==static_cast<unsigned>((side?(hit?-2:2):(hit?2:-2))&255),"Both paddles use complete ball overlap and reflect only on the crossing edge");if(hit)require(f.get("rally_dy")==static_cast<unsigned>((side?deflect(r):std::clamp(deflect(r)+spin,-3,3))&255),"Player spin is added once and clamped independently of the CPU angle");}
  for(unsigned y=9;y<=61;++y)for(int vy=-3;vy<=3;++vy){f.put("rally_x",60);f.put("rally_dx",2);f.put("rally_y",y);f.put("rally_dy",vy&255);f.call("rally_motion");const int next=static_cast<int>(y)+vy;const bool bounce=next<9||next>61;require(f.get("rally_y")==static_cast<unsigned>(bounce?y:next)&&f.get("rally_dy")==static_cast<unsigned>((bounce?-vy:vy)&255),"Top and bottom walls preserve the ball for the contact tick and reverse signed velocity");}
  for(unsigned side:{0,1}){f.put("rally_x",side?124:2);f.put("rally_dx",side?2:254);f.put("rally_y",32);f.put("rally_dy",0);f.put("rally_you",4);f.put("rally_them",4);f.put("rally_active",1);f.put("phase",2);f.call("rally_motion");require(f.get("rally_you")==4+side&&f.get("rally_them")==5-side&&f.get("phase")==5-side&&f.get("rally_active")==0&&f.get("rally_x")==60,"The fifth point ends the match once and returns to a stationary serve state");}
  for(unsigned period=1;period<=3;++period)for(unsigned clock=0;clock<period;++clock){f.put("rally_x",60);f.put("rally_y",45);f.put("rally_dx",2);f.put("rally_dy",0);f.put("rally_cpu",30);f.put("rally_ai_period",period);f.put("rally_ai_clock",clock);f.call("rally_world");require(f.get("rally_cpu")==30+static_cast<unsigned>(clock+1==period)&&f.get("rally_ai_clock")==static_cast<unsigned>((clock+1)%period),"CPU movement obeys its difficulty-specific emulated-tick cadence");}
 }
 {
  Fixture f(root,"penalty-arc");const std::array<int,5> targets{24,44,64,84,104};
  for(unsigned power=0;power<=10;++power)for(unsigned height=0;height<2;++height)for(unsigned target=0;target<5;++target)for(unsigned keeper=16;keeper<=104;++keeper){f.put("penalty_power",power);f.put("penalty_height",height);f.put("penalty_target",target);f.put("penalty_keeper",keeper);f.put("penalty_kicks",0);f.put("penalty_goals",0);f.put("phase",2);f.call("penalty_result");const bool valid=height?(power==5||power==6):(power>=4&&power<=7);const unsigned result=!valid?3:std::abs(targets[target]-static_cast<int>(keeper))<=static_cast<int>(height?5:8)?2:1;require(f.get("penalty_reason")==result&&f.get("penalty_kicks")==1&&f.get("penalty_goals")==static_cast<unsigned>(result==1)&&f.get("penalty_mode")==3,"All power windows, goal lanes, shot heights and keeper reach boundaries classify exactly once");}
  for(unsigned stage=0;stage<3;++stage)for(unsigned keeper=16;keeper<=104;++keeper)for(int direction:{-1,1}){f.put("stage",stage);f.put("penalty_keeper",keeper);f.put("penalty_keeper_dir",direction&255);f.call("penalty_patrol");const int candidate=static_cast<int>(keeper)+direction*static_cast<int>(stage+2);const int next=std::clamp(candidate,16,104),nextDir=candidate<16?1:candidate>104?-1:direction;require(f.get("penalty_keeper")==static_cast<unsigned>(next)&&f.get("penalty_keeper_dir")==static_cast<unsigned>(nextDir&255),"Patrol reflects at both goal boundaries for every difficulty");}
  const auto floor16=[](int n){return n>=0?n/16:-((-n+15)/16);};for(unsigned stage=0;stage<3;++stage)for(unsigned target=0;target<5;++target)for(unsigned height=0;height<2;++height)for(unsigned progress=1;progress<=16;++progress){f.put("stage",stage);f.put("penalty_mode",2);f.put("penalty_progress",progress-1);f.put("penalty_target",target);f.put("penalty_height",height);f.put("penalty_keeper",64);f.put("penalty_keeper_dir",1);f.put("penalty_power",5);f.put("penalty_kicks",0);f.put("penalty_goals",0);f.put("phase",2);f.call("penalty_world");const int difference=targets[target]-64;const unsigned keeper=progress<=4?66+stage:static_cast<unsigned>(64+(difference==0?0:(difference>0?1:-1))*static_cast<int>(stage+1));require(f.get("penalty_x")==64+floor16((targets[target]-64)*static_cast<int>(progress))&&f.get("penalty_y")==60+floor16(((height?14:24)-60)*static_cast<int>(progress))&&f.get("penalty_keeper")==keeper,"Flight tables and delayed keeper dives match independent integer interpolation");}
  for(unsigned stage=0;stage<3;++stage)for(unsigned goals=0;goals<=9;++goals){f.put("stage",stage);f.put("penalty_kicks",9);f.put("penalty_goals",goals);f.put("penalty_power",0);f.put("penalty_height",0);f.put("phase",2);f.call("penalty_result");require(f.get("penalty_kicks")==10&&f.get("penalty_goals")==goals&&f.get("phase")==static_cast<unsigned>(goals>=6+stage?4:5),"The tenth completed kick applies the selected challenge threshold");}
 }
 {
  Fixture f(root,"beat-step");const std::array<unsigned,4> masks{4,2,1,8};const auto putWord=[&](const std::string& n,unsigned v){f.put(n,v>>8);f.put(n,v&255,1);};const auto word=[&](const std::string& n){return f.get(n)*256U+f.get(n,1);};
  for(unsigned previous=0;previous<16;++previous)for(unsigned held=0;held<16;++held){f.put("beat_previous",previous);f.put("input_held",held);f.call("beat_edges");require(f.get("beat_previous")==held&&f.get("beat_pressed")==((~previous)&held&15),"Every combination of held logical directions produces only fresh edges");}
  for(unsigned lane=0;lane<4;++lane)for(unsigned window:{5,6,8})for(int offset=-12;offset<=12;++offset)for(unsigned pressed=0;pressed<16;++pressed){f.fill("beat_notes",64,lane);f.put("beat_interval",40);f.put("beat_window",window);f.put("beat_total",4);f.put("beat_index",0);f.put("beat_active",1);f.put("beat_life",6);f.put("beat_combo",7);f.put("beat_judgement",0);f.put("beat_pressed",pressed);f.put("phase",2);putWord("beat_due",500);putWord("beat_time",500+offset);putWord("beat_score",250);f.call("beat_rules");const bool expired=offset>static_cast<int>(window),correct=pressed==masks[lane]&&std::abs(offset)<=static_cast<int>(window),damage=expired||(pressed&&!correct);const unsigned score=250+(correct?(std::abs(offset)<=2?100:60):0);require(word("beat_score")==score&&f.get("beat_index")==static_cast<unsigned>(expired||correct)&&word("beat_due")==500+40*static_cast<unsigned>(expired||correct)&&f.get("beat_life")==6-static_cast<unsigned>(damage)&&f.get("beat_combo")==static_cast<unsigned>(damage?0:correct?8:7),"All directions, chords, early/late window edges, expiry and byte-carry scores match independent timing rules");require(f.get("beat_judgement")==static_cast<unsigned>(damage?3:correct?(std::abs(offset)<=2?1:2):0),"PERFECT, GOOD and MISS match exact timing distance");}
  f.put("beat_interval",24);f.put("beat_window",5);f.put("beat_total",64);f.put("beat_index",0);f.put("beat_active",1);f.put("beat_life",6);f.put("beat_pressed",4);f.put("phase",2);putWord("beat_due",64);putWord("beat_time",1000);f.call("beat_rules");require(f.get("beat_life")==0&&f.get("beat_index")==6&&f.get("phase")==5&&word("beat_due")==208,"A delayed update consumes each missed note once and stops immediately on the sixth miss");
  for(unsigned life:{1,2}){f.put("beat_interval",40);f.put("beat_window",8);f.put("beat_total",1);f.put("beat_index",0);f.put("beat_active",1);f.put("beat_life",life);f.put("beat_pressed",0);f.put("phase",2);putWord("beat_due",500);putWord("beat_time",509);f.call("beat_rules");require(f.get("beat_index")==1&&f.get("beat_life")==life-1&&f.get("phase")==static_cast<unsigned>(life==1?5:4),"The final chart miss clears only if a life remains");}
  for(unsigned clock:{0,1,240,250,255})for(unsigned delta:{0,1,7,20}){f.put("beat_active",1);f.put("beat_clock",clock);f.put("input_ticks",(clock+delta)&255);f.put("input_held",0);f.put("input_event",0);f.put("resume_pending",0);f.put("beat_previous",0);f.put("beat_draw_count",0);f.put("beat_index",0);f.put("beat_total",32);f.put("beat_life",6);f.put("phase",2);putWord("beat_time",10);putWord("beat_due",1000);putWord("beat_sound_due",3000);f.call("game_update");require(word("beat_time")==10+delta,"CPU tick accumulation stays exact across the eight-bit timer wrap");}
 }
 {
  Fixture f(root,"balance-dock");const auto word=[&](const std::string& n){return f.get(n)*256U+f.get(n,1);};
  for(unsigned width=1;width<=128;++width)for(unsigned left:{0U,(128-width)/2,128-width})for(unsigned x=0;x<=128-width;++x){f.put("dock_width",width);f.put("dock_left",left);f.put("dock_x",x);f.put("dock_y",58);f.put("dock_count",0);f.put("dock_goal",20);f.put("dock_score",0);f.put("dock_score",250,1);f.put("dock_mode",2);f.put("phase",2);f.put("stage",2);f.call("dock_land");const unsigned lo=std::max(x,left),hi=std::min(x+width,left+width);const bool overlap=hi>lo;if(overlap){const unsigned kept=hi-lo;require(f.get("dock_left")==lo&&f.get("dock_width")==kept&&f.get("dock_layers_left")==lo&&f.get("dock_layers_width")==kept&&f.get("dock_count")==1&&word("dock_score")==250+kept+(kept==width?20:0)&&f.get("dock_mode")==1,"All widths and relative landing offsets preserve only the supported interval and score it once");}else require(f.get("phase")==5&&f.get("dock_count")==0&&f.get("dock_width")==width&&word("dock_score")==250,"An unsupported crate fails without changing the completed stack or score");}
  for(unsigned width:{1,2,3,8,40,80,128})for(unsigned x=0;x<=128-width;++x)for(unsigned speed=1;speed<=4;++speed)for(int dir:{-1,1}){f.put("dock_mode",1);f.put("dock_x",x);f.put("dock_width",width);f.put("dock_speed",speed);f.put("dock_direction",dir&255);f.call("dock_world");const int candidate=static_cast<int>(x)+dir*static_cast<int>(speed),limit=static_cast<int>(128-width);const unsigned next=static_cast<unsigned>(std::clamp(candidate,0,limit));const int direction=candidate<0?1:candidate>limit?-1:dir;require(f.get("dock_x")==next&&f.get("dock_direction")==static_cast<unsigned>(direction&255),"Crates of every representative width reflect at the visible court boundaries");}
  for(unsigned mode=0;mode<4;++mode)for(unsigned flips:{0,1,3}){f.put("dock_mode",mode);f.put("dock_flips",flips);f.put("dock_direction",1);f.call("game_aux",0,1);const bool used=mode==1&&flips>0;require(f.get("dock_flips")==flips-static_cast<unsigned>(used)&&f.get("dock_direction")==static_cast<unsigned>(used?255:1),"FLIP consumes one token only while a crate is moving horizontally");}
  for(unsigned goal:{12,16,20}){f.put("dock_count",goal-1);f.put("dock_goal",goal);f.put("dock_left",24);f.put("dock_x",24);f.put("dock_width",80);f.put("dock_score",0);f.put("dock_score",250,1);f.put("dock_mode",2);f.put("phase",2);f.call("dock_land");require(f.get("dock_count")==goal&&f.get("phase")==4&&f.get("dock_mode")==3&&word("dock_score")==350&&f.get("dock_layers_width",goal-1)==80,"The last supported crate completes at each difficulty's target height");}
  for(unsigned width:{1,2,3,8,80,128})for(unsigned left:{0U,(128-width)/2,128-width})for(unsigned y=8;y<=60;y+=2)for(unsigned colour:{0,1}){f.fill("framebuffer",1536,165);f.put("dock_span_left",left);f.put("dock_span_size",width);f.put("dock_span_y",y);f.put("dock_colour",colour);f.put("dock_pattern",1);f.call("dirty_reset");f.call("dock_span");bool any_changed=false;for(unsigned q=left;q<left+width;++q){const unsigned mask=3U<<(y%8),top=1U<<(y%8),pixel=(165&~mask)|(colour?((q+1)%4?mask:top):0);any_changed=any_changed||pixel!=165;}for(unsigned band=0;band<8;++band){require(f.get("dirty_min",band)==(any_changed&&band==y/8?left+f.get("view_origin"):192),"Dock LCD range starts at physical shifted x");require(f.get("dirty_max",band)==(any_changed&&band==y/8?left+width-1+f.get("view_origin"):0),"Dock LCD range ends at physical shifted x");}const unsigned mask=3U<<(y%8),top=1U<<(y%8);for(unsigned band=0;band<8;++band)for(unsigned x=0;x<192;++x){unsigned expected=165;if(band==y/8&&x>=left+f.get("view_origin")&&x<left+width+f.get("view_origin"))expected=(165&~mask)|(colour?((x-f.get("view_origin")+1)%4?mask:top):0);require(f.get("framebuffer",band*192+x)==expected,"Two-pixel spans preserve all other rows, LCD controllers and neighboring HUD columns");}}
 }
 std::cout<<"PASS: HUD fonts/cache/viewport/dialog/gauges, laser cycles, twelve card effects, costs, shield/poison, factory contention/conversion/shipping, crater edges, connect-four windows and tactics, reversi rays, five-stone windows and open ends, hex distance fields, pawn movement boundaries, dot box ownership, pipe loops and leaks, number merges and terminal states, mine first-click safety and flood fill, loop checkpoints and closure, pyramid availability and pairs, golf rank wrapping and coverage, all dice category scores and bonus, risk banking thresholds and limits, brick and paddle contacts, snake boundaries and vacating tail, maze distance fields and ghost contact, river traffic and home bays, tower landings and checkpoints, bomb rays and vault keys, trail racing CPU and simultaneous contacts, gravity masks and unique stars, patrol projectiles and shield durability, orbital nearest hits and rotating approach, target deadlines and score thresholds, ricochet vectors and cycle guards, quest resources and melee order, sonar occlusion and oxygen ordering, dungeon pursuit and equipment, rotating cameras and network timing, railway queues and merge safety, orchard growth and daily budgets, trading prices and voyage accounting, mini-golf fixed-point contacts, paddle spin and match scoring, penalty power and goalkeeper reach, rhythm edges and timing windows, cargo overlap and exact masked drawing\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
