// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const [modulePath] = process.argv.slice(2);
if (!modulePath) {
    throw new Error(
        "Usage: node lcd_indicator_view_test.mjs <lcd-indicator-view.mjs>",
    );
}

const {Jr800VisibleLcdIndicators, lcdIndicatorView} = await import(
    pathToFileURL(modulePath)
);

assert.equal(Jr800VisibleLcdIndicators.length, 15);
assert.deepEqual(
    Jr800VisibleLcdIndicators.filter(({group}) => group === "page")
        .map(({label}) => label),
    ["1", "2", "3", "4", "5", "6", "7", "8"],
);
assert.deepEqual(
    Jr800VisibleLcdIndicators.filter(({group}) => group === "mode")
        .map(({label}) => label),
    ["CAP.L", "GRAPH", "KANA", "INS", "CTRL", "RAD", "DEG"],
);
assert.ok(
    Jr800VisibleLcdIndicators.every(({description}) => (
        typeof description === "string" && description.length > 0
    )),
);
assert.match(
    Jr800VisibleLcdIndicators.find(({name}) => name === "capital-lock")
        .description,
    /CTRL\+9.*initial state off/,
);
assert.match(
    Jr800VisibleLcdIndicators.find(({name}) => name === "graphics-input")
        .description,
    /CTRL\+0.*initial state off/,
);
assert.match(
    Jr800VisibleLcdIndicators.find(({name}) => name === "kana-input")
        .description,
    /CTRL\+\^.*initial state off/,
);
assert.match(
    Jr800VisibleLcdIndicators.find(({name}) => name === "insert-mode")
        .description,
    /CTRL\+A.*initial state off/,
);
assert.match(
    Jr800VisibleLcdIndicators.find(({name}) => name === "control-mode")
        .description,
    /CTRL\+8.*initial state unresolved/,
);
assert.ok(
    Jr800VisibleLcdIndicators.every(
        ({name}) => name !== "battery-warning",
    ),
);

const unavailable = lcdIndicatorView(null);
assert.ok(unavailable.entries.every(({state}) => state === "unavailable"));
assert.match(unavailable.summary, /battery telemetry omitted/);

const names = [
    "page-1",
    "page-2",
    "page-3",
    "page-4",
    "page-5",
    "page-6",
    "page-7",
    "page-8",
    "capital-lock",
    "graphics-input",
    "kana-input",
    "insert-mode",
    "control-mode",
    "radian-mode",
    "degree-mode",
    "battery-warning",
];
const unknownSnapshot = Object.fromEntries(names.map((name) => [name, null]));
const unknown = lcdIndicatorView(unknownSnapshot);
assert.ok(unknown.entries.every(({state, valueText}) => (
    state === "unknown" && valueText === "?"
)));

const rawSnapshot = {...unknownSnapshot, "page-1": 0, "capital-lock": 0xff};
const raw = lcdIndicatorView(rawSnapshot);
assert.deepEqual(
    raw.entries.filter(({state}) => state === "raw")
        .map(({name, valueText}) => [name, valueText]),
    [["page-1", "$00"], ["capital-lock", "$FF"]],
);
assert.ok(
    raw.entries.filter(({state}) => state === "raw")
        .every(({detail}) => detail.includes("drive state unresolved")),
    "Raw zero and nonzero values must not become off/on states",
);

assert.throws(
    () => lcdIndicatorView([]),
    /must be an object or null/,
);
const incomplete = {...unknownSnapshot};
delete incomplete["page-8"];
assert.throws(() => lcdIndicatorView(incomplete), /incomplete/);
assert.throws(
    () => lcdIndicatorView({...unknownSnapshot, extra: null}),
    /unknown fields/,
);
assert.throws(
    () => lcdIndicatorView({...unknownSnapshot, "page-1": 256}),
    /raw value is invalid/,
);
