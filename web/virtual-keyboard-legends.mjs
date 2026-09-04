// SPDX-License-Identifier: MIT

const pfLegends = Object.fromEntries(
    Array.from({length: 10}, (_, index) => {
        const number = index + 1;
        return [`pf-${number}`, {normal: `PF${number}`, shift: `PF${number + 10}`}];
    }),
);

const legends = Object.freeze({
    ...pfLegends,
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
    colon: {normal: ":", shift: "-", control: "RIGHT"},
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

export function virtualKeyboardLegend(
    key,
    {shift = false, control = false, controlMode = "unknown"} = {},
) {
    if (typeof key !== "string" || !Object.hasOwn(legends, key)) {
        throw new TypeError("Virtual keyboard legend key is unsupported");
    }
    const shifted = requireBoolean(shift, "Shift");
    const controlled = requireBoolean(control, "Control");
    if (!Jr800VirtualControlModes.includes(controlMode)) {
        throw new TypeError("Virtual keyboard control mode is unsupported");
    }
    const legend = legends[key];
    const hasControlMeaning = legend.control !== undefined
        || legend.function !== undefined
        || legend.controlMode !== undefined;
    if (controlled && hasControlMeaning) {
        if (legend.control !== undefined) {
            return legend.control;
        }
        if (controlMode === "function" && legend.function !== undefined) {
            return legend.function;
        }
        if (controlMode === "control" && legend.controlMode !== undefined) {
            return legend.controlMode;
        }
        return `CTRL ${legend.normal}`;
    }
    if (shifted && legend.shift !== undefined) {
        return legend.shift;
    }
    return legend.normal;
}
