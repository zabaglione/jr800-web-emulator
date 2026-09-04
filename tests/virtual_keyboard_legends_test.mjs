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
    virtualKeyboardLegend,
} = await import(pathToFileURL(modulePath));

assert.equal(Jr800VirtualLegendKeys.length, 57);
assert.equal(new Set(Jr800VirtualLegendKeys).size, Jr800VirtualLegendKeys.length);
assert.deepEqual(Jr800VirtualControlModes, ["unknown", "function", "control"]);

assert.equal(virtualKeyboardLegend("main-1"), "1");
assert.equal(virtualKeyboardLegend("main-1", {shift: true}), "!");
assert.equal(virtualKeyboardLegend("main-caret", {shift: true}), "¥");
assert.equal(virtualKeyboardLegend("semicolon", {shift: true}), "?");
assert.equal(virtualKeyboardLegend("colon", {shift: true}), "-");
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
