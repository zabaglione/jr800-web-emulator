// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const dirs=[[0,-1,1,2],[0,1,2,1],[-1,0,4,8],[1,0,8,4]];
const rotate=m=>((m&1)<<3)|((m&8)>>2)|((m&2)<<1)|((m&4)>>2);
export function pipeModel(b){
 let leaks=0;const links=[];
 for(let p=0;p<36;p++)for(const [dx,dy,bit,op] of dirs){
  const x=p%6+dx,y=Math.floor(p/6)+dy,q=y*6+x,connected=!!(b[p]&bit)&&x>=0&&x<6&&y>=0&&y<6&&!!(b[q]&op);
  links.push(connected?q:255);if((b[p]&bit)&&!connected)leaks++;
 }
 const wet=Array(36).fill(0);wet[0]=1;const todo=[0];for(let i=0;i<todo.length;i++)for(const q of links.slice(todo[i]*4,todo[i]*4+4))if(q!==255&&!wet[q]){wet[q]=1;todo.push(q);}
 return {wet,leaks,links,dry:36-todo.length};
}
function aim(g,p){while(g.read('cursor')%6<p%6)g.tap('right');while(g.read('cursor')%6>p%6)g.tap('left');while(Math.floor(g.read('cursor')/6)<Math.floor(p/6))g.tap('down');while(Math.floor(g.read('cursor')/6)>Math.floor(p/6))g.tap('up');}
function verify(g,b){const model=pipeModel(b);assert.deepEqual(g.read('board',36),b);assert.deepEqual(g.read('pipe_wet',36),model.wet);assert.deepEqual(g.read('pipe_connections',144),model.links);assert.equal(g.read('pipe_leaks'),model.leaks);assert.equal(g.read('grid_stat'),model.dry);assert.equal(g.read('phase'),!model.leaks&&!model.dry?4:2);}
export async function checkPipe(g){
 const levels=JSON.parse(await readFile(new URL('../pipe-weave/solutions.json',import.meta.url)));await g.start();
 for(let stage=0;stage<20;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);let b=[...levels.boards[stage]];verify(g,b);
  if(!stage){
   await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);g.tap('space');g.menu(1);verify(g,b);assert.equal(g.read('moves'),0);
   for(let i=0;i<260;i++)g.tap('space');assert.equal(g.read('moves'),255);g.menu(1);assert.equal(g.read('moves'),255);g.menu(2);verify(g,b);
  }
  for(let p=0;p<36&&g.read('phase')===2;p++){
   if(b[p]!==levels.solved[stage][p])aim(g,p);
   while(b[p]!==levels.solved[stage][p]&&g.read('phase')===2){b[p]=rotate(b[p]);g.tap('space');verify(g,b);}
   if(stage===7&&p===17)await g.save('gameplay-2');if(stage===19&&p===28)await g.save('gameplay-3');
  }
  assert.equal(g.read('phase'),4);assert.equal(g.read('pipe_leaks'),0);assert.equal(g.read('grid_stat'),0);
 }
}
