// SPDX-License-Identifier: MIT

import {
    NativeProgramWavError,
    normalizeViewOptions,
    WasmMachine,
} from "./wasm-machine.mjs";

import {JR800_CPU_CLOCK_HZ, RUN_TURN_MS, RunPacer} from "./run-pacer.mjs";

const RUN_SLICE_INSTRUCTIONS = 1000;
// The debugger retains 1024 bus accesses, including fetches. Drain sound
// before a worst-case instruction batch can evict a Port 1 transition.
const AUDIO_SLICE_INSTRUCTIONS = 64;
const RUN_PROGRESS_INTERVAL_MS = 50;
const SUSPENDED_SLICE_CYCLES = 65_536;
const DEFAULT_SUSPENDED_CYCLE_LIMIT = SUSPENDED_SLICE_CYCLES;
const JR800_AUDIO_ACCESS_FILTER = Object.freeze({
    firstAddress: 0x0002,
    lastAddress: 0x0002,
    kindMask: 0x04,
});

let sendToHost;
let subscribe;

if (typeof self !== "undefined" && typeof self.postMessage === "function") {
    sendToHost = (message) => self.postMessage(message);
    subscribe = (callback) => self.addEventListener("message", (event) => callback(event.data));
} else {
    const {parentPort} = await import("node:worker_threads");
    if (parentPort === null) {
        throw new Error("Worker transport is unavailable");
    }
    sendToHost = (message) => parentPort.postMessage(message);
    subscribe = (callback) => parentPort.on("message", callback);
}

let machine;
let machineModuleUrl;
let running = false;
let runPacer = null;
let pacedRunEnd = null;
let lastRunProgressTime = 0;
let pauseRequested = false;
let runGeneration = 0;
let remainingInstructions = 0;
let executedInstructions = 0;
let remainingSuspendedCycles = DEFAULT_SUSPENDED_CYCLE_LIMIT;
let suspendedCyclesElapsed = 0;
let runTargetAddress = null;
let stepOverPending = false;
let stepOutState = null;
const expressionWatches = new Map();
const symbolWatches = new Map();
let audioEnabled = false;
let lastAudioAccessSequence = 0n;
let lastAudioLevel = null;
let audioFrameStartCycle = null;
let audioFrameInitialLevel = null;
let audioFrameTransitions = [];
const minimumKeyboardReleaseCycles = new Map();
const deferredKeyboardReleases = new Set();
const pendingKeyboardReleaseResponses = new Map();

function machineCycle(current) {
    return BigInt(current.state().cycleCount);
}

function clearKeyboardHoldState(current = null, release = false) {
    if (release && current?.kind === "jr800") {
        const keys = new Set([
            ...minimumKeyboardReleaseCycles.keys(),
            ...deferredKeyboardReleases,
        ]);
        for (const key of keys) {
            current.setKeyboardKeyState(key, false);
        }
    }
    for (const pending of pendingKeyboardReleaseResponses.values()) {
        for (const {id, result} of pending) {
            response(id, result);
        }
    }
    minimumKeyboardReleaseCycles.clear();
    deferredKeyboardReleases.clear();
    pendingKeyboardReleaseResponses.clear();
}

function applyDeferredKeyboardReleases(current) {
    if (current.kind !== "jr800" || deferredKeyboardReleases.size === 0) {
        return;
    }
    const cycle = machineCycle(current);
    for (const key of [...deferredKeyboardReleases]) {
        if (cycle < minimumKeyboardReleaseCycles.get(key)) {
            continue;
        }
        current.setKeyboardKeyState(key, false);
        for (const {id, result} of pendingKeyboardReleaseResponses.get(key) ?? []) {
            response(id, result);
        }
        deferredKeyboardReleases.delete(key);
        minimumKeyboardReleaseCycles.delete(key);
        pendingKeyboardReleaseResponses.delete(key);
    }
}

function resetAudioCollector() {
    lastAudioAccessSequence = 0n;
    lastAudioLevel = null;
    audioFrameStartCycle = null;
    audioFrameInitialLevel = null;
    audioFrameTransitions = [];
}

