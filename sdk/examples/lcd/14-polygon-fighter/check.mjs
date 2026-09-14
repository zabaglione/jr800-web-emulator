// SPDX-License-Identifier: MIT
// Run HD6301 instructions on the C++ WASM core and compare against a numeric
// projection + direct line-intersection oracle, independent of the edge walker.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const [wasmDir, outDir, sample, mode = 'test'] = process.argv.slice(2);
assert.ok(wasmDir && outDir && sample === '14-polygon-fighter', 'Expected WASM directory, output directory, sample');
assert.ok(['run', 'debug', 'test'].includes(mode), 'Unknown mode');
const url = name => pathToFileURL(resolve(wasmDir, name)).href;
const {WasmMachine} = await import(url('wasm-machine.mjs'));
const {jr800BasicBootExperimentConfiguration} = await import(url('basic-boot-profile.mjs'));
const symbols = Object.fromEntries([...(await readFile(resolve(outDir, `${sample}.sym`), 'utf8'))
    .matchAll(/^ \$([0-9A-F]+) G (\S+)/gm)].map(([,a,n]) => [n, parseInt(a,16)]));
const configuration = {...jr800BasicBootExperimentConfiguration(),
    // The ROM boot probe uses the normal browser configuration; the synthetic
    // bootstrap separately proves the demo runs without expansion RAM.
    expansionRamInitialValue: process.env.JR800_SAMPLE_ROM ? 0 : undefined,
    ignoreUnsupportedIo: Boolean(process.env.JR800_SAMPLE_ROM)};
const machine = await WasmMachine.createJr800(url('jr800_wasm.mjs'), configuration);
const read = (name, size=1) => machine.memory(symbols[name], size);
const signed = n => n < 128 ? n : n - 256;
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const timings = [], captures = [], angles = new Set(), shades = new Set();
const transfers = [], pollGaps = [], nominalHz = 1_228_800;
let inputPolling = mode === 'test', lastPollCycle, rotationTransfers = [], rotationSha256;
let stops = 0, previousCycles = 0, firstPanel, header, footer, vertices, faces, code, lowerRam;
let stateGuard, stackGuard, rotationTimings = [], shortPressCases = 0;
let edgePairs = [], edgeCaches = [];

function rotated(v, angle) {
    const sin = Math.round(127*Math.sin(angle*Math.PI/32));
    const cos = Math.round(127*Math.sin(((angle+16)%64)*Math.PI/32));
    const mul = (x,y) => Math.floor(x*y/128);
    const [x,y,z] = v;
    const xx = mul(x,cos) + mul(z,sin), zz = mul(z,cos) - mul(x,sin);
    return [xx, mul(y,118)-mul(zz,49), mul(y,49)+mul(zz,118)];
}

function oracle(angle) {
    const projected = vertices.map(v => {
        const [x,y,z] = rotated(v,angle);
        return [96+x,34-y,128+z];
    });
    for (const [x,y] of projected) {
        assert.ok(x >= 0 && x < 192 && y >= 8 && y < 56, `Projection outside viewport: ${angle}: ${x},${y}`);
    }
    assert.deepEqual([...read('projected_vertices', vertices.length*3)], projected.flat(), 'Fixed-point projection');
    edgeCaches.forEach(({address, capacity}, i) => {
        const header = machine.memory(address, 3);
        if (header[0] !== angle) return;
        let [a,b] = edgePairs[i].map(index => projected[index]);
        if (a[1] > b[1]) [a,b] = [b,a];
        const dy = b[1]-a[1], dx = b[0]-a[0];
        assert.ok(dy > 0 && dy+1 <= capacity, `Edge ${i} exceeds cache capacity`);
        assert.equal(header[1]*256+header[2], address+3-a[1], `Edge ${i} anchor`);
        const expected = Array.from({length:dy+1}, (_,y) =>
            a[0]+Math.sign(dx)*Math.floor(Math.abs(dx)*y/dy));
        assert.deepEqual([...machine.memory(address+3, dy+1)], expected, `Edge ${i} cached intersections`);
    });
    const visible = [];
    faces.forEach((f,i) => {
        let n = rotated(f.slice(3,6),angle);
        if (n[2] <= 0) {
            if (!(f[6]&1) || n[2] === 0) return;
            n = n.map(x => -x);
        }
        const intensity = n.reduce((sum,x,j) => sum + Math.floor(x*[-48,88,72][j]/128), 0);
        const shade = f[6]&2 ? 2 : intensity < 0 ? 0 : intensity < 24 ? 2 : intensity < 60 ? 4 : 6;
        shades.add(shade);
        visible.push({face: i, points: f.slice(0,3).map(v => projected[v/3]), shade,
            depth: f.slice(0,3).reduce((sum,v) => sum+projected[v/3][2],0)});
    });
    visible.sort((a,b) => a.depth-b.depth);
    assert.equal(read('visible_count')[0],visible.length,'Back-face culling');
    const records = read('face_records',visible.length*5);
    visible.forEach((f,i) => assert.deepEqual([...records.slice(i*5,i*5+5)],
        [f.depth>>8, f.depth&255, (symbols.faces+f.face*12)>>8, (symbols.faces+f.face*12)&255, f.shade],
        'Depth order and lighting'));
    const expected = new Uint8Array(1536);
    const set = (x,y,value) => {
        const address = (y>>3)*192+x, mask = 1<<(y&7);
        expected[address] = (expected[address]&~mask) | (value ? mask : 0);
    };
    for (const {points,shade} of visible) {
        const min = Math.min(...points.map(p=>p[1])), max = Math.max(...points.map(p=>p[1]));
        for (let y=min; y<=max; y++) {
            const intersections = [];
            points.forEach((p,i) => {
                let [a,b] = [p,points[(i+1)%3]];
                if (a[1]>b[1]) [a,b]=[b,a];
                if (y<a[1] || y>b[1]) return;
                if (a[1]===b[1]) intersections.push(a[0],b[0]);
                else intersections.push(a[0] + Math.sign(b[0]-a[0]) *
                    Math.floor(Math.abs(b[0]-a[0])*(y-a[1])/(b[1]-a[1])));
            });
            const left = Math.min(...intersections), right = Math.max(...intersections);
            const pattern = [3,3,3,1,1,2,1,0][shade+(y&1)];
            for (let x=left; x<=right; x++) set(x,y,
                y===min || y===max || x===left || x===right || ((pattern>>(x&1))&1));
        }
    }
    return expected;
}

