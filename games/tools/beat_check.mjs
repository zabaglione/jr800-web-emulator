// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const masks=[4,2,1,8],keys=['left','down','up','right'],letters=['letter-a','letter-s','letter-w','letter-d'];
const state=g=>({time:g.word('beat_time'),due:g.word('beat_due'),score:g.word('beat_score'),life:g.read('beat_life'),combo:g.read('beat_combo'),index:g.read('beat_index'),active:g.read('beat_active'),previous:g.read('beat_previous'),pressed:g.read('beat_pressed'),judgement:g.read('beat_judgement'),phase:g.read('phase')});
export async function checkBeat(g){
 const charts=JSON.parse(await readFile(new URL('../beat-step/charts.json',import.meta.url)));await g.start();let judged=0;
 const frame=g.frame.bind(g);g.frame=()=>{const b=state(g),resume=g.read('resume_pending'),held=(g.keys&1||g.keys&64?1:0)|(g.keys&2||g.keys&128?2:0)|(g.keys&4||g.keys&256?4:0)|(g.keys&8||g.keys&512?8:0);const panel=frame(),after=state(g);if(b.phase===2&&b.active&&after.phase!==3){const s={...b,time:after.time},chart=charts[g.read('stage')];assert.ok(s.time>=b.time&&s.time-b.time<24,'Chart time advances in bounded emulated ticks');s.pressed=held&~(resume?held:b.previous)&15;s.previous=held;let expired=false;const damage=()=>{s.combo=0;s.judgement=3;if(!--s.life){s.active=0;s.phase=5;}};const advance=()=>{s.index++;s.due+=chart.interval;if(s.active&&s.index>=chart.notes.length){s.active=0;s.phase=4;}};while(s.active&&s.time-s.due>chart.window){damage();advance();expired=true;}if(s.active&&!expired&&s.pressed){const d=Math.abs(s.time-s.due);if(d>chart.window||s.pressed!==masks[chart.notes[s.index]])damage();else{s.score+=d<=2?100:60;s.judgement=d<=2?1:2;s.combo++;advance();judged++;}}assert.deepEqual(after,s,'Expiry, edge-only direction judgement, score, combo and lives match the independent chart model');}return panel;};
 const idle=state(g);for(let n=0;n<20;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),idle);g.tap('space');g.tap('right');assert.equal(g.read('beat_life'),5,'An early wrong direction costs one life');g.tap('return');const paused=state(g);for(let n=0;n<30;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),paused);g.tap('return');g.menu(2);
 g.tap('space');while(g.word('beat_time')<g.word('beat_due')-1)g.frame();g.hold('left',true);g.frame();const firstScore=g.word('beat_score');assert.ok(firstScore>0);while(g.read('beat_index')<3)g.frame();assert.equal(g.word('beat_score'),firstScore,'Holding a direction cannot hit later notes through SDK autorepeat');g.hold('left',false);g.frame();g.menu(2);
 g.tap('space');let count=0;while(g.read('phase')===2){g.frame();assert.ok(++count<1200);}assert.equal(g.read('phase'),5);assert.equal(g.read('beat_life'),0);g.tap('space');
 for(let stage=0;stage<3;stage++){
  if(stage)g.tap('space');const chart=charts[stage];assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('beat_notes',chart.notes.length),chart.notes);if(stage===1){g.menu(1);assert.equal(g.read('beat_mute'),1);}g.tap('space');
  for(let note=0;note<chart.notes.length;note++){
   const desired=g.word('beat_due')-3;let n=0;while(g.word('beat_time')<desired){g.frame();assert.ok(++n<100);}if(stage===0&&note===8)await g.save('gameplay-1');if(stage===1&&note===16)await g.save('gameplay-2');if(stage===2&&note===40)await g.save('gameplay-3');assert.equal(g.read('beat_index'),note);const beforeHit=state(g);g.tap((stage%2?letters:keys)[chart.notes[note]]);assert.equal(g.read('beat_life'),6,JSON.stringify({stage,note,beforeHit,after:state(g),cycles:g.cycles.slice(-5)}));assert.equal(g.read('beat_index'),note+1,'A released physical direction consumes exactly one note');
  }
  assert.equal(g.read('phase'),4,`Rhythm chart ${stage+1}`);assert.equal(g.read('beat_life'),6);assert.equal(g.read('beat_combo'),chart.notes.length);assert.ok(g.word('beat_score')>=chart.notes.length*60);
 }
 assert.ok(judged>=145);
}
