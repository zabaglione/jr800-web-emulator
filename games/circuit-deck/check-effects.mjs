// SPDX-License-Identifier: MIT
// Play a complete campaign through the main controls and inspect intermediate
// LCD states, sounds and menu suspension without altering program memory.
import assert from 'node:assert/strict';
import {mkdir,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game,png} from '../tools/harness.mjs';
const [wasm,out]=process.argv.slice(2),g=await Game.open(wasm,out,'circuit-deck');
const cards=JSON.parse(await readFile(new URL('./cards.json',import.meta.url)));
const m=g.machine,run=m.runTo.bind(m),events=[],captures=[],tones=[];
let pauseArmed=true,paused=false,lastValues=null,observedFields=new Set(),intros=[],defeats=[];
const values=()=>['deck_enemy_hp','deck_hp','deck_enemy_block','deck_block'].map(k=>g.read(k));
const markers=['deck_effect_frame','deck_intro_frame','deck_defeat_frame','sound_tone'];
for(const name of markers)m.setExecutionBreakpoint(g.symbols[name],true);
m.runTo=(address,limit)=>{
 for(let i=0;i<20000;i++){
  const stop=run(address,limit);if(stop.reason!=='execution-breakpoint')return stop;
  const pc=m.state().pc,cycle=Number(m.state().cycleCount);
  if(pc===g.symbols.sound_tone){const s=m.state();tones.push({period:s.x,count:s.a*256+s.b,active:g.read('deck_active'),card:g.read('card_id'),clear:g.read('clear_active')});}
  else{
   const frame={kind:pc===g.symbols.deck_intro_frame?'intro':pc===g.symbols.deck_defeat_frame?'defeat':'effect',battle:g.read('battle'),cycle,values:values(),active:g.read('deck_active'),mask:g.read('deck_flash_mask'),message:g.read('deck_message')};
   const panel=m.lcdPanel(),fb=m.memory(g.symbols.framebuffer,1536);
   for(let y=0;y<64;y++)for(let x=0;x<192;x++)assert.equal(panel.dots[y*192+x],1+((fb[(y>>3)*192+x]>>(y&7))&1),'Each presented effect reached the LCD');
   if(frame.kind==='effect'){
    assert.ok(lastValues);const delta=frame.values.map((v,i)=>Math.abs(v-lastValues[i]));assert.equal(delta.reduce((a,b)=>a+b),1,'Exactly one affected HP or shield point changes per cue');
    const field=delta.findIndex(v=>v);observedFields.add(field);assert.equal(frame.mask,1<<field);lastValues=frame.values;
    if(pauseArmed&&frame.active===1&&frame.mask===1){pauseArmed=false;paused=true;g.hold('return',true);}
    if(captures.filter(c=>c.kind==='effect').length<12)captures.push({...frame,image:png(panel.dots)});
   }else{
    if(frame.kind==='intro'){
     const portrait=[...m.memory(g.symbols.boss_portraits+frame.battle*144,144)];
     for(let y=0;y<48;y++)for(let x=0;x<96;x++)assert.equal(panel.dots[(y+8)*192+x],1+((portrait[(y>>4)*48+(x>>1)]>>((y>>1)&7))&1),'Arrival portrait doubles both axes without losing its contours');
    }
    (frame.kind==='intro'?intros:defeats).push(frame.battle);captures.push({...frame,image:png(panel.dots)});
   }
   events.push(frame);
  }
  m.step();
 }
 throw new Error('Effect observer exceeded its bound');
};
async function act(key){lastValues=values();g.tap(key);if(g.read('phase')===3&&paused){
 g.hold('return',false);g.frame();const held=values();for(let i=0;i<12;i++){g.frame();assert.deepEqual(values(),held);assert.equal(g.word('dirty_bytes'),0);}
 const rounds=g.read('rounds');g.tap('down');g.tap('space');assert.equal(g.read('phase'),2,'Menu selection resumes the per-point effect');assert.equal(g.read('rounds'),rounds,'TURN END cannot overwrite an unfinished effect');
}}
try{
 const sounds=g.read('deck_card_sounds',144);const signatures=[];
 for(let id=0;id<12;id++){const a=sounds.slice(id*12,id*12+12);assert.deepEqual(a.slice(8),[0,0,0,0]);assert.ok(a[2]*256+a[3]>0&&a[6]*256+a[7]>0);signatures.push(a.slice(0,8).join(','));}
 assert.equal(new Set(signatures).size,12,'Every card has its own two-tone signature');
 await g.start();assert.deepEqual(intros,[0]);await g.save('boss-1');
 let actions=0,rewards=0;const played=new Set();
 while(g.read('phase')===2&&actions++<800){
  if(g.read('battle_mode')){
   const options=g.read('rewards',3),priority=[3,1,2,7,1,6,10,3,9,8,5,5];
   const best=options.reduce((a,c,i)=>priority[c]>priority[options[a]]?i:a,0);
   while(g.read('selected')!==best)g.tap('right');await act('space');rewards++;await g.save('boss-'+(rewards+1));continue;
  }
  const hand=g.read('hand',3),en=g.read('energy');
  const choices=hand.map((id,i)=>({id,i,v:id===255?-999:cards[id].stats[0]>en?-999:(cards[id].stats[1]+cards[id].stats[4]*5+Math.min(40-g.read('hp'),cards[id].stats[3])+cards[id].stats[2]*.6+cards[id].stats[5]*2+cards[id].stats[6]*3)})).sort((a,b)=>b.v-a.v);
  if(choices[0].v>=0){while(g.read('selected')!==choices[0].i)g.tap('right');played.add(choices[0].id);await act('space');}
  else {g.tap('down');assert.equal(g.read('selected'),3);await act('space');}
 }
 assert.equal(g.read('phase'),4);assert.equal(rewards,8);assert.deepEqual(intros,[0,1,2,3,4,5,6,7,8]);assert.deepEqual(defeats,[0,1,2,3,4,5,6,7]);
 assert.ok(paused);assert.deepEqual([...observedFields].sort(),[0,1,2,3]);
 const endTurnSounds=tones.filter(t=>t.active===2&&t.count!==3),cardSounds=tones.filter(t=>t.active===1&&t.count!==3);assert.ok(cardSounds.length>=played.size*2);assert.ok(endTurnSounds.length>0);
 g.finishClear();await g.save('campaign-clear');assert.ok(tones.some(t=>t.clear&&t.period===169),'Final-clear melody differs from the four-note boss victory cue');
 const directory=resolve(out,'effects');await mkdir(directory,{recursive:true});for(const [i,c] of captures.entries())await writeFile(resolve(directory,String(i).padStart(2,'0')+'-'+c.kind+'.png'),c.image);
 const result={passed:true,bosses:intros.length,bossDefeatJingles:defeats.length,mainTurnEnd:true,perPointChanges:events.filter(e=>e.kind==='effect').length,affectedFields:[...observedFields].sort(),pauseResume:paused,cardSoundSignatures:12,playedCards:[...played].sort((a,b)=>a-b),toneCalls:tones.length,actions,frames:g.frames,physicalDevice:false};
 await writeFile(resolve(out,'effects-verification.json'),JSON.stringify(result,null,2)+'\n');await writeFile(resolve(directory,'timing.json'),JSON.stringify(events,null,2)+'\n');console.log(JSON.stringify(result));
}finally{g.destroy();}
