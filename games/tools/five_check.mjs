// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const axes=[[1,0],[0,1],[1,1],[-1,1]],windows=[];
for(let y=0;y<7;y++)for(let x=0;x<14;x++)for(const [dx,dy] of axes)if(x+4*dx>=0&&x+4*dx<14&&y+4*dy<7)windows.push([0,1,2,3,4].map(i=>(y+i*dy)*14+x+i*dx));
function lineInfo(b,p,side){let best=1,open=0;for(const [dx,dy] of axes){let n=1,ends=0;for(const sign of [-1,1]){let x=p%14+dx*sign,y=Math.floor(p/14)+dy*sign;while(x>=0&&x<14&&y>=0&&y<7&&b[y*14+x]===side){n++;x+=dx*sign;y+=dy*sign;}if(x>=0&&x<14&&y>=0&&y<7&&!b[y*14+x])ends++;}if(n>best||(n===best&&ends>open)){best=n;open=ends;}}return {n:best,open};}
const length=(b,p,side)=>lineInfo(b,p,side).n;
function ai(b){let best=-1,pos=-1;for(let p=0;p<98;p++)if(!b[p]){
 const own=lineInfo(b,p,2),enemy=lineInfo(b,p,1),a=[0,0,0,8,8,8,16,16,16,24,24,140,32,225,230][own.n*3+own.open],d=[0,0,0,6,6,6,12,12,12,18,18,130,24,150,220][enemy.n*3+enemy.open];
 const weight=Math.max(0,6-Math.abs(p%14-6)-Math.abs(Math.floor(p/14)-3)),score=own.n>=5?255:enemy.n>=5?240:(Math.max(a,d)>=128?Math.max(a,d):a+d)+weight;
 if(score>best){best=score;pos=p;}
 }return pos;}
function turn(b,p){b=[...b];assert.equal(b[p],0);b[p]=1;if(length(b,p,1)>=5)return {b,result:1};const cpu=ai(b);if(cpu<0)return {b,result:3};b[cpu]=2;return {b,result:length(b,cpu,2)>=5?2:b.every(Boolean)?3:0};}
function value(b){let score=0;for(const line of windows){let own=0,enemy=0;for(const p of line){own+=b[p]===1;enemy+=b[p]===2;}if(!enemy)score+=[0,1,6,40,400,100000][own];if(!own)score-=[0,1,6,40,500,100000][enemy];}return score;}
function candidates(b){const played=b.map((c,p)=>c?p:-1).filter(p=>p>=0);if(!played.length)return [48];return b.map((c,p)=>!c&&played.some(q=>Math.abs(q%14-p%14)<=2&&Math.abs(Math.floor(q/14)-Math.floor(p/14))<=2)?p:-1).filter(p=>p>=0);}
function choose(b,weak,depth=1){let options=candidates(b).map(p=>{const state=turn(b,p);return {p,...state,score:state.result===1?1e6:state.result===2?-1e6:value(state.b)};}).sort((a,c)=>c.score-a.score||a.p-c.p);
 if(weak)return options.at(-1);
 if(depth>1)options=options.slice(0,8).map(o=>({...o,score:o.result?o.score:choose(o.b,false,depth-1).score})).sort((a,c)=>c.score-a.score||a.p-c.p);
 return options[0];
}
export function makeFiveReplays(){const replays=[];for(const weak of [true,false]){let b=Array(98).fill(0),result=0,route=[];for(let n=0;n<49&&!result;n++){const choice=choose(b,weak,weak?1:2);route.push(choice.p);b=choice.b;result=choice.result;}assert.equal(result,weak?2:1,JSON.stringify({weak,result,route}));replays.push(route);}return replays;}
export async function checkFive(g){const replays=JSON.parse(await readFile(new URL('../five-stones/replays.json',import.meta.url)));
 await g.start();
 for(let match=0;match<2;match++){
  if(match)g.tap('space');let b=Array(98).fill(0);
  if(!match){
   g.tap('space');g.menu(1);assert.deepEqual(g.read('board',98),b,'Undo restores a full turn');assert.equal(g.read('moves'),0);
   g.frame();assert.equal(g.word('dirty_bytes'),0);
  }
  for(let i=0;i<replays[match].length;i++){
   const p=replays[match][i];while(g.read('cursor')%14<p%14)g.tap('right');while(g.read('cursor')%14>p%14)g.tap('left');
   while(Math.floor(g.read('cursor')/14)<Math.floor(p/14))g.tap('down');while(Math.floor(g.read('cursor')/14)>Math.floor(p/14))g.tap('up');
   const expected=turn(b,p);g.tap('space');b=expected.b;
   assert.deepEqual(g.read('board',98),b,'Independent line and tactical model');assert.equal(g.read('phase'),expected.result===1||expected.result===3?4:expected.result===2?5:2);
   if(!match&&i===1)await g.save('gameplay-1');if(match&&i===2)await g.save('gameplay-2');if(match&&i===replays[match].length-2)await g.save('gameplay-3');
   if(!i){const moves=g.read('moves');g.tap('space');assert.deepEqual(g.read('board',98),b,'Occupied intersections reject placement');assert.equal(g.read('moves'),moves);}
  }
  assert.equal(g.read('five_result'),match?1:2);
 }
}
