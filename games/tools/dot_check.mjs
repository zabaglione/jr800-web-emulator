// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
const edges=[];
for(let y=0;y<7;y++)for(let x=0;x<9;x++)if((x+y)%2)edges.push([x,y]);
const index=(x,y)=>edges.findIndex(([X,Y])=>X===x&&Y===y);
const boxes=[];for(let y=1;y<7;y+=2)for(let x=1;x<9;x+=2)boxes.push([index(x,y-1),index(x,y+1),index(x-1,y),index(x+1,y)]);
const neighbours=edges.map(([x,y],i)=>[[0,-1],[0,1],[-1,0],[1,0]].map(([dx,dy])=>{
 const candidates=edges.map(([X,Y],j)=>[2*((X-x)*dx+(Y-y)*dy)+3*Math.abs((X-x)*dy-(Y-y)*dx),j,(X-x)*dx+(Y-y)*dy]).filter(([,j,forward])=>j!==i&&forward>0).sort((a,b)=>a[0]-b[0]||a[1]-b[1]);return candidates.length?candidates[0][1]:i;
}));
function aim(g,p){const todo=[[g.read('dot_cursor'),[]]],seen=new Set([todo[0][0]]);for(let i=0;i<todo.length;i++){const [q,path]=todo[i];if(q===p){for(const k of path)g.tap(k);return;}for(let d=0;d<4;d++){const n=neighbours[q][d];if(!seen.has(n)){seen.add(n);todo.push([n,[...path,['up','down','left','right'][d]]]);}}}throw new Error('Edge cursor disconnected');}
function draw(b,edge,side){b[edge]=side;let gained=0;for(let i=0;i<12;i++)if(!b[31+i]&&boxes[i].every(e=>b[e])){b[31+i]=side;gained++;}return gained;}
function choice(b,level){
 let best=-1,score=0;
 for(let e=0;e<31;e++)if(!b[e]){
  if(!level)return e;
  const counts=boxes.filter(box=>box.includes(e)).map(box=>box.filter(q=>b[q]||q===e).length);
  const s=32+64*counts.filter(n=>n===4).length-(level===2?12*counts.filter(n=>n===3).length:0);
  if(s>score){score=s;best=e;}
 }return best;
}
function play(b,e,level){const next=[...b],gain=draw(next,e,1);let cpu=0;if(!gain){let move=choice(next,level);while(move>=0){cpu++;if(!draw(next,move,2))break;move=choice(next,level);}}return {next,gain,cpu};}
export async function checkDot(g){
 await g.start();let wins=0,losses=0,extra=0,chain=0;
 for(let level=0;level<3;level++){
  if(level){g.tap('return');assert.equal(g.read('phase'),1);g.tap('right');g.tap('space');}
  assert.equal(g.read('stage'),level);let b=Array(43).fill(0);
  assert.deepEqual(g.read('board',43),b);
  if(!level){await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);}
  for(let turn=0;turn<31&&g.read('phase')===2;turn++){
   const e=choice(b,level===0?0:2),{next,gain,cpu}=play(b,e,level);aim(g,e);g.tap('space');
   assert.deepEqual(g.read('board',43),next,'Every closed box belongs to the player who drew its final edge');
   const own=next.slice(31).filter(n=>n===1).length,opponent=next.slice(31).filter(n=>n===2).length;
   assert.equal(g.read('dot_player_score'),own);assert.equal(g.read('dot_cpu_score'),opponent);assert.equal(g.read('grid_stat'),12-own-opponent);
   assert.equal(g.read('phase'),own+opponent<12?2:own>=opponent?4:5);
   if(gain){extra++;assert.equal(cpu,0,'Closing a box grants another human turn');}if(cpu>1)chain++;
   if(!level&&!turn){g.menu(1);assert.deepEqual(g.read('board',43),b);assert.equal(g.read('moves'),0);aim(g,e);g.tap('space');assert.deepEqual(g.read('board',43),next);g.tap('space');assert.deepEqual(g.read('board',43),next,'Used edge cannot be redrawn');}
   b=next;if(level===1&&turn===6)await g.save('gameplay-2');
  }
  assert.equal(g.read('grid_stat'),0);if(level===2)await g.save('gameplay-3');if(g.read('phase')===4)wins++;else losses++;
 }
 assert.ok(wins&&losses,'Winning and losing matches are playable');assert.ok(extra&&chain,'Both sides receive consecutive turns');g.tap('space');assert.equal(g.read('phase'),2);
}
