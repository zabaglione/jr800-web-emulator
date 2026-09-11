// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const fields=['x','height','velocity','grounded','camera','lives','highest','checkpoint'];
const state=g=>Object.fromEntries(fields.map(k=>[k,g.read('tower_'+k)]));
function model(s,xs,held){
 s={...s};if(held&4)s.x=Math.max(0,s.x-2);else if(held&8)s.x=Math.min(124,s.x+2);
 const v=s.velocity>127?s.velocity-256:s.velocity,next=s.height+v;
 const die=()=>{s.lives--;if(s.lives){s.height=s.checkpoint*16;s.x=xs[s.checkpoint]*8+10;s.camera=Math.max(0,s.height-16);s.velocity=0;s.grounded=1;}return s;};
 if(next<0)return die();
 if(v<=0){for(let i=11;i>=0;i--)if(i*16<=s.height&&i*16>=next&&s.x+3>=xs[i]*8&&s.x<xs[i]*8+24){s.height=i*16;s.velocity=0;s.grounded=1;if(i>s.highest){s.highest=i;s.checkpoint=i&~3;}return s;}}
 s.height=next;s.grounded=0;if(s.height<s.camera)return die();s.velocity=(v-1)&255;if(s.height-s.camera>40)s.camera=(s.height-32)&240;return s;
}
function pixels(g){
 const s=state(g),xs=g.read('tower_platforms',12),fb=g.machine.memory(g.symbols.framebuffer,1536),hero=[6,6,15,6,9,9];
 for(let y=8;y<64;y++)for(let x=0;x<128;x++){
  const row=(y>>3)-1,col=x>>3,h=s.camera+(6-row)*8,i=h/16;let on=false;
  if(Number.isInteger(i)&&i<12&&col>=xs[i]&&col<xs[i]+3)on=(y&7)===7||((i%4===0)?(y&7)===6:((y&7)===6&&(x%8===0||x%8===7)));
  else if(col===0||col===15)on=y%8===3||y%8===7||(x%8===3&&y%8>=3);
  const a=x-s.x,b=y-(58-s.height+s.camera);if(a>=0&&a<4&&b>=0&&b<6)on ||=!!(hero[b]&(1<<a));
  assert.equal((fb[(y>>3)*192+x]>>(y&7))&1,on?1:0,`Tower pixels ${x},${y}`);
 }
}
export async function checkTower(g){
 const levels=JSON.parse(await readFile(new URL('../tower-leap/levels.json',import.meta.url)));await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);pixels(g);
 for(let n=0;n<3;n++)g.menu(1);assert.equal(g.read('phase'),5);g.tap('space');assert.equal(g.read('tower_lives'),3);
 let snap2=false,snap3=false,checkpointTest=false,paused=false,airHeld=false;
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('tower_platforms',12),levels[stage]);
  for(let frame=0;g.read('phase')===2;frame++){
   assert.ok(frame<2000,`Tower ${stage+1}`);const s=state(g),next=Math.min(11,s.highest+1),target=levels[stage][next]*8+10;
   const left=s.x>target+1,right=s.x<target-1,keyLeft=stage%2?'letter-a':'left',keyRight=stage%2?'letter-d':'right';
   g.hold(keyLeft,left&&!s.grounded);g.hold(keyRight,right&&!s.grounded);g.hold('space',!!s.grounded);
   const steps=g.read('tower_steps');g.frame();const expected={...s};if(s.grounded&&(g.read('input_event')&16)){expected.grounded=0;expected.velocity=7;}
   if(g.read('tower_steps')!==steps){assert.deepEqual(state(g),model(expected,levels[stage],g.read('input_held')),'Every jump, landing and camera shift matches an independent integer model');}
   else assert.deepEqual(state(g),expected,'Only a grounded jump edge may change motion between simulation ticks');
   if(frame%17===0)pixels(g);
   if(stage===0&&!paused&&g.read('tower_height')>24){g.hold(keyLeft,false);g.hold(keyRight,false);g.hold('space',false);g.frame();g.tap('return');const frozen=state(g);for(let n=0;n<30;n++)g.frame();g.tap('return');assert.deepEqual(state(g),frozen);paused=true;}
   if(stage===0&&!checkpointTest&&g.read('tower_highest')===4&&g.read('tower_grounded')){g.hold(keyLeft,false);g.hold(keyRight,false);g.hold('space',false);g.frame();g.menu(1);assert.equal(g.read('tower_height'),64);assert.equal(g.read('tower_lives'),2);checkpointTest=true;}
   if(stage===4&&!snap2&&g.read('tower_highest')===4&&g.read('tower_height')>75){await g.save('gameplay-2');snap2=true;}
   if(stage===11&&!snap3&&g.read('tower_highest')===9&&g.read('tower_height')>154){await g.save('gameplay-3');snap3=true;}
   if(!s.grounded&&(g.read('input_event')&16))airHeld=true;
  }
  for(const k of ['left','right','letter-a','letter-d','space'])g.hold(k,false);g.frame();assert.equal(g.read('phase'),4);assert.equal(g.read('tower_highest'),11);
 }
 assert.ok(snap2&&snap3&&checkpointTest&&paused);void airHeld;
 // Walking off the first platform causes a real fall and returns to the checkpoint.
 g.tap('space');g.hold('left',true);for(let n=0;n<250&&g.read('tower_lives')===3;n++)g.frame();g.hold('left',false);g.frame();assert.equal(g.read('tower_lives'),2);assert.equal(g.read('tower_height'),0);
}
