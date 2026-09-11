// SPDX-License-Identifier: MIT
// Independent models drive keyboard input; no state is written into the machine.
import {readFile} from 'node:fs/promises';
import {checkBoard} from './board_check.mjs';
import {checkAce} from './ace_check.mjs';
import {checkSuit} from './suit_check.mjs';
import {checkDice} from './dice_check.mjs';
import {checkLuck} from './luck_check.mjs';
import {checkWall} from './wall_check.mjs';
import {checkTail} from './tail_check.mjs';
import {checkMaze} from './maze_check.mjs';
import {checkDock} from './dock_check.mjs';
import {checkBeat} from './beat_check.mjs';
import {checkPenalty} from './penalty_check.mjs';
import {checkRally} from './rally_check.mjs';
import {checkPutt} from './putt_check.mjs';
import {checkMarket} from './market_check.mjs';
import {checkOrchard} from './orchard_check.mjs';
import {checkDispatch} from './dispatch_check.mjs';
import {checkSignal} from './signal_check.mjs';
import {checkRogue} from './rogue_check.mjs';
import {checkEcho} from './echo_check.mjs';
import {checkQuest} from './quest_check.mjs';
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
 if(id==='relay-quest')return checkQuest(g);
 if(id==='echo-cavern')return checkEcho(g);
 if(id==='micro-rogue')return checkRogue(g);
 if(id==='signal-ghost')return checkSignal(g);
 if(id==='rail-dispatch')return checkDispatch(g);
 if(id==='orchard-days')return checkOrchard(g);
 if(id==='market-harbor')return checkMarket(g);
 if(id==='wind-putt')return checkPutt(g);
 if(id==='rally-return')return checkRally(g);
 if(id==='penalty-arc')return checkPenalty(g);
 if(id==='beat-step')return checkBeat(g);
 if(id==='balance-dock')return checkDock(g);
 return checkBoard(g,id);
}
