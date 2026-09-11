// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
function aim(g,p){
 const b=g.read('board',49),start=g.read('cursor'),todo=[[start,[]]],seen=new Set([start]);
 for(let i=0;i<todo.length;i++){
  const [q,path]=todo[i];if(q===p){for(const key of path)g.tap(key);return;}
  for(const [dx,dy,key] of [[0,-1,'up'],[0,1,'down'],[-1,0,'left'],[1,0,'right']]){
   const x=q%7+dx,y=Math.floor(q/7)+dy,n=y*7+x;
   if(x>=0&&x<7&&y>=0&&y<7&&b[n]!==255&&!seen.has(n)){seen.add(n);todo.push([n,[...path,key]]);}
  }
 }throw new Error('Inaccessible board hole');
}
function valid(b,p,q){return b[p]===1&&b[q]===0&&((p%7===q%7&&Math.abs(p-q)===14)||(Math.floor(p/7)===Math.floor(q/7)&&Math.abs(p-q)===2))&&b[(p+q)/2]===1;}
export async function checkPeg(g){
 const levels=JSON.parse(await readFile(new URL('../peg-rescue/solutions.json',import.meta.url)));await g.start();
 for(let stage=0;stage<20;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('board',49),levels.boards[stage]);
  if(!stage){
   await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);g.tap('space');assert.equal(g.read('selection_active'),1);g.tap('return');assert.equal(g.read('phase'),2);assert.equal(g.read('selection_active'),0);
  }
  for(let i=0;i<levels.moves[stage].length;i++){
   const [p,q]=levels.moves[stage][i],before=g.read('board',49);assert.ok(valid(before,p,q));aim(g,p);g.tap('space');
   assert.equal(g.read('selection_active'),1);assert.equal(g.read('peg_source'),p);
   assert.deepEqual(g.read('peg_middle',49),before.map((_,t)=>valid(before,p,t)?(p+t)/2:255));
   if(stage===9&&i===3)await g.save('gameplay-2');aim(g,q);g.tap('space');
   const after=[...before];after[p]=0;after[(p+q)/2]=0;after[q]=1;assert.deepEqual(g.read('board',49),after);assert.equal(g.read('grid_stat'),after.filter(v=>v===1).length);
   if(!stage&&!i){g.menu(1);assert.deepEqual(g.read('board',49),before);assert.equal(g.read('moves'),0);aim(g,p);g.tap('space');aim(g,q);g.tap('space');}
   if(stage===19&&i===17)await g.save('gameplay-3');
  }
  assert.equal(g.read('phase'),4);assert.equal(g.read('grid_stat'),1);
 }
}
