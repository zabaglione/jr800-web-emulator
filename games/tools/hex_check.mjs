// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const links=Array.from({length:36},(_,p)=>[[0,-1],[0,1],[-1,0],[1,0],[1,-1],[-1,1]].map(([dx,dy])=>[p%6+dx,Math.floor(p/6)+dy]).filter(([x,y])=>x>=0&&x<6&&y>=0&&y<6).map(([x,y])=>y*6+x));
function field(b,edge){const who=edge<2?2:1,d=Array(36).fill(255),q=[];
 for(let p=0;p<36;p++)if((edge===0?Math.floor(p/6)===0:edge===1?Math.floor(p/6)===5:edge===2?p%6===0:p%6===5)&&b[p]!==3-who){d[p]=b[p]===who?0:1;if(d[p])q.push(p);else q.unshift(p);}
 while(q.length){const p=q.shift();for(const n of links[p]){if(b[n]===3-who)continue;const w=b[n]===who?0:1;if(d[p]+w<d[n]){d[n]=d[p]+w;if(w)q.push(n);else q.unshift(n);}}}return d;
}
const gaps=b=>({cpu:Math.min(...field(b,0).slice(30)),you:Math.min(...field(b,2).filter((_,p)=>p%6===5))});
function ai(b){const m=[0,1,2,3].map(e=>field(b,e));let best=-1,cell=-1;
 for(let p=0;p<36;p++)if(!b[p]){const a=Math.min(30,m[0][p]+m[1][p]),d=Math.min(30,m[2][p]+m[3][p]),weight=Math.max(0,5-Math.abs(p%6-2)-Math.abs(Math.floor(p/6)-2)),rank=a===2?255:d===2?240:181-a*4-d*2+weight;if(rank>best){best=rank;cell=p;}}
 assert.ok(cell>=0);return cell;
}
function actions(b,charge){return b.map((v,p)=>!v?{p,relay:false}:charge&&v===2&&links[p].filter(q=>b[q]===1).length>=2?{p,relay:true}:null).filter(Boolean);}
function turn(b,charge,a){assert.ok(actions(b,charge).some(x=>x.p===a.p&&x.relay===a.relay));b=[...b];b[a.p]=1;if(a.relay)charge=0;let distance=gaps(b);if(!distance.you)return {b,charge,distance,result:1};b[ai(b)]=2;distance=gaps(b);return {b,charge,distance,result:distance.cpu===0?2:0};}
function choose(b,charge,weak,depth=2){let options=actions(b,charge).map(a=>{const state=turn(b,charge,a);return {a,...state,score:state.result===1?1e6:state.result===2?-1e6:state.distance.cpu*16-state.distance.you*24+state.charge*5};}).sort((a,c)=>c.score-a.score||a.a.p-c.a.p);
 if(weak)return options.filter(x=>!x.a.relay).at(-1)??options.at(-1);
 if(depth>1)options=options.slice(0,8).map(o=>({...o,score:o.result?o.score:choose(o.b,o.charge,false,depth-1).score})).sort((a,c)=>c.score-a.score||a.a.p-c.a.p);return options[0];
}
export function makeHexReplays(){const routes=[];let anyRelay=false;
 for(let match=0;match<3;match++){let b=Array(36).fill(0),charge=1,result=0,route=[];
  for(let n=0;n<37&&!result;n++){
   let option=choose(b,charge,match===0);
   if(match===2&&charge){const relay=actions(b,charge).find(a=>a.relay);if(relay)option={a:relay,...turn(b,charge,relay)};}
   route.push(option.a);b=option.b;charge=option.charge;result=option.result;anyRelay ||=option.a.relay;
  }
  assert.ok(result);if(match<2)assert.equal(result,match===0?2:1,JSON.stringify({match,result,route}));routes.push(route);
 }
 assert.ok(anyRelay);return routes;
}
function cursor(g,p){while(g.read('cursor')%6<p%6)g.tap('right');while(g.read('cursor')%6>p%6)g.tap('left');while(Math.floor(g.read('cursor')/6)<Math.floor(p/6))g.tap('down');while(Math.floor(g.read('cursor')/6)>Math.floor(p/6))g.tap('up');}
export async function checkHex(g){const routes=JSON.parse(await readFile(new URL('../hex-front/replays.json',import.meta.url)));await g.start();let relayTested=false,capturedLate=false;
 for(let match=0;match<routes.length;match++){
  if(match)g.tap('space');let b=Array(36).fill(0),charge=1;
  if(!match){
   g.frame();assert.equal(g.word('dirty_bytes'),0);g.menu(1);g.tap('space');assert.deepEqual(g.read('board',36),b,'An empty node cannot be converted');assert.equal(g.read('hex_charge'),1);g.tap('return');assert.equal(g.read('selection_active'),0);
   g.tap('space');const placed=g.read('board',36),cpu=placed.indexOf(2);cursor(g,cpu);g.menu(1);g.tap('space');assert.deepEqual(g.read('board',36),placed,'Two adjacent friendly nodes are required');g.tap('return');g.menu(2);assert.deepEqual(g.read('board',36),b);assert.equal(g.read('moves'),0);
  }
  for(let i=0;i<routes[match].length;i++){
   const a=routes[match][i],old=[...b];cursor(g,a.p);
   if(a.relay){g.menu(1);assert.equal(g.read('selection_active'),1);if(!relayTested)await g.save('gameplay-2');}
   const expected=turn(b,charge,a);g.tap('space');
   if(a.relay&&!relayTested&&g.read('phase')===2){
    g.menu(2);assert.deepEqual(g.read('board',36),old);assert.equal(g.read('hex_charge'),1,'Undo restores the relay charge');cursor(g,a.p);g.menu(1);g.tap('space');relayTested=true;
   }
   b=expected.b;charge=expected.charge;assert.deepEqual(g.read('board',36),b,'Six-neighbour connectivity and legal CPU choice');assert.equal(g.read('hex_charge'),charge);assert.equal(g.read('grid_stat'),expected.distance.you);
   assert.equal(g.read('phase'),expected.result===1?4:expected.result===2?5:2);
   if(!match&&i===1)await g.save('gameplay-1');if(!charge&&!capturedLate&&g.read('phase')===2){await g.save('gameplay-3');capturedLate=true;}
  }
 }
 assert.ok(relayTested&&capturedLate,'Conversion, undo and later play were captured');
}