function synchronizeAudioCollector(current) {
    audioFrameStartCycle = Number(machineCycle(current));
    const records = current.kind === "jr800"
        ? current.accesses(JR800_AUDIO_ACCESS_FILTER)
        : [];
    for (const record of records) {
        lastAudioAccessSequence = BigInt(record.sequence);
        if (record.valueKnown) {
            lastAudioLevel = (record.value & 0x10) !== 0;
        }
    }
    audioFrameInitialLevel = lastAudioLevel;
}

function collectAudioTransitions(current) {
    if (!audioEnabled || current.kind !== "jr800") {
        return;
    }
    for (const record of current.accesses(JR800_AUDIO_ACCESS_FILTER)) {
        const sequence = BigInt(record.sequence);
        if (sequence <= lastAudioAccessSequence) {
            continue;
        }
        lastAudioAccessSequence = sequence;
        if (!record.valueKnown) {
            lastAudioLevel = null;
            continue;
        }
        const level = (record.value & 0x10) !== 0;
        const previousLevel = record.previousValueKnown
            ? (record.previousValue & 0x10) !== 0
            : lastAudioLevel;
        lastAudioLevel = level;
        if (previousLevel !== null && previousLevel !== level) {
            audioFrameTransitions.push({cycle: record.instructionCycle, level});
        }
    }
}

function emitAudioFrame(current) {
    if (!audioEnabled || current.kind !== "jr800") return;
    const endCycle = Number(machineCycle(current));
    if (audioFrameStartCycle !== null && endCycle > audioFrameStartCycle) {
        sendToHost({
            type: "event",
            event: "audio-transitions",
            clockHz: JR800_CPU_CLOCK_HZ,
            startCycle: audioFrameStartCycle,
            endCycle,
            initialLevel: audioFrameInitialLevel,
            transitions: audioFrameTransitions,
        });
    }
    audioFrameStartCycle = endCycle;
    audioFrameInitialLevel = lastAudioLevel;
    audioFrameTransitions = [];
}

function response(id, result) {
    sendToHost({type: "response", id, ok: true, result});
}

function failure(id, error) {
    const detail = error instanceof Error ? error.stack ?? error.message : String(error);
    const responseMessage = {type: "response", id, ok: false, error: detail};
    if (error instanceof NativeProgramWavError) {
        responseMessage.errorCode = "native-program-wav";
        responseMessage.issue = error.issue;
        responseMessage.burstIndex = error.burstIndex;
    }
    sendToHost(responseMessage);
}

function requireMachine() {
    if (!machine) {
        throw new Error("Worker has not been initialized");
    }
    return machine;
}

function requireIdle() {
    if (running) {
        throw new Error("Machine is running");
    }
}

function parseAddress(value) {
    if (!Number.isInteger(value) || value < 0 || value > 0xffff) {
        throw new RangeError("Address must be a uint16 value");
    }
    return value;
}

function normalizeMachineView(current, view) {
    if (view === undefined && current.kind === "jr800") {
        return normalizeViewOptions({memoryAddress: 0x8000});
    }
    return normalizeViewOptions(view);
}

function normalizeReadableMachineView(current, view) {
    const normalized = normalizeMachineView(current, view);
    current.memory(normalized.memoryAddress, normalized.memoryLength);
    return normalized;
}

function snapshotWithWatches(current, view) {
    const snapshot = current.snapshot(view);
    const expressionResults = [];
    for (const [id, expression] of expressionWatches) {
        expressionResults.push({
            id,
            expression,
            ...current.evaluateExpressionWatch(id),
        });
    }
    const symbolResults = [];
    for (const [id, name] of symbolWatches) {
        symbolResults.push({
            id,
            name,
            ...current.evaluateSymbolWatch(id),
        });
    }
    return {
        ...snapshot,
        expressionWatches: expressionResults,
        symbolWatches: symbolResults,
    };
}

function stopFocusAddress(stop, state) {
    if (stop.reason === "memory-watchpoint" || stop.reason === "cpu-fault") {
        return stop.pcBefore;
    }
    if (stop.reason === "execution-breakpoint"
        || stop.reason === "breakpoint-condition-error"
        || stop.reason === "address-reached") {
        return stop.triggerAddress;
    }
    return state.pc;
}

