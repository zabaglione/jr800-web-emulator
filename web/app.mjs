// SPDX-License-Identifier: MIT

import {
    AccessTraceMask,
    JR800_LOGICAL_ROM_BYTES,
    Jr800KeyboardKey,
    MAX_JR8ROM_BYTES,
    MAX_LINKED_BINARY_BYTES,
} from "./wasm-machine.mjs";
import {
    Jr800VisibleLcdIndicators,
    lcdIndicatorView,
} from "./lcd-indicator-view.mjs";
import {lcdPanelImage} from "./lcd-panel-view.mjs";
import {jr800KeyForHostCode} from "./keyboard-input.mjs";
import {
    Jr800VirtualKeyboardState,
    isJr800VirtualLatchKey,
} from "./virtual-keyboard-input.mjs";
import {
    Jr800VirtualLegendKeys,
    virtualKeyboardLegend,
} from "./virtual-keyboard-legends.mjs";
import {
    Jr800BasicRunSlice,
    basicRunCanContinue,
    jr800BasicBootExperimentConfiguration,
} from "./basic-boot-profile.mjs";

const worker = new Worker(new URL("./jr800-worker.mjs", import.meta.url), {type: "module"});

class WorkerClient {
    constructor(target) {
        this.target = target;
        this.nextId = 1;
        this.pending = new Map();
        this.listeners = new Map();
        target.addEventListener("message", (event) => this.#receive(event.data));
        target.addEventListener("error", (event) => this.#emit("error", event.error ?? event.message));
    }

    #receive(message) {
        if (message?.type === "response") {
            const pending = this.pending.get(message.id);
            if (!pending) {
                return;
            }
            this.pending.delete(message.id);
            clearTimeout(pending.timeout);
            if (message.ok) {
                pending.resolve(message.result);
            } else {
                pending.reject(new Error(message.error));
            }
            return;
        }
        if (message?.type === "event") {
            this.#emit(message.event, message);
        }
    }

    #emit(name, value) {
        for (const listener of this.listeners.get(name) ?? []) {
            listener(value);
        }
    }

    on(name, listener) {
        const listeners = this.listeners.get(name) ?? [];
        listeners.push(listener);
        this.listeners.set(name, listeners);
    }

    request(command, fields = {}, transfer = []) {
        const id = this.nextId++;
        return new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                this.pending.delete(id);
                reject(new Error(`Worker command timed out: ${command}`));
            }, 20_000);
            this.pending.set(id, {resolve, reject, timeout});
            try {
                this.target.postMessage({id, command, ...fields}, transfer);
            } catch (error) {
                clearTimeout(timeout);
                this.pending.delete(id);
                reject(error);
            }
        });
    }
}

const client = new WorkerClient(worker);
const elements = Object.fromEntries(
    [
        "status", "application-file", "debug-file", "stack-pointer", "load",
        "jr8rom-file", "raw-rom-warning", "boot-basic", "load-rom",
        "resume-machine", "pause-basic",
        "hardware-program-file", "load-program",
        "reset-sp-enabled", "reset-sp-value",
        "reset-x-enabled", "reset-x-value",
        "reset-a-enabled", "reset-a-value",
        "reset-b-enabled", "reset-b-value",
        "reset-cc-enabled", "reset-cc-value", "reset-cc-known-mask",
        "internal-ram-enabled", "internal-ram-value",
        "standard-ram-enabled", "standard-ram-value",
        "expansion-ram-enabled", "expansion-ram-value",
        "lcd-enabled", "lcd-value", "calendar-enabled",
        "calendar-address-source", "calendar-upper-read",
        "calendar-cpu-cycle-ratio-enabled",
        "port1-enabled", "port1-value", "port1-known-mask",
        "port2-enabled", "port2-value", "port2-known-mask", "ram-standby",
        "keyboard-window-enabled", "keyboard-window-value",
        "reset", "run", "run-to", "run-to-address", "run-to-source",
        "run-to-source-location", "run-to-symbol", "run-to-symbol-name",
        "pause", "step",
        "step-over", "step-out", "instruction-limit", "profile",
        "suspended-cycle-limit", "calendar-oscillator-ticks",
        "advance-calendar", "adjust-calendar-seconds",
        "register-pc", "register-sp", "register-x", "register-a", "register-b",
        "register-cc", "execution-state", "calendar-alarm-terminal",
        "port2-timer-output", "lcd-substituted-read-count",
        "cycles", "current-address",
        "disassembly", "source",
        "breakpoint-address", "breakpoint-condition",
        "enable-breakpoint", "disable-breakpoint",
        "watchpoint-address", "watchpoint-mode", "enable-watchpoint",
        "disable-watchpoint",
        "watch-expression", "add-expression-watch",
        "expression-watch-body",
        "watch-symbol", "add-symbol-watch", "symbol-watch-body",
        "memory-address", "memory-length", "refresh-memory", "memory",
        "clear-history", "history", "trace", "access-count",
        "trace-first-address", "trace-last-address", "trace-kind",
        "apply-trace-filter",
        "machine-console", "layout-workbench", "layout-device",
        "lcd-panel-card", "lcd-panel", "lcd-summary", "lcd-indicator-summary",
        "virtual-keyboard-card", "virtual-keyboard-summary", "force-power-off",
        "keyboard-address", "keyboard-value", "keyboard-known",
        "set-keyboard-response", "keyboard-summary",
    ].map((id) => [id, document.getElementById(id)]),
);
const lcdIndicatorElements = new Map(
    [...document.querySelectorAll("[data-lcd-indicator]")]
        .map((element) => [element.dataset.lcdIndicator, element]),
);
for (const {name} of Jr800VisibleLcdIndicators) {
    if (!lcdIndicatorElements.has(name)) {
        throw new Error("LCD indicator presentation is incomplete");
    }
}
const virtualKeyboardButtonElements = [
    ...document.querySelectorAll("button[data-jr800-key]"),
];
const modeledKeyboardKeys = Object.keys(Jr800KeyboardKey);
const virtualKeyboardButtons = new Map(
    virtualKeyboardButtonElements
        .map((button) => [button.dataset.jr800Key, button]),
);
if (virtualKeyboardButtonElements.length !== modeledKeyboardKeys.length
    || virtualKeyboardButtons.size !== modeledKeyboardKeys.length
    || modeledKeyboardKeys.some((key) => !virtualKeyboardButtons.has(key))) {
    throw new Error("Virtual keyboard must expose all modeled key positions");
}
const virtualLegendButtons = new Map(
    [...document.querySelectorAll("button[data-legend-key]")]
        .map((button) => [button.dataset.legendKey, button]),
);
if (virtualLegendButtons.size !== Jr800VirtualLegendKeys.length
    || Jr800VirtualLegendKeys.some((key) => !virtualLegendButtons.has(key))) {
    throw new Error("Virtual keyboard legend positions are incomplete");
}

let initialized = false;
let loaded = false;
let running = false;
let machineKind = "synthetic";
let calendarAttached = false;
let basicRunContinuous = false;
let nextExpressionWatchId = 1;
let nextSymbolWatchId = 1;
const virtualKeyboardState = new Jr800VirtualKeyboardState();

