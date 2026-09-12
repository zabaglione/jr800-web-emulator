// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game,png} from '../tools/harness.mjs';
const [wasm,out]=process.argv.slice(2),g=await Game.open(wasm,out,'push-luck');
const m=g.machine,run=m.runTo.bind(m),events=[],captures=[];let soundEdges=0;
for(const name of ['luck_effect_frame','luck_banner_frame'])m.setExecutionBreakpoint(g.symbols[name],true);
m.setMemoryWatchpoint(2,'write',true);
m.runTo=(address,limit)=>{
 for(let i=0;i<20000;i++){
  const stop=run(address,limit);
  if(stop.reason==='memory-watchpoint'){soundEdges++;m.step();continue;}
  if(stop.reason!=='execution-breakpoint')return stop;
  const banner=m.state().pc===g.symbols.luck_banner_frame;
  const event={kind:banner?'banner':'roll',cycle:Number(m.state().cycleCount),side:g.read('luck_side'),face:g.read('luck_face'),shown:g.read('luck_show_face'),pot:g.read('luck_pot'),rolls:g.read('luck_rolls'),seed:g.read('seed')};
  events.push(event);
  if(banner){assert.equal(event.side,1);assert.equal(event.rolls,0,'CPU announcement precedes all CPU rolls');assert.deepEqual(g.read('luck_trail',6),[0,0,0,0,0,0]);}
  if(captures.length<40)captures.push({event,image:png(m.lcdPanel().dots)});
  m.step();
 }
 throw new Error('Animation observation exceeded bound');
};
try{
 await g.start();const seed=g.read('seed');g.tap('space');let throws=0;while(!g.read('luck_side')){assert.ok(throws++<16);g.tap('space');}
 assert.deepEqual(events.slice(0,6).map(e=>e.shown),[1,2,3,4,5,6]);assert.ok(events.slice(0,6).every(e=>e.seed===seed&&e.pot===0&&e.rolls===0),'Rolling artwork changes no game state or RNG');
 const firstBanner=events.find(e=>e.kind==='banner');assert.ok(firstBanner);assert.ok(Number(m.state().cycleCount)-firstBanner.cycle>800000,'CPU TURN remains visible for over half a second');
 const bannerCount=()=>events.filter(e=>e.kind==='banner').length;
 const finishCpu=()=>{let i=0;while(g.read('luck_side')&&g.read('phase')===2){assert.ok(i++<500);g.frame();}};
 finishCpu();let attempts=0;while(!g.read('luck_pot')){assert.ok(attempts++<12);const before=g.read('seed');g.tap('space');assert.notEqual(g.read('seed'),before,'First player input after CPU completion responds');finishCpu();}
 const banked=g.read('luck_pot'),score=g.read('luck_scores',2)[0],count=bannerCount();g.tap('right');assert.equal(g.read('luck_action'),1);await g.save('action-bank');g.tap('space');
 assert.equal(g.read('luck_scores',2)[0],score+banked);assert.equal(g.read('luck_pot'),0);assert.equal(g.read('luck_side'),1);assert.equal(bannerCount(),count+1,'Main-screen BANK announces CPU TURN');
 assert.ok(soundEdges>200);await g.save('after-cpu-banner');
 const directory=resolve(out,'effects');await mkdir(directory,{recursive:true});for(const [i,c] of captures.entries())await writeFile(resolve(directory,String(i).padStart(2,'0')+'.png'),c.image);
 const result={passed:true,mainBank:true,bustBanner:true,bankBanner:true,rngUnchangedDuringAnimation:true,firstHumanInput:true,soundEdges,events};
 await writeFile(resolve(out,'effects-verification.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({...result,events:events.length}));
}finally{g.destroy();}