function frame(validate=true) {
    if (stops) machine.step();
    // Observe every poll during a complete revolution, without inserting
    // instructions or changing emulated timing in the program under test.
    for (;;) {
        const stop = machine.runTo(symbols.frame_ready, 1_000_000);
        if (stop.reason === 'address-reached') break;
        assert.ok(inputPolling && stop.reason === 'execution-breakpoint' &&
            machine.state().pc === symbols.poll_controls, JSON.stringify(stop));
        const cycle = Number(machine.state().cycleCount);
        if (lastPollCycle !== undefined) pollGaps.push(cycle-lastPollCycle);
        lastPollCycle = cycle;
        machine.step();
    }
    assert.equal(machine.state().sp,0x5fff,'Unbalanced stack');
    const cycle = Number(machine.state().cycleCount);
    if (stops && !read('paused')[0]) {
        timings.push(cycle-previousCycles);
        const bytes = read('dirty_bytes',2);
        transfers.push(bytes[0]*256+bytes[1]);
    }
    previousCycles=cycle;
    stops++;
    const panel = machine.lcdPanel(), fb = read('framebuffer',1536);
    assert.equal(panel.width,192); assert.equal(panel.height,64);
    for (let y=0;y<64;y++) for (let x=0;x<192;x++) {
        assert.equal(panel.dots[y*192+x],1+((fb[(y>>3)*192+x]>>(y&7))&1),`LCD at ${x},${y}`);
    }
    firstPanel ??= panel;
    header ??= fb.slice(0,192); footer ??= fb.slice(1344);
    assert.deepEqual(fb.slice(0,192),header,'Header overwritten');
    assert.deepEqual(fb.slice(1344),footer,'Footer overwritten');
    assert.deepEqual(machine.memory(0x2000,0x800),lowerRam,'BASIC workspace overwritten');
    assert.deepEqual(machine.memory(0x2800,0x1800),code,'Program modified');
    assert.deepEqual(machine.memory(symbols.state_end,0x4c00-symbols.state_end),stateGuard,'State overflow');
    assert.deepEqual(machine.memory(0x5e00,0x100),stackGuard,'Stack reserve overflow');
    edgeCaches.forEach(({address,capacity,guard},i) =>
        assert.equal(machine.memory(address+3+capacity,1)[0],guard,`Edge ${i} cache overflow`));
    const angle=read('angle')[0]; angles.add(angle);
    if (validate) assert.deepEqual(fb.slice(192,1344),oracle(angle).slice(192,1344),`Triangle rasterization at ${angle}`);
    return {angle, fb: [...fb], dots: [...panel.dots]};
}

