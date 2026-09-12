// SPDX-License-Identifier: MIT
// Observe each displayed object and its real sound-port writes during startup.
import assert from 'node:assert/strict';
import {mkdir,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game,png} from '../tools/harness.mjs';
const [wasm,out]=process.argv.slice(2),stages=JSON.parse(await readFile(new URL('./challenges.json',import.meta.url))).stages;
let frames=0,totalCues=0,totalEdges=0;const sequences=[];
for(const index of [0,10,30,39]){
 const g=await Game.open(wasm,out,'box-shift'),stage=stages[index],m=g.machine,run=m.runTo.bind(m);
 const stops=[g.symbols.box_intro_frame,g.symbols.box_intro_player_frame],seen=[],edges=[];
 for(const s of stops)m.setExecutionBreakpoint(s,true);m.setMemoryWatchpoint(2,'write',true);
 let active=false;
 m.runTo=(address,limit)=>{
  for(let i=0;i<20000;i++){
   const stop=run(address,limit);
   if(stop.reason==='memory-watchpoint'){
    if(active){const e=m.accesses({firstAddress:2,lastAddress:2}).at(-1);edges.push({cycle:Number(e.instructionCycle),value:e.value&16});}m.step();continue;
   }
   if(stop.reason!=='execution-breakpoint')return stop;
   assert.ok(stops.includes(m.state().pc));active=true;
   const kind=g.read('box_intro_kind'),step=g.read('box_intro_step'),cell=kind===3?g.read('player'):g.read('box_intro_index'),pixels=[...m.lcdPanel().dots];
   assert.deepEqual(g.read('board',112),stage.initial.board,'Introduction never mutates puzzle state');assert.equal(g.read('player'),stage.initial.start);assert.equal(g.word('moves'),0);assert.equal(g.word('challenge_moves'),0);assert.equal(g.read('undo_valid'),0);
   const previous=seen.at(-1);
   if(previous){const changed=pixels.flatMap((p,i)=>p===previous.pixels[i]?[]:[i]);assert.ok(changed.length>0,'Every cue changes its indicated object');for(const p of changed){const x=p%192-g.read('view_origin'),y=Math.floor(p/192)-8;assert.equal(Math.floor(y/8)*16+Math.floor(x/8),cell,'Only the next indicated object changes');}}
   seen.push({kind,step,cell,pixels,cycle:Number(m.state().cycleCount),edges:edges.length});m.step();
  }throw new Error('Intro exceeded instrumentation bound');
 };
 try{
  await g.start(index);active=false;
  const crates=stage.initial.board.flatMap((v,i)=>v&4?[i]:[]),goals=stage.initial.board.flatMap((v,i)=>v&2?[i]:[]);
  const expected=[...crates.map(cell=>[1,0,cell]),...goals.map(cell=>[2,0,cell]),...[0,1,2,3,4].map(step=>[3,step,stage.initial.start])];
  assert.deepEqual(seen.map(({kind,step,cell})=>[kind,step,cell]),expected,'All crates precede all goals, then the player blinks twice and stays visible');
  assert.equal(edges.length,(crates.length+goals.length)*32+72,'Each object has one cue; player has its own two-note cue');
  for(let i=1;i<seen.length;i++)assert.ok(seen[i].cycle-seen[i-1].cycle>=140000,'Each cue remains visible');
  const intervals=seen.filter(f=>f.kind<3).map(f=>edges[f.edges+1].cycle-edges[f.edges].cycle);
  const player=seen.find(f=>f.kind===3),playerInterval=edges[player.edges+1].cycle-edges[player.edges].cycle;
  assert.ok(intervals.every(v=>v>playerInterval),'Player sound has a distinct higher pitch');
  assert.equal(g.read('box_intro_kind'),0);const panel=[...m.lcdPanel().dots];g.frame();assert.equal(g.word('dirty_bytes'),0);
  g.tap('return');g.tap('return');assert.deepEqual([...m.lcdPanel().dots],panel);assert.equal(seen.length,expected.length,'Menu return does not replay the intro');
  const dir=resolve(out,'intro',String(index+1));await mkdir(dir,{recursive:true});if(index===0)for(const [i,f] of seen.entries())await writeFile(resolve(dir,String(i).padStart(2,'0')+'.png'),png(f.pixels));
  sequences.push({stage:index+1,crates:crates.length,goals:goals.length,frames:seen.map(({pixels,...f})=>f)});totalCues+=crates.length+goals.length+1;totalEdges+=edges.length;frames+=g.frames;
 }finally{g.destroy();}
}
const result={passed:true,stages:4,totalCues,totalEdges,sequentialReveal:true,distinctPlayerSound:true,playerBlinks:2,logicalStateUnchanged:true,frames,physicalDevice:false};
await writeFile(resolve(out,'intro-verification.json'),JSON.stringify(result,null,2)+'\n');await writeFile(resolve(out,'intro-timing.json'),JSON.stringify(sequences,null,2)+'\n');console.log(JSON.stringify(result));