function emitStopped(
    stop,
    memoryAddress = 0,
    memoryLength = 32,
    traceFilter = undefined,
) {
    const current = requireMachine();
    const state = current.state();
    const snapshot = snapshotWithWatches(current, {
        memoryAddress,
        memoryLength,
        focusAddress: stopFocusAddress(stop, state),
        traceFilter,
    });
    sendToHost({type: "event", event: "stopped", stop, snapshot});
}

function finishRun(stop, memoryAddress, memoryLength, traceFilter) {
    emitAudioFrame(requireMachine());
    pacedRunEnd = runPacer !== null
        && (stop.reason === "instruction-limit" || stop.reason === "sleeping")
        ? {machine: requireMachine(), cycle: Number(machineCycle(requireMachine()))}
        : null;
    if (pacedRunEnd === null) {
        runPacer = null;
    }
    running = false;
    pauseRequested = false;
    runTargetAddress = null;
    stepOverPending = false;
    stepOutState = null;
    if (stop.reason !== "instruction-limit" && stop.reason !== "sleeping") {
        clearKeyboardHoldState(requireMachine(), true);
    }
    emitStopped(
        {
            ...stop,
            totalInstructionsExecuted: executedInstructions,
            totalSuspendedCyclesElapsed: suspendedCyclesElapsed,
        },
        memoryAddress,
        memoryLength,
        traceFilter,
    );
}

function scheduleRunSlice(
    generation,
    memoryAddress,
    memoryLength,
    traceFilter,
) {
    setTimeout(() => {
        if (!running || generation !== runGeneration) {
            return;
        }
        if (pauseRequested) {
            const state = requireMachine().state();
            finishRun(
                {
                    reason: "paused",
                    fault: "none",
                    triggerAddress: state.pc,
                    instructionsExecuted: 0,
                    pcBefore: state.pc,
                    pcAfter: state.pc,
                    bytes: [0, 0, 0],
                    instructionLength: 0,
                    bytesFetched: 0,
                    cycles: 0,
                    stepOutState,
                },
                memoryAddress,
                memoryLength,
                traceFilter,
            );
            return;
        }

        try {
            const deadline = performance.now() + RUN_TURN_MS;
            do {
                const slice = Math.min(remainingInstructions,
                    audioEnabled ? AUDIO_SLICE_INSTRUCTIONS : RUN_SLICE_INSTRUCTIONS);
                const current = requireMachine();
                let stop;
                if (stepOutState !== null) {
                    const result = current.stepOut(slice, stepOutState);
                    stop = result.stop;
                    if (stop.reason === "instruction-limit"
                        || stop.reason === "sleeping") {
                        stepOutState = result.state;
                        stop = {...stop, stepOutState: result.state};
                    } else {
                        stepOutState = null;
                    }
                } else if (stepOverPending) {
                    stepOverPending = false;
                    stop = current.stepOver(slice);
                    if (stop.continuationAddress !== null) {
                        runTargetAddress = stop.continuationAddress;
                    }
                } else {
                    stop = runTargetAddress === null
                        ? current.run(slice)
                        : current.runTo(runTargetAddress, slice);
                }
                applyDeferredKeyboardReleases(current);
                collectAudioTransitions(current);
                const executed = Number(stop.instructionsExecuted);
                executedInstructions += executed;
                remainingInstructions -= executed;
                if (current.kind === "jr800" && stop.reason === "sleeping"
                    && remainingInstructions > 0) {
                    if (remainingSuspendedCycles === 0) {
                        finishRun(stop, memoryAddress, memoryLength, traceFilter);
                        return;
                    }
                    const cycleSlice = Math.min(
                        remainingSuspendedCycles,
                        SUSPENDED_SLICE_CYCLES,
                        runPacer === null ? SUSPENDED_SLICE_CYCLES
                            : Math.max(1, runPacer.cycleBudget(
                                Number(machineCycle(current)), performance.now(),
                            )),
                    );
                    const suspended = current.advanceSuspendedCycles(
                        cycleSlice,
                    );
                    applyDeferredKeyboardReleases(current);
                    suspendedCyclesElapsed += suspended.cyclesElapsed;
                    remainingSuspendedCycles -= suspended.cyclesElapsed;
                    if (suspended.busFault !== "none") {
                        finishRun(
                            {
                                ...stop,
                                reason: "cpu-fault",
                                fault: "bus-advance",
                                busFault: suspended.busFault,
                                suspendedAdvance: suspended,
                            },
                            memoryAddress,
                            memoryLength,
                            traceFilter,
                        );
                        return;
                    }
                    if (!suspended.interruptKnown
                        || suspended.interruptSource !== "none") {
                        continue;
                    }
                    if (remainingSuspendedCycles > 0) {
                        continue;
                    }
                    finishRun(
                        {...stop, suspendedAdvance: suspended},
                        memoryAddress,
                        memoryLength,
                        traceFilter,
                    );
                    return;
                }
                if (stop.reason !== "instruction-limit" || remainingInstructions === 0) {
                    finishRun(stop, memoryAddress, memoryLength, traceFilter);
                    return;
                }
            } while (performance.now() < deadline
                && (runPacer === null || runPacer.cycleBudget(
                    Number(machineCycle(requireMachine())), performance.now(),
                ) > 0));
            emitAudioFrame(requireMachine());
            const now = performance.now();
            if (runPacer !== null && now - lastRunProgressTime >= RUN_PROGRESS_INTERVAL_MS) {
                sendToHost({
                    type: "event",
                    event: "progress",
                    snapshot: snapshotWithWatches(requireMachine(), {
                        memoryAddress, memoryLength, traceFilter,
                    }),
                });
                lastRunProgressTime = now;
            }
            scheduleRunSlice(
                generation,
                memoryAddress,
                memoryLength,
                traceFilter,
            );
        } catch (error) {
            running = false;
            runPacer = null;
            pauseRequested = false;
            runTargetAddress = null;
            stepOverPending = false;
            stepOutState = null;
            const detail = error instanceof Error ? error.stack ?? error.message : String(error);
            sendToHost({type: "event", event: "error", error: detail});
        }
    }, runPacer === null ? 0 : runPacer.delay(
        Number(machineCycle(requireMachine())), performance.now(),
    ));
}