function setMachineLayout(layout) {
    if (layout !== "workbench" && layout !== "device") {
        throw new TypeError(`Unsupported machine layout: ${layout}`);
    }
    elements["machine-console"].dataset.layout = layout;
    elements["layout-workbench"].setAttribute(
        "aria-pressed",
        layout === "workbench" ? "true" : "false",
    );
    elements["layout-device"].setAttribute(
        "aria-pressed",
        layout === "device" ? "true" : "false",
    );
}

function setStatus(text, tone = "idle") {
    elements.status.textContent = text;
    elements.status.dataset.tone = tone;
}

function virtualKeyboardAvailable() {
    return loaded && machineKind === "jr800";
}

function hostKeyboardTargetIsInteractive(target) {
    return target instanceof Element
        && target.closest(
            "input, textarea, select, button, a, summary, [contenteditable]",
        ) !== null;
}

function virtualKeyLabel(button) {
    return button.querySelector("span")?.textContent?.trim()
        ?? button.dataset.jr800Key;
}

function renderVirtualKeyboardLegends() {
    const modifiers = {
        shift: virtualKeyboardState.isPressed("shift"),
        control: virtualKeyboardState.isPressed("control"),
    };
    for (const [key, button] of virtualLegendButtons) {
        const label = virtualKeyboardLegend(key, modifiers);
        const target = button.querySelector("[data-key-legend]");
        if (target === null) {
            throw new Error("Virtual keyboard legend target is missing");
        }
        target.textContent = label;
        button.dataset.legendSize = label.length > 5 ? "compact" : "normal";
    }
}

function renderVirtualKeyboardState() {
    renderVirtualKeyboardLegends();
    const held = [];
    for (const [key, button] of virtualKeyboardButtons) {
        const pressed = virtualKeyboardState.isPressed(key);
        button.dataset.pressed = pressed ? "true" : "false";
        if (isJr800VirtualLatchKey(key)) {
            button.setAttribute(
                "aria-pressed",
                virtualKeyboardState.isLatched(key) ? "true" : "false",
            );
        }
        if (pressed) {
            held.push(virtualKeyLabel(button));
        }
    }

    if (!virtualKeyboardAvailable()) {
        elements["virtual-keyboard-summary"].textContent =
            "Keyboard unavailable";
    } else if (held.length === 0) {
        elements["virtual-keyboard-summary"].textContent =
            "Keyboard ready";
    } else {
        elements["virtual-keyboard-summary"].textContent =
            `Held: ${held.join(" + ")}`;
    }
}

function sendVirtualKeyboardTransition(transition) {
    if (transition === null) {
        return;
    }
    renderVirtualKeyboardState();
    if (!virtualKeyboardAvailable()) {
        return;
    }
    void client.request("set-keyboard-key-state", transition).catch((error) => {
        virtualKeyboardState.releaseAll();
        renderVirtualKeyboardState();
        setStatus(error instanceof Error ? error.message : String(error), "error");
    });
}

function releaseAllVirtualKeys(sendToMachine = true) {
    const releases = virtualKeyboardState.releaseAll();
    renderVirtualKeyboardState();
    if (!sendToMachine || !virtualKeyboardAvailable()) {
        return;
    }
    for (const release of releases) {
        void client.request("set-keyboard-key-state", release).catch((error) => {
            setStatus(
                error instanceof Error ? error.message : String(error),
                "error",
            );
        });
    }
}

function setControls() {
    elements.load.disabled = !initialized || running;
    elements["load-rom"].disabled = !initialized || running;
    elements["boot-basic"].disabled = !initialized || running;
    elements["load-program"].disabled = !loaded || running
        || machineKind !== "jr800";
    for (const id of [
        "application-file", "debug-file", "stack-pointer", "jr8rom-file",
    ]) {
        elements[id].disabled = !initialized || running;
    }
    elements["hardware-program-file"].disabled = !loaded || running
        || machineKind !== "jr800";
    for (const id of [
        "reset", "run", "run-to", "run-to-source", "run-to-symbol",
        "step", "step-over", "step-out",
        "enable-breakpoint",
        "disable-breakpoint", "enable-watchpoint", "disable-watchpoint",
        "add-expression-watch", "add-symbol-watch",
        "refresh-memory", "clear-history", "apply-trace-filter",
    ]) {
        elements[id].disabled = !loaded || running;
    }
    for (const id of [
        "trace-first-address", "trace-last-address", "trace-kind",
        "watch-expression", "watch-symbol",
    ]) {
        elements[id].disabled = !loaded || running;
    }
    for (const button of elements["expression-watch-body"].querySelectorAll(
        "button[data-watch-id]",
    )) {
        button.disabled = !loaded || running;
    }
    for (const button of elements["symbol-watch-body"].querySelectorAll(
        "button[data-watch-id]",
    )) {
        button.disabled = !loaded || running;
    }
    elements["set-keyboard-response"].disabled = !loaded
        || machineKind !== "jr800";
    const keyboardDisabled = !virtualKeyboardAvailable();
    for (const button of virtualKeyboardButtons.values()) {
        button.disabled = keyboardDisabled;
    }
    elements["force-power-off"].disabled = keyboardDisabled;
    elements["virtual-keyboard-card"].dataset.available = keyboardDisabled
        ? "false"
        : "true";
    renderVirtualKeyboardState();
    const calendarOperationDisabled = !loaded || running
        || machineKind !== "jr800" || !calendarAttached;
    elements["calendar-oscillator-ticks"].disabled = calendarOperationDisabled;
    elements["advance-calendar"].disabled = calendarOperationDisabled;
    elements["adjust-calendar-seconds"].disabled = calendarOperationDisabled;
    elements["run-to-source"].disabled = !loaded || running
        || machineKind !== "synthetic";
    elements["run-to-symbol"].disabled = !loaded || running
        || machineKind !== "synthetic";
    elements["add-symbol-watch"].disabled = !loaded || running
        || machineKind !== "synthetic";
    elements["watch-symbol"].disabled = !loaded || running
        || machineKind !== "synthetic";
    elements.pause.disabled = !running;
    elements["resume-machine"].disabled = !loaded || running
        || machineKind !== "jr800";
    elements["pause-basic"].disabled = !running
        || machineKind !== "jr800";

    const editable = initialized && !running;
    const standardRamEnabled = elements["standard-ram-enabled"].checked;
    if (!standardRamEnabled) {
        elements["expansion-ram-enabled"].checked = false;
    }
    const calendarEnabled = elements["calendar-enabled"].checked;
    if (!calendarEnabled) {
        elements["calendar-cpu-cycle-ratio-enabled"].checked = false;
    }
    const dependentControls = [
        ["reset-sp-enabled", true],
        ["reset-sp-value", elements["reset-sp-enabled"].checked],
        ["reset-x-enabled", true],
        ["reset-x-value", elements["reset-x-enabled"].checked],
        ["reset-a-enabled", true],
        ["reset-a-value", elements["reset-a-enabled"].checked],
        ["reset-b-enabled", true],
        ["reset-b-value", elements["reset-b-enabled"].checked],
        ["reset-cc-enabled", true],
        ["reset-cc-value", elements["reset-cc-enabled"].checked],
        ["reset-cc-known-mask", elements["reset-cc-enabled"].checked],
        ["internal-ram-enabled", true],
        ["internal-ram-value", elements["internal-ram-enabled"].checked],
        ["standard-ram-enabled", true],
        ["standard-ram-value", standardRamEnabled],
        ["expansion-ram-enabled", standardRamEnabled],
        [
            "expansion-ram-value",
            standardRamEnabled && elements["expansion-ram-enabled"].checked,
        ],
        ["lcd-enabled", true],
        ["lcd-value", elements["lcd-enabled"].checked],
        ["calendar-enabled", true],
        ["calendar-address-source", calendarEnabled],
        ["calendar-upper-read", calendarEnabled],
        ["calendar-cpu-cycle-ratio-enabled", calendarEnabled],
        ["port1-enabled", true],
        ["port1-value", elements["port1-enabled"].checked],
        ["port1-known-mask", elements["port1-enabled"].checked],
        ["port2-enabled", true],
        ["port2-value", elements["port2-enabled"].checked],
        ["port2-known-mask", elements["port2-enabled"].checked],
        ["ram-standby", true],
        ["keyboard-window-enabled", true],
        [
            "keyboard-window-value",
            elements["keyboard-window-enabled"].checked,
        ],
    ];
    for (const [id, dependencyMet] of dependentControls) {
        elements[id].disabled = !editable || !dependencyMet;
    }
}

