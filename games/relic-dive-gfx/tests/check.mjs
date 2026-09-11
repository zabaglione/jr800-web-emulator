// SPDX-License-Identifier: MIT
// Every fixture executes the real HD6301 program in the shared JR-800 core.
// The test-only J8A writer supplies RAM fixtures, never game rules or rendering.
import assert from "node:assert/strict";
import {chooseHardAction} from "./hard-planner.mjs";
import {createSearchModel, generateFloor, descend, newAdventure} from "./search-model.mjs";
import {createHash} from "node:crypto";
import {readFile, writeFile} from "node:fs/promises";
import {resolve} from "node:path";
import {pathToFileURL} from "node:url";
const [wasmDirectory, outputDirectory, sample, mode = "test"] = process.argv.slice(2);
assert.equal(sample, "07-relic-dive");
const url = name => pathToFileURL(resolve(wasmDirectory, name)).href;
const {WasmMachine} = await import(url("wasm-machine.mjs"));
const {jr800BasicBootExperimentConfiguration} = await import(url("basic-boot-profile.mjs"));
const source = String(await readFile(new URL("constants.inc", import.meta.url)));
const c = Object.fromEntries([...source.matchAll(/\.equ (\w+), (\$[\dA-F]+|\d+)/g)]
    .map(([, n, a]) => [n, a.startsWith("$") ? parseInt(a.slice(1), 16) : +a]));
const symbols = Object.fromEntries([...String(await readFile(resolve(outputDirectory, `${sample}.sym`))).matchAll(/^ \$([0-9A-F]+) G (\S+)/gm)]
    .map(([, a, n]) => [n, parseInt(a, 16)]));
const application = new Uint8Array(await readFile(resolve(outputDirectory, `${sample}.j8a`)));
const machine = await WasmMachine.createJr800(url("jr800_wasm.mjs"),
    {...jr800BasicBootExperimentConfiguration(), ignoreUnsupportedIo: Boolean(process.env.JR800_SAMPLE_ROM)});
const keys = [null, "keypad-8", "keypad-2", "keypad-4", "keypad-6", "return", "space", "keypad-7", "keypad-9", "keypad-1", "keypad-3", "keypad-5"];
const read = name => machine.memory(c[name], 1)[0];
const word = name => machine.memory(c[name], 2).reduce((n, b) => n * 256 + b, 0);
let boundaries = 0, maxTurnCycles = 0, maxFloorCycles = 0, seeds = 0, modelChecks = 0;
let terrainDiversity;
const startedAt = performance.now();
const kindSeen = new Set();
let recording = false;
const inputTrace = [];
const offlinePlan=process.env.RELIC_DIVE_PLAN?JSON.parse(await readFile(process.env.RELIC_DIVE_PLAN)):null;
let offlineIndex=0;
const savedPrefix=offlinePlan?.prefix??null;
const titleInstructions=savedPrefix?.titleInstructions??0;
assert.ok(Number.isSafeInteger(titleInstructions)&&titleInstructions>=0&&titleInstructions<=1_000_000);

