// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
function next(seed){do{seed=(seed>>1)^((seed&1)?184:0);}while(seed>=253);return {seed,face:(seed-1)%6+1};}
function state(g){return {scores:g.read('luck_scores',2),pot:g.read('luck_pot'),face:g.read('luck_face'),side:g.read('luck_side'),rolls:g.read('luck_rolls'),trail:g.read('luck_trail',6),seed:g.read('seed'),phase:g.read('phase'),stage:g.read('stage')};}
function turn(s){s.side^=1;if(s.side){s.rolls=0;s.trail=Array(6).fill(0);}}
function throwDie(s){const r=next(s.seed);s.seed=r.seed;s.face=r.face;if(s.side)s.trail[s.rolls++]=r.face;if(r.face===1){s.pot=0;turn(s);}else s.pot=Math.min(255,s.pot+r.face);}
function bank(s){if(!s.pot)return;s.scores[s.side]=Math.min(255,s.scores[s.side]+s.pot);s.pot=0;if(s.scores[s.side]>=100)s.phase=s.side?5:4;else turn(s);}
function decide(s){return s.pot&&(s.scores[1]+s.pot>=100||s.rolls>=6||s.pot>=((s.stage===2&&s.scores[0]>=80)?32:[12,18,24][s.stage]));}
function inspect(g,s){assert.deepEqual(state(g),s);}
function human(g,action){const s=state(g);assert.equal(s.side,0);if(action==='roll'){throwDie(s);g.tap('space');}else{bank(s);g.menu(1);}inspect(g,s);}
async function cpu(g,capture=false){let shot=false;for(let frames=0;g.read('phase')===2&&g.read('luck_side');frames++){assert.ok(frames<500);const s=state(g),ready=g.read('luck_delay')===1;if(ready){if(decide(s))bank(s);else throwDie(s);}g.frame();inspect(g,s);if(!ready)assert.equal(g.word('dirty_bytes'),0,'CPU waiting has no display transfer');if(capture&&!shot&&s.rolls>=3&&s.side){await g.save('gameplay-2');shot=true;}}return shot;}
export async function checkLuck(g){await g.start();g.frame();assert.equal(g.word('dirty_bytes'),0);human(g,'bank');human(g,'roll');await g.save('gameplay-1');await cpu(g);
 // Deliberate losses: never bank, so only the CPU can reach the target.
 let paused=false;
 for(let turnCount=0;g.read('phase')===2;turnCount++){
  assert.ok(turnCount<400);if(g.read('luck_side')){
   if(!paused){g.tap('return');assert.equal(g.read('phase'),3);const frozen=state(g),delay=g.read('luck_delay');for(let i=0;i<40;i++)g.frame();assert.deepEqual(state(g),frozen);assert.equal(g.read('luck_delay'),delay);g.tap('return');paused=true;}
   await cpu(g);
  }else human(g,'roll');
 }assert.equal(g.read('phase'),5);g.tap('space');
 let second=false,third=false;
 for(let stage=0;stage<3;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);
  for(let turns=0;g.read('phase')===2;turns++){
   assert.ok(turns<600);if(g.read('luck_side'))second=(await cpu(g,!second))||second;
   else {const s=state(g);human(g,s.pot&&(s.scores[0]+s.pot>=100||next(s.seed).face===1)?'bank':'roll');if(stage===2&&!third&&g.read('luck_pot')>=20){await g.save('gameplay-3');third=true;}}
  }assert.equal(g.read('phase'),4,`Difficulty ${stage+1}`);
 }assert.ok(paused&&second&&third);
}
