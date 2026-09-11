// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {letters} from './harness.mjs';
const D=[-14,14,-1,1],K=['up','down','left','right'];
const state=g=>({board:g.read('board',98),known:g.read('echo_known',98),p:g.read('cursor'),air:g.read('echo_air'),left:g.read('echo_left'),score:g.word('echo_score'),mapped:g.read('echo_mapped'),moves:g.read('moves'),phase:g.read('phase')});
function reveal(s,p){if(!s.known[p]){s.known[p]=1;s.mapped++;}}
function ping(s){const q=[[s.p,0]],seen=new Set([s.p]);reveal(s,s.p);for(let i=0;i<q.length;i++){const [p,d]=q[i];for(const [dx,dy] of [[0,-1],[0,1],[-1,0],[1,0]]){const x=p%14+dx,y=Math.floor(p/14)+dy;if(x<0||x>=14||y<0||y>=7)continue;const n=y*14+x;if(seen.has(n))continue;seen.add(n);reveal(s,n);if(d+1<3&&s.board[n]!==1)q.push([n,d+1]);}}return s;}
function action(s,k){if(k==='space'){s.air=Math.max(0,s.air-2);if(!s.air){s.phase=5;return s;}return ping(s);}const q=s.p+D[K.indexOf(k)],v=s.board[q];reveal(s,q);if(v===1)return s;s.p=q;s.moves=Math.min(255,s.moves+1);s.air=Math.max(0,s.air-(v===4?3:1));if(!s.air){s.phase=5;return s;}if(v===2){s.left--;s.score+=100;s.board[q]=0;}if(v===3){s.air=Math.min(99,s.air+24);s.board[q]=0;}if(v===5&&!s.left){s.phase=4;s.score+=s.air*5;}return s;}
export async function checkEcho(g){
 const levels=JSON.parse(await readFile(new URL('../echo-cavern/levels.json',import.meta.url))),solutions=JSON.parse(await readFile(new URL('../echo-cavern/solutions.json',import.meta.url)));await g.start();await g.save('gameplay-1');
 function key(k,letter=false,menu=false){const before=state(g),expected=action(before,k);if(menu)g.menu(1);else g.tap(letter&&K.includes(k)?letters[k]:k);assert.deepEqual(state(g),expected,'Sound propagation, discovery, hazards, air, gems and score match independent rules');}
 const frozen=state(g);for(let i=0;i<30;i++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),frozen);
 key('space',false,true);g.menu(2);assert.deepEqual(state(g),frozen);
 // Repeated sonar uses real oxygen and eventually fails even on a known map.
 for(let n=0;g.read('phase')===2;n++){assert.ok(n<40);key('space');}assert.equal(g.read('phase'),5);g.tap('space');
 let second=false,third=false,spikes=0,tanks=0;
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);const initial={board:levels[stage],known:Array(98).fill(0),p:15,mapped:0};ping(initial);assert.deepEqual(g.read('echo_known',98),initial.known,'Initial free sonar obeys wall occlusion');
  for(let i=0;i<solutions[stage].length;i++){
   const k=solutions[stage][i];if(K.includes(k)){const v=g.read('board',98)[g.read('cursor')+D[K.indexOf(k)]];if(v===4)spikes++;if(v===3)tanks++;}key(k,stage%2===1);
   if(stage===4&&k==='space'&&!second){await g.save('gameplay-2');second=true;}
   if(stage===10&&!g.read('echo_left')&&g.read('phase')===2&&!third){await g.save('gameplay-3');third=true;}
  }
  assert.equal(g.read('phase'),4,`Cavern ${stage+1}`);assert.ok(g.read('echo_air')>0);
 }
 assert.ok(second&&third&&spikes&&tanks);
}
