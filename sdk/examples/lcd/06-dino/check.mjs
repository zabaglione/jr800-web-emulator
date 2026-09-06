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
const read = name => machine.memory(symbols[name], 1)[0];
const number = name => [...machine.memory(symbols[name], 3)].reduce((value, digit) => value * 10 + digit, 0);
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
        assert.deepEqual(frame().dots, ready.dots, "Ready screen must remain still");
        input(true);
        // Exact integer jump arc; holding and a second airborne press cannot reset it.
        const arc = [7, 13, 18, 22, 25, 27, 28, 28, 27, 25, 22, 18, 13, 7, 0];
        for (let index = 0; index < arc.length; index++) {
            if (index === 3) input(false);
            if (index === 4) input(true);
            frame();
            assert.equal(read("phase"), 1);
            assert.equal(read("height"), arc[index]);
            if (arc[index] > 0) checkDino("dino_air", arc[index]);
            if (index === 6) await savePanel("jump.svg");
        }
        assert.equal(read("velocity"), 0);
        frame(); assert.equal(read("height"), 0, "Held SPACE repeated the jump");
        const seenWalkPatterns = new Set();
        for (let i = 0; i < 5; i++) {
            frame();
            const pattern = read("animation") & 2 ? "dino_run_b" : "dino_run_a";
            checkDino(pattern, 0);
            seenWalkPatterns.add(pattern);
        }
        assert.equal(seenWalkPatterns.size, 2, "Missing walk animation");
        for (let i = 0; i < 100 && read("phase") === 1; i++) frame();
        assert.equal(read("phase"), 2, "Ground collision did not stop the game");
        const firstScore = number("distance");
        assert.ok(firstScore > 0);
        assert.equal(number("high_score"), firstScore);
        await savePanel("game-over.svg");
        const dead = machine.lcdPanel().dots;
        for (let i = 0; i < 5; i++) assert.deepEqual(frame().dots, dead, "Held SPACE restarted after death");
        input(false); frame();
        input(true); frame();
        assert.equal(read("phase"), 1);
        assert.equal(number("distance"), 0);
        assert.equal(number("high_score"), firstScore, "Retry lost the best score");
        input(false);

        // Play using actual SPACE events. No writes to gameplay state or PCG RAM.
        const speeds = new Set(), kinds = new Set();
        let jumps = 0;
        const recent = [];
        for (let i = 0; i < 4050; i++) {
            const x = read("obstacle_x"), speed = read("speed");
            const jump = read("height") === 0 && read("gap_wait") === 0
                && x >= 35 && x <= 35 + speed * 5;
            input(jump);
            if (jump) jumps++;
            recent.push({i, x, speed, height: read("height"), velocity: read("velocity"),
                gap: read("gap_wait"), previous: read("space_previous"), jump});
            if (recent.length > 20) recent.shift();
            frame();
            assert.equal(read("phase"), 1, `Autoplay collision: ${JSON.stringify(recent)}`);
            assert.ok(read("height") <= 28);
            speeds.add(read("speed")); kinds.add(read("obstacle_kind"));
        }
        assert.deepEqual([...speeds].sort(), [3, 4, 5], "Difficulty did not increase");
        assert.deepEqual([...kinds].sort(), [0, 1], "Missing cactus variant");
        assert.ok(jumps > 40, "Insufficient successful jumps");
        assert.equal(number("distance"), 999, "Distance did not saturate at 999");
        input(false);
        for (let i = 0; i < 150 && read("phase") === 1; i++) frame();
        assert.equal(read("phase"), 2);
        assert.equal(number("high_score"), 999);
        // Sweep short taps across rendering/transfer phases. The hold uses the
        // same minimum E-cycle duration as ordinary Web keyboard conditioning.
        function runCycles(count) {
            const end = Number(machine.state().cycleCount) + count;
            while (Number(machine.state().cycleCount) < end) {
                const stop = machine.run(64);
                assert.equal(stop.reason, "instruction-limit");
            }
        }
        for (let offset = 0; offset < 110_000; offset += 5000) {
            machine.loadProgram(application);
            frame();
            assert.equal(read("phase"), 0);
            runCycles(offset);
            input(true); runCycles(49_152); input(false);
            lastCycles = undefined; // Exclude deliberate mid-frame test advances.
            frame(); frame();
            assert.equal(read("phase"), 1, `Missed short SPACE tap at cycle offset ${offset}`);
            assert.ok(read("height") > 0, "Short SPACE tap did not jump");
        }
    }
    const result = {sample, mode, frames, passed: true, firstFrameSha256: readyHash,
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
