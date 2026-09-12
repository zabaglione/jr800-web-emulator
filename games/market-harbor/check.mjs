// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const state=g=>({port:g.read('market_port'),day:g.read('market_day'),cash:g.word('market_cash'),goal:g.word('market_goal'),capacity:g.read('market_capacity'),cargo:g.read('market_cargo',3),load:g.read('market_load'),good:g.read('market_good'),mode:g.read('market_mode'),prices:g.read('market_prices',3),dest:g.read('market_destination'),selection:g.read('selection_active'),reason:g.read('market_reason'),message:g.read('market_message'),phase:g.read('phase')});
const trip=(a,b)=>Math.min((a-b+4)%4,(b-a+4)%4);
export async function checkMarket(g){
 const read=name=>readFile(new URL(`./${name}.json`,import.meta.url)).then(JSON.parse);const [levels,solutions,prices]=await Promise.all([read('levels'),read('solutions'),read('prices')]);const price=(p,d,l,k)=>Math.max(1,prices.bases[p][k]+prices.season[k][(d+l.offset)%8]);await g.start();await g.save('gameplay-1');let second=false,third=false;
 function select(good,sell,letter=false){while(g.read('market_good')!==good)g.tap(letter?'letter-s':'down');g.tap(sell?(letter?'letter-d':'right'):(letter?'letter-a':'left'));assert.equal(g.read('market_good'),good);assert.equal(g.read('market_mode'),Number(sell));}
 function trade(){const s=state(g),k=s.good,p=s.prices[k];if(s.mode){if(s.cargo[k]){s.cargo[k]--;s.load--;s.cash+=p;if(s.cash>=s.goal)s.phase=4;}}else if(s.load<s.capacity&&s.cash>=p){s.cargo[k]++;s.load++;s.cash-=p;}g.tap('space');assert.deepEqual(state(g),s,'Single-unit transactions preserve cash, cargo and capacity');}
 function sellAll(){const s=state(g);s.cash+=s.cargo.reduce((sum,n,k)=>sum+n*s.prices[k],0);s.cargo=[0,0,0];s.load=0;if(s.cash>=s.goal)s.phase=4;g.menu(2);assert.deepEqual(state(g),s,'Batch sale uses current prices and clears each cargo once');}
 function open(dest){while(g.read('market_good')!==3)g.tap('down');const before=state(g);g.tap('space');assert.equal(g.read('selection_active'),1,'SAIL is reachable directly from the fourth selection on the main screen');for(const k of ['cash','day','cargo','load','port'])assert.deepEqual(state(g)[k],before[k],'Opening SAIL does not spend cash or advance the day');while(g.read('market_destination')!==dest)g.tap('down');const s=state(g),level=levels[g.read('stage')],expected=[];for(let port=0;port<4;port++)for(let k=0;k<3;k++)expected.push(price(port,s.day+trip(port,s.port),level,k));assert.deepEqual(g.read('market_quotes',12),expected,'Port list quotes arrival-day prices, including the free current-port return');return s;}
 function sail(){const s=state(g),days=trip(s.port,s.dest),fare=days*2,level=levels[g.read('stage')];if(!days)s.selection=0;else if(s.cash<fare)s.message=1;else if(s.day+days>=level.limit){s.selection=0;s.phase=5;s.reason=1;}else{s.day+=days;s.cash-=fare;s.port=s.dest;s.selection=0;s.message=0;s.prices=[0,1,2].map(k=>price(s.port,s.day,level,k));if(s.cash<2&&!s.load){s.phase=5;s.reason=2;}}g.tap('space');assert.deepEqual(state(g),s,'Confirmed travel applies fare and time once, and refuses unfunded or over-deadline voyages');}
 const frozen=state(g);for(let n=0;n<25;n++){const blink=g.read('cursor_blink_mask');g.frame();if(blink===g.read('cursor_blink_mask'))assert.equal(g.word('dirty_bytes'),0);else assert.ok(g.word('dirty_bytes')<=192,'Only the focus marker is transferred');}assert.deepEqual(state(g),frozen);
 select(2,false);trade();select(0,false);trade();assert.equal(g.word('market_cash'),2);const before=state(g);open(2);sail();assert.equal(g.read('market_message'),1);g.tap('return');const after=state(g);for(const name of ['cash','day','port','load','cargo','prices'])assert.deepEqual(after[name],before[name],'Cancelling the destination changes no economic state');sellAll();assert.equal(g.word('market_cash'),24);
 select(0,false);for(let n=0;n<6;n++)trade();assert.equal(g.read('market_load'),6);trade();sellAll();select(0,true);trade();
 while(g.read('phase')===2){open((g.read('market_port')+1)%4);sail();}assert.equal(g.read('market_reason'),1);g.tap('space');
 for(let stage=0;stage<12;stage++){
  if(stage)g.tap('space');const level=levels[stage];assert.equal(g.read('stage'),stage);assert.equal(g.word('market_cash'),level.cash);assert.equal(g.word('market_goal'),level.goal);assert.equal(g.read('market_capacity'),level.capacity);assert.deepEqual(g.read('market_prices',3),[0,1,2].map(k=>price(0,0,level,k)));
  for(let hop=0;hop<solutions[stage].length;hop++){
   const plan=solutions[stage][hop];select(plan.good,false,stage%2===1);for(let n=0;n<plan.count;n++)trade();open(plan.dest);if(stage===4&&hop===1){await g.save('gameplay-2');second=true;}sail();assert.equal(g.read('phase'),2);
   if(stage===10&&hop===2){await g.save('gameplay-3');third=true;}
   if(stage%2===1)sellAll();else{select(plan.good,true);for(let n=0;n<plan.count&&g.read('phase')===2;n++)trade();}
   if(g.read('phase')===4)break;
  }
  assert.equal(g.read('phase'),4,`Trade contract ${stage+1}`);assert.ok(g.word('market_cash')>=level.goal);assert.ok(g.read('market_day')<level.limit);
 }
 assert.ok(second&&third);
}
