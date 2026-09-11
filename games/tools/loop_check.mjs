// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {letters} from './harness.mjs';
const directions=[[0,-1,'up',1,2],[0,1,'down',2,1],[-1,0,'left',4,8],[1,0,'right',8,4]];
function direction(p,q){return directions.find(([dx,dy])=>q%6-p%6===dx&&Math.floor(q/6)-Math.floor(p/6)===dy);}
function verify(g,board,path,closed=false){
 const links=Array(36).fill(0),marks=Array(36).fill(0);for(const p of path)marks[p]=1;
 const line=closed?[...path,path[0]]:path;
 for(let i=1;i<line.length;i++){const d=direction(line[i-1],line[i]);assert.ok(d);links[line[i-1]]|=d[3];links[line[i]]|=d[4];}
 assert.deepEqual(g.read('board',36),board);assert.deepEqual(g.read('loop_marks',36),marks);assert.deepEqual(g.read('loop_paths',36),links);
 assert.equal(g.read('cursor'),path.at(-1));assert.equal(g.read('moves'),path.length-1);assert.equal(g.read('grid_stat'),board.filter(Boolean).length-path.length);
 assert.equal(g.read('loop_next'),1+path.filter(p=>board[p]>=2&&board[p]<=4).length);assert.equal(g.read('phase'),closed?4:2);
}
export async function checkLoop(g){
 const levels=JSON.parse(await readFile(new URL('../loop-trace/solutions.json',import.meta.url)));await g.start();let blockedCheckpoint=0,checkpointUndo=false;
 for(let stage=0;stage<20;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);const board=levels.boards[stage],route=levels.paths[stage];let path=[route[0]];verify(g,board,path);
  if(!stage){
   await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);g.tap('space');verify(g,board,path);
   const d=direction(route[0],route[1]);g.tap(d[2]);g.tap(direction(route[1],route[0])[2]);verify(g,board,path);
   g.tap(letters[d[2]]);g.menu(1);verify(g,board,path);
  }
  for(let i=1;i<route.length;i++){
   const head=path.at(-1);
   for(const [dx,dy,key] of directions){
    const x=head%6+dx,y=Math.floor(head/6)+dy,q=y*6+x;if(x<0||x>=6||y<0||y>=6||!board[q]){g.tap(key);verify(g,board,path);}
    else if(!path.includes(q)&&board[q]>=2&&board[q]<=4&&board[q]-1!==g.read('loop_next')){g.tap(key);verify(g,board,path);blockedCheckpoint++;}
   }
   const p=route[i],d=direction(head,p);assert.ok(d);g.tap(d[2]);path.push(p);verify(g,board,path);
   if(!checkpointUndo&&board[p]===2){g.menu(1);verify(g,board,path.slice(0,-1));g.tap(d[2]);verify(g,board,path);checkpointUndo=true;}
   if(stage===9&&i===8)await g.save('gameplay-2');
  }
  assert.equal(g.read('grid_stat'),0);assert.equal(g.read('phase'),2,'Visiting every node still requires closing the loop');g.tap('space');verify(g,board,path,true);
  if(stage===19)await g.save('gameplay-3');
 }
 assert.ok(blockedCheckpoint&&checkpointUndo,'Checkpoint order and backtracking were exercised');
}
