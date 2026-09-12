// SPDX-License-Identifier: MIT
// Game-owned Makefiles select this reusable LCD/input/sound contract.
import assert from 'node:assert/strict';
import {writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game,png} from './harness.mjs';

export async function checkFocus(g){
 await g.start();
 const maskName=g.symbols.cursor_blink_mask?'cursor_blink_mask':'pipe_cursor_mask';
 const names=['cursor','selected','market_good','market_destination','penalty_target','penalty_height','putt_aim','putt_power'].filter(n=>g.symbols[n]);
 const snapshot=()=>Object.fromEntries(names.map(n=>[n,g.read(n)])),before=snapshot();
 let mask=g.read(maskName),pixels=[...g.machine.lcdPanel().dots],toggles=0,first,second;
 const start=Number(g.machine.state().cycleCount);
 for(let f=0;toggles<3&&f<2000;f++){
  g.frame();assert.equal(g.read('phase'),2);assert.deepEqual(snapshot(),before,'Blinking never moves or replaces the real selection');
  const next=g.read(maskName);
  if(next!==mask){
   assert.equal(next,mask^128);const panel=[...g.machine.lcdPanel().dots],changed=panel.reduce((n,v,i)=>n+(v!==pixels[i]),0);
   assert.ok(changed>0,'Each focus phase actually reaches the LCD');
   if(!first){first=panel;await writeFile(resolve(g.out,'focus-1.png'),png(panel));}
   else if(!second){second=panel;await writeFile(resolve(g.out,'focus-2.png'),png(panel));}
   mask=next;pixels=panel;toggles++;
  }
 }
 assert.equal(toggles,3,'Focus keeps blinking through several timer wraps');
 assert.ok(Number(g.machine.state().cycleCount)-start>=1_000_000,'Blink speed remains readable');
 return {focusToggles:toggles,selectionPreserved:true};
}

export async function checkIntro(g){
 const m=g.machine,run=m.runTo.bind(m),point=g.symbols.actor_intro_frame;
 assert.ok(point);m.setExecutionBreakpoint(point,true);m.setMemoryWatchpoint(2,'write',true);
 const seen=[];let edges=0,active=false;
 m.runTo=(address,limit)=>{
  for(let i=0;i<10000;i++){
   const stop=run(address,limit);
   if(stop.reason==='memory-watchpoint'){if(active)edges++;m.step();continue;}
   if(stop.reason!=='execution-breakpoint')return stop;
   assert.equal(m.state().pc,point);active=true;
   assert.equal(g.read('phase'),2,'The introduction presents the live stage');
   seen.push({step:g.read('actor_intro_step'),cycle:Number(m.state().cycleCount),pixels:[...m.lcdPanel().dots]});m.step();
  }throw new Error('Introduction exceeded the observation bound');
 };
 await g.start();active=false;m.setExecutionBreakpoint(point,false);m.setMemoryWatchpoint(2,'write',false);m.runTo=run;
 assert.deepEqual(seen.map(s=>s.step),[0,1,2,3],'Two complete blinks finish with the actor visible');assert.ok(edges>=32,'Startup emits a real speaker-port cue');
 for(let i=1;i<seen.length;i++){
  assert.ok(seen[i].cycle-seen[i-1].cycle>=60000,'Each phase lasts long enough to locate the actor');
  const changed=seen[i].pixels.flatMap((v,p)=>v!==seen[i-1].pixels[p]?[p]:[]);assert.ok(changed.length>0);
  const xs=changed.map(p=>p%192),ys=changed.map(p=>Math.floor(p/192));
  assert.ok(Math.max(...xs)-Math.min(...xs)<32&&Math.max(...ys)-Math.min(...ys)<32,'Blink stays around the player');
 }
 await writeFile(resolve(g.out,'start-blink-1.png'),png(seen[0].pixels));await writeFile(resolve(g.out,'start-blink-2.png'),png(seen[1].pixels));
 return {startBlinks:2,speakerWrites:edges,automaticCue:true};
}

if(process.argv[1]&&import.meta.url===new URL('file://'+resolve(process.argv[1])).href){
 const [wasm,out,id,kind]=process.argv.slice(2),g=await Game.open(wasm,out,id);
 try{const result={id,passed:true,...await (kind==='focus'?checkFocus(g):checkIntro(g)),physicalDevice:false};await writeFile(resolve(out,'presentation-verification.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));}finally{g.destroy();}
}
