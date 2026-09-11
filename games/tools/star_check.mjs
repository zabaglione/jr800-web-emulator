// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const state=g=>({p:g.read('cursor'),enemies:g.read('star_enemies',20),shields:g.read('star_shields',16),left:g.read('star_left'),lives:g.read('star_lives'),running:g.read('star_running'),shot:g.read('star_shot'),bolt:g.read('star_bolt'),offset:g.read('star_offset'),base:g.read('star_base'),direction:g.read('star_direction')===255?-1:1,bounces:g.read('star_bounces'),march:g.read('star_march_clock'),fire:g.read('star_fire_clock'),score:g.word('star_score'),seed:g.read('seed'),stage:g.read('stage'),phase:g.read('phase')});
const cell=(s,i)=>(s.base+Math.floor(i/10))*16+s.offset+i%10;
function hit(s){if(s.shot===255)return;if(s.shot===s.bolt){s.shot=s.bolt=255;return;}const i=s.enemies.findIndex((hp,i)=>hp&&cell(s,i)===s.shot);if(i>=0){if(!--s.enemies[i])s.left--;s.score+=10;s.shot=255;if(!s.left)s.phase=4;return;}if(s.shot>=80&&s.shot<96&&s.shields[s.shot%16]){s.shields[s.shot%16]--;s.shot=255;}}
function contact(s){if(s.bolt===255)return;if(s.bolt===s.shot){s.shot=s.bolt=255;return;}if(s.bolt===s.p){if(!--s.lives)s.phase=5;else{s.p=104;s.shot=s.bolt=255;s.running=0;}return;}if(s.bolt>=80&&s.bolt<96&&s.shields[s.bolt%16]){s.shields[s.bolt%16]--;s.bolt=255;}}
function world(s){
 if(s.shot!==255){s.shot=s.shot>=16?s.shot-16:255;hit(s);}if(s.phase!==2)return;
 if(s.bolt!==255){s.bolt=s.bolt+16<112?s.bolt+16:255;contact(s);if(s.phase!==2||!s.running)return;}
 if(++s.march>=[8,6,4][Math.floor(s.stage/4)]){s.march=0;const x=s.offset+s.direction;if(x<0||x>6){s.direction*=-1;if(++s.bounces===2){s.bounces=0;if(++s.base===5){s.phase=5;return;}}}else s.offset=x;hit(s);if(s.phase!==2)return;}
 if(++s.fire===8){s.fire=0;if(s.bolt===255){s.seed=(s.seed>>1)^((s.seed&1)?0xb8:0);let rank=(s.seed-1)%s.left,i=0;for(;i<20;i++)if(s.enemies[i]&&rank--===0)break;if(i<10&&s.enemies[i+10])i+=10;s.bolt=cell(s,i)+16;contact(s);}}
}
function fire(s){s.running=1;if(s.shot===255){s.shot=s.p-16;hit(s);}}
function board(s){const b=Array(112).fill(0);s.shields.forEach((hp,i)=>{if(hp)b[80+i]=hp+2;});s.enemies.forEach((hp,i)=>{if(hp)b[cell(s,i)]=hp;});return b;}
export async function checkStar(g){
 const levels=JSON.parse(await readFile(new URL('../star-patrol/levels.json',import.meta.url)));await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);
 const frame=g.frame.bind(g);g.frame=()=>{const s=state(g),steps=g.read('star_steps'),panel=frame();if(s.phase===2&&g.read('phase')!==3){const event=g.read('input_event')||((g.read('phase')!==2||g.read('star_lives')!==s.lives)?((g.keys&63)|((g.keys>>6)&15)):0);const p=s.p;if(event&4)s.p=Math.max(96,s.p-1);else if(event&8)s.p=Math.min(111,s.p+1);if(s.p!==p)contact(s);if(s.phase===2&&(event&16))fire(s);if(s.phase===2&&g.read('star_steps')!==steps)world(s);assert.deepEqual(state(g),s,'Alien formation, shots, shields, enemy fire and score match independent simulation');if(s.base<5)assert.deepEqual(g.read('board',112),board(s),'Formation and shield background match current state');}return panel;};
 // Let the attack continue without firing to exercise an actual defeat and retry.
 g.tap('space');for(let n=0;g.read('phase')===2;n++){assert.ok(n<16000);if(!g.read('star_running'))g.tap('space');else g.frame();}assert.equal(g.read('phase'),5);g.tap('space');
 let second=false,third=false,paused=false;
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('star_enemies',20),levels[stage]);g.tap('space');
  for(let action=0;g.read('phase')===2;action++){
   assert.ok(action<7000,`Patrol ${stage+1}`);const s=state(g),col=s.p%16;
   if(!s.running){g.tap('space');continue;}
   let target=col;
   if(s.bolt!==255&&s.bolt%16===col&&s.bolt>=64&&!s.shields[col])target=col?col-1:1;
   else if(s.shot===255){let best=-Infinity;for(let x=0;x<16;x++){const n=structuredClone(s);n.p=96+x;fire(n);for(let t=0;t<7&&n.phase===2&&n.shot!==255;t++)world(n);const score=(n.score-s.score)*20-Math.abs(col-x)*2-(n.lives<s.lives?1000:0)-(s.shields[x]?25:0);if(score>best){best=score;target=x;}}}
   if(target<col)g.tap(stage%2?'letter-a':'left');else if(target>col)g.tap(stage%2?'letter-d':'right');else if(s.shot===255)g.tap('space');else g.frame();
   if(!paused&&action===50){g.tap('return');const frozen=state(g);for(let n=0;n<30;n++)g.frame();g.tap('return');assert.deepEqual(state(g),{...frozen,phase:2});paused=true;}
   if(stage===4&&!second&&g.read('star_left')<=12){await g.save('gameplay-2');second=true;}
   if(stage===11&&!third&&g.read('star_left')<=7){await g.save('gameplay-3');third=true;}
  }
  assert.equal(g.read('phase'),4,`Patrol ${stage+1}`);assert.equal(g.read('star_left'),0);assert.equal(g.word('star_score'),levels[stage].reduce((a,b)=>a+b,0)*10);console.log(JSON.stringify({stage:stage+1,lives:g.read('star_lives'),score:g.word('star_score')}));
 }
 assert.ok(second&&third&&paused);
}
