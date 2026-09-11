// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const state=g=>({enemies:g.read('orbit_enemies',24),position:g.read('orbit_position'),left:g.read('orbit_left'),core:g.read('orbit_core'),pulse:g.read('orbit_pulse'),aim:g.read('orbit_aim'),running:g.read('orbit_running'),flash:g.read('orbit_flash'),score:g.word('orbit_score'),turn:g.read('orbit_turn'),phase:g.read('phase')});
function end(s){if(!s.core)s.phase=5;else if(!s.left)s.phase=4;}
function shoot(s){for(let i=s.aim;i<24;i+=8)if(s.enemies[i]){if(!--s.enemies[i])s.left--;s.score+=10;break;}end(s);}
function world(s,wave,stage){if(s.flash)s.flash--;s.turn^=1;if(s.turn)return;const leaks=s.enemies.slice(0,8).filter(Boolean).length;s.left-=leaks;s.core=Math.max(0,s.core-leaks);const n=Array(24).fill(0);for(let i=8;i<24;i++)n[Math.floor((i-8)/8)*8+(i%8+(stage>=8?1:0))%8]=s.enemies[i];s.enemies=n;end(s);if(s.phase!==2)return;if(s.position<wave.length){const c=wave[s.position++];s.enemies[16+(c&7)]=(c>>3)+1;}}
export async function checkOrbit(g){
 const levels=JSON.parse(await readFile(new URL('../orbit-guard/levels.json',import.meta.url)));await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);
 const frame=g.frame.bind(g);g.frame=()=>{const s=state(g),steps=g.read('orbit_steps'),weapon=g.read('orbit_weapon_clock'),panel=frame();if(s.phase===2&&g.read('phase')!==3){const event=g.read('input_event')||(g.read('phase')!==2?((g.keys&63)|((g.keys>>6)&15)):0);if(event&1)s.aim=0;else if(event&2)s.aim=4;else if(event&4)s.aim=(s.aim+7)%8;else if(event&8)s.aim=(s.aim+1)%8;if(event&16){s.running=1;if(g.read('orbit_weapon_clock')!==weapon){s.flash=2;shoot(s);}}if(s.phase===2&&g.read('orbit_steps')!==steps)world(s,levels[g.read('stage')],g.read('stage'));assert.deepEqual(state(g),s,'Orbital approach, rotation, nearest hits and core damage match an independent model');}return panel;};
 g.tap('space');for(let n=0;g.read('phase')===2;n++){assert.ok(n<1200);g.frame();}assert.equal(g.read('phase'),5);g.tap('space');
 let second=false,third=false,paused=false,pulsed=false;
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);g.tap('space');
  if([0,4,11].includes(stage)){
   while(g.read('orbit_position')<3)g.frame();
   if(stage===0){const s=state(g),hp=s.enemies.reduce((a,b)=>a+b,0),count=s.enemies.filter(Boolean).length;g.menu(1);assert.equal(g.read('orbit_pulse'),0);assert.equal(g.read('orbit_left'),s.left-count);assert.equal(g.word('orbit_score'),s.score+hp*10);const before=state(g);g.menu(1);assert.deepEqual(state(g),before,'The emergency pulse can be spent only once');pulsed=true;}
   if(stage===4){await g.save('gameplay-2');second=true;}
   if(stage===11){await g.save('gameplay-3');third=true;}
  }
  for(let action=0;g.read('phase')===2;action++){
   assert.ok(action<1800,`Orbit ${stage+1}`);const s=state(g),i=s.enemies.findIndex(Boolean);
   if(i<0)g.frame();else{const angle=i%8,d=(angle-s.aim+8)%8;if(d===0)g.tap('space');else if(angle===0)g.tap(stage%2?'letter-w':'up');else if(angle===4)g.tap(stage%2?'letter-s':'down');else g.tap(d<=4?(stage%2?'letter-d':'right'):(stage%2?'letter-a':'left'));}
   if(!paused&&action===15){g.tap('return');const frozen=state(g);for(let n=0;n<30;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}g.tap('return');assert.deepEqual(state(g),{...frozen,phase:2});paused=true;}
  }
  assert.equal(g.read('phase'),4);assert.equal(g.read('orbit_core'),5);assert.equal(g.word('orbit_score'),levels[stage].reduce((n,c)=>n+((c>>3)+1)*10,0));console.log(JSON.stringify({stage:stage+1,enemies:levels[stage].length,score:g.word('orbit_score')}));
 }
 assert.ok(second&&third&&paused&&pulsed);
}
