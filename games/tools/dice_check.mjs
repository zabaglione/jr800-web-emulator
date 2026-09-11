// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
export function diceScores(hand){const c=Array(7).fill(0);for(const n of hand)c[n]++;const sum=hand.reduce((a,b)=>a+b),max=Math.max(...c);let run=0,longest=0;for(let n=1;n<=6;n++){run=c[n]?run+1:0;longest=Math.max(longest,run);}return [...c.slice(1).map((n,i)=>n*(i+1)),max>=3?sum:0,max>=4?sum:0,c.includes(2)&&c.includes(3)?25:0,longest>=4?30:0,longest>=5?40:0,max===5?50:0,sum];}
function rolled(hand,seed,mask){const h=[...hand];for(let i=0;i<5;i++)if(!(mask&(1<<i))){do{seed=(seed>>1)^((seed&1)?184:0);}while(seed>=253);h[i]=(seed-1)%6+1;}return {hand:h,seed};}
function inspect(g){assert.deepEqual(g.read('dice_scores',13),diceScores(g.read('board',5)));}
function holds(g,mask){for(let i=0;i<5;i++)if(g.read('dice_holds',5)[i]!==((mask>>i)&1)){while(g.read('cursor')<i)g.tap('right');while(g.read('cursor')>i)g.tap('left');g.tap('space');}assert.deepEqual(g.read('dice_holds',5),Array.from({length:5},(_,i)=>(mask>>i)&1));}
function roll(g,mask){holds(g,mask);const seed=g.read('seed'),before=g.read('board',5),left=g.read('dice_rolls'),expect=rolled(before,seed,mask);g.menu(1);assert.equal(g.read('dice_rolls'),Math.max(0,left-1));assert.deepEqual(g.read('board',5),left?expect.hand:before);assert.equal(g.read('seed'),left?expect.seed:seed);inspect(g);}
function aim(g,category){while(g.read('dice_category')!==category)g.tap('down');}
function score(g,category){g.menu(2);aim(g,category);const points=g.read('dice_scores',13)[category],total=g.word('dice_total'),upper=g.read('dice_upper'),bonus=g.read('dice_bonus'),round=g.read('dice_round');g.tap('space');const newUpper=upper+(category<6?points:0),addBonus=!bonus&&newUpper>=63?35:0;assert.equal(g.word('dice_total'),total+points+addBonus);assert.equal(g.read('dice_upper'),newUpper);assert.equal(g.read('dice_round'),round+1);assert.equal(g.read('dice_used',13)[category],1);assert.equal(g.read('dice_saved',13)[category],points);inspect(g);}
function best(g){const used=g.read('dice_used',13),base=g.read('board',5),seed=g.read('seed'),left=g.read('dice_rolls');let best=null;
 const consider=(h,s,path)=>{const points=diceScores(h);for(let c=0;c<13;c++)if(!used[c]){const v=c<6?points[c]/(c+1)*8+(c+1)*.05:points[c]-(c===11?12:c===10?8:c===9?4:c===12?16:0);if(!best||v>best.value)best={value:v,path,c};}};
 consider(base,seed,[]);if(left)for(let m=0;m<32;m++){const a=rolled(base,seed,m);consider(a.hand,a.seed,[m]);if(left>1)for(let n=0;n<32;n++){const b=rolled(a.hand,a.seed,n);consider(b.hand,b.seed,[m,n]);}}return best;
}
export async function checkDice(g){await g.start();await g.save('gameplay-1');inspect(g);g.frame();assert.equal(g.word('dirty_bytes'),0);
 // All-held throws consume one allowance, never a random draw. A fourth throw is inert.
 roll(g,31);roll(g,31);roll(g,0);g.menu(2);const preview=g.read('dice_scores',13);g.tap('return');assert.equal(g.read('selection_active'),0);assert.deepEqual(g.read('dice_scores',13),preview);
 // Deliberately fill every column without rerolls to exercise a failed scorecard.
 for(let c=0;c<13;c++)score(g,c);assert.equal(g.read('phase'),5);g.tap('space');assert.equal(g.read('phase'),2);
 let second=false,third=false,bonus=false,duplicate=false;
 for(let stage=0;stage<3;stage++){
  if(stage)g.tap('space');assert.equal(g.read('stage'),stage);
  for(let round=0;round<13;round++){
   const plan=best(g);for(const mask of plan.path)roll(g,mask);
   if(!second&&stage===1&&round===5){await g.save('gameplay-2');second=true;}
   if(!third&&stage===2&&round===8){g.menu(2);aim(g,plan.c);await g.save('gameplay-3');g.tap('return');third=true;}
   score(g,plan.c);bonus ||= g.read('dice_bonus')===35;
   if(!duplicate&&round===0){g.menu(2);aim(g,plan.c);const total=g.word('dice_total');g.tap('space');assert.equal(g.read('dice_round'),1);assert.equal(g.word('dice_total'),total);assert.equal(g.read('selection_active'),1);g.tap('return');duplicate=true;}
  }
  assert.equal(g.read('phase'),4,`Difficulty ${stage+1}: ${g.word('dice_total')} points`);
 }assert.ok(second&&third&&bonus&&duplicate);
}
