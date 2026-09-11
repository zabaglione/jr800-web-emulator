// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const keys=['up','down','left','right'],delta=[-15,15,-1,1];
function neighbor(p,d){const x=p%15+(d===2?-1:d===3?1:0),y=Math.floor(p/15)+(d===0?-1:d===1?1:0);return x>=0&&x<15&&y>=0&&y<7?y*15+x:255;}
function distances(board,start){const dist=Array(105).fill(255),q=[start];dist[start]=0;for(let i=0;i<q.length;i++)for(let d=0;d<4;d++){const n=neighbor(q[i],d);if(n!==255&&board[n]!==1&&dist[n]===255){dist[n]=dist[q[i]]+1;q.push(n);}}return dist;}
function reset(s){s.p=16;s.ghosts=[88,76];s.sleep=[0,0];s.power=32;s.ticks=0;s.running=0;s.direction=3;s.queued=3;}
function fresh(board){const s={board:[...board],lives:3,score:0,phase:2};s.board[16]=0;reset(s);return s;}
function contact(s){for(let i=0;i<2;i++)if(!s.sleep[i]&&s.ghosts[i]===s.p){if(s.power){s.sleep[i]=2;s.ghosts[i]=[88,76][i];s.score+=50;}else{if(--s.lives)reset(s);else{s.running=0;s.phase=5;}return;}}}
function world(s){if(s.power)s.power--;let n=neighbor(s.p,s.queued);if(n!==255&&s.board[n]!==1)s.direction=s.queued;else n=neighbor(s.p,s.direction);
 if(n!==255&&s.board[n]!==1){s.p=n;if(s.board[n]>=2){s.score+=s.board[n]===3?20:10;if(s.board[n]===3)s.power=32;s.board[n]=0;}}
 contact(s);if(!s.running)return;if(!s.board.some(n=>n>=2)){s.running=0;s.phase=4;return;}if(++s.ticks<3)return;s.ticks=0;const dist=distances(s.board,s.p);
 for(let i=0;i<2;i++){if(s.sleep[i]){s.sleep[i]--;continue;}let best=s.ghosts[i],value=s.power?0:255;for(let d=0;d<4;d++){const n=neighbor(s.ghosts[i],d);if(n===255||dist[n]===255)continue;if(s.power?dist[n]>=value:dist[n]<value){best=n;value=dist[n];}}s.ghosts[i]=best;}contact(s);
}
function inspect(g,s){assert.deepEqual(g.read('board',105),s.board);assert.equal(g.read('cursor'),s.p);assert.equal(g.read('grid_stat'),s.board.filter(n=>n>=2).length);assert.equal(g.read('maze_lives'),s.lives);assert.equal(g.read('maze_power'),s.power);assert.deepEqual(g.read('maze_ghosts',2),s.ghosts);assert.deepEqual(g.read('maze_sleep',2),s.sleep);assert.equal(g.read('maze_ticks'),s.ticks);assert.equal(g.word('maze_score'),s.score);assert.equal(g.read('phase'),s.phase);assert.equal(g.read('maze_running'),s.running);assert.deepEqual(g.read('maze_distance',105),distances(s.board,s.p));}
function frame(g,s,key){const before=g.read('maze_clock'),resuming=g.read('resume_pending');if(key==='space')s.running=1;else if(key)s.queued=keys.indexOf(key);g.frame();if(!resuming&&before!==g.read('maze_clock'))world(s);inspect(g,s);return !resuming&&before!==g.read('maze_clock');}
function pulse(g,s,key){g.hold(key,true);frame(g,s,key);g.hold(key,false);frame(g,s);}
function choose(s){const dist=distances(s.board,s.p),food=Array.from({length:105},(_,p)=>p).filter(p=>s.board[p]>=2);let targets=food;if(s.power<14){const powers=food.filter(p=>s.board[p]===3&&dist[p]<28);if(powers.length)targets=powers;}targets.sort((a,b)=>dist[a]-dist[b]||a-b);const target=targets[0],toTarget=distances(s.board,target);let best=null;
 for(let d=0;d<4;d++){const t=structuredClone(s);t.queued=d;world(t);if(t.lives<s.lives)continue;const score=(s.board[t.p]>=2?20:0)-toTarget[t.p]+(t.power&&!s.power?30:0);if(!best||score>best.score)best={d,score};}return best?.d??0;
}
export async function checkMaze(g){const levels=JSON.parse(await readFile(new URL('../maze-chase/levels.json',import.meta.url)));await g.start();const losing=fresh(levels[0]);
 for(let frames=0;losing.phase===2;frames++){assert.ok(frames<8000);if(!losing.running)pulse(g,losing,'space');else frame(g,losing);}
 assert.equal(losing.phase,5);g.tap('space');let second=false,third=false;
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);const s=fresh(levels[stage]);inspect(g,s);if(!stage){await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);}pulse(g,s,'space');
  for(let turns=0;s.phase===2;turns++){
   assert.ok(turns<600,`Maze ${stage+1}`);if(!s.running)pulse(g,s,'space');const d=choose(s);g.hold(keys[d],true);let moved=false;
   for(let f=0;f<80;f++){if(frame(g,s,keys[d])){moved=true;break;}}g.hold(keys[d],false);assert.ok(moved);
   if(stage===0&&turns===10){g.menu(1);s.running=0;inspect(g,s);for(let n=0;n<40;n++){g.frame();assert.equal(g.word('dirty_bytes'),0);}inspect(g,s);pulse(g,s,'space');}
   if(stage===4&&!second&&s.board.filter(n=>n>=2).length<24){await g.save('gameplay-2');second=true;}
   if(stage===11&&!third&&s.board.filter(n=>n>=2).length<10){await g.save('gameplay-3');third=true;}
  }
  g.frame();assert.equal(s.phase,4,`Maze ${stage+1} ended with ${s.board.filter(n=>n>=2).length} dots`);console.log(JSON.stringify({stage:stage+1,score:s.score,lives:s.lives}));
 }assert.ok(second&&third);
}
