// SPDX-License-Identifier: MIT
// Observe the program after real keys; never patch gameplay memory.
import assert from 'node:assert/strict';

export function iceReady(g){
 for(let i=0;g.read('ice_mode')===1&&i<240;i++)g.frame();
 assert.notEqual(g.read('ice_mode'),1,'The introduction completes');
}

export function iceSettle(g,from,to,delta){
 const observed=[g.read('cursor')];let last=observed[0],celebrationStart=null;
 const notes=new Set();
 for(let f=0;[2,3].includes(g.read('ice_mode'))&&g.read('phase')===2&&f<400;f++){
  if(g.read('ice_mode')===3){
   celebrationStart??=Number(g.machine.state().cycleCount);
   notes.add(g.read('ice_note'));
   if(!g.iceClearChecked&&g.read('ice_note')===2){
    g.iceClearChecked=true;const timer=g.read('ice_timer');g.tap('return');
    for(let j=0;j<25;j++)g.frame();
    assert.equal(g.read('ice_timer'),timer);assert.equal(g.read('ice_note'),2,'The menu freezes the clear melody');
    g.tap('return');g.hold('space',true);
   }
  }
  g.frame();const p=g.read('cursor');
  if(p!==last){observed.push(p);last=p;}
 }
 assert.ok(g.read('ice_mode')===0||g.read('phase')===4,'The slide or celebration completes');
 const expected=[];for(let p=from+delta;;p+=delta){expected.push(p);if(p===to)break;assert.ok(expected.length<=14);}
 assert.deepEqual(observed,expected,'Every traversed cell is displayed, in order, without teleporting');
 if(celebrationStart!==null){
  assert.ok(Number(g.machine.state().cycleCount)-celebrationStart>=1_500_000,'The solved board remains visible for over one second');
  assert.deepEqual([...notes],[0,1,2,3,4,5,6,7],'All seven clear notes advance before the result');
  if(g.keys&16){const stage=g.read('stage');for(let j=0;j<20;j++)g.frame();assert.equal(g.read('stage'),stage,'Held clear input cannot skip the result');g.hold('space',false);g.frame();}
 }
}

function tile(g,cell){
 const x=g.read('view_origin')+8+(cell%14)*8,band=1+Math.floor(cell/14);
 return [...g.machine.memory(g.symbols.framebuffer+band*192+x,8)];
}
export async function checkIceIntro(g,stage){
 const start=g.read('cursor'),board=g.read('board',98),states=[],poses=new Set();
 const gems=[board.indexOf(3),board.indexOf(4)],floor=tile(g,gems[0]);
 assert.equal(g.read('ice_reveal'),0);assert.equal(g.read('ice_visible'),0);
 assert.deepEqual(tile(g,gems[1]),floor,'Both gems are initially hidden');
 const tick=g.read('ice_timer');g.tap('return');assert.equal(g.read('phase'),3);
 for(let i=0;i<20;i++)g.frame();
 assert.equal(g.read('ice_timer'),tick,'The menu freezes the introduction');g.tap('return');
 g.hold('left',true);g.hold('space',true);
 for(let i=0;g.read('ice_mode')===1&&i<240;i++){
  g.frame();const n=g.read('ice_reveal');
  if(states.at(-1)!==n){
   states.push(n);
   if(n>=1&&n<=4)assert.ok(g.read('ice_sfx_left')>0,'Each item receives a short SE');
   if(n===1){assert.notDeepEqual(tile(g,gems[0]),floor);assert.deepEqual(tile(g,gems[1]),floor);await g.save('intro-item');}
   if(n===2)assert.notDeepEqual(tile(g,gems[1]),floor);
   if(n===5){assert.equal(g.read('ice_visible'),1);assert.ok(g.read('ice_sfx_left')>0);await g.save('intro-player');}
  }
  if(n>=5)poses.add(g.read('ice_visible'));
  assert.equal(g.read('cursor'),start);assert.equal(g.word('challenge_moves'),0,'The intro and held keys spend no moves');
 }
 assert.deepEqual(states,Array.from({length:12},(_,i)=>i));assert.deepEqual([...poses],[1,0]);
 assert.notDeepEqual(tile(g,start),tile(g,gems[0]),'The skater and diamond have different silhouettes');
 for(let i=0;i<25;i++)g.frame();assert.equal(g.word('challenge_moves'),0,'Held intro inputs do not start a slide');
 g.hold('left',false);g.hold('space',false);g.frame();
 assert.deepEqual(g.read('board',98),board);assert.equal(g.read('ice_visible'),1);
 // Pause and undo in the middle of a long first slide.
 g.tap(stage.normal[0]);assert.equal(g.read('ice_mode'),2);
 const at=g.read('cursor'),timer=g.read('ice_timer');g.tap('return');
 for(let i=0;i<30;i++)g.frame();
 assert.equal(g.read('cursor'),at);assert.equal(g.read('ice_timer'),timer,'Sliding stops in the menu');
 g.tap('down');g.tap('space');assert.equal(g.read('ice_mode'),0);
 assert.equal(g.read('cursor'),start);assert.deepEqual(g.read('board',98),board);assert.equal(g.word('challenge_moves'),2);
 g.menu(3);iceReady(g);
}
