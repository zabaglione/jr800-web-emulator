// SPDX-License-Identifier: MIT
// Shared test harness only. Gameplay and rendering execute in the JR-800 CPU.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

export async function fixture({romPath=process.env.JR800_SAMPLE_ROM}={}) {
    const [wasmDirectory, outputDirectory, sample, mode = 'test'] = process.argv.slice(2);
    assert.ok(wasmDirectory && outputDirectory && sample, 'Missing sample arguments');
    assert.ok(['run', 'debug', 'test'].includes(mode), 'Unknown mode');
    const url = name => pathToFileURL(resolve(wasmDirectory, name)).href;
    const {WasmMachine} = await import(url('wasm-machine.mjs'));
    const {jr800BasicBootExperimentConfiguration} = await import(url('basic-boot-profile.mjs'));
    const machine = await WasmMachine.createJr800(url('jr800_wasm.mjs'), {
        ...jr800BasicBootExperimentConfiguration(),
        expansionRamInitialValue: romPath ? 0 : undefined,
        ignoreUnsupportedIo: Boolean(romPath),
    });
    const symbols = Object.fromEntries([...(await readFile(resolve(outputDirectory, sample+'.sym'), 'utf8'))
        .matchAll(/^ \$([0-9A-F]+) G (\S+)/gm)].map(([,address,name]) => [name, parseInt(address,16)]));
    const sections = [...(await readFile(resolve(outputDirectory, sample+'.map'), 'utf8'))
        .matchAll(/^  \$([0-9A-F]+)-\$([0-9A-F]+)  (PROGRAM|FRAME|STATE|RUNTIME|CONTEXT)  /gm)]
        .map(([,start,end,region]) => ({start:parseInt(start,16),end:parseInt(end,16),region}));
    const sizes = {};
    for (const s of sections) sizes[s.region] = (sizes[s.region] ?? 0) + s.end-s.start+1;
    let frames = 0, previousCycle, maximumPollGap = 0, lastPollCycle, polling = false;
    const cycles = [], lcdBytes = [], keys = new Set();
    const guards = [];
    const read = (name, size = 1) => {
        assert.ok(Number.isInteger(symbols[name]), 'Missing symbol: '+name);
        const data = machine.memory(symbols[name],size);
        return size === 1 ? data[0] : data;
    };
    const key = (name, pressed) => {
        if (pressed === keys.has(name)) return;
        machine.setKeyboardKeyState(name,pressed);
        if (pressed) keys.add(name); else keys.delete(name);
    };
    const release = () => { for (const name of [...keys]) key(name,false); };
    function frame() {
        if (frames) machine.step();
        for (;;) {
            const result = machine.runTo(symbols.frame_ready, 2_000_000);
            if (result.reason === 'address-reached') break;
            assert.ok(polling && result.reason === 'execution-breakpoint' &&
                machine.state().pc === symbols.input_poll, JSON.stringify(result));
            const now = Number(machine.state().cycleCount);
            if (lastPollCycle !== undefined) maximumPollGap = Math.max(maximumPollGap, now-lastPollCycle);
            lastPollCycle = now;
            machine.step();
        }
        assert.equal(machine.state().sp,0x5fff,'Unbalanced stack');
        assert.equal(read('p3_error'),0,'Renderer rejected program arguments');
        const cycle = Number(machine.state().cycleCount);
        if (previousCycle !== undefined) cycles.push(cycle-previousCycle);
        previousCycle=cycle;
        const amount=read('dirty_bytes',2);
        lcdBytes.push(amount[0]*256+amount[1]);
        const framebuffer=read('framebuffer',1536), panel=machine.lcdPanel();
        assert.equal(panel.width,192); assert.equal(panel.height,64);
        for(let y=0;y<64;y++) for(let x=0;x<192;x++)
            assert.equal(panel.dots[y*192+x],1+((framebuffer[(y>>3)*192+x]>>(y&7))&1),`LCD pixel ${x},${y}`);
        for(const guard of guards) assert.deepEqual(machine.memory(guard.address,guard.data.length),guard.data,guard.name);
        frames++;
        return {dots:[...panel.dots],framebuffer:[...framebuffer],cycle};
    }
    async function capture(name='screen') {
        const dots=machine.lcdPanel().dots, paths=[];
        dots.forEach((dot,i)=>{if(dot===2) paths.push(`M${i%192} ${Math.floor(i/192)}h1v1h-1z`);});
        await writeFile(resolve(outputDirectory,name+'.svg'),
            `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 64" width="1152" height="384" shape-rendering="crispEdges"><rect width="192" height="64" fill="#b1b5a8"/><path fill="#354238" d="${paths.join('')}"/></svg>\n`);
        await writeFile(resolve(outputDirectory,name+'.pgm'), Buffer.concat([
            Buffer.from('P5\n192 64\n255\n'), Buffer.from(Uint8Array.from(dots, dot => dot === 2 ? 35 : 180))]));
    }
    const tap = name => { key(name,true); frame(); key(name,false); frame(); };
    const until = (condition, limit=800) => {
        for(let i=0;i<limit;i++) { if(condition()) return; frame(); }
        assert.fail('Input replay did not reach its expected state');
    };
    try {
        if (romPath) {
            const path=resolve(romPath), bytes=await readFile(path);
            if(path.endsWith('.j8r')) machine.loadJr8rom(bytes); else machine.loadLogicalRom(bytes);
            const boot=machine.run(1_000_000);
            assert.ok(['instruction-limit','sleeping'].includes(boot.reason),JSON.stringify(boot));
        } else {
            const rom=new Uint8Array(32768).fill(1);
            rom.set([0x20,0xfe]); rom[32766]=0x80; rom[32767]=0;
            machine.loadLogicalRom(rom);
        }
        const binary=await readFile(resolve(outputDirectory,sample+'.j8a'));
        machine.loadProgram(binary);
        for(const [address,length,name] of [[0x2000,0x800,'Lower RAM'],[0x5200,0x200,'Framebuffer tail'],
                [0x5df0,48,'Workspace and stack margin'], ...sections.filter(s=>s.region==='PROGRAM')
                    .map(s=>[s.start,s.end-s.start+1,'Immutable program data'])]) {
            guards.push({address,name,data:Uint8Array.from(machine.memory(address,length))});
        }
        return {machine,symbols,mode,sample,read,key,release,tap,until,frame,capture,cycles,lcdBytes,
            observePolling(enabled=true) {
                polling=enabled;
                lastPollCycle=undefined;
                machine.setExecutionBreakpoint(symbols.input_poll,enabled);
            },
            timing(start=0, end=cycles.length) {
                const values=cycles.slice(start,end), mean=values.reduce((s,n)=>s+n,0)/values.length;
                return {frames:values.length,minCycles:Math.min(...values),maxCycles:Math.max(...values),
                    meanCycles:mean,nominalFps:1228800/mean};
            },
            async finish(extra={}) {
                if (maximumPollGap) assert.ok(maximumPollGap<49152, 'Input scan missed the browser minimum key hold');
                let breakExit;
                if (mode==='test' && romPath) {
                    release();
                    machine.setExecutionBreakpoint(symbols.input_poll,false);
                    key('break',true);
                    const stop=machine.runTo(symbols.basic_exit,200_000);
                    assert.equal(stop.reason,'address-reached','BREAK did not enter the BASIC return routine');
                    key('break',false);
                    const resumed=machine.run(150_000);
                    assert.ok(['instruction-limit','sleeping'].includes(resumed.reason),JSON.stringify(resumed));
                    assert.ok(machine.state().pc>=0x8000,'BREAK did not return to ROM execution');
                    breakExit={enteredReturnRoutine:true,romExecution:true,reason:resumed.reason};
                }
                const mean=cycles.reduce((sum,n)=>sum+n,0)/Math.max(1,cycles.length);
                const result={sample,mode,bootstrap:romPath?'owner-supplied':'project-authored',
                    expansionRam:Boolean(romPath),frames,cycles:{min:Math.min(...cycles),max:Math.max(...cycles),mean},
                    nominalFps:mean?1228800/mean:null,lcdBytes:{max:Math.max(...lcdBytes),mean:lcdBytes.reduce((s,n)=>s+n,0)/lcdBytes.length},
                    sizes,maxInputPollGapCycles:maximumPollGap || null,breakExit,
                    applicationSha256:createHash('sha256').update(binary).digest('hex'),...extra,passed:true};
                const report=mode==='test' ? (romPath?'verification-owned-rom':'verification') : mode;
                await writeFile(resolve(outputDirectory,report+'.json'),JSON.stringify(result,null,2)+'\n');
                console.log(JSON.stringify(result));
                if(mode==='debug') console.log(JSON.stringify({state:machine.state(),symbols},null,2));
            },
            close() { release(); machine.destroy(); },
        };
    } catch(error) { machine.destroy(); throw error; }
}
