// SPDX-License-Identifier: MIT
// Execute real HD6301 instructions through the JR-800 WASM hardware model.
import assert from "node:assert/strict";
import {createHash} from "node:crypto";
import {readFile, writeFile} from "node:fs/promises";
import {resolve} from "node:path";
import {pathToFileURL} from "node:url";

const [wasmDirectory, outputDirectory, sample, mode = "test"] = process.argv.slice(2);
assert.ok(wasmDirectory && outputDirectory && sample,
    "Usage: node check.mjs <wasm-dir> <output-dir> <sample> [run|debug|test]");
assert.ok(["run", "debug", "test"].includes(mode), "Unknown mode");
const moduleUrl = name => pathToFileURL(resolve(wasmDirectory, name)).href;
const {WasmMachine} = await import(moduleUrl("wasm-machine.mjs"));
const {jr800BasicBootExperimentConfiguration} = await import(moduleUrl("basic-boot-profile.mjs"));
const symbolsText = await readFile(resolve(outputDirectory, `${sample}.sym`), "utf8");
const symbols = Object.fromEntries([...symbolsText.matchAll(/^ \$([0-9A-F]+) G (\S+)/gm)]
    .map(([, address, name]) => [name, parseInt(address, 16)]));
assert.ok(symbols.frame_ready && symbols.framebuffer, "Required symbols missing");
const machine = await WasmMachine.createJr800(moduleUrl("jr800_wasm.mjs"),
    {...jr800BasicBootExperimentConfiguration(), ignoreUnsupportedIo: Boolean(process.env.JR800_SAMPLE_ROM)});
