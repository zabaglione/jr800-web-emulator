// SPDX-License-Identifier: MIT

import {Jr800LcdIndicatorNames} from "./wasm-machine.mjs";

export const Jr800VisibleLcdIndicators = Object.freeze([
    Object.freeze({
        name: "page-1", label: "1", group: "page",
        description: "BASIC page selection and page-list position",
    }),
    Object.freeze({
        name: "page-2", label: "2", group: "page",
        description: "BASIC page selection and page-list position",
    }),
    Object.freeze({
        name: "page-3", label: "3", group: "page",
        description: "BASIC page selection and page-list position",
    }),
    Object.freeze({
        name: "page-4", label: "4", group: "page",
        description: "BASIC page selection and page-list position",
    }),
    Object.freeze({
        name: "page-5", label: "5", group: "page",
        description: "BASIC page selection and page-list position",
    }),
    Object.freeze({
        name: "page-6", label: "6", group: "page",
        description: "BASIC page selection and page-list position",
    }),
    Object.freeze({
        name: "page-7", label: "7", group: "page",
        description: "BASIC page selection and page-list position",
    }),
    Object.freeze({
        name: "page-8", label: "8", group: "page",
        description: "BASIC page selection and page-list position",
    }),
    Object.freeze({
        name: "capital-lock",
        label: "CAP.L",
        group: "mode",
        description: "BASIC CTRL+9 toggle; reported initial state off",
    }),
    Object.freeze({
        name: "graphics-input",
        label: "GRAPH",
        group: "mode",
        description: "BASIC CTRL+0 toggle; reported initial state off",
    }),
    Object.freeze({
        name: "kana-input", label: "KANA", group: "mode",
        description: "BASIC CTRL+^ toggle; reported initial state off",
    }),
    Object.freeze({
        name: "insert-mode", label: "INS", group: "mode",
        description: "BASIC CTRL+A in control mode; reported initial state off",
    }),
    Object.freeze({
        name: "control-mode", label: "CTRL", group: "mode",
        description: "BASIC CTRL+8 mode toggle; reported initial state unresolved",
    }),
    Object.freeze({
        name: "radian-mode", label: "RAD", group: "mode",
        description: "Radian angle mode",
    }),
    Object.freeze({
        name: "degree-mode", label: "DEG", group: "mode",
        description: "Degree angle mode",
    }),
]);

function validateSnapshot(indicators) {
    if (indicators === null) {
        return;
    }
    if (typeof indicators !== "object" || Array.isArray(indicators)) {
        throw new TypeError("LCD indicator snapshot must be an object or null");
    }
    for (const name of Jr800LcdIndicatorNames) {
        if (!Object.hasOwn(indicators, name)) {
            throw new TypeError("LCD indicator snapshot is incomplete");
        }
        const value = indicators[name];
        if (value !== null && (
            !Number.isInteger(value) || value < 0 || value > 0xff
        )) {
            throw new RangeError("LCD indicator raw value is invalid");
        }
    }
    if (Object.keys(indicators).length !== Jr800LcdIndicatorNames.length) {
        throw new TypeError("LCD indicator snapshot has unknown fields");
    }
}

export function lcdIndicatorView(indicators) {
    validateSnapshot(indicators);
    let knownCount = 0;
    const entries = Jr800VisibleLcdIndicators.map((identity) => {
        const rawValue = indicators?.[identity.name] ?? null;
        const state = indicators === null
            ? "unavailable"
            : rawValue === null ? "unknown" : "raw";
        if (state === "raw") {
            ++knownCount;
        }
        const valueText = state === "unavailable"
            ? "n/a"
            : state === "unknown"
                ? "?"
                : `$${rawValue.toString(16).toUpperCase().padStart(2, "0")}`;
        const detail = state === "raw"
            ? `raw RAM ${valueText}; drive state unresolved`
            : state === "unknown"
                ? "raw RAM unknown; drive state unresolved"
                : "indicator data unavailable";
        return Object.freeze({...identity, state, rawValue, valueText, detail});
    });
    const summary = indicators === null
        ? "Indicator RAM unavailable; battery telemetry omitted"
        : `${knownCount} of ${entries.length} raw values known; `
            + "drive states unresolved; battery telemetry omitted";
    return Object.freeze({entries: Object.freeze(entries), summary});
}
