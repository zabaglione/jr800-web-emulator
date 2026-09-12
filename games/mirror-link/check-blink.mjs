// SPDX-License-Identifier: MIT
// Observe repeated timer-driven cursor changes, including an 8-bit timer wrap.
import assert from 'node:assert/strict';
import {writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from '../tools/harness.mjs';
const [wasm,out]=process.argv.slice(2),g=await Game.open(wasm,out,'mirror-link');
const pixels=()=>[...g.machine.lcdPanel().dots];
const model=()=>({board:g.read('board',112),beams:g.read('beams',112),lit:g.read('lit_count'),moves:g.word('challenge_moves')});
try{
 await g.start();const initial=model(),cursor=g.read('cursor'),changes=[];let previous=pixels(),mask=g.read('mirror_cursor_mask'),lastTick=g.read('input_ticks');
 await g.save('cursor-visible');
 for(let i=0;i<450&&changes.length<12;i++){
  const tick=g.read('input_ticks'),oldClock=g.read('mirror_blink_clock');g.frame();const next=g.read('mirror_cursor_mask'),screen=pixels();
  assert.deepEqual(model(),initial,'Blinking changes no board, ray, score or move state');
  if(next!==mask){
   assert.ok(((tick-oldClock)&255)>=24,'The input timer gates each blink');
   const changed=screen.flatMap((p,j)=>p===previous[j]?[]:[j]);assert.equal(changed.length,64);
   for(const p of changed){const x=p%192,y=Math.floor(p/192);assert.ok(x>=32+(cursor%16)*8&&x<40+(cursor%16)*8&&y>=8+(cursor>>4)*8&&y<16+(cursor>>4)*8,'Only the selected 8x8 square blinks');}
   assert.ok(g.word('dirty_bytes')<=32,'Only the cursor LCD span is transferred');
   changes.push({mask:next,ticks:(g.read('input_ticks')-lastTick)&255,cycles:Number(g.machine.state().cycleCount)});lastTick=g.read('input_ticks');
   if(changes.length===1)await g.save('cursor-hidden');
  }else{assert.deepEqual(screen,previous);assert.equal(g.word('dirty_bytes'),0,'No transfer between cursor changes');}
  previous=screen;mask=next;
 }
 assert.equal(changes.length,12);assert.ok(changes.slice(1).every(c=>c.ticks===changes[1].ticks),'Blink interval remains constant across timer wrap');
 while(g.read('mirror_cursor_mask'))g.frame();g.tap('right');assert.equal(g.read('cursor'),cursor+1);assert.equal(g.read('mirror_cursor_mask'),128,'Movement immediately shows the new cursor');
 while(g.read('mirror_cursor_mask'))g.frame();g.tap('return');assert.equal(g.read('phase'),3);const menu=pixels();for(let i=0;i<70;i++){g.frame();assert.deepEqual(pixels(),menu);assert.equal(g.word('dirty_bytes'),0);}
 g.tap('return');assert.equal(g.read('phase'),2);assert.equal(g.read('mirror_cursor_mask'),128,'Menu return immediately restores visible focus');
 const result={passed:true,blinkTransitions:changes.length,intervalInputTicks:changes[1].ticks,onlyCursorChanges:true,unchangedBoardAndBeams:true,immediateMovementFocus:true,pausedMenuStable:true,timerWrap:true,frames:g.frames,physicalDevice:false};
 await writeFile(resolve(out,'blink-verification.json'),JSON.stringify(result,null,2)+'\n');await writeFile(resolve(out,'blink-timing.json'),JSON.stringify(changes,null,2)+'\n');console.log(JSON.stringify(result));
}finally{g.destroy();}