function fixtureApp(entry, segments) {
    segments = [...segments].sort((a, b) => a[0] - b[0]);
    const identity = Buffer.concat([Buffer.from([1, 0, 0, 0, 8]), Buffer.from("hd6301v1"), Buffer.from([0])]);
    const head = Buffer.alloc(6); head.writeUInt16BE(entry); head.writeUInt32BE(segments.length, 2);
    const body = Buffer.concat([head, ...segments.map(([address, bytes]) => {
        const h = Buffer.alloc(11); h[0] = 1; h.writeUInt16BE(address, 1);
        h.writeUInt32BE(bytes.length, 3); h.writeUInt32BE(bytes.length, 7);
        return Buffer.concat([h, Buffer.from(bytes)]);
    })]);
    const hash = createHash("sha256").update("JR8APP-INTEGRITY-V1\0").update(identity).update(body).digest();
    const header = Buffer.alloc(12); header.write("JR8APP\0\0"); header.writeUInt16BE(1, 8);
    return new Uint8Array(Buffer.concat([header, identity, hash, body]));
}
function patch(entries) {
    assert.ok(!["play", "replay", "capture"].includes(mode), "Full adventure must not patch RAM");
    machine.loadProgram(fixtureApp(0, entries), {runAfterLoad: false});
}
function put(name, value) { patch([[c[name], [value]]]); }
function call(name) {
    const address = symbols[name];
    const harness = [0x8e, 0x5f, 0xff, 0xbd, address >> 8, address & 255,
        0x7e, symbols.frame_ready >> 8, symbols.frame_ready & 255];
    machine.loadProgram(fixtureApp(0x5600, [[0x5600, harness]]));
    return ready();
}
function ready(limit = 1_000_000) {
    const result = machine.runTo(symbols.frame_ready, limit);
    assert.equal(result.reason, "address-reached", JSON.stringify(result));
    assert.equal(machine.state().sp, 0x5fff, "Stack did not balance");
    boundaries++;
    return result;
}
function lcd() {
    const panel = machine.lcdPanel(), buffer = machine.memory(symbols.framebuffer, 1536);
    for (let y = 0; y < 64; y++) for (let x = 0; x < 192; x++) {
        assert.equal(panel.dots[y * 192 + x], 1 + ((buffer[(y >> 3) * 192 + x] >> (y & 7)) & 1));
    }
    return panel;
}
function release() {
    for (const key of keys.slice(1)) machine.setKeyboardKeyState(key, false);
    machine.step(); machine.run(150);
}
function press(key) {
    if (recording) inputTrace.push(key);
    release();
    const before = Number(machine.state().cycleCount), turns = word("G_TURNS"), floor = read("G_FLOOR");
    machine.setKeyboardKeyState(keys[key], true);
    ready();
    if (read("G_FLOOR") !== floor) maxFloorCycles = Math.max(maxFloorCycles, Number(machine.state().cycleCount) - before);
    else if (word("G_TURNS") !== turns) maxTurnCycles = Math.max(maxTurnCycles, Number(machine.state().cycleCount) - before);
    lcd();
}
async function screen(name) {
    const panel = lcd(), paths = [];
    panel.dots.forEach((dot, i) => { if (dot === 2) paths.push(`M${i % 192} ${Math.floor(i / 192)}h1v1h-1z`); });
    await writeFile(resolve(outputDirectory, name), `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 64" width="960" height="320" shape-rendering="crispEdges"><rect width="192" height="64" fill="#b1b5a8"/><path fill="#4d544f" d="${paths.join("")}"/></svg>\n`);
}
function stableState() {
    // Persistent world and gameplay fields; exclude UI selection and scratch.
    return Buffer.concat([Buffer.from(machine.memory(c.G_DIFFICULTY, 21)),
        Buffer.from(machine.memory(c.G_BAG, 14)), Buffer.from(machine.memory(c.G_POISON, 2)),
        Buffer.from(machine.memory(c.FLOORS, 768))]);
}
function arena({enemies = [], items = [], hp = 20, food = 200, x = 4, y = 4, floor = 0, turns = 0} = {}) {
    for (const key of keys.slice(1)) machine.setKeyboardKeyState(key, false);
    const map = new Uint8Array(768);
    for (let y = 1; y < 15; y++) for (let x = 1; x < 23; x++) map[y * 24 + x] = 129;
    enemies.forEach((e, i) => map.set(e, 384 + i * 8));
    items.forEach((item, i) => map.set(item, c.ITEM_START + i * 3));
    const state = machine.memory(0x4800, 256);
    state[4] = 0; state[5] = 1; state[6] = 1; state[7] = floor; state[8] = x; state[9] = y;
    state[10] = hp; state[11] = 20; state[12] = food; state[13] = 3; state[14] = 0;
    state[15] = 1; state[16] = 0; state[17] = 0; state[18] = 0; state[19] = 0; state[20] = 0;
    state[21] = 0; state[22] = 0; state[23] = turns >> 8; state[24] = turns & 255;
    state[29] = 0; state.fill(0, 30, 37); state.set([0, 1, 2, 3, 4], 37);
    state[42] = 0; state[43] = 0; state.fill(0, 46, 55); state[58] = 10;
    state[59] = 0; state[60] = 0;
    patch([[0x4800, state], [c.FLOORS, map]]);
    call("update_visibility");
}
function bagAction(slot, action = 0) {
    const onStairs = (machine.memory(c.FLOORS + read("G_Y") * 24 + read("G_X"), 1)[0] & 127) === 3;
    press(5); // main
    if (onStairs) press(2);
    press(5); // inventory
    for (let i = 0; i < slot; i++) press(2);
    press(5); // item detail
    for (let i = 0; i < action; i++) press(2);
    press(5);
}
function waitTurn() { press(5); press(2); press(5); }
function validateFloor() {
    const map = machine.memory(c.FLOORS, 768);
    const tiles = [...map.slice(0, 384)].map(v => v & 127);
    const entry = read("G_X") + read("G_Y") * 24;
    assert.equal(tiles[entry],1,"Arrival is not floor");
    const reached = new Set([entry]), queue = [...reached];
    for (let i = 0; i < queue.length; i++) {
        const pos = queue[i];
        for (const d of [-24, 24, -1, 1]) if (tiles[pos + d] && !reached.has(pos + d)) {
            reached.add(pos + d); queue.push(pos + d);
        }
    }
    assert.equal(reached.size, tiles.filter(Boolean).length, "Disconnected floor");
    const goals=tiles.flatMap((v,i)=>v===3||v===4?[i]:[]);
    assert.equal(goals.length,1);
    assert.ok(reached.has(goals[0]), "Goal is unreachable");
    assert.ok(Math.abs(goals[0]%24-read("G_X"))+Math.abs(Math.floor(goals[0]/24)-read("G_Y"))>=12,"Goal too close to arrival");
    for(let i=0;i<384;i++)if(i<24||i>=360||i%24===0||i%24===23)assert.equal(tiles[i],0,"Open map border");
    assert.equal(tiles.filter(t => t === 2).length, 0, "Upstairs remained");
    assert.equal(tiles[goals[0]], read("G_FLOOR") + 1 === read("G_DEPTH") ? 4 : 3);
    const occupied = new Set();
    let enemyCount = 0, foodCount = 0;
    for (let i = 0; i < c.MAX_ENEMIES; i++) {
        const e = map.slice(384 + i * 8, 392 + i * 8);
        if (!e[3]) continue;
        enemyCount++; kindSeen.add(e[2]);
        assert.ok(e[2] >= 1 && e[2] <= 12);
        const pos = e[0] + e[1] * 24;
        assert.equal(tiles[pos], 1); assert.notEqual(pos, entry);
        assert.ok(Math.abs(e[0]-read("G_X"))+Math.abs(e[1]-read("G_Y"))>=6,"Unsafe arrival");
        assert.ok(!occupied.has(pos), "Enemy/item overlap"); occupied.add(pos);
    }
    assert.equal(enemyCount, read("G_ENEMY_COUNT"));
    assert.ok(enemyCount <= c.MAX_ENEMIES);
    for (let i = 0; i < c.MAX_ITEMS; i++) {
        const item = map.slice(c.ITEM_START + i * 3, c.ITEM_START + i * 3 + 3);
        if (!item[2]) continue;
        assert.ok(item[2] >= 1 && item[2] <= 13);
        const pos = item[0] + item[1] * 24;
        assert.equal(tiles[pos], 1); assert.notEqual(pos,entry); assert.ok(!occupied.has(pos), "Item overlap"); occupied.add(pos);
        if (item[2] === 1) foodCount++;
    }
    assert.equal(foodCount, read("G_DIFFICULTY") === 0 ? 2 : 1);
}
try {
    if (process.env.JR800_SAMPLE_ROM) {
        const bytes = await readFile(process.env.JR800_SAMPLE_ROM);
        if (process.env.JR800_SAMPLE_ROM.endsWith(".j8r")) machine.loadJr8rom(bytes);
        else machine.loadLogicalRom(bytes);
        const boot = machine.run(1_000_000);
        assert.ok(["instruction-limit", "sleeping"].includes(boot.reason));
    } else {
        const rom = new Uint8Array(32768).fill(1); rom.set([32, 254]); rom[32766] = 128; rom[32767] = 0;
        machine.loadLogicalRom(rom);
    }
    machine.loadProgram(application); ready(); await screen("screen.svg");
    if (mode === "capture") {
        const difficulty=Number(process.env.RELIC_DIVE_PLAY_DIFFICULTY??2);
        assert.ok([1,2].includes(difficulty));
        const prefix=difficulty===2?[2,5]:[5];
        for(const key of prefix)press(key);
        const root={titleInstructions:0,inputTrace:prefix,expected:[...machine.memory(0x4806,21)],state:[...machine.memory(0x4800,64)],map:[...machine.memory(c.FLOORS,768)],sha256:createHash("sha256").update(application).digest("hex")};
        await writeFile(resolve(outputDirectory,`${difficulty===2?"hard":"normal"}-root.json`),JSON.stringify(root,null,2)+"\n");
        console.log({capturedFloor:1,seed:word("G_SEED")});
    } else if (mode === "replay") {
        const record = JSON.parse(await readFile(new URL("hard-clear.json",import.meta.url)));
        assert.equal(record.sha256, createHash("sha256").update(application).digest("hex"));
        const replayFloors=[];let replayFloor=-1;
        for (let i=0; i<record.inputTrace.length; i++) {
            if (i===1 && record.titleInstructions) {release(); machine.step(); machine.run(record.titleInstructions);}
            press(record.inputTrace[i]);
            if(read("G_MODE")===1&&read("G_FLOOR")!==replayFloor){
                replayFloor=read("G_FLOOR");replayFloors.push({floor:replayFloor+1,turns:word("G_TURNS"),hp:read("G_HP"),food:read("G_FOOD"),attack:read("G_ATTACK"),defense:read("G_DEFENSE")});
            }
        }
        assert.equal(read("G_DIFFICULTY"),2); assert.equal(read("G_FLOOR"),19);
        assert.equal(read("G_MODE"),9); assert.equal(read("G_TREASURE"),1);
        assert.equal(word("G_TURNS"),record.playResult.turns); assert.equal(read("G_HP"),record.playResult.hp);
        assert.equal(replayFloors.length,20);
        await screen("hard-replay.svg");
        const replayResult={floorHistory:replayFloors,turns:word("G_TURNS"),hp:read("G_HP"),treasure:read("G_TREASURE"),mode:read("G_MODE"),inputCount:record.inputTrace.length,
            elapsedMilliseconds:Math.round(performance.now()-startedAt),sha256:createHash("sha256").update(application).digest("hex")};
        await writeFile(resolve(outputDirectory,"hard-replay.json"),JSON.stringify(replayResult,null,2)+"\n");
        console.log("Hard clear replay passed without a planner or RAM patches",replayResult);

    } else if (!["test", "play"].includes(mode)) {
        if (mode === "debug") console.log({symbols, state: machine.state()});
    } else {
        if (mode === "test") {
            release();machine.setKeyboardKeyState("return",true);
            machine.loadProgram(application);ready();machine.step();machine.run(10000);
            assert.equal(read("G_MODE"),0,"Held launch RETURN skipped the title");
            const timedStarts=[];
            for(const wait of [0,1000,10000,100000,1000]){
                release();machine.loadProgram(application);ready();machine.step();if(wait)machine.run(wait);press(5);
                timedStarts.push({seed:word("G_SEED"),terrain:createHash("sha256").update(machine.memory(c.FLOORS,384)).digest("hex")});
            }
            assert.deepEqual(timedStarts[1],timedStarts[4],"Identical input timing is not reproducible");
            assert.equal(new Set(timedStarts.slice(0,4).map(r=>r.terrain)).size,4,"Title timing did not change maps");
            release();machine.loadProgram(application);ready();
            press(5); assert.equal(read("G_MODE"), 1); await screen("dungeon.svg");
            assert.equal(read("G_DEPTH"), 10); validateFloor();
            const original = stableState(); press(5); press(5); press(6); press(6);
            assert.deepEqual(stableState(), original, "Menus consumed a turn or RNG");
            // 1,000 independent seeds, all difficulties and floor depths.
            const layouts=new Set(), arrivals=new Set(), exits=new Set(), graphs=new Set(), roomCounts=new Set(), partitionCounts=new Set();
            let corridorOnly=0,largeRooms=0;
            for (let seed = 1; seed <= 1000; seed++) {
                const difficulty = seed % 3, depth = [5, 10, 20][difficulty];
                patch([[c.G_RNG, [seed >> 8, seed & 255]], [c.G_DIFFICULTY, [difficulty]],
                    [c.G_FLOOR, [seed % depth]], [c.G_DEPTH, [depth]]]);
                const generated=generateFloor(machine.memory(0x4800,64));
                call("generate_world"); validateFloor(); seeds++;
                assert.deepEqual(machine.memory(c.FLOORS,768),generated.map,"Generation model mismatch");
                assert.deepEqual(machine.memory(c.G_X,2),generated.state.slice(8,10),"Arrival model mismatch");
                layouts.add(createHash("sha256").update(generated.map.slice(0,384)).digest("hex"));
                arrivals.add(read("G_X")+read("G_Y")*24);
                exits.add(generated.map.slice(0,384).findIndex(v=>v===3||v===4));
                graphs.add([...machine.memory(c.GEN_LINKS,read("GEN_N"))].join(","));
                roomCounts.add(read("GEN_ROOMS"));partitionCounts.add(read("GEN_N"));
                if(read("GEN_OMITTED"))corridorOnly++;
                if([...machine.memory(c.RECT_W,read("GEN_N"))].some(v=>v>=10)||[...machine.memory(c.RECT_H,read("GEN_N"))].some(v=>v>=10))largeRooms++;
                assert.deepEqual(machine.memory(c.G_RNG,2),generated.state.slice(25,27),"Generation RNG mismatch");
            }
            assert.ok(layouts.size>=950,"Terrain variety collapsed");
            assert.ok(arrivals.size>=60&&exits.size>=40,"Arrival/exit positions collapsed");
            assert.ok(graphs.size>=10,"Corridor topology collapsed");
            assert.deepEqual([...roomCounts].sort(),[4,5,6]);
            assert.ok(corridorOnly>100&&largeRooms>100,"Structural variations are missing");
            terrainDiversity={roomCounts:[...roomCounts].sort(),partitionCounts:[...partitionCounts].sort(),corridorOnly,largeRooms,layouts:layouts.size,arrivals:arrivals.size,exits:exits.size,graphs:graphs.size,timedStarts};
            console.log(terrainDiversity);
            assert.equal(kindSeen.size, 12);
            arena(); press(1); assert.equal(read("G_Y"), 3); press(2); assert.equal(read("G_Y"), 4);
            press(3); assert.equal(read("G_X"), 3); press(4); assert.equal(read("G_X"), 4);
            arena(); put("G_X", 1); press(3); assert.equal(word("G_TURNS"), 0, "Wall bumped time");
            arena({enemies: [[5, 4, 2, 5, 255, 255, 0, 0]]});
            press(4); assert.equal(machine.memory(c.FLOORS + 387, 1)[0], 2); assert.equal(read("G_HP"), 17);
            machine.step(); machine.run(20000); assert.equal(word("G_TURNS"), 1, "Held key repeated attack");
            press(4); assert.equal(machine.memory(c.FLOORS + 387, 1)[0], 0); assert.equal(read("G_X"), 4);
            // All diagonals share melee/pickup/turn rules, without held repetition.
            for (const [key, dx, dy] of [[7, -1, -1], [8, 1, -1], [9, -1, 1], [10, 1, 1]]) {
                arena(); press(key);
                assert.deepEqual([read("G_X"), read("G_Y"), word("G_TURNS")], [4 + dx, 4 + dy, 1]);
                machine.step(); machine.run(20000);
                assert.equal(word("G_TURNS"), 1, "Held diagonal repeated");
                for (const [wx, wy] of [[4 + dx, 4], [4, 4 + dy], [4 + dx, 4 + dy]]) {
                    arena(); patch([[c.FLOORS + wy * 24 + wx, [0]]]);
                    const before = stableState(); press(key);
                    assert.deepEqual(stableState(), before, "Diagonal crossed wall or ticked time");
                }
                arena({enemies: [[4 + dx, 4 + dy, 2, 5, 255, 255, 0, 0]]}); press(key);
                assert.equal(machine.memory(c.FLOORS + 387, 1)[0], 2);
                assert.deepEqual([read("G_X"), read("G_Y"), word("G_TURNS")], [4, 4, 1]);
                arena({items: [[4 + dx, 4 + dy, 1]]}); press(key); assert.equal(read("G_BAG"), 1);
            }
            arena({x: 1, y: 1}); press(7); assert.equal(word("G_TURNS"), 0);
            arena({x: 22, y: 14}); press(10); assert.equal(word("G_TURNS"), 0);
            // Dedicated wait is the same turn as the menu action, including hazards.
            arena({hp: 12});
            for (let i = 0; i < 12; i++) press(11);
            assert.deepEqual([read("G_X"), read("G_Y"), read("G_HP"), read("G_FOOD"), word("G_TURNS")], [4, 4, 13, 188, 12]);
            machine.step(); machine.run(20000); assert.equal(word("G_TURNS"), 12, "Held wait repeated");
            arena(); press(11); assert.equal(read("G_HP"), 20, "Wait exceeded max HP");
            arena({food: 0}); press(11); press(11); assert.equal(read("G_HP"), 19);
            arena({hp: 12}); put("G_POISON", 6); press(11); press(11); assert.equal(read("G_HP"), 11);
            arena({enemies: [[5, 4, 2, 5, 255, 255, 0, 0]]}); press(11); assert.equal(read("G_HP"), 17);
            arena({hp: 12}); waitTurn(); const menuWait = stableState();
            arena({hp: 12}); press(11); assert.deepEqual(stableState(), menuWait);
            press(5); const inMenu = stableState(); press(11); press(7);
            assert.equal(read("G_MODE"), 2); assert.deepEqual(stableState(), inMenu);
            press(6); press(5); press(2); press(2); press(5);
            assert.equal(read("G_MODE"), 5); press(7);
            assert.deepEqual([read("G_INSPECT_X"), read("G_INSPECT_Y")], [3, 3]);
            press(11); assert.equal(read("G_MODE"), 5); press(6);
            // Hunger, starvation cadence, healing and progression.
            arena({food: 0}); waitTurn(); assert.equal(read("G_HP"), 20); waitTurn(); assert.equal(read("G_HP"), 19);
            arena({hp: 12}); put("G_REGEN", 11); waitTurn(); assert.equal(read("G_HP"), 13);
            arena({enemies: [[5, 4, 5, 3, 255, 255, 0, 0]]}); put("G_KILLS", 2); press(4);
            assert.equal(read("G_LEVEL"), 2); assert.equal(read("G_MAX_HP"), 22);
            // Auto pickup, full bag, identification and equipment exchange.
            arena({items: [[5, 4, 1]]}); press(4); assert.equal(read("G_BAG"), 1);
            arena({items: [[5, 4, 1]]}); patch([[c.G_BAG, [1, 1, 1, 1, 1, 1]]]); press(4);
            assert.equal(machine.memory(c.FLOORS + c.ITEM_START + 2, 1)[0], 1);
            arena({hp: 10}); patch([[c.G_BAG, [2, 2, 0, 0, 0, 0]]]); bagAction(0);
            assert.equal(read("G_HP"), 18); assert.equal(read("G_KNOWN"), 1); assert.equal(read("G_BAG"), 0);
            arena(); patch([[c.G_BAG, [8, 0, 0, 0, 0, 0]], [c.G_WEAPON, [7]]]); put("G_ATTACK", 4);
            bagAction(0); assert.equal(read("G_ATTACK"), 5); assert.equal(read("G_WEAPON"), 8);
            assert.equal(machine.memory(c.FLOORS + c.ITEM_START + 2, 1)[0], 7);
            arena(); put("G_BAG", 1); bagAction(0, 1); assert.equal(read("G_BAG"), 0);
            assert.equal(machine.memory(c.FLOORS + c.ITEM_START + 2, 1)[0], 1);
            // Move presses arriving throughout CPU drawing and LCD transfer are latched.
            for (let offset = 0; offset <= 48000; offset += 2000) {
                arena();
                const address = symbols.redraw;
                machine.loadProgram(fixtureApp(0x5600, [[0x5600, [0x8e, 0x5f, 0xff, 0x7e, address >> 8, address & 255]]]));
                if (offset) machine.run(offset);
                machine.setKeyboardKeyState("keypad-6", true);
                const untilCycle = Number(machine.state().cycleCount) + 49152;
                while (Number(machine.state().cycleCount) < untilCycle) machine.run(100);
                machine.setKeyboardKeyState("keypad-6", false);
                machine.run(150000);
                assert.equal(word("G_TURNS"), 1, `Short press lost/repeated at instruction offset ${offset}`);
                assert.equal(read("G_X"), 5); lcd();
            }
            // Auto-walk stops before loot, at a branch, and when new terrain appears.
            for (const scenario of ["end", "item", "branch", "unknown"]) {
                arena();
                const corridor = new Uint8Array(384).fill(128);
                for (let x = 1; x <= 15; x++) corridor[4 * 24 + x] = 129;
                if (scenario === "branch") corridor[3 * 24 + 7] = 129;
                if (scenario === "unknown") corridor[4 * 24 + 10] = 1;
                patch([[c.FLOORS, corridor]]);
                if (scenario === "item") patch([[c.FLOORS + c.ITEM_START, [9, 4, 1]]]);
                machine.setKeyboardKeyState("keypad-6", true);
                machine.step(); machine.run(1500000);
                machine.setKeyboardKeyState("keypad-6", false);
                assert.equal(read("G_X"), {end: 15, item: 8, branch: 7, unknown: 5}[scenario], scenario);
                assert.equal(read("G_AUTOWALK"), 0); lcd();
            }
            arena({hp: 3}); put("G_BAG", 3); put("G_REGEN", 11); bagAction(0);
            assert.equal(read("G_MODE"), 8, "Fatal poison was resurrected by regeneration");
            arena(); put("G_BAG", 4); bagAction(0); assert.equal(read("G_BUFF"), 12);
            for (let i = 0; i < 12; i++) waitTurn(); assert.equal(read("G_BUFF"), 0);
            arena(); put("G_BAG", 5); bagAction(0);
            assert.ok([...machine.memory(c.FLOORS, 384)].every(value => value & 128));
            assert.equal(read("G_KNOWN"), 8);
            arena(); put("G_BAG", 6); bagAction(0);
            assert.equal(read("G_X"), 1); assert.equal(read("G_Y"), 1); assert.equal(read("G_KNOWN"), 16);
            // Special enemies: poison, rust, theft, ranged attack and regeneration.
            arena({enemies: [[5, 4, 10, 5, 255, 255, 0, 0]]}); waitTurn(); assert.equal(read("G_POISON"), 6);
            arena({enemies: [[5, 4, 11, 8, 255, 255, 0, 0]]}); put("G_DEFENSE", 2); put("G_ARMOR", 11);
            waitTurn(); assert.equal(read("G_DEFENSE"), 1); assert.equal(read("G_ARMOR"), 10);
            arena({enemies: [[5, 4, 9, 5, 255, 255, 0, 0]]}); patch([[c.G_GOLD, [0, 20]]]);
            waitTurn(); assert.equal(word("G_GOLD"), 10); waitTurn(); assert.equal(machine.memory(c.FLOORS + 384, 1)[0], 6);
            arena({enemies: [[8, 4, 12, 10, 255, 255, 0, 0]]}); waitTurn(); assert.equal(read("G_HP"), 16);
            arena({enemies: [[5, 4, 8, 8, 255, 255, 0, 0]], turns: 3}); waitTurn();
            assert.equal(machine.memory(c.FLOORS + 387, 1)[0], 9);
            // Descent discards the previous floor; no ascent command exists.
            arena(); patch([[c.FLOORS + 100, [131]]]); press(5); press(5);
            assert.equal(read("G_FLOOR"), 1); validateFloor();
            arena({floor: 9, items: []}); patch([[c.FLOORS + 101, [132]]]); press(4);
            assert.equal(read("G_MODE"), 9, "Relic pickup did not win immediately"); await screen("victory.svg");
            arena({hp: 1, enemies: [[5, 4, 2, 5, 255, 255, 0, 0]]});
            patch([[c.G_GOLD, [255, 255]]]); waitTurn();
            assert.equal(read("G_MODE"), 8); await screen("death.svg");
            // Dense visibility/AI stress fixture reaches the 32-enemy storage ceiling.
            const crowd = [];
            for (let y = 5; y <= 10 && crowd.length < 32; y++) for (let x = 7; x <= 17 && crowd.length < 32; x++) {
                if (x === 12 && y === 8) continue;
                crowd.push([x, y, crowd.length % 12 + 1, 12, 255, 255, 0, 0]);
            }
            arena({enemies: crowd, x: 12, y: 8, hp: 250}); put("G_MAX_HP", 250); put("G_DEFENSE", 3);
            for (let i = 0; i < 6; i++) waitTurn();
            // Differential search-model checks always use the original machine program.
            function compareModel(action) {
                const before = {state: machine.memory(0x4800,64), map: machine.memory(c.FLOORS,768)};
                const expected = createSearchModel(before.map).step(before, action);
                if (!expected) return;
                if (action.slot === undefined) press(action.key); else bagAction(action.slot, action.discard ? 1 : 0);
                assert.equal(read("G_MODE"), expected.state[5]);
                assert.deepEqual(machine.memory(0x4806,21), expected.state.slice(6,27), "Model gameplay mismatch");
                assert.deepEqual(machine.memory(c.G_BAG,14), expected.state.slice(30,44), "Model inventory mismatch");
                assert.deepEqual(machine.memory(c.G_POISON,2), expected.state.slice(59,61), "Model poison mismatch");
                assert.deepEqual(machine.memory(c.FLOORS,768), expected.map, "Model floor mismatch");
                modelChecks++;
            }
            for (let kind = 1; kind <= 12; kind++) {
                arena({hp:200, enemies:[[8,4,kind,12,255,255,0,0]], food:kind%2 ? 0 : 200});
                put("G_MAX_HP", 220); put("G_DIFFICULTY", 2); put("G_DEFENSE", 3); put("G_ARMOR", 12);
                for (const key of [11,4,10,11,7,4,4,11,3,9,8,11]) compareModel({key});
            }
            for (let item = 1; item <= 12; item++) {
                arena({hp:10, enemies:[[8,4,6,4,255,255,0,0]]});
                put("G_DIFFICULTY",2); patch([[c.G_BAG,[item,0,0,0,0,0]]]);
                compareModel({slot:0});
                for (const key of [4,2,3,11]) compareModel({key});
            }
            arena(); patch([[c.G_BAG,[7,0,0,0,0,0]]]); compareModel({slot:0,discard:true});
            // Reaching an item with a full bag is not successful acquisition.
            for (const excessItem of [3,6,12]) {
                arena({food:20,items:[[5,4,1]]});put("G_DIFFICULTY",2);
                patch([[c.G_BAG,Array(6).fill(excessItem)]]);
                if(excessItem===12){put("G_ARMOR",12);put("G_DEFENSE",3);}
                const acquisition=chooseHardAction({initialNode:{state:machine.memory(0x4800,64),map:machine.memory(c.FLOORS,768)},target:101,quiet:true});
                assert.ok(acquisition.length,"No acquisition path with a full bag");
                for(const action of acquisition)compareModel(action);
                assert.ok(read("G_FOOD")>20,"Food was not picked up and consumed");
            }
            put("G_DIFFICULTY",2);patch([[c.G_SEED,[0,1]]]);call("new_game");
            const startTemplate={state:machine.memory(0x4800,64),map:machine.memory(c.FLOORS,768)},palettes=new Set();
            for(let seed=10;seed<74;seed++){
                const expected=newAdventure(startTemplate,seed);patch([[c.G_SEED,[0,seed]]]);call("new_game");
                assert.deepEqual(machine.memory(0x4806,21),expected.state.slice(6,27),"New adventure model mismatch");
                assert.deepEqual(machine.memory(c.G_BAG,14),expected.state.slice(30,44),"Initial identities mismatch");
                assert.deepEqual(machine.memory(c.FLOORS,768),expected.map,"Initial map mismatch");
                palettes.add([...machine.memory(c.G_IDENTITIES,5)].join(","));
            }
            assert.equal(palettes.size,8,"Initial shuffle cases are incomplete");
            console.log({modelChecks});
            // Whole-machine checkpoints retain the suspension screen and CPU state.
            arena(); press(4); press(5); press(2); press(2); press(2); press(5);
            assert.equal(read("G_SUSPEND"), 1); assert.equal(read("G_MODE"), 7);
            const saved = machine.exportState();
            const savedWorld = stableState();
            const savedLcd = lcd().dots;
            machine.importState(saved);
            assert.equal(read("G_SUSPEND"), 1); assert.equal(read("G_MODE"), 7);
            assert.deepEqual(lcd().dots, savedLcd);
            press(5); assert.equal(read("G_SUSPEND"), 0);
            assert.deepEqual(stableState(), savedWorld);
            press(4); const expected = stableState(), expectedLcd = lcd().dots;
            machine.importState(saved); press(5); press(4);
            assert.deepEqual(stableState(), expected); assert.deepEqual(lcd().dots, expectedLcd);
            const beforeRejected = stableState();
            assert.throws(() => machine.importState(Uint8Array.of(1)));
            assert.deepEqual(stableState(), beforeRejected);
            assert.deepEqual(lcd().dots, expectedLcd);
        }
        // Complete an adventure through physical key input only. The route
        // planner reads the full map as a test oracle; it does not mutate game RAM.
        for (const key of keys.slice(1)) machine.setKeyboardKeyState(key, false);
        machine.loadProgram(application); ready();
        const playDifficulty = Number(process.env.RELIC_DIVE_PLAY_DIFFICULTY ?? (mode === "play" ? 1 : 0));
        assert.ok([0, 1, 2].includes(playDifficulty));
        if (mode === "play") {
            recording = true;
            const prefix=savedPrefix;
            if(prefix){
                assert.equal(prefix.sha256,createHash("sha256").update(application).digest("hex"));
                for(let i=0;i<prefix.inputTrace.length;i++){
                    if(i===1&&prefix.titleInstructions){release();machine.step();machine.run(prefix.titleInstructions);}
                    press(prefix.inputTrace[i]);
                }
                assert.deepEqual([...machine.memory(0x4806,21)],prefix.expected);
            } else {
            if (playDifficulty === 0) press(1);
            if (playDifficulty === 2) press(2);
            assert.ok(Number.isSafeInteger(titleInstructions) && titleInstructions >= 0 && titleInstructions <= 1_000_000);
            if (titleInstructions) { release(); machine.step(); machine.run(titleInstructions); }
            press(5);
            }
        } else {
            patch([[c.G_DIFFICULTY, [playDifficulty]], [c.G_SEED, [0, 1]]]); call("new_game");
        }
        assert.ok(playDifficulty!==2||offlinePlan,"HARD play requires RELIC_DIVE_PLAN; use replay mode for the verified clear");
        const adventureSeed = word("G_SEED"), floorHistory = [];
        let previousFloor = -1;
        let playActions = 0;
        while (read("G_MODE") === 1 && playActions++ < (playDifficulty === 2 ? 3000 : 1200)) {
            if (read("G_FLOOR") !== previousFloor) {
                previousFloor = read("G_FLOOR");
                const progress = {floor: previousFloor + 1, turns: word("G_TURNS"), hp: read("G_HP"), food: read("G_FOOD"), attack: read("G_ATTACK"), defense: read("G_DEFENSE")};
                floorHistory.push(progress); console.log(progress);
                if(mode==="play" && playDifficulty===2)await writeFile(resolve(outputDirectory,"hard-prefix.json"),JSON.stringify({titleInstructions:titleInstructions,inputTrace,expected:[...machine.memory(0x4806,21)],state:[...machine.memory(0x4800,64)],map:[...machine.memory(c.FLOORS,768)],sha256:createHash("sha256").update(application).digest("hex")},null,2));
            }
            if (playDifficulty === 2 && playActions % 50 === 0) console.log({actions:playActions, turn:word("G_TURNS"), floor:read("G_FLOOR")+1, hp:read("G_HP"), food:read("G_FOOD")});
            if(offlinePlan){
                const action=offlinePlan.actions[offlineIndex++];assert.ok(action,"Offline plan exhausted");
                const before={state:machine.memory(0x4800,64),map:machine.memory(c.FLOORS,768)};
                const expected=action.descend?descend(before):createSearchModel(before.map).step(before,action);
                assert.ok(expected&&expected.state[5]!==8,"Offline action is fatal or invalid");
                if(action.descend){assert.equal(before.map[before.state[9]*24+before.state[8]]&127,3);press(5);press(5);}
                else if(action.slot===undefined)press(action.key);else bagAction(action.slot,action.discard?1:0);
                assert.equal(read("G_MODE"),expected.state[5]);
                assert.deepEqual(machine.memory(0x4806,21),expected.state.slice(6,27),"Offline gameplay diverged");
                assert.deepEqual(machine.memory(c.FLOORS,768),expected.map,"Offline map diverged");
                assert.deepEqual(machine.memory(c.G_BAG,14),expected.state.slice(30,44),"Offline inventory diverged");
                assert.deepEqual(machine.memory(c.G_POISON,2),expected.state.slice(59,61),"Offline poison diverged");
                continue;
            }
            const bag = machine.memory(c.G_BAG,6), identities = machine.memory(c.G_IDENTITIES,5);
            const map = machine.memory(c.FLOORS,768), px=read("G_X"), py=read("G_Y"), here=py*24+px;
            const enemies=Array.from({length:c.MAX_ENEMIES},(_,i)=>map.slice(384+i*8,392+i*8)).filter(e=>e[3]);
            const nearby=enemies.filter(e=>Math.abs(e[0]-px)+Math.abs(e[1]-py)<=6);
            let used=false;
            for(let slot=0;slot<6;slot++){
                const item=bag[slot];
                if(item===1&&read("G_FOOD")<160
                    ||item>=2&&item<=4&&identities[item-2]===0&&read("G_HP")<=read("G_MAX_HP")-[10,7][playDifficulty]
                    ||playDifficulty>0&&item>=2&&item<=4&&identities[item-2]===2&&nearby.length&&!read("G_BUFF")
                    ||item>=7&&item<=9&&item>read("G_WEAPON")||item>=10&&item<=12&&item>read("G_ARMOR")){
                    bagAction(slot);used=true;break;
                }
            }
            if(used)continue;
            if(playDifficulty>0){
                const adjacent=enemies.filter(e=>Math.abs(e[0]-px)<=1&&Math.abs(e[1]-py)<=1
                    &&(map[py*24+e[0]]&127)&&(map[e[1]*24+px]&127));
                adjacent.sort((a,b)=>Number(b[2]===11)-Number(a[2]===11)||a[3]-b[3]);
                if(adjacent.length){
                    const dx=adjacent[0][0]-px,dy=adjacent[0][1]-py;
                    press(dy<0?(dx<0?7:dx>0?8:1):dy>0?(dx<0?9:dx>0?10:2):dx<0?3:4);continue;
                }
                if(!nearby.length&&read("G_HP")<read("G_MAX_HP")-2&&read("G_FOOD")>95){press(11);continue;}
                if(!bag.includes(0)){
                    const junk=bag.findIndex(item=>item>=7||item>=2&&item<=4&&identities[item-2]===1||item>=5&&item<=6);
                    if(junk>=0&&!Array.from({length:c.MAX_ITEMS},(_,i)=>c.ITEM_START+i*3).some(o=>map[o+2]&&map[o]===px&&map[o+1]===py)){
                        bagAction(junk,1);continue;
                    }
                }
            }
            const queue=[here],paths=new Map([[here,[]]]);
            for(let i=0;i<queue.length;i++){
                const current=queue[i];
                for(const [d,key]of(playDifficulty===0?[[-24,1],[24,2],[-1,3],[1,4]]:[[-25,7],[-23,8],[23,9],[25,10],[-24,1],[24,2],[-1,3],[1,4]])){
                    const next=current+d;
                    if(key>=7){const dx=[7,9].includes(key)?-1:1,dy=key<=8?-24:24;if(!(map[current+dx]&127)||!(map[current+dy]&127))continue;}
                    if(next<0||next>=384||!(map[next]&127)||paths.has(next))continue;
                    paths.set(next,[...paths.get(current),key]);queue.push(next);
                }
            }
            const targets=[];
            if(bag.includes(0))for(let i=0;i<c.MAX_ITEMS;i++){
                const o=c.ITEM_START+i*3,item=map[o+2],pos=map[o+1]*24+map[o];
                if(item===1||item>=7&&item<=9&&item>read("G_WEAPON")||item>=10&&item<=12&&item>read("G_ARMOR")
                    ||item>=2&&item<=4&&identities[item-2]===0){if(paths.has(pos)&&pos!==here)targets.push(paths.get(pos));}
            }
            if(!targets.length){
                const goal=map.slice(0,384).findIndex(v=>[3,4].includes(v&127));
                if(goal===here){press(5);press(5);continue;}
                targets.push(paths.get(goal));
            }
            targets.sort((a,b)=>a.length-b.length);assert.ok(targets[0]?.length,"Autoplay has no route");press(targets[0][0]);
        }
        if(offlinePlan){assert.equal(offlineIndex,offlinePlan.actions.length);assert.deepEqual([...machine.memory(0x4806,21)],offlinePlan.expected);}
        const playResult = {difficulty: playDifficulty, mode: read("G_MODE"), floor: read("G_FLOOR") + 1,
            turns: word("G_TURNS"), hp: read("G_HP"), actions: playActions, elapsedMilliseconds: Math.round(performance.now()-startedAt)};
        console.log({playResult});
        if (mode === "play") {
            const playName = ["easy", "normal", "hard"][playDifficulty];
            await screen(`${playName}-play.svg`);
            await writeFile(resolve(outputDirectory, `${playName}-play.json`), JSON.stringify({titleInstructions: titleInstructions, adventureSeed, floorHistory, playResult, inputTrace, keyNames: keys,
                sha256: createHash("sha256").update(application).digest("hex"), oracle: offlinePlan ? "Full map and item identities read; candidate actions searched in a lightweight model and checked against the original emulator; actual adventure receives keys only" : "Full map and item identities read; no RAM writes after loading application"}, null, 2) + "\n");
        }
        assert.equal(read("G_MODE"), 9, "End-to-end adventure did not finish");
        assert.equal(read("G_DIFFICULTY"), playDifficulty);
        assert.equal(read("G_FLOOR") + 1, [5, 10, 20][playDifficulty]);
        assert.equal(read("G_TREASURE"), 1);
        assert.ok(read("G_HP") > 0);
        // Include status-polling instructions; the immediate-BUSY model excludes
        // the physical LCD's additional wait. This is an emulator CPU-work bound.
        if(mode === "test")assert.ok(maxTurnCycles <= 307200, `Turn exceeded 250 ms: ${maxTurnCycles}`);
        if (mode === "test") await writeFile(resolve(outputDirectory, "verification.json"), JSON.stringify({seeds, boundaries, modelChecks, terrainDiversity,
            enemyKinds: [...kindSeen].sort((a, b) => a - b), maxTurnCycles, maxFloorCycles, playResult,
            maxTurnMilliseconds: maxTurnCycles / 1228.8, sha256: createHash("sha256").update(application).digest("hex")}, null, 2) + "\n");
        console.log(JSON.stringify({seeds, boundaries, modelChecks, maxTurnCycles, maxTurnMilliseconds: maxTurnCycles / 1228.8}));
    }
} catch(error) {
    if(mode==="play"&&recording&&read("G_DIFFICULTY")===2)await writeFile(resolve(outputDirectory,"hard-progress.json"),JSON.stringify({
        titleInstructions:titleInstructions,inputTrace,
        expected:[...machine.memory(0x4806,21)],sha256:createHash("sha256").update(application).digest("hex"),state:[...machine.memory(0x4800,64)],map:[...machine.memory(c.FLOORS,768)],error:String(error)
    },null,2));
    throw error;
} finally { machine.destroy(); }
