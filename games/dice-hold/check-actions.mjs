// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from '../tools/harness.mjs';
const [wasm,out]=process.argv.slice(2),g=await Game.open(wasm,out,'dice-hold');
try {
 await g.start();g.tap('space');const kept=g.read('board',5)[0];
 g.tap('down');assert.equal(g.read('dice_action'),1);await g.save('action-roll');
 for(let i=0;i<2;i++){const left=g.read('dice_rolls');g.tap('space');assert.equal(g.read('board',5)[0],kept);assert.equal(g.read('dice_rolls'),left-1);}
 const before=g.read('board',5),seed=g.read('seed');g.tap('space');assert.deepEqual(g.read('board',5),before);assert.equal(g.read('seed'),seed);
 g.tap('right');assert.equal(g.read('dice_action'),2);await g.save('action-score');
 g.tap('return');assert.equal(g.read('phase'),3);g.tap('return');assert.equal(g.read('dice_action'),2);
 g.tap('space');assert.equal(g.read('selection_active'),1);g.tap('return');assert.equal(g.read('selection_active'),0);
 g.tap('up');assert.equal(g.read('dice_action'),0);g.tap('right');assert.equal(g.read('cursor'),1);g.tap('space');assert.equal(g.read('dice_holds',5)[1],1);
 g.tap('down');g.tap('right');g.tap('space');const points=g.read('dice_scores',13)[0];g.tap('space');assert.equal(g.read('dice_round'),1);assert.equal(g.word('dice_total'),points);assert.equal(g.read('dice_action'),0);
 g.frame();assert.equal(g.word('dirty_bytes'),0);
 const result={passed:true,mainActions:['ROLL','SCORE'],heldDiePreserved:true,exhaustedRollInert:true,menuResume:true,frames:g.frames};
 await writeFile(resolve(out,'actions-verification.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
}finally{g.destroy();}