function parseNumber(text, maximum, label) {
    const source = text.trim();
    let value;
    if (/^\$[0-9a-f]+$/i.test(source)) {
        value = Number.parseInt(source.slice(1), 16);
    } else if (/^0x[0-9a-f]+$/i.test(source)) {
        value = Number.parseInt(source.slice(2), 16);
    } else if (/^[0-9]+$/.test(source)) {
        value = Number.parseInt(source, 10);
    } else {
        value = Number.NaN;
    }
    if (!Number.isInteger(value) || value < 0 || value > maximum) {
        throw new RangeError(`${label} is out of range`);
    }
    return value;
}

function address(text) {
    return parseNumber(text, 0xffff, "Address");
}

function sourceLocation(value) {
    const text = value;
    const separator = text.lastIndexOf(":");
    if (separator <= 0 || separator + 1 >= text.length) {
        throw new TypeError("Source location must use path:line");
    }
    const sourcePath = text.slice(0, separator);
    const line = parseNumber(
        text.slice(separator + 1),
        0xffff_ffff,
        "Source line",
    );
    if (line < 1) {
        throw new RangeError("Source line must be positive");
    }
    return {sourcePath, line};
}

function viewOptions() {
    const memoryAddress = address(elements["memory-address"].value);
    const memoryLength = parseNumber(elements["memory-length"].value, 256, "Memory length");
    if (memoryLength < 1 || memoryAddress + memoryLength > 0x1_0000) {
        throw new RangeError("Memory range is out of bounds");
    }
    const firstAddress = address(elements["trace-first-address"].value);
    const lastAddress = address(elements["trace-last-address"].value);
    if (firstAddress > lastAddress) {
        throw new RangeError("Trace address range is reversed");
    }
    const kind = elements["trace-kind"].value;
    if (!Object.hasOwn(AccessTraceMask, kind)) {
        throw new TypeError("Trace kind is invalid");
    }
    return {
        memoryAddress,
        memoryLength,
        traceFilter: {
            firstAddress,
            lastAddress,
            kindMask: AccessTraceMask[kind],
        },
    };
}

function byteValue(id, label) {
    return parseNumber(elements[id].value, 0xff, label);
}

function wordValue(id, label) {
    return parseNumber(elements[id].value, 0xffff, label);
}

function knownBitsConfiguration(prefix, maximum, label) {
    if (!elements[`${prefix}-enabled`].checked) {
        return undefined;
    }
    const value = byteValue(`${prefix}-value`, `${label} value`);
    const knownMask = byteValue(
        `${prefix}-known-mask`,
        `${label} known mask`,
    );
    if (value > maximum || knownMask > maximum) {
        throw new RangeError(`${label} exceeds its allowed mask`);
    }
    if ((value & ~knownMask) !== 0) {
        throw new RangeError(`${label} value contains unknown pin bits`);
    }
    return {value, knownMask};
}

function hardwareConfiguration() {
    const configuration = {};
    if (elements["reset-sp-enabled"].checked) {
        configuration.resetStackPointer = wordValue(
            "reset-sp-value",
            "Reset stack pointer",
        );
    }
    if (elements["reset-x-enabled"].checked) {
        configuration.resetIndexRegister = wordValue(
            "reset-x-value",
            "Reset index register",
        );
    }
    if (elements["reset-a-enabled"].checked) {
        configuration.resetAccumulatorA = byteValue(
            "reset-a-value",
            "Reset accumulator A",
        );
    }
    if (elements["reset-b-enabled"].checked) {
        configuration.resetAccumulatorB = byteValue(
            "reset-b-value",
            "Reset accumulator B",
        );
    }
    const resetConditionCode = knownBitsConfiguration(
        "reset-cc",
        0x2f,
        "Reset condition code",
    );
    if (resetConditionCode !== undefined) {
        configuration.resetConditionCode = resetConditionCode;
    }
    if (elements["internal-ram-enabled"].checked) {
        configuration.internalRamInitialValue = byteValue(
            "internal-ram-value",
            "Internal RAM initial value",
        );
    }
    if (elements["standard-ram-enabled"].checked) {
        configuration.standardRamInitialValue = byteValue(
            "standard-ram-value",
            "Standard RAM initial value",
        );
    }
    if (elements["expansion-ram-enabled"].checked) {
        configuration.expansionRamInitialValue = byteValue(
            "expansion-ram-value",
            "Expansion RAM initial value",
        );
    }
    if (elements["lcd-enabled"].checked) {
        configuration.lcdUnknownDataReadValue = byteValue(
            "lcd-value",
            "LCD unknown-data read value",
        );
    }
    if (elements["calendar-enabled"].checked) {
        configuration.calendarAddressSource =
            elements["calendar-address-source"].value;
        configuration.calendarUpperRead =
            elements["calendar-upper-read"].value;
        if (elements["calendar-cpu-cycle-ratio-enabled"].checked) {
            configuration.calendarCpuCycleRatio =
                "e030-nominal-1.2288mhz";
        }
    }
    const port1Pins = knownBitsConfiguration("port1", 0xff, "Port 1");
    const port2Pins = knownBitsConfiguration("port2", 0x1f, "Port 2");
    if (port1Pins !== undefined) {
        configuration.port1Pins = port1Pins;
    }
    if (port2Pins !== undefined) {
        configuration.port2Pins = port2Pins;
    }
    const ramStandby = elements["ram-standby"].value;
    if (ramStandby === "valid") {
        configuration.ramStandbyPowerValid = true;
    } else if (ramStandby === "invalid") {
        configuration.ramStandbyPowerValid = false;
    } else if (ramStandby !== "") {
        throw new RangeError("Unknown RAM standby selection");
    }
    if (elements["keyboard-window-enabled"].checked) {
        configuration.keyboardWindowValue = byteValue(
            "keyboard-window-value",
            "Keyboard window value",
        );
    }
    return configuration;
}