async function saveSvg(name, dots) {
    const path=[];
    dots.forEach((value,i)=>{if(value===2)path.push(`M${i%192} ${Math.floor(i/192)}h1v1h-1z`);});
    await writeFile(resolve(outDir,`${name}.svg`),
        `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 64" width="1152" height="384" shape-rendering="crispEdges"><rect width="192" height="64" fill="#b1b5a8"/><path fill="#354238" d="${path.join('')}"/></svg>\n`);
}

try {
    if (process.env.JR800_SAMPLE_ROM) {
        const path=resolve(process.env.JR800_SAMPLE_ROM), rom=await readFile(path);
        if(path.endsWith('.j8r')) machine.loadJr8rom(rom); else machine.loadLogicalRom(rom);
        const boot=machine.run(1_000_000);
        assert.ok(['instruction-limit','sleeping'].includes(boot.reason),JSON.stringify(boot));
    } else {
        const rom=new Uint8Array(32768).fill(1);
        rom.set([0x20,0xfe]);rom[32766]=0x80;rom[32767]=0;
        machine.loadLogicalRom(rom);
    }
    const application = await readFile(resolve(outDir,`${sample}.j8a`));
    machine.loadProgram(application);
    vertices=Array.from({length:(symbols.vertices_end-symbols.vertices)/5},(_,i)=>
        [...machine.memory(symbols.vertices+i*5,3)].map(signed));
    faces=Array.from({length:(symbols.faces_end-symbols.faces)/12},(_,i)=>
        [...machine.memory(symbols.faces+i*12,7)].map((v,j)=>j>=3&&j<6?signed(v):v));
    assert.equal(vertices.length,20);assert.equal(faces.length,23);
    vertices.forEach((v,i) => assert.deepEqual([...machine.memory(symbols.vertices+i*5+3,2)],
        [Math.floor(v[1]*118/128)&255,Math.floor(v[1]*49/128)&255],'Vertex fixed pitch constants'));
    faces.forEach((f,i) => assert.deepEqual([...machine.memory(symbols.faces+i*12+7,2)],
        [Math.floor(f[4]*118/128)&255,Math.floor(f[4]*49/128)&255],'Normal fixed pitch constants'));
    edgePairs = [...new Map(faces.flatMap(f => [0,1,2].map(i => {
        const pair = [f[i]/3,f[(i+1)%3]/3].sort((a,b)=>a-b);
        return [pair.join(','),pair];
    }))).values()].sort((a,b)=>a[0]-b[0] || a[1]-b[1]);
    const pointers = read('edge_cache_pointers',symbols.edge_cache_pointers_end-symbols.edge_cache_pointers);
    assert.equal(edgePairs.length,42);
    assert.equal(pointers.length,edgePairs.length*2,'Edge pointer table size');
    const addresses = Array.from({length:edgePairs.length},(_,i)=>pointers[i*2]*256+pointers[i*2+1]);
    assert.equal(addresses[0],symbols.edge_cache);
    assert.equal(symbols.state_end,symbols.edge_cache_end,'State guard must follow all caches');
    edgeCaches = addresses.map((address,i) => {
        const next = addresses[i+1] ?? symbols.edge_cache_end, capacity = next-address-4;
        assert.ok(capacity>0 && capacity<=48,'Invalid edge cache allocation');
        return {address,capacity,guard:machine.memory(next-1,1)[0]};
    });
    faces.forEach((f,i) => {
        const expected = [0,1,2].map(j => {
            const pair = [f[(j+1)%3]/3,f[(j+2)%3]/3].sort((a,b)=>a-b);
            return edgePairs.findIndex(p=>p[0]===pair[0] && p[1]===pair[1])*2;
        });
        assert.deepEqual([...machine.memory(symbols.faces+i*12+9,3)],expected,'Opposite edge identities');
    });
    code=machine.memory(0x2800,0x1800);lowerRam=machine.memory(0x2000,0x800);
    stateGuard=machine.memory(symbols.state_end,0x4c00-symbols.state_end);
    stackGuard=machine.memory(0x5e00,0x100);
    if (inputPolling) machine.setExecutionBreakpoint(symbols.poll_controls,true);
    captures.push(frame());
    await saveSvg('screen',captures[0].dots);
    if (mode==='test') {
        for(let i=1;i<64;i++) captures.push(frame());
        const repeated=frame();
        rotationTimings=[...timings];
        rotationTransfers=[...transfers];
        inputPolling=false;
        machine.setExecutionBreakpoint(symbols.poll_controls,false);
        assert.ok(Math.max(...pollGaps)<49152,'Input polling exceeds minimum browser key hold');
        assert.ok(Math.max(...rotationTransfers)<=300,'Dirty transfer exceeds 300-byte viewport budget');
        rotationSha256=hash(Buffer.concat([...captures].sort((a,b)=>a.angle-b.angle).map(f=>Buffer.from(f.fb))));
        // All 64 original framebuffers, ordered by angle. This protects the
        // appearance even if the model data and numeric oracle change together.
        assert.equal(rotationSha256,'6f31df7e9061e5078dd1e6f1f63b5435618154d12d78a3465d95abcc567a7cb5',
            'Optimized rotation differs from original pixels');
        assert.deepEqual(repeated,captures[0],'Full revolution must return to identical output');
        assert.equal(angles.size,64);assert.equal(shades.size,4);
        assert.equal(new Set(captures.map(f=>hash(Uint8Array.from(f.fb)))).size,64,'Rotation frames must differ');
        const key=on=>machine.setKeyboardKeyState('keypad-5',on);
        key(true);const paused=frame();assert.equal(read('paused')[0],1);
        for(let i=0;i<5;i++) assert.deepEqual(frame(),paused,'Held pause toggled or redrew');
        key(false);assert.deepEqual(frame(),paused,'Release must remain paused');
        key(true);const resumed=frame();assert.equal(read('paused')[0],0);
        assert.equal(resumed.angle,(paused.angle+1)&63,'Resume must advance one angle');
        frame();assert.equal(read('paused')[0],0,'Held resume toggled again');key(false);
        machine.setKeyboardKeyState('keypad-4',true);frame();
        assert.equal(read('paused')[0],0,'Unassigned key affected pause');
        machine.setKeyboardKeyState('keypad-4',false);
        assert.ok(Math.max(...rotationTimings)<110000,'Rotation exceeded 110,000 E-cycle frame budget');
        assert.ok(rotationTimings.reduce((a,b)=>a+b,0)/64<100000,'Rotation mean exceeds 100,000 E cycles');
        // Browser key holds last at least 49,152 E cycles. Exercise presses
        // arriving throughout rendering and transfer across successive frames, then release before
        // waiting for a complete frame. The flag must retain each short edge.
        const runCycles = count => {
            const end=Number(machine.state().cycleCount)+count;
            while(Number(machine.state().cycleCount)<end) {
                const stop=machine.run(128);
                assert.equal(stop.reason,'instruction-limit',JSON.stringify(stop));
            }
        };
        for(let offset=0;offset<=250000;offset+=12500) {
            frame();
            runCycles(offset);
            key(true);runCycles(49152);key(false);
            const pausedFrame=frame();
            assert.equal(read('paused')[0],1,`Lost short press at cycle offset ${offset}`);
            assert.deepEqual(frame(),pausedFrame,'Short press did not remain paused');
            key(true);frame();assert.equal(read('paused')[0],0);
            key(false);frame();shortPressCases++;
        }
        for (const angle of [0,8,16,24,32,40,48,56]) {
            await saveSvg(`rotation-${String(angle).padStart(2,'0')}`,captures.find(f=>f.angle===angle).dots);
        }
        await writeFile(resolve(outDir,'rotation.json'),JSON.stringify(captures)+'\n');
    }
    const mean = rotationTimings.length ? rotationTimings.reduce((a,b)=>a+b,0)/rotationTimings.length : null;
    const result={sample,mode,model:'JR-800 WASM',bootstrap:process.env.JR800_SAMPLE_ROM?'owner-supplied':'project-authored',
        expansionRam:Boolean(process.env.JR800_SAMPLE_ROM),vertices:vertices.length,triangles:faces.length,stops,angles:angles.size,shortPressCases,
        cycles:rotationTimings.length?{min:Math.min(...rotationTimings),max:Math.max(...rotationTimings),mean}:null,
        nominalHz, nominalFps:mean===null?null:nominalHz/mean,
        lcdBytes:rotationTransfers.length?{min:Math.min(...rotationTransfers),max:Math.max(...rotationTransfers),
            mean:rotationTransfers.reduce((a,b)=>a+b,0)/rotationTransfers.length}:null,
        maxInputPollGapCycles:pollGaps.length?Math.max(...pollGaps):null,
        edgeCache:{edges:edgeCaches.length,bytes:symbols.edge_cache_end-symbols.edge_cache},
        rotationSha256, applicationSha256:hash(application),
        firstFrameSha256:hash(firstPanel.dots),passed:true};
    const report=mode==='test' ? (process.env.JR800_SAMPLE_ROM?'verification-owned-rom':'verification') : mode;
    await writeFile(resolve(outDir,`${report}.json`),JSON.stringify(result,null,2)+'\n');
    console.log(JSON.stringify(result));
    if(mode==='debug') console.log(JSON.stringify({state:machine.state(),symbols},null,2));
} finally {machine.destroy();}
