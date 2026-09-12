// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const vectors=[[0,-256],[181,-181],[256,0],[181,181],[0,256],[-181,181],[-256,0],[-181,-181]];
const signed=n=>n>=32768?n-65536:n;
const state=g=>({x:g.word('putt_x'),y:g.word('putt_y'),vx:signed(g.word('putt_vx')),vy:signed(g.word('putt_vy')),left:g.read('putt_left'),shots:g.read('putt_shots'),phase:g.read('phase')});
export async function checkPutt(g){
 const levels=JSON.parse(await readFile(new URL('./levels.json',import.meta.url))),solutions=JSON.parse(await readFile(new URL('./solutions.json',import.meta.url)));let ticks=0,bounces=0;await g.start();
 const frame=g.frame.bind(g);g.frame=()=>{const before=state(g),phase=g.read('phase');const panel=frame();const actual=state(g);if(phase===2&&before.left&&actual.left!==before.left){const s={...before},level=levels[g.read('stage')];if(s.left===8){s.vx=Math.floor(s.vx/2);s.vy=Math.floor(s.vy/2);}const blocked=(x,y)=>x<1||x>125||y<9||y>61||[[x,y],[x+1,y],[x,y+1],[x+1,y+1]].some(([X,Y])=>level.board[((Y-8)>>3)*16+(X>>3)]===1);let q=s.x+s.vx;if(blocked(Math.floor(q/256),s.y>>8)){s.vx=-s.vx;bounces++;}else s.x=q;q=s.y+s.vy;if(blocked(s.x>>8,Math.floor(q/256))){s.vy=-s.vy;bounces++;}else s.y=q;if(Math.abs((s.x>>8)-level.cup[0])<=2&&Math.abs((s.y>>8)-level.cup[1])<=2){s.left=0;s.phase=4;}else if(!--s.left){s.x&=65280;s.y&=65280;if(s.shots>=12)s.phase=5;}assert.deepEqual(actual,s,'Every fixed-point movement, wall contact, slowdown and cup capture matches an independent model');ticks++;}return panel;};
 function aim(angle,power,letters=false){while(g.read('putt_aim')!==angle)g.tap(letters?'letter-d':'right');while(g.read('putt_power')<power)g.tap(letters?'letter-w':'up');while(g.read('putt_power')>power)g.tap(letters?'letter-s':'down');}
 function launch(){const n=g.read('putt_shots'),angle=g.read('putt_aim'),wind=levels[g.read('stage')].wind;g.hold('space',true);g.frame();assert.equal(g.read('putt_shots'),n+1);assert.deepEqual([signed(g.word('putt_vx')),signed(g.word('putt_vy'))],[vectors[angle][0]+wind*32,vectors[angle][1]]);g.hold('space',false);g.frame();}
 const idle=state(g);for(let n=0;n<20;n++){const blink=g.read('cursor_blink_mask');g.frame();if(blink===g.read('cursor_blink_mask'))assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),idle);await g.save('gameplay-1');
 aim(2,8);launch();g.tap('return');const frozen=state(g);for(let n=0;n<30;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),frozen);g.tap('return');while(g.read('putt_left'))g.frame();g.menu(2);
 for(let n=0;n<12;n++)g.menu(1);assert.equal(g.read('phase'),5,'Twelve penalty retees exhaust the hole');g.tap('space');
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('putt_board',112),levels[stage].board);assert.equal(g.read('putt_shots'),0);
  for(let shot=0;shot<solutions[stage].length;shot++){const [angle,power]=solutions[stage][shot];aim(angle,power,stage%2===1);launch();let frames=0;while(g.read('putt_left')){g.frame();if(stage===4&&shot===1&&frames===10)await g.save('gameplay-2');if(stage===10&&shot===1&&frames===14)await g.save('gameplay-3');assert.ok(++frames<180);}if(g.read('phase')===4)break;}
  assert.equal(g.read('phase'),4,`Hole ${stage+1} clears with legal shots`);assert.ok(g.read('putt_shots')<=12);
 }
 assert.ok(ticks>400&&bounces>0);
}
