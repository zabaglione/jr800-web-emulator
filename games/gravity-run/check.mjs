// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const snapshot=g=>({distance:g.read('gravity_distance'),row:g.read('gravity_row'),direction:g.read('gravity_direction')===255?-1:1,running:g.read('gravity_running'),lives:g.read('gravity_lives'),checkpoint:g.read('gravity_checkpoint'),score:g.word('gravity_score'),stars:g.read('gravity_stars'),collected:g.read('gravity_collected',80),phase:g.read('phase')});
function world(s,course){
 s.distance++;s.row=Math.max(1,Math.min(5,s.row+s.direction));const col=s.distance+3,code=course[col];
 if(code&(1<<(s.row-1))){if(!--s.lives)s.phase=5;else{s.distance=s.checkpoint;s.row=5;s.direction=1;s.running=0;}return;}
 if(code>>5===s.row&&!s.collected[col]){s.collected[col]=1;s.stars++;s.score+=10;}
 if(s.distance>=32)s.checkpoint=32;if(s.distance===64){s.score+=100;s.phase=4;}
}
function board(s,c){return Array.from({length:112},(_,p)=>{const y=Math.floor(p/16),x=s.distance+p%16,v=c[x];return y===0||y===6?1:v&(1<<(y-1))?2:v>>5===y&&!s.collected[x]?3:0;});}
export async function checkGravity(g){
 const levels=JSON.parse(await readFile(new URL('./levels.json',import.meta.url))),solutions=JSON.parse(await readFile(new URL('./solutions.json',import.meta.url)));await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);
 const frame=g.frame.bind(g);let joined=0;g.frame=()=>{const s=snapshot(g),stage=g.read('stage'),steps=g.read('gravity_steps'),panel=frame();if(s.phase===2&&g.read('phase')!==3){const event=g.read('input_event')||(g.read('phase')!==2?((g.keys&63)|((g.keys>>6)&15)):0);if(event&1)s.direction=-1;else if(event&2)s.direction=1;if(event&16){if(s.running)s.direction*=-1;else s.running=1;}if(g.read('gravity_steps')!==steps)world(s,levels[stage]);if(s.phase===4){assert.equal(g.read('stage'),(stage+1)%12);assert.equal(g.read('clear_active'),0);Object.assign(s,{distance:0,checkpoint:0,running:1,collected:Array(80).fill(0),phase:2});joined++;}assert.deepEqual(snapshot(g),s,'Gravity, hazards, continuous stage joins and cumulative scoring match an independent model');assert.deepEqual(g.read('board',112),board(s,levels[g.read('stage')]),'The scrolling viewport is rebuilt from the current course columns');}return panel;};
 g.tap('up');assert.equal(g.read('gravity_direction'),255);g.tap('down');g.tap('letter-w');assert.equal(g.read('gravity_direction'),255);g.tap('letter-s');assert.equal(g.read('gravity_direction'),1);
 for(let life=3;life>0;life--){g.tap('space');for(let n=0;g.read('phase')===2&&g.read('gravity_lives')===life;n++){assert.ok(n<200);g.frame();}}assert.equal(g.read('phase'),5);g.tap('space');
 g.tap('space');g.hold('space',true);g.frame();for(let n=0;n<3;n++){g.frame();assert.equal(g.read('gravity_direction'),255,'A held action flips gravity only once');}g.hold('space',false);g.frame();g.menu(2);
 let second=false,third=false,paused=false;
 async function advance(flip){const step=g.read('gravity_steps'),direction=(g.read('gravity_direction')===255?-1:1)*(flip?-1:1);g.hold('up',direction===-1);g.hold('down',direction===1);for(let n=0;g.read('gravity_steps')===step&&g.read('phase')===2&&g.read('gravity_running');n++){assert.ok(n<100);g.frame();}}
 function route(course,row,direction){let states=[{row,direction,score:0,path:[]}];for(let distance=1;distance<=64;distance++){const next=new Map();for(const s of states)for(const flip of [0,1]){const d=flip?-s.direction:s.direction,y=Math.max(1,Math.min(5,s.row+d)),v=course[distance+3];if(v&(1<<(y-1)))continue;const t={row:y,direction:d,score:s.score+Number(v>>5===y),path:[...s.path,flip]},key=y+':'+d;if(!next.has(key)||next.get(key).score<t.score)next.set(key,t);}states=[...next.values()];assert.ok(states.length,'A route is reachable from the actual entry direction');}return states.sort((a,b)=>b.score-a.score)[0];}
 g.tap('space');
 for(let stage=0;stage<12;stage++){
  assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('gravity_course',80),levels[stage]);const initialStars=g.read('gravity_stars'),initialScore=g.word('gravity_score'),r=route(levels[stage],g.read('gravity_row'),g.read('gravity_direction')===255?-1:1);
  for(let distance=1;distance<=64;distance++){
   await advance(r.path[distance-1]);assert.equal(g.read('gravity_distance'),distance===64?0:distance);assert.equal(g.read('gravity_lives'),3);
   if(!paused&&distance===18){g.hold('up',false);g.hold('down',false);g.tap('return');const frozen=snapshot(g);for(let n=0;n<30;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}g.tap('return');assert.deepEqual(snapshot(g),{...frozen,phase:2});paused=true;}
   if(stage===4&&!second&&distance>15&&g.read('gravity_row')===3){await g.save('gameplay-2');second=true;}
   if(stage===11&&!third&&distance>42&&g.read('gravity_row')===3){await g.save('gameplay-3');third=true;}
  }
  assert.equal(g.read('phase'),2);assert.equal(g.read('gravity_running'),1);assert.equal(g.read('gravity_stars'),initialStars+r.score);assert.equal(g.word('gravity_score'),initialScore+100+r.score*10);assert.equal(g.read('gravity_banner'),1);
 }
 assert.equal(joined,12);assert.ok(second&&third&&paused);
 // Fall after the middle checkpoint and confirm that collected stars survive.
 g.hold('up',false);g.hold('down',false);g.menu(2);g.tap('space');for(let d=1;d<=33;d++)await advance(solutions[0].flips[d-1]);assert.equal(g.read('gravity_checkpoint'),32);const collected=g.read('gravity_collected',80),score=g.word('gravity_score');
 const c=levels[0];let q=[{row:g.read('gravity_row'),direction:g.read('gravity_direction')===255?-1:1,path:[]}],crash;
 for(let n=0;n<30&&!crash;n++){const next=[];for(const s of q)for(const flip of [0,1]){const d=flip?-s.direction:s.direction,row=Math.max(1,Math.min(5,s.row+d)),path=[...s.path,flip];if(c[37+n]&(1<<(row-1))){crash=path;break;}next.push({row,direction:d,path});}const seen=new Set();q=next.filter(s=>{const k=s.row+':'+s.direction;if(seen.has(k))return false;seen.add(k);return true;});}
 assert.ok(crash);for(const flip of crash)await advance(flip);assert.equal(g.read('gravity_lives'),2);assert.equal(g.read('gravity_distance'),32);assert.equal(g.read('gravity_running'),0);assert.ok(g.word('gravity_score')>=score);for(let p=0;p<80;p++)if(collected[p])assert.equal(g.read('gravity_collected',80)[p],1);
 g.hold('up',false);g.hold('down',false);g.frame();g.menu(2);assert.equal(g.read('gravity_distance'),0);assert.equal(g.read('gravity_stars'),0);
}
