// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {letters} from './harness.mjs';
const duration=[0,3,5],cost=[0,2,3],price=[0,7,12];
const state=g=>({crops:g.read('orchard_crops',18),growth:g.read('orchard_growth',18),wet:g.read('orchard_wet',18),dry:g.read('orchard_dry',18),p:g.read('cursor'),day:g.read('orchard_day'),limit:g.read('orchard_limit'),goal:g.read('orchard_goal'),coins:g.word('orchard_coins'),actions:g.read('orchard_actions'),seed:g.read('orchard_seed'),rain:g.read('orchard_rain'),phase:g.read('phase')});
function action(s,key,level){if(key==='swap'){s.seed=3-s.seed;return s;}if(key==='next'){if(s.day+1>=s.limit){s.phase=5;return s;}for(let p=0;p<18;p++){const c=s.crops[p];if(!c)continue;if(s.growth[p]<duration[c]){if(s.wet[p]||s.rain){s.growth[p]++;s.dry[p]=0;}else{if(++s.dry[p]===2){s.crops[p]=0;s.growth[p]=0;s.dry[p]=0;}}}s.wet[p]=0;}s.day++;s.rain=level.rain[s.day];s.actions=3;return s;}if(!s.actions)return s;const p=s.p,c=s.crops[p];if(!c){if(s.coins<cost[s.seed])return s;s.coins-=cost[s.seed];s.crops[p]=s.seed;s.growth[p]=s.dry[p]=0;s.wet[p]=1;}else if(s.growth[p]===duration[c]){s.coins+=price[c];s.crops[p]=s.growth[p]=s.wet[p]=s.dry[p]=0;if(s.coins>=s.goal)s.phase=4;}else{if(s.wet[p])return s;s.wet[p]=1;s.dry[p]=0;}s.actions--;return s;}
export async function checkOrchard(g){
 const levels=JSON.parse(await readFile(new URL('../orchard-days/levels.json',import.meta.url))),solutions=JSON.parse(await readFile(new URL('../orchard-days/solutions.json',import.meta.url)));await g.start();await g.save('gameplay-1');let second=false,third=false,beans=false,berries=false;
 function key(k){const expected=action(state(g),k,levels[g.read('stage')]);if(k==='next')g.menu(1);else if(k==='swap')g.menu(2);else g.tap('space');assert.deepEqual(state(g),expected,'Crop stages, watering, dry days, rain, money and action limits follow independent rules');}
 function move(p,letter=false){for(const [key,predicate] of [['right',()=>g.read('cursor')%6<p%6],['left',()=>g.read('cursor')%6>p%6],['down',()=>Math.floor(g.read('cursor')/6)<Math.floor(p/6)],['up',()=>Math.floor(g.read('cursor')/6)>Math.floor(p/6)]])while(predicate())g.tap(letter?letters[key]:key);}
 const frozen=state(g);for(let i=0;i<25;i++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),frozen);key('space');key('space');assert.equal(g.read('orchard_actions'),2,'Watering an already watered seed uses no extra action');for(let i=0;i<5;i++)key('next');assert.equal(g.read('orchard_crops',18)[0],0,'Two dry days after the rainy recovery wither the crop');while(g.read('phase')===2)key('next');assert.equal(g.read('phase'),5);g.tap('space');
 key('swap');for(const p of [0,1,2]){move(p);key('space');}assert.equal(g.word('orchard_coins'),1);move(3);key('space');assert.equal(g.read('orchard_actions'),0);key('next');key('space');assert.equal(g.read('orchard_actions'),3,'Insufficient money never spends the daily work budget');g.menu(3);
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.equal(g.read('orchard_limit'),levels[stage].days);assert.equal(g.read('orchard_goal'),levels[stage].goal);
  for(const item of solutions[stage]){if(item==='next')key('next');else{move(item.cell,stage%2===1);if(item.seed){beans||=item.seed===1;berries||=item.seed===2;if(g.read('orchard_seed')!==item.seed)key('swap');}key('space');}
   if(stage===4&&g.read('orchard_day')===3&&g.read('orchard_crops',18).filter(Boolean).length>=3&&!second){await g.save('gameplay-2');second=true;}
   if(stage===10&&g.read('orchard_day')>=7&&g.read('phase')===2&&!third){await g.save('gameplay-3');third=true;}
   if(!g.read('orchard_actions')&&g.read('phase')===2)key('space');
  }
  assert.equal(g.read('phase'),4,`Farm goal ${stage+1}`);assert.ok(g.word('orchard_coins')>=levels[stage].goal);
 }
 assert.ok(second&&third&&beans&&berries);
}
