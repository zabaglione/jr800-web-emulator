// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
function options(b,p){return b.map((v,q)=>!v&&[12,21].includes(Math.abs(p%6-q%6)*10+Math.abs(Math.floor(p/6)-Math.floor(q/6)))?q:-1).filter(q=>q>=0);}
function aim(g,p){while(g.read('cursor')%6<p%6)g.tap('right');while(g.read('cursor')%6>p%6)g.tap('left');while(Math.floor(g.read('cursor')/6)<Math.floor(p/6))g.tap('down');while(Math.floor(g.read('cursor')/6)>Math.floor(p/6))g.tap('up');}
export async function checkTour(g){const solutions=JSON.parse(await readFile(new URL('../knight-tour/solutions.json',import.meta.url)));await g.start();
 for(let stage=0;stage<6;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);
  if(!stage){
   await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);g.tap('space');assert.equal(g.read('moves'),0,'Visited square cannot be entered');
   for(let i=0;i<35&&!g.read('knight_stuck');i++){
    const p=g.read('knight_player'),board=g.read('board',36);const choices=options(board,p).sort((a,b)=>options(board,b).length-options(board,a).length||a-b);aim(g,choices[0]);g.tap('space');
   }
   assert.equal(g.read('knight_stuck'),1,'A dead end remains recoverable');assert.equal(g.read('phase'),2);const moves=g.read('moves');g.menu(1);assert.equal(g.read('moves'),moves-1);assert.equal(g.read('knight_stuck'),0);g.menu(2);
  }
  let b=Array(36).fill(0);b[solutions[stage][0]]=1;
  for(let i=1;i<36;i++){
   const p=g.read('knight_player'),legal=options(b,p);assert.deepEqual(g.read('knight_legal',36),b.map((_,q)=>Number(legal.includes(q))));
   const q=solutions[stage][i];assert.ok(legal.includes(q));aim(g,q);g.tap('space');b[q]=i+1;assert.deepEqual(g.read('board',36),b);assert.equal(g.read('grid_stat'),35-i);
   if(stage===2&&i===14)await g.save('gameplay-2');if(stage===5&&i===28)await g.save('gameplay-3');
  }
  assert.equal(g.read('phase'),4);assert.equal(new Set(b).size,36);
 }
}
