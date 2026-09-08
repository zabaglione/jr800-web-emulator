// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const [wasmDir, outDir, sample, mode='test'] = process.argv.slice(2);
assert.ok(['run','debug','test'].includes(mode));
const url = name => pathToFileURL(resolve(wasmDir,name)).href;
const {WasmMachine} = await import(url('wasm-machine.mjs'));
const {jr800BasicBootExperimentConfiguration} = await import(url('basic-boot-profile.mjs'));
const symbols = Object.fromEntries([... (await readFile(resolve(outDir,`${sample}.sym`),'utf8')).matchAll(/^ \$([0-9A-F]+) G (\S+)/gm)].map(([,a,n])=>[n,parseInt(a,16)]));
const machine = await WasmMachine.createJr800(url('jr800_wasm.mjs'), {...jr800BasicBootExperimentConfiguration(),ignoreUnsupportedIo:Boolean(process.env.JR800_SAMPLE_ROM)});
const read = name => {assert.ok(symbols[name],name);return machine.memory(symbols[name],1)[0];};
const bytes = (name,count) => [...machine.memory(symbols[name],count)];
const key = (n,on) => machine.setKeyboardKeyState(`keypad-${n}`,on);
let frames=0,firstPanel,lastCycles=0;
const timings=[], audio=[];
let lastSequence=-1;
function frame(captureAudio=false) {
    if(frames) machine.step();
    let stop;
    do {
        stop=machine.runTo(symbols.frame_ready,captureAudio?64:2000000);
        if(captureAudio) for(const e of machine.accesses({firstAddress:2,lastAddress:2})) {
            if(e.kind==='data-write' && Number(e.sequence)>lastSequence) {
                audio.push({cycle:Number(e.instructionCycle),value:e.value});lastSequence=Number(e.sequence);
            }
        }
    } while(captureAudio && stop.reason==='instruction-limit');
    assert.equal(stop.reason,'address-reached',JSON.stringify(stop));
    assert.equal(machine.state().sp,0x5fff,'stack');
    const panel=machine.lcdPanel(), fb=machine.memory(symbols.framebuffer,1536);
    for(let y=0;y<64;y++) for(let x=0;x<192;x++) assert.equal(panel.dots[y*192+x],1+((fb[(y>>3)*192+x]>>(y&7))&1),`LCD ${x},${y}`);
    const cycles=Number(machine.state().cycleCount);
    if(frames)timings.push(cycles-lastCycles);
    lastCycles=cycles;frames++;
    firstPanel??=panel;
    return panel;
}
async function save(name,panel=machine.lcdPanel()) {
    const d=[]; panel.dots.forEach((v,i)=>{if(v===2)d.push(`M${i%192} ${i/192|0}h1v1h-1z`);});
    await writeFile(resolve(outDir,`${name}.svg`),`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 64" width="960" height="320" shape-rendering="crispEdges"><rect width="192" height="64" fill="#b1b5a8"/><path fill="#354238" d="${d.join('')}"/></svg>\n`);
    await writeFile(resolve(outDir,`${name}.pixels.json`),JSON.stringify([...panel.dots]));
}
try {
    if(process.env.JR800_SAMPLE_ROM) {
        const p=resolve(process.env.JR800_SAMPLE_ROM), data=await readFile(p);
        if(p.endsWith('.j8r'))machine.loadJr8rom(data);else machine.loadLogicalRom(data);
        const boot=machine.run(1000000);assert.ok(['instruction-limit','sleeping'].includes(boot.reason));
    } else {
        const rom=new Uint8Array(32768).fill(1);rom.set([0x20,0xfe]);rom[32766]=0x80;rom[32767]=0;
        machine.loadLogicalRom(rom);
    }
    const app=await readFile(resolve(outDir,`${sample}.j8a`));
    machine.loadProgram(app);frame();await save('screen',firstPanel);
    const results={};
    if(mode==='test') {
        if(sample==='09-sound-sequence') {
            assert.ok(!symbols.keys_keypad && !symbols.sprite8 && !symbols.scroll_left);
            frame(true);assert.equal(read('phrase'),1);
            const effectEdges=audio.length;
            assert.equal(effectEdges,112);
            frame(true);assert.equal(read('phrase'),0);
            assert.equal(audio.length-effectEdges,780);
            assert.ok(audio.every((e,i)=>e.value===(i%2?0xef:0xff)));
            results.audioEdges=audio.length;
        } else if(sample==='10-sprite-xor') {
            assert.ok(!symbols.keys_keypad && !symbols.sound_play && !symbols.scroll_left);
            for(let i=0;i<370;i++) {
                if(i)frame();
                const x=i<=184?i:368-i, expectedX=i<=368?x:1;
                assert.equal(read('position'),expectedX);
                const y=20+(expectedX&15), image=[0x3c,0x7e,0xdb,0xff,0xff,0xdb,0x7e,0x3c];
                const data=machine.memory(symbols.framebuffer+384,960);
                for(let row=16;row<56;row++) for(let col=0;col<192;col++) {
                    const background=col%8===0 ? ((0x11>>(row&7))&1):0;
                    const sprite=col>=expectedX&&col<expectedX+8&&row>=y&&row<y+8 ? (image[col-expectedX]>>(row-y))&1:0;
                    assert.equal((data[((row>>3)-2)*192+col]>>(row&7))&1,background^sprite,'XOR background restoration');
                }
                if(i===93)await save('moving');
            }
        } else if(sample==='11-scroll-steps') {
            assert.ok(!symbols.sprite8 && !symbols.sound_play);
            let expected=bytes('framebuffer',1536).slice(576,768);
            key(4,true);frame();expected=[...expected.slice(1),0];
            assert.deepEqual(bytes('framebuffer',1536).slice(576,768),expected);
            key(4,false);key(5,true);frame();assert.equal(read('amount'),8);
            frame();assert.equal(read('amount'),8,'held toggle');key(5,false);
            key(6,true);frame();expected=[...Array(8).fill(0),...expected.slice(0,184)];
            assert.deepEqual(bytes('framebuffer',1536).slice(576,768),expected);
            key(6,false);frame();await save('eight-pixels');
        } else if(sample==='12-key-edges') {
            assert.ok(!symbols.sprite8 && !symbols.sound_play && !symbols.scroll_left);
            key(5,true);frame();assert.deepEqual(bytes('saved_edges',3),[32,32,0]);assert.equal(read('presses'),1);await save('pressed');
            frame();assert.deepEqual(bytes('saved_edges',3),[32,0,0]);assert.equal(read('presses'),1);
            key(5,false);frame();assert.deepEqual(bytes('saved_edges',3),[0,0,32]);
            for(let i=0;i<9;i++){key(5,true);frame();key(5,false);frame();}
            assert.equal(read('presses'),0,'decimal wrap');
        } else if(sample==='13-star-courier') {
            const objects=()=>{const b=bytes('objects',18);return [0,3,6,9,12,15].map(i=>({x:b[i],lane:b[i+1],type:b[i+2]})).filter(o=>o.x);};
            const start=()=>{key(8,false);key(2,false);key(5,false);machine.loadProgram(app);frame();key(5,true);frame();key(5,false);frame();};
            assert.equal(read('phase'),0);start();assert.equal(read('phase'),1);assert.equal(read('hull'),3);
            key(4,true);frame();assert.equal(read('lane'),1,'old up key ignored');key(4,false);
            key(6,true);frame();assert.equal(read('lane'),1,'old down key ignored');key(6,false);
            key(8,true);frame();assert.equal(read('lane'),0);frame();assert.equal(read('lane'),0);
            key(8,false);key(2,true);frame();assert.equal(read('lane'),1);key(2,false);frame();
            key(5,true);frame();assert.equal(read('energy'),2);assert.equal(read('shield'),12);
            frame();assert.equal(read('energy'),2);key(5,false);
            start();
            for(let i=0;i<310 && read('phase')===1;i++)frame();
            assert.equal(read('phase'),2,'unprotected stationary ship must lose');await save('game-over');
            key(5,true);frame();assert.equal(read('phase'),1,'retry');frame();assert.equal(read('energy'),3,'held retry must not activate shield');key(5,false);
            start();
            // Hold lane 1 and activate only when its first rock approaches.
            for(let i=0;i<140 && !objects().some(o=>o.type===0&&o.lane===1&&o.x<=44&&o.x>31);i++)frame();
            assert.ok(objects().some(o=>o.type===0&&o.lane===1&&o.x<=44&&o.x>31),'shield encounter');
            key(5,true);frame();key(5,false);
            const shieldHull=read('hull');
            for(let i=0;i<6;i++)frame();
            assert.equal(read('hull'),shieldHull,'shield blocked rock');
            assert.equal(read('energy'),2,'shield cost');
            await save('shield');
            start();
            // Take one hit, then follow only repair pickups using key edges.
            for(let i=0;i<160 && read('hull')===3;i++)frame();
            assert.equal(read('hull'),2,'damage before repair');
            let steering=0, healed=false;
            for(let i=0;i<160 && read('phase')===1;i++) {
                const repair=objects().find(o=>o.type===2&&o.x>=17);
                let move=repair?(repair.lane<read('lane')?8:repair.lane>read('lane')?2:0):0;
                if(move===steering)move=0;
                key(8,move===8);key(2,move===2);steering=move;
                const before=read('hull');frame();
                if(read('hull')>before){healed=true;break;}
            }
            assert.ok(healed,'repair pickup heals damage');
            start();
            let previousKey=0, cargo=0, repairs=0, blocked=0, lastScore=0;
            const sectors=new Set();
            for(let i=0;i<300 && read('phase')===1;i++) {
                const currentLane=read('lane'), objs=objects(), speed=read('speed');sectors.add(read('sector'));
                const approaching=objs.filter(o=>o.x>=17 && o.x<95).sort((a,b)=>a.x-b.x);
                const nearest=approaching[0];let desired=currentLane;
                if(nearest) {
                    if(nearest.type>0)desired=nearest.lane;
                    else if(nearest.lane===currentLane) {
                        desired=[0,1,2].filter(l=>l!==currentLane).sort((a,b)=>Math.abs(a-currentLane)-Math.abs(b-currentLane)).find(l=>!objs.some(o=>o.type===0&&o.lane===l&&o.x>=17&&o.x<55))??currentLane;
                    }
                }
                let nextKey=desired<currentLane?8:desired>currentLane?2:0;
                // Each directional action is a press edge, with release between repeats.
                if(nextKey===previousKey)nextKey=0;
                key(8,nextKey===8);key(2,nextKey===2);previousKey=nextKey;
                const oldHull=read('hull'),oldShield=read('shield');
                frame();
                cargo+=read('score')-lastScore;lastScore=read('score');
                if(read('hull')>oldHull)repairs++;
                if(oldShield && objs.some(o=>o.type===0&&o.lane===read('lane')&&o.x-speed<32&&o.x-speed>=17))blocked++;
                assert.ok(read('hull')>0,`pilot failed ${i}`);
                if(i===85)await save('playing');
            }
            assert.equal(read('phase'),3,'three-sector clear');assert.deepEqual([...sectors],[1,2,3]);assert.ok(cargo>0,'cargo collected');
            await save('clear');results.flight={cargo,repairs,blocked,hull:read('hull'),score:read('score')*10};
        } else throw new Error(`Unknown sample ${sample}`);
    }
    const report={sample,mode,frames,passed:true,...results,minFrameCycles:timings.length?Math.min(...timings):0,maxFrameCycles:timings.length?Math.max(...timings):0};
    await writeFile(resolve(outDir,mode==='test'?'verification.json':`${mode}.json`),JSON.stringify(report,null,2)+'\n');
    if(mode==='debug')console.log(JSON.stringify({state:machine.state(),symbols}));
    console.log(JSON.stringify(report));
} finally {machine.destroy();}