function applyBasicBootExperimentControls() {
    const configuration = jr800BasicBootExperimentConfiguration();
    for (const id of [
        "reset-sp-enabled",
        "reset-x-enabled",
        "reset-a-enabled",
        "reset-b-enabled",
        "reset-cc-enabled",
    ]) {
        elements[id].checked = false;
    }

    for (const [enabledId, valueId, value] of [
        ["internal-ram-enabled", "internal-ram-value", configuration.internalRamInitialValue],
        ["standard-ram-enabled", "standard-ram-value", configuration.standardRamInitialValue],
        ["expansion-ram-enabled", "expansion-ram-value", configuration.expansionRamInitialValue],
        ["lcd-enabled", "lcd-value", configuration.lcdUnknownDataReadValue],
        ["keyboard-window-enabled", "keyboard-window-value", configuration.keyboardWindowValue],
    ]) {
        elements[enabledId].checked = true;
        elements[valueId].value = hex(value, 2);
    }

    elements["calendar-enabled"].checked = true;
    elements["calendar-address-source"].value =
        configuration.calendarAddressSource;
    elements["calendar-upper-read"].value = configuration.calendarUpperRead;
    elements["calendar-cpu-cycle-ratio-enabled"].checked = false;

    for (const [prefix, pins] of [
        ["port1", configuration.port1Pins],
        ["port2", configuration.port2Pins],
    ]) {
        elements[`${prefix}-enabled`].checked = true;
        elements[`${prefix}-value`].value = hex(pins.value, 2);
        elements[`${prefix}-known-mask`].value = hex(pins.knownMask, 2);
    }
    elements["ram-standby"].value = configuration.ramStandbyPowerValid
        ? "valid"
        : "invalid";
    setControls();
    return hardwareConfiguration();
}

function hex(value, width) {
    return `$${Number(value).toString(16).toUpperCase().padStart(width, "0")}`;
}

function registerText(state, field, width, mask) {
    return (state.registerKnownMask & mask) !== 0
        ? hex(state[field], width)
        : "?".repeat(width);
}

function conditionCodeText(state) {
    if (state.conditionCodeKnownMask === 0xff) {
        return hex(state.conditionCode, 2);
    }
    let bits = "";
    for (let mask = 0x80; mask !== 0; mask >>= 1) {
        bits += (state.conditionCodeKnownMask & mask) === 0
            ? "?"
            : (state.conditionCode & mask) === 0 ? "0" : "1";
    }
    return bits;
}

function renderLcdPanel(panel) {
    const canvas = elements["lcd-panel"];
    const context = canvas.getContext("2d");
    if (context === null) {
        elements["lcd-summary"].textContent = "Canvas rendering is unavailable";
        return;
    }

    const view = lcdPanelImage(panel);
    const image = context.createImageData(view.width, view.height);
    image.data.set(view.rgba);
    context.putImageData(image, 0, 0);
    elements["lcd-summary"].textContent = view.summary;
    canvas.setAttribute("aria-label", view.ariaLabel);
}

function renderLcdIndicators(indicators) {
    const view = lcdIndicatorView(indicators);
    for (const entry of view.entries) {
        const element = lcdIndicatorElements.get(entry.name);
        const value = element.querySelector("[data-lcd-indicator-value]");
        element.dataset.state = entry.state;
        element.setAttribute(
            "aria-label",
            `${entry.label} indicator: ${entry.description}; ${entry.detail}`,
        );
        element.title = `${entry.description}; ${entry.detail}`;
        value.textContent = entry.valueText;
    }
    elements["lcd-indicator-summary"].textContent = view.summary;
}

function renderKeyboardActivity(activity) {
    elements["keyboard-summary"].textContent = activity === null
        ? "Keyboard activity unavailable"
        : `${activity.readAttempts} read attempts · `
            + `${activity.distinctAddresses} distinct addresses`;
}

function byteList(bytes, length = bytes.length) {
    return bytes.slice(0, length).map((value) => hex(value, 2).slice(1)).join(" ");
}

function cell(text) {
    const td = document.createElement("td");
    td.textContent = text;
    return td;
}

function emptyRow(columnCount, text) {
    const row = document.createElement("tr");
    const value = cell(text);
    value.colSpan = columnCount;
    value.className = "empty";
    row.append(value);
    return row;
}

function renderMemory(memory) {
    const lines = [];
    for (let offset = 0; offset < memory.bytes.length; offset += 16) {
        const bytes = memory.bytes.slice(offset, offset + 16);
        const addressText = hex(memory.address + offset, 4);
        const hexText = byteList(bytes).padEnd(47, " ");
        const ascii = bytes.map((value) => value >= 0x20 && value <= 0x7e
            ? String.fromCharCode(value)
            : ".").join("");
        lines.push(`${addressText}  ${hexText}  ${ascii}`);
    }
    elements.memory.textContent = lines.join("\n") || "No memory loaded";
}

function renderHistory(history) {
    const rows = history.slice().reverse().map((entry) => {
        const row = document.createElement("tr");
        row.append(
            cell(String(entry.sequence)),
            cell(hex(entry.pcBefore, 4)),
            cell(byteList(entry.bytes, entry.bytesFetched)),
            cell(registerText(entry.stateAfter, "a", 2, 0x08)),
            cell(`${entry.cycleBegin} + ${entry.cycles}`),
            cell(entry.fault),
        );
        return row;
    });
    elements.history.replaceChildren(...(rows.length ? rows : [emptyRow(6, "No history")]));
}

function renderTrace(accesses) {
    const rows = accesses.slice(-256).reverse().map((entry) => {
        const row = document.createElement("tr");
        row.append(
            cell(String(entry.sequence)),
            cell(hex(entry.instructionPc, 4)),
            cell(entry.kind),
            cell(hex(entry.address, 4)),
            cell(entry.valueKnown ? hex(entry.value, 2) : "??"),
            cell(
                entry.previousValueKnown ? hex(entry.previousValue, 2) : "??",
            ),
        );
        return row;
    });
    elements.trace.replaceChildren(...(rows.length ? rows : [emptyRow(6, "No accesses")]));
    elements["access-count"].textContent = accesses.length > 256
        ? `${accesses.length} matching; latest 256 shown`
        : `${accesses.length} matching records`;
}

function expressionValueText(value) {
    const numeric = BigInt(value);
    let width = 16;
    if (numeric <= 0xffn) {
        width = 2;
    } else if (numeric <= 0xffffn) {
        width = 4;
    } else if (numeric <= 0xffff_ffffn) {
        width = 8;
    }
    return `$${numeric.toString(16).toUpperCase().padStart(width, "0")}`;
}

