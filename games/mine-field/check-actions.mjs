// SPDX-License-Identifier: MIT
// Main-screen mode selection, flagging and cancellation through JR-800 keys.
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from '../tools/harness.mjs';
const [wasm,out]=process.argv.slice(2),g=await Game.open(wasm,out,'mine-field');
const stage=JSON.parse(await readFile(new URL('./challenges.json',import.meta.url))).stages[0];
const state=()=>({board:g.read('board',98),flags:g.read('mine_flags'),moves:g.word('challenge_moves'),safe:g.read('grid_stat')});
function aim(p){
 for(const [key,test] of [['right',()=>g.read('cursor')%14<p%14],['left',()=>g.read('cursor')%14>p%14],['down',()=>Math.floor(g.read('cursor')/14)<Math.floor(p/14)],['up',()=>Math.floor(g.read('cursor')/14)>Math.floor(p/14)]]){
  let n=0;while(test()){assert.ok(n++<14);g.tap(key);}
 }
}
function focus(){const p=g.read('cursor');aim(84+p%14);g.tap('down');assert.equal(g.read('mine_control'),1);}
function toggle(){const before=state(),mode=g.read('selection_active');focus();g.tap('space');assert.equal(g.read('selection_active'),mode^1);assert.equal(g.read('mine_control'),0);assert.deepEqual(state(),before,'Changing mode neither opens a cell nor spends a move');}
try{
 await g.start();const initial=state(),start=g.read('cursor');
 for(let col=0;col<14;col++){
  const p=84+col;aim(p);g.tap('down');assert.equal(g.read('mine_control'),1);assert.equal(g.read('cursor'),p);assert.ok(g.read('tile_cache',112).every(v=>!(v&128)),'Mode control has the only focus');g.tap('left');assert.equal(g.read('mine_control'),0);assert.equal(g.read('cursor'),p);
 }
 for(let row=0;row<7;row++){
  const p=row*14+13;aim(p);g.tap('right');assert.equal(g.read('mine_control'),1);g.tap('up');assert.equal(g.read('mine_control'),0);assert.equal(g.read('cursor'),p);
 }
 assert.deepEqual(state(),initial);focus();await g.save('action-mode-focus');g.tap('return');assert.equal(g.read('phase'),3);g.tap('return');assert.equal(g.read('mine_control'),1);g.tap('up');
 toggle();await g.save('action-flag-mode');const mine=stage.initial.mines[0];aim(mine);g.tap('space');assert.equal(g.read('mine_flags'),1);assert.equal(g.word('challenge_moves'),1);assert.equal(g.read('board',98)[mine],initial.board[mine]|64);await g.save('action-flagged');
 toggle();assert.equal(g.read('selection_active'),0);toggle();aim(mine);g.tap('space');assert.equal(g.read('mine_flags'),0);assert.equal(g.word('challenge_moves'),2);assert.deepEqual(g.read('board',98),initial.board);
 g.tap('return');assert.equal(g.read('phase'),2);assert.equal(g.read('selection_active'),0,'RETURN still leaves flag mode');
 focus();g.menu(1);assert.equal(g.read('selection_active'),1);assert.equal(g.read('mine_control'),0,'Menu flag mode returns focus to the board');g.tap('return');
 focus();g.menu(2);assert.deepEqual(state(),initial);assert.equal(g.read('cursor'),start);assert.equal(g.read('mine_control'),0);assert.equal(g.read('selection_active'),0);
 g.frame();assert.equal(g.word('dirty_bytes'),0);
 const result={passed:true,mainFlagMode:true,bottomEntries:14,rightEntries:7,modeSwitchCostsNoMoves:true,flagAndUnflag:true,cancelPreservesCursor:true,menuResume:true,resetClearsMode:true,frames:g.frames,physicalDevice:false};
 await writeFile(resolve(out,'actions-verification.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
}finally{g.destroy();}
