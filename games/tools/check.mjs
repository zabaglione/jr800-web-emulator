// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from './harness.mjs';
import {iceReady} from './ice_check.mjs';
import {settleMotion} from './motion_check.mjs';
import {checkLibrary} from './library_check.mjs';
import {checkPuzzle,puzzleIds} from './puzzle_check.mjs';
const [wasm,out,id,mode='test']=process.argv.slice(2);
if(id==='relic-dive-gfx'){await import('../relic-dive-gfx/check.mjs');process.exit(process.exitCode??0);}
const g=await Game.open(wasm,out,id);
if(['box-shift','step-strike'].includes(id)){
 const start=g.start.bind(g);g.start=async(...args)=>{
  await start(...args);
  const panel=g.machine.lcdPanel(),origin=g.read('view_origin');
  const pixel=(cell,x,y)=>panel.dots[(8+Math.floor(cell/16)*8+y)*192+origin+(cell%16)*8+x]===2;
  const p=g.read('player');
  assert.ok(pixel(p,3,1)&&pixel(p,4,1)&&!pixel(p,1,1),'Player head is at the top of the LCD sprite');
  assert.ok(pixel(p,2,6)&&pixel(p,5,6),'Player feet are below the body');
  if(id==='step-strike')for(const enemy of g.read('guards',4).filter(x=>x!==255)){
   assert.ok(pixel(enemy,2,1)&&pixel(enemy,5,1)&&!pixel(enemy,1,1),'Guard head is above its body');
   assert.ok(pixel(enemy,1,6)&&pixel(enemy,6,6),'Guard feet are below the body');
  }
 };
}
if(id==='arc-duel'){
 const frame=g.frame.bind(g);let instruments;
 g.frame=()=>{const panel=frame();if(g.read('phase')===2){
  // Headings remain untouched when terrain or projectiles change underneath.
  const spans=[[1,7],[33,3],[65,3],[96,3],[130,11],[166,7],[194,19],[243,15],[358,23]];
  const headings=spans.flatMap(([offset,size])=>[...g.machine.memory(g.symbols.framebuffer+offset,size)]);
  if(instruments)assert.deepEqual(headings,instruments,'Artillery static instrument headings remain intact');else instruments=headings;
 }return panel;};
}
try{
 await g.save('title');g.frame();assert.equal(g.word('dirty_bytes'),0,'Idle title performs no LCD writes');
 if(mode==='smoke'){
  await g.start();if(id==='ice-route')iceReady(g);settleMotion(g);g.frame();assert.equal(g.word('dirty_bytes'),0,'Idle play');
  g.tap('right');g.tap('space');assert.equal(g.read('phase'),2,'First action remains playable');
  if(g.read('selection_active')){g.tap('return');assert.equal(g.read('selection_active'),0,'RETURN cancels selection');assert.equal(g.read('phase'),2);}
  g.tap('return');assert.equal(g.read('phase'),3,'Menu opens');
  for(let i=0;i<3;i++){g.frame();assert.equal(g.word('dirty_bytes'),0,'Paused menu has no LCD writes');}
  g.tap('return');assert.equal(g.read('phase'),2,'Menu returns to play');
  g.tap('return');g.tap('space');assert.equal(g.read('phase'),2,'SPACE resumes play');
 }else if(mode==='test'&&puzzleIds.includes(id)){
  await checkPuzzle(g,id);
 }else if(mode==='test'&&id==='circuit-deck'){
  const cards=JSON.parse(await readFile(new URL('../circuit-deck/cards.json',import.meta.url)));
  await g.start();
  for(let i=0;i<50&&g.read('phase')===2;i++)g.menu(1);
  assert.equal(g.read('phase'),5,'Enemy attacks cause defeat');assert.ok(g.read('shuffles')>1,'Discard pile recycled');g.tap('space');assert.equal(g.read('hp'),40);
  await g.save('gameplay-1');
  let actions=0,rewards=0,bossCaptured=false;
  while(g.read('phase')===2&&actions++<800){
   if(g.read('battle_mode')){
    if(rewards===2)await g.save('gameplay-2');
    // Prefer damage, poison and sustainable healing when constructing this test deck.
    const options=g.read('rewards',3);const priority=[3,1,2,7,1,6,10,3,9,8,5,5];
    const best=options.reduce((a,c,i)=>priority[c]>priority[options[a]]?i:a,0);
    while(g.read('selected')!==best)g.tap('right');
    const size=g.read('deck_size');g.tap('space');assert.equal(g.read('deck_size'),size+1);rewards++;continue;
   }
   if(g.read('battle')===8&&!bossCaptured){await g.save('gameplay-3');bossCaptured=true;}
   const h=g.read('hand',3),en=g.read('energy');
   const choices=h.map((id,i)=>({id,i,v:id===255?-999:cards[id].stats[0]>en?-999:(cards[id].stats[1]+cards[id].stats[4]*5+Math.min(40-g.read('hp'),cards[id].stats[3])+cards[id].stats[2]*0.6+cards[id].stats[5]*2+cards[id].stats[6]*3)})).sort((a,b)=>b.v-a.v);
   if(choices[0].v>=0){
    while(g.read('selected')!==choices[0].i)g.tap('right');
    const oldEnergy=g.read('energy'),c=cards[choices[0].id].stats;
    g.tap('space');assert.equal(g.read('hand',3)[choices[0].i],255,'Played card removed');
    assert.equal(g.read('energy'),oldEnergy-c[0]+c[6]);
   }else {g.menu(1);}
  }
  assert.equal(g.read('phase'),4,JSON.stringify({battle:g.read('battle'),hp:g.read('hp'),actions}));
  assert.equal(rewards,8);assert.ok(g.read('shuffles')>=9,'Every battle shuffles its deck');
 }else if(mode==='test'&&id==='arc-duel'){
  const predict=(h,w,a,p)=>{
   let x=16*256,y=(h[16]-7)*256,vx=Math.round(128*Math.cos((15+a*5)*Math.PI/180))*p,vy=-Math.round(128*Math.sin((15+a*5)*Math.PI/180))*p;
   for(let t=1;t<160;t++){
    x+=vx;y+=vy;vx+=w;vy+=32;
    const X=Math.floor(x/256),Y=Math.floor(y/256);
    if(X<0||X>=192)return null;
    if(Y>=h[X])return {x:X,y:Math.min(63,Y),t};
   }return null;
  };
  let flightCaptured=false;
  for(let difficulty=0;difficulty<3;difficulty++){
   if(difficulty===0)await g.start();else g.tap('space');
   assert.equal(g.read('stage'),difficulty);
   for(let t=0;t<difficulty*2;t++)g.menu(1);
   assert.equal(g.read('terrain'),difficulty*2);
   if(difficulty===0)await g.save('gameplay-1');
   let firing=0;
   while(g.read('phase')===2&&firing<18){
    if(g.read('arc_mode')===4){g.tap('space');continue;}
    assert.equal(g.read('arc_mode'),0);
    const h=g.read('heights',192),w=g.read('wind')>127?g.read('wind')-256:g.read('wind');
    let best=null;
    for(let a=0;a<13;a++)for(let p=2;p<=9;p++){
     const hit=predict(h,w,a,p);if(!hit)continue;
     const score=Math.abs(hit.x-175)+Math.abs(hit.y-(h[175]-3));
     if(!best||score<best.score)best={a,p,hit,score};
    }
    assert.ok(best&&best.score<26,'Reachable artillery target');
    while(g.read('angle')<best.a)g.tap('up');while(g.read('angle')>best.a)g.tap('down');
    while(g.read('power')<best.p)g.tap('right');while(g.read('power')>best.p)g.tap('left');
    g.tap('space');firing++;
    for(let f=0;g.read('phase')===2&&![0,4].includes(g.read('arc_mode'))&&f<700;f++){
     g.frame();
     if(difficulty===0&&firing===1&&!flightCaptured&&g.read('arc_mode')===1&&g.read('shot_x')>=32&&g.read('shot_y')>=12&&g.read('shot_y')<g.read('heights',192)[g.read('shot_x')]-3){await g.save('gameplay-2');flightCaptured=true;}
     if(difficulty===2&&firing===3&&f===3)await g.save('gameplay-3');
    }
   }
   assert.equal(g.read('phase'),4,JSON.stringify({difficulty,firing,hp:g.read('health'),enemy:g.read('cpu_health'),mode:g.read('arc_mode')}));
   assert.equal(g.read('wins'),2);
  }
  assert.ok(flightCaptured,'The flight screenshot contains a visible shell above the terrain');
  g.tap('space');
  for(let shot=0;shot<20&&g.read('phase')===2;shot++){
   if(g.read('arc_mode')===4)g.tap('space');
   while(g.read('angle')<12)g.tap('up');while(g.read('power')>2)g.tap('left');g.tap('space');
   for(let f=0;g.read('phase')===2&&![0,4].includes(g.read('arc_mode'))&&f<700;f++)g.frame();
  }
  assert.equal(g.read('phase'),5,'Artillery match defeat');g.tap('space');assert.equal(g.read('health'),100);assert.equal(g.read('cpu_wins'),0);
 }else if(mode!=='test'){await g.start();await g.save('gameplay-1');}else await checkLibrary(g,id);
 g.finishClear();await g.clearSceneTask;await g.motionSceneTask;
 if(['test','smoke'].includes(mode)&&!process.env.JR800_GAME_ROM)await writeFile(resolve(out,mode==='smoke'?'smoke-replay.txt':'replay.txt'),[g.symbols.frame_ready,g.symbols.framebuffer,g.symbols.phase,g.symbols.update_begin].join(' ')+'\n'+g.trace.join('\n')+'\n');
 const result={id,mode,passed:true,frames:g.frames,maxFrameCycles:Math.max(...g.cycles),maxDataBytes:Math.max(...g.transfers),idleDataBytes:0,bootstrap:process.env.JR800_GAME_ROM?'owner-supplied':'project-authored'};
 await writeFile(resolve(out,mode==='test'?(process.env.JR800_GAME_ROM?'owner-verification.json':'verification.json'):`${mode}.json`),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
 if(mode==='debug')console.log(JSON.stringify({state:g.machine.state(),symbols:g.symbols},null,2));
}finally{g.destroy();}