function expressionWatchResultText(watch) {
    if (watch.error === "none") {
        return expressionValueText(watch.value);
    }
    if (watch.error === "unknown-state") {
        return `${watch.error}: ${watch.stateFault}`;
    }
    if (watch.error === "memory-access") {
        return `${watch.error}: ${watch.busFault} at ${hex(watch.faultAddress, 4)}`;
    }
    return watch.error;
}

function renderExpressionWatches(watches) {
    const rows = watches.map((watch) => {
        const row = document.createElement("tr");
        const expression = cell(watch.expression);
        expression.className = "watch-label-cell";
        const removeCell = cell("");
        const remove = document.createElement("button");
        remove.type = "button";
        remove.dataset.watchId = String(watch.id);
        remove.className = "watch-remove";
        remove.textContent = "Remove";
        remove.disabled = running;
        removeCell.append(remove);
        row.append(
            expression,
            cell(expressionWatchResultText(watch)),
            removeCell,
        );
        return row;
    });
    elements["expression-watch-body"].replaceChildren(
        ...(rows.length ? rows : [emptyRow(3, "No expression watches")]),
    );
}

function symbolWatchResultText(watch) {
    const metadata = [watch.kind, watch.binding, `size ${watch.size}`];
    if (watch.sourceFileIndex !== null) {
        metadata.push(`source #${watch.sourceFileIndex}`);
    }
    return `${hex(watch.value, 4)} · ${metadata.join(" · ")}`;
}

function renderSymbolWatches(watches) {
    const rows = watches.map((watch) => {
        const row = document.createElement("tr");
        const name = cell(watch.name);
        name.className = "watch-label-cell";
        const removeCell = cell("");
        const remove = document.createElement("button");
        remove.type = "button";
        remove.dataset.watchId = String(watch.id);
        remove.className = "watch-remove";
        remove.textContent = "Remove";
        remove.disabled = running;
        removeCell.append(remove);
        row.append(name, cell(symbolWatchResultText(watch)), removeCell);
        return row;
    });
    elements["symbol-watch-body"].replaceChildren(
        ...(rows.length ? rows : [emptyRow(3, "No symbol watches")]),
    );
}

function render(snapshot) {
    const {state} = snapshot;
    elements.profile.textContent = state.profile;
    elements["register-pc"].textContent = registerText(state, "pc", 4, 0x01);
    elements["register-sp"].textContent = registerText(state, "sp", 4, 0x02);
    elements["register-x"].textContent = registerText(state, "x", 4, 0x04);
    elements["register-a"].textContent = registerText(state, "a", 2, 0x08);
    elements["register-b"].textContent = registerText(state, "b", 2, 0x10);
    elements["register-cc"].textContent = conditionCodeText(state);
    elements["execution-state"].textContent = state.executionState;
    elements["calendar-alarm-terminal"].textContent =
        state.calendarAlarmTerminal;
    elements["port2-timer-output"].textContent = state.port2TimerOutput;
    elements["lcd-substituted-read-count"].textContent =
        state.lcdSubstitutedDataReadCount === null
            ? "unavailable"
            : String(state.lcdSubstitutedDataReadCount);
    elements.cycles.textContent = String(state.cycleCount);
    elements["current-address"].textContent = hex(snapshot.disassembly.address, 4);
    elements.disassembly.textContent = snapshot.disassembly.text;
    elements.source.textContent = snapshot.source
        ? `${snapshot.source.path}:${snapshot.source.line}:${snapshot.source.column}`
        : machineKind === "jr800"
            ? "Source mapping is unavailable for a logical ROM"
            : "No source mapping";
    renderMemory(snapshot.memory);
    renderLcdPanel(snapshot.lcdPanel);
    renderLcdIndicators(snapshot.lcdIndicators);
    renderKeyboardActivity(snapshot.keyboardActivity);
    renderHistory(snapshot.history);
    renderTrace(snapshot.accesses);
    renderExpressionWatches(snapshot.expressionWatches ?? []);
    renderSymbolWatches(snapshot.symbolWatches ?? []);
}

function renderPoweredOff() {
    elements.profile.textContent = "Power off";
    elements["register-pc"].textContent = "----";
    elements["register-sp"].textContent = "----";
    elements["register-x"].textContent = "----";
    elements["register-a"].textContent = "--";
    elements["register-b"].textContent = "--";
    elements["register-cc"].textContent = "--";
    elements["execution-state"].textContent = "off";
    elements["calendar-alarm-terminal"].textContent = "unavailable";
    elements["port2-timer-output"].textContent = "unavailable";
    elements["lcd-substituted-read-count"].textContent = "unavailable";
    elements.cycles.textContent = "0";
    elements["current-address"].textContent = "$----";
    elements.disassembly.textContent = "Power off";
    elements.source.textContent = "No source mapping";
    renderMemory({address: 0, bytes: []});
    renderLcdPanel(null);
    renderLcdIndicators(null);
    renderKeyboardActivity(null);
    renderHistory([]);
    renderTrace([]);
    renderExpressionWatches([]);
    renderSymbolWatches([]);
}

function stopText(stop) {
    const count = stop.totalInstructionsExecuted ?? stop.instructionsExecuted;
    const trigger = stop.reason === "memory-watchpoint"
        || stop.reason === "execution-breakpoint"
        || stop.reason === "breakpoint-condition-error"
        || stop.reason === "address-reached"
        || stop.reason === "step-out-complete"
        ? ` at ${hex(stop.triggerAddress, 4)}`
        : "";
    const access = stop.reason === "memory-watchpoint" && stop.triggerAccess
        ? ` (${stop.triggerAccess})`
        : "";
    const suspended = stop.totalSuspendedCyclesElapsed
        ? `; ${stop.totalSuspendedCyclesElapsed} suspended cycles`
        : "";
    const fault = stop.fault && stop.fault !== "none"
        ? `; fault ${stop.fault}`
        : "";
    const condition = stop.reason === "breakpoint-condition-error"
        ? `; condition ${stop.conditionError}`
            + (stop.stateFault !== "none" ? ` (${stop.stateFault})` : "")
            + (stop.busFault !== "none" ? ` (${stop.busFault})` : "")
            + (stop.conditionError === "memory-access"
                ? ` at ${hex(stop.conditionFaultAddress, 4)}`
                : "")
        : "";
    return `Stopped: ${stop.reason}${trigger}${access}; ${count} instructions${suspended}${fault}${condition}`;
}

async function perform(operation, pendingText) {
    try {
        if (pendingText) {
            setStatus(pendingText, "running");
        }
        return await operation();
    } catch (error) {
        setStatus(error instanceof Error ? error.message : String(error), "error");
        throw error;
    }
}

async function readLinkedBinary(file, label) {
    if (file.size === 0) {
        throw new TypeError(`${label} must not be empty`);
    }
    if (file.size > MAX_LINKED_BINARY_BYTES) {
        throw new RangeError(
            `${label} exceeds ${MAX_LINKED_BINARY_BYTES} bytes`,
        );
    }
    return file.arrayBuffer();
}

async function readJr8rom(file) {
    if (file.size === 0) {
        throw new TypeError("JR8ROM file must not be empty");
    }
    if (file.size > MAX_JR8ROM_BYTES) {
        throw new RangeError(
            `JR8ROM file exceeds ${MAX_JR8ROM_BYTES} bytes`,
        );
    }
    return file.arrayBuffer();
}

