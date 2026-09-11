// SPDX-License-Identifier: MIT
// Original rule/model checks, with this edition's physical keys and artwork.
import {readFile,writeFile} from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
const [wasm,out,id,mode='test']=process.argv.slice(2);
if(id!=='relic-dive-gfx')throw new Error('Unexpected game ID');
const original=new URL('../../sdk/examples/lcd/07-relic-dive/',import.meta.url);
const baseline=new URL('./tests/',import.meta.url);
let source=await readFile(new URL('check.mjs',baseline),'utf8');
function once(before,after){if(source.split(before).length!==2)throw new Error('Original check boundary changed: '+before);source=source.replace(before,after);}
once('assert.equal(sample, "07-relic-dive");','assert.equal(sample, "relic-dive-gfx");');
source=source.replaceAll('"return", "space"','"space", "return"').replace('setKeyboardKeyState("return",true)','setKeyboardKeyState("space",true)');
source=source.replaceAll('0x5600','0x2300');
source=source.replace(/from "\.\/([^\"]+)"/g,(_,name)=>'from '+JSON.stringify(new URL(name,baseline).href));
source=source.replace(/new URL\("([^\"]+)",\s*import.meta.url\)/g,(_,name)=>'new URL('+JSON.stringify(new URL(name,name==='hard-clear.json'?new URL('./',import.meta.url):original).href)+')');
source='import {png as capturePng} from '+JSON.stringify(new URL('../tools/harness.mjs',import.meta.url).href)+';\n'+source;
once('const panel = lcd(), paths = [];','const panel = lcd(), paths = [];\n    const base=name.replace(/\\.svg$/,"");\n    await writeFile(resolve(outputDirectory,base+".png"),capturePng(panel.dots));\n    await writeFile(resolve(outputDirectory,base+"-1x.png"),capturePng(panel.dots,1));');
// A separate instruction/key event stream handles this original turn-driven loop.
// No gameplay addresses or replay behavior are added to the emulator host.
source=source.replaceAll('machine.setKeyboardKeyState(', 'eventKey(').replaceAll('machine.step()', 'eventStep()').replaceAll('machine.run(', 'eventRun(');
const helpers=await readFile(new URL('check-support.txt',import.meta.url),'utf8');
once('function fixtureApp(entry, segments) {',helpers+'\nfunction fixtureApp(entry, segments) {');
once('    boundaries++;\n    return result;',`    boundaries++;
    if(eventEnabled){let h=2166136261;for(const b of machine.memory(symbols.framebuffer,1536))h=Math.imul(h^b,16777619)>>>0;events.push('F '+h+' '+machine.state().cycleCount+' '+read('G_MODE'));}
    return result;`);
once('machine.loadProgram(application); ready(); await screen("screen.svg");',`machine.loadProgram(application);eventEnabled=['replay','smoke'].includes(mode);ready();await screen('screen.svg');await screen('title.svg');`);
once('if (mode === "capture") {',`if(mode==='smoke'){
        for(const key of [5,5,5,6,6,6,6,3,4,1,2,11])press(key);
        assert.equal(read('G_MODE'),1);await saveEvents('smoke-');
    } else if (mode === "capture") {`);
once('if (i===1 && record.titleInstructions) {release(); eventStep(); eventRun(record.titleInstructions);}', 'if (i===0 && record.titleInstructions) {eventStep(); eventRun(record.titleInstructions);}');
once('        assert.equal(read("G_DIFFICULTY"),2); assert.equal(read("G_FLOOR"),19);',`        await screen('gameplay-3.svg');
        assert.equal(read("G_DIFFICULTY"),2); assert.equal(read("G_FLOOR"),19);`);
once('                replayFloor=read("G_FLOOR");replayFloors.push',`                replayFloor=read("G_FLOOR");
                if(replayFloor===0)await screen('gameplay-1.svg');
                if(replayFloor===9)await screen('gameplay-2.svg');
                replayFloors.push`);
once('        console.log("Hard clear replay passed without a planner or RAM patches",replayResult);',`        await checkClear();await saveEvents('');
        console.log("Hard clear replay passed without a planner or RAM patches",replayResult);`);
once('            const original = stableState(); press(5); press(5); press(6); press(6);',`            checkAliases();
            const original = stableState(); press(5); press(5); press(6); press(6);`);
const generated=resolve(out,`graphics-check-${mode}.mjs`);await writeFile(generated,source);
const result=spawnSync(process.execPath,[generated,resolve(wasm),resolve(out),id,mode],{stdio:'inherit',env:{...process.env,...(process.env.JR800_GAME_ROM?{JR800_SAMPLE_ROM:process.env.JR800_GAME_ROM}:{})}});
if(result.error)throw result.error;process.exitCode=result.status??1;

if(result.status===0&&mode==='test'){
 const hard=spawnSync(process.execPath,[new URL(import.meta.url).pathname,resolve(wasm),resolve(out),id,'replay'],{stdio:'inherit',env:process.env});
 if(hard.error)throw hard.error;process.exitCode=hard.status??1;
}
