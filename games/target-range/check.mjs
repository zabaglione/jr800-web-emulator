// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const limits=[144,112,88],goals=[180,220,250];
const state=g=>({round:g.read('range_round'),mode:g.read('range_mode'),remaining:g.read('range_remaining'),target:g.read('range_target'),decoy:g.read('range_decoy'),decoy2:g.read('range_decoy2'),score:g.word('range_score'),misses:g.read('range_misses'),result:g.read('range_result'),p:g.read('cursor'),board:g.read('board',9),phase:g.read('phase')});
function begin(s,levels,stage){s.board.fill(0);[s.target,s.decoy,s.decoy2]=levels[stage][s.round];if(s.target!==255)s.board[s.target]=1;s.board[s.decoy]=2;if(s.decoy2!==255)s.board[s.decoy2]=2;s.remaining=limits[stage];s.mode=1;s.result=0;}
function feedback(s,result){s.result=result;s.mode=2;s.remaining=12;if(s.misses>=3)s.phase=5;}
function expire(s){if(s.target===255){s.score+=10;feedback(s,2);}else{s.misses++;feedback(s,3);}}
function move(g,p){while(g.read('cursor')%3<p%3)g.tap('right');while(g.read('cursor')%3>p%3)g.tap('left');while(Math.floor(g.read('cursor')/3)<Math.floor(p/3))g.tap('down');while(Math.floor(g.read('cursor')/3)>Math.floor(p/3))g.tap('up');}
export async function checkRange(g){
 const levels=JSON.parse(await readFile(new URL('./levels.json',import.meta.url)));await g.start();g.frame();assert.equal(g.word('dirty_bytes'),0);
 const frame=g.frame.bind(g);g.frame=()=>{const s=state(g),clock=g.read('range_clock'),pending=g.read('range_trial_pending')||g.read('resume_pending'),panel=frame();if(s.phase===2&&g.read('phase')!==3){const stage=g.read('stage'),event=g.read('input_event')||((g.read('phase')!==2||g.read('range_trial_pending'))?((g.keys&63)|((g.keys>>6)&15)):0),elapsed=pending?0:(g.read('range_clock')-clock+256)%256;
  if(!s.mode){if(event&16)begin(s,levels,stage);}
  else if(s.remaining<=elapsed){if(s.mode===1)expire(s);else if(++s.round===20)s.phase=s.score>=goals[stage]?4:5;else begin(s,levels,stage);}
  else{s.remaining-=elapsed;if(s.mode===1){let p=s.p;if(event&1&&p>=3)p-=3;else if(event&2&&p<6)p+=3;else if(event&4&&p%3)p--;else if(event&8&&p%3<2)p++;s.p=p;if(event&16){if(p===s.target){s.score+=10+Math.floor(s.remaining/8);s.board[p]=3;feedback(s,1);}else{s.misses++;s.board[p]=4;feedback(s,4);}}}}
  assert.deepEqual(state(g),s,'Target presentation, deadlines, decisions and reaction bonuses match an independent model');
 }return panel;};
 const next=()=>{for(let n=0;g.read('phase')===2&&g.read('range_mode')===2;n++){assert.ok(n<100);g.frame();}};
 g.tap('space');for(let n=0;n<3;n++){if(n)next();const wrong=(g.read('range_target')+1)%9;move(g,wrong);g.tap('space');}assert.equal(g.read('phase'),5);g.tap('space');
 let second=false,third=false,paused=false;
 for(let stage=0;stage<3;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);g.tap('space');if(!stage)await g.save('gameplay-1');
  for(let round=0;round<20;round++){
   assert.equal(g.read('range_round'),round);const target=g.read('range_target');
   if(target===255){while(g.read('range_mode')===1)g.frame();assert.equal(g.read('range_result'),2);}
   else{move(g,target);if(stage===1&&!second&&round===7){await g.save('gameplay-2');second=true;}if(!paused&&round===2){g.tap('return');const frozen=state(g);for(let n=0;n<30;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}g.tap('return');assert.deepEqual(state(g),{...frozen,phase:2});paused=true;}g.tap('space');assert.equal(g.read('range_result'),1);}
   if(stage===2&&!third&&round===14){await g.save('gameplay-3');third=true;}next();
  }
  assert.equal(g.read('phase'),4);assert.equal(g.read('range_misses'),0);assert.ok(g.word('range_score')>=goals[stage]);console.log(JSON.stringify({difficulty:stage+1,score:g.word('range_score')}));
 }
 assert.ok(second&&third&&paused);
 // Correct but deliberately slow shots can fail the final score requirement.
 g.tap('return');g.tap('space');g.tap('space');
 for(let round=0;round<20;round++){const target=g.read('range_target');if(target===255){while(g.read('range_mode')===1)g.frame();}else{move(g,target);while(g.read('range_remaining')>20)g.frame();g.tap('space');}next();}
 assert.equal(g.read('phase'),5);assert.equal(g.read('range_misses'),0);assert.ok(g.word('range_score')<250);g.tap('space');assert.equal(g.word('range_score'),0);
}
