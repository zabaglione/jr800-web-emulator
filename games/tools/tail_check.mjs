// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {letters} from './harness.mjs';
const dirs=['up','down','left','right'],step=[-14,14,-1,1];
const cycle=[];for(let x=0;x<14;x++)cycle.push(x);for(let x=13;x>=1;x--)for(let i=0;i<6;i++)cycle.push((x%2?i+1:6-i)*14+x);for(let y=6;y>=1;y--)cycle.push(y*14);
const successor=new Map(cycle.map((p,i)=>[p,cycle[(i+1)%98]]));
function inspect(g,body,eaten){assert.equal(g.read('tail_length'),body.length);assert.equal(g.read('tail_eaten'),eaten);assert.deepEqual(g.read('tail_body',body.length),body);const board=Array(98).fill(0);for(const p of body)board[p]=1;if(g.read('phase')===2)board[g.read('tail_food')]=2;assert.deepEqual(g.read('board',98),board);assert.equal(g.read('cursor'),body[0]);}
async function advance(g,key,body,eaten,useLetters=false){const head=body[0],food=g.read('tail_food'),d=dirs.indexOf(key),p=head+step[d];g.hold(useLetters?letters[key]:key,true);let moved=false;
 for(let frame=0;frame<80;frame++){g.frame();if(g.read('cursor')!==head||g.read('phase')!==2){moved=true;break;}assert.equal(g.word('dirty_bytes'),0,'Between moves there is no LCD transfer');}
 g.hold(useLetters?letters[key]:key,false);assert.ok(moved);assert.notEqual(g.read('phase'),5,'A Hamiltonian circuit avoids the body');if(p===food)eaten++;else body.pop();body.unshift(p);inspect(g,body,eaten);return eaten;
}
export async function checkTail(g){await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);
 // A short upward pulse cannot reverse the initial downward heading.
 g.tap('up');assert.equal(g.read('tail_queued'),1);g.tap('space');for(let f=0;g.read('phase')===2&&f<500;f++)g.frame();assert.equal(g.read('phase'),5,'Leaving the board loses the game');g.tap('space');
 g.tap('left');assert.equal(g.read('tail_queued'),2);g.tap('space');for(let i=0;g.read('cursor')===49&&i<80;i++)g.frame();assert.equal(g.read('cursor'),48,'A short direction pulse survives until the move tick');
 g.menu(1);assert.equal(g.read('tail_running'),0);const stopped=g.read('tail_body',4);for(let i=0;i<50;i++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(g.read('tail_body',4),stopped);g.menu(2);
 let second=false,third=false;
 for(let stage=0;stage<3;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);let body=g.read('tail_body',4),eaten=0;inspect(g,body,eaten);g.tap('space');
  for(let moves=0;g.read('phase')===2;moves++){
   assert.ok(moves<4000);const next=successor.get(body[0]),key=dirs[step.indexOf(next-body[0])];assert.ok(key);eaten=await advance(g,key,body,eaten,moves%2===0);
   if(stage===1&&!second&&eaten>=10){await g.save('gameplay-2');second=true;}
   if(stage===2&&!third&&eaten>=24){await g.save('gameplay-3');third=true;}
   if(stage===0&&moves===30){g.frame();g.tap('return');assert.equal(g.read('phase'),3);const frozen=g.read('tail_body',body.length);for(let i=0;i<40;i++)g.frame();assert.deepEqual(g.read('tail_body',body.length),frozen);g.tap('return');inspect(g,body,eaten);}
  }assert.equal(g.read('phase'),4);assert.equal(eaten,[10,20,30][stage]);g.frame();
 }assert.ok(second&&third);
}
