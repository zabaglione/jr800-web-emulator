// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const [modulePath] = process.argv.slice(2);
if (!modulePath) {
    throw new Error("Usage: node keyboard_input_test.mjs <keyboard-input.mjs>");
}

const keyboardInputModule = await import(pathToFileURL(modulePath));
const {
    Jr800HostKeyBinding,
    jr800KeyForHostCode,
} = keyboardInputModule;

assert.equal(
    Object.hasOwn(keyboardInputModule, "Jr800HostKeyboardState"),
    false,
    "Host and virtual sources must share one application-level aggregator",
);

for (const digit of Array.from({length: 10}, (_, index) => index)) {
    assert.equal(jr800KeyForHostCode(`Digit${digit}`), `main-${digit}`);
    assert.equal(jr800KeyForHostCode(`Numpad${digit}`), `keypad-${digit}`);
}
for (const letter of "ABCDEFGHIJKLMNOPQRSTUVWXYZ") {
    assert.equal(
        jr800KeyForHostCode(`Key${letter}`),
        `letter-${letter.toLowerCase()}`,
    );
}
for (const number of Array.from({length: 10}, (_, index) => index + 1)) {
    assert.equal(jr800KeyForHostCode(`F${number}`), `pf-${number}`);
}
assert.equal(jr800KeyForHostCode("ShiftLeft"), "shift");
assert.equal(jr800KeyForHostCode("ShiftRight"), "shift");
assert.equal(jr800KeyForHostCode("ControlLeft"), "control");
assert.equal(jr800KeyForHostCode("ControlRight"), "control");
assert.equal(jr800KeyForHostCode("Space"), "space");
assert.equal(jr800KeyForHostCode("Enter"), "return");
assert.equal(jr800KeyForHostCode("NumpadEnter"), "return");
assert.equal(jr800KeyForHostCode("ContextMenu"), "menu");
assert.equal(jr800KeyForHostCode("Pause"), "break");
assert.equal(jr800KeyForHostCode("Home"), "home-cls");
assert.equal(jr800KeyForHostCode("Equal"), "main-caret");
assert.equal(jr800KeyForHostCode("Semicolon"), "semicolon");
assert.equal(jr800KeyForHostCode("Quote"), "colon");
assert.equal(jr800KeyForHostCode("Comma"), "comma");
assert.equal(jr800KeyForHostCode("Period"), "period");
assert.equal(jr800KeyForHostCode("Backspace"), "keypad-insert-rub");
assert.equal(jr800KeyForHostCode("ArrowUp"), "keypad-vertical-arrows");
assert.equal(jr800KeyForHostCode("ArrowRight"), "keypad-horizontal-arrows");
assert.deepEqual(
    [
        "NumpadMultiply",
        "NumpadAdd",
        "NumpadEqual",
        "NumpadSubtract",
        "NumpadDecimal",
        "NumpadDivide",
    ].map((code) => jr800KeyForHostCode(code)),
    [
        "keypad-multiply",
        "keypad-add",
        "keypad-equal",
        "keypad-subtract",
        "keypad-decimal",
        "keypad-divide",
    ],
);
assert.equal(jr800KeyForHostCode("Escape"), null);
assert.equal(Object.keys(Jr800HostKeyBinding).length, 80);
assert.equal(new Set(Object.values(Jr800HostKeyBinding)).size, 77);
assert.throws(
    () => jr800KeyForHostCode(1),
    /Host keyboard code must be a string/,
);
