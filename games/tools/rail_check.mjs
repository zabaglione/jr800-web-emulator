// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
export function railMove(board,direction){
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
function spawn(b,seed){const random=()=>seed=(seed>>>1)^((seed&1)?0xb8:0);let p;do{p=random()&15;}while(b[p]);b[p]=(random()&15)?1:2;return seed;}
function evaluate(b){
 let empty=0,rough=0,mono=0,max=0;
 for(let y=0;y<4;y++)for(let x=0;x<4;x++){const a=b[y*4+x];if(!a)empty++;max=Math.max(max,a);if(x<3&&a&&b[y*4+x+1])rough+=Math.abs(a-b[y*4+x+1]);if(y<3&&a&&b[y*4+x+4])rough+=Math.abs(a-b[y*4+x+4]);}
 for(let line=0;line<4;line++)for(let axis=0;axis<2;axis++){let up=0,down=0;for(let i=0;i<3;i++){const a=b[axis?i*4+line:line*4+i],c=b[axis?(i+1)*4+line:line*4+i+1];if(a>c)down+=a-c;else up+=c-a;}mono+=Math.min(up,down);}
 return empty*300+max*100-rough*20-mono*70+Math.max(b[0],b[3],b[12],b[15])*120;
}
function choose(b,careless=false){
 const cache=new Map();
 function player(board,depth){if(!depth)return evaluate(board);const k=board.join(',')+':'+depth;if(cache.has(k))return cache.get(k);let best=-1e8;for(let d=0;d<4;d++){const next=railMove(board,d);if(next.changed)best=Math.max(best,chance(next.board,depth));}cache.set(k,best);return best;}
 function chance(board,depth){const empty=board.map((n,i)=>n? -1:i).filter(i=>i>=0);if(!empty.length)return player(board,depth-1);let value=0;for(const p of empty){board[p]=1;value+=player(board,depth-1)*15/16;board[p]=2;value+=player(board,depth-1)/16;board[p]=0;}return value/empty.length;}
 let best=careless?1e9:-1e9,pick=-1;
 for(let d=0;d<4;d++){const next=railMove(b,d);if(!next.changed)continue;const v=careless?evaluate(next.board):chance(next.board,2);if(careless?v<best:v>best){best=v;pick=d;}}
 return pick;
}
export async function checkRail(g){
 const file=new URL('../number-rail/replays.json',import.meta.url),generate=process.env.JR800_GENERATE_REPLAYS==='1';
 if(generate)assert.ok(!process.env.JR800_GAME_ROM,'Generate replay instructions with the project-authored bootstrap');
 const replay=generate?[[],[],[]]:JSON.parse(await readFile(file));const keys=['up','down','left','right'];
 await g.start();let loss=false,undo=false,second=false,third=false;
 for(let stage=0;stage<3;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);let step=0,retries=0;
  if(!stage){await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);}
  while(g.read('phase')!==4&&step<2400){
   if(g.read('phase')===5){
    loss=true;assert.ok(++retries<=6,'A goal must be reachable through play');
    if(generate)replay[stage].push('retry');else assert.equal(replay[stage][step],'retry');step++;
    let s=g.read('seed');const b=Array(16).fill(0);s=spawn(b,s);s=spawn(b,s);g.tap('space');assert.deepEqual(g.read('board',16),b);assert.equal(g.read('seed'),s);assert.equal(g.word('rail_score'),0);continue;
   }
   const b=g.read('board',16),seed=g.read('seed'),score=g.word('rail_score'),moves=g.read('moves'),totalMoves=g.word('rail_moves');
   const d=generate?choose(b,!stage&&!loss):replay[stage][step];assert.ok(Number.isInteger(d)&&d>=0&&d<4);if(generate)replay[stage].push(d);step++;
   const result=railMove(b,d);assert.ok(result.changed);const expected=[...result.board],nextSeed=spawn(expected,seed);g.tap(keys[d]);
   assert.deepEqual(g.read('board',16),expected,'Single merge per original tile and exactly one random spawn');assert.equal(g.read('seed'),nextSeed);assert.equal(g.word('rail_score'),Math.min(65535,score+result.points));assert.equal(g.read('moves'),Math.min(255,moves+1));
   assert.equal(g.word('rail_moves'),Math.min(9999,totalMoves+1),'Long matches keep a four-digit move count');
   const won=Math.max(...expected)>=[7,9,11][stage],stuck=![0,1,2,3].some(dir=>railMove(expected,dir).changed);
   assert.equal(g.read('phase'),won?4:stuck?5:2);
   if(!undo&&g.read('phase')===2){
    g.menu(1);assert.deepEqual(g.read('board',16),b);assert.equal(g.word('rail_score'),score);assert.equal(g.word('rail_moves'),totalMoves);assert.equal(g.read('seed'),seed);g.tap(keys[d]);assert.deepEqual(g.read('board',16),expected,'Undo replays the same random tile');undo=true;
   }
   if(stage===1&&!second&&Math.max(...expected)>=6){await g.save('gameplay-2');second=true;}
   if(stage===2&&!third&&Math.max(...expected)>=10){await g.save('gameplay-3');third=true;}
  }
  assert.equal(g.read('phase'),4,`Reach goal ${stage+1}`);if(!generate)assert.equal(step,replay[stage].length);
 }
 assert.ok(loss&&undo&&second&&third);if(generate)await writeFile(file,JSON.stringify(replay)+'\n');
}
