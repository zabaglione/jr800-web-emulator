// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game,letters} from './harness.mjs';
import {checkLibrary} from './library_check.mjs';
const [wasm,out,id,mode='test']=process.argv.slice(2);
const g=await Game.open(wasm,out,id);
if(id==='arc-duel'){
 const frame=g.frame.bind(g);
 g.frame=()=>{const panel=frame();if(g.read('phase')===2){
  for(const [label,x,band] of [['A',0,0],['P',30,0],['W',60,0],['HP',96,0],['CPU',132,0],['WINS',0,1],['LAND',84,1]]){
   const expected=[...label].flatMap(c=>[...g.machine.memory(g.symbols.font+(c.charCodeAt(0)-32)*5,5),0]);
   assert.deepEqual([...g.machine.memory(g.symbols.framebuffer+band*192+x,expected.length)],expected,'Artillery HUD remains intact');
  }
 }return panel;};
}
try{
 await g.save('title');g.frame();assert.equal(g.word('dirty_bytes'),0,'Idle title performs no LCD writes');
 if(mode==='smoke'){
  await g.start();g.frame();assert.equal(g.word('dirty_bytes'),0,'Idle play');
  g.tap('right');g.tap('space');assert.equal(g.read('phase'),2,'First action remains playable');
  g.tap('return');assert.equal(g.read('phase'),3,'Menu opens');
  for(let i=0;i<3;i++){g.frame();assert.equal(g.word('dirty_bytes'),0,'Paused menu has no LCD writes');}
  g.tap('return');assert.equal(g.read('phase'),2,'Menu returns to play');
  g.tap('return');g.tap('space');assert.equal(g.read('phase'),2,'SPACE resumes play');
 }else if(mode==='test'&&id==='box-shift'){
  const solutions=JSON.parse(await readFile(new URL('../box-shift/solutions.json',import.meta.url)));
  let completed=0;
  for(let stage=0;stage<20;stage++){
   if(stage===0)await g.start();else {g.tap('space');assert.equal(g.read('stage'),stage);}
   const initial=g.read('board',112),p=g.read('player');
   if(stage===0){
    await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0,'Idle play');
    // One logical direction has the same meaning on either physical key group.
    const d=solutions[0][0];g.tap(d);const moved=g.read('player');g.menu(1);assert.equal(g.read('player'),p);assert.deepEqual(g.read('board',112),initial);
    g.tap(letters[d]);assert.equal(g.read('player'),moved);g.menu(1);assert.equal(g.word('moves'),0);
    g.hold('space',true);for(let i=0;i<4;i++)g.frame();assert.equal(g.read('phase'),2);g.hold('space',false);g.frame();
   }
   for(let i=0;i<solutions[stage].length;i++){
    g.tap(solutions[stage][i]);
    if(stage===0&&i===25)await g.save('gameplay-2');
    if(stage===12&&i===40)await g.save('gameplay-3');
    if(g.read('phase')===4)break;
   }
   assert.equal(g.read('phase'),4,`Stage ${stage+1} clears by keys`);completed++;
  }
  assert.equal(completed,20);
 }else if(mode==='test'&&id==='mirror-link'){
  const solutions=JSON.parse(await readFile(new URL('../mirror-link/solutions.json',import.meta.url)));
  for(let stage=0;stage<20;stage++){
   if(stage===0)await g.start();else g.tap('space');
   assert.equal(g.read('stage'),stage);g.frame();assert.equal(g.word('dirty_bytes'),0);
   if(stage===0)await g.save('gameplay-1');
   for(const cell of solutions[stage]){
    while((g.read('cursor')&15)<(cell&15))g.tap('right');
    while((g.read('cursor')&15)>(cell&15))g.tap('left');
    while((g.read('cursor')>>4)<(cell>>4))g.tap('down');
    while((g.read('cursor')>>4)>(cell>>4))g.tap('up');
    if(stage===7)await g.save('gameplay-2');
    if(stage===16)await g.save('gameplay-3');
    g.tap('space');
    if(g.read('phase')===4)break;
   }
   assert.equal(g.read('phase'),4,`Mirror stage ${stage+1}`);
   assert.equal(g.read('lit_count'),g.read('goal_count'));
  }
 }else if(mode==='test'&&id==='step-strike'){
  const solutions=JSON.parse(await readFile(new URL('../step-strike/solutions.json',import.meta.url)));
  for(let stage=0;stage<20;stage++){
   if(stage===0)await g.start();else g.tap('space');
   assert.equal(g.read('stage'),stage);
   if(stage===0){
    const board=g.read('board',112),guards=g.read('guards',4).filter(x=>x!==255),initial=g.read('player');
    const queue=[[initial,[]]],seen=new Set([initial]);let route;
    while(queue.length&&!route){const [p,path]=queue.shift();
     const visible=guards.some(enemy=>{const d=(p>>4)===(enemy>>4)?(p<enemy?1:-1):(p&15)===(enemy&15)?(p<enemy?16:-16):0;if(!d)return false;let q=p+d;while(q!==enemy&&!board[q])q+=d;return q===enemy;});
     if(visible)route=path;else for(const [d,key] of [[1,'right'],[-1,'left'],[16,'down'],[-16,'up']]){const q=p+d;if(!board[q]&&!guards.includes(q)&&!seen.has(q)){seen.add(q);queue.push([q,[...path,key]]);}}
    }
    assert.ok(route);for(const key of route){g.tap(key);if(g.read('phase')===5)break;}
    for(let i=0;i<30&&g.read('phase')===2;i++)g.menu(1);
    assert.equal(g.read('phase'),5,'Guard projectiles cause defeat');g.tap('space');assert.equal(g.read('player'),initial,'Retry restores the stage');
   }
   const state=[g.read('player'),g.read('guards',4),g.read('bullet_cells',8),g.read('turns')];
   for(let i=0;i<5;i++)g.frame();
   assert.deepEqual([g.read('player'),g.read('guards',4),g.read('bullet_cells',8),g.read('turns')],state,'World stops without an action');
   if(stage===0)await g.save('gameplay-1');
   for(let i=0;i<solutions[stage].length;i++){
    const action=solutions[stage][i];
    if(action==='wait')g.menu(1);else g.tap(action==='fire'?'space':action);
    if(stage===8&&i===4)await g.save('gameplay-2');
    if(stage===17&&i===5)await g.save('gameplay-3');
    assert.notEqual(g.read('phase'),5,`Unexpected defeat at stage ${stage+1}, action ${i+1}`);
    if(g.read('phase')===4)break;
   }
   assert.equal(g.read('phase'),4,`Tactical stage ${stage+1}`);
   assert.equal(g.read('guard_count'),0);
  }
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
 }else if(mode==='test'&&id==='pocket-factory'){
  const solutions=JSON.parse(await readFile(new URL('../pocket-factory/solutions.json',import.meta.url)));
  for(let stage=0;stage<12;stage++){
   if(stage===0)await g.start();else g.tap('space');
   assert.equal(g.read('stage'),stage);
   if(stage===0){
    await g.save('gameplay-1');g.menu(2);
    for(let i=0;i<100;i++)g.frame();assert.equal(g.read('shipped_a')+g.read('shipped_b'),0,'Disconnected belts jam without false shipments');
    assert.equal(g.read('items',112).filter(Boolean).length,2,'Resources remain blocked at their sources');
    g.menu(2);const stopped=g.read('items',112);for(let i=0;i<20;i++)g.frame();assert.deepEqual(g.read('items',112),stopped,'Paused factory');
    g.menu(1);g.tap('return');assert.equal(g.read('selection_active'),0);assert.equal(g.read('phase'),2,'RETURN cancels tool selection');g.menu(3);
   }
   for(const [cell,tile] of solutions[stage]){
    while((g.read('cursor')&15)<(cell&15))g.tap('right');
    while((g.read('cursor')&15)>(cell&15))g.tap('left');
    while((g.read('cursor')>>4)<(cell>>4))g.tap('down');
    while((g.read('cursor')>>4)>(cell>>4))g.tap('up');
    if(g.read('tool')!==tile-6){
     g.menu(1);assert.equal(g.read('selection_active'),1);
     while(g.read('tool')!==tile-6)g.tap('right');
     g.tap('space');assert.equal(g.read('selection_active'),0);
    }
    g.tap('space');assert.equal(g.read('board',112)[cell],tile);
   }
   assert.equal(g.read('factory_steps'),0,'Build mode freezes production');
   g.menu(2);assert.equal(g.read('running'),1);
   for(let f=0;g.read('phase')===2&&f<1200;f++){
    g.frame();
    if(stage===0&&f===160)await g.save('gameplay-2');
    if(stage===10&&f===250)await g.save('gameplay-3');
   }
   assert.equal(g.read('phase'),4,`Factory ${stage+1} ships both products`);
   assert.equal(g.read('shipped_a'),g.read('target_a'));
   assert.equal(g.read('shipped_b'),g.read('target_b'));
  }
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
     if(difficulty===0&&firing===1&&f===12)await g.save('gameplay-2');
     if(difficulty===2&&firing===3&&f===3)await g.save('gameplay-3');
    }
   }
   assert.equal(g.read('phase'),4,JSON.stringify({difficulty,firing,hp:g.read('health'),enemy:g.read('cpu_health'),mode:g.read('arc_mode')}));
   assert.equal(g.read('wins'),2);
  }
  g.tap('space');
  for(let shot=0;shot<20&&g.read('phase')===2;shot++){
   if(g.read('arc_mode')===4)g.tap('space');
   while(g.read('angle')<12)g.tap('up');while(g.read('power')>2)g.tap('left');g.tap('space');
   for(let f=0;g.read('phase')===2&&![0,4].includes(g.read('arc_mode'))&&f<700;f++)g.frame();
  }
  assert.equal(g.read('phase'),5,'Artillery match defeat');g.tap('space');assert.equal(g.read('health'),100);assert.equal(g.read('cpu_wins'),0);
 }else if(mode!=='test'){await g.start();await g.save('gameplay-1');}else await checkLibrary(g,id);
 if(['test','smoke'].includes(mode)&&!process.env.JR800_GAME_ROM)await writeFile(resolve(out,mode==='smoke'?'smoke-replay.txt':'replay.txt'),[g.symbols.frame_ready,g.symbols.framebuffer,g.symbols.phase,g.symbols.update_begin].join(' ')+'\n'+g.trace.join('\n')+'\n');
 const result={id,mode,passed:true,frames:g.frames,maxFrameCycles:Math.max(...g.cycles),maxDataBytes:Math.max(...g.transfers),idleDataBytes:0,bootstrap:process.env.JR800_GAME_ROM?'owner-supplied':'project-authored'};
 await writeFile(resolve(out,mode==='test'?(process.env.JR800_GAME_ROM?'owner-verification.json':'verification.json'):`${mode}.json`),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
 if(mode==='debug')console.log(JSON.stringify({state:g.machine.state(),symbols:g.symbols},null,2));
}finally{g.destroy();}
