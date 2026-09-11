// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const rank=(s,p)=>(p>=28||!s.board[p+7])?s.board[p]:0;
const matches=(a,b)=>a&&[1,12].includes(Math.abs(a-b));
const legal=s=>Array.from({length:35},(_,p)=>p).filter(p=>matches(rank(s,p),s.waste));
const status=s=>s.board.every(n=>!n)?4:s.pos<16||legal(s).length?2:5;
function aim(g,p){for(const [key,d,test] of [['right',1,()=>g.read('cursor')%7<p%7],['left',-1,()=>g.read('cursor')%7>p%7],['down',7,()=>Math.floor(g.read('cursor')/7)<Math.floor(p/7)],['up',-7,()=>Math.floor(g.read('cursor')/7)>Math.floor(p/7)]])while(test())g.tap(key);}
function inspect(g,s){assert.deepEqual(g.read('board',35),s.board);assert.equal(g.read('suit_waste'),s.waste);assert.equal(g.read('suit_stock_pos'),s.pos);assert.equal(g.read('suit_chain'),s.chain);assert.equal(g.word('suit_score'),s.score);assert.equal(g.read('moves'),s.moves);assert.equal(g.read('grid_stat'),s.board.filter(Boolean).length);assert.equal(g.read('phase'),status(s));}
function play(g,s,a){if(a[0]==='draw'){assert.ok(s.pos<16);s.waste=s.stock[s.pos++];s.chain=0;g.menu(1);}else{const p=a[1];assert.ok(matches(rank(s,p),s.waste));s.waste=s.board[p];s.board[p]=0;s.score+=++s.chain;aim(g,p);g.tap('space');}s.moves++;inspect(g,s);}
export async function checkSuit(g){
 const levels=JSON.parse(await readFile(new URL('../suit-run/solutions.json',import.meta.url)));const fresh=i=>({board:levels.deals[i].slice(0,35),stock:levels.deals[i].slice(36),waste:levels.deals[i][35],pos:0,chain:0,score:0,moves:0});
 await g.start();let s=fresh(0);await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);aim(g,0);g.tap('space');inspect(g,s);
 play(g,s,['draw']);const once=structuredClone(s);play(g,s,['draw']);g.menu(2);s=once;inspect(g,s);g.menu(3);s=fresh(0);
 for(let i=0;i<16;i++)play(g,s,['draw']);
 while(status(s)===2)play(g,s,['take',legal(s)[0]]);
 assert.equal(status(s),5);g.tap('space');s=fresh(0);let undone=false,second=false,third=false,wrapped=false;
 for(let stage=0;stage<20;stage++){
  if(stage){g.tap('space');s=fresh(stage);}assert.equal(g.read('stage'),stage);for(let n=1;n<=13;n++)assert.equal(levels.deals[stage].filter(v=>v===n).length,4);inspect(g,s);
  for(const a of levels.steps[stage]){
   const before=structuredClone(s);if(a[0]==='take'&&Math.abs(s.waste-s.board[a[1]])===12)wrapped=true;play(g,s,a);
   if(!undone&&a[0]==='take'&&s.chain>1&&status(s)===2){g.menu(2);s=before;inspect(g,s);play(g,s,a);undone=true;}
   if(stage===8&&!second&&s.chain>=4){await g.save('gameplay-2');second=true;}
   if(stage===19&&!third&&s.board.filter(Boolean).length<=7&&status(s)===2){await g.save('gameplay-3');third=true;}
  }assert.equal(g.read('phase'),4);
 }assert.ok(undone&&second&&third&&wrapped);
}
