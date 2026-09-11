// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const vectors=[[0,-1],[1,-1],[1,0],[1,1],[0,1],[-1,1],[-1,0],[-1,-1]],slash=[2,1,0,7,6,5,4,3],back=[6,5,4,3,2,1,0,7];
const state=g=>({board:g.read('board',48),p:g.read('cursor'),aim:g.read('rico_aim'),active:g.read('rico_active'),ammo:g.read('rico_ammo'),left:g.read('rico_left'),score:g.word('rico_score'),bullet:g.read('rico_bullet'),direction:g.read('rico_direction'),visited:g.read('rico_visited',384),trail:g.read('rico_trail',48),phase:g.read('phase')});
function step(s){
 const stop=()=>{s.active=0;s.bullet=255;if(s.phase!==4&&!s.ammo)s.phase=5;return s;};
 const [dx,dy]=vectors[s.direction],x=s.bullet%8+dx,y=Math.floor(s.bullet/8)+dy;if(x<0||x>=8||y<0||y>=6)return stop();const p=y*8+x,v=s.board[p];if(v===1||p===s.p||s.visited[p*8+s.direction])return stop();
 s.visited[p*8+s.direction]=1;s.trail[p]=1;s.bullet=p;
 if(v===4){s.board[p]=0;s.left--;s.score+=100;if(!s.left){s.phase=4;return stop();}}else if(v===2)s.direction=slash[s.direction];else if(v===3)s.direction=back[s.direction];return s;
}
export async function checkRico(g){
 const levels=JSON.parse(await readFile(new URL('../ricochet-ops/levels.json',import.meta.url))),solutions=JSON.parse(await readFile(new URL('../ricochet-ops/solutions.json',import.meta.url)));await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);
 function aim(col,a,letters=false){while(g.read('cursor')%8<col)g.tap(letters?'letter-d':'right');while(g.read('cursor')%8>col)g.tap(letters?'letter-a':'left');while(g.read('rico_aim')!==a)g.tap(letters?'letter-w':'up');}
 let second=false,third=false,paused=false,undo=false;
 async function flight(stage){for(let n=0;g.read('rico_active');n++){assert.ok(n<2500);const before=state(g),count=g.read('rico_steps');g.frame();const changed=g.read('rico_steps')!==count;assert.deepEqual(state(g),changed?step(before):before,'Every ricochet, target, boundary and repeated state matches an independent ray model');
  if(stage===8&&!second&&g.read('rico_trail',48).filter(Boolean).length>=5){await g.save('gameplay-2');second=true;}
  if(stage===18&&!third&&g.read('rico_left')===1&&g.read('rico_active')){await g.save('gameplay-3');third=true;}
  if(!paused&&n===4){g.tap('return');const frozen=state(g);for(let j=0;j<30;j++){g.frame();assert.equal(g.word('dirty_bytes'),0);}g.tap('return');assert.deepEqual(state(g),{...frozen,phase:2});paused=true;}
 }}
 aim(1,4);for(let n=0;n<3;n++){g.tap('space');await flight(-1);}assert.equal(g.read('phase'),5);g.tap('space');
 for(let stage=0;stage<20;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('board',48),levels[stage]);
  for(let shot=0;shot<solutions[stage].length;shot++){
   const {column,angle}=solutions[stage][shot];aim(column,angle,stage%2===1);
   if(!undo){const before=state(g);g.tap('space');assert.equal(g.read('rico_active'),1);g.menu(1);const after=state(g);assert.equal(after.active,0);assert.equal(after.ammo,before.ammo);assert.equal(after.score,before.score);assert.equal(after.left,before.left);assert.equal(after.p,before.p);assert.equal(after.aim,before.aim);assert.deepEqual(after.board,before.board);assert.equal(g.read('rico_prev_valid'),0);undo=true;}
   g.tap('space');await flight(stage);
  }
  assert.equal(g.read('phase'),4,`Ricochet ${stage+1}`);assert.equal(g.read('rico_left'),0);assert.equal(g.word('rico_score'),300);assert.equal(g.read('rico_ammo'),3-solutions[stage].length);
 }
 assert.ok(second&&third&&paused&&undo);
}
