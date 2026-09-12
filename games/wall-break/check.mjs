// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {playPixelVisible} from '../tools/harness.mjs';
import {readFile} from 'node:fs/promises';
function pixels(g){
 const board=g.read('wall_board',24),x=g.read('wall_x'),y=g.read('wall_y'),p=g.read('wall_paddle'),fb=g.machine.memory(g.symbols.framebuffer,1536);
 for(let Y=8;Y<64;Y++)for(let X=0;X<128;X++){
  if(!playPixelVisible(g,X,Y))continue;
  let on=false;
  if(Y<32){
   const v=board[Math.floor((Y-8)/8)*8+Math.floor(X/16)],a=X%16,b=Y%8;
   // Independently reconstruct the front rim, lower lip and right side of each brick.
   const rim=(b===1&&a>=2&&a<=13)||(b===6&&a>=2&&a<=14)||(b===7&&a>=3&&a<=15)||((a===1||a===14)&&b>=2&&b<=6)||(a===15&&b>=3&&b<=7);
   on=!!v&&(rim||(v===2&&a>=3&&a<=12&&(b===3||b===4)));
  }
  on ||= X>=x&&X<x+2&&Y>=y&&Y<y+2;on ||= X>=p&&X<p+22&&Y>=60&&Y<62;
  assert.equal((fb[(Y>>3)*192+X+g.read('view_origin')]>>(Y&7))&1,on?1:0,`Scene reconstruction ${X},${Y}`);
 }
}
export async function checkWall(g){
 const levels=JSON.parse(await readFile(new URL('./levels.json',import.meta.url)));await g.start();await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);pixels(g);let second=false,third=false;
 // Relaunch is an explicit lost ball, and a failed screen retries the same stage.
 for(let i=0;i<3;i++){g.tap('space');g.menu(1);}assert.equal(g.read('phase'),5);g.tap('space');assert.equal(g.read('stage'),0);
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('wall_board',24),levels[stage]);let previous=[...levels[stage]],score=0;
  g.tap('space');let returns=0,previousDy=255;
  for(let frames=0;g.read('phase')===2;frames++){
   if(frames%1000===0)console.log(JSON.stringify({stage:stage+1,frames,left:g.read('wall_left'),lives:g.read('wall_lives')}));
   assert.ok(frames<10000,`Stage ${stage+1}: ${g.read('wall_left')} bricks left`);
   const x=g.read('wall_x'),y=g.read('wall_y'),dy=g.read('wall_dy'),paddle=g.read('wall_paddle');
   if(dy===255&&previousDy===1)returns++;previousDy=dy;
   // Catch the descending ball near alternating ends to vary its next route.
   const target=Math.max(0,Math.min(106,x-[2,14,7,19,4,17,9,13,3,18,6,15][(returns+Math.floor(frames/900))%12]));const left=paddle>target+1,right=paddle<target-1;
   g.hold('left',left);g.hold('right',right);g.frame();
   const board=g.read('wall_board',24);for(let i=0;i<24;i++){assert.ok(board[i]<=previous[i]);score+=(previous[i]-board[i])*10;}previous=board;if(frames%47===0)pixels(g);assert.equal(g.word('wall_score'),score);assert.equal(g.read('wall_left'),board.filter(Boolean).length);
   if(g.read('phase')===2&&!g.read('wall_active')){g.hold('left',false);g.hold('right',false);g.tap('space');}
   if(stage===0&&frames===150){g.hold('left',false);g.hold('right',false);g.frame();g.tap('return');assert.equal(g.read('phase'),3);const frozen=[g.read('wall_x'),g.read('wall_y'),g.read('wall_paddle')];for(let n=0;n<40;n++)g.frame();g.tap('return');assert.deepEqual([g.read('wall_x'),g.read('wall_y'),g.read('wall_paddle')],frozen);pixels(g);}
   if(stage===4&&!second&&g.read('wall_left')<12){await g.save('gameplay-2');second=true;}
   if(stage===11&&!third&&g.read('wall_left')<7){await g.save('gameplay-3');third=true;}
  }
  g.hold('left',false);g.hold('right',false);g.frame();assert.equal(g.read('phase'),4,`Stage ${stage+1}`);
 }assert.ok(second&&third);
}
