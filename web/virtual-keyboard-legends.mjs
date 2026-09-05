// SPDX-License-Identifier: MIT

import {lcdIndicatorView} from "./lcd-indicator-view.mjs";

const pfLegends = Object.fromEntries(
    Array.from({length: 10}, (_, index) => {
        const number = index + 1;
        return [`pf-${number}`, {normal: `PF${number}`, shift: `PF${number + 10}`}];
    }),
);

const legends = Object.freeze({
    ...pfLegends,
    ...Object.fromEntries(Array.from({length: 10}, (_, digit) => (
        [`keypad-${digit}`, {normal: String(digit)}]
    ))),
    "keypad-divide": {normal: "/"},
    "keypad-multiply": {normal: "*"},
    "keypad-subtract": {normal: "-"},
    "keypad-add": {normal: "+"},
    "keypad-decimal": {normal: "."},
    "keypad-equal": {normal: "="},
    "main-1": {normal: "1", shift: "!", control: "ERASE"},
    "main-2": {normal: "2", shift: "\"", control: "HCOPY"},
    "main-3": {normal: "3", shift: "#", control: "SAVE"},
    "main-4": {normal: "4", shift: "$", control: "LOAD"},
    "main-5": {normal: "5", shift: "%", control: "VERIFY"},
    "main-6": {normal: "6", shift: "&", control: "OPEN"},
    "main-7": {normal: "7", shift: "'", control: "CLOSE"},
    "main-8": {normal: "8", shift: "(", control: "CTRL"},
    "main-9": {normal: "9", shift: ")", control: "CAP.L"},
    "main-0": {normal: "0", shift: "@", control: "GRAPH"},
    "main-caret": {normal: "^", shift: "¥", control: "KANA"},
    break: {normal: "BREAK", control: "CLEAR"},
    "letter-q": {normal: "Q", function: "GOSUB"},
    "letter-w": {normal: "W", function: "RETURN"},
    "letter-e": {normal: "E", function: "END", controlMode: "ERASE"},
    "letter-r": {normal: "R", function: "RUN"},
    "letter-t": {normal: "T", function: "THEN"},
    "letter-y": {normal: "Y", function: "LOCATE"},
    "letter-u": {normal: "U", function: "TITLE"},
    "letter-i": {normal: "I", function: "INPUT"},
    "letter-o": {normal: "O", function: "PAGE"},
    "letter-p": {normal: "P", function: "PRINT", controlMode: "HCOPY"},
    return: {normal: "RETURN", control: "DATA"},
    "letter-a": {normal: "A", function: "AUTO", controlMode: "INS"},
    "letter-s": {normal: "S", function: "STOP", controlMode: "INS"},
    "letter-d": {normal: "D", function: "DIM", controlMode: "MENU"},
    "letter-f": {normal: "F", function: "FOR"},
    "letter-g": {normal: "G", function: "GOTO"},
    "letter-h": {normal: "H", function: "POKE", controlMode: "RUB"},
    "letter-j": {normal: "J", function: "READ"},
    "letter-k": {normal: "K", function: "KEY", controlMode: "HOME"},
    "letter-l": {normal: "L", function: "LIST", controlMode: "CLS"},
    semicolon: {normal: ";", shift: "?", control: "LEFT"},
    colon: {normal: ":", shift: "_", control: "RIGHT"},
    "letter-z": {normal: "Z", function: "SOUND"},
    "letter-x": {normal: "X", function: "CONT"},
    "letter-c": {normal: "C", function: "CLEAR", controlMode: "CANCEL"},
    "letter-v": {normal: "V", function: "REM"},
    "letter-b": {normal: "B", function: "RESTORE"},
    "letter-n": {normal: "N", function: "NEXT"},
    "letter-m": {normal: "M", function: "DATA", controlMode: "RETURN"},
    comma: {normal: ",", shift: "<", control: "UP"},
    period: {normal: ".", shift: ">", control: "DOWN"},
    "insert-rub": {normal: "RUB", shift: "INS", control: "POKE"},
    "vertical-arrows": {normal: "↑", shift: "↓"},
    "horizontal-arrows": {normal: "→", shift: "←"},
    "home-cls": {normal: "CLS", shift: "HOME", control: "LIST"},
});

// Key roles come from the owner's manual, pp. 6-13 and code table A.3.
// Values are character identities, never embedded glyphs or ROM key tables.
const graphicsCodes = Object.freeze({
    "letter-q": 0x80, "letter-w": 0x81, "letter-e": 0x82,
    "letter-r": 0x83, "letter-t": 0x84, "letter-y": 0x85,
    "letter-u": 0x86, "letter-i": 0x98, "letter-o": 0x91,
    "letter-p": 0x99, "letter-a": 0x88, "letter-s": 0x89,
    "letter-d": 0x8a, "letter-f": 0x8b, "letter-g": 0x8c,
    "letter-h": 0x87, "letter-j": 0x96, "letter-k": 0x93,
    "letter-l": 0x8f, semicolon: 0x92, colon: 0x95,
    "letter-z": 0x8e, "letter-x": 0x8d, "letter-c": 0x94,
    "letter-v": 0x97, "letter-b": 0x9c, "letter-n": 0x9d,
    "letter-m": 0x9a, comma: 0x90, period: 0x9b, "main-caret": 0x9e,
});

