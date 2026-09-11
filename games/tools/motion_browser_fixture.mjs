// SPDX-License-Identifier: MIT
// Build deterministic LCD expectations with the emulator test driver, never the browser host.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {Game} from './harness.mjs';
import {checkBoard} from './board_check.mjs';
export async function motionFixture(wasm,out,id){
 let keys=[];const stage=id==='line-four'?1:0;
 if(id==='line-four'){
  const g=await Game.open(wasm,out,id),tap=g.tap.bind(g);
  g.tap=key=>{if(g.read('phase')===2&&g.read('stage')===1)keys.push(key);tap(key);};
  try{await checkBoard(g,id);}finally{g.destroy();}
 }
 const g=await Game.open(wasm,out,id),steps=[];
 const flag={'line-four':'four_active','pipe-weave':'pipe_running','number-rail':'rail_active','step-strike':'step_firing'}[id];
 const pixels=()=>[...g.machine.memory(g.symbols.framebuffer,1536)];
 const settle=()=>{for(let i=0;g.read(flag)&&g.read('phase')===2&&i<300;i++)g.frame();assert.equal(g.read(flag),0);};
 try{
  await g.start(stage);settle();const initial=pixels();
  const tap=key=>{
   const frames=[],frame=g.frame.bind(g);g.frame=()=>{const p=frame();frames.push(pixels());return p;};
   try{g.tap(key);settle();}finally{g.frame=frame;}
   const unique=[...new Map(frames.map(p=>[p.join(','),p])).values()];
   steps.push({key,end:pixels(),intermediate:unique.slice(0,-1)});
  };
  if(id==='line-four')for(const key of keys)tap(key);
  else{
   const challenge=JSON.parse(await readFile(new URL(`../${id}/challenges.json`,import.meta.url))).stages[0];
   for(const action of challenge.normal){
    if(id==='pipe-weave'){
     while(g.read('cursor')%6<action%6)tap('right');while(g.read('cursor')%6>action%6)tap('left');
     while(Math.floor(g.read('cursor')/6)<Math.floor(action/6))tap('down');while(Math.floor(g.read('cursor')/6)>Math.floor(action/6))tap('up');
     tap('space');
    }else tap(action==='fire'?'space':action);
   }
  }
  assert.equal(g.read('phase'),4);assert.equal(g.read('clear_active'),1);
  return {id,stage,initial,steps};
 }finally{g.destroy();}
}
