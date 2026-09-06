// SPDX-License-Identifier: MIT
import assert from "node:assert/strict";
import {createHash} from "node:crypto";
import {readFile, writeFile} from "node:fs/promises";
import {resolve} from "node:path";
import {pathToFileURL} from "node:url";

const [wasmDirectory, outputDirectory, sample, mode = "test"] = process.argv.slice(2);
assert.equal(sample, "06-dino");
assert.ok(["run", "debug", "test"].includes(mode));
const moduleUrl = name => pathToFileURL(resolve(wasmDirectory, name)).href;
const {WasmMachine} = await import(moduleUrl("wasm-machine.mjs"));
const {jr800BasicBootExperimentConfiguration} = await import(moduleUrl("basic-boot-profile.mjs"));
const symbolText = await readFile(resolve(outputDirectory, `${sample}.sym`), "utf8");
const symbols = Object.fromEntries([...symbolText.matchAll(/^ \$([0-9A-F]+) G (\S+)/gm)]
    .map(([, address, name]) => [name, parseInt(address, 16)]));
const machine = await WasmMachine.createJr800(moduleUrl("jr800_wasm.mjs"),
    {...jr800BasicBootExperimentConfiguration(), ignoreUnsupportedIo: Boolean(process.env.JR800_SAMPLE_ROM)});
let frames = 0;
let lastCycles;
const frameCycles = [];
const playResults = {};
const read = name => machine.memory(symbols[name], 1)[0];
const number = name => [...machine.memory(symbols[name], name === "distance" ? 3 : 5)].reduce((value, digit) => value * 10 + digit, 0);
const input = pressed => machine.setKeyboardKeyState("space", pressed);

function frame() {
    if (frames) machine.step();
    const stop = machine.runTo(symbols.frame_ready, 100_000);
    assert.equal(stop.reason, "address-reached", JSON.stringify(stop));
    assert.equal(machine.state().sp, 0x5fff, "Unbalanced stack");
    const panel = machine.lcdPanel();
    const buffer = machine.memory(symbols.framebuffer, 1536);
    for (let y = 0; y < 64; y++) {
        for (let x = 0; x < 192; x++) {
            assert.equal(panel.dots[y * 192 + x],
                1 + ((buffer[(y >> 3) * 192 + x] >> (y & 7)) & 1),
                `LCD mismatch at ${x},${y}`);
        }
    }
    const cycles = Number(machine.state().cycleCount);
    if (lastCycles !== undefined && read("phase") === 1) frameCycles.push(cycles - lastCycles);
    lastCycles = cycles;
    frames++;
    return panel;
}

function checkDino(pattern, height) {
    const bytes = machine.memory(symbols[pattern], 32);
    const panel = machine.lcdPanel();
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            const expected = (bytes[x * 2 + (y >> 3)] >> (y & 7)) & 1;
            assert.equal(panel.dots[(40 - height + y) * 192 + 24 + x], expected + 1,
                `PCG alignment ${pattern}, height=${height}, x=${x}, y=${y}`);
        }
    }
}

async function savePanel(name, panel = machine.lcdPanel()) {
    const paths = [];
    panel.dots.forEach((dot, index) => {
        if (dot === 2) paths.push(`M${index % 192} ${Math.floor(index / 192)}h1v1h-1z`);
    });
    await writeFile(resolve(outputDirectory, name),
        `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 64" width="960" height="320" shape-rendering="crispEdges"><rect width="192" height="64" fill="#b1b5a8"/><path fill="#4d544f" d="${paths.join("")}"/></svg>\n`);
}

