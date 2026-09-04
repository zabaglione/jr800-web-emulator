// SPDX-License-Identifier: MIT

import {Jr800KeyboardKey} from "./wasm-machine.mjs";

export const Jr800HostKeyBinding = Object.freeze({
    ShiftLeft: "shift",
    ShiftRight: "shift",
    ControlLeft: "control",
    ControlRight: "control",
    ContextMenu: "menu",
    Enter: "return",
    NumpadEnter: "return",
    Space: "space",
    Pause: "break",
    Home: "home-cls",
    Digit0: "main-0",
    Digit1: "main-1",
    Digit2: "main-2",
    Digit3: "main-3",
    Digit4: "main-4",
    Digit5: "main-5",
    Digit6: "main-6",
    Digit7: "main-7",
    Digit8: "main-8",
    Digit9: "main-9",
    Equal: "main-caret",
    KeyA: "letter-a",
    KeyB: "letter-b",
    KeyC: "letter-c",
    KeyD: "letter-d",
    KeyE: "letter-e",
    KeyF: "letter-f",
    KeyG: "letter-g",
    KeyH: "letter-h",
    KeyI: "letter-i",
    KeyJ: "letter-j",
    KeyK: "letter-k",
    KeyL: "letter-l",
    KeyM: "letter-m",
    KeyN: "letter-n",
    KeyO: "letter-o",
    KeyP: "letter-p",
    KeyQ: "letter-q",
    KeyR: "letter-r",
    KeyS: "letter-s",
    KeyT: "letter-t",
    KeyU: "letter-u",
    KeyV: "letter-v",
    KeyW: "letter-w",
    KeyX: "letter-x",
    KeyY: "letter-y",
    KeyZ: "letter-z",
    Semicolon: "semicolon",
    Quote: "colon",
    Comma: "comma",
    Period: "period",
    F1: "pf-1",
    F2: "pf-2",
    F3: "pf-3",
    F4: "pf-4",
    F5: "pf-5",
    F6: "pf-6",
    F7: "pf-7",
    F8: "pf-8",
    F9: "pf-9",
    F10: "pf-10",
    Backspace: "keypad-insert-rub",
    ArrowUp: "keypad-vertical-arrows",
    ArrowRight: "keypad-horizontal-arrows",
    Numpad0: "keypad-0",
    Numpad1: "keypad-1",
    Numpad2: "keypad-2",
    Numpad3: "keypad-3",
    Numpad4: "keypad-4",
    Numpad5: "keypad-5",
    Numpad6: "keypad-6",
    Numpad7: "keypad-7",
    Numpad8: "keypad-8",
    Numpad9: "keypad-9",
    NumpadMultiply: "keypad-multiply",
    NumpadAdd: "keypad-add",
    NumpadEqual: "keypad-equal",
    NumpadSubtract: "keypad-subtract",
    NumpadDecimal: "keypad-decimal",
    NumpadDivide: "keypad-divide",
});

for (const key of Object.values(Jr800HostKeyBinding)) {
    if (!Object.hasOwn(Jr800KeyboardKey, key)) {
        throw new Error("Host keyboard binding uses an unsupported JR-800 key");
    }
}
const boundKeyboardKeys = new Set(Object.values(Jr800HostKeyBinding));
const modeledKeyboardKeys = Object.keys(Jr800KeyboardKey);
if (boundKeyboardKeys.size !== modeledKeyboardKeys.length
    || modeledKeyboardKeys.some((key) => !boundKeyboardKeys.has(key))) {
    throw new Error("Host keyboard binding must cover every JR-800 key");
}

export function jr800KeyForHostCode(code) {
    if (typeof code !== "string") {
        throw new TypeError("Host keyboard code must be a string");
    }
    return Jr800HostKeyBinding[code] ?? null;
}