async function readRawRom(file) {
    if (file.size !== JR800_LOGICAL_ROM_BYTES) {
        throw new RangeError(
            `Raw ROM file must be exactly ${JR800_LOGICAL_ROM_BYTES} bytes`,
        );
    }
    return file.arrayBuffer();
}

function romFileFormat(file) {
    const name = file.name.toLowerCase();
    if (name.endsWith(".j8r")) {
        return "jr8rom";
    }
    if (name.endsWith(".rom")) {
        return "raw";
    }
    throw new TypeError("ROM file must use the .j8r or .rom extension");
}

function selectedRomFile() {
    const file = elements["jr8rom-file"].files?.[0];
    if (!file) {
        throw new Error("Select a .j8r or .rom file");
    }
    return file;
}

function rawRomLoadApproved() {
    const pendingFile = elements["jr8rom-file"].files?.[0];
    return !pendingFile?.name.toLowerCase().endsWith(".rom")
        || window.confirm(elements["raw-rom-warning"].textContent);
}

async function loadJr800Machine(romFile, configuration) {
    const format = romFileFormat(romFile);
    const romData = format === "jr8rom"
        ? await readJr8rom(romFile)
        : await readRawRom(romFile);
    const initialView = {memoryAddress: 0x8000, memoryLength: 32};
    const command = format === "jr8rom" ? "load-jr800" : "load-jr800-raw";
    const romField = format === "jr8rom"
        ? {romContainer: romData}
        : {logicalRom: romData};
    const result = await client.request(command, {
        ...romField,
        configuration,
        view: initialView,
    }, [romData]);
    machineKind = "jr800";
    calendarAttached = configuration.calendarAddressSource !== undefined;
    loaded = true;
    releaseAllVirtualKeys(false);
    nextExpressionWatchId = 1;
    nextSymbolWatchId = 1;
    elements["memory-address"].value = "$8000";
    elements["breakpoint-address"].value = "$8000";
    elements["run-to-address"].value = "$8000";
    render(result);
    setControls();
}

elements.load.addEventListener("click", () => {
    void perform(async () => {
        const applicationFile = elements["application-file"].files?.[0];
        if (!applicationFile) {
            throw new Error("Select a JR8APP file");
        }
        const debugFile = elements["debug-file"].files?.[0];
        const application = await readLinkedBinary(applicationFile, "JR8APP file");
        const debugInfo = debugFile
            ? await readLinkedBinary(debugFile, "JR8DBG file")
            : undefined;
        const transfer = debugInfo ? [application, debugInfo] : [application];
        const result = await client.request("load", {
            application,
            debugInfo,
            stackPointer: address(elements["stack-pointer"].value),
            view: viewOptions(),
        }, transfer);
        basicRunContinuous = false;
        machineKind = "synthetic";
        calendarAttached = false;
        loaded = true;
        releaseAllVirtualKeys(false);
        nextExpressionWatchId = 1;
        nextSymbolWatchId = 1;
        render(result);
        setStatus(`Loaded ${applicationFile.name}`, "ready");
        setControls();
    }, "Loading application").catch(() => {});
});

elements["load-rom"].addEventListener("click", () => {
    if (!rawRomLoadApproved()) {
        return;
    }
    void perform(async () => {
        const romFile = selectedRomFile();
        const configuration = hardwareConfiguration();
        basicRunContinuous = false;
        await loadJr800Machine(romFile, configuration);
        setStatus(`Loaded local ROM ${romFile.name}`, "ready");
    }, "Loading local ROM").catch(() => {});
});

elements["boot-basic"].addEventListener("click", () => {
    if (!rawRomLoadApproved()) {
        return;
    }
    void perform(async () => {
        const romFile = selectedRomFile();
        const configuration = applyBasicBootExperimentControls();
        basicRunContinuous = false;
        await loadJr800Machine(romFile, configuration);
        await startBasicRun();
    }, "Starting BASIC experiment").catch(() => {
        basicRunContinuous = false;
        running = false;
        setControls();
    });
});

elements["load-program"].addEventListener("click", () => {
    void perform(async () => {
        const programFile = elements["hardware-program-file"].files?.[0];
        if (!programFile) {
            throw new Error("Select a JR8APP RAM program file");
        }
        const application = await readLinkedBinary(
            programFile,
            "JR8APP RAM program file",
        );
        const result = await client.request("load-program", {
            application,
            view: viewOptions(),
        }, [application]);
        basicRunContinuous = false;
        releaseAllVirtualKeys(false);
        render(result);
        setStatus(`Loaded RAM program ${programFile.name}`, "ready");
        setControls();
    }, "Loading RAM program").catch(() => {});
});

elements.reset.addEventListener("click", () => {
    void perform(async () => {
        basicRunContinuous = false;
        const snapshot = await client.request("reset", {view: viewOptions()});
        render(snapshot);
        setStatus("Machine reset", "ready");
    }, "Resetting machine").catch(() => {});
});

elements.step.addEventListener("click", () => {
    void perform(async () => {
        const result = await client.request("step", {view: viewOptions()});
        render(result.snapshot);
        setStatus(stopText(result.stop), "ready");
    }, "Stepping").catch(() => {});
});

function executionLimits() {
    const instructionLimit = parseNumber(
        elements["instruction-limit"].value,
        Number.MAX_SAFE_INTEGER,
        "Instruction limit",
    );
    if (instructionLimit < 1) {
        throw new RangeError("Instruction limit must be positive");
    }
    const suspendedCycleLimit = parseNumber(
        elements["suspended-cycle-limit"].value,
        0xffff_ffff,
        "Suspended cycle limit",
    );
    if (suspendedCycleLimit < 1) {
        throw new RangeError("Suspended cycle limit must be positive");
    }
    return {instructionLimit, suspendedCycleLimit};
}

async function startRun(command, fields, status, limits = executionLimits()) {
    const result = await client.request(command, {
        ...limits,
        ...fields,
        view: viewOptions(),
    });
    running = result.running;
    setStatus(status, "running");
    setControls();
}

async function startBasicRun() {
    basicRunContinuous = true;
    try {
        await startRun(
            "run",
            {},
            "BASIC running",
            Jr800BasicRunSlice,
        );
    } catch (error) {
        basicRunContinuous = false;
        throw error;
    }
}

elements["resume-machine"].addEventListener("click", () => {
    void perform(() => startBasicRun()).catch(() => {
        basicRunContinuous = false;
        running = false;
        setControls();
    });
});

elements["pause-basic"].addEventListener("click", () => {
    basicRunContinuous = false;
    void perform(async () => {
        await client.request("pause");
        setStatus("Pause requested", "running");
    }).catch(() => {});
});

elements.run.addEventListener("click", () => {
    basicRunContinuous = false;
    void perform(() => startRun("run", {}, "Running in worker"))
        .catch(() => {});
});

elements["step-over"].addEventListener("click", () => {
    basicRunContinuous = false;
    void perform(() => startRun(
        "step-over",
        {},
        "Stepping over current instruction",
    )).catch(() => {});
});

