// SPDX-License-Identifier: MIT
// Inspect real intermediate LCD frames and Port 1 writes, without changing RAM.
import assert from 'node:assert/strict';
import {mkdir,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game,png} from '../tools/harness.mjs';
import {checkReversi} from '../tools/reversi_check.mjs';
const [wasm,out]=process.argv.slice(2);
const initial=()=>Array.from({length:36},(_,p)=>[14,21].includes(p)?1:[15,20].includes(p)?2:0);
function flips(board,cell,side){
 const result=[];
 for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){
  if(!dx&&!dy)continue;
  let x=cell%6+dx,y=Math.floor(cell/6)+dy;const ray=[];
  while(x>=0&&x<6&&y>=0&&y<6&&board[y*6+x]===3-side){ray.push(y*6+x);x+=dx;y+=dy;}
  if(x>=0&&x<6&&y>=0&&y<6&&board[y*6+x]===side)result.push(...ray);
 }
 return result;
}
const g=await Game.open(wasm,out,'reversi-mini');
const symbols=Object.fromEntries([...(await readFile(resolve(out,'reversi-mini.sym'),'utf8')).matchAll(/^ \$([0-9A-F]+) [GL] (\S+)/gm)].map(([,a,n])=>[n,parseInt(a,16)]));
const machine=g.machine,run=machine.runTo.bind(machine),marker=symbols.rev_animation_frame,sound=symbols.rev_disk_sound;
const totals={frames:0,moves:0,flips:0,cues:0,cpuBlinks:0,multiDiskMoves:0};
let current,previous,tone,callback,record=false;const captures=[],writes=[];
let resultPause=false,resultPauseChecked=false;
const count=(b,side)=>b.filter(v=>v===side).length;
function observe(){
 const s={side:g.read('rev_active'),phase:g.read('rev_anim_phase'),cell:g.read('rev_anim_cell'),placed:g.read('rev_placed_cell'),index:g.read('rev_queue_index'),left:g.read('rev_flash_left'),sprite:g.read('rev_sprite'),cycle:Number(machine.state().cycleCount)};
 const display=g.read('rev_display',36),order=g.read('rev_queue',36).slice(0,g.read('rev_queue_count'));
 if(s.phase===1&&(!previous||previous.phase!==1||previous.placed!==s.placed||previous.side!==s.side)){
  const base=[...display];base[s.placed]=0;
  assert.deepEqual(order,flips(base,s.placed,s.side),'Queued flips match independent bounded rays in order');
  assert.ok(order.length&&new Set(order).size===order.length);
  current={base,order,placed:s.placed,side:s.side,blinks:[],cueCount:1};
  const final=[...display];for(const cell of order)final[cell]=s.side;
  assert.deepEqual(g.read('board',36),final,'Presented move uses the tested complete-move rule routine');
  totals.moves++;totals.multiDiskMoves+=Number(order.length>1);
 }
 assert.ok(current);
 if(s.phase===4&&(previous?.phase!==4||previous.left!==s.left)){
  current.blinks.push(s.left);if(s.left===6)current.blinkStart=s.cycle;
 }
 if(s.phase===5&&s.index===0&&s.side===2){
  assert.deepEqual(current.blinks,[6,5,4,3,2,1],'CPU completes three visible/hidden cycles before any flip');
  assert.ok(s.cycle-current.blinkStart>=900000,'CPU location remains highlighted for about 0.8 seconds');
  totals.cpuBlinks++;
 }
 const expected=[...current.base];expected[current.placed]=current.side;
 const completed=s.phase>=5?s.index+Number(s.phase===8):0;
 for(const cell of current.order.slice(0,completed))expected[cell]=current.side;
 assert.deepEqual(display,expected,'Exactly the current disk changes after its edge and new-face frames');
 assert.equal(g.read('grid_stat'),count(display,1));assert.equal(g.read('rev_cpu_disks'),count(display,2));
 assert.equal(g.read('phase'),2,'A pass or result waits until the complete animation');assert.equal(g.read('cursor'),255);
 const sprite={1:6,2:s.side+3,3:s.side,4:s.left%2?0:2,5:6-s.side,6:6,7:s.side+3,8:s.side}[s.phase];
 assert.equal(s.sprite,sprite);
 const panel=machine.lcdPanel(),tile=machine.memory(symbols.tiles+8+sprite*16,16),origin=g.read('view_origin')+16+(s.cell%6)*16,top=8+Math.floor(s.cell/6)*8;
 for(let y=0;y<8;y++)for(let x=0;x<16;x++)assert.equal(panel.dots[(top+y)*192+origin+x],1+((tile[x]>>y)&1),'Actual LCD shows the selected animation face');
 if(tone){
  assert.equal(tone.edges.length,32,'Each placement/flip sounds exactly sixteen complete periods');
  tone.edges.forEach((edge,i)=>assert.equal(edge.value&16,i%2?0:16,'Port 1 emits alternating high/low edges'));
  tone=undefined;
 }
 if(s.phase===8){totals.flips++;if(s.index===current.order.length-1)assert.equal(current.cueCount,current.order.length+1,'One placement cue and one cue per captured disk');}
 totals.frames++;
 if(record)captures.push({cycle:s.cycle,side:s.side,phase:s.phase,cell:s.cell,png:png(panel.dots)});
 previous=s;callback?.(s);
}
machine.setExecutionBreakpoint(marker,true);machine.setExecutionBreakpoint(sound,true);machine.setMemoryWatchpoint(2,'write',true);
machine.runTo=(address,limit)=>{
 for(let stops=0;stops<10000;stops++){
  const stop=run(address,limit);
  if(stop.reason==='address-reached'&&g.read('phase')===3&&resultPause){
   resultPause=false;resultPauseChecked=true;const before=g.read('rev_display',36);
   g.hold('return',false);
   for(let i=0;i<180;i++){machine.step();assert.equal(run(address,limit).reason,'address-reached');}
   assert.deepEqual(g.read('rev_display',36),before,'Pausing the final flip preserves the board');
   g.hold('return',true);machine.step();assert.equal(run(address,limit).reason,'address-reached');
   g.hold('return',false);machine.step();
   const finished=machine.runTo(address,limit);
   assert.equal(g.read('phase'),4);assert.equal(g.read('clear_active'),1,'The resumed final flip starts the win presentation');
   resultPauseChecked=true;return finished;
  }
  if(stop.reason==='memory-watchpoint'){
   const edge=machine.accesses({firstAddress:2,lastAddress:2}).at(-1);
   assert.equal(edge.kind,'data-write');tone?.edges.push(edge);
   if(record)writes.push({cycle:Number(edge.instructionCycle),value:edge.value});
   machine.step();continue;
  }
  if(stop.reason!=='execution-breakpoint')return stop;
  const pc=machine.state().pc;
  if(pc===sound){
   if(g.read('rev_anim_phase')===5)current.cueCount++;
   tone={edges:[]};totals.cues++;machine.step();continue;
  }
  assert.equal(pc,marker);machine.step();
  assert.equal(run(marker+3,limit).reason,'address-reached','Intermediate LCD transfer finishes');
  observe();
 }
 throw new Error('Unbounded animation instrumentation');
};
try{
 callback=s=>{
  if(s.phase===1)g.hold('space',false);
  const board=g.read('board',36),canMove=side=>board.some((v,p)=>!v&&flips(board,p,side).length);
  if(!resultPauseChecked&&!resultPause&&s.phase===7&&s.index===current.order.length-1&&count(board,1)>=count(board,2)&&!canMove(1)&&!canMove(2)){
   resultPause=true;g.hold('return',true);
  }
 };
 await checkReversi(g);g.finishClear();
 callback=undefined;assert.ok(resultPauseChecked,'The final winning flip was paused and resumed');
 assert.ok(totals.multiDiskMoves>20&&totals.cpuBlinks>20,'Complete games exercise both colors, multi-ray moves and CPU passes');
 // A held direction/SPACE cannot move the restored cursor or start another turn.
 g.tap('space');assert.equal(g.read('phase'),2,'A completed match starts again');
 g.tap('right');let held=false;
 callback=s=>{if(!held&&s.phase===1){held=true;g.hold('right',true);}};
 g.tap('space');callback=undefined;
 const moves=g.read('moves');for(let i=0;i<5;i++)g.frame();
 assert.equal(g.read('moves'),moves);assert.equal(g.read('cursor'),9);
 g.hold('right',false);g.frame();
 // Pause during the CPU blink and both colors' narrow/edge frames; resume,
 // undo and reset must all release the same shell stack and animation state.
 for(const [side,part,action] of [[2,4,'resume'],[1,6,'undo'],[2,7,'reset']]){
  g.menu(2);g.tap('right');previous=undefined;let paused=false,resumed=false,pausedState;
  callback=s=>{
   if(s.phase===1)g.hold('space',false);
   if(!paused&&s.side===side&&s.phase===part){paused=true;pausedState=[s.side,s.phase,s.index,s.sprite,s.left];g.hold('return',true);}
   else if(resumed){assert.deepEqual([s.side,s.phase,s.index,s.sprite,s.left],pausedState,'Resume preserves the exact interrupted face');resumed=false;}
  };
  g.tap('space');assert.ok(paused);assert.equal(g.read('phase'),3);
  g.hold('return',false);g.frame();const board=g.read('rev_display',36),anim=g.read('rev_anim_phase'),cues=totals.cues;
  for(let i=0;i<180;i++)g.frame();
  assert.deepEqual(g.read('rev_display',36),board);assert.equal(g.read('rev_anim_phase'),anim);assert.equal(totals.cues,cues,'Paused animation emits no disk cues');
  if(action==='resume'){resumed=true;g.tap('return');assert.equal(resumed,false);assert.equal(g.read('moves'),1);}
  else {for(let i=0;i<(action==='undo'?1:2);i++)g.tap('down');g.tap('space');assert.deepEqual(g.read('board',36),initial());assert.equal(g.read('moves'),0);}
  callback=undefined;assert.equal(g.read('rev_active'),0);assert.equal(machine.state().sp,0x5fff);
 }
 // Capture an unpaused opening turn, with its real Port 1 timing, for review.
 g.menu(2);g.tap('right');previous=undefined;
 const captureStill=()=>captures.push({cycle:Number(machine.state().cycleCount),side:0,phase:0,cell:g.read('cursor'),png:png(machine.lcdPanel().dots)});
 captureStill();record=true;g.tap('space');record=false;captureStill();
 const directory=resolve(out,'animation');await mkdir(directory,{recursive:true});
 for(const [i,frame] of captures.entries())await writeFile(resolve(directory,`${String(i).padStart(3,'0')}.png`),frame.png);
 await writeFile(resolve(directory,'timing.json'),JSON.stringify({clockHz:1228800,frames:captures.map(({png,...frame})=>frame),writes},null,2)+'\n');
 const result={passed:true,...totals,pausedCases:4,resumedWin:true,heldInput:true,actualPortWrites:true,physicalDevice:false};
 await writeFile(resolve(out,'animation-verification.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
}finally{g.destroy();}