function startRun(
    message,
    {
        targetAddress = null,
        pendingStepOver = false,
        initialStepOutState = null,
    } = {},
) {
    const current = requireMachine();
    if (message.realtime !== undefined && typeof message.realtime !== "boolean") {
        throw new TypeError("Realtime pacing must be a boolean");
    }
    if (message.realtime && (current.kind !== "jr800"
        || targetAddress !== null || pendingStepOver || initialStepOutState !== null)) {
        throw new Error("Realtime pacing requires a plain JR-800 run");
    }
    const instructionLimit = message.instructionLimit ?? 1_000_000;
    if (!Number.isSafeInteger(instructionLimit) || instructionLimit < 1) {
        throw new RangeError("Instruction limit must be a positive safe integer");
    }
    const requestedSuspendedCycleLimit = message.suspendedCycleLimit
        ?? DEFAULT_SUSPENDED_CYCLE_LIMIT;
    if (!Number.isInteger(requestedSuspendedCycleLimit)
        || requestedSuspendedCycleLimit < 1
        || requestedSuspendedCycleLimit > 0xffff_ffff) {
        throw new RangeError(
            "Suspended cycle limit must be a uint32 value",
        );
    }
    const view = normalizeReadableMachineView(current, message.view);
    if (current.kind === "jr800") {
        current.clearKeyboardActivity();
        if (audioEnabled && audioFrameStartCycle === null) {
            synchronizeAudioCollector(current);
        }
    }
    const cycle = Number(machineCycle(current));
    runPacer = message.realtime
        ? (pacedRunEnd?.machine === current && pacedRunEnd.cycle === cycle
            ? runPacer : new RunPacer(cycle, performance.now()))
        : null;
    pacedRunEnd = null;
    lastRunProgressTime = performance.now();
    remainingInstructions = instructionLimit;
    executedInstructions = 0;
    remainingSuspendedCycles = requestedSuspendedCycleLimit;
    suspendedCyclesElapsed = 0;
    runTargetAddress = targetAddress;
    stepOverPending = pendingStepOver;
    stepOutState = initialStepOutState;
    pauseRequested = false;
    running = true;
    ++runGeneration;
    scheduleRunSlice(
        runGeneration,
        view.memoryAddress,
        view.memoryLength,
        view.traceFilter,
    );
    return {running: true};
}

