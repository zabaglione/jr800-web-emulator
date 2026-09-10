// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
const rays=[[-1,-1],[0,-1],[1,-1],[-1,0],[1,0],[-1,1],[0,1],[1,1]];
function flips(b,p,side){if(b[p])return [];const out=[];
 for(const [dx,dy] of rays){let x=p%6+dx,y=Math.floor(p/6)+dy;const run=[];
  while(x>=0&&x<6&&y>=0&&y<6&&b[y*6+x]===3-side){run.push(y*6+x);x+=dx;y+=dy;}
  if(x>=0&&x<6&&y>=0&&y<6&&b[y*6+x]===side)out.push(...run);
 }return out;
}
function legal(b,side){return b.map((_,p)=>({p,flip:flips(b,p,side)})).filter(m=>m.flip.length);}
function place(b,m,side){const next=[...b];for(const p of [m.p,...m.flip])next[p]=side;return next;}
function weight(p){const x=Math.min(p%6,5-p%6),y=Math.min(Math.floor(p/6),5-Math.floor(p/6));return x===0&&y===0?60:x===1&&y===1?-40:Math.min(x,y)===0&&Math.max(x,y)===1?-24:Math.min(x,y)===0?16:0;}
function cpu(b){return legal(b,2).sort((a,c)=>(weight(c.p)+c.flip.length*2)-(weight(a.p)+a.flip.length*2)||a.p-c.p)[0];}
function afterHuman(b,m){b=place(b,m,1);let passed=0;
 for(let i=0;i<36;i++){
  const c=cpu(b);
  if(!c){return {b,passed:legal(b,1).length?2:passed,end:!legal(b,1).length};}
  b=place(b,c,2);if(legal(b,1).length)return {b,passed,end:false};passed=1;
 }throw new Error('Unbounded pass loop');
}
const initial=()=>{const b=Array(36).fill(0);b[14]=b[21]=1;b[15]=b[20]=2;return b;};
function value(b){return b.reduce((s,c,p)=>s+(c===1?1:c===2?-1:0)*(weight(p)+4),0)+8*(legal(b,1).length-legal(b,2).length);}
function choose(b,style,depth=2){const options=legal(b,1);let pick=options[0],score=-1e9;
 for(const m of options){const next=afterHuman(b,m);let v=next.end?(next.b.filter(c=>c===1).length-next.b.filter(c=>c===2).length)*1000:value(next.b);
  if(style==='weak')v=-weight(m.p)-m.flip.length*2;
  else if(depth>1&&!next.end)v=choose(next.b,style,depth-1).score;
  if(v>score){score=v;pick=m;}
 }return {move:pick,score};}
export async function checkReversi(g){
 let sawPass=false,sawWin=false,sawLoss=false;
 await g.start();
 for(const [match,style] of ['weak','strong','strong'].entries()){
  if(match)g.tap('space');let b=initial();assert.deepEqual(g.read('board',36),b);
  if(!match){
   g.tap('space');assert.deepEqual(g.read('board',36),b,'Invalid square is inert');assert.equal(g.read('moves'),0);
   g.tap('right');g.tap('space');g.menu(1);assert.deepEqual(g.read('board',36),b,'Undo returns both players');assert.equal(g.read('moves'),0);g.menu(2);
   await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);
  }
  for(let turn=0;turn<32&&g.read('phase')===2;turn++){
   const moves=legal(b,1);assert.ok(moves.length);assert.deepEqual(g.read('rev_legal',36),b.map((_,p)=>flips(b,p,1).length),'All legal-move indicators');
   const {move}=choose(b,style,match===2?3:2),result=afterHuman(b,move);
   while(g.read('cursor')%6<move.p%6)g.tap('right');while(g.read('cursor')%6>move.p%6)g.tap('left');
   while(Math.floor(g.read('cursor')/6)<Math.floor(move.p/6))g.tap('down');while(Math.floor(g.read('cursor')/6)>Math.floor(move.p/6))g.tap('up');
   g.tap('space');b=result.b;assert.deepEqual(g.read('board',36),b,'Legal human and CPU flips, including forced passes');
   assert.equal(g.read('grid_stat'),b.filter(c=>c===1).length);assert.equal(g.read('rev_cpu_disks'),b.filter(c=>c===2).length);
   sawPass ||=Boolean(result.passed);
   if(match===1&&turn===4)await g.save('gameplay-2');if(match===2&&turn===10)await g.save('gameplay-3');
   const resultPhase=!result.end?2:b.filter(c=>c===1).length>=b.filter(c=>c===2).length?4:5;
   assert.equal(g.read('phase'),resultPhase);
  }
  assert.notEqual(g.read('phase'),2,'Both players finish');sawWin ||=g.read('rev_result')===1;sawLoss ||=g.read('rev_result')===2;
 }
 assert.ok(sawWin&&sawLoss,'Wins and defeats are reached by legal keys');
 assert.ok(sawPass,'A forced pass was exercised in a complete game');
}
