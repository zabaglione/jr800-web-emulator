// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
const names={'pipe-weave':'pipe_running','line-four':'four_active','number-rail':'rail_active','step-strike':'step_firing'};
export function settleMotion(g,id=g.id){
 const flag=names[id];if(!flag||!g.read(flag))return;
 const turns=id==='step-strike'?g.read('turns'):null,paths=new Map();
 let wet=id==='pipe-weave'?g.read('pipe_wet',36):null;
 const sum=()=>g.read('board',16).reduce((n,v)=>n+(v?2**v:0),0);
 const total=id==='number-rail'?sum():null;
 for(let i=0;g.read(flag)&&g.read('phase')===2&&i<300;i++){
  if(!g.motionPauseChecked&&i===2){
   g.motionPauseChecked=true;const before=g.read('board',id==='step-strike'?112:id==='line-four'?42:id==='number-rail'?16:36);
   g.tap('return');assert.equal(g.read('phase'),3);
   for(let j=0;j<12;j++){g.frame();assert.equal(g.word('dirty_bytes'),0,'Paused motion performs no LCD transfers');}
   assert.deepEqual(g.read('board',before.length),before,'The menu freezes the board during an animated action');
   assert.equal(g.read(flag),1);g.tap('return');assert.equal(g.read('phase'),2);
  }
  if(id==='line-four'){
   const side=g.read('four_side'),cell=g.read('four_fall_cell'),path=paths.get(side)??[];
   if(path.at(-1)!==cell){if(path.length)assert.equal(cell,path.at(-1)+7,'Disk visits every row in its chosen column');else assert.ok(cell<7,'Disk begins at the top');path.push(cell);paths.set(side,path);}
  }
  const visible=id==='step-strike'?g.read('step_shot_cell')!==g.read('player'):
   id==='line-four'?g.read('four_fall_cell')>=14:i===5;
  if(!g.motionSceneTask&&visible)g.motionSceneTask=g.save('motion',false);
  g.frame();
  if(g.read(flag)){
   if(turns!==null)assert.equal(g.read('turns'),turns,'Enemy time stays frozen until the player shot finishes');
   if(total!==null)assert.equal(sum(),total,'Sliding and merging preserve total tile value before spawning');
  }
  if(wet){
   const now=g.read('pipe_wet',36),b=g.read('board',36);
   for(let p=0;p<36;p++){
    if(wet[p])assert.ok(now[p],'Water does not disappear within a wave');
    else if(now[p])assert.ok([[0,-1,1,2],[0,1,2,1],[-1,0,4,8],[1,0,8,4]].some(([dx,dy,bit,opp])=>{const x=p%6+dx,y=Math.floor(p/6)+dy,q=y*6+x;return x>=0&&x<6&&y>=0&&y<6&&wet[q]&&(b[p]&bit)&&(b[q]&opp);}), 'New water is connected to the previous visible wave');
   }wet=now;
  }
 }
 assert.equal(g.read(flag),0,'Visible action completes within its bounded animation');
}
