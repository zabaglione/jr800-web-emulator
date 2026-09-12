// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const opposite=[1,0,3,2],keys=['up','down','left','right'];
export function neighbor(p,d){const x=p%16+(d===2?-1:d===3?1:0),y=Math.floor(p/16)+(d===0?-1:d===1?1:0);return x>=0&&x<16&&y>=0&&y<7?y*16+x:255;}
export function area(b,start,block=255){if(start===255||start===block||b[start])return 0;const q=[start],seen=new Uint8Array(112);seen[start]=1;for(let i=0;i<q.length;i++)for(let d=0;d<4;d++){const n=neighbor(q[i],d);if(n!==255&&n!==block&&!b[n]&&!seen[n]){seen[n]=1;q.push(n);}}return q.length;}
export function cpu(s,pn){let best=null;for(let d=0;d<4;d++){if(d===opposite[s.cpuDir])continue;const n=neighbor(s.enemy,d);if(n===255||s.board[n])continue;let score=s.stage===0?1:s.stage===1?[0,1,2,3].filter(q=>{const p=neighbor(n,q);return p!==255&&!s.board[p];}).length*4:area(s.board,n,pn)*2;score+=d===s.cpuDir?1:0;if(!best||score>best.score)best={n,d,score};}return best??{n:255,d:s.cpuDir,score:0};}
function turn(s,d){const b=[...s.board],pn=neighbor(s.p,d),c=cpu(s,pn),pb=pn===255||b[pn],cb=c.n===255;let result=0;if((pb&&cb)||pn===c.n)result=3;else if(pb)result=2;else if(cb)result=1;if(!result){b[pn]=1;b[c.n]=2;}return {...s,board:b,p:result?s.p:pn,enemy:result?s.enemy:c.n,dir:d,cpuDir:c.d,result};}
function snapshot(g){return {board:g.read('board',112),p:g.read('cursor'),enemy:g.read('claim_enemy'),dir:g.read('claim_dir'),cpuDir:g.read('claim_cpu_dir'),stage:g.read('stage')};}
function quality(s){const ps=[0,1,2,3].map(d=>neighbor(s.p,d)).filter(n=>n!==255&&!s.board[n]),es=[0,1,2,3].map(d=>neighbor(s.enemy,d)).filter(n=>n!==255&&!s.board[n]);return Math.max(0,...ps.map(n=>area(s.board,n)))-Math.max(0,...es.map(n=>area(s.board,n)))+ps.length*.2-es.length*.2;}
function solve(s){let beam=[{s,path:[]}];for(let t=0;t<112;t++){const next=[];for(const item of beam)for(let d=0;d<4;d++){if(d===opposite[item.s.dir])continue;const n=turn(item.s,d),path=[...item.path,d];if(n.result===1)return path;if(!n.result)next.push({s:n,path,q:quality(n)});}next.sort((a,b)=>b.q-a.q);beam=next.slice(0,160);if(!beam.length)return null;}return null;}
export async function checkClaim(g){
 const arenas=JSON.parse(await readFile(new URL('./levels.json',import.meta.url)));await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);
 let held=null,paused=false,second=false,third=false;const seen=new Set();
 const drive=d=>{if(held!==d){if(held!==null)g.hold(keys[held],false);g.hold(keys[d],true);held=d;}const old=g.read('claim_steps'),s=snapshot(g);for(let n=0;g.read('claim_steps')===old;n++){assert.ok(n<100);g.frame();}const expected=turn(s,d);assert.deepEqual(snapshot(g),Object.fromEntries(Object.entries(expected).filter(([k])=>k!=='result')),'Simultaneous movement and CPU policy match an independent board model');assert.equal(g.read('claim_mode'),expected.result?expected.result+1:1);};
 const release=()=>{if(held!==null)g.hold(keys[held],false);held=null;g.frame();};
 for(let loss=0;loss<2;loss++){g.tap('space');while(g.read('claim_mode')===1)drive(0);release();assert.equal(g.read('claim_mode'),3);if(!loss)g.tap('space');}assert.equal(g.read('phase'),5);g.tap('space');
 for(let stage=0;stage<3;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);
  while(g.read('phase')===2){
   assert.equal(g.read('claim_mode'),1);const arena=g.read('claim_arena');seen.add(arena);const expected=[...arenas[arena]];expected[50]=1;expected[61]=2;assert.deepEqual(g.read('board',112),expected);
   const plan=solve(snapshot(g));assert.ok(plan,`Winning route for difficulty ${stage+1}, arena ${arena+1}`);console.log(JSON.stringify({difficulty:stage+1,arena:arena+1,turns:plan.length}));g.tap('space');
   for(let i=0;i<plan.length;i++){
    drive(plan[i]);
    if(stage===1&&!second&&i===5){await g.save('gameplay-2');second=true;}
    if(stage===2&&!third&&i===8){await g.save('gameplay-3');third=true;}
    if(!paused&&i===5){release();g.tap('return');const frozen=snapshot(g);for(let n=0;n<30;n++)g.frame();g.tap('return');assert.deepEqual(snapshot(g),frozen);paused=true;}
   }
   release();assert.equal(g.read('claim_mode'),2);if(g.read('phase')===2)g.tap('space');
  }
  assert.equal(g.read('phase'),4);assert.equal(g.read('claim_wins'),2);
 }
 assert.ok(second&&third&&paused);assert.equal(seen.size,6);
}
