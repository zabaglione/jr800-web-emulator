// SPDX-License-Identifier: MIT
// Independent certificates and real key input validate the JR-800 campaign shell.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {iceReady,iceSettle,checkIceIntro} from './ice_check.mjs';
export const puzzleIds=['box-shift','mirror-link','lamp-grid','slide-nine','ice-route','switch-maze','pipe-weave','number-rail','mine-field','loop-trace','knight-tour','peg-rescue','step-strike','ricochet-ops','pocket-factory'];
function railMove(board,direction){
 const next=[...board];let points=0;
 for(let line=0;line<4;line++){
  const indices=Array.from({length:4},(_,step)=>direction===0?step*4+line:direction===1?(3-step)*4+line:direction===2?line*4+step:line*4+3-step);
  const values=indices.map(p=>board[p]).filter(Boolean),out=[];
  for(let i=0;i<values.length;i++){
   if(values[i]===values[i+1]&&values[i]<11){out.push(values[i]+1);points+=2**(values[i]+1);i++;}else out.push(values[i]);
  }
  for(let i=0;i<4;i++)next[indices[i]]=out[i]??0;
 }return {board:next,points,changed:next.some((n,i)=>n!==board[i])};
}

const alphabet='2346789ACDEFHJKM';

export function password(id,stage,scores){
 const digits=[puzzleIds.indexOf(id),stage>>4,stage&15];
 if(scores){assert.equal(scores.length,40);for(let i=0;i<40;i+=2)digits.push(scores[i]*4+scores[i+1]);}
 // CRC-8/MAXIM with the campaign format's initial value; symbols are packed nibbles.
 let crc=0xa7;
 for(const byte of digits){crc^=byte;for(let bit=0;bit<8;bit++)crc=(crc>>>1)^((crc&1)?0x8c:0);}
 return [...digits,crc>>4,crc&15];
}

export function aim(g,cell,width){
 while(g.read('cursor')%width<cell%width)g.tap('right');
 while(g.read('cursor')%width>cell%width)g.tap('left');
 while(Math.floor(g.read('cursor')/width)<Math.floor(cell/width))g.tap('down');
 while(Math.floor(g.read('cursor')/width)>Math.floor(cell/width))g.tap('up');
 assert.equal(g.read('cursor'),cell);
}

function aimPeg(g,target){
 const board=g.read('board',49),start=g.read('cursor'),todo=[[start,[]]],seen=new Set([start]);
 for(let i=0;i<todo.length;i++){
  const [p,path]=todo[i];if(p===target){for(const key of path)g.tap(key);return;}
  for(const [dx,dy,key] of [[0,-1,'up'],[0,1,'down'],[-1,0,'left'],[1,0,'right']]){
   const x=p%7+dx,y=Math.floor(p/7)+dy,q=y*7+x;
   if(x>=0&&x<7&&y>=0&&y<7&&board[q]!==255&&!seen.has(q)){seen.add(q);todo.push([q,[...path,key]]);}
  }
 }
 throw new Error('Unreachable peg cursor position');
}

function pegJump(g,move){aimPeg(g,move[0]);g.tap('space');assert.equal(g.read('selection_active'),1);aimPeg(g,move[1]);g.tap('space');}

function tacticalKey(g,key){if(key==='wait')g.menu(1);else g.tap(key==='fire'?'space':key);}
function ricoShot(g,[column,angle]){
 while(g.read('cursor')<40+column)g.tap('right');while(g.read('cursor')>40+column)g.tap('left');
 while(g.read('rico_aim')!==angle)g.tap('up');g.tap('space');
 for(let f=0;g.read('rico_active')&&g.read('phase')===2&&f<3000;f++)g.frame();
 assert.ok(!g.read('rico_active')||g.read('phase')===4);
}
function factoryPlace(g,[p,tile]){
 aim(g,p,16);const tool=tile?tile-6:6;
 if(g.read('tool')!==tool){g.menu(1);while(g.read('tool')!==tool)g.tap('right');g.tap('space');}
 g.tap('space');assert.equal(g.read('board',112)[p],tile);
}

function select(g){
 if(g.read('phase')===2)g.menu(4);
 else if([4,5,6].includes(g.read('phase')))g.tap('return');
 else if(g.read('phase')===0)g.tap('space');
 assert.equal(g.read('phase'),1);
}

