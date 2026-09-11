// SPDX-License-Identifier: MIT
// Native replays the exact physical keys used by the WASM game acceptance tests.
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "jr800/core/jr800_machine.hpp"
#include "jr800/formats/jr8app.hpp"
using namespace jr800::core;
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
struct Metrics final:MachineObserver {
    std::uint64_t last_scan{},max_scan_gap{},lcd_bytes{},update_cycle{},max_update{},max_play_update{};
    std::uint16_t min_sp{0x5fff},update_pc{};
    bool updating{},playing{};
    void on_machine_detached(Machine&) noexcept override {}
    void on_step_begin(const CpuState& s) noexcept override {
        if(s.pc==update_pc){update_cycle=s.cycle_count;updating=true;}
    }
    void on_step_end(const StepResult&,const CpuState& s) noexcept override {if(s.knowledge.knows(CpuRegister::stack_pointer))min_sp=std::min(min_sp,s.sp);}
    void on_bus_access(const BusAccessEvent& e) noexcept override {
        if(e.kind==AccessKind::data_read&&e.address==0x0ffd){
            if(last_scan)max_scan_gap=std::max(max_scan_gap,e.instruction_cycle-last_scan);
            last_scan=e.instruction_cycle;
        }
        if(e.kind==AccessKind::data_write&&e.address>=0x0b00&&e.address<0x0c00)++lcd_bytes;
    }
};
int main(int argc,char** argv){try {
    require(argc==4,"Usage: game_replay_test game.j8a replay.txt metrics.json");
    std::ifstream input(argv[1],std::ios::binary);
    require(input.good(),"Missing application");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    const auto app=jr800::formats::jr8app::read(bytes);
    Jr800ExperimentalMachineConfiguration config;
    config.internal_ram=Jr800ExperimentalInternalRamConfiguration{0};
    config.memory=Jr800ExperimentalMemoryConfiguration{0,std::nullopt};
    config.lcd=Jr800ExperimentalLcdConfiguration{0};
    config.calendar=Jr800ExperimentalCalendarConfiguration{
        Jr800ExperimentalCalendarAddressSource::cpu_a0_to_a3,
        Jr800ExperimentalCalendarUpperReadBits::all_zero,
        Jr800ExperimentalCalendarCpuCycleRatio::e030_nominal_1_2288_mhz};
    Jr800Machine machine(config);
    std::vector<std::uint8_t> rom(32768,1);rom[0]=0x20;rom[1]=0xfe;rom[32766]=0x80;rom[32767]=0;
    require(machine.load_logical_rom(rom)==Jr800MemoryStatus::ok,"Synthetic bootstrap");
    require(machine.initialize_from_reset_entry().succeeded(),"Device reset");
    machine.set_port1_pin_state(255,255);machine.set_port2_pin_state(0x1e,0x1f);
    machine.set_ram_standby_power_valid(false,true);
    for(unsigned a=0x0c00;a<0x1000;++a)require(machine.set_keyboard_bus_response(a,255,true),"Keyboard setup");
    unsigned allocated=0;
    for(const auto& segment:app.segments){
        require(segment.address>=0x2800&&segment.address+segment.logical_size<=0x5e00,"Preserved BASIC workspace and standard RAM/stack boundary");
        const auto status=segment.kind==jr800::formats::jr8app::SegmentKind::zero_fill
            ? machine.host_fill_ram(segment.address,segment.logical_size,0)
            : machine.host_load_ram(segment.address,segment.data);
        require(status==Jr800MemoryStatus::ok,"RAM load");allocated+=segment.logical_size;
    }
    machine.host_start_program(app.entry_point);
    auto& execution=machine.execution();
    std::ifstream replay(argv[2]);unsigned frame_pc,fb,phase,update_pc;
    require(bool(replay>>frame_pc>>fb>>phase>>update_pc),"Replay header");
    Metrics metrics;metrics.update_pc=update_pc;
    require(execution.add_observer(&metrics),"Metrics observer");
    const std::array<Jr800Key,10> keys{Jr800Key::keypad_8,Jr800Key::keypad_2,Jr800Key::keypad_4,Jr800Key::keypad_6,Jr800Key::space,Jr800Key::return_key,Jr800Key::letter_w,Jr800Key::letter_s,Jr800Key::letter_a,Jr800Key::letter_d};
    auto read=[&](unsigned a){auto r=execution.inspect8(a);require(r.succeeded(),"RAM inspect");return *r.value;};
    auto step=[&](){auto r=execution.step_instruction();require(r.succeeded(),"Native instruction at "+std::to_string(r.pc_before));};
    unsigned mask,expected_hash,expected_bytes,expected_phase,frames=0,maximum_data=0;
    std::uint64_t expected_cycles;
    while(replay>>mask>>expected_hash>>expected_cycles>>expected_bytes>>expected_phase){
        for(unsigned i=0;i<keys.size();++i)require(machine.set_keyboard_key_state(keys[i],mask&(1U<<i)),"Key event");
        const auto before_lcd=metrics.lcd_bytes;
        metrics.playing=frames&&read(phase)==2;
        if(frames)step();
        unsigned steps=0;
        while(execution.cpu().state().pc!=frame_pc&&steps++<1000000)step();
        require(execution.cpu().state().pc==frame_pc,"Native frame timeout");
        require(execution.cpu().state().sp==0x5fff,"Unbalanced stack");
        require(execution.cpu().state().cycle_count==expected_cycles,"Native/WASM cycle mismatch at frame "+std::to_string(frames));
        require(read(phase)==expected_phase,"Native/WASM phase mismatch");
        std::uint32_t hash=2166136261U;
        for(unsigned i=0;i<1536;++i)hash=(hash^read(fb+i))*16777619U;
        require(hash==expected_hash,"Native/WASM framebuffer mismatch");
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<192;++x)
            require(machine.lcd_panel_dot(x,y)==bool(read(fb+(y/8)*192+x)&(1U<<(y%8))),"Native LCD mismatch");
        if(frames)require(metrics.lcd_bytes-before_lcd==expected_bytes,"Native/WASM LCD transfer mismatch");
        maximum_data=std::max(maximum_data,expected_bytes);
        if(metrics.updating){
            const auto cycles=expected_cycles-metrics.update_cycle;
            metrics.max_update=std::max(metrics.max_update,cycles);
            if(metrics.playing&&expected_phase==2)metrics.max_play_update=std::max(metrics.max_play_update,cycles);
        }
        ++frames;
    }
    require(frames>10,"Replay is incomplete");
    require(metrics.min_sp>=0x5e00,"Stack reserve exhausted");
    std::ofstream output(argv[3]);
    output<<"{\n  \"frames\": "<<frames<<",\n  \"allocatedBytes\": "<<allocated
        <<",\n  \"stackPeakBytes\": "<<0x5fff-metrics.min_sp
        <<",\n  \"stackReserveBytes\": 512,\n  \"maxKeyScanGapCycles\": "<<metrics.max_scan_gap
        <<",\n  \"maxUpdateCycles\": "<<metrics.max_update
        <<",\n  \"maxPlayUpdateCycles\": "<<metrics.max_play_update
        <<",\n  \"maxLcdDataBytes\": "<<maximum_data<<"\n}\n";
    std::cout<<"PASS native/WASM replay: "<<frames<<" frames, stack="<<0x5fff-metrics.min_sp<<" bytes, key gap="<<metrics.max_scan_gap<<" cycles\n";
    execution.remove_observer(metrics);
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
