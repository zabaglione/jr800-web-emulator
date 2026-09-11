// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const dirs=[[-15,'up'],[15,'down'],[-1,'left'],[1,'right']];
const blocked=v=>[1,2,4].includes(v);
const state=g=>({board:g.read('board',105),p:g.read('cursor'),cell:g.read('bomb_cell'),fuse:g.read('bomb_fuse'),flames:g.read('bomb_flames',105),flameTime:g.read('bomb_flame_time'),enemies:g.read('bomb_enemies',2),dirs:g.read('bomb_dirs',2).map(x=>x>127?x-256:x),enemyClock:g.read('bomb_enemy_clock'),lives:g.read('bomb_lives'),running:g.read('bomb_running'),score:g.word('bomb_score'),keys:g.read('grid_stat'),phase:g.read('phase')});
function rays(board,p){const r=[p];for(const [d]of dirs)for(let n=1;n<=3;n++){const q=p+d*n;if(board[q]===1)break;r.push(q);if([2,4].includes(board[q]))break;}return r;}
function contact(s){
 for(let e=0;e<2;e++){const p=s.enemies[e];if(p===255)continue;if(s.flames[p]){s.enemies[e]=255;s.score+=100;}else if(p===s.p){die(s);return;}}
 if(s.flames[s.p]){die(s);return;}if(s.board[s.p]===5){s.board[s.p]=0;s.keys--;s.score+=50;}else if(s.board[s.p]===3&&!s.keys){s.score+=200;s.phase=4;}
}
function die(s){if(!--s.lives){s.phase=5;return;}s.p=16;s.cell=255;s.fuse=0;s.flameTime=0;s.enemyClock=0;s.running=0;s.flames.fill(0);}
function world(s){
 if(s.flameTime&&!--s.flameTime)s.flames.fill(0);
 if(s.fuse&&!--s.fuse){for(const p of rays(s.board,s.cell)){s.flames[p]=1;if([2,4].includes(s.board[p])){s.board[p]=s.board[p]===2?0:5;s.score+=30;}}s.cell=255;s.flameTime=4;}
 if(++s.enemyClock===4){s.enemyClock=0;for(let i=0;i<2;i++){const p=s.enemies[i];if(p===255)continue;if(s.flames[p]){s.enemies[i]=255;s.score+=100;continue;}const q=p+s.dirs[i];if(blocked(s.board[q])||q===s.cell)s.dirs[i]*=-1;else s.enemies[i]=q;}}
 contact(s);
}
function route(s,goal,crates=false){
 const q=[[0,s.p,[]]],cost=new Map([[s.p,0]]);
 while(q.length){q.sort((a,b)=>b[0]-a[0]);const [c,p,path]=q.pop();if(goal(p))return path;
  for(const [d,key]of dirs){const n=p+d;if(n<0||n>=105||s.board[n]===1||(!crates&&blocked(s.board[n]))||n===s.cell||s.flames[n])continue;
   const nc=c+1+(blocked(s.board[n])?15:0)+(s.enemies.some(e=>e!==255&&Math.floor(e/15)===Math.floor(n/15)&&Math.abs(e-n)<=2)?20:0);
   if(nc<(cost.get(n)??Infinity)){cost.set(n,nc);q.push([nc,n,[...path,key]]);}}
 }return null;
}
export async function checkBomb(g){
 const levels=JSON.parse(await readFile(new URL('../bomb-vault/levels.json',import.meta.url)));await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);
 // Compare every live key update and world tick against a separate board model.
 const frame=g.frame.bind(g);g.frame=()=>{const before=state(g),steps=g.read('bomb_steps'),panel=frame();if(before.phase===2&&g.read('phase')!==3){const s=structuredClone(before),event=g.read('input_event')||(g.read('phase')!==2?((g.keys&63)|((g.keys>>6)&15)):0);
  const d=event&1?-15:event&2?15:event&4?-1:event&8?1:0;
  if(d){const q=s.p+d;if(q>=0&&q<105&&!blocked(s.board[q])&&q!==s.cell){s.p=q;s.running=1;contact(s);}}
  if(s.phase===2&&(event&16)){s.running=1;if(s.cell===255&&!s.flameTime){s.cell=s.p;s.fuse=24;}}
  if(s.phase===2&&g.read('bomb_steps')!==steps)world(s);
  assert.deepEqual(state(g),s,'Bomb, flame, patrol, key and life state match independent simulation');
 }return panel;};
 for(let life=3;life>0;life--){g.tap('space');for(let n=0;g.read('phase')===2&&g.read('bomb_lives')===life;n++){assert.ok(n<300);g.frame();}}assert.equal(g.read('phase'),5);g.tap('space');
 let second=false,third=false,paused=false;
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('board',105),levels[stage]);let bombs=0;
  for(let action=0;g.read('phase')===2;action++){
   assert.ok(action<1600,`Bomb vault ${stage+1}`);let s=state(g);const targets=s.board.flatMap((v,p)=>[4,5].includes(v)?[p]:[]);const goal=targets.length?targets[0]:88;
   const path=route(s,p=>p===goal,true);assert.ok(path&&path.length,JSON.stringify({stage,goal,p:s.p}));const key=path[0],delta=dirs.find(d=>d[1]===key)[0],next=s.p+delta;
   const danger=s.enemies.some(e=>e!==255&&Math.floor(e/15)===Math.floor(next/15)&&Math.abs(e-next)<=2);
   if(blocked(s.board[next])||danger){
    const blast=new Set(rays(s.board,s.p));const refuge=route(s,p=>!blast.has(p)&&s.enemies.every(e=>e===255||Math.floor(e/15)!==Math.floor(p/15)),false);
    assert.ok(refuge&&refuge.length,`Retreat ${stage+1},${s.p}`);g.tap('space');bombs++;
    for(const k of refuge)g.tap(k);
    for(let n=0;g.read('bomb_fuse')||g.read('bomb_flame_time');n++){
     assert.ok(n<300);g.frame();
     if(stage===4&&!second&&g.read('bomb_flame_time')){await g.save('gameplay-2');second=true;}
     if(!paused&&g.read('bomb_fuse')===12){g.tap('return');const frozen=state(g);for(let j=0;j<30;j++)g.frame();g.tap('return');assert.deepEqual(state(g),{...frozen,phase:2});paused=true;}
    }
   }else g.tap(stage%2?({up:'letter-w',down:'letter-s',left:'letter-a',right:'letter-d'}[key]):key);
   if(stage===11&&!third&&g.read('grid_stat')===0&&g.read('phase')===2){await g.save('gameplay-3');third=true;}
   assert.notEqual(g.read('phase'),5,`Stage ${stage+1} failed after ${bombs} bombs`);
  }
  assert.equal(g.read('phase'),4);assert.equal(g.read('grid_stat'),0);console.log(JSON.stringify({stage:stage+1,bombs,lives:g.read('bomb_lives'),score:g.word('bomb_score')}));
 }
 assert.ok(second&&third&&paused);
}