export function enterPassword(g,digits){
 select(g);for(let i=0;i<(digits.length===5?1:2);i++)g.tap('down');g.tap('space');
 assert.equal(g.read('phase'),6);assert.equal(g.read('password_length'),digits.length);
 for(let i=0;i<digits.length;i++){
  assert.equal(g.read('password_cursor'),i);
  let delta=(digits[i]-g.read('password_digits',25)[i]+16)%16;
  const key=delta<=8?'up':'down';if(delta>8)delta=16-delta;
  for(let j=0;j<delta;j++)g.tap(key);
  assert.equal(g.read('password_digits',25)[i],digits[i],'One logical character change per short direction input');
  if(i<digits.length-1)g.tap('right');
 }
 g.tap('space');
}

export function checkPasswords(g,id){
 select(g);
 const stage=g.read('stage'),best=g.read('challenge_best',40);
 assert.deepEqual(g.read('password_digits',25),password(id,stage,best),'Displayed record encodes each independent two-bit best');
 const text=String.fromCharCode(...g.read('password_text',30)).split('\0')[0];
 assert.equal(text.replaceAll(' ',''),password(id,stage,best).map(n=>alphabet[n]).join(''));
 const bad=password(id,stage,best);bad[12]^=1;
 enterPassword(g,bad);assert.equal(g.read('phase'),6);assert.equal(g.read('password_error'),1);
 assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('challenge_best',40),best,'Bad CRC changes no progress');
 const other=password(puzzleIds[(puzzleIds.indexOf(id)+1)%15],stage);
 enterPassword(g,other);assert.equal(g.read('phase'),6);assert.equal(g.read('password_error'),1);
 assert.deepEqual(g.read('challenge_best',40),best,'A code from another game is rejected');
 enterPassword(g,password(id,39));assert.equal(g.read('phase'),1);assert.equal(g.read('stage'),39);
 assert.deepEqual(g.read('challenge_best',40),best,'A short code preserves earned records');
 const pattern=Array.from({length:40},(_,i)=>i%4);
 enterPassword(g,password(id,23,pattern));assert.equal(g.read('phase'),1);assert.equal(g.read('stage'),23);
 assert.deepEqual(g.read('challenge_best',40),pattern,'All forty records restore with all four values');
 enterPassword(g,password(id,stage,best));assert.equal(g.read('phase'),1);assert.equal(g.read('stage'),stage);
 assert.deepEqual(g.read('challenge_best',40),best);
 g.frame();assert.equal(g.word('dirty_bytes'),0,'Idle selection sends no LCD data');
}

async function lampRoute(g,stage,route){
 let board=stage.initial.board.slice(),mask=0;
 for(const cell of route){
  aim(g,cell,5);
  for(const q of [cell,cell-5,cell+5,cell%5?cell-1:-1,cell%5<4?cell+1:-1])if(q>=0&&q<25)board[q]^=1;
  stage.bonus_cells.forEach((p,i)=>{if(p===cell)mask|=1<<i;});
  g.tap('space');assert.deepEqual(g.read('board',25),board,'The specified cross and no other lamp toggles');
  assert.equal(g.read('challenge_bonus'),mask);
 }
 assert.equal(board.some(Boolean),false);
}

async function walkRoute(g,id,stage,route){
 if(id==='ice-route')iceReady(g);
 const board=stage.initial.board.slice();let p=stage.initial.start,keys=2,gates=0,bonus=0;
 assert.deepEqual(g.read('board',98),board);assert.equal(g.read('cursor'),p);
 for(const key of route){
  const d={up:-14,down:14,left:-1,right:1}[key];
  if(id==='ice-route'){
   assert.notEqual(board[p+d],1);
   do{p+=d;if([3,4].includes(board[p])){board[p]=0;keys--;}
    stage.bonus_cells.forEach((cell,i)=>{if(cell===p)bonus|=1<<i;});
    if(board[p]===2&&!keys)break;
   }while(board[p+d]!==1);
  }else{
   p+=d;const tile=board[p];assert.notEqual(tile,1);
   if([6,7].includes(tile))assert.ok(gates&(1<<(tile-6)));
   if([2,3].includes(tile)){board[p]=0;keys--;}
   if([4,5].includes(tile))gates^=1<<(tile-4);
   stage.bonus_cells.forEach((cell,i)=>{if(cell===p)bonus|=1<<i;});
  }
  const from=g.read('cursor');
  g.tap(key);if(id==='ice-route')iceSettle(g,from,p,d);
  assert.equal(g.read('cursor'),p);assert.deepEqual(g.read('board',98),board);
  assert.equal(g.read('grid_stat'),keys);assert.equal(g.read('challenge_bonus'),bonus);
  if(id==='switch-maze')assert.equal(g.read('gates'),gates);
 }
}

