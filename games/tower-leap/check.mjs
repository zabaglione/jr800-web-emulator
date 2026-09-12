// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {playPixelVisible} from '../tools/harness.mjs';
import {readFile} from 'node:fs/promises';
const fields=['x','height','velocity','grounded','camera','lives','highest','checkpoint'];
const state=g=>Object.fromEntries(fields.map(k=>[k,g.read('tower_'+k)]));
function model(s,xs,held,e,origins){
 s={...s};if(held&4)s.x=Math.max(0,s.x-2);else if(held&8)s.x=Math.min(124,s.x+2);
 const v=s.velocity>127?s.velocity-256:s.velocity,next=s.height+v;
 const die=()=>{s.lives--;if(e&&s.lives){e.xs=[...origins];e.crumble=Array(12).fill(0);e.enemyX=32;e.enemyDir=1;}if(s.lives){s.height=s.checkpoint*16;s.x=xs[s.checkpoint]*8+10;s.camera=Math.max(0,s.height-16);s.velocity=0;s.grounded=1;}return s;};
 if(next<0)return die();
 if(v<=0){for(let i=11;i>=0;i--)if(i*16<=s.height&&i*16>=next&&s.x+3>=xs[i]*8&&s.x<xs[i]*8+24){s.height=i*16;s.velocity=0;s.grounded=1;if(i>s.highest){s.highest=i;s.checkpoint=i&~3;}return s;}}
 s.height=next;s.grounded=0;if(s.height<s.camera)return die();s.velocity=(v-1)&255;if(s.height-s.camera>40)s.camera=(s.height-32)&240;return s;
}
let kinds,art,origins,crumbleLimit;
const environment=g=>({ticks:g.read('tower_environment_ticks'),motion:g.read('tower_motion_phase'),xs:g.read('tower_platforms',12),crumble:g.read('tower_crumble',12),enemyX:g.read('tower_enemy_x'),enemyDir:g.read('tower_enemy_dir')===255?-1:1,enemyHeight:g.read('tower_enemy_height')});
function tick(s,e,held){
 e=structuredClone(e);s={...s};if(++e.ticks===12){e.ticks=0;e.motion=(e.motion+1)%4;}
 for(let i=0;i<12;i++){
  if(kinds[i]===1&&!e.ticks){const next=origins[i]+[0,1,0,-1][e.motion];if(s.grounded&&s.height===i*16)s.x+=(next-e.xs[i])*8;e.xs[i]=next;}
  if(kinds[i]===2&&s.grounded&&s.height===i*16&&++e.crumble[i]>=crumbleLimit)e.xs[i]=255;
 }
 e.enemyX+=e.enemyDir;if([32,96].includes(e.enemyX))e.enemyDir=-e.enemyDir;
 s=model(s,e.xs,held,e,origins);
 if(s.highest!==11&&s.height+5>=e.enemyHeight&&s.height<e.enemyHeight+4&&s.x+3>=e.enemyX&&s.x<e.enemyX+5){
  if(--s.lives){e.xs=[...origins];e.crumble=Array(12).fill(0);e.enemyX=32;e.enemyDir=1;s.height=s.checkpoint*16;s.x=origins[s.checkpoint]*8+10;s.camera=Math.max(0,s.height-16);s.velocity=0;s.grounded=1;}
 }
 return {s,e};
}
function pixels(g){
 const s=state(g),e=environment(g),fb=g.machine.memory(g.symbols.framebuffer,1536),hero=[6,6,15,6,9,9],drone=[14,27,21,27];
 for(let y=8;y<64;y++)for(let x=0;x<128;x++){
  if(!playPixelVisible(g,x,y))continue;
  const row=(y>>3)-1,col=x>>3,h=s.camera+(6-row)*8,i=h/16;let tile=0;
  if(Number.isInteger(i)&&i<12&&e.xs[i]!==255&&col>=e.xs[i]&&col<e.xs[i]+3)tile=i%4===0?2:kinds[i]===1?4:kinds[i]===2?(e.crumble[i]>=4?6:5):1;
  else if(col===0||col===15)tile=3;
  let on=!!(art[tile*8+x%8]>>(y%8)&1);
  const a=x-s.x,b=y-(58-s.height+s.camera);if(a>=0&&a<4&&b>=0&&b<6)on ||=!!(hero[b]&(1<<a));
  const dx=x-e.enemyX,dy=y-(60-e.enemyHeight+s.camera);if(dx>=0&&dx<5&&dy>=0&&dy<4)on ||=!!(drone[dy]&(1<<dx));
  assert.equal((fb[(y>>3)*192+x+g.read('view_origin')]>>(y&7))&1,on?1:0,`Tower pixels ${x},${y}`);
 }
}
function jumpPlan(s,e,next){
 for(const offset of [10,2,18]){let v={s:{...s,grounded:0,velocity:7},e:structuredClone(e)};
  for(let n=0;n<25;n++){const target=v.e.xs[next]*8+offset,held=v.s.x>target+1?4:v.s.x<target-1?8:0;v=tick(v.s,v.e,held);if(v.s.lives<s.lives)break;if(v.s.grounded){if(v.s.height>=next*16)return offset;break;}}
 }
 return null;
}
export async function checkTower(g){
 const levels=JSON.parse(await readFile(new URL('./levels.json',import.meta.url))),gimmicks=JSON.parse(await readFile(new URL('./gimmicks.json',import.meta.url))),assets=await readFile(new URL('./assets.s',import.meta.url),'utf8');art=[...assets.match(/^tiles:\n((?:\s*\.byte[^\n]*\n)+)/m)[1].matchAll(/\$([0-9A-F]{2})/g)].map(m=>parseInt(m[1],16));kinds=gimmicks.kinds[0];origins=levels[0];crumbleLimit=gimmicks.crumbleTicks[0];await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);pixels(g);
 for(let n=0;n<3;n++)g.menu(1);assert.equal(g.read('phase'),5);g.tap('space');assert.equal(g.read('tower_lives'),3);
 let snap2=false,snap3=false,checkpointTest=false,paused=false,airHeld=false,movingSeen=false,crumbled=false;
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');kinds=gimmicks.kinds[stage];origins=levels[stage];crumbleLimit=gimmicks.crumbleTicks[stage];assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('tower_platforms',12),levels[stage]);let next=1,offset=10,waitCollapse=false;
  for(let frame=0;g.read('phase')===2;frame++){
   assert.ok(frame<4000,`Tower ${stage+1}: ${JSON.stringify({state:state(g),environment:environment(g)})}`);const s=state(g),e=environment(g);
   if(s.grounded){next=Math.min(11,Math.floor(s.height/16)+1);offset=jumpPlan(s,e,next);}
   if(stage===0&&!crumbled&&s.grounded&&s.height===48)waitCollapse=true;
   const target=e.xs[next]*8+(offset??10),left=s.x>target+1,right=s.x<target-1,keyLeft=stage%2?'letter-a':'left',keyRight=stage%2?'letter-d':'right';
   g.hold(keyLeft,left&&!s.grounded);g.hold(keyRight,right&&!s.grounded);g.hold('space',!!s.grounded&&offset!==null&&!waitCollapse&&!(g.read('input_held')&16));
   const steps=g.read('tower_steps');g.frame();const expected={...s};if(s.grounded&&(g.read('input_event')&16)){expected.grounded=0;expected.velocity=7;}
   if(g.read('tower_steps')!==steps){const predicted=tick(expected,e,g.read('input_held'));assert.deepEqual(state(g),predicted.s,'Jump, moving platform carry, crumble, drone contact and camera match an independent model');assert.deepEqual(environment(g),predicted.e,'Environment advances on the same simulation tick');}
   else assert.deepEqual(state(g),expected,'Only a grounded jump edge may change motion between simulation ticks');
   movingSeen ||= g.read('tower_platforms',12).some((x,i)=>kinds[i]===1&&x!==origins[i]);
   if(waitCollapse&&g.read('tower_platforms',12)[3]===255){assert.ok(g.read('tower_crumble',12)[3]>=crumbleLimit);await g.save('crumbling-floor');g.hold(keyLeft,false);g.hold(keyRight,false);g.hold('space',false);g.menu(1);waitCollapse=false;crumbled=true;}
   if(frame%17===0)pixels(g);
   if(stage===0&&!paused&&g.read('tower_height')>24){g.hold(keyLeft,false);g.hold(keyRight,false);g.hold('space',false);g.frame();g.tap('return');const frozen=state(g);for(let n=0;n<30;n++)g.frame();g.tap('return');assert.deepEqual(state(g),frozen);paused=true;}
   if(stage===0&&!checkpointTest&&g.read('tower_highest')===4&&g.read('tower_grounded')){g.hold(keyLeft,false);g.hold(keyRight,false);g.hold('space',false);g.frame();g.menu(1);assert.equal(g.read('tower_height'),64);assert.ok(g.read('tower_lives')>=1);checkpointTest=true;}
   if(stage===4&&!snap2&&g.read('tower_highest')===4&&g.read('tower_height')>75){await g.save('gameplay-2');snap2=true;}
   if(stage===11&&!snap3&&g.read('tower_highest')===9&&g.read('tower_height')>154){await g.save('gameplay-3');snap3=true;}
   if(!s.grounded&&(g.read('input_event')&16))airHeld=true;
  }
  for(const k of ['left','right','letter-a','letter-d','space'])g.hold(k,false);g.frame();assert.equal(g.read('phase'),4);assert.equal(g.read('tower_highest'),11);
 }
 assert.ok(snap2&&snap3&&checkpointTest&&paused&&movingSeen&&crumbled);void airHeld;
 // Walking off the first platform causes a real fall and returns to the checkpoint.
 g.tap('space');g.hold('left',true);for(let n=0;n<250&&g.read('tower_lives')===3;n++)g.frame();g.hold('left',false);g.frame();assert.equal(g.read('tower_lives'),2);assert.equal(g.read('tower_height'),0);
}
