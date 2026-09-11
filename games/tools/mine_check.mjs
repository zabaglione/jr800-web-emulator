// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
const adjacent=p=>{const a=[];for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){const x=p%14+dx,y=Math.floor(p/14)+dy;if((dx||dy)&&x>=0&&x<14&&y>=0&&y<7)a.push(y*14+x);}return a;};
function aim(g,p){while(g.read('cursor')%14<p%14)g.tap('right');while(g.read('cursor')%14>p%14)g.tap('left');while(Math.floor(g.read('cursor')/14)<Math.floor(p/14))g.tap('down');while(Math.floor(g.read('cursor')/14)>Math.floor(p/14))g.tap('up');}
function field(seed,p,total){const b=Array(98).fill(0),safe=new Set([p,...adjacent(p)]);let count=0;while(count<total){seed=(seed>>>1)^((seed&1)?0xb8:0);const q=seed&127;if(q<98&&!safe.has(q)&&!b[q]){b[q]=16;count++;}}for(let q=0;q<98;q++)if(!b[q])b[q]=adjacent(q).filter(n=>b[n]&16).length;return {b,seed};}
function reveal(state,p){
 const b=state.b;if(b[p]&96)return;
 if(b[p]&16){state.phase=5;state.blast=p;return;}
 b[p]|=32;const todo=[p];for(let i=0;i<todo.length;i++)if(!(b[todo[i]]&15))for(const q of adjacent(todo[i]))if(!(b[q]&96)){assert.equal(b[q]&16,0);b[q]|=32;todo.push(q);}
}
function finish(state){if(state.phase!==5&&state.b.every(n=>(n&16)||(n&32))){state.phase=4;state.b=state.b.map(n=>n&16?n|64:n);}}
function inspect(g,state){assert.deepEqual(g.read('board',98),state.b);assert.equal(g.read('phase'),state.phase);assert.equal(g.read('mine_flags'),state.b.filter(n=>n&64).length);assert.equal(g.read('grid_stat'),state.b.filter(n=>!(n&16)&&!(n&32)).length);assert.equal(g.read('seed'),state.seed);if(state.phase===5)assert.equal(g.read('mine_blast'),state.blast);}
export async function checkMine(g){
 await g.start();let wrongChord=false;
 for(let stage=0;stage<3;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);const total=[10,15,20][stage],start=[0,49,97][stage];
  assert.equal(g.read('mine_armed'),0);assert.equal(g.read('grid_stat'),98-total);g.frame();assert.equal(g.word('dirty_bytes'),0);
  if(!stage){g.menu(1);g.tap('space');assert.equal(g.read('mine_flags'),1);g.tap('return');g.tap('space');assert.equal(g.read('mine_armed'),0,'Flagged first cell is not opened');g.menu(1);g.tap('space');g.tap('return');}
  aim(g,start);let state={...field(g.read('seed'),start,total),phase:2};reveal(state,start);finish(state);g.tap('space');inspect(g,state);
  assert.ok([start,...adjacent(start)].every(p=>!(state.b[p]&16)),'First cell and neighbours are safe');
  if(!stage){
   await g.save('gameplay-1');const target=state.b.findIndex((n,p)=>(n&32)&&(n&15)>0&&adjacent(p).some(q=>!(state.b[q]&48)));
   assert.ok(target>=0,'A visible number has a hidden safe neighbour');
   const around=adjacent(target),needed=state.b[target]&15,bad=around.find(q=>!(state.b[q]&48));
   const flags=[bad,...around.filter(q=>state.b[q]&16).slice(0,needed-1)];assert.equal(flags.length,needed);
   g.menu(1);for(const p of flags){aim(g,p);g.tap('space');state.b[p]|=64;}g.tap('return');aim(g,target);
   for(const q of around){reveal(state,q);if(state.phase===5)break;}g.tap('space');inspect(g,state);assert.equal(state.phase,5,'Incorrect matching flags can expose a mine');await g.save('gameplay-3');wrongChord=true;
   g.tap('space');assert.equal(g.read('mine_armed'),0);assert.deepEqual(g.read('board',98),Array(98).fill(0));aim(g,start);state={...field(g.read('seed'),start,total),phase:2};reveal(state,start);finish(state);g.tap('space');inspect(g,state);
  }
  // Deterministic state knowledge selects safe test inputs; it is not a logical-solvability claim.
  g.menu(1);for(let p=0;p<98;p++)if(state.b[p]&16){aim(g,p);g.tap('space');state.b[p]|=64;}g.tap('return');inspect(g,state);
  if(stage===1)await g.save('gameplay-2');
  for(let p=0;p<98&&state.phase===2;p++)if(!(state.b[p]&48)){
   const number=adjacent(p).find(q=>(state.b[q]&32)&&((state.b[q]&15)===adjacent(q).filter(n=>state.b[n]&64).length));
   if(number!==undefined){aim(g,number);for(const q of adjacent(number))reveal(state,q);}else{aim(g,p);reveal(state,p);}
   finish(state);g.tap('space');inspect(g,state);
  }
  assert.equal(g.read('phase'),4);
 }
 assert.ok(wrongChord);
}
