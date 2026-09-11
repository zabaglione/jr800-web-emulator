// SPDX-License-Identifier: MIT
// Independent models drive keyboard input; no state is written into the machine.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {letters} from './harness.mjs';
import {checkBoard} from './board_check.mjs';
import {checkPipe} from './pipe_check.mjs';
import {checkRail} from './rail_check.mjs';
import {checkMine} from './mine_check.mjs';
import {checkLoop} from './loop_check.mjs';
import {checkAce} from './ace_check.mjs';
import {checkSuit} from './suit_check.mjs';
import {checkDice} from './dice_check.mjs';
import {checkLuck} from './luck_check.mjs';
import {checkWall} from './wall_check.mjs';
import {checkTail} from './tail_check.mjs';
import {checkMaze} from './maze_check.mjs';
import {checkRico} from './rico_check.mjs';
import {checkRange} from './range_check.mjs';
import {checkOrbit} from './orbit_check.mjs';
import {checkStar} from './star_check.mjs';
import {checkGravity} from './gravity_check.mjs';
import {checkClaim} from './claim_check.mjs';
import {checkBomb} from './bomb_check.mjs';
import {checkTower} from './tower_check.mjs';
import {checkRiver} from './river_check.mjs';
export async function data(id,name='solutions') {return JSON.parse(await readFile(new URL(`../${id}/${name}.json`,import.meta.url)));}
export function moveCursor(g,cell,w){
 while(g.read('cursor')%w<cell%w)g.tap('right');
 while(g.read('cursor')%w>cell%w)g.tap('left');
 while(Math.floor(g.read('cursor')/w)<Math.floor(cell/w))g.tap('down');
 while(Math.floor(g.read('cursor')/w)>Math.floor(cell/w))g.tap('up');
}
export async function checkLibrary(g,id){
 if(id==='pipe-weave')return checkPipe(g);
 if(id==='number-rail')return checkRail(g);
 if(id==='mine-field')return checkMine(g);
 if(id==='loop-trace')return checkLoop(g);
 if(id==='ace-stack')return checkAce(g);
 if(id==='suit-run')return checkSuit(g);
 if(id==='dice-hold')return checkDice(g);
 if(id==='push-luck')return checkLuck(g);
 if(id==='wall-break')return checkWall(g);
 if(id==='tail-trail')return checkTail(g);
 if(id==='maze-chase')return checkMaze(g);
 if(id==='river-hop')return checkRiver(g);
 if(id==='tower-leap')return checkTower(g);
 if(id==='bomb-vault')return checkBomb(g);
 if(id==='grid-claim')return checkClaim(g);
 if(id==='gravity-run')return checkGravity(g);
 if(id==='star-patrol')return checkStar(g);
 if(id==='orbit-guard')return checkOrbit(g);
 if(id==='target-range')return checkRange(g);
 if(id==='ricochet-ops')return checkRico(g);
 if(id==='lamp-grid'){
  const solutions=await data(id);
  for(let stage=0;stage<solutions.length;stage++){
   if(!stage)await g.start();else g.tap('space');
   assert.equal(g.read('stage'),stage);
   g.frame();assert.equal(g.word('dirty_bytes'),0,'Idle grid sends no LCD data');
   if(!stage){
    await g.save('gameplay-1');const before=g.read('board',25);
    g.tap('space');g.menu(1);assert.deepEqual(g.read('board',25),before,'Undo restores all five neighbours');assert.equal(g.read('moves'),0);
    g.tap('right');const c=g.read('cursor');g.tap('left');g.tap(letters.right);assert.equal(g.read('cursor'),c);
    g.menu(2);assert.deepEqual(g.read('board',25),before,'Reset restores the puzzle');
    for(let i=0;i<260;i++)g.tap('space');
    assert.equal(g.read('moves'),255);g.menu(1);assert.equal(g.read('moves'),255,'Undo restores the saturated counter');g.menu(2);
   }
   for(let i=0;i<solutions[stage].length;i++){
    const cell=solutions[stage][i];moveCursor(g,cell,5);
    const expected=g.read('board',25);
    for(const q of [cell,cell-5,cell+5,cell%5?cell-1:-1,cell%5<4?cell+1:-1])if(q>=0&&q<25)expected[q]^=1;
    g.tap('space');assert.deepEqual(g.read('board',25),expected,'Exactly the cross toggles');
    if(stage===7&&i===1)await g.save('gameplay-2');
    if(stage===16&&i===2)await g.save('gameplay-3');
   }
   assert.equal(g.read('phase'),4,`Lamp puzzle ${stage+1}`);assert.equal(g.read('grid_stat'),0);
  }
 }else if(id==='slide-nine'){
  const solutions=await data(id);
  for(let stage=0;stage<20;stage++){
   if(!stage)await g.start();else g.tap('space');
   assert.equal(g.read('stage'),stage);g.frame();assert.equal(g.word('dirty_bytes'),0);
   if(!stage){
    await g.save('gameplay-1');const before=g.read('board',9),blank=g.read('cursor');
    g.tap(solutions[0][0]);g.menu(1);assert.deepEqual(g.read('board',9),before);assert.equal(g.read('cursor'),blank);
    g.tap(letters[solutions[0][0]]);g.menu(2);assert.deepEqual(g.read('board',9),before);
    const first=solutions[0][0],back={up:'down',down:'up',left:'right',right:'left'}[first];
    for(let i=0;i<260;i++)g.tap(i%2?back:first);
    assert.equal(g.read('moves'),255);g.menu(1);assert.equal(g.read('moves'),255,'Undo at the display limit');g.menu(2);
   }
   for(let i=0;i<solutions[stage].length;i++){
    const key=solutions[stage][i],old=g.read('cursor'),board=g.read('board',9),next=old+({up:-3,down:3,left:-1,right:1})[key];
    [board[old],board[next]]=[board[next],board[old]];g.tap(key);
    assert.deepEqual(g.read('board',9),board,'Only the tile next to the blank moves');
    if(stage===8&&i===3)await g.save('gameplay-2');
    if(stage===18&&i===5)await g.save('gameplay-3');
   }
   assert.equal(g.read('phase'),4,`Slide puzzle ${stage+1}`);assert.deepEqual(g.read('board',9),[1,2,3,4,5,6,7,8,0]);
  }
 }else if(id==='ice-route'){
  const solutions=await data(id),levels=await data(id,'levels');
  for(let stage=0;stage<20;stage++){
   if(!stage)await g.start();else g.tap('space');assert.equal(g.read('stage'),stage);
   let board=[...levels[stage].board],p=levels[stage].start,gems=2;
   if(!stage){
    await g.save('gameplay-1');g.tap(solutions[0][0]);g.menu(1);
    assert.deepEqual(g.read('board',98),board);assert.equal(g.read('cursor'),p);assert.equal(g.read('grid_stat'),2);
    g.frame();assert.equal(g.word('dirty_bytes'),0);
   }
   for(let i=0;i<solutions[stage].length;i++){
    const key=solutions[stage][i],d={up:-14,down:14,left:-1,right:1}[key];
    while(board[p+d]!==1){p+=d;if([3,4].includes(board[p])){board[p]=0;gems--;}if(board[p]===2&&!gems)break;}
    g.tap(key);assert.equal(g.read('cursor'),p);assert.equal(g.read('grid_stat'),gems);assert.deepEqual(g.read('board',98),board);
    if(stage===6&&i===1)await g.save('gameplay-2');
    if(stage===17&&i===3)await g.save('gameplay-3');
   }
   assert.equal(g.read('phase'),4,`Ice puzzle ${stage+1}`);
  }
 }else if(id==='switch-maze'){
  const solutions=await data(id),levels=await data(id,'levels');let undoSwitch=false,closedDoor=false;
  for(let stage=0;stage<20;stage++){
   if(!stage)await g.start();else g.tap('space');assert.equal(g.read('stage'),stage);
   let board=[...levels[stage].board],p=levels[stage].start,keys=2,mask=0;
   if(!stage){await g.save('gameplay-1');g.frame();assert.equal(g.word('dirty_bytes'),0);}
   for(let i=0;i<solutions[stage].length;i++){
    for(const [key,d] of Object.entries({up:-14,down:14,left:-1,right:1})){
     const cell=board[p+d];if([6,7].includes(cell)&&!(mask&(1<<(cell-6)))){
      const moves=g.read('moves');g.tap(key);assert.equal(g.read('cursor'),p);assert.equal(g.read('moves'),moves);closedDoor=true;
     }
    }
    const key=solutions[stage][i],d={up:-14,down:14,left:-1,right:1}[key],previous=p,oldMask=mask;
    p+=d;const cell=board[p];if([2,3].includes(cell)){board[p]=0;keys--;}if([4,5].includes(cell))mask^=1<<(cell-4);
    g.tap(key);
    if([4,5].includes(cell)&&!undoSwitch){g.menu(1);assert.equal(g.read('gates'),oldMask);assert.equal(g.read('cursor'),previous);g.tap(key);undoSwitch=true;}
    assert.equal(g.read('cursor'),p);assert.equal(g.read('gates'),mask);assert.equal(g.read('grid_stat'),keys);assert.deepEqual(g.read('board',98),board);
    if(stage===7&&i===8)await g.save('gameplay-2');if(stage===16&&i===16)await g.save('gameplay-3');
   }
   assert.equal(g.read('phase'),4,`Switch puzzle ${stage+1}`);
  }
  assert.ok(undoSwitch&&closedDoor,'Switch undo and closed-door blocking were exercised');
 }else await checkBoard(g,id);
}