const kanaCodes = Object.freeze({
    "main-1": [0xc7], "main-2": [0xcc], "main-3": [0xb1, 0xa7],
    "main-4": [0xb3, 0xa9], "main-5": [0xb4, 0xaa],
    "main-6": [0xb5, 0xab], "main-7": [0xd4, 0xac],
    "main-8": [0xd5, 0xad], "main-9": [0xd6, 0xae],
    "main-0": [0xdc, 0xa6], "main-caret": [0xce],
    "letter-q": [0xc0], "letter-w": [0xc3], "letter-e": [0xb2, 0xa8],
    "letter-r": [0xbd], "letter-t": [0xb6], "letter-y": [0xdd],
    "letter-u": [0xc5], "letter-i": [0xc6], "letter-o": [0xd7],
    "letter-p": [0xbe], "letter-a": [0xc1], "letter-s": [0xc4],
    "letter-d": [0xbc], "letter-f": [0xca], "letter-g": [0xb7],
    "letter-h": [0xb8], "letter-j": [0xcf], "letter-k": [0xc9],
    "letter-l": [0xd8], semicolon: [0xda], colon: [0xb9],
    "letter-z": [0xc2, 0xaf], "letter-x": [0xbb], "letter-c": [0xbf],
    "letter-v": [0xcb], "letter-b": [0xba], "letter-n": [0xd0],
    "letter-m": [0xd3], comma: [0xc8, 0xa4], period: [0xd9, 0xa1],
    "keypad-7": [0xcd], "keypad-8": [0xb0, 0xa0], "keypad-9": [],
    "keypad-4": [0xde], "keypad-5": [0xdf, 0xa2], "keypad-6": [],
    "keypad-1": [0xd1, 0xa3], "keypad-2": [], "keypad-3": [],
    "keypad-0": [0xd2, 0xa5], "keypad-decimal": [0xdb],
    "keypad-equal": [], "keypad-divide": [0x7b],
    "keypad-multiply": [0x7d], "keypad-subtract": [0x5b],
    "keypad-add": [0x5d],
});

export const Jr800VirtualLegendKeys = Object.freeze(Object.keys(legends));
export const Jr800VirtualControlModes = Object.freeze([
    "unknown",
    "function",
    "control",
]);

function requireBoolean(value, name) {
    if (typeof value !== "boolean") {
        throw new TypeError(`${name} state must be boolean`);
    }
    return value;
}

export function virtualKeyboardModes(indicators) {
    const requests = Object.fromEntries(lcdIndicatorView(indicators).entries
        .map(({name, request}) => [name, request]));
    const graph = requests["graphics-input"];
    const kana = requests["kana-input"];
    return {
        capitalLock: requests["capital-lock"],
        inputMode: graph === null || kana === null || (graph && kana)
            ? "unknown" : graph ? "graphics" : kana ? "kana" : "normal",
        controlMode: requests["control-mode"] === null
            ? "unknown" : requests["control-mode"] ? "control" : "function",
    };
}

function textFace(label) {
    return {label, characterCode: null, blank: false};
}

function characterFace(label, characterCode) {
    return {label, characterCode, blank: false};
}

export function virtualKeyboardLegend(
    key,
    {shift = false, control = false, controlMode = "unknown",
        capitalLock = null, inputMode = "unknown"} = {},
) {
    if (typeof key !== "string" || !Object.hasOwn(legends, key)) {
        throw new TypeError("Virtual keyboard legend key is unsupported");
    }
    const shifted = requireBoolean(shift, "Shift");
    const controlled = requireBoolean(control, "Control");
    if (capitalLock !== null) {
        requireBoolean(capitalLock, "Capital lock");
    }
    if (!["unknown", "normal", "graphics", "kana"].includes(inputMode)) {
        throw new TypeError("Virtual keyboard input mode is unsupported");
    }
    if (!Jr800VirtualControlModes.includes(controlMode)) {
        throw new TypeError("Virtual keyboard control mode is unsupported");
    }
    const legend = legends[key];
    const hasControlMeaning = legend.control !== undefined
        || legend.function !== undefined
        || legend.controlMode !== undefined;
    if (controlled && hasControlMeaning) {
        if (legend.control !== undefined) {
            return textFace(legend.control);
        }
        if (controlMode === "function" && legend.function !== undefined) {
            return textFace(legend.function);
        }
        if (controlMode === "control" && legend.controlMode !== undefined) {
            return textFace(legend.controlMode);
        }
        return textFace(`CTRL ${legend.normal}`);
    }
    if (inputMode === "graphics" && Object.hasOwn(graphicsCodes, key)) {
        return characterFace(`GRAPH ${legend.normal}`, graphicsCodes[key]);
    }
    if (inputMode === "kana" && Object.hasOwn(kanaCodes, key)) {
        const codes = kanaCodes[key];
        if (codes.length === 0) {
            return {...textFace(`${legend.normal}: no character`), blank: true};
        }
        const code = shifted && codes.length === 2 ? codes[1] : codes[0];
        return characterFace(`KANA ${legend.normal}`, code);
    }
    if (key.startsWith("letter-")) {
        if (inputMode === "unknown" || capitalLock === null) {
            return textFace(legend.normal);
        }
        const label = capitalLock !== shifted
            ? legend.normal : legend.normal.toLowerCase();
        return characterFace(label, label.charCodeAt(0));
    }
    const label = shifted && legend.shift !== undefined ? legend.shift : legend.normal;
    return label.length === 1 && !["vertical-arrows", "horizontal-arrows"].includes(key)
        ? characterFace(label, label === "¥" ? 0x5c : label.charCodeAt(0))
        : textFace(label);
}
