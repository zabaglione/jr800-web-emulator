// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const [modulePath] = process.argv.slice(2);
if (!modulePath) {
    throw new Error(
        "Usage: node virtual_keyboard_input_test.mjs <virtual-keyboard-input.mjs>",
    );
}

const {
    Jr800VirtualKeyboardState,
    Jr800TypingRollover,
    Jr800VirtualLatchKeys,
    isJr800VirtualLatchKey,
} = await import(pathToFileURL(modulePath));

assert.deepEqual(Jr800VirtualLatchKeys, ["shift", "control"]);
assert.equal(isJr800VirtualLatchKey("shift"), true);
assert.equal(isJr800VirtualLatchKey("control"), true);
assert.equal(isJr800VirtualLatchKey("letter-a"), false);
assert.throws(
    () => isJr800VirtualLatchKey("missing"),
    /Virtual keyboard key is unsupported/,
);

const state = new Jr800VirtualKeyboardState();
assert.deepEqual(
    state.press("pointer:7", "letter-a"),
    {key: "letter-a", pressed: true},
);
assert.equal(state.press("pointer:7", "letter-a"), null);
assert.equal(state.isPressed("letter-a"), true);
assert.deepEqual(
    state.release("pointer:7"),
    {key: "letter-a", pressed: false},
);
assert.equal(state.release("pointer:7"), null);
assert.equal(state.isPressed("letter-a"), false);

assert.deepEqual(
    state.press("pointer:expanded", "keypad-divide"),
    {key: "keypad-divide", pressed: true},
);
assert.deepEqual(
    state.release("pointer:expanded"),
    {key: "keypad-divide", pressed: false},
);

assert.deepEqual(
    state.toggleLatch("control"),
    {key: "control", pressed: true},
);
assert.equal(state.isLatched("control"), true);
assert.equal(state.isPressed("control"), true);
assert.deepEqual(
    state.toggleLatch("control"),
    {key: "control", pressed: false},
);
assert.equal(state.isLatched("control"), false);
assert.throws(
    () => state.toggleLatch("letter-x"),
    /Virtual keyboard key cannot be latched/,
);

assert.deepEqual(
    state.toggleLatch("shift"),
    {key: "shift", pressed: true},
);
assert.equal(state.press("pointer:8", "shift"), null);
assert.equal(state.toggleLatch("shift"), null);
assert.deepEqual(
    state.release("pointer:8"),
    {key: "shift", pressed: false},
);

assert.deepEqual(
    state.press("pointer:9", "keypad-7"),
    {key: "keypad-7", pressed: true},
);
assert.throws(
    () => state.press("pointer:9", "keypad-6"),
    /Virtual keyboard source is already active/,
);
assert.deepEqual(
    state.toggleLatch("control"),
    {key: "control", pressed: true},
);
assert.deepEqual(state.releaseAll(), [
    {key: "control", pressed: false},
    {key: "keypad-7", pressed: false},
]);
assert.equal(state.isPressed("control"), false);
assert.equal(state.isLatched("control"), false);
assert.deepEqual(state.releaseAll(), []);

assert.throws(
    () => state.press("", "letter-a"),
    /Virtual keyboard source must be a nonempty string/,
);
assert.throws(
    () => state.press("pointer:10", "missing"),
    /Virtual keyboard key is unsupported/,
);

const rollover = new Jr800TypingRollover();
assert.deepEqual(rollover.transition({key: "letter-a", pressed: true}), [
    {key: "letter-a", pressed: true},
]);
assert.deepEqual(rollover.transition({key: "letter-b", pressed: true}), [
    {key: "letter-a", pressed: false},
    {key: "letter-b", pressed: true},
]);
assert.deepEqual(rollover.transition({key: "letter-a", pressed: false}), []);
assert.deepEqual(rollover.transition({key: "letter-b", pressed: true}), []);
assert.deepEqual(rollover.transition({key: "shift", pressed: true}), [
    {key: "shift", pressed: true},
]);
assert.deepEqual(rollover.transition({key: "letter-c", pressed: true}), [
    {key: "letter-b", pressed: false},
    {key: "letter-c", pressed: true},
]);
assert.deepEqual(rollover.transition({key: "shift", pressed: false}), [
    {key: "shift", pressed: false},
]);
assert.deepEqual(rollover.transition({key: "letter-c", pressed: false}), [
    {key: "letter-c", pressed: false},
]);
rollover.transition({key: "letter-a", pressed: true});
rollover.reset();
assert.deepEqual(rollover.transition({key: "letter-b", pressed: true}), [
    {key: "letter-b", pressed: true},
]);
assert.throws(() => rollover.transition({key: "letter-a", pressed: 1}), /boolean/);