elements["step-out"].addEventListener("click", () => {
    basicRunContinuous = false;
    void perform(() => startRun(
        "step-out",
        {},
        "Stepping out of current routine",
    )).catch(() => {});
});

elements["run-to"].addEventListener("click", () => {
    basicRunContinuous = false;
    void perform(() => {
        const target = address(elements["run-to-address"].value);
        return startRun(
            "run-to",
            {address: target},
            `Running to ${hex(target, 4)}`,
        );
    }).catch(() => {});
});

elements["run-to-source"].addEventListener("click", () => {
    basicRunContinuous = false;
    void perform(() => {
        const target = sourceLocation(
            elements["run-to-source-location"].value,
        );
        return startRun(
            "run-to-source",
            target,
            `Running to ${target.sourcePath}:${target.line}`,
        );
    }).catch(() => {});
});

elements["run-to-symbol"].addEventListener("click", () => {
    basicRunContinuous = false;
    void perform(() => {
        const symbolName = elements["run-to-symbol-name"].value;
        if (symbolName.length === 0) {
            throw new TypeError("Symbol name must be nonempty");
        }
        return startRun(
            "run-to-symbol",
            {symbolName},
            `Running to symbol ${symbolName}`,
        );
    }).catch(() => {});
});

elements.pause.addEventListener("click", () => {
    basicRunContinuous = false;
    void perform(async () => {
        await client.request("pause");
        setStatus("Pause requested", "running");
    }).catch(() => {});
});

async function setPoint(command, input, enabled, label, condition = "") {
    const value = address(input.value);
    await client.request(command, {address: value, enabled, condition});
    const conditionText = enabled && condition.length !== 0
        ? ` when ${condition}`
        : "";
    setStatus(
        `${label} ${enabled ? "enabled" : "disabled"} at ${hex(value, 4)}${conditionText}`,
        "ready",
    );
}

elements["enable-breakpoint"].addEventListener("click", () => {
    void perform(() => setPoint(
        "set-execution-breakpoint",
        elements["breakpoint-address"],
        true,
        "Execution breakpoint",
        elements["breakpoint-condition"].value.trim(),
    )).catch(() => {});
});
elements["disable-breakpoint"].addEventListener("click", () => {
    void perform(() => setPoint(
        "set-execution-breakpoint",
        elements["breakpoint-address"],
        false,
        "Execution breakpoint",
    )).catch(() => {});
});
elements["enable-watchpoint"].addEventListener("click", () => {
    void perform(async () => {
        const value = address(elements["watchpoint-address"].value);
        const mode = elements["watchpoint-mode"].value;
        await client.request("set-memory-watchpoint", {
            address: value,
            mode,
            enabled: true,
        });
        setStatus(
            `Memory watchpoint (${mode}) enabled at ${hex(value, 4)}`,
            "ready",
        );
    }).catch(() => {});
});
elements["disable-watchpoint"].addEventListener("click", () => {
    void perform(async () => {
        const value = address(elements["watchpoint-address"].value);
        const mode = elements["watchpoint-mode"].value;
        await client.request("set-memory-watchpoint", {
            address: value,
            mode,
            enabled: false,
        });
        setStatus(
            `Memory watchpoint (${mode}) disabled at ${hex(value, 4)}`,
            "ready",
        );
    }).catch(() => {});
});

elements["add-expression-watch"].addEventListener("click", () => {
    void perform(async () => {
        const expression = elements["watch-expression"].value;
        if (nextExpressionWatchId > 0xffff_ffff) {
            throw new RangeError("Expression watch ID space is exhausted");
        }
        const snapshot = await client.request("set-expression-watch", {
            watchId: nextExpressionWatchId,
            expression,
            view: viewOptions(),
        });
        ++nextExpressionWatchId;
        elements["watch-expression"].value = "";
        render(snapshot);
        setStatus(`Expression watch added: ${expression}`, "ready");
    }, "Adding expression watch").catch(() => {});
});

elements["expression-watch-body"].addEventListener("click", (event) => {
    const button = event.target instanceof Element
        ? event.target.closest("button[data-watch-id]")
        : null;
    if (button === null
        || !elements["expression-watch-body"].contains(button)) {
        return;
    }
    void perform(async () => {
        const watchId = Number(button.dataset.watchId);
        const snapshot = await client.request("clear-expression-watch", {
            watchId,
            view: viewOptions(),
        });
        render(snapshot);
        setStatus("Expression watch removed", "ready");
    }, "Removing expression watch").catch(() => {});
});

elements["add-symbol-watch"].addEventListener("click", () => {
    void perform(async () => {
        const name = elements["watch-symbol"].value;
        if (nextSymbolWatchId > 0xffff_ffff) {
            throw new RangeError("Symbol watch ID space is exhausted");
        }
        const snapshot = await client.request("set-symbol-watch", {
            watchId: nextSymbolWatchId,
            name,
            view: viewOptions(),
        });
        ++nextSymbolWatchId;
        elements["watch-symbol"].value = "";
        render(snapshot);
        setStatus(`Symbol watch added: ${name}`, "ready");
    }, "Adding symbol watch").catch(() => {});
});

elements["symbol-watch-body"].addEventListener("click", (event) => {
    const button = event.target instanceof Element
        ? event.target.closest("button[data-watch-id]")
        : null;
    if (button === null || !elements["symbol-watch-body"].contains(button)) {
        return;
    }
    void perform(async () => {
        const watchId = Number(button.dataset.watchId);
        const snapshot = await client.request("clear-symbol-watch", {
            watchId,
            view: viewOptions(),
        });
        render(snapshot);
        setStatus("Symbol watch removed", "ready");
    }, "Removing symbol watch").catch(() => {});
});

elements["set-keyboard-response"].addEventListener("click", () => {
    void perform(async () => {
        const keyboardAddress = address(elements["keyboard-address"].value);
        if (keyboardAddress < 0x0c00 || keyboardAddress > 0x0fff) {
            throw new RangeError("Keyboard address must be within $0C00-$0FFF");
        }
        const known = elements["keyboard-known"].checked;
        const value = known
            ? byteValue("keyboard-value", "Keyboard response")
            : 0;
        const update = await client.request("set-keyboard-response", {
            address: keyboardAddress,
            value,
            known,
        });
        if (!update.appliedDuringRun) {
            const snapshot = await client.request("snapshot", {
                view: viewOptions(),
            });
            render(snapshot);
        }
        const timing = update.appliedDuringRun
            ? ` at a run boundary after ${update.totalInstructionsExecuted} instructions`
            : "";
        setStatus(
            known
                ? `Raw keyboard response set at ${hex(keyboardAddress, 4)}${timing}`
                : `Raw keyboard response cleared at ${hex(keyboardAddress, 4)}${timing}`,
            update.appliedDuringRun ? "running" : "ready",
        );
    }, "Updating raw keyboard response").catch(() => {});
});

