// SPDX-License-Identifier: MIT
// Exercise main-screen DRAW and menu UNDO through real keys, without RAM edits.
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from '../tools/harness.mjs';
const [wasm,out]=process.argv.slice(2),levels=JSON.parse(await readFile(new URL('./solutions.json',import.meta.url)));
const state=g=>({board:g.read('board',35),waste:g.read('suit_waste'),pos:g.read('suit_stock_pos'),chain:g.read('suit_chain'),score:g.word('suit_score'),moves:g.read('moves'),left:g.read('grid_stat')});
function aim(g,p){
 if(g.read('suit_action'))g.tap('up');
 for(const [key,test] of [['right',()=>g.read('cursor')%7<p%7],['left',()=>g.read('cursor')%7>p%7],['down',()=>Math.floor(g.read('cursor')/7)<Math.floor(p/7)],['up',()=>Math.floor(g.read('cursor')/7)>Math.floor(p/7)]]){
  let n=0;while(test()){assert.ok(n++<7,'Card navigation stays on the board');g.tap(key);}
 }
}
function draw(g){
 if(!g.read('suit_action')){g.tap('down');assert.equal(g.read('suit_action'),1);}
 const before=state(g),deal=levels.deals[g.read('stage')];g.tap('space');
 assert.deepEqual(state(g),{...before,waste:deal[36+before.pos],pos:before.pos+1,chain:0,moves:before.moves+1});
}
let frames=0;const g=await Game.open(wasm,out,'suit-run');
try{
 await g.start();const initial=state(g),cursor=g.read('cursor');g.tap('down');assert.equal(g.read('suit_action'),1);assert.ok(g.read('tile_cache',112).every(v=>!(v&128)),'Only the DRAW control has focus');await g.save('action-draw');
 for(const key of ['left','right','down']){g.tap(key);assert.equal(g.read('cursor'),cursor);assert.deepEqual(state(g),initial);}
 draw(g);g.tap('return');assert.equal(g.read('phase'),3);await g.save('action-undo-menu');g.tap('return');assert.equal(g.read('suit_action'),1);
 g.menu(2);assert.deepEqual(state(g),initial);assert.equal(g.read('suit_action'),0);g.menu(2);assert.deepEqual(state(g),initial);
 g.tap('up');assert.equal(g.read('cursor'),cursor-7);g.tap('down');assert.equal(g.read('cursor'),cursor);assert.equal(g.read('suit_action'),0,'Down onto a remaining card keeps card focus');
 let chosen=-1;
 for(let n=0;n<16;n++){
  const s=state(g);chosen=s.board.findIndex((v,p)=>v&&(p>=28||!s.board[p+7])&&[1,12].includes(Math.abs(v-s.waste)));
  if(chosen>=0)break;draw(g);if(g.read('suit_action'))g.tap('up');
 }
 assert.ok(chosen>=0);aim(g,chosen);const before=state(g);g.tap('space');assert.equal(g.read('suit_chain'),before.chain+1);assert.equal(g.word('suit_score'),before.score+before.chain+1);
 const chain=state(g);g.tap('down');assert.equal(g.read('suit_action'),1,'Down from a removed card reaches DRAW');draw(g);g.menu(2);assert.deepEqual(state(g),chain,'UNDO restores chain and score after a main-screen DRAW');
 g.tap('down');g.tap('up');assert.equal(g.read('cursor'),chosen-7,'Up returns to the newly exposed card');
 for(let column=0;column<7;column++){
  const board=g.read('board',35);let p=28+column;while(p>=7&&!board[p])p-=7;aim(g,p);
  const s=state(g);g.tap('down');assert.equal(g.read('suit_action'),1);g.tap('up');assert.equal(g.read('cursor'),p);assert.deepEqual(state(g),s);
 }
 g.frame();assert.equal(g.word('dirty_bytes'),0);frames+=g.frames;
}finally{g.destroy();}
// This authored deal still has legal board cards after its sixteenth draw.
const e=await Game.open(wasm,out,'suit-run');
try{
 await e.start(2);for(let i=0;i<16;i++)draw(e);assert.equal(e.read('phase'),2);assert.equal(e.read('suit_stock_pos'),16);await e.save('action-empty');
 const before=state(e);e.tap('space');assert.deepEqual(state(e),before,'An empty DRAW is inert');e.menu(2);assert.equal(e.read('suit_stock_pos'),15);assert.equal(e.read('suit_action'),0);frames+=e.frames;
}finally{e.destroy();}
const result={passed:true,mainDraw:true,undoInMenu:true,undoRestoresChainAndScore:true,columns:7,menuResume:true,emptyDrawInert:true,frames,physicalDevice:false};
await writeFile(resolve(out,'actions-verification.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