let frames = 0;
let firstPanel;
const read = name => machine.memory(symbols[name], 1)[0];
function frame() {
    if (frames) machine.step(); // Leave the previous frame_ready address.
    const stop = machine.runTo(symbols.frame_ready, 200_000);
    assert.equal(stop.reason, "address-reached", JSON.stringify(stop));
    assert.equal(machine.state().sp, 0x5fff, "Unbalanced subroutine stack");
    const panel = machine.lcdPanel();
    const framebuffer = machine.memory(symbols.framebuffer, 1536);
    assert.equal(panel.width, 192);
    assert.equal(panel.height, 64);
    for (let y = 0; y < 64; y++) {
        for (let x = 0; x < 192; x++) {
            const bit = (framebuffer[Math.floor(y / 8) * 192 + x] >> (y % 8)) & 1;
            assert.equal(panel.dots[y * 192 + x], bit + 1, `LCD mismatch at ${x},${y}`);
        }
    }
    assert.ok(panel.dots.includes(2), "Blank display");
    if (!firstPanel) firstPanel = panel;
    frames++;
    return panel;
}
function sprite(expectedPosition) {
    assert.equal(read("position"), expectedPosition);
    const band = machine.memory(symbols.framebuffer + 768, 192);
    const shape = [0x3c, 0x7e, 0xdb, 0xff, 0xff, 0xdb, 0x7e, 0x3c];
    assert.deepEqual([...band], Array.from({length: 192}, (_, x) =>
        shape[x - expectedPosition] ?? 0), "Sprite position or erasure failed");
}
const digits = [
    [0x3e,0x51,0x49,0x45,0x3e], [0,0x42,0x7f,0x40,0],
    [0x42,0x61,0x51,0x49,0x46], [0x41,0x49,0x49,0x49,0x36],
    [0x18,0x14,0x12,0x7f,0x10], [0x4f,0x49,0x49,0x49,0x31],
    [0x3e,0x49,0x49,0x49,0x30], [1,0x71,9,5,3],
    [0x36,0x49,0x49,0x49,0x36], [0x06,0x49,0x49,0x49,0x3e],
];
try {
    // Original two-byte BRA-to-self bootstrap, not a manufacturer ROM.
    // No bootstrap instruction is executed: loadProgram supplies the entry PC.
    const rom = new Uint8Array(32768).fill(1);
    rom.set([0x20, 0xfe]);
    rom[32766] = 0x80;
    if (process.env.JR800_SAMPLE_ROM) {
        const romPath = resolve(process.env.JR800_SAMPLE_ROM);
        const bytes = await readFile(romPath);
        if (romPath.endsWith(".j8r")) machine.loadJr8rom(bytes);
        else machine.loadLogicalRom(bytes);
        // Use the same explicit unmapped-I/O policy as normal browser boot.
        const boot = machine.run(1_000_000);
        assert.ok(["instruction-limit", "sleeping"].includes(boot.reason), JSON.stringify(boot));
    } else {
        machine.loadLogicalRom(rom);
    }
    const application = await readFile(resolve(outputDirectory, `${sample}.j8a`));
    machine.loadProgram(application);
    frame();
    if (sample === "01-hello") {
        assert.deepEqual([...machine.memory(0x4000 + 192 + 60, 6)],
            [0x7f, 8, 8, 8, 0x7f, 0], "H glyph");
        assert.deepEqual([...machine.memory(0x4000 + 960 + 57 + 4 * 6, 6)],
            [0x63, 0x14, 8, 0x14, 0x63, 0], "X glyph above font offset 255");
    } else if (sample === "02-checkerboard") {
        for (let y = 0; y < 64; y++) {
            for (let x = 0; x < 192; x++) {
                const expected = (Math.floor(x / 4) + Math.floor(y / 4)) % 2 === 0;
                assert.equal(firstPanel.dots[y * 192 + x], expected ? 2 : 1);
            }
        }
    } else if (sample === "03-counter") {
        for (let count = 0; count <= (mode === "test" ? 100 : 0); count++) {
            if (count) frame();
            const value = count % 100;
            const tens = Math.floor(value / 10), ones = value % 10;
            assert.equal(read("tens"), tens);
            assert.equal(read("ones"), ones);
            assert.deepEqual([...machine.memory(0x4000 + 576 + 90, 12)],
                [...digits[tens], 0, ...digits[ones], 0], `Decimal display ${value}`);
        }
    } else if (sample === "04-bounce") {
        for (let index = 0; index <= (mode === "test" ? 94 : 0); index++) {
            if (index) frame();
            const phase = index % 92;
            sprite(phase <= 46 ? phase * 4 : (92 - phase) * 4);
        }
    } else if (sample === "05-keypad") {
        sprite(92);
        if (mode === "test") {
            frame(); sprite(92); // Idle preserves position.
            machine.setKeyboardKeyState("keypad-4", true);
            for (let i = 1; i <= 25; i++) { frame(); sprite(Math.max(92 - 4 * i, 0)); }
            machine.setKeyboardKeyState("keypad-4", false);
            frame(); sprite(0);
            machine.setKeyboardKeyState("keypad-6", true);
            for (let i = 1; i <= 48; i++) { frame(); sprite(Math.min(4 * i, 184)); }
            machine.setKeyboardKeyState("keypad-6", false);
            frame(); sprite(184);
            machine.setKeyboardKeyState("keypad-5", true);
            frame(); sprite(184); // Unassigned keys do not move the sprite.
            machine.setKeyboardKeyState("keypad-5", false);
        }
        assert.ok(mode !== "test" || machine.keyboardActivity().readAttempts > 0);
    } else {
        throw new Error(`Unknown sample: ${sample}`);
    }
    const paths = [];
    firstPanel.dots.forEach((dot, index) => {
        if (dot === 2) paths.push(`M${index % 192} ${Math.floor(index / 192)}h1v1h-1z`);
    });
    await writeFile(resolve(outputDirectory, "screen.svg"),
        `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 64" width="768" height="256" shape-rendering="crispEdges"><rect width="192" height="64" fill="#b1b5a8"/><path fill="#4d544f" d="${paths.join("")}"/></svg>\n`);
    const result = {sample, mode, model: "JR-800 WASM",
        bootstrap: process.env.JR800_SAMPLE_ROM ? "owner-supplied" : "project-authored",
        frames, cycles: machine.state().cycleCount, firstFrameSha256:
            createHash("sha256").update(firstPanel.dots).digest("hex"), passed: true};
    await writeFile(resolve(outputDirectory, "verification.json"), JSON.stringify(result, null, 2) + "\n");
    console.log(JSON.stringify(result));
    if (mode === "debug") {
        console.log(JSON.stringify({frameReady: symbols.frame_ready, state: machine.state(), symbols}, null, 2));
    }
} finally {
    machine.destroy();
}