elements["advance-calendar"].addEventListener("click", () => {
    void perform(async () => {
        const ticks = parseNumber(
            elements["calendar-oscillator-ticks"].value,
            0xffff_ffff,
            "Calendar oscillator ticks",
        );
        const result = await client.request("advance-calendar-oscillator", {
            ticks,
            view: viewOptions(),
        });
        render(result.snapshot);
        setStatus(`Advanced calendar by ${ticks} oscillator ticks`, "ready");
    }, "Advancing calendar oscillator").catch(() => {});
});

elements["adjust-calendar-seconds"].addEventListener("click", () => {
    void perform(async () => {
        const snapshot = await client.request("adjust-calendar-seconds", {
            view: viewOptions(),
        });
        render(snapshot);
        setStatus("Adjusted calendar seconds once", "ready");
    }, "Adjusting calendar seconds").catch(() => {});
});

elements["refresh-memory"].addEventListener("click", () => {
    void perform(async () => {
        const snapshot = await client.request("snapshot", {view: viewOptions()});
        render(snapshot);
        setStatus("Snapshot refreshed", "ready");
    }, "Refreshing snapshot").catch(() => {});
});

elements["apply-trace-filter"].addEventListener("click", () => {
    void perform(async () => {
        const snapshot = await client.request("snapshot", {view: viewOptions()});
        render(snapshot);
        setStatus("Trace filter applied", "ready");
    }, "Applying trace filter").catch(() => {});
});

elements["clear-history"].addEventListener("click", () => {
    void perform(async () => {
        await client.request("clear-history");
        const snapshot = await client.request("snapshot", {view: viewOptions()});
        render(snapshot);
        setStatus("History and trace cleared", "ready");
    }).catch(() => {});
});

elements["layout-workbench"].addEventListener("click", () => {
    setMachineLayout("workbench");
});
elements["layout-device"].addEventListener("click", () => {
    setMachineLayout("device");
});
elements["force-power-off"].addEventListener("click", () => {
    if (!virtualKeyboardAvailable() || !window.confirm(
        "Force power off the emulator? The loaded session will be discarded.",
    )) {
        return;
    }
    basicRunContinuous = false;
    void perform(async () => {
        const result = await client.request("power-off");
        if (result.poweredOff !== true) {
            throw new Error("Worker did not confirm power off");
        }
        running = false;
        loaded = false;
        machineKind = "synthetic";
        calendarAttached = false;
        releaseAllVirtualKeys(false);
        renderPoweredOff();
        setControls();
        setStatus("Power off", "idle");
    }, "Powering off").catch(() => {});
});

elements["virtual-keyboard-card"].addEventListener("pointerdown", (event) => {
    const button = event.target instanceof Element
        ? event.target.closest("button[data-jr800-key]")
        : null;
    if (button === null || button.disabled
        || button.dataset.latching === "true") {
        return;
    }
    event.preventDefault();
    button.setPointerCapture(event.pointerId);
    sendVirtualKeyboardTransition(virtualKeyboardState.press(
        `pointer:${event.pointerId}`,
        button.dataset.jr800Key,
    ));
});

function releaseVirtualPointer(event) {
    sendVirtualKeyboardTransition(
        virtualKeyboardState.release(`pointer:${event.pointerId}`),
    );
}

elements["virtual-keyboard-card"].addEventListener(
    "pointerup",
    releaseVirtualPointer,
);
elements["virtual-keyboard-card"].addEventListener(
    "pointercancel",
    releaseVirtualPointer,
);
elements["virtual-keyboard-card"].addEventListener(
    "lostpointercapture",
    releaseVirtualPointer,
);

elements["virtual-keyboard-card"].addEventListener("click", (event) => {
    const button = event.target instanceof Element
        ? event.target.closest("button[data-jr800-key]")
        : null;
    if (button === null || button.disabled) {
        return;
    }
    const key = button.dataset.jr800Key;
    if (button.dataset.latching === "true") {
        sendVirtualKeyboardTransition(virtualKeyboardState.toggleLatch(key));
        event.preventDefault();
    }
});

elements["virtual-keyboard-card"].addEventListener("keydown", (event) => {
    const button = event.target instanceof Element
        ? event.target.closest("button[data-jr800-key]")
        : null;
    if (button === null || button.disabled
        || button.dataset.latching === "true"
        || !["Enter", "NumpadEnter", "Space"].includes(event.code)) {
        return;
    }
    event.preventDefault();
    sendVirtualKeyboardTransition(virtualKeyboardState.press(
        `button:${event.code}`,
        button.dataset.jr800Key,
    ));
});

document.addEventListener("keyup", (event) => {
    if (!["Enter", "NumpadEnter", "Space"].includes(event.code)) {
        return;
    }
    sendVirtualKeyboardTransition(
        virtualKeyboardState.release(`button:${event.code}`),
    );
});

document.addEventListener("keydown", (event) => {
    if (event.defaultPrevented || !virtualKeyboardAvailable()
        || hostKeyboardTargetIsInteractive(event.target)) {
        return;
    }
    const key = jr800KeyForHostCode(event.code);
    if (key === null) {
        return;
    }
    event.preventDefault();
    sendVirtualKeyboardTransition(virtualKeyboardState.press(
        `host:${event.code}`,
        key,
    ));
});

document.addEventListener("keyup", (event) => {
    const key = jr800KeyForHostCode(event.code);
    if (key === null) {
        return;
    }
    const transition = virtualKeyboardState.release(`host:${event.code}`);
    if (transition !== null) {
        event.preventDefault();
    }
    sendVirtualKeyboardTransition(transition);
});

window.addEventListener("blur", () => releaseAllVirtualKeys());
document.addEventListener("visibilitychange", () => {
    if (document.visibilityState === "hidden") {
        releaseAllVirtualKeys();
    }
});

client.on("stopped", (message) => {
    running = false;
    render(message.snapshot);
    if (basicRunContinuous && basicRunCanContinue(message.stop)
        && loaded && machineKind === "jr800") {
        void perform(() => startBasicRun()).catch(() => {
            basicRunContinuous = false;
            running = false;
            setControls();
        });
        return;
    }
    basicRunContinuous = false;
    setStatus(stopText(message.stop), "ready");
    setControls();
});
client.on("error", (message) => {
    basicRunContinuous = false;
    running = false;
    const detail = message?.error ?? message;
    setStatus(detail instanceof Error ? detail.message : String(detail), "error");
    setControls();
});

for (const id of [
    "reset-sp-enabled", "reset-x-enabled", "reset-a-enabled",
    "reset-b-enabled", "reset-cc-enabled", "internal-ram-enabled",
    "standard-ram-enabled",
    "expansion-ram-enabled", "lcd-enabled",
    "calendar-enabled", "calendar-cpu-cycle-ratio-enabled",
    "port1-enabled", "port2-enabled",
    "keyboard-window-enabled",
]) {
    elements[id].addEventListener("change", setControls);
}

setControls();
void perform(async () => {
    const moduleUrl = new URL("./jr800_wasm.mjs", import.meta.url).href;
    const result = await client.request("initialize", {moduleUrl});
    initialized = true;
    setStatus(`Worker ready; ABI ${result.abiVersion}`, "ready");
    setControls();
}, "Initializing worker").catch(() => {});
