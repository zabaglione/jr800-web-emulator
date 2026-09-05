// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const [modulePath] = process.argv.slice(2);
if (!modulePath) {
    throw new Error(
        "Usage: node virtual_keyboard_legends_test.mjs <virtual-keyboard-legends.mjs>",
    );
}

const {
    Jr800VirtualControlModes,
    Jr800VirtualLegendKeys,
    virtualKeyboardLegend: keyFace,
    virtualKeyboardModes,
} = await import(pathToFileURL(modulePath));

const virtualKeyboardLegend = (key, modifiers) => keyFace(key, modifiers).label;

assert.equal(Jr800VirtualLegendKeys.length, 73);
assert.equal(new Set(Jr800VirtualLegendKeys).size, Jr800VirtualLegendKeys.length);
assert.deepEqual(Jr800VirtualControlModes, ["unknown", "function", "control"]);

assert.equal(virtualKeyboardLegend("main-1"), "1");
assert.equal(virtualKeyboardLegend("main-1", {shift: true}), "!");
assert.equal(virtualKeyboardLegend("main-caret", {shift: true}), "¥");
assert.equal(virtualKeyboardLegend("semicolon", {shift: true}), "?");
assert.equal(virtualKeyboardLegend("colon", {shift: true}), "_");
assert.equal(virtualKeyboardLegend("comma", {shift: true}), "<");
assert.equal(virtualKeyboardLegend("period", {shift: true}), ">");
assert.equal(virtualKeyboardLegend("pf-1", {shift: true}), "PF11");
assert.equal(virtualKeyboardLegend("pf-10", {shift: true}), "PF20");
assert.equal(virtualKeyboardLegend("insert-rub", {shift: true}), "INS");
assert.equal(virtualKeyboardLegend("vertical-arrows", {shift: true}), "↓");
assert.equal(virtualKeyboardLegend("horizontal-arrows", {shift: true}), "←");
assert.equal(virtualKeyboardLegend("home-cls", {shift: true}), "HOME");

assert.equal(virtualKeyboardLegend("main-1", {control: true}), "ERASE");
assert.equal(virtualKeyboardLegend("main-0", {control: true}), "GRAPH");
assert.equal(virtualKeyboardLegend("main-caret", {control: true}), "KANA");
assert.equal(virtualKeyboardLegend("letter-a", {control: true}), "CTRL A");
assert.equal(virtualKeyboardLegend("letter-b", {control: true}), "CTRL B");
assert.equal(
    virtualKeyboardLegend(
        "letter-a",
        {control: true, controlMode: "function"},
    ),
    "AUTO",
);
assert.equal(
    virtualKeyboardLegend(
        "letter-a",
        {control: true, controlMode: "control"},
    ),
    "INS",
);
assert.equal(
    virtualKeyboardLegend(
        "letter-c",
        {control: true, controlMode: "control"},
    ),
    "CANCEL",
);
assert.equal(virtualKeyboardLegend("break", {control: true}), "CLEAR");
assert.equal(virtualKeyboardLegend("return", {control: true}), "DATA");
assert.equal(virtualKeyboardLegend("insert-rub", {control: true}), "POKE");
assert.equal(virtualKeyboardLegend("home-cls", {control: true}), "LIST");
assert.equal(
    virtualKeyboardLegend("main-1", {shift: true, control: true}),
    "ERASE",
);
assert.equal(
    virtualKeyboardLegend("pf-1", {shift: true, control: true}),
    "PF11",
);

assert.throws(
    () => virtualKeyboardLegend("missing"),
    /legend key is unsupported/,
);
assert.throws(
    () => virtualKeyboardLegend("main-1", {shift: 1}),
    /Shift state must be boolean/,
);
assert.throws(
    () => virtualKeyboardLegend("main-1", {control: null}),
    /Control state must be boolean/,
);
assert.throws(
    () => virtualKeyboardLegend("main-1", {controlMode: "other"}),
    /control mode is unsupported/,
);

