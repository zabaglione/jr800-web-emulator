// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const keys=['up','down','left','right'],rows=[1,2,4,5],directions=[1,-1,-1,1],homes=[1,4,7,10,12];
function neighbor(p,d){const x=p%14+(d===2?-1:d===3?1:0),y=Math.floor(p/14)+(d===0?-1:d===1?1:0);return x>=0&&x<14&&y>=0&&y<7?y*14+x:255;}
function fresh(level){return {lanes:structuredClone(level.lanes),periods:[...level.periods],timers:[...level.periods],homes:Array(5).fill(0),p:90,running:1,lives:5,time:100,score:0,phase:2};}
function spawn(s){s.p=90;s.time=100;s.running=0;}
function die(s){s.effect=1;if(--s.lives)spawn(s);else{s.running=0;s.phase=5;}}
function check(s){const x=s.p%14,y=Math.floor(s.p/14);if(y===0){const i=homes.indexOf(x);if(i<0||s.homes[i])return die(s);s.homes[i]=1;s.effect=2;s.score=Math.min(65535,s.score+100+s.time);if(s.homes.every(Boolean)){s.phase=4;s.running=0;}else spawn(s);return;}
 const lane=rows.indexOf(y);if(lane>=0&&(lane<2?!s.lanes[lane][x]:s.lanes[lane][x]))die(s);
}
function world(s){if(!--s.time){die(s);return;}for(let i=0;i<4;i++){if(--s.timers[i])continue;s.timers[i]=s.periods[i];if(directions[i]===1)s.lanes[i].unshift(s.lanes[i].pop());else s.lanes[i].push(s.lanes[i].shift());if(i<2&&Math.floor(s.p/14)===rows[i]){const x=s.p%14+directions[i];if(x<0||x>=14){die(s);return;}s.p=rows[i]*14+x;}}check(s);}
function inspect(g,s){assert.deepEqual(g.read('river_lanes',56),s.lanes.flat());assert.deepEqual(g.read('river_timers',4),s.timers);assert.deepEqual(g.read('river_homes',5),s.homes);assert.equal(g.read('cursor'),s.p);assert.equal(g.read('river_running'),s.running);assert.equal(g.read('river_lives'),s.lives);assert.equal(g.read('river_time'),s.time);assert.equal(g.word('river_score'),s.score);assert.equal(g.read('phase'),s.phase);assert.equal(g.read('grid_stat'),s.homes.filter(Boolean).length);}
function frame(g,s,key){const before=g.read('river_steps'),starting=key==='space'&&!s.running;if(starting)s.running=1;
 if(s.running&&keys.includes(key)){const n=neighbor(s.p,keys.indexOf(key));if(n!==255){s.p=n;check(s);}}
 g.frame();const moved=g.read('river_steps')!==before;if(moved)world(s);
 if(s.effect){
  if(s.phase===4){const score=s.score,lives=s.lives;Object.assign(s,fresh(allLevels[g.read('stage')]),{score,lives});}
  else if(s.phase===2)s.running=1;
  delete s.effect;
 }
 inspect(g,s);assert.equal(g.word('river_display_score'),s.score,'The short score animation ends at the real score');return moved;
}
let allLevels;
function pulse(g,s,key){g.hold(key,true);frame(g,s,key);g.hold(key,false);frame(g,s);}
// Search in the moving traffic schedule. Each edge is one hop or wait followed by one world tick.
function plan(s){const todo=[{s:structuredClone(s),path:[]}],seen=new Set(),count=s.homes.filter(Boolean).length;
 for(let q=0;q<todo.length&&q<15000;q++){
  const v=todo[q];if(v.s.homes.filter(Boolean).length>count)return v.path;
  for(const d of [0,2,3,1,4]){const t=structuredClone(v.s);if(d<4){const n=neighbor(t.p,d);if(n===255)continue;t.p=n;check(t);}if(t.lives<s.lives)continue;if(t.running)world(t);if(t.lives<s.lives)continue;
   const id=[t.p,t.time,...t.timers].join(',');if(seen.has(id))continue;seen.add(id);todo.push({s:t,path:[...v.path,d]});
  }
 }throw new Error('No safe crossing in the lane schedule');
}
export async function checkRiver(g){allLevels=JSON.parse(await readFile(new URL('./levels.json',import.meta.url)));const levels=allLevels;await g.start();let s=fresh(levels[0]);await g.save('gameplay-1');inspect(g,s);
 // Timeout retries happen automatically; the fifth miss plays game over.
 while(s.phase===2)frame(g,s);assert.equal(s.phase,5);g.tap('space');s=fresh(levels[0]);let second=false,third=false,carried=false,paused=false,previousPeriod=255;
 for(let stage=0;stage<12;stage++){
  assert.equal(g.read('stage'),stage);inspect(g,s);assert.ok(g.read('river_tick_period')<previousPeriod||g.read('river_tick_period')===6,'Each successive stage tightens the traffic interval');previousPeriod=g.read('river_tick_period');
  for(let home=0;home<5;home++){
   const beforeScore=s.score,beforeLives=s.lives;
   for(let actions=0;g.read('stage')===stage&&s.homes.filter(Boolean).length===home;actions++){
    assert.ok(actions<100);const d=plan(s)[0];if(d<4)pulse(g,s,keys[d]);
    if(g.read('stage')!==stage||s.homes.filter(Boolean).length!==home)break;
    if(!paused){g.menu(1);s.running=0;inspect(g,s);for(let i=0;i<40;i++){g.frame();assert.equal(g.word('dirty_bytes'),0);}pulse(g,s,'space');paused=true;}
    for(let f=0;f<100;f++){const p=s.p;if(frame(g,s)){if(p!==s.p&&Math.floor(p/14)<3&&Math.floor(p/14)>0)carried=true;break;}assert.ok(f<99);}
    if(stage===4&&!second&&Math.floor(s.p/14)<3){await g.save('gameplay-2');second=true;}
   }
   assert.equal(s.lives,beforeLives,`Safe crossing ${stage+1}/${home+1}`);assert.ok(s.score>=beforeScore+100);assert.equal(s.running,1,'Arrival resumes without another SPACE');
   if(home<4)assert.equal(s.homes.filter(Boolean).length,home+1);
   if(stage===11&&home===3){await g.save('gameplay-3');third=true;}
  }
  assert.equal(s.phase,2);assert.equal(g.read('stage'),(stage+1)%12);assert.equal(g.read('clear_active'),0,'The next stage does not stop at the generic result dialog');console.log(JSON.stringify({completedStage:stage+1,score:s.score,lives:s.lives}));
 }
 assert.equal(g.read('river_cycle'),12,'After stage twelve the next circuit stays difficult');assert.equal(g.read('river_tick_period'),6);assert.ok(second&&third&&carried&&paused);
}