async function route(g,id,stage,actions){
 if(id==='lamp-grid')return lampRoute(g,stage,actions);
 if(['ice-route','switch-maze'].includes(id))return walkRoute(g,id,stage,actions);
 if(id==='box-shift'){
  const board=stage.initial.board.slice();let p=stage.initial.start,bonus=0;
  assert.deepEqual(g.read('board',112),board);assert.equal(g.read('player'),p);
  for(const key of actions){
   const d={up:-16,down:16,left:-1,right:1}[key],q=p+d;
   assert.equal(board[q]&1,0);
   if(board[q]&4){assert.equal(board[q+d]&5,0);board[q]&=~4;board[q+d]|=4;}
   p=q;stage.bonus_cells.forEach((cell,i)=>{if(cell===p)bonus|=1<<i;});
   g.tap(key);assert.equal(g.read('player'),p);assert.deepEqual(g.read('board',112),board);assert.equal(g.read('challenge_bonus'),bonus);
  }
  assert.ok(board.every(tile=>(tile&6)!==4));return;
 }
 if(id==='mirror-link'){
  const board=stage.initial.board.slice();let bonus=0;
  for(const cell of actions){
   aim(g,cell,16);assert.ok([2,3].includes(board[cell]));board[cell]^=1;
   const beams=Array(112).fill(0);
   for(const source of stage.initial.sources){
    let p=source,d=0;const seen=new Set();
    for(;;){
     p+=[1,16,-1,-16][d];if(p<0||p>=112||board[p]===1||seen.has(p*4+d))break;
     seen.add(p*4+d);beams[p]|=(d&1)+1;
     if(board[p]===2)d^=3;else if(board[p]===3)d^=1;else if(board[p]>=4)break;
    }
   }
   stage.bonus_cells.forEach((p,i)=>{if(beams[p])bonus|=1<<i;});
   g.tap('space');assert.deepEqual(g.read('board',112),board);assert.deepEqual(g.read('beams',112),beams);
   assert.equal(g.read('challenge_bonus'),bonus);assert.equal(g.read('lit_count'),board.filter((tile,p)=>tile===4&&beams[p]).length);
  }
  return;
 }
 if(id==='slide-nine'){
  let board=stage.initial.board.slice(),p=board.indexOf(0),bonus=0;
  assert.deepEqual(g.read('board',9),board);
  for(const key of actions){
   const q=p+{up:-3,down:3,left:-1,right:1}[key];
   if(board[q]===stage.bonus_tile&&p===stage.bonus_cells[0])bonus=1;
   [board[p],board[q]]=[board[q],board[p]];p=q;g.tap(key);
   assert.deepEqual(g.read('board',9),board);assert.equal(g.read('cursor'),p);assert.equal(g.read('challenge_bonus'),bonus);
  }
  assert.deepEqual(board,[1,2,3,4,5,6,7,8,0]);return;
 }
 if(id==='step-strike'){
  const board=stage.initial.board;let p=stage.initial.start,face=0,guards=stage.initial.guards.slice(),bullets=[],turn=0,bonus=0;
  for(const key of actions){
   const action=['right','down','left','up','fire','wait'].indexOf(key),deltas=[1,16,-1,-16];assert.ok(action>=0);
   if(action<4){p+=deltas[action];assert.equal(board[p],0);face=action;guards=guards.filter(g=>g!==p);}
   else if(action===4){let q=p;for(let r=0;r<4;r++){q+=deltas[face];if(board[q])break;if(guards.includes(q)){guards=guards.filter(g=>g!==q);break;}}}
   stage.bonus_cells.forEach((cell,i)=>{if(p===cell)bonus|=1<<i;});
   assert.ok(bullets.every(([q])=>q!==p));bullets=bullets.map(([q,d])=>[q+deltas[d],d]).filter(([q])=>!board[q]);assert.ok(bullets.every(([q])=>q!==p));
   turn++;
   if(turn%3===0)for(const g of guards){
    let d;if(Math.floor(p/16)===Math.floor(g/16))d=p>g?0:2;else if(p%16===g%16)d=p>g?1:3;else continue;
    let q=g+deltas[d];while(q!==p&&!board[q])q+=deltas[d];if(q===p&&bullets.length<8)bullets.push([g,d]);
   }
   tacticalKey(g,key);assert.equal(g.read('player'),p);assert.equal(g.read('facing'),face);assert.equal(g.read('guard_count'),guards.length);
   assert.deepEqual(g.read('guards',4).filter(x=>x!==255),guards);
   const cells=g.read('bullet_cells',8),dirs=g.read('bullet_dirs',8),actual=cells.flatMap((q,i)=>q===255?[]:[[q,dirs[i]]]);
   const sort=(a,b)=>a[0]-b[0]||a[1]-b[1];assert.deepEqual(actual.sort(sort),bullets.slice().sort(sort));assert.equal(g.read('challenge_bonus'),bonus);
  }
  assert.equal(guards.length,0);return;
 }
 if(id==='ricochet-ops'){
  const board=stage.initial.board.slice();let bonus=0,ammo=stage.initial.ammo,score=0;
  const dirs=[[0,-1],[1,-1],[1,0],[1,1],[0,1],[-1,1],[-1,0],[-1,-1]],slash=[2,1,0,7,6,5,4,3],back=[6,5,4,3,2,1,0,7];
  for(const [col,initial] of actions){
   let p=40+col,angle=initial,left=board.filter(v=>v===4).length;const seen=Array(48).fill(0),trail=Array(48).fill(0);
   for(;;){const [dx,dy]=dirs[angle],x=p%8+dx,y=Math.floor(p/8)+dy;if(x<0||x>=8||y<0||y>=6)break;p=y*8+x;
    if(board[p]===1||p===40+col||(seen[p]&(1<<angle)))break;seen[p]|=1<<angle;trail[p]=1;
    stage.bonus_cells.forEach((cell,i)=>{if(cell===p)bonus|=1<<i;});
    if(board[p]===4){board[p]=0;left--;score+=100;if(!left)break;}
    else if(board[p]===2)angle=slash[angle];else if(board[p]===3)angle=back[angle];
   }
   ammo--;ricoShot(g,[col,initial]);assert.deepEqual(g.read('board',48),board);assert.equal(g.read('rico_left'),left);assert.equal(g.read('rico_ammo'),ammo);assert.equal(g.word('rico_score'),score);
   assert.deepEqual(g.read('rico_visited',48),seen);assert.deepEqual(g.read('rico_trail',48),trail);assert.equal(g.read('challenge_bonus'),bonus);
  }
  return;
 }
 if(id==='pocket-factory'){
  const board=stage.initial.board.slice();assert.deepEqual(g.read('board',112),board);
  for(const action of actions){factoryPlace(g,action);board[action[0]]=action[1];}
  assert.equal(g.read('factory_steps'),0,'Construction freezes transport');
  let items=Array(112).fill(0),steps=0,shipped=[0,0],bonus=0;const flow=[0,1,1,0,0,0,1,16,-1,-16,1,1];
  const tick=()=>{
   const next=items.slice();
   for(let p=0;p<112;p++){
    const value=items[p],tile=board[p];if(!value)continue;
    if(tile===3||tile===4){if(value===tile){const lane=tile-3;shipped[lane]=Math.min(stage.initial.targets[lane],shipped[lane]+1);next[p]=0;}continue;}
    if((tile===10&&value===1)||(tile===11&&value===2)){next[p]=value+2;continue;}
    const q=p+flow[tile];if(q<0||q>=112||board[q]<3||board[q]===5||items[q]||next[q])continue;
    if(Math.floor(q/16)!==Math.floor(p/16)&&Math.abs(q-p)!==16)continue;
    next[q]=value;next[p]=0;if(value>=3)stage.bonus_cells.forEach((cell,i)=>{if(cell===q)bonus|=1<<i;});
   }
   steps++;if(steps%3===0)for(let p=0;p<112;p++)if(board[p]>0&&board[p]<3&&!next[p])next[p]=board[p];items=next;
  };
  const frame=g.frame;
  g.frame=function(){const panel=frame.call(g);while((steps&255)!==g.read('factory_steps'))tick();assert.deepEqual(g.read('items',112),items);assert.equal(g.read('shipped_a'),shipped[0]);assert.equal(g.read('shipped_b'),shipped[1]);assert.equal(g.read('challenge_bonus'),bonus);return panel;};
  try{g.menu(2);for(let f=0;g.read('phase')===2&&f<5000;f++)g.frame();}finally{g.frame=frame;}
  assert.deepEqual(shipped,stage.initial.targets);return;
 }
 if(id==='number-rail'){
  let board=stage.initial.board.slice(),seed=stage.initial.seed,score=0;
  assert.deepEqual(g.read('board',16),board);assert.equal(g.read('seed'),seed);
  for(const key of actions){
   const next=railMove(board,['up','down','left','right'].indexOf(key));assert.ok(next.changed);board=next.board;score=Math.min(65535,score+next.points);
   let p;do{seed=(seed>>>1)^((seed&1)?0xb8:0);p=seed&15;}while(board[p]);seed=(seed>>>1)^((seed&1)?0xb8:0);board[p]=(seed&15)?1:2;
   g.tap(key);assert.deepEqual(g.read('board',16),board);assert.equal(g.read('seed'),seed);assert.equal(g.word('rail_score'),score);
  }
  assert.equal(g.read('challenge_bonus'),Number(board[stage.bonus_cells[0]]>=stage.initial.goal));return;
 }
 if(id==='mine-field'){
  const board=stage.initial.board.slice();
  const adjacent=p=>{const out=[];for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){const x=p%14+dx,y=Math.floor(p/14)+dy;if((dx||dy)&&x>=0&&x<14&&y>=0&&y<7)out.push(y*14+x);}return out;};
  const reveal=p=>{assert.equal(board[p]&16,0);assert.equal(board[p]&32,0);board[p]|=32;const todo=[p];for(let i=0;i<todo.length;i++)if(!(board[todo[i]]&15))for(const q of adjacent(todo[i]))if(!(board[q]&96)){assert.equal(board[q]&16,0);board[q]|=32;todo.push(q);}};
  reveal(stage.initial.start);assert.deepEqual(g.read('board',98),board);
  for(const [kind,p] of actions){
   aim(g,p,14);
   if(kind==='flag'){assert.ok(board[p]&16);if(!g.read('selection_active'))g.menu(1);board[p]|=64;}
   else{if(g.read('selection_active'))g.tap('return');reveal(p);}
   g.tap('space');assert.deepEqual(g.read('board',98),board);assert.equal(g.read('mine_flags'),board.filter(v=>v&64).length);assert.equal(g.read('grid_stat'),board.filter(v=>!(v&48)).length);
  }
  return;
 }
 if(id==='pipe-weave'){
  const board=stage.initial.board.slice();assert.deepEqual(g.read('board',36),board);
  for(const cell of actions){
   const m=board[cell];board[cell]=((m&1)<<3)|((m&8)>>2)|((m&2)<<1)|((m&4)>>2);
   const wet=Array(36).fill(0),todo=[0];wet[0]=1;let leaks=0;
   const dirs=[[0,-1,1,2],[0,1,2,1],[-1,0,4,8],[1,0,8,4]];
   for(let p=0;p<36;p++)for(const [dx,dy,bit,opp] of dirs){
    if(!(board[p]&bit))continue;const x=p%6+dx,y=Math.floor(p/6)+dy,q=y*6+x;
    if(x<0||x>=6||y<0||y>=6||!(board[q]&opp))leaks++;
   }
   for(let j=0;j<todo.length;j++){const p=todo[j];for(const [dx,dy,bit,opp] of dirs){
    const x=p%6+dx,y=Math.floor(p/6)+dy,q=y*6+x;
    if(x>=0&&x<6&&y>=0&&y<6&&(board[p]&bit)&&(board[q]&opp)&&!wet[q]){wet[q]=1;todo.push(q);}
   }}
   aim(g,cell,6);g.tap('space');assert.deepEqual(g.read('board',36),board);assert.deepEqual(g.read('pipe_wet',36),wet);assert.equal(g.read('pipe_leaks'),leaks);
  }
  assert.equal(g.read('pipe_wet',36)[35],1);return;
 }
 if(id==='loop-trace'){
  const board=stage.initial.board,marks=Array(36).fill(0);let p=stage.initial.start,next=2,bonus=0;marks[p]=1;
  for(const q of actions){
   if(q==='close'){assert.equal(next,5);g.tap('space');continue;}
   const key=q===p-6?'up':q===p+6?'down':q===p-1&&Math.floor(p/6)===Math.floor(q/6)?'left':q===p+1&&Math.floor(p/6)===Math.floor(q/6)?'right':null;
   assert.ok(key);assert.equal(marks[q],0);assert.ok(board[q]);if(board[q]>1){assert.equal(board[q],next);next++;}
   marks[q]=1;p=q;stage.bonus_cells.forEach((cell,i)=>{if(cell===p)bonus|=1<<i;});
   g.tap(key);assert.equal(g.read('cursor'),p);assert.deepEqual(g.read('loop_marks',36),marks);assert.equal(g.read('grid_stat'),5-next);assert.equal(g.read('challenge_bonus'),bonus);
  }
  return;
 }
 if(id==='peg-rescue'){
  const board=stage.initial.board.slice();assert.deepEqual(g.read('board',49),board);
  for(const [p,q] of actions){
   const m=(p+q)/2;assert.equal(board[p],1);assert.equal(board[m],1);assert.equal(board[q],0);
   assert.ok((p%7===q%7&&Math.abs(p-q)===14)||(Math.floor(p/7)===Math.floor(q/7)&&Math.abs(p-q)===2));
   board[p]=board[m]=0;board[q]=1;pegJump(g,[p,q]);
   assert.deepEqual(g.read('board',49),board);assert.equal(g.read('grid_stat'),board.filter(x=>x===1).length);
  }
  assert.equal(board.filter(x=>x===1).length,1);assert.equal(g.read('challenge_bonus'),Number(board[stage.bonus_cells[0]]===1));return;
 }
 if(id==='knight-tour'){
  const board=stage.initial.board.slice();let p=stage.initial.start;board[p]=1;
  assert.deepEqual(g.read('board',36),board);
  for(let i=0;i<actions.length;i++){
   const q=actions[i],dx=Math.abs(p%6-q%6),dy=Math.abs(Math.floor(p/6)-Math.floor(q/6));
   assert.ok((dx===1&&dy===2)||(dx===2&&dy===1));assert.equal(board[q],0);board[q]=i+2;p=q;
   aim(g,q,6);g.tap('space');assert.deepEqual(g.read('board',36),board);assert.equal(g.read('knight_player'),q);
   assert.equal(g.read('grid_stat'),board.filter(x=>x===0).length);
  }
  assert.equal(g.read('challenge_bonus'),Number(p===stage.bonus_cells[0]));return;
 }
 throw new Error(`No campaign driver for ${id}`);
}