const {Jr800LcdIndicatorNames} = await import(
    new URL("./wasm-machine.mjs", pathToFileURL(modulePath)),
);
const indicators = Object.fromEntries(Jr800LcdIndicatorNames.map(name => [name, 0]));
const normal = virtualKeyboardModes(indicators);
assert.deepEqual(normal, {capitalLock: false, inputMode: "normal", controlMode: "function"});
assert.deepEqual(virtualKeyboardModes(null), {
    capitalLock: null, inputMode: "unknown", controlMode: "unknown",
});
for (const caps of [false, true]) {
    for (const shift of [false, true]) {
        const modes = virtualKeyboardModes({...indicators, "capital-lock": caps ? 4 : 0});
        const face = keyFace("letter-a", {...modes, shift});
        assert.equal(face.label, caps !== shift ? "A" : "a");
        assert.equal(face.characterCode, caps !== shift ? 0x41 : 0x61);
    }
}
assert.equal(keyFace("letter-a", {...normal, capitalLock: null}).characterCode, null);
assert.equal(virtualKeyboardModes({...indicators, "graphics-input": 1}).inputMode, "normal");
assert.equal(virtualKeyboardModes({...indicators, "graphics-input": null}).inputMode, "unknown");
assert.equal(virtualKeyboardModes({...indicators, "graphics-input": 4, "kana-input": 4}).inputMode, "unknown");

const graph = virtualKeyboardModes({...indicators, "graphics-input": 4});
assert.equal(graph.inputMode, "graphics");
const graphRows = [
    ["qwertyuiop", [0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x98, 0x91, 0x99]],
    ["asdfghjkl", [0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x87, 0x96, 0x93, 0x8f]],
    ["zxcvbnm", [0x8e, 0x8d, 0x94, 0x97, 0x9c, 0x9d, 0x9a]],
];
for (const [letters, codes] of graphRows) {
    for (const [index, letter] of [...letters].entries()) {
        for (const shift of [false, true]) {
            assert.equal(keyFace(`letter-${letter}`, {...graph, shift}).characterCode, codes[index]);
        }
    }
}
assert.equal(keyFace("main-caret", graph).characterCode, 0x9e);
assert.equal(keyFace("main-1", graph).characterCode, 0x31);
assert.equal(keyFace("keypad-7", graph).characterCode, 0x37);

const kana = virtualKeyboardModes({...indicators, "kana-input": 4});
assert.equal(kana.inputMode, "kana");
for (const [key, full, small] of [
    ["main-3", 0xb1, 0xa7], ["main-4", 0xb3, 0xa9],
    ["main-5", 0xb4, 0xaa], ["main-6", 0xb5, 0xab],
    ["main-7", 0xd4, 0xac], ["main-8", 0xd5, 0xad],
    ["main-9", 0xd6, 0xae], ["main-0", 0xdc, 0xa6],
    ["letter-e", 0xb2, 0xa8], ["letter-z", 0xc2, 0xaf],
    ["comma", 0xc8, 0xa4], ["period", 0xd9, 0xa1],
    ["keypad-1", 0xd1, 0xa3], ["keypad-0", 0xd2, 0xa5],
    ["keypad-5", 0xdf, 0xa2], ["keypad-8", 0xb0, 0xa0],
]) {
    assert.equal(keyFace(key, kana).characterCode, full);
    assert.equal(keyFace(key, {...kana, shift: true}).characterCode, small);
}
assert.equal(keyFace("keypad-4", kana).characterCode, 0xde);
assert.equal(keyFace("keypad-5", kana).characterCode, 0xdf);
assert.equal(keyFace("keypad-multiply", kana).characterCode, 0x7d);
assert.equal(keyFace("comma", graph).characterCode, 0x90);
assert.equal(keyFace("keypad-9", kana).blank, true);
assert.equal(keyFace("letter-a", {...kana, capitalLock: true, shift: true}).characterCode, 0xc1);
for (const mode of [normal, graph, kana]) {
    assert.equal(keyFace("main-9", {...mode, control: true}).label, "CAP.L");
    assert.equal(keyFace("main-0", {...mode, control: true}).label, "GRAPH");
    assert.equal(keyFace("main-caret", {...mode, control: true}).label, "KANA");
    assert.equal(keyFace("letter-a", {...mode, control: true}).label, "AUTO");
    assert.equal(keyFace("pf-1", {...mode, shift: true}).label, "PF11");
}
const controlMode = virtualKeyboardModes({...indicators, "control-mode": 4});
assert.equal(keyFace("letter-a", {...controlMode, control: true}).label, "INS");
assert.throws(() => keyFace("letter-a", {capitalLock: 1}), /Capital lock/);
assert.throws(() => keyFace("letter-a", {inputMode: "invalid"}), /input mode/);
