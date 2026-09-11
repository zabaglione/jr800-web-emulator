// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const children=[],positions=[];
for(let row=0;row<7;row++)for(let col=0;col<=row;col++){children.push(row<6?[(row+1)*(row+2)/2+col,(row+1)*(row+2)/2+col+1]:[]);positions.push([7-row+col*2,row]);}positions.push([18,3]);
const dirs=[[0,-1,'up'],[0,1,'down'],[-1,0,'left'],[1,0,'right']];
const nav=positions.map(([x,y])=>dirs.map(([dx,dy])=>{const c=positions.map(([X,Y],i)=>[2*((X-x)*dx+(Y-y)*dy)+3*Math.abs((X-x)*dy-(Y-y)*dx),i,(X-x)*dx+(Y-y)*dy]).filter(([, ,n])=>n>0).sort((a,b)=>a[0]-b[0]||a[1]-b[1]);return c.length?c[0][1]:255;}));
function aim(g,p){const todo=[[g.read('cursor'),[]]],seen=new Set([todo[0][0]]);for(let i=0;i<todo.length;i++){const [q,path]=todo[i];if(q===p){for(const k of path)g.tap(k);return;}for(let d=0;d<4;d++){const n=nav[q][d];if(n!==255&&!seen.has(n)){seen.add(n);todo.push([n,[...path,dirs[d][2]]]);}}}throw new Error('Card cursor disconnected');}
function rank(s,p){return p===28?s.waste.at(-1)??0:children[p].every(c=>!s.board[c])?s.board[p]:0;}
function actions(s){const available=Array.from({length:29},(_,p)=>[p,rank(s,p)]).filter(([,n])=>n),a=[];for(const [p,n] of available){if(n===13)a.push(['king',p]);else for(const [q,m] of available)if(q>p&&m+n===13)a.push(['pair',p,q]);}return a;}
function status(s){return s.board.every(n=>!n)?4:s.pos<24||actions(s).length?2:5;}
function take(s,p){if(p===28)s.waste.pop();else s.board[p]=0;}
function apply(s,action){if(action[0]==='draw'){assert.ok(s.pos<24);s.waste.push(s.stock[s.pos++]);}else if(action[0]==='king'){assert.equal(rank(s,action[1]),13);take(s,action[1]);}else{assert.notEqual(action[1],action[2]);assert.equal(rank(s,action[1])+rank(s,action[2]),13);take(s,action[1]);take(s,action[2]);}s.moves++;}
function inspect(g,s){const b=[...s.board,...s.waste,...Array(24-s.waste.length).fill(0)];assert.deepEqual(g.read('board',52),b);assert.equal(g.read('ace_stock_pos'),s.pos);assert.equal(g.read('ace_waste_count'),s.waste.length);assert.equal(g.read('grid_stat'),s.board.filter(Boolean).length);assert.equal(g.read('moves'),s.moves);assert.equal(g.read('phase'),status(s));}
async function play(g,s,action,capture=false){
 if(action[0]==='draw')g.menu(1);
 else {aim(g,action[1]);g.tap('space');if(action[0]==='pair'){assert.equal(g.read('selection_active'),1);if(capture)await g.save('gameplay-2');aim(g,action[2]);g.tap('space');}}
 apply(s,action);inspect(g,s);
}
export async function checkAce(g){
 const levels=JSON.parse(await readFile(new URL('../ace-stack/solutions.json',import.meta.url)));await g.start();
 const fresh=stage=>({board:levels.deals[stage].slice(0,28),stock:levels.deals[stage].slice(28),waste:[],pos:0,moves:0});let s=fresh(0);
 await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);aim(g,0);g.tap('space');assert.equal(g.read('selection_active'),0,'Covered card cannot be chosen');inspect(g,s);
 await play(g,s,['draw']);const one=structuredClone(s);await play(g,s,['draw']);g.menu(2);s=one;inspect(g,s);g.menu(3);s=fresh(0);inspect(g,s);
 for(let i=0;i<24;i++)await play(g,s,['draw']);
 for(let i=0;i<52&&status(s)===2;i++)await play(g,s,actions(s)[0]);
 assert.equal(g.read('phase'),5,'Careless stock use can exhaust the legal moves');g.tap('space');s=fresh(0);inspect(g,s);
 let undone=false,second=false,third=false;
 for(let stage=0;stage<20;stage++){
  if(stage){g.tap('space');s=fresh(stage);}assert.equal(g.read('stage'),stage);inspect(g,s);for(let rank=1;rank<=13;rank++)assert.equal(levels.deals[stage].filter(n=>n===rank).length,4,'Standard 52-card rank counts');
  for(const action of levels.steps[stage]){
   const before=structuredClone(s),capture=stage===7&&!second&&action[0]==='pair'&&action.includes(28);
   await play(g,s,action,capture);if(capture)second=true;
   if(!undone&&action[0]==='pair'&&action.includes(28)&&status(s)===2){g.menu(2);s=before;inspect(g,s);await play(g,s,action);undone=true;}
   if(stage===19&&!third&&s.board.filter(Boolean).length<=8&&status(s)===2){await g.save('gameplay-3');third=true;}
  }
  assert.equal(g.read('phase'),4);
 }
 assert.ok(undone&&second&&third);
}
