// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {letters} from './harness.mjs';
const D=[-14,14,-1,1],K=['up','down','left','right'];
const state=g=>({board:g.read('board',98),p:g.read('cursor'),aim:g.read('quest_aim'),hp:g.read('quest_hp'),keys:g.read('quest_keys'),pot:g.read('quest_pot'),left:g.read('quest_left'),score:g.word('quest_score'),turns:g.read('moves'),phase:g.read('phase')});
function action(s,key){
 const b=s.board,p=s.p;let turn=false;
 if(key==='heal'){if(s.pot&&s.hp<9){s.pot--;s.hp=Math.min(9,s.hp+3);turn=true;}}
 else if(key==='space'){const q=p+D[s.aim],v=b[q];if(v>=7){b[q]=v===9?7:0;if(!b[q])s.score+=25;turn=true;}}
 else{const d=K.indexOf(key);s.aim=d;const q=p+D[d],v=b[q];if(v===1||v>=7||(v===3&&!s.keys))return s;s.p=q;turn=true;
  if(v===2){s.keys++;b[q]=0;}if(v===3){s.keys--;b[q]=0;}if(v===4){s.pot++;b[q]=0;}if(v===5){s.left--;s.score+=100;b[q]=0;}if(v===6&&!s.left){s.phase=4;s.score+=s.hp*10;return s;}}
 if(turn){s.turns=Math.min(255,s.turns+1);s.hp=Math.max(0,s.hp-D.filter(d=>b[s.p+d]>=7).length);if(!s.hp)s.phase=5;}return s;
}
export async function checkQuest(g){
 const levels=JSON.parse(await readFile(new URL('../relay-quest/levels.json',import.meta.url))),solutions=JSON.parse(await readFile(new URL('../relay-quest/solutions.json',import.meta.url)));await g.start();await g.save('gameplay-1');
 function key(k,letter=false){const expected=action(state(g),k);if(k==='heal')g.menu(1);else g.tap(letter&&K.includes(k)?letters[k]:k);assert.deepEqual(state(g),expected,'Keys, medicine, locks, relays, damage and combat match independent rules');}
 const frozen=state(g);for(let n=0;n<35;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),frozen);g.tap('return');for(let n=0;n<25;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}g.tap('return');assert.deepEqual(state(g),frozen);
 // Reach the first guard, then repeatedly step away and return without attacking.
 const first=solutions[0],attack=first.indexOf('space');for(const k of first.slice(0,attack))key(k);
 const p=g.read('cursor'),out=['up','down','left'].find(k=>{const v=g.read('board',98)[p+D[K.indexOf(k)]];return v===0;});assert.ok(out);const back={up:'down',down:'up',left:'right'}[out];
 for(let n=0;g.read('phase')===2;n++){assert.ok(n<40);key(n%2?back:out);}assert.equal(g.read('phase'),5);g.tap('space');assert.deepEqual(g.read('board',98),levels[0]);
 let healed=false,second=false,third=false;
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('board',98),levels[stage]);
  for(let i=0;i<solutions[stage].length;i++){
   const k=solutions[stage][i];key(k,stage%2===1);if(k==='heal')healed=true;
   if(stage===4&&k==='space'&&!second){await g.save('gameplay-2');second=true;}
   if(stage===10&&g.read('quest_left')===0&&g.read('phase')===2&&!third){await g.save('gameplay-3');third=true;}
  }
  assert.equal(g.read('phase'),4,`Ruin ${stage+1}`);assert.equal(g.read('quest_left'),0);assert.ok(g.read('quest_hp')>0);
 }
 assert.ok(healed&&second&&third);
}
