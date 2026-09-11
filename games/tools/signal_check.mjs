// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {letters} from './harness.mjs';
const V=[[0,-1],[1,-1],[1,0],[1,1],[0,1],[-1,1],[-1,0],[-1,-1]],D=[-14,14,-1,1],K=['up','down','left','right'];
const state=g=>({board:g.read('board',98),cameras:g.read('signal_cameras',3),bases:g.read('signal_bases',3),vision:g.read('signal_vision',98),p:g.read('cursor'),tick:g.read('signal_tick'),disabled:g.read('signal_disabled'),left:g.read('grid_stat'),moves:g.read('moves'),score:g.word('signal_score'),phase:g.read('phase')});
function sight(s){const visible=Array(98).fill(0);for(let a=0;a<3;a++){if(s.disabled&(a===1?2:1))continue;for(const side of [-1,0,1]){const [dx,dy]=V[((s.bases[a]+s.tick)*2+side+8)%8];let x=s.cameras[a]%14,y=Math.floor(s.cameras[a]/14);for(let n=0;n<4;n++){x+=dx;y+=dy;if(x<0||x>=14||y<0||y>=7)break;const p=y*14+x;if(s.board[p]===1||s.cameras.includes(p))break;visible[p]=1;}}}return visible;}
function action(s,k){if(k==='space'){const v=s.board[s.p];if(v===2||v===3){const bit=1<<(v-2);if(!(s.disabled&bit)){s.disabled|=bit;s.left--;s.score+=100;}}}else if(k!=='wait'){const p=s.p+D[K.indexOf(k)];if(s.board[p]===1||s.cameras.includes(p))return s;s.p=p;}
 s.moves=Math.min(255,s.moves+1);s.tick=(s.tick+1)%4;s.vision=sight(s);if(s.vision[s.p])s.phase=5;else if(s.disabled===3&&s.board[s.p]===4){s.phase=4;s.score+=Math.max(0,100-s.moves);}return s;}
export async function checkSignal(g){
 const levels=JSON.parse(await readFile(new URL('../signal-ghost/levels.json',import.meta.url))),solutions=JSON.parse(await readFile(new URL('../signal-ghost/solutions.json',import.meta.url)));await g.start();await g.save('gameplay-1');
 function key(k,letter=false){const expected=action(state(g),k);if(k==='wait')g.menu(1);else g.tap(letter&&K.includes(k)?letters[k]:k);assert.deepEqual(state(g),expected,'Camera cones, wall occlusion, synchronized rotation, network switches and scoring match the model');}
 const frozen=state(g);for(let n=0;n<25;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}assert.deepEqual(state(g),frozen);g.tap('return');for(let n=0;n<20;n++)g.frame();g.tap('return');assert.deepEqual(state(g),frozen);
 const expected=action(state(g),'space');g.hold('space',true);g.frame();assert.deepEqual(state(g),expected);for(let n=0;n<12;n++)g.frame();assert.deepEqual(state(g),expected,'Held SPACE advances only one turn');g.hold('space',false);g.frame();g.menu(2);
 let failed=false;
 for(const k of solutions[0]){const s=state(g),exposed=[0,1,2,3].some(t=>sight({...s,tick:t})[s.p]);if(exposed){for(let n=0;g.read('phase')===2;n++){assert.ok(n<4);key('wait');}failed=true;break;}key(k);}
 assert.ok(failed);assert.equal(g.read('phase'),5);g.tap('space');let second=false,third=false,waited=false;
 for(let stage=0;stage<20;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);assert.deepEqual(g.read('board',98),levels[stage].board);assert.deepEqual(g.read('signal_cameras',3),levels[stage].cameras);assert.deepEqual(g.read('signal_bases',3),levels[stage].bases);assert.deepEqual(g.read('signal_vision',98),sight(state(g)));
  for(let i=0;i<solutions[stage].length;i++){const k=solutions[stage][i];if(k==='space'){const v=g.read('board',98)[g.read('cursor')];if(![2,3].includes(v)||(g.read('signal_disabled')&(1<<(v-2))))waited=true;}key(k,stage%2===1);if(stage===7&&i===7){await g.save('gameplay-2');second=true;}if(stage===17&&[1,2].includes(g.read('signal_disabled'))&&!third){await g.save('gameplay-3');third=true;}}
  assert.equal(g.read('phase'),4,`Stealth route ${stage+1}`);assert.equal(g.read('signal_disabled'),3);assert.equal(g.read('grid_stat'),0);assert.equal(g.word('signal_score'),300-solutions[stage].length);
 }
 assert.ok(second&&third&&waited);
}