async function dispatch(message) {
    const id = message?.id;
    try {
        switch (message?.command) {
        case "initialize": {
            requireIdle();
            const candidate = await WasmMachine.create(message.moduleUrl);
            let abiVersion;
            try {
                abiVersion = candidate.state().abiVersion;
            } catch (error) {
                candidate.destroy();
                throw error;
            }
            const previous = machine;
            machine = candidate;
            machineModuleUrl = message.moduleUrl;
            expressionWatches.clear();
            symbolWatches.clear();
            clearKeyboardHoldState();
            resetAudioCollector();
            if (previous) {
                previous.destroy();
            }
            response(id, {abiVersion});
            break;
        }
        case "load": {
            requireIdle();
            let current = machine;
            let created = false;
            if (!current) {
                if (typeof machineModuleUrl !== "string"
                    || machineModuleUrl.length === 0) {
                    throw new Error("Worker has not been initialized");
                }
                current = await WasmMachine.create(machineModuleUrl);
                created = true;
            }
            let snapshot;
            try {
                snapshot = current.load(message.application, {
                    debugInfo: message.debugInfo,
                    initialStackPointer: message.stackPointer ?? 0x01ff,
                    view: message.view,
                });
            } catch (error) {
                if (created) {
                    current.destroy();
                }
                throw error;
            }
            if (created) {
                machine = current;
            }
            expressionWatches.clear();
            symbolWatches.clear();
            clearKeyboardHoldState();
            resetAudioCollector();
            response(id, {
                ...snapshot,
                expressionWatches: [],
                symbolWatches: [],
            });
            break;
        }
        case "load-jr800":
        case "load-jr800-raw": {
            requireIdle();
            const moduleUrl = message.moduleUrl ?? machineModuleUrl;
            if (typeof moduleUrl !== "string" || moduleUrl.length === 0) {
                throw new TypeError("WASM module URL is required");
            }
            const candidate = await WasmMachine.createJr800(
                moduleUrl,
                message.configuration,
            );
            let snapshot;
            try {
                snapshot = message.command === "load-jr800"
                    ? candidate.loadJr8rom(message.romContainer, {
                        view: message.view,
                    })
                    : candidate.loadLogicalRom(message.logicalRom, {
                        view: message.view,
                    });
            } catch (error) {
                candidate.destroy();
                throw error;
            }
            const previous = machine;
            machine = candidate;
            machineModuleUrl = moduleUrl;
            expressionWatches.clear();
            symbolWatches.clear();
            clearKeyboardHoldState();
            resetAudioCollector();
            if (previous) {
                previous.destroy();
            }
            response(id, {
                ...snapshot,
                expressionWatches: [],
                symbolWatches: [],
            });
            break;
        }
        case "load-program":
        case "load-native-program-wav": {
            requireIdle();
            const current = requireMachine();
            const view = normalizeReadableMachineView(current, message.view);
            if (message.command === "load-program") {
                current.loadProgram(message.application, {view});
            } else {
                current.loadNativeProgramWav(message.wav, {view});
            }
            clearKeyboardHoldState(current, true);
            const snapshot = snapshotWithWatches(current, view);
            expressionWatches.clear();
            symbolWatches.clear();
            response(id, {
                ...snapshot,
                expressionWatches: [],
                symbolWatches: [],
            });
            break;
        }
        case "reset": {
            requireIdle();
            const current = requireMachine();
            const view = normalizeReadableMachineView(current, message.view);
            current.reset();
            clearKeyboardHoldState(current, true);
            resetAudioCollector();
            if (audioEnabled) {
                synchronizeAudioCollector(current);
            }
            sendToHost({type: "event", event: "audio-reset"});
            response(id, snapshotWithWatches(current, view));
            break;
        }
        case "step": {
            requireIdle();
            const current = requireMachine();
            if (audioEnabled && audioFrameStartCycle === null) {
                synchronizeAudioCollector(current);
            }
            const view = normalizeReadableMachineView(current, message.view);
            const stop = current.step();
            applyDeferredKeyboardReleases(current);
            collectAudioTransitions(current);
            emitAudioFrame(current);
            response(id, {
                stop,
                snapshot: snapshotWithWatches(current, {
                    ...view,
                    focusAddress: stopFocusAddress(stop, current.state()),
                }),
            });
            break;
        }
        case "set-audio-enabled": {
            const current = requireMachine();
            const enabled = Boolean(message.enabled);
            if (enabled !== audioEnabled || audioFrameStartCycle === null) {
                audioEnabled = enabled;
                resetAudioCollector();
                if (audioEnabled) synchronizeAudioCollector(current);
            }
            response(id, {enabled: audioEnabled});
            break;
        }
        case "run": {
            requireIdle();
            response(id, startRun(message));
            break;
        }
        case "run-to": {
            requireIdle();
            response(id, startRun(message, {
                targetAddress: parseAddress(message.address),
            }));
            break;
        }
        case "run-to-source": {
            requireIdle();
            const current = requireMachine();
            response(id, startRun(message, {
                targetAddress: current.sourceAddress(
                    message.sourcePath,
                    message.line,
                ),
            }));
            break;
        }
        case "run-to-symbol": {
            requireIdle();
            const current = requireMachine();
            response(id, startRun(message, {
                targetAddress: current.symbolAddress(message.symbolName),
            }));
            break;
        }
        case "step-over": {
            requireIdle();
            response(id, startRun(message, {pendingStepOver: true}));
            break;
        }
        case "step-out": {
            requireIdle();
            response(id, startRun(message, {
                initialStepOutState: message.stepOutState ?? {
                    continued: false,
                    nestingDepth: 0,
                },
            }));
            break;
        }
        case "advance-suspended-cycles": {
            requireIdle();
            const current = requireMachine();
            const view = normalizeReadableMachineView(current, message.view);
            if (!Number.isInteger(message.cycleLimit)
                || message.cycleLimit < 1
                || message.cycleLimit > SUSPENDED_SLICE_CYCLES) {
                throw new RangeError(
                    `Worker cycle limit must be between 1 and ${SUSPENDED_SLICE_CYCLES}`,
                );
            }
            const advance = current.advanceSuspendedCycles(message.cycleLimit);
            applyDeferredKeyboardReleases(current);
            response(id, {
                advance,
                snapshot: snapshotWithWatches(current, view),
            });
            break;
        }
        case "advance-calendar-oscillator": {
            requireIdle();
            const current = requireMachine();
            const view = normalizeReadableMachineView(current, message.view);
            current.advanceCalendarOscillatorTicks(message.ticks);
            response(id, {
                ticks: message.ticks,
                snapshot: snapshotWithWatches(current, view),
            });
            break;
        }
        case "adjust-calendar-seconds": {
            requireIdle();
            const current = requireMachine();
            const view = normalizeReadableMachineView(current, message.view);
            current.adjustCalendarSeconds();
            response(id, snapshotWithWatches(current, view));
            break;
        }
        case "set-keyboard-response": {
            const current = requireMachine();
            const address = parseAddress(message.address);
            const known = message.known ?? true;
            const appliedDuringRun = running;
            const totalInstructionsExecuted = appliedDuringRun
                ? executedInstructions
                : null;
            current.setKeyboardBusResponse(
                address,
                message.value,
                known,
            );
            response(id, {
                address,
                value: message.value,
                known,
                appliedDuringRun,
                totalInstructionsExecuted,
            });
            break;
        }
        case "set-keyboard-key-state": {
            const current = requireMachine();
            const minimumHoldCycles = message.minimumHoldCycles ?? 0;
            if (!Number.isInteger(minimumHoldCycles)
                || minimumHoldCycles < 0
                || minimumHoldCycles > 0xffff_ffff) {
                throw new RangeError("Minimum key hold must be a uint32 cycle count");
            }
            const appliedDuringRun = running;
            const totalInstructionsExecuted = appliedDuringRun
                ? executedInstructions
                : null;
            const result = {
                key: message.key,
                pressed: message.pressed,
                appliedDuringRun,
                totalInstructionsExecuted,
            };
            if (message.pressed === true) {
                current.setKeyboardKeyState(message.key, true);
                deferredKeyboardReleases.delete(message.key);
                minimumKeyboardReleaseCycles.set(
                    message.key,
                    machineCycle(current) + BigInt(minimumHoldCycles),
                );
            } else if (message.pressed === false) {
                const releaseCycle = minimumKeyboardReleaseCycles.get(
                    message.key,
                );
                if (releaseCycle !== undefined
                    && machineCycle(current) < releaseCycle) {
                    deferredKeyboardReleases.add(message.key);
                    const pending = pendingKeyboardReleaseResponses.get(
                        message.key,
                    ) ?? [];
                    pending.push({id, result});
                    pendingKeyboardReleaseResponses.set(message.key, pending);
                    break;
                } else {
                    current.setKeyboardKeyState(message.key, false);
                    deferredKeyboardReleases.delete(message.key);
                    minimumKeyboardReleaseCycles.delete(message.key);
                }
            } else {
                current.setKeyboardKeyState(message.key, message.pressed);
            }
            response(id, result);
            break;
        }
        case "pause":
            pauseRequested = running;
            response(id, {pausePending: running});
            break;
        case "set-execution-breakpoint": {
            const current = requireMachine();
            current.setExecutionBreakpoint(
                parseAddress(message.address),
                message.enabled,
                message.enabled ? message.condition ?? "" : "",
            );
            response(id, {
                address: message.address,
                enabled: message.enabled,
                condition: message.enabled ? message.condition ?? "" : "",
            });
            break;
        }
        case "set-memory-watchpoint": {
            const current = requireMachine();
            current.setMemoryWatchpoint(
                parseAddress(message.address),
                message.mode,
                message.enabled,
            );
            response(id, {
                address: message.address,
                mode: message.mode,
                enabled: Boolean(message.enabled),
            });
            break;
        }
        case "set-expression-watch": {
            requireIdle();
            const current = requireMachine();
            const view = normalizeReadableMachineView(current, message.view);
            current.setExpressionWatch(message.watchId, message.expression);
            expressionWatches.set(message.watchId, message.expression);
            response(id, snapshotWithWatches(current, view));
            break;
        }
        case "clear-expression-watch": {
            requireIdle();
            const current = requireMachine();
            const view = normalizeReadableMachineView(current, message.view);
            current.clearExpressionWatch(message.watchId);
            expressionWatches.delete(message.watchId);
            response(id, snapshotWithWatches(current, view));
            break;
        }
        case "set-symbol-watch": {
            requireIdle();
            const current = requireMachine();
            const view = normalizeReadableMachineView(current, message.view);
            current.setSymbolWatch(message.watchId, message.name);
            symbolWatches.set(message.watchId, message.name);
            response(id, snapshotWithWatches(current, view));
            break;
        }
        case "clear-symbol-watch": {
            requireIdle();
            const current = requireMachine();
            const view = normalizeReadableMachineView(current, message.view);
            current.clearSymbolWatch(message.watchId);
            symbolWatches.delete(message.watchId);
            response(id, snapshotWithWatches(current, view));
            break;
        }
        case "snapshot":
            response(
                id,
                snapshotWithWatches(requireMachine(), message.view),
            );
            break;
        case "clear-history":
            requireMachine().clearHistory();
            response(id, {cleared: true});
            break;
        case "power-off": {
            runPacer = null;
            pacedRunEnd = null;
            const current = requireMachine();
            running = false;
            pauseRequested = false;
            ++runGeneration;
            remainingInstructions = 0;
            executedInstructions = 0;
            remainingSuspendedCycles = DEFAULT_SUSPENDED_CYCLE_LIMIT;
            suspendedCyclesElapsed = 0;
            runTargetAddress = null;
            stepOverPending = false;
            stepOutState = null;
            expressionWatches.clear();
            symbolWatches.clear();
            clearKeyboardHoldState();
            resetAudioCollector();
            sendToHost({type: "event", event: "audio-reset"});
            current.destroy();
            machine = undefined;
            response(id, {poweredOff: true});
            break;
        }
        case "dispose":
            requireIdle();
            runPacer = null;
            pacedRunEnd = null;
            if (machine) {
                machine.destroy();
            }
            machine = undefined;
            machineModuleUrl = undefined;
            runTargetAddress = null;
            stepOverPending = false;
            stepOutState = null;
            expressionWatches.clear();
            symbolWatches.clear();
            clearKeyboardHoldState();
            resetAudioCollector();
            response(id, {disposed: true});
            break;
        default:
            throw new Error("Unknown worker command");
        }
    } catch (error) {
        failure(id, error);
    }
}

subscribe((message) => {
    void dispatch(message);
});