function firstAction(g,id,stage){
 if(id==='ice-route')iceReady(g);
 if(id==='lamp-grid'){aim(g,stage.bonus_cells[0],5);g.tap('space');}
 else if(id==='step-strike')tacticalKey(g,stage.normal[0]);
 else if(id==='ricochet-ops')ricoShot(g,stage.bonus[0]);
 else if(id==='pocket-factory')factoryPlace(g,stage.normal[0]);
 else if(id==='mine-field'){aim(g,stage.normal[0][1],14);g.tap('space');}
 else if(id==='pipe-weave'){aim(g,stage.normal[0],6);g.tap('space');}
 else if(id==='loop-trace'){const p=stage.initial.start,q=stage.normal[0];g.tap(q===p-6?'up':q===p+6?'down':q===p-1?'left':'right');}
 else if(id==='mirror-link'){aim(g,stage.normal[0],16);g.tap('space');}
 else if(id==='peg-rescue')pegJump(g,stage.normal[0]);
 else if(id==='knight-tour'){aim(g,stage.normal[0],6);g.tap('space');}
 else g.tap(stage.normal[0]);
}

export async function checkPuzzle(g,id){
 await g.save('title');
 const tap=g.tap.bind(g),aliases={up:'letter-w',down:'letter-s',left:'letter-a',right:'letter-d'};
 g.tap=function(key){return tap(this.useLetters?(aliases[key]??key):key);};
 const frame=g.frame.bind(g);
 g.frame=function(){const panel=frame();if(this.captureAt&&this.read('phase')===2&&this.word('challenge_moves')>=this.captureAt&&(id!=='pocket-factory'||this.read('shipped_a')+this.read('shipped_b')>0)){this.captureAt=0;this.sceneTask=this.save('gameplay-2');}return panel;};
 const source=await readFile(new URL(`../${id}/assets.s`,import.meta.url),'utf8');
 const raw=source.split('title_art:')[1].split(/\n[A-Za-z_]\w*:/)[0].match(/\$[0-9A-F]{2}/g).map(x=>parseInt(x.slice(1),16));
 assert.equal(raw.length,1536);assert.deepEqual([...g.machine.memory(g.symbols.framebuffer,1536)],raw,'Compressed title decodes to the original artwork');
 const stages=JSON.parse(await readFile(new URL(`../${id}/challenges.json`,import.meta.url))).stages;
 assert.equal(stages.length,40);await g.start();
 for(let i=0;i<stages.length;i++){
  g.useLetters=Boolean(i%2);
  const stage=stages[i];if(i)g.tap('space');assert.equal(g.read('stage'),i);assert.equal(g.word('challenge_par'),stage.par);
  if(id==='ice-route'){if(i===0)await checkIceIntro(g,stage);else iceReady(g);}
  g.frame();assert.equal(g.word('dirty_bytes'),0,'Idle puzzle sends no LCD data');
  if(i===0){
   await g.save('gameplay-1');const board=g.read('board',112);firstAction(g,id,stage);
   assert.equal(g.word('challenge_moves'),1);
   if(id==='lamp-grid')assert.notEqual(g.read('challenge_bonus'),0);
   if(!['mine-field','step-strike','pocket-factory'].includes(id)){g.menu(1);assert.equal(g.word('challenge_moves'),2,'Undo consumes an action');assert.equal(g.read('challenge_bonus'),0,'Undo restores pickups');assert.deepEqual(g.read('board',112),board);}
   g.menu(3);assert.equal(g.word('challenge_moves'),0);assert.equal(g.read('challenge_bonus'),0);
  }
  await route(g,id,stage,stage.normal);
  assert.equal(g.read('phase'),4,`${id} normal ${i+1}`);assert.equal(g.read('challenge_rating'),2);
  if(i===0){checkPasswords(g,id);g.tap('space');}
  else{g.tap('return');g.tap('space');}
  if(i===30)g.captureAt=Math.max(1,Math.floor(stage.par/2));
  await route(g,id,stage,stage.bonus);
  assert.equal(g.read('phase'),4,`${id} bonus ${i+1}`);assert.equal(g.read('challenge_rating'),3);
  assert.equal(g.word('challenge_moves'),stage.par);assert.equal(g.read('challenge_best',40)[i],3);
  if(i===30){await g.sceneTask;assert.equal(g.captureAt,0,'An advanced live scene was captured');}if(i===39)await g.save('gameplay-3');
 }
 assert.ok(g.read('challenge_best',40).every(x=>x===3));
 // Extra real actions still allow a clear, but cannot replace an existing better rank.
 g.tap('return');await g.save('selection');g.tap('space');const stage=stages[39];
 const repeats=Math.ceil((stage.par+2)/2);
 for(let i=0;i<(id==='step-strike'?0:repeats);i++){if(id==='pocket-factory'){factoryPlace(g,[stage.normal[0][0],6]);factoryPlace(g,[stage.normal[0][0],0]);}else if(id==='mine-field'){aim(g,stage.initial.mines[0],14);g.menu(1);g.tap('space');g.tap('space');g.tap('return');}else{firstAction(g,id,stage);g.menu(1);}}
 await route(g,id,stage,stage.overpar??stage.bonus);assert.equal(g.read('phase'),4);assert.equal(g.read('challenge_rating'),1);assert.equal(g.read('challenge_best',40)[39],3);
}
