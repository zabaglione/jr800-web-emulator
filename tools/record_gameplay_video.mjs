// SPDX-License-Identifier: MIT
// Offline LCD/speaker capture at the emulator's normal CPU clock. Input only;
// no game RAM patches, frame stretching, ROM files, or host-side game rendering.
import assert from 'node:assert/strict';
import {readFileSync, writeFileSync, mkdirSync, openSync, writeSync, closeSync, unlinkSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {png} from '../games/tools/harness.mjs';
import {Jr800AudioSignal} from '../web/audio-signal.mjs';
import {JR800_CPU_CLOCK_HZ as HZ} from '../web/run-pacer.mjs';
import {play} from './video_playthroughs.mjs';

const [id, output = 'build/game-videos-review', secondsText = '30'] = process.argv.slice(2);
const root = resolve(import.meta.dirname, '..'), seconds = Number(secondsText), fps = 30, rate = 48000;
const catalog = JSON.parse(readFileSync(resolve(root,'games/catalog.json'))).programs;
assert(catalog.some(g => g.id === id), 'Choose a catalog game');
assert(seconds > 0 && seconds <= 60 && Number.isInteger(seconds * fps));
assert(!process.env.JR800_GAME_ROM && !process.env.JR800_SAMPLE_ROM, 'Capture uses the project-authored bootstrap only');
const out = resolve(output,id); mkdirSync(out,{recursive:true});
const build = resolve(root,'build/games',id), wasm = resolve(root,'build/wasm-release/web-module');
const url = n => pathToFileURL(resolve(wasm,n)).href;
const {WasmMachine} = await import(url('wasm-machine.mjs'));
const {jr800BasicBootExperimentConfiguration} = await import(url('basic-boot-profile.mjs'));
const config = {...jr800BasicBootExperimentConfiguration()}; delete config.expansionRamInitialValue;
const machine = await WasmMachine.createJr800(url('jr800_wasm.mjs'),config);
const symbols = Object.fromEntries([...readFileSync(resolve(build,id+'.sym'),'utf8').matchAll(/^ \$([0-9A-F]+) G (\S+)/gm)].map(([,a,n]) => [n,parseInt(a,16)]));
if(id==='relic-dive-gfx')for(const [,name,value] of readFileSync(resolve(root,'sdk/examples/lcd/07-relic-dive/constants.inc'),'utf8').matchAll(/\.equ (\w+), (\$[\dA-F]+|\d+)/g))symbols[name]=value.startsWith('$')?parseInt(value.slice(1),16):Number(value);
const bootstrap = new Uint8Array(32768).fill(1); bootstrap.set([0x20,0xfe]); bootstrap[32766]=0x80; bootstrap[32767]=0;
machine.loadLogicalRom(bootstrap);
const app = readFileSync(resolve(build,id+'.j8a')); machine.loadProgram(app);
assert.equal(machine.runTo(symbols.frame_ready,1000000).reason,'address-reached');
const start = Number(machine.state().cycleCount), end = start + Math.round(seconds*HZ);
const videoPath=resolve(out,'lcd.rgb'), audioPath=resolve(out,'speaker.f32');
const video=openSync(videoPath,'w'), audio=openSync(audioPath,'w');
const filter={firstAddress:2,lastAddress:2,kindMask:4};
let lastSequence=0, level=false;
for(const a of machine.accesses(filter)){lastSequence=Number(a.sequence);if(a.valueKnown)level=!!(a.value&16);}
const signal=new Jr800AudioSignal(HZ,rate,start,level), events=[], changes=[], seen=new Set(), held=new Set();
let cycle=start, frame=0, packetStart=start, packetLevel=level, transitions=[], edges=0, priorHash='', lastChange=0, longestStill=0;
const done = new Error('Recording complete');
const keys={up:'keypad-8',down:'keypad-2',left:'keypad-4',right:'keypad-6'};
function capture(){
 const dots=machine.lcdPanel().dots, rgb=Buffer.alloc(192*64*3), palette=[[175,184,153],[31,44,35]];
 for(let p=0;p<dots.length;p++)rgb.set(palette[dots[p]===2?1:0],p*3);
 writeSync(video,rgb);
 const hash=createHash('sha256').update(rgb).digest('hex');seen.add(hash);
 if(hash!==priorHash){longestStill=Math.max(longestStill,(frame-lastChange)/fps);lastChange=frame;priorHash=hash;changes.push(frame);}
 if(frame%150===0)writeFileSync(resolve(out,`frame-${String(frame/fps).padStart(2,'0')}.png`),png(dots));
 if(frame%30===0)observations.push({time:frame/fps,...snapshot()});
 frame++;
}
function flushAudio(){
 const samples=signal.render({clockHz:HZ,startCycle:packetStart,endCycle:cycle,initialLevel:packetLevel,transitions});
 writeSync(audio,Buffer.from(samples.buffer,samples.byteOffset,samples.byteLength));
 packetStart=cycle;packetLevel=level;transitions=[];
}
const observations=[];
function snapshot(){
 const fields=['phase','stage','moves','player','cursor','grid_stat','hp','enemy_hp','battle','selection_active','rev_active','wall_left','wall_lives','range_round','rogue_floor','dispatch_done','orchard_day','market_day','shipped_a','shipped_b','gravity_distance','G_MODE','G_FLOOR','G_HP','G_X','G_Y'];
 return Object.fromEntries(fields.filter(n=>symbols[n]).map(n=>[n,machine.memory(symbols[n],1)[0]]));
}
const v={id,machine,symbols,root,
 read(n,len=1){assert(n in symbols,`Missing symbol ${id}: ${n}`);const b=machine.memory(symbols[n],len);return len===1?b[0]:[...b];},
 word(n){const b=this.read(n,2);return b[0]*256+b[1];},
 has(n){return n in symbols;},
 data(name){return JSON.parse(readFileSync(resolve(root,'games',id,name+'.json')));},
 get time(){return (cycle-start)/HZ;},
 hold(key,on){key=keys[key]??key;if(held.has(key)===on)return;machine.setKeyboardKeyState(key,on);on?held.add(key):held.delete(key);events.push({time:this.time,key,down:on});},
 release(){for(const key of [...held])this.hold(key,false);},
 wait(duration){
  const target=Math.min(end,cycle+Math.round(duration*HZ));
  while(cycle<target){
  const boundary=Math.min(target,start+Math.round(frame/fps*HZ));
   while(cycle<boundary){
    const stop=machine.run(Math.max(1,Math.min(64,Math.floor((boundary-cycle)/16))));
    assert.equal(stop.reason,'instruction-limit',JSON.stringify(stop));cycle=Number(machine.state().cycleCount);
    for(const a of machine.accesses(filter)){
     if(Number(a.sequence)<=lastSequence)continue;lastSequence=Number(a.sequence);
     assert(a.valueKnown,'Speaker port is known');const next=!!(a.value&16);
     if(next!==level){transitions.push({cycle:Number(a.instructionCycle),level:next});edges++;}level=next;
    }
   }
   if(frame<seconds*fps&&cycle>=start+Math.round(frame/fps*HZ))capture();
   flushAudio();
  }
  if(cycle>=end)throw done;
 },
 tap(key,pause=0.26){this.hold(key,true);this.wait(0.14);this.hold(key,false);this.wait(pause);},
 until(predicate,limit=8){const deadline=this.time+limit;while(predicate()&&this.time<deadline)this.wait(0.05);assert(!predicate(),`${id}: timed out at ${this.time.toFixed(2)}`);},
 aim(cell,width,symbol='cursor'){
  for(let i=0;i<50;i++){
   const p=this.read(symbol);if(p===cell)return;
   this.tap(p%width<cell%width?'right':p%width>cell%width?'left':Math.floor(p/width)<Math.floor(cell/width)?'down':'up',0.19);
  }throw new Error(`${id}: cursor cannot reach ${cell}`);
 },
 menu(choice){this.tap('return',0.45);for(let i=0;i<choice;i++)this.tap('down');this.tap('space',0.5);},
 settle(){
  const busy=['rev_active','motion_active','slide_active','lamp_active','pipe_active','rico_active','actor_intro_active','intro_active','clear_active','box_intro_kind','dot_cpu_active'];
  this.until(()=>busy.some(n=>this.has(n)&&this.read(n))||(this.id==='ice-route'&&this.read('phase')===2&&this.read('ice_mode')!==0),12);
 },
 start(stage=0){this.wait(0.7);this.tap('space',0.55);for(let i=0;i<stage;i++)this.tap('right');this.tap('space',1.3);this.settle();},
 next(){this.release();this.wait(1.2);this.settle();const phase=this.read('phase');for(let n=0;n<4;n++){this.tap('space',1.1);this.settle();if(this.read('phase')!==phase)break;}},
};
try{capture();play(v);v.wait(seconds);}catch(error){if(error!==done)throw error;}
finally{closeSync(video);closeSync(audio);machine.destroy();}
assert.equal(frame,seconds*fps);assert(seen.size>=15,`${id}: too few distinct frames`);
const encoded=spawnSync('ffmpeg',['-v','error','-y','-f','rawvideo','-pixel_format','rgb24','-video_size','192x64','-framerate',String(fps),'-i',videoPath,'-f','f32le','-ar',String(rate),'-ac','1','-i',audioPath,'-vf','scale=768:256:flags=neighbor,pad=800:288:16:16:color=0x1f2c23','-af','highpass=f=40,lowpass=f=10000','-c:v','libx264','-preset','slow','-crf','16','-pix_fmt','yuv420p','-c:a','aac','-b:a','96k','-t',String(seconds),'-movflags','+faststart','-map_metadata','-1',resolve(out,id+'.mp4')],{encoding:'utf8'});
assert.equal(encoded.status,0,encoded.stderr);
unlinkSync(videoPath);unlinkSync(audioPath);
const report={id,duration:seconds,fps,width:800,height:288,clockHz:HZ,playbackSpeed:1,applicationSha256:createHash('sha256').update(app).digest('hex'),bootstrap:'project-authored',frames:frame,distinctFrames:seen.size,speakerEdges:edges,longestStillSeconds:Math.max(longestStill,(frame-lastChange)/fps),events,observations};
writeFileSync(resolve(out,'recording.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({...report,events:events.length,observations:undefined}));
