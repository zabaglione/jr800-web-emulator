// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {letters} from './harness.mjs';
const D=[-14,14,-1,1],K=['up','down','left','right'];
const state=g=>({board:g.read('board',98),p:g.read('cursor'),hp:g.read('rogue_hp'),max:g.read('rogue_max_hp'),sword:g.read('rogue_sword'),armor:g.read('rogue_armor'),pot:g.read('rogue_pot'),relic:g.read('rogue_relic'),left:g.read('rogue_left'),floor:g.read('rogue_floor'),room:g.read('rogue_room'),positions:g.read('rogue_positions',3),health:g.read('rogue_health',3),score:g.word('rogue_score'),moves:g.read('moves'),phase:g.read('phase'),seed:g.read('seed'),stage:g.read('stage')});
function distances(b,root){const d=Array(98).fill(255),q=[root];d[root]=0;for(let i=0;i<q.length;i++)for(const delta of D){const p=q[i]+delta;if(p>=0&&p<98&&b[p]!==1&&d[p]===255){d[p]=d[q[i]]+1;q.push(p);}}return d;}
function load(s,rooms){s.seed=(s.seed>>1)^((s.seed&1)?0xb8:0);s.room=s.seed%12;s.board=[...rooms[s.room]];s.positions=[];s.health=[];s.board.forEach((v,p)=>{if(v===7){s.positions.push(p);s.health.push(2+s.floor+(s.stage?1:0));s.board[p]=0;}});if(s.floor===4)s.health[0]=10+s.stage;s.p=15;s.moves=0;s.relic=0;s.left=3;return s;}
function enemies(s){const distance=distances(s.board,s.p);for(let actor=0;actor<3;actor++){if(!s.health[actor])continue;const origin=s.positions[actor],range=distance[origin];if(range>=7)continue;if(range===1){const damage=Math.max(1,(s.floor===4&&actor===0?5:2+Math.floor(s.floor/2))-s.armor);s.hp=Math.max(0,s.hp-damage);if(!s.hp){s.phase=5;return s;}continue;}let best=origin,bestDistance=range;for(const delta of D){const p=origin+delta;if(distance[p]<bestDistance&&!s.positions.some((q,i)=>s.health[i]&&q===p)){best=p;bestDistance=distance[p];}}s.positions[actor]=best;}return s;}
function action(s,key,rooms){
 if(key==='space'){if(!s.pot||s.hp===s.max)return s;s.pot--;s.hp=Math.min(s.max,s.hp+8);}
 else if(key!=='wait'){
  const p=s.p+D[K.indexOf(key)],v=s.board[p];if(v===1||v===undefined)return s;const actor=s.positions.findIndex((q,i)=>q===p&&s.health[i]);
  if(actor>=0){s.health[actor]=Math.max(0,s.health[actor]-s.sword);if(!s.health[actor]){s.left--;s.score+=20;}}
  else{s.p=p;if(v===2){s.sword=Math.min(5,s.sword+1);s.board[p]=0;}if(v===3){s.armor=Math.min(2,s.armor+1);s.board[p]=0;}if(v===4&&s.pot<5){s.pot++;s.board[p]=0;}if(v===5){s.relic=1;s.score+=100;s.board[p]=0;}if(v===9){s.score+=10;s.board[p]=0;}
   if(v===6&&s.relic&&!s.left){if(s.floor===4){s.phase=4;s.score+=s.hp*5;return s;}s.floor++;return load(s,rooms);}}
 }
 s.moves=Math.min(255,s.moves+1);return enemies(s);
}
function choice(s){
 const adjacent=s.positions.map((p,i)=>({p,i,hp:s.health[i]})).filter(a=>a.hp&&D.includes(a.p-s.p)).sort((a,b)=>a.hp-b.hp);
 if(s.pot&&(s.hp<=s.max-8||s.hp<=6))return 'space';
 if(adjacent.length)return K[D.indexOf(adjacent[0].p-s.p)];
 const distance=distances(s.board,s.p);let goals=s.board.map((v,p)=>({v,p,d:distance[p]})).filter(t=>t.v===2&&s.sword<5||t.v===3&&s.armor<2||t.v===4&&s.pot<2);
 if(!goals.length)goals=s.positions.map((p,i)=>({p,d:distance[p],hp:s.health[i]})).filter(t=>t.hp);
 if(!goals.length)goals=s.board.map((v,p)=>({v,p,d:distance[p]})).filter(t=>t.v===(s.relic?6:5));
 goals.sort((a,b)=>a.d-b.d);assert.ok(goals.length);const target=goals[0].p,back=distances(s.board,target);let best=-1;
 for(let i=0;i<4;i++){const p=s.p+D[i];if(back[p]<back[s.p]&&(best<0||back[p]<back[s.p+D[best]]))best=i;}
 assert.ok(best>=0);return K[best];
}
export async function checkRogue(g){
 const rooms=JSON.parse(await readFile(new URL('../micro-rogue/rooms.json',import.meta.url)));await g.start();await g.save('gameplay-1');let count=0,second=false,third=false,healed=false;const layouts=new Set();
 function key(k,letter=false){const expected=action(state(g),k,rooms);if(k==='wait')g.menu(1);else g.tap(letter&&K.includes(k)?letters[k]:k);assert.deepEqual(state(g),expected,'Dungeon generation, equipment, combat, ordered pursuit and floor transitions match independent rules');count++;}
 const frozen=state(g);for(let n=0;n<25;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),frozen);g.tap('return');for(let n=0;n<20;n++)g.frame();g.tap('return');assert.deepEqual(state(g),frozen);
 // Walk towards a guard then wait through actual pursuit and fatal attacks.
 for(let n=0;g.read('phase')===2;n++){assert.ok(n<150);const s=state(g),d=distances(s.board,s.p),nearest=s.positions.filter((p,i)=>s.health[i]).sort((a,b)=>d[a]-d[b])[0];if(d[nearest]<=6)key('wait');else{const back=distances(s.board,nearest),dir=D.findIndex(v=>back[s.p+v]<back[s.p]);key(K[dir]);}}assert.equal(g.read('phase'),5);g.tap('space');
 for(let difficulty=0;difficulty<3;difficulty++){
  if(difficulty)g.tap('space');assert.equal(g.read('stage'),difficulty);
  let turns=0;
  while(g.read('phase')===2){assert.ok(turns++<700,`No stalled run at ${JSON.stringify(state(g))}`);const before=state(g);layouts.add(before.room);const k=choice(before);if(k==='space')healed=true;key(k,difficulty%2===1);
   if(difficulty===1&&g.read('rogue_floor')===2&&g.read('rogue_left')<3&&!second){await g.save('gameplay-2');second=true;}
   if(difficulty===2&&g.read('rogue_floor')===4&&g.read('rogue_left')===1&&!third){await g.save('gameplay-3');third=true;}
  }
  assert.equal(g.read('phase'),4,`Five-floor run on difficulty ${difficulty+1}`);assert.equal(g.read('rogue_floor'),4);assert.equal(g.read('rogue_relic'),1);assert.equal(g.read('rogue_left'),0);
 }
 assert.ok(second&&third&&healed&&layouts.size>=6);return {count,rooms:layouts.size};
}
