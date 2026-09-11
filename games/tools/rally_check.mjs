// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
const signed=n=>n>=128?n-256:n;
const state=g=>({x:g.read('rally_x'),y:g.read('rally_y'),dx:signed(g.read('rally_dx')),dy:signed(g.read('rally_dy')),player:g.read('rally_player'),cpu:g.read('rally_cpu'),you:g.read('rally_you'),them:g.read('rally_them'),active:g.read('rally_active'),count:g.read('rally_count'),clock:g.read('rally_ai_clock'),phase:g.read('phase')});
const deflect=r=>r<3?-3:r<6?-1:r<10?1:3;
export async function checkRally(g){
 await g.start();let worlds=0,hits=0,held=null,shot2=false,shot3=false;
 const frame=g.frame.bind(g);g.frame=()=>{const before=state(g),keys=g.keys,spin=signed(g.read('rally_spin')),period=g.read('rally_ai_period');const panel=frame(),actual=state(g);if(actual.count!==before.count&&before.phase===2){const s={...before};s.count=(s.count+1)&255;if(keys&((1<<0)|(1<<6)))s.player=Math.max(9,s.player-3);else if(keys&((1<<1)|(1<<7)))s.player=Math.min(51,s.player+3);if(++s.clock>=period){s.clock=0;s.cpu=Math.max(9,Math.min(51,s.cpu+Math.sign(s.y-(s.cpu+5))));}let q=s.y+s.dy;if(q<9||q>61)s.dy=-s.dy;else s.y=q;q=s.x+s.dx;let bounce=false;if(s.dx<0&&q<10&&s.x>=10){const r=s.y+1-s.player;if(r>=0&&r<13){s.dy=Math.max(-3,Math.min(3,deflect(r)+spin));s.dx=2;s.x=10;bounce=true;hits++;}}else if(s.dx>=0&&q>=115&&s.x<115){const r=s.y+1-s.cpu;if(r>=0&&r<13){s.dy=deflect(r);s.dx=-2;s.x=114;bounce=true;hits++;}}if(!bounce){if(q<2||q>=125){if(q<2)s.them++;else s.you++;s.active=0;s.clock=0;s.x=60;s.y=36;s.cpu=30;s.dx=2;s.dy=0;if(s.you===5)s.phase=4;if(s.them===5)s.phase=5;}else s.x=q;}assert.deepEqual(actual,s,'CPU reaction, bounded paddles, impact angles, wall bounces and scoring match the independent rally model');worlds++;}return panel;};
 function hold(key){if(held===key)return;if(held)g.hold(held,false);held=key;if(key)g.hold(key,true);}
 function target(){const s=state(g);let x=s.x,y=s.y,dy=s.dy;if(s.dx<0){while(x>=10){const q=y+dy;if(q<9||q>61)dy=-dy;else y=q;x-=2;}}const impact=y<36?11:1;return Math.max(9,Math.min(51,y+1-impact));}
 const frozen=state(g);for(let n=0;n<16;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),frozen);await g.save('gameplay-1');g.tap('left');assert.equal(g.read('rally_spin'),255);g.tap('right');assert.equal(g.read('rally_spin'),1);g.menu(1);assert.equal(g.read('rally_spin'),0);
 g.tap('space');g.tap('return');const paused=state(g);for(let n=0;n<24;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),paused);g.tap('return');g.menu(2);
 // Deliberately leave the paddle at the top to exercise match defeat and retry.
 for(let n=0;n<12000&&g.read('phase')===2;n++){if(!g.read('rally_active')){hold(null);g.frame();g.tap('space');}else{hold('up');g.frame();}}hold(null);g.frame();assert.equal(g.read('phase'),5,'An unattended paddle loses the five-point match');g.tap('space');assert.equal(g.read('rally_you'),0);assert.equal(g.read('rally_them'),0);
 for(let stage=0;stage<3;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);let turns=0;
  while(g.read('phase')===2&&turns++<30000){if(!g.read('rally_active')){hold(null);g.frame();g.tap('space');continue;}const wanted=target(),p=g.read('rally_player');hold(wanted<p-1?(stage%2?'letter-w':'up'):wanted>p+1?(stage%2?'letter-s':'down'):null);g.frame();if(stage===1&&g.read('rally_you')===1&&g.read('rally_active')&&g.read('rally_x')>85&&signed(g.read('rally_dy'))>0&&!shot2){await g.save('gameplay-2');shot2=true;}if(stage===2&&g.read('rally_you')===3&&g.read('rally_active')&&g.read('rally_x')<40&&signed(g.read('rally_dx'))<0&&!shot3){await g.save('gameplay-3');shot3=true;}}
  hold(null);g.frame();assert.equal(g.read('phase'),4,`Difficulty ${stage+1} is winnable with ordinary paddle keys (${JSON.stringify(state(g))})`);assert.equal(g.read('rally_you'),5);
 }
 assert.ok(worlds>400&&hits>10&&shot2&&shot3);
}
