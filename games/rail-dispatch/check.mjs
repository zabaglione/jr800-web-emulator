// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const state=g=>({positions:g.read('dispatch_positions',4),targets:g.read('dispatch_targets',4),gates:g.read('dispatch_gates',2),route:g.read('dispatch_switch'),time:g.read('dispatch_time'),done:g.read('dispatch_done'),next:g.read('dispatch_next_train'),total:g.read('dispatch_total'),running:g.read('dispatch_running'),reason:g.read('dispatch_reason'),phase:g.read('phase')});
function world(s,schedule,network){s.time++;const fail=reason=>{s.running=0;s.reason=reason;s.phase=5;return s;};const proposed=s.positions.map(p=>p===255||p===2&&!s.gates[0]||p===7&&!s.gates[1]?p:p===14?(s.route?21:15):network.next[p]);
 for(let pass=0;pass<4;pass++)for(let a=0;a<4;a++)if(proposed[a]!==255&&proposed[a]!==s.positions[a]){for(let b=0;b<4;b++)if(a!==b&&s.positions[b]===proposed[a]&&proposed[b]===s.positions[b]){proposed[a]=s.positions[a];break;}}
 for(let a=0;a<4;a++)for(let b=a+1;b<4;b++)if(proposed[a]!==255&&proposed[b]!==255&&(proposed[a]===proposed[b]||proposed[a]===s.positions[b]&&proposed[b]===s.positions[a]))return fail(1);
 for(let a=0;a<4;a++){s.positions[a]=proposed[a];if(proposed[a]===20||proposed[a]===26){if(s.targets[a]!==Number(proposed[a]===26))return fail(2);s.positions[a]=255;s.done++;}}
 if(s.next<s.total){const [arrival,entry,dest]=schedule[s.next],p=entry*5;let free=-1;for(let i=0;i<4;i++)if(s.positions[i]===255)free=i;if(s.time>=arrival&&free>=0&&!s.positions.includes(p)){s.positions[free]=p;s.targets[free]=dest;s.next++;}}
 if(s.done===s.total){s.running=0;s.phase=4;}else if(s.time>=180)return fail(3);return s;
}
export async function checkDispatch(g){
 const schedules=JSON.parse(await readFile(new URL('./levels.json',import.meta.url))),network=JSON.parse(await readFile(new URL('./network.json',import.meta.url)));await g.start();await g.save('gameplay-1');let worlds=0,second=false,third=false;
 const original=g.frame.bind(g);g.frame=()=>{const before=state(g),stage=g.read('stage'),panel=original(),after=state(g);if(after.time===before.time+1){assert.deepEqual(after,world(before,schedules[stage],network),'Every train movement, blocked queue, collision, shipment and scheduled arrival matches an independent model');worlds++;}return panel;};
 function pause(){if(g.read('dispatch_running'))g.menu(1);assert.equal(g.read('dispatch_running'),0);}
 function run(){if(!g.read('dispatch_running'))g.menu(1);assert.equal(g.read('dispatch_running'),1);}
 function controls(upper,lower,route,letters=false){assert.equal(g.read('phase'),2,JSON.stringify(state(g)));pause();for(const [index,value] of [[0,upper],[1,lower],[2,route]]){const actual=index===2?g.read('dispatch_switch'):g.read('dispatch_gates',2)[index];if(actual===value)continue;while(g.read('dispatch_selected')!==index)g.tap(letters?'letter-d':'right');g.tap('space');assert.equal(index===2?g.read('dispatch_switch'):g.read('dispatch_gates',2)[index],value);}assert.deepEqual(g.read('dispatch_gates',2),[upper,lower]);assert.equal(g.read('dispatch_switch'),route);}
 async function until(predicate,stage=-1){run();let frames=0;while(!predicate()&&g.read('phase')===2){assert.ok(frames++<2400);g.frame();const s=state(g);if(stage===4&&s.positions.some(p=>p>=10&&p<=14)&&s.positions.includes(2)&&!second){await g.save('gameplay-2');second=true;}if(stage===10&&s.done>=5&&s.positions.some(p=>p>=10&&p<=19)&&!third){await g.save('gameplay-3');third=true;}}if(g.read('phase')===2)pause();}
 const frozen=state(g);for(let n=0;n<25;n++){const blink=g.read('cursor_blink_mask');g.frame();if(blink===g.read('cursor_blink_mask'))assert.equal(g.word('dirty_bytes'),0);else assert.ok(g.word('dirty_bytes')<=128,'Only the focus marker is transferred');}assert.deepEqual(state(g),frozen);
 await until(()=>{const p=g.read('dispatch_positions',4);return p.includes(2)&&p.includes(7);});controls(1,1,0);await until(()=>g.read('phase')===5);assert.equal(g.read('dispatch_reason'),1);g.tap('space');
 await until(()=>g.read('dispatch_positions',4).some(p=>p===2||p===7));let s=state(g),slot=s.positions.findIndex(p=>p===2||p===7),entry=s.positions[slot]===2?0:1;controls(entry===0?1:0,entry===1?1:0,1-s.targets[slot]);await until(()=>g.read('dispatch_positions',4)[slot]!==entry*5+2);controls(0,0,1-s.targets[slot]);await until(()=>g.read('phase')===5);assert.equal(g.read('dispatch_reason'),2);g.tap('space');
 await until(()=>g.read('phase')===5);assert.equal(g.read('dispatch_reason'),3);assert.equal(g.read('dispatch_time'),180);g.tap('space');
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.equal(g.read('dispatch_total'),schedules[stage].length);
  for(let shipment=0;shipment<schedules[stage].length;shipment++){
   controls(0,0,g.read('dispatch_switch'),stage%2===1);await until(()=>g.read('dispatch_positions',4).some(p=>p===2||p===7),stage);
   s=state(g);slot=s.positions.findIndex(p=>p===2||p===7);assert.ok(slot>=0);entry=s.positions[slot]===2?0:1;const dest=s.targets[slot];controls(entry===0?1:0,entry===1?1:0,dest,stage%2===1);
   await until(()=>g.read('dispatch_positions',4)[slot]!==entry*5+2,stage);controls(0,0,dest,stage%2===1);await until(()=>g.read('dispatch_done')===shipment+1,stage);assert.equal(g.read('dispatch_done'),shipment+1);assert.ok([2,4].includes(g.read('phase')),JSON.stringify(state(g)));
  }
  assert.equal(g.read('phase'),4,`Timetable ${stage+1}`);assert.ok(g.read('dispatch_time')<=180);assert.equal(g.read('dispatch_next_train'),schedules[stage].length);
 }
 assert.ok(second&&third&&worlds>1000);
}
