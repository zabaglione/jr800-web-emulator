// SPDX-License-Identifier: MIT
// Game-owned navigation, rules and CPU turn display checks.
import assert from 'node:assert/strict';
const edges=[];
for(let y=0;y<7;y++)for(let x=0;x<9;x++)if((x+y)%2)edges.push([x,y]);
const index=(x,y)=>edges.findIndex(([X,Y])=>X===x&&Y===y);
const boxes=[];for(let y=1;y<7;y+=2)for(let x=1;x<9;x+=2)boxes.push([index(x,y-1),index(x,y+1),index(x-1,y),index(x+1,y)]);
const keys=['up','down','left','right'];
function move(edge,column,key){
 let [x,y]=edges[edge];
 if(key==='up'||key==='down')y=Math.max(0,Math.min(6,y+(key==='up'?-1:1)));
 else {x+=key==='left'?-2:2;if(x<0||x>8)return [edge,column];column=x;}
 const row=edges.map(([X,Y],i)=>[Math.abs(X-column),i,Y]).filter(([,i,Y])=>Y===y).sort((a,b)=>a[0]-b[0]||a[1]-b[1]);
 return [row[0][1],column];
}
function aim(g,p){
 const todo=[[g.read('dot_cursor'),g.read('dot_column'),[]]],seen=new Set([todo[0].slice(0,2).join()]);
 for(let i=0;i<todo.length;i++){
  const [q,column,path]=todo[i];if(q===p){for(const k of path)g.tap(k);assert.equal(g.read('dot_cursor'),p);return;}
  for(const key of keys){const n=move(q,column,key),id=n.join();if(!seen.has(id)){seen.add(id);todo.push([...n,[...path,key]]);}}
 }
 throw new Error('Edge cursor disconnected');
}
function checkNavigation(g){
 for(let e=0;e<31;e++)for(const key of keys){
  aim(g,e);const column=g.read('dot_column'),expected=move(e,column,key);g.tap(key);
  assert.deepEqual([g.read('dot_cursor'),g.read('dot_column')],expected,'Every edge follows row-based navigation, with clamped boundaries');
  assert.equal(g.read('cursor'),edges[expected[0]][1]*9+edges[expected[0]][0]);
  if(key==='up'||key==='down'){
   const y=edges[e][1];if((key==='up'&&y>0)||(key==='down'&&y<6)){
    assert.notEqual(edges[g.read('dot_cursor')][1]%2,y%2,'One vertical step switches line orientation');
    g.tap(key==='up'?'down':'up');assert.equal(g.read('dot_cursor'),e,'Vertical round trips retain the original column without drift');
   }
  }else assert.equal(edges[g.read('dot_cursor')][1],edges[e][1],'Horizontal movement never jumps to another row at an edge');
 }
 assert.deepEqual(g.read('board',43),Array(43).fill(0));assert.equal(g.read('moves'),0);
 g.menu(2);
}
function draw(b,edge,side){b[edge]=side;let gained=0;for(let i=0;i<12;i++)if(!b[31+i]&&boxes[i].every(e=>b[e])){b[31+i]=side;gained++;}return gained;}
function choice(b,level){
 let best=-1,score=0;
 for(let e=0;e<31;e++)if(!b[e]){
  if(!level)return e;
  const counts=boxes.filter(box=>box.includes(e)).map(box=>box.filter(q=>b[q]||q===e).length);
  const s=32+64*counts.filter(n=>n===4).length-(level===2?12*counts.filter(n=>n===3).length:0);
  if(s>score){score=s;best=e;}
 }return best;
}
function play(b,e,level){
 const next=[...b],gain=draw(next,e,1),steps=[];
 if(!gain){let move=choice(next,level);while(move>=0){const captured=draw(next,move,2);steps.push({edge:move,board:[...next],captured});if(!captured)break;move=choice(next,level);}}
 return {next,gain,cpu:steps.length,steps};
}
function checkBlinkPixels(g,step){
 const dots=g.machine.lcdPanel().dots,hidden=!g.read('dot_flash_visible'),origin=g.read('view_origin')+24;
 const pixel=(x,y)=>dots[y*192+origin+x]===2;
 for(let e=0;e<31;e++){
  const [x,y]=edges[e],occupied=Boolean(step.board[e])&&!(hidden&&e===step.edge);
  assert.equal(pixel(x*8+3,8+y*8+3),occupied,'Only the latest CPU edge blinks; the player cursor is hidden');
  if(x%2){assert.equal(pixel(x*8-1,8+y*8+3),occupied,'Left node arm follows its edge');assert.equal(pixel(x*8+8,8+y*8+3),occupied,'Right node arm follows its edge');}
  else {assert.equal(pixel(x*8+3,8+y*8-1),occupied,'Top node arm follows its edge');assert.equal(pixel(x*8+3,8+y*8+8),occupied,'Bottom node arm follows its edge');}
 }
 for(let box=0;box<12;box++){
  const x=(box%4*2+1)*8,y=8+(Math.floor(box/4)*2+1)*8,owner=step.board[31+box];
  const visible=!(hidden&&owner===2&&boxes[box].includes(step.edge));
  assert.equal(pixel(x+1,y+1),Boolean(owner)&&visible,'New CPU boxes blink with their closing edge');
  assert.equal(pixel(x+3,y+3),owner===2&&visible,'Previous box ownership remains visible');
 }
}
async function finishCpu(g,steps,{exercise=false}={}){
 const cursor=g.read('dot_cursor'),column=g.read('dot_column'),moves=g.read('moves');
 for(const step of steps){
  assert.equal(g.read('dot_cpu_active'),1);assert.equal(g.read('dot_edge'),step.edge,'CPU reveals its moves one at a time');
  assert.equal(g.read('dot_flash_left'),6,'Every CPU move starts a complete blink interval');
  assert.deepEqual(g.read('board',43),step.board);
  if(exercise){
   g.tap('return');assert.equal(g.read('phase'),3);
   const left=g.read('dot_flash_left'),visible=g.read('dot_flash_visible');
   for(let i=0;i<30;i++)g.frame();
   assert.equal(g.read('dot_flash_left'),left);assert.equal(g.read('dot_flash_visible'),visible);assert.equal(g.word('dirty_bytes'),0,'Menu freezes the CPU animation');
   g.tap('return');assert.equal(g.read('phase'),2);assert.equal(g.read('dot_flash_left'),left,'Resuming does not count paused time');
   g.hold('space',true);g.hold('right',true);
  }
  const start=Number(g.machine.state().cycleCount),states=[],phases=[];
  for(let frame=0;g.read('dot_cpu_active')&&g.read('dot_edge')===step.edge&&frame<100;frame++){
   assert.equal(g.read('phase'),2,'Result and next turn wait for the last CPU blink');
   assert.deepEqual(g.read('board',43),step.board,'Blinking never alters the logical board');
   assert.equal(g.read('moves'),moves);assert.equal(g.read('dot_cursor'),cursor);assert.equal(g.read('dot_column'),column);
   checkBlinkPixels(g,step);
   const visible=g.read('dot_flash_visible');if(states.at(-1)!==visible)states.push(visible);
   const left=g.read('dot_flash_left');if(phases.at(-1)!==left)phases.push(left);
   if(!g.dotBlinkCaptured){await g.save('cpu-blink-on',false);g.dotBlinkCaptured=true;}
   if(!visible&&!g.dotBlinkHiddenCaptured){await g.save('cpu-blink-off',false);g.dotBlinkHiddenCaptured=true;}
   if(step.captured&&!g.dotBoxBlinkCaptured){await g.save('cpu-capture-on',false);g.dotBoxBlinkCaptured=true;}
   if(step.captured&&!visible&&!g.dotBoxBlinkHiddenCaptured){await g.save('cpu-capture-off',false);g.dotBoxBlinkHiddenCaptured=true;}
   g.frame();
  }
  assert.deepEqual(states,[1,0,1,0,1,0],'Each CPU edge visibly blinks three times');
  assert.deepEqual(phases,[6,5,4,3,2,1]);
  if(!exercise){const cycles=Number(g.machine.state().cycleCount)-start;assert.ok(cycles>1_000_000&&cycles<1_700_000,`Blink lasts about one second: ${cycles} cycles`);}
  exercise=false;
 }
 assert.equal(g.read('dot_cpu_active'),0);
 if(g.keys){
  for(let i=0;i<24;i++)g.frame();
  assert.equal(g.read('moves'),moves);assert.equal(g.read('dot_cursor'),cursor,'Held input cannot leak into the player turn');
  g.hold('space',false);g.hold('right',false);g.frame();
 }
 assert.equal(g.read('cursor'),edges[cursor][1]*9+edges[cursor][0]);
}
export async function checkDot(g){
 await g.start();checkNavigation(g);let wins=0,losses=0,extra=0,chain=0;
 for(let level=0;level<3;level++){
  if(level){g.tap('return');assert.equal(g.read('phase'),1);g.tap('right');g.tap('space');}
  assert.equal(g.read('stage'),level);let b=Array(43).fill(0);
  assert.deepEqual(g.read('board',43),b);
  if(!level){await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);}
  for(let turn=0;turn<31&&g.read('phase')===2;turn++){
   const e=choice(b,level===0?0:2),{next,gain,cpu,steps}=play(b,e,level);aim(g,e);g.tap('space');
   if(!level&&!turn){
    assert.equal(g.read('dot_cpu_active'),1);g.menu(1);
    assert.equal(g.read('dot_cpu_active'),0,'Undo cancels a CPU blink');assert.deepEqual(g.read('board',43),b);assert.equal(g.read('moves'),0);
    aim(g,e);g.tap('space');
   }
   await finishCpu(g,steps,{exercise:!level&&!turn});
   assert.deepEqual(g.read('board',43),next,'Every closed box belongs to the player who drew its final edge');
   const own=next.slice(31).filter(n=>n===1).length,opponent=next.slice(31).filter(n=>n===2).length;
   assert.equal(g.read('dot_player_score'),own);assert.equal(g.read('dot_cpu_score'),opponent);assert.equal(g.read('grid_stat'),12-own-opponent);
   assert.equal(g.read('phase'),own+opponent<12?2:own>=opponent?4:5);
   if(gain){extra++;assert.equal(cpu,0,'Closing a box grants another human turn');}if(cpu>1)chain++;
   if(!level&&!turn){g.menu(1);assert.deepEqual(g.read('board',43),b);assert.equal(g.read('moves'),0);aim(g,e);g.tap('space');await finishCpu(g,steps);assert.deepEqual(g.read('board',43),next);g.tap('space');assert.deepEqual(g.read('board',43),next,'Used edge cannot be redrawn');}
   b=next;if(level===1&&turn===6)await g.save('gameplay-2');
  }
  assert.equal(g.read('grid_stat'),0);if(level===2)await g.save('gameplay-3');if(g.read('phase')===4)wins++;else losses++;
 }
 assert.ok(wins&&losses,'Winning and losing matches are playable');assert.ok(extra&&chain,'Both sides receive consecutive turns');g.tap('space');assert.equal(g.read('phase'),2);
}