try {
    if (process.env.JR800_SAMPLE_ROM) {
        const romPath = resolve(process.env.JR800_SAMPLE_ROM);
        const bytes = await readFile(romPath);
        if (romPath.endsWith(".j8r")) machine.loadJr8rom(bytes);
        else machine.loadLogicalRom(bytes);
        const boot = machine.run(1_000_000);
        assert.ok(["instruction-limit", "sleeping"].includes(boot.reason), JSON.stringify(boot));
    } else {
        const rom = new Uint8Array(32768).fill(1);
        rom.set([0x20, 0xfe]); rom[32766] = 0x80; rom[32767] = 0;
        machine.loadLogicalRom(rom);
    }
    const application = await readFile(resolve(outputDirectory, `${sample}.j8a`));
    machine.loadProgram(application);
    const ready = frame();
    assert.equal(read("phase"), 0);
    assert.equal(number("distance"), 0);
    checkDino("dino_run_a", 0);
    await savePanel("screen.svg", ready);
    const readyHash = createHash("sha256").update(ready.dots).digest("hex");
    if (mode === "test") {
        const objects = () => {
            const data=machine.memory(symbols.obstacles,9);
            return [0,3,6].map(i=>({x:(data[i]<<8|data[i+1])<<16>>16,kind:data[i+2]}));
        };
        const target = () => objects().filter(o=>o.x>=(o.kind===4?9:17)).sort((a,b)=>a.x-b.x)[0];
        const reset = () => {
            input(false);machine.loadProgram(application);lastCycles=undefined;
            frame();input(true);frame();
        };
        assert.ok(objects().every(o=>o.x>=192),"Initial objects must start off-screen");
        assert.deepEqual(frame().dots,ready.dots,"Title must remain still");
        const arcs=[];
        for(const hold of [1,2,3,4]) {
            reset();
            // RETURN has no gameplay effect, including during ascent.
            machine.setKeyboardKeyState("return",true);
            const heights=[read("height")];
            for(let i=1;i<20 && read("height")>0;i++) {
                input(i<hold);frame();heights.push(read("height"));
                if(read("height")) checkDino("dino_air",read("height"));
                if(i===6 && hold===4) await savePanel("jump.svg");
            }
            machine.setKeyboardKeyState("return",false);
            arcs.push({hold,peak:Math.max(...heights),updates:heights.length,heights});
        }
        assert.deepEqual(arcs.map(a=>a.peak),[13,19,24,28]);
        assert.deepEqual(arcs.map(a=>a.updates),[10,12,14,15]);
        reset();for(let i=0;i<20;i++)frame();
        assert.equal(read("height"),0,"Held SPACE repeated the jump");
        while(read("phase")===1)frame();
        assert.equal(number("high_score"),number("score"));
        await savePanel("game-over.svg");
        const dead=machine.lcdPanel().dots;
        for(let i=0;i<3;i++)assert.deepEqual(frame().dots,dead,"Held SPACE retried");
        const best=number("high_score");input(false);frame();input(true);frame();input(false);
        assert.equal(number("high_score"),best,"Retry lost best score");
        let holdLeft=0;
        const act = (style="variable") => {
            if(holdLeft>0){input(true);holdLeft--;return;}
            const o=target(),speed=read("speed");
            const threshold=o?.kind===4?8+speed*10:o?.kind===3?35+speed*2:35+speed*5;
            if(o && read("height")===0 && !read("space_previous") && o.x<=threshold) {
                const hold=style==="short"?1:style==="long"?4:(o.kind===3||o.kind===4)?1:4;
                input(true);holdLeft=hold-1;
            } else input(false);
        };
        reset();input(false);
        const kinds=new Set(),speeds=new Set(),edgeKinds=new Set();
        let pairCheckpoint,pitCheckpoint,shorts=0,longs=0;
        const recent=[];
        for(let i=0;i<4050;i++) {
            const before=objects(),speed=read("speed"),o=target();
            if(read("height")===0 && o?.kind===4 && o.x>=90 && o.x<=110) {
                if(!pitCheckpoint)pitCheckpoint=machine.exportState();
                const next=before.filter(n=>n.x>o.x).sort((a,b)=>a.x-b.x)[0];
                if(!pairCheckpoint && speed===5 && next?.kind===2 && next.x-o.x<80)pairCheckpoint=machine.exportState();
            }
            const priorHeight=read("height");
            act();frame();
            recent.push({i,objects:before,h:priorHeight,holdLeft});if(recent.length>20)recent.shift();
            assert.equal(read("phase"),1,`Variable-jump collision: ${JSON.stringify(recent)}`);
            if(priorHeight===0 && read("height")>0) {if(holdLeft)longs++;else shorts++;}
            assert.ok(read("height")<=28);
            objects().forEach((item,slot)=>{
                if(item.x>before[slot].x)assert.ok(item.x>=192,`Object appeared inside screen: ${item.x}`);
                else assert.equal(item.x,before[slot].x-speed,"Unexpected horizontal motion");
                if(before[slot].x>=192 && item.x<192)edgeKinds.add(item.kind);
                kinds.add(item.kind);
            });
            speeds.add(read("speed"));
            if(o?.kind===4 && o.x>=70 && o.x<76)await savePanel("pit.svg");
        }
        assert.deepEqual([...kinds].sort(),[0,1,2,3,4]);
        assert.deepEqual([...edgeKinds].sort(),[0,1,2,3,4]);
        assert.deepEqual([...speeds].sort(),[3,4,5]);
        assert.equal(number("distance"),999);assert.ok(number("score")>999);
        assert.ok(shorts>10 && longs>10);
        const finalScore=number("score");
        input(false);while(read("phase")===1)frame();
        assert.equal(number("high_score"),number("score"));
        assert.ok(number("high_score")>=finalScore);
        assert.ok(pairCheckpoint && pitCheckpoint,"Missing pit/rock approach");
        // At the same takeoff timing, a small jump leaves time for the next rock.
        for(const style of ["variable","long"]) {
            machine.importState(pairCheckpoint);lastCycles=undefined;input(false);holdLeft=0;
            for(let i=0;i<40 && read("phase")===1;i++){act(style);frame();}
            assert.equal(read("phase"),style==="variable"?1:2,"Height choice must affect the next jump window");
        }
        reset();input(false);holdLeft=0;
        for(let i=0;i<500 && read("phase")===1;i++){act("short");frame();}
        assert.equal(read("phase"),2,"A low hop must not clear a rock");
        machine.importState(pitCheckpoint);lastCycles=undefined;input(false);
        while(read("phase")===1)frame();
        assert.equal(read("height"),0,"Walking into a pit must cause a fall");
        function runCycles(count){
            const end=Number(machine.state().cycleCount)+count;
            while(Number(machine.state().cycleCount)<end)assert.equal(machine.run(64).reason,"instruction-limit");
        }
        for(let offset=0;offset<120000;offset+=5000){
            input(false);machine.loadProgram(application);frame();
            runCycles(offset);input(true);runCycles(49152);input(false);lastCycles=undefined;frame();frame();
            assert.equal(read("phase"),1);assert.ok(read("height")>0,`Missed SPACE at ${offset}`);
        }
        Object.assign(playResults,{arcs,shorts,longs,distance:999,score:finalScore,
            allTypesEnterFromRight:true,heightChoiceEnablesNextJump:true,pitFall:true});
    }
    const result = {sample, mode, frames, playResults, passed: true, firstFrameSha256: readyHash,
        bootstrap: process.env.JR800_SAMPLE_ROM ? "owner-supplied" : "project-authored",
        runningFrameCycles: frameCycles.length ? {
            min: Math.min(...frameCycles), max: Math.max(...frameCycles),
            mean: Math.round(frameCycles.reduce((sum, value) => sum + value, 0) / frameCycles.length),
        } : null};
    await writeFile(resolve(outputDirectory, "verification.json"), JSON.stringify(result, null, 2) + "\n");
    console.log(JSON.stringify(result));
    if (mode === "debug") console.log(JSON.stringify({symbols, state: machine.state()}, null, 2));
} finally {
    machine.destroy();
}
