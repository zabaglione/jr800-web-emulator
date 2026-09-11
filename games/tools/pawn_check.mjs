// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
export function pawnMoves(b,side){
 const moves=[];
 for(let p=0;p<36;p++)if(b[p]===side)for(const dx of [0,-1,1]){
  const x=p%6+dx,y=Math.floor(p/6)+(side===1?-1:1),q=y*6+x;
  if(x>=0&&x<6&&y>=0&&y<6&&b[q]!==side&&(dx||!b[q]))moves.push([p,q]);
 }return moves;
}
function apply(b,[p,q],side){const next=[...b];next[p]=0;next[q]=side;return next;}
function win(b,side){return b.slice(side===1?0:30,side===1?6:36).includes(side)||!pawnMoves(b,3-side).length;}
function cpu(b,level){
 let best,score=0;
 for(const move of pawnMoves(b,2)){
  const q=move[1],next=apply(b,move,2),human=pawnMoves(next,1);
  let s=32+q+(b[q]?(level?16:4):0);
  if(q>=30||!human.length)s=255;
  else if(level===2&&human.some(([,to])=>to<6))s=1;
  else if(level&&human.some(([,to])=>to===q))s-=14;
  if(s>score){score=s;best=move;}
 }return best;
}
function evaluate(b){
 let value=0,advance=[0,0];
 for(let p=0;p<36;p++)if(b[p]){const side=b[p],row=side===1?5-Math.floor(p/6):Math.floor(p/6);advance[side-1]=Math.max(advance[side-1],row);value+=(side===1?1:-1)*(15+row*row*3);}
 return value+advance[0]**3*6-advance[1]**3*6;
}
function human(b,level,depth=2){
 const cache=new Map();
 function search(board,n){
  const key=board.join('')+n;if(cache.has(key))return cache.get(key);
  if(!n)return [evaluate(board),null];let best=-1e8,pick;
  for(const move of pawnMoves(board,1)){
   let next=apply(board,move,1),s;
   if(win(next,1))s=100000+n;
   else {next=apply(next,cpu(next,level),2);s=win(next,2)?-100000-n:search(next,n-1)[0];}
   if(s>best){best=s;pick=move;}
  }const result=[best,pick];cache.set(key,result);return result;
 }return search(b,depth)[1];
}
function careless(b,level){
 let value=1e9,pick;
 for(const move of pawnMoves(b,1)){
  let next=apply(b,move,1),s=100000;
  if(!win(next,1)){next=apply(next,cpu(next,level),2);s=win(next,2)?-100000:evaluate(next);}
  if(s<value){value=s;pick=move;}
 }return pick;
}
function aim(g,p){while(g.read('cursor')%6<p%6)g.tap('right');while(g.read('cursor')%6>p%6)g.tap('left');while(Math.floor(g.read('cursor')/6)<Math.floor(p/6))g.tap('down');while(Math.floor(g.read('cursor')/6)>Math.floor(p/6))g.tap('up');}
export async function checkPawn(g){
 await g.start();let wins=0,losses=0;
 for(let level=0;level<3;level++){
  if(level){g.tap('return');assert.equal(g.read('phase'),1);g.tap('right');g.tap('space');}
  assert.equal(g.read('stage'),level);let b=g.read('board',36);
  assert.deepEqual(b,[...Array(12).fill(2),...Array(12).fill(0),...Array(12).fill(1)]);
  if(!level){await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);g.tap('space');g.tap('return');assert.equal(g.read('selection_active'),0);assert.equal(g.read('phase'),2);}
  for(let turn=0;turn<60&&g.read('phase')===2;turn++){
   const move=level===0?careless(b,level):human(b,level),[p,q]=move;
   aim(g,p);g.tap('space');const opts=pawnMoves(b,1).filter(([from])=>from===p).map(([,to])=>to);
   assert.deepEqual(g.read('pawn_options',36),b.map((_,i)=>Number(opts.includes(i))));
   if(level===1&&turn===2)await g.save('gameplay-2');
   aim(g,q);g.tap('space');let next=apply(b,move,1),state=4;
   if(!win(next,1)){next=apply(next,cpu(next,level),2);state=win(next,2)?5:2;}
   assert.deepEqual(g.read('board',36),next,'Human and CPU obey forward/diagonal movement');assert.equal(g.read('phase'),state);assert.equal(g.read('grid_stat'),next.filter(n=>n===2).length);
   if(!level&&!turn){g.menu(1);assert.deepEqual(g.read('board',36),b);assert.equal(g.read('moves'),0);aim(g,p);g.tap('space');aim(g,q);g.tap('space');assert.deepEqual(g.read('board',36),next);}
   b=next;if(level===2&&turn===6)await g.save('gameplay-3');
  }
  assert.ok([4,5].includes(g.read('phase')));if(g.read('phase')===4)wins++;else losses++;
 }
 assert.ok(wins,'A legal match can be won');assert.ok(losses,'CPU can win a legal match');
 g.tap('space');assert.equal(g.read('phase'),2);
}
