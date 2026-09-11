// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {checkReversi} from './reversi_check.mjs';
import {checkFive} from './five_check.mjs';
import {checkHex} from './hex_check.mjs';
import {checkPawn} from './pawn_check.mjs';
import {checkDot} from './dot_check.mjs';
export async function checkBoard(g,id){
 if(id==='dot-claim')return checkDot(g);
 if(id==='pawn-race')return checkPawn(g);
 if(id==='hex-front')return checkHex(g);
 if(id==='five-stones')return checkFive(g);
 if(id==='reversi-mini')return checkReversi(g);
 if(id!=='line-four')throw new Error(`No board driver for ${id}`);
 const lines=[];
 for(let y=0;y<6;y++)for(let x=0;x<7;x++)for(const [dx,dy] of [[1,0],[0,1],[1,1],[-1,1]]){
  if(x+3*dx>=0&&x+3*dx<7&&y+3*dy<6)lines.push([0,1,2,3].map(i=>(y+i*dy)*7+x+i*dx));
 }
 const slot=(b,c)=>{for(let y=5;y>=0;y--)if(!b[y*7+c])return y*7+c;return -1;};
 const winner=b=>{for(const line of lines){const n=b[line[0]];if(n&&line.every(p=>b[p]===n))return n;}return 0;};
 const evaluate=b=>{
  let score=0;for(const line of lines){const a=line.filter(p=>b[p]===1).length,c=line.filter(p=>b[p]===2).length;
   if(!c)score+=[0,1,12,90,100000][a];if(!a)score-=[0,1,12,90,100000][c];
  }return score;
 };
 const order=[3,2,4,1,5,0,6];
 function search(b,depth,side,alpha=-1e9,beta=1e9){
  const win=winner(b);if(win)return win===1?100000+depth:-100000-depth;
  if(!depth)return evaluate(b);
  let score=side===1?-1e9:1e9,found=false;
  for(const c of order){const p=slot(b,c);if(p<0)continue;found=true;b[p]=side;const v=search(b,depth-1,3-side,alpha,beta);b[p]=0;
   if(side===1){score=Math.max(score,v);alpha=Math.max(alpha,score);}else{score=Math.min(score,v);beta=Math.min(beta,score);}if(alpha>=beta)break;
  }return found?score:0;
 }
 function bestColumn(b){let best=-1e9,col=-1;for(const c of order){const p=slot(b,c);if(p<0)continue;b[p]=1;const v=search(b,4,2);b[p]=0;if(v>best){best=v;col=c;}}return col;}
 function drop(col){while(g.read('cursor')<col)g.tap('right');while(g.read('cursor')>col)g.tap('left');
  const b=g.read('board',42),p=slot(b,col);g.tap('space');const after=g.read('board',42);
  if(p<0){assert.deepEqual(after,b);return;}
  assert.equal(after[p],1,'Human disk falls to the lowest free cell');
  const changed=after.map((c,i)=>c!==b[i]?i:-1).filter(i=>i>=0);assert.ok(changed.length>=1&&changed.length<=2);
  for(const i of changed)if(i!==p){assert.equal(after[i],2);b[p]=1;assert.equal(slot(b,i%7),i,'CPU obeys gravity');}
  const win=winner(after);assert.equal(g.read('phase'),win===1?4:win===2?5:after.every(Boolean)?4:2);
 }
 await g.start(2);
 for(let turns=0;turns<21&&g.read('phase')===2;turns++){
  const b=g.read('board',42);let selected=-1,score=1e9;
  for(const c of [0,6,1,5,2,4,3]){const p=slot(b,c);if(p<0)continue;b[p]=1;const s=evaluate(b);b[p]=0;if(s<score){score=s;selected=c;}}
  drop(selected);
 }
 assert.equal(g.read('phase'),5,'CPU can win a legal match');g.tap('space');assert.deepEqual(g.read('board',42),Array(42).fill(0),'Defeat retry clears the board');
 g.tap('return');for(let i=0;i<4;i++)g.tap('down');g.tap('space');assert.equal(g.read('phase'),1);g.tap('left');g.tap('left');g.tap('space');
 for(let difficulty=0;difficulty<3;difficulty++){
  if(difficulty)g.tap('space');assert.equal(g.read('stage'),difficulty);
  g.frame();assert.equal(g.word('dirty_bytes'),0,'Stationary board performs no transfer');
  if(!difficulty){
   const b=g.read('board',42);drop(3);g.menu(1);assert.deepEqual(g.read('board',42),b,'Undo restores both disks');assert.equal(g.read('moves'),0);
   g.hold('space',true);for(let i=0;i<5;i++)g.frame();g.hold('space',false);g.frame();assert.equal(g.read('moves'),1,'Holding SPACE drops exactly once');g.menu(2);
  }
  for(let turns=0;turns<21&&g.read('phase')===2;turns++){
   drop(bestColumn(g.read('board',42)));
   if(difficulty===0&&turns===1)await g.save('gameplay-1');
   if(difficulty===1&&turns===2)await g.save('gameplay-2');
   if(difficulty===2&&turns===4)await g.save('gameplay-3');
  }
  assert.equal(g.read('phase'),4,`Complete difficulty ${difficulty+1}`);
 }
}
