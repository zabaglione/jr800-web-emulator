// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {createHash} from "node:crypto";
import {readFile} from "node:fs/promises";
import {resolve} from "node:path";
import {pathToFileURL} from "node:url";
import {Worker} from "node:worker_threads";

const [
    workerPath,
    modulePath,
    applicationPath,
    debugPath,
    expectedPath,
    sleepApplicationPath,
    jr800ExpectedPath,
] = process.argv.slice(2);
if (
    !workerPath
    || !modulePath
    || !applicationPath
    || !debugPath
    || !expectedPath
    || !sleepApplicationPath
    || !jr800ExpectedPath
) {
    throw new Error(
        "Usage: node wasm_worker_vertical_slice.mjs "
        + "<worker> <module> <app.hex> <debug.hex> <expected.json> <slp.hex> "
        + "<jr800-expected.json>",
    );
}

const decodeHex = async (path) => Uint8Array.from(
    Buffer.from((await readFile(path, "utf8")).replace(/\s/g, ""), "hex"),
);

const expected = JSON.parse(await readFile(expectedPath, "utf8"));
const jr800Expected = JSON.parse(await readFile(jr800ExpectedPath, "utf8"));
const application = await decodeHex(applicationPath);
const debugInfo = await decodeHex(debugPath);
const sleepApplication = await decodeHex(sleepApplicationPath);
const workerUrl = pathToFileURL(resolve(workerPath));
const moduleUrl = pathToFileURL(resolve(modulePath)).href;
const worker = new Worker(workerUrl, {type: "module"});

let nextId = 1;
const pending = new Map();
const queuedEvents = [];
const eventWaiters = [];

worker.on("message", (message) => {
    if (message?.type === "response") {
        const request = pending.get(message.id);
        if (!request) {
            return;
        }
        pending.delete(message.id);
        clearTimeout(request.timeout);
        if (message.ok) {
            request.resolve(message.result);
        } else {
            request.reject(new Error(message.error));
        }
        return;
    }
    if (message?.type === "event") {
        const waiterIndex = eventWaiters.findIndex((waiter) => waiter.event === message.event);
        if (waiterIndex >= 0) {
            const [waiter] = eventWaiters.splice(waiterIndex, 1);
            clearTimeout(waiter.timeout);
            waiter.resolve(message);
        } else {
            queuedEvents.push(message);
        }
    }
});

function request(command, fields = {}, transfer = []) {
    const id = nextId++;
    return new Promise((resolvePromise, rejectPromise) => {
        const timeout = setTimeout(() => {
            pending.delete(id);
            rejectPromise(new Error(`Worker command timed out: ${command}`));
        }, 15_000);
        pending.set(id, {resolve: resolvePromise, reject: rejectPromise, timeout});
        try {
            worker.postMessage({id, command, ...fields}, transfer);
        } catch (error) {
            clearTimeout(timeout);
            pending.delete(id);
            rejectPromise(error);
        }
    });
}

function bigEndianUint32(bytes, offset) {
    return ((bytes[offset] << 24) >>> 0)
        | (bytes[offset + 1] << 16)
        | (bytes[offset + 2] << 8)
        | bytes[offset + 3];
}

function makeLogicalRomBytes(program) {
    const rom = new Uint8Array(32 * 1024);
    rom.fill(0x01);
    rom.set(program);
    rom[rom.byteLength - 2] = 0x80;
    rom[rom.byteLength - 1] = 0x00;
    return rom;
}

function makeJr8romFromSegments(segments) {
    const ordered = [...segments].sort((left, right) => left[0] - right[0]);
    const count = Buffer.alloc(4);
    count.writeUInt32BE(ordered.length);
    const records = ordered.map(([address, data]) => {
        const record = Buffer.alloc(6 + data.byteLength);
        record.writeUInt16BE(address, 0);
        record.writeUInt32BE(data.byteLength, 2);
        Buffer.from(data.buffer, data.byteOffset, data.byteLength).copy(record, 6);
        return record;
    });
    const integrity = createHash("sha256")
        .update(Buffer.from("JR8ROM-INTEGRITY-V1\0", "ascii"))
        .update(count)
        .update(Buffer.concat(records))
        .digest();
    const header = Buffer.alloc(52);
    Buffer.from("JR8ROM\0\0", "ascii").copy(header);
    header.writeUInt16BE(1, 8);
    header.writeUInt16BE(0, 10);
    header.writeUInt32BE(0, 12);
    integrity.copy(header, 16);
    count.copy(header, 48);
    return Uint8Array.from(Buffer.concat([header, ...records]));
}

function makeJr8rom(program) {
    return makeJr8romFromSegments([[0x8000, makeLogicalRomBytes(program)]]);
}

function makeJr8app(
    address,
    entryPoint,
    program,
    targetProfile = "hd6301v1",
) {
    const target = Buffer.from(targetProfile, "ascii");
    const text = Buffer.alloc(4 + target.byteLength);
    text.writeUInt32BE(target.byteLength);
    target.copy(text, 4);

    const integrityPrefix = Buffer.from("JR8APP-INTEGRITY-V1\0", "ascii");
    const integrityRecord = Buffer.alloc(1 + 2 + 4 + program.byteLength);
    integrityRecord.writeUInt8(1, 0);
    integrityRecord.writeUInt16BE(address, 1);
    integrityRecord.writeUInt32BE(program.byteLength, 3);
    Buffer.from(
        program.buffer,
        program.byteOffset,
        program.byteLength,
    ).copy(integrityRecord, 7);
    const integrityHeader = Buffer.alloc(2 + 4);
    integrityHeader.writeUInt16BE(entryPoint, 0);
    integrityHeader.writeUInt32BE(1, 2);
    const integrity = createHash("sha256")
        .update(integrityPrefix)
        .update(text)
        .update(integrityHeader)
        .update(integrityRecord)
        .digest();

    const header = Buffer.alloc(8 + 2 + 2 + 4);
    Buffer.from("JR8APP\0\0", "ascii").copy(header);
    header.writeUInt16BE(1, 8);
    header.writeUInt16BE(0, 10);
    header.writeUInt32BE(0, 12);
    const applicationHeader = Buffer.alloc(2 + 2 + 32 + 4);
    applicationHeader.writeUInt16BE(entryPoint, 0);
    applicationHeader.writeUInt16BE(0, 2);
    integrity.copy(applicationHeader, 4);
    applicationHeader.writeUInt32BE(1, 36);
    const segment = Buffer.alloc(1 + 1 + 2 + 4 + 4 + program.byteLength);
    segment.writeUInt8(1, 0);
    segment.writeUInt8(0, 1);
    segment.writeUInt16BE(address, 2);
    segment.writeUInt32BE(program.byteLength, 4);
    segment.writeUInt32BE(program.byteLength, 8);
    Buffer.from(
        program.buffer,
        program.byteOffset,
        program.byteLength,
    ).copy(segment, 12);
    return Uint8Array.from(Buffer.concat([
        header,
        text,
        applicationHeader,
        segment,
    ]));
}

function makeNativeProgramWav(address, entryPoint, program) {
    const sampleRate = 48_000;
    const amplitude = 12_000;
    const samples = Array(sampleRate / 10).fill(0);
    const appendCycle = (longPeriod) => {
        const halfPeriod = longPeriod ? 21 : 11;
        for (let index = 0; index < halfPeriod; index += 1) {
            samples.push(amplitude);
        }
        for (let index = 0; index < halfPeriod; index += 1) {
            samples.push(-amplitude);
        }
    };
    const appendCycles = (longPeriod, count) => {
        for (let index = 0; index < count; index += 1) {
            appendCycle(longPeriod);
        }
    };
    const appendByte = (value) => {
        for (let bit = 7; bit >= 0; bit -= 1) {
            appendCycle(((value >> bit) & 1) !== 0);
        }
        appendCycle(true);
    };
    const appendBlock = (bytes, longSyncCycles, shortSyncCycles) => {
        appendCycles(false, 4_000);
        appendCycles(true, longSyncCycles);
        appendCycles(false, shortSyncCycles);
        appendCycles(true, 2);
        for (const byte of bytes) {
            appendByte(byte);
        }
    };
    const additiveSum = (bytes) => bytes.reduce(
        (sum, byte) => (sum + byte) & 0xffff,
        0,
    );

    const header = Buffer.alloc(34);
    header.writeUInt8(0x01, 0);
    Buffer.from("WORKERWAV", "ascii").copy(header, 1);
    header.writeUInt16LE(program.byteLength, 17);
    header.writeUInt16LE(address, 19);
    header.writeUInt16LE(entryPoint, 21);
    header.writeUInt16BE(additiveSum(header.subarray(0, 32)), 32);
    const data = Buffer.alloc(program.byteLength + 2);
    Buffer.from(
        program.buffer,
        program.byteOffset,
        program.byteLength,
    ).copy(data);
    data.writeUInt16BE(additiveSum(program), program.byteLength);

    appendBlock(header, 40, 40);
    for (let index = 0; index < sampleRate / 500; index += 1) {
        samples.push(0);
    }
    appendBlock(data, 20, 20);
    for (let index = 0; index < sampleRate / 10; index += 1) {
        samples.push(0);
    }

    const pcm = Buffer.alloc(samples.length * 2);
    samples.forEach((sample, index) => pcm.writeInt16LE(sample, index * 2));
    const wav = Buffer.alloc(44 + pcm.byteLength);
    Buffer.from("RIFF", "ascii").copy(wav, 0);
    wav.writeUInt32LE(36 + pcm.byteLength, 4);
    Buffer.from("WAVEfmt ", "ascii").copy(wav, 8);
    wav.writeUInt32LE(16, 16);
    wav.writeUInt16LE(1, 20);
    wav.writeUInt16LE(1, 22);
    wav.writeUInt32LE(sampleRate, 24);
    wav.writeUInt32LE(sampleRate * 2, 28);
    wav.writeUInt16LE(2, 32);
    wav.writeUInt16LE(16, 34);
    Buffer.from("data", "ascii").copy(wav, 36);
    wav.writeUInt32LE(pcm.byteLength, 40);
    pcm.copy(wav, 44);
    return Uint8Array.from(wav);
}

function nextEvent(event) {
    const queuedIndex = queuedEvents.findIndex((message) => message.event === event);
    if (queuedIndex >= 0) {
        return Promise.resolve(queuedEvents.splice(queuedIndex, 1)[0]);
    }
    return new Promise((resolvePromise, rejectPromise) => {
        const waiter = {event, resolve: resolvePromise, reject: rejectPromise};
        waiter.timeout = setTimeout(() => {
            const index = eventWaiters.indexOf(waiter);
            if (index >= 0) {
                eventWaiters.splice(index, 1);
            }
            rejectPromise(new Error(`Worker event timed out: ${event}`));
        }, 15_000);
        eventWaiters.push(waiter);
    });
}

function paritySnapshot(message) {
    const {stop, snapshot} = message;
    return {
        stop: {
            reason: stop.reason,
            triggerAddress: stop.triggerAddress,
            triggerAccess: stop.triggerAccess,
            instructionsExecuted: stop.instructionsExecuted,
            pcBefore: stop.pcBefore,
            pcAfter: stop.pcAfter,
            fault: stop.fault,
        },
        state: {
            profile: snapshot.state.profile,
            pc: snapshot.state.pc,
            sp: snapshot.state.sp,
            x: snapshot.state.x,
            a: snapshot.state.a,
            b: snapshot.state.b,
            conditionCode: snapshot.state.conditionCode,
            executionState: snapshot.state.executionState,
            cycleCount: snapshot.state.cycleCount,
        },
        memory: snapshot.memory.bytes,
        historyCount: snapshot.history.length,
        accessCount: snapshot.accesses.length,
        source: {
            path: snapshot.source?.path,
            line: snapshot.source?.line,
            column: snapshot.source?.column,
        },
        disassembly: snapshot.disassembly.text,
    };
}

function jr800SnapshotParity(snapshot) {
    return {
        state: {
            abiVersion: snapshot.state.abiVersion,
            profile: snapshot.state.profile,
            pc: snapshot.state.pc,
            sp: snapshot.state.sp,
            x: snapshot.state.x,
            a: snapshot.state.a,
            b: snapshot.state.b,
            conditionCode: snapshot.state.conditionCode,
            executionState: snapshot.state.executionState,
            cycleCount: snapshot.state.cycleCount,
            registerKnownMask: snapshot.state.registerKnownMask,
            conditionCodeKnownMask: snapshot.state.conditionCodeKnownMask,
            calendarAlarmTerminal: snapshot.state.calendarAlarmTerminal,
            port2TimerOutput: snapshot.state.port2TimerOutput,
            lcdSubstitutedDataReadCount:
                snapshot.state.lcdSubstitutedDataReadCount,
        },
        memory: snapshot.memory.bytes,
        historyCount: snapshot.history.length,
        accessCount: snapshot.accesses.length,
        keyboardActivity: snapshot.keyboardActivity,
        disassembly: {
            address: snapshot.disassembly.address,
            bytes: snapshot.disassembly.bytes,
            length: snapshot.disassembly.length,
            supported: snapshot.disassembly.supported,
            text: snapshot.disassembly.text,
        },
    };
}

function jr800StopParity(stop) {
    return {
        reason: stop.reason,
        fault: stop.fault,
        instructionsExecuted: stop.instructionsExecuted,
        pcBefore: stop.pcBefore,
        pcAfter: stop.pcAfter,
        bytes: stop.bytes,
        instructionLength: stop.instructionLength,
        bytesFetched: stop.bytesFetched,
        cycles: stop.cycles,
        busFault: stop.busFault,
        stepKind: stop.stepKind,
        interruptSource: stop.interruptSource,
    };
}

try {
    const initialized = await request("initialize", {moduleUrl});
    assert.equal(initialized.abiVersion, 36);
    const initialApplication = application.slice();
    const initialDebugInfo = debugInfo.slice();
    const syntheticLoaded = await request("load", {
        application: initialApplication,
        debugInfo: initialDebugInfo,
        view: {memoryAddress: 0, memoryLength: 2},
    }, [initialApplication.buffer, initialDebugInfo.buffer]);
    assert.equal(syntheticLoaded.state.port2TimerOutput, "unavailable");
    assert.equal(syntheticLoaded.state.lcdSubstitutedDataReadCount, null);

    const symbolWatch = await request("set-symbol-watch", {
        watchId: 5,
        name: "loop",
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.deepEqual(symbolWatch.symbolWatches, [{
        id: 5,
        name: "loop",
        value: 0x020a,
        binding: "local",
        kind: "address",
        size: 0,
        sourceFileIndex: 0,
    }]);
    await assert.rejects(
        request("set-symbol-watch", {
            watchId: 5,
            name: "missing",
            view: {memoryAddress: 0, memoryLength: 2},
        }),
        /set-symbol-watch failed: not-found/,
    );
    const preservedSymbolWatch = await request("snapshot", {
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.deepEqual(preservedSymbolWatch.symbolWatches, symbolWatch.symbolWatches);
    await request("set-symbol-watch", {
        watchId: 6,
        name: "loop",
        view: {memoryAddress: 0, memoryLength: 2},
    });
    const oneSymbolWatch = await request("clear-symbol-watch", {
        watchId: 6,
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(oneSymbolWatch.symbolWatches.length, 1);
    await assert.rejects(
        request("clear-symbol-watch", {
            watchId: 6,
            view: {memoryAddress: 0, memoryLength: 2},
        }),
        /clear-symbol-watch failed: not-found/,
    );
    await assert.rejects(
        request("set-symbol-watch", {
            watchId: 7,
            name: "loop",
            view: {memoryAddress: 0, memoryLength: Number.MAX_SAFE_INTEGER},
        }),
        /Memory range is out of bounds/,
    );

    const resolvedSymbolExpression = await request("set-expression-watch", {
        watchId: 11,
        expression: "symbol(\"loop\")",
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(resolvedSymbolExpression.expressionWatches[0].value, 0x020a);
    assert.equal(resolvedSymbolExpression.expressionWatches[0].error, "none");
    await request("clear-expression-watch", {
        watchId: 11,
        view: {memoryAddress: 0, memoryLength: 2},
    });
    const missingSymbolExpression = await request("set-expression-watch", {
        watchId: 11,
        expression: "symbol(\"missing\")",
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(
        missingSymbolExpression.expressionWatches[0].error,
        "symbol-not-found",
    );
    await request("clear-expression-watch", {
        watchId: 11,
        view: {memoryAddress: 0, memoryLength: 2},
    });

    const registerWatch = await request("set-expression-watch", {
        watchId: 7,
        expression: "PC",
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.deepEqual(registerWatch.expressionWatches, [{
        id: 7,
        expression: "PC",
        value: 0x0200,
        error: "none",
        busFault: "none",
        faultAddress: 0,
        stateFault: "none",
    }]);
    const memoryWatch = await request("set-expression-watch", {
        watchId: 8,
        expression: "mem8[$0001]",
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(memoryWatch.expressionWatches.length, 2);
    assert.equal(memoryWatch.expressionWatches[1].value, 0);

    const firstStep = await request("step", {view: {memoryAddress: 0, memoryLength: 2}});
    assert.equal(firstStep.snapshot.state.pc, 0x0202);
    assert.equal(firstStep.snapshot.expressionWatches[0].value, 0x0202);
    assert.equal(firstStep.stop.continuationAddress, null);
    assert.ok(firstStep.snapshot.accesses.every((entry) => entry.valueKnown));
    assert.ok(
        firstStep.snapshot.accesses.every((entry) => entry.previousValueKnown),
    );
    await assert.rejects(
        request("set-expression-watch", {
            watchId: 7,
            expression: "PC ==",
            view: {memoryAddress: 0, memoryLength: 2},
        }),
        /set-expression-watch failed: invalid-expression/,
    );
    const preservedWatch = await request("snapshot", {
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(preservedWatch.expressionWatches[0].expression, "PC");
    assert.equal(preservedWatch.expressionWatches[0].value, 0x0202);
    const evaluationErrorWatch = await request("set-expression-watch", {
        watchId: 9,
        expression: "mem8[$10000]",
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(
        evaluationErrorWatch.expressionWatches[2].error,
        "address-out-of-range",
    );
    await request("clear-expression-watch", {
        watchId: 9,
        view: {memoryAddress: 0, memoryLength: 2},
    });
    const wideValueWatch = await request("set-expression-watch", {
        watchId: 9,
        expression: "~0",
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(
        wideValueWatch.expressionWatches[2].value,
        "18446744073709551615",
    );
    await request("clear-expression-watch", {
        watchId: 9,
        view: {memoryAddress: 0, memoryLength: 2},
    });
    const oneWatch = await request("clear-expression-watch", {
        watchId: 8,
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(oneWatch.expressionWatches.length, 1);
    await assert.rejects(
        request("clear-expression-watch", {
            watchId: 8,
            view: {memoryAddress: 0, memoryLength: 2},
        }),
        /clear-expression-watch failed: not-found/,
    );
    await assert.rejects(
        request("set-expression-watch", {
            watchId: 10,
            expression: "A",
            view: {memoryAddress: 0, memoryLength: Number.MAX_SAFE_INTEGER},
        }),
        /Memory range is out of bounds/,
    );
    const beforeRejectedLoad = await request("snapshot", {
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(beforeRejectedLoad.expressionWatches.length, 1);
    assert.equal(beforeRejectedLoad.symbolWatches.length, 1);

    const filteredFetch = await request("snapshot", {
        view: {
            memoryAddress: 0,
            memoryLength: 2,
            traceFilter: {
                firstAddress: 0x0201,
                lastAddress: 0x0201,
                kindMask: 0x01,
            },
        },
    });
    assert.equal(filteredFetch.accesses.length, 1);
    assert.equal(filteredFetch.accesses[0].address, 0x0201);
    assert.equal(filteredFetch.accesses[0].kind, "instruction-fetch");
    const excludedFetch = await request("snapshot", {
        view: {
            memoryAddress: 0,
            memoryLength: 2,
            traceFilter: {
                firstAddress: 0x0201,
                lastAddress: 0x0201,
                kindMask: 0x06,
            },
        },
    });
    assert.deepEqual(excludedFetch.accesses, []);
    await assert.rejects(
        request("snapshot", {
            view: {
                memoryAddress: 0,
                memoryLength: 2,
                traceFilter: {
                    firstAddress: 0x0201,
                    lastAddress: 0x0200,
                    kindMask: 0x07,
                },
            },
        }),
        /Trace address range is reversed/,
    );
    await assert.rejects(
        request("snapshot", {
            view: {
                memoryAddress: 0,
                memoryLength: 2,
                traceFilter: {
                    firstAddress: 0,
                    lastAddress: 0xffff,
                    kindMask: 0x1_0000_0007,
                },
            },
        }),
        /Trace kind mask is invalid/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedLoad,
        "access filtering changed the captured trace or machine state",
    );

    await assert.rejects(
        request("initialize", {
            moduleUrl: new URL("missing-jr800-module.mjs", workerUrl).href,
        }),
        /missing-jr800-module/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedLoad,
        "failed reinitialization changed the active machine",
    );

    const mismatchedDebugInfo = debugInfo.slice();
    const targetLength = bigEndianUint32(mismatchedDebugInfo, 16);
    const digestOffset = 20 + targetLength;
    assert.ok(digestOffset < mismatchedDebugInfo.byteLength);
    mismatchedDebugInfo[digestOffset] ^= 0xff;
    const rejectedApplication = application.slice();
    await assert.rejects(
        request("load", {
            application: rejectedApplication,
            debugInfo: mismatchedDebugInfo,
            view: {memoryAddress: 0, memoryLength: 2},
        }, [rejectedApplication.buffer, mismatchedDebugInfo.buffer]),
        /load-debug-info failed: integrity-mismatch/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedLoad,
        "rejected debug info changed the active machine",
    );

    const invalidViewApplication = application.slice();
    const invalidViewDebugInfo = debugInfo.slice();
    await assert.rejects(
        request("load", {
            application: invalidViewApplication,
            debugInfo: invalidViewDebugInfo,
            view: {memoryAddress: 0, memoryLength: Number.MAX_SAFE_INTEGER},
        }, [invalidViewApplication.buffer, invalidViewDebugInfo.buffer]),
        /Memory range is out of bounds/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedLoad,
        "invalid snapshot options changed the active machine",
    );

    for (const command of ["reset", "step", "run"]) {
        await assert.rejects(
            request(command, {
                instructionLimit: 10,
                view: {memoryAddress: 0, memoryLength: Number.MAX_SAFE_INTEGER},
            }),
            /Memory range is out of bounds/,
        );
        assert.deepEqual(
            await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
            beforeRejectedLoad,
            `${command} changed the machine before validating its view`,
        );
    }

    await assert.rejects(
        request("run-to", {
            address: 0x0207,
            instructionLimit: 10,
            view: {memoryAddress: 0, memoryLength: Number.MAX_SAFE_INTEGER},
        }),
        /Memory range is out of bounds/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedLoad,
        "run-to changed the machine before validating its view",
    );

    await assert.rejects(
        request("run-to-source", {
            sourcePath: "main.s",
            line: 9,
            instructionLimit: 10,
            view: {memoryAddress: 0, memoryLength: Number.MAX_SAFE_INTEGER},
        }),
        /Memory range is out of bounds/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedLoad,
        "run-to-source changed the machine before validating its view",
    );

    await assert.rejects(
        request("run-to", {
            address: 0x1_0000,
            instructionLimit: 10,
        }),
        /Address must be a uint16 value/,
    );

    const wrappedStackApplication = application.slice();
    await assert.rejects(
        request("load", {
            application: wrappedStackApplication,
            stackPointer: 0x1_0000_01ff,
        }, [wrappedStackApplication.buffer]),
        /Initial stack pointer must be a uint16 value/,
    );
    await assert.rejects(
        request("snapshot", {
            view: {
                memoryAddress: 0,
                memoryLength: 2,
                focusAddress: 0x1_0000_0200,
            },
        }),
        /Focus address must be a uint16 value/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedLoad,
        "wrapped uint32 input changed the active machine",
    );

    const oversizedApplication = new Uint8Array(64 * 1024 * 1024 + 1);
    await assert.rejects(
        request("load", {application: oversizedApplication}, [oversizedApplication.buffer]),
        /Binary input exceeds 67108864 bytes/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedLoad,
        "oversized input changed the active machine",
    );

    await assert.rejects(
        request("run-to-source", {
            sourcePath: "main.s",
            line: 0,
            instructionLimit: 100,
        }),
        /Source line must be a nonzero uint32 value/,
    );
    await assert.rejects(
        request("run-to-source", {
            sourcePath: "missing.s",
            line: 9,
            instructionLimit: 100,
        }),
        /source-address failed: not-found/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedLoad,
        "rejected source location changed the active machine",
    );

    await request("reset", {view: {memoryAddress: 0, memoryLength: 2}});
    await request("set-execution-breakpoint", {address: 0x0207, enabled: true});
    const runToSourcePromise = nextEvent("stopped");
    await request("run-to-source", {
        sourcePath: "main.s",
        line: 9,
        instructionLimit: 100,
        view: {
            memoryAddress: 0,
            memoryLength: 2,
            traceFilter: {
                firstAddress: 0,
                lastAddress: 0,
                kindMask: 0x04,
            },
        },
    });
    const runToSource = await runToSourcePromise;
    assert.equal(runToSource.stop.reason, "address-reached");
    assert.equal(runToSource.stop.triggerAddress, 0x0207);
    assert.equal(runToSource.stop.totalInstructionsExecuted, 3);
    assert.equal(runToSource.snapshot.state.pc, 0x0207);
    assert.equal(runToSource.snapshot.accesses.length, 1);
    assert.equal(runToSource.snapshot.accesses[0].kind, "data-write");
    assert.equal(runToSource.snapshot.accesses[0].address, 0);
    await request("set-execution-breakpoint", {address: 0x0207, enabled: false});

    await assert.rejects(
        request("run-to-symbol", {
            symbolName: "missing",
            instructionLimit: 100,
        }),
        /symbol-address failed: not-found/,
    );
    await assert.rejects(
        request("run-to-symbol", {
            symbolName: "",
            instructionLimit: 100,
        }),
        /Symbol name must be nonempty text without NUL/,
    );
    await assert.rejects(
        request("run-to-symbol", {
            symbolName: "lo\0op",
            instructionLimit: 100,
        }),
        /Symbol name must be nonempty text without NUL/,
    );
    await request("reset", {view: {memoryAddress: 0, memoryLength: 2}});
    await request("set-execution-breakpoint", {address: 0x020a, enabled: true});
    const runToSymbolPromise = nextEvent("stopped");
    await request("run-to-symbol", {
        symbolName: "loop",
        instructionLimit: 100,
        view: {memoryAddress: 0, memoryLength: 2},
    });
    const runToSymbol = await runToSymbolPromise;
    assert.equal(runToSymbol.stop.reason, "address-reached");
    assert.equal(runToSymbol.stop.triggerAddress, 0x020a);
    assert.equal(runToSymbol.stop.totalInstructionsExecuted, 4);
    assert.equal(runToSymbol.snapshot.state.pc, 0x020a);
    assert.deepEqual(runToSymbol.snapshot.memory.bytes, [0x42, 0x99]);
    await request("set-execution-breakpoint", {address: 0x020a, enabled: false});

    await request("reset", {view: {memoryAddress: 0, memoryLength: 2}});
    await request("set-execution-breakpoint", {address: 0x0207, enabled: true});
    const runToPromise = nextEvent("stopped");
    await request("run-to", {
        address: 0x0207,
        instructionLimit: 100,
        view: {memoryAddress: 0, memoryLength: 2},
    });
    const runTo = await runToPromise;
    assert.equal(runTo.stop.reason, "address-reached");
    assert.equal(runTo.stop.triggerAddress, 0x0207);
    assert.equal(runTo.stop.totalInstructionsExecuted, 3);
    assert.equal(runTo.snapshot.state.pc, 0x0207);
    assert.deepEqual(runTo.snapshot.memory.bytes, [0x42, 0x00]);
    await request("set-execution-breakpoint", {address: 0x0207, enabled: false});
    await request("reset", {view: {memoryAddress: 0, memoryLength: 2}});
    await request("set-memory-watchpoint", {
        address: 1,
        mode: "write",
        enabled: true,
    });

    const stoppedPromise = nextEvent("stopped");
    const started = await request("run", {
        instructionLimit: 100,
        view: {memoryAddress: 0, memoryLength: 2},
    });
    assert.equal(started.running, true);
    const stopped = await stoppedPromise;
    assert.deepEqual(paritySnapshot(stopped), expected);

    await request("reset", {view: {memoryAddress: 0, memoryLength: 2}});
    await request("set-memory-watchpoint", {
        address: 1,
        mode: "write",
        enabled: false,
    });
    await assert.rejects(
        request("set-memory-watchpoint", {
            address: 1,
            mode: "execute",
            enabled: true,
        }),
        /Watchpoint mode must be read, write, or access/,
    );
    await request("set-execution-breakpoint", {address: 0x0200, enabled: true});
    const breakpointPromise = nextEvent("stopped");
    await request("run", {instructionLimit: 10});
    const breakpoint = await breakpointPromise;
    assert.equal(breakpoint.stop.reason, "execution-breakpoint");
    assert.equal(breakpoint.stop.instructionsExecuted, 0);

    await request("set-execution-breakpoint", {address: 0x0200, enabled: false});
    await request("reset", {view: {memoryAddress: 0, memoryLength: 2}});
    const symbolCondition = "A == $99 && z == 0 && mem8[$0001] == $99 "
        + "&& symbol(\"loop\") == $020A";
    const conditionalSetup = await request("set-execution-breakpoint", {
        address: 0x020a,
        enabled: true,
        condition: symbolCondition,
    });
    assert.equal(conditionalSetup.condition, symbolCondition);
    const conditionalPromise = nextEvent("stopped");
    await request("run", {
        instructionLimit: 100,
        view: {memoryAddress: 0, memoryLength: 2},
    });
    const conditional = await conditionalPromise;
    assert.equal(conditional.stop.reason, "execution-breakpoint");
    assert.equal(conditional.stop.conditionError, "none");
    assert.equal(conditional.stop.totalInstructionsExecuted, 4);
    assert.equal(conditional.snapshot.state.pc, 0x020a);
    assert.deepEqual(conditional.snapshot.memory.bytes, [0x42, 0x99]);

    await assert.rejects(
        request("set-execution-breakpoint", {
            address: 0x020a,
            enabled: true,
            condition: "A ==",
        }),
        /set-conditional-execution-breakpoint failed: invalid-expression/,
    );
    await assert.rejects(
        request("set-execution-breakpoint", {
            address: 0x020a,
            enabled: "true",
        }),
        /Breakpoint enabled state must be boolean/,
    );
    const preservedConditionalPromise = nextEvent("stopped");
    await request("run", {instructionLimit: 1});
    const preservedConditional = await preservedConditionalPromise;
    assert.equal(preservedConditional.stop.reason, "execution-breakpoint");
    assert.equal(preservedConditional.stop.totalInstructionsExecuted, 0);

    await request("set-execution-breakpoint", {
        address: 0x020a,
        enabled: true,
        condition: "mem8[$10000]",
    });
    const conditionErrorPromise = nextEvent("stopped");
    await request("run", {instructionLimit: 1});
    const conditionError = await conditionErrorPromise;
    assert.equal(conditionError.stop.reason, "breakpoint-condition-error");
    assert.equal(conditionError.stop.conditionError, "address-out-of-range");
    assert.equal(conditionError.stop.totalInstructionsExecuted, 0);
    assert.equal(conditionError.snapshot.state.pc, 0x020a);

    await request("set-execution-breakpoint", {address: 0x020a, enabled: false});
    const resetWithWatch = await request("reset");
    assert.equal(resetWithWatch.expressionWatches.length, 1);
    assert.equal(resetWithWatch.expressionWatches[0].value, 0x0200);
    assert.equal(resetWithWatch.symbolWatches.length, 1);
    assert.equal(resetWithWatch.symbolWatches[0].value, 0x020a);
    const step = await request("step", {view: {memoryAddress: 0, memoryLength: 2}});
    assert.equal(step.stop.reason, "step-complete");
    assert.equal(step.snapshot.state.pc, 0x0202);

    await request("reset");
    const pausedPromise = nextEvent("stopped");
    await request("run", {instructionLimit: 10_000_000});
    const pause = await request("pause");
    assert.equal(pause.pausePending, true);
    const paused = await pausedPromise;
    assert.equal(paused.stop.reason, "paused");
    assert.ok(paused.stop.totalInstructionsExecuted < 10_000_000);

    const sleepLoaded = await request("load", {
        application: sleepApplication,
        view: {memoryAddress: 0x0200, memoryLength: 2},
    }, [sleepApplication.buffer]);
    assert.deepEqual(sleepLoaded.expressionWatches, []);
    assert.deepEqual(sleepLoaded.symbolWatches, []);
    const sleepPromise = nextEvent("stopped");
    await request("run", {
        instructionLimit: 10,
        view: {memoryAddress: 0x0200, memoryLength: 2},
    });
    const sleep = await sleepPromise;
    assert.equal(sleep.stop.reason, "sleeping");
    assert.equal(sleep.stop.instructionsExecuted, 1);
    assert.equal(sleep.stop.totalInstructionsExecuted, 1);
    assert.equal(sleep.stop.fault, "none");
    assert.equal(sleep.snapshot.state.executionState, "sleeping");
    assert.equal(sleep.snapshot.state.pc, 0x0201);
    assert.equal(sleep.snapshot.state.cycleCount, 4);
    assert.equal(sleep.snapshot.history.length, 1);
    assert.equal(sleep.snapshot.accesses.length, 1);

    const dormantPromise = nextEvent("stopped");
    await request("run", {instructionLimit: 10});
    const dormant = await dormantPromise;
    assert.equal(dormant.stop.reason, "sleeping");
    assert.equal(dormant.stop.instructionsExecuted, 0);
    assert.equal(dormant.stop.totalInstructionsExecuted, 0);
    assert.equal(dormant.stop.fault, "none");
    assert.equal(dormant.snapshot.state.executionState, "sleeping");
    assert.equal(dormant.snapshot.state.cycleCount, 4);
    assert.equal(dormant.snapshot.history.length, 1);
    assert.equal(dormant.snapshot.accesses.length, 1);

    const active = await request("reset");
    assert.equal(active.state.executionState, "active");
    assert.equal(active.state.pc, 0x0200);
    assert.equal(active.state.cycleCount, 0);

    const beforeRejectedJr800Load = await request("snapshot", {
        view: {memoryAddress: 0, memoryLength: 2},
    });
    const rejectedConfigurationRom = makeJr8rom(Uint8Array.of(0x01));
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: rejectedConfigurationRom,
            configuration: {expansionRamInitialValue: 0},
        }, [rejectedConfigurationRom.buffer]),
        /Expansion RAM requires standard RAM/,
    );
    const invalidInternalRamRom = makeJr8rom(Uint8Array.of(0x01));
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: invalidInternalRamRom,
            configuration: {internalRamInitialValue: 0x100},
        }, [invalidInternalRamRom.buffer]),
        /Internal RAM initial value must be a byte value/,
    );
    const invalidResetConditionCodeRom = makeJr8rom(Uint8Array.of(0x01));
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: invalidResetConditionCodeRom,
            configuration: {
                resetConditionCode: {value: 0, knownMask: 0x40},
            },
        }, [invalidResetConditionCodeRom.buffer]),
        /Reset condition code exceeds its allowed mask/,
    );
    const unknownConfigurationRom = makeJr8rom(Uint8Array.of(0x01));
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: unknownConfigurationRom,
            configuration: {keyboardWindowVale: 0},
        }, [unknownConfigurationRom.buffer]),
        /configuration contains an unknown field/,
    );
    const detachedCalendarRatioRom = makeJr8rom(Uint8Array.of(0x01));
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: detachedCalendarRatioRom,
            configuration: {
                calendarCpuCycleRatio: "e030-nominal-1.2288mhz",
            },
        }, [detachedCalendarRatioRom.buffer]),
        /Calendar CPU-cycle ratio requires the calendar adapter/,
    );
    const unknownCalendarRatioRom = makeJr8rom(Uint8Array.of(0x01));
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: unknownCalendarRatioRom,
            configuration: {
                calendarAddressSource: "a0-a3",
                calendarUpperRead: "zero",
                calendarCpuCycleRatio: "measured-1.2288mhz",
            },
        }, [unknownCalendarRatioRom.buffer]),
        /Unknown calendar CPU-cycle ratio/,
    );
    const obsoleteCalendarRatioRom = makeJr8rom(Uint8Array.of(0x01));
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: obsoleteCalendarRatioRom,
            configuration: {
                calendarAddressSource: "a0-a3",
                calendarUpperRead: "zero",
                calendarCpuCycleRatio: "e030-assumed-1.2288mhz",
            },
        }, [obsoleteCalendarRatioRom.buffer]),
        /Unknown calendar CPU-cycle ratio/,
    );
    const rawRom = makeLogicalRomBytes(Uint8Array.of(0x01));
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: rawRom,
        }, [rawRom.buffer]),
        /load-jr8rom failed: invalid-jr8rom/,
    );
    const incompleteRomBytes = makeLogicalRomBytes(Uint8Array.of(0x01));
    const incompleteRom = makeJr8romFromSegments([
        [0x8000, incompleteRomBytes.subarray(0, incompleteRomBytes.byteLength - 1)],
    ]);
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: incompleteRom,
        }, [incompleteRom.buffer]),
        /load-jr8rom failed: incomplete-jr8rom/,
    );
    const damagedRom = makeJr8rom(Uint8Array.of(0x01));
    damagedRom[damagedRom.byteLength - 1] ^= 0x01;
    await assert.rejects(
        request("load-jr800", {
            moduleUrl,
            romContainer: damagedRom,
        }, [damagedRom.buffer]),
        /load-jr8rom failed: integrity-mismatch/,
    );
    assert.deepEqual(
        await request("snapshot", {view: {memoryAddress: 0, memoryLength: 2}}),
        beforeRejectedJr800Load,
        "rejected JR-800 load changed the active synthetic machine",
    );
    await assert.rejects(
        request("adjust-calendar-seconds", {
            view: {memoryAddress: 0, memoryLength: 2},
        }),
        /adjust-calendar-seconds failed: wrong-machine-kind/,
    );

    const acceptedRawRom = makeLogicalRomBytes(Uint8Array.of(0x01));
    const rawRomLoaded = await request("load-jr800-raw", {
        moduleUrl,
        logicalRom: acceptedRawRom,
        view: {memoryAddress: 0x8000, memoryLength: 2},
    }, [acceptedRawRom.buffer]);
    assert.equal(rawRomLoaded.state.pc, 0x8000);
    assert.deepEqual(rawRomLoaded.memory.bytes, [0x01, 0x01]);

    const invalidRawRom = new Uint8Array(32 * 1024 - 1);
    await assert.rejects(
        request("load-jr800-raw", {
            moduleUrl,
            logicalRom: invalidRawRom,
        }, [invalidRawRom.buffer]),
        /Logical ROM input must be exactly 32768 bytes/,
    );
    assert.deepEqual(
        await request("snapshot", {
            view: {memoryAddress: 0x8000, memoryLength: 2},
        }),
        rawRomLoaded,
        "rejected raw ROM load changed the active JR-800 machine",
    );

    const nopRomBytes = makeLogicalRomBytes(Uint8Array.of(0x01));
    const nopRom = makeJr8romFromSegments([
        [0xc000, nopRomBytes.subarray(0x4000)],
        [0x8000, nopRomBytes.subarray(0, 0x4000)],
    ]);
    const jr800Loaded = await request("load-jr800", {
        moduleUrl,
        romContainer: nopRom,
        configuration: {
            resetStackPointer: 0x2345,
            resetIndexRegister: 0x3456,
            resetAccumulatorA: 0x67,
            resetAccumulatorB: 0x89,
            resetConditionCode: {value: 0x25, knownMask: 0x2f},
            internalRamInitialValue: 0xc3,
            standardRamInitialValue: 0xa5,
            expansionRamInitialValue: 0x5a,
            lcdUnknownDataReadValue: 0x3c,
            calendarAddressSource: "a1-a4",
            calendarUpperRead: "one",
            calendarCpuCycleRatio: "e030-nominal-1.2288mhz",
            keyboardWindowValue: 0x7e,
            port1Pins: {value: 0x01, knownMask: 0x01},
            port2Pins: {value: 0x00, knownMask: 0x1f},
            ramStandbyPowerValid: true,
        },
        view: {memoryAddress: 0x8000, memoryLength: 2},
    }, [nopRom.buffer]);
    assert.equal(jr800Loaded.state.abiVersion, 36);
    assert.equal(jr800Loaded.state.profile, "hd6301v1");
    assert.equal(jr800Loaded.state.pc, 0x8000);
    assert.equal(jr800Loaded.state.sp, 0x2345);
    assert.equal(jr800Loaded.state.x, 0x3456);
    assert.equal(jr800Loaded.state.a, 0x67);
    assert.equal(jr800Loaded.state.b, 0x89);
    assert.equal(jr800Loaded.state.registerKnownMask, 0x1f);
    assert.equal(jr800Loaded.state.conditionCode, 0xf5);
    assert.equal(jr800Loaded.state.conditionCodeKnownMask, 0xff);
    assert.equal(jr800Loaded.state.calendarAlarmTerminal, "released");
    assert.equal(jr800Loaded.state.port2TimerOutput, "disabled");
    assert.equal(jr800Loaded.state.lcdSubstitutedDataReadCount, 0);
    assert.deepEqual(jr800Loaded.memory.bytes, [0x01, 0x01]);
    assert.equal(jr800Loaded.disassembly.text, "NOP");
    assert.deepEqual(jr800Loaded.disassembly.bytes, [0x01, 0x00, 0x00]);
    assert.equal(jr800Loaded.disassembly.length, 1);
    assert.equal(jr800Loaded.disassembly.supported, true);
    assert.equal(jr800Loaded.source, null);
    assert.equal(jr800Loaded.lcdPanel.width, 192);
    assert.equal(jr800Loaded.lcdPanel.height, 64);
    assert.equal(jr800Loaded.lcdPanel.dots.length, 192 * 64);
    assert.ok(jr800Loaded.lcdPanel.dots instanceof Uint8Array);
    assert.deepEqual(Object.keys(jr800Loaded.lcdIndicators), [
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
    ]);
    assert.ok(
        Object.values(jr800Loaded.lcdIndicators).every(
            (value) => value === null,
        ),
    );
    assert.deepEqual(jr800Loaded.keyboardActivity, {
        readAttempts: 0,
        distinctAddresses: 0,
    });
    assert.deepEqual(jr800Loaded.expressionWatches, []);
    assert.deepEqual(jr800Loaded.symbolWatches, []);
    const internalRamSnapshot = await request("snapshot", {
        view: {memoryAddress: 0x0080, memoryLength: 2},
    });
    assert.deepEqual(internalRamSnapshot.memory.bytes, [0xc3, 0xc3]);
    await assert.rejects(
        request("set-symbol-watch", {
            watchId: 1,
            name: "entry",
            view: {memoryAddress: 0x8000, memoryLength: 2},
        }),
        /set-symbol-watch failed: wrong-machine-kind/,
    );
    const configuredRegisterWatch = await request("set-expression-watch", {
        watchId: 1,
        expression: "A",
        view: {memoryAddress: 0x8000, memoryLength: 2},
    });
    assert.equal(configuredRegisterWatch.expressionWatches[0].value, 0x67);
    assert.equal(configuredRegisterWatch.expressionWatches[0].error, "none");
    assert.equal(configuredRegisterWatch.expressionWatches[0].stateFault, "none");
    const unreadableMemoryWatch = await request("set-expression-watch", {
        watchId: 2,
        expression: "mem8[$1000]",
        view: {memoryAddress: 0x8000, memoryLength: 2},
    });
    assert.equal(unreadableMemoryWatch.expressionWatches[1].error, "memory-access");
    assert.equal(
        unreadableMemoryWatch.expressionWatches[1].busFault,
        "unsupported-access",
    );
    assert.equal(unreadableMemoryWatch.expressionWatches[1].faultAddress, 0x1000);
    assert.ok(jr800Loaded.lcdPanel.dots.every((state) => state === 1));
    await assert.rejects(
        request("run-to-symbol", {
            symbolName: "entry",
            instructionLimit: 10,
        }),
        /symbol-address failed: wrong-machine-kind/,
    );

    const beforeUnsupportedRealView = await request("snapshot", {
        view: {memoryAddress: 0x8000, memoryLength: 2},
    });
    for (const [command, fields] of [
        ["reset", {}],
        ["step", {}],
        ["step-over", {instructionLimit: 10, suspendedCycleLimit: 12}],
        ["step-out", {instructionLimit: 10, suspendedCycleLimit: 12}],
        ["run", {instructionLimit: 10, suspendedCycleLimit: 12}],
        ["advance-suspended-cycles", {cycleLimit: 1}],
    ]) {
        await assert.rejects(
            request(command, {
                ...fields,
                view: {memoryAddress: 0x1000, memoryLength: 1},
            }),
            /read-memory failed: unsupported-access/,
        );
        assert.deepEqual(
            await request("snapshot", {
                view: {memoryAddress: 0x8000, memoryLength: 2},
            }),
            beforeUnsupportedRealView,
            `${command} changed the JR-800 machine before validating its view`,
        );
    }

    const standardRam = await request("snapshot", {
        view: {memoryAddress: 0x2000, memoryLength: 1},
    });
    assert.deepEqual(standardRam.memory.bytes, [0xa5]);
    const expansionRam = await request("snapshot", {
        view: {memoryAddress: 0x6000, memoryLength: 1},
    });
    assert.deepEqual(expansionRam.memory.bytes, [0x5a]);
    const calendar = await request("snapshot", {
        view: {memoryAddress: 0x0600, memoryLength: 1},
    });
    assert.deepEqual(calendar.memory.bytes, [0xf0]);
    const lcdStatus = await request("snapshot", {
        view: {memoryAddress: 0x0a01, memoryLength: 1},
    });
    assert.deepEqual(lcdStatus.memory.bytes, [0x60]);

    const jr800Step = await request("step", {
        view: {memoryAddress: 0x8000, memoryLength: 2},
    });
    assert.equal(jr800Step.stop.reason, "step-complete");
    assert.equal(jr800Step.stop.stepKind, "instruction");
    assert.equal(jr800Step.stop.busFault, "none");
    assert.equal(jr800Step.stop.interruptSource, "none");
    assert.equal(jr800Step.snapshot.state.pc, 0x8001);
    assert.equal(jr800Step.snapshot.history[0].stepKind, "instruction");
    assert.equal(jr800Step.snapshot.history[0].stateAfter.registerKnownMask, 0x1f);
    assert.deepEqual(
        {
            initial: jr800SnapshotParity(jr800Loaded),
            step: {
                stop: jr800StopParity(jr800Step.stop),
                snapshot: jr800SnapshotParity(jr800Step.snapshot),
            },
        },
        jr800Expected,
    );

    const initialKeyboard = await request("snapshot", {
        view: {memoryAddress: 0x0c00, memoryLength: 1},
    });
    assert.deepEqual(initialKeyboard.memory.bytes, [0x7e]);
    const idleKeyboardUpdate = await request("set-keyboard-response", {
        address: 0x0c00,
        value: 0x3c,
        known: true,
    });
    assert.equal(idleKeyboardUpdate.appliedDuringRun, false);
    assert.equal(idleKeyboardUpdate.totalInstructionsExecuted, null);
    const updatedKeyboard = await request("snapshot", {
        view: {memoryAddress: 0x0c00, memoryLength: 1},
    });
    assert.deepEqual(updatedKeyboard.memory.bytes, [0x3c]);
    assert.deepEqual(updatedKeyboard.keyboardActivity, {
        readAttempts: 0,
        distinctAddresses: 0,
    });
    await request("set-keyboard-response", {
        address: 0x0f7f,
        value: 0xff,
        known: true,
    });
    await assert.rejects(
        request("set-keyboard-key-state", {
            key: "unknown-key",
            pressed: true,
        }),
        /Keyboard key is not supported/,
    );
    await assert.rejects(
        request("set-keyboard-key-state", {
            key: "letter-x",
            pressed: 1,
        }),
        /Keyboard pressed state must be boolean/,
    );
    const idleKeyPress = await request("set-keyboard-key-state", {
        key: "letter-x",
        pressed: true,
    });
    assert.deepEqual(idleKeyPress, {
        key: "letter-x",
        pressed: true,
        appliedDuringRun: false,
        totalInstructionsExecuted: null,
    });
    const pressedKeyboard = await request("snapshot", {
        view: {memoryAddress: 0x0f7f, memoryLength: 1},
    });
    assert.deepEqual(pressedKeyboard.memory.bytes, [0xfe]);
    await request("set-keyboard-key-state", {
        key: "letter-x",
        pressed: false,
    });
    const releasedKeyboard = await request("snapshot", {
        view: {memoryAddress: 0x0f7f, memoryLength: 1},
    });
    assert.deepEqual(releasedKeyboard.memory.bytes, [0xff]);
    await assert.rejects(
        request("set-keyboard-key-state", {
            key: "letter-x",
            pressed: true,
            minimumHoldCycles: -1,
        }),
        /Minimum key hold must be a uint32 cycle count/,
    );
    await request("set-keyboard-key-state", {
        key: "letter-x",
        pressed: true,
        minimumHoldCycles: 100,
    });
    const deferredKeyboardReleasePromise = request("set-keyboard-key-state", {
        key: "letter-x",
        pressed: false,
    });
    const deferredKeyboardRelease = await request("snapshot", {
        view: {memoryAddress: 0x0f7f, memoryLength: 1},
    });
    assert.deepEqual(deferredKeyboardRelease.memory.bytes, [0xfe]);
    const keyboardHoldStopPromise = nextEvent("stopped");
    await request("run", {instructionLimit: 1_000});
    const keyboardHoldStop = await keyboardHoldStopPromise;
    await deferredKeyboardReleasePromise;
    assert.equal(keyboardHoldStop.stop.reason, "instruction-limit");
    const minimumHoldReleased = await request("snapshot", {
        view: {memoryAddress: 0x0f7f, memoryLength: 1},
    });
    assert.deepEqual(minimumHoldReleased.memory.bytes, [0xff]);
    const modeledKeyboardKeys = [
        "shift",
        "control",
        "menu",
        "return",
        "space",
        "main-1",
        "letter-a",
        "letter-x",
        "keypad-insert-rub",
        "keypad-vertical-arrows",
        "keypad-horizontal-arrows",
        "keypad-0",
        "keypad-1",
        "keypad-2",
        "keypad-3",
        "keypad-4",
        "keypad-5",
        "keypad-6",
        "keypad-7",
        "break",
        "home-cls",
        "main-0",
        "main-2",
        "main-3",
        "main-4",
        "main-5",
        "main-6",
        "main-7",
        "main-8",
        "main-9",
        "main-caret",
        "letter-b",
        "letter-c",
        "letter-d",
        "letter-e",
        "letter-f",
        "letter-g",
        "letter-h",
        "letter-i",
        "letter-j",
        "letter-k",
        "letter-l",
        "letter-m",
        "letter-n",
        "letter-o",
        "letter-p",
        "letter-q",
        "letter-r",
        "letter-s",
        "letter-t",
        "letter-u",
        "letter-v",
        "letter-w",
        "letter-y",
        "letter-z",
        "colon",
        "semicolon",
        "comma",
        "period",
        "pf-1",
        "pf-2",
        "pf-3",
        "pf-4",
        "pf-5",
        "pf-6",
        "pf-7",
        "pf-8",
        "pf-9",
        "pf-10",
        "keypad-8",
        "keypad-9",
        "keypad-multiply",
        "keypad-add",
        "keypad-equal",
        "keypad-subtract",
        "keypad-decimal",
        "keypad-divide",
    ];
    for (const key of modeledKeyboardKeys) {
        await request("set-keyboard-key-state", {key, pressed: true});
        await request("set-keyboard-key-state", {key, pressed: false});
    }
    assert.equal(modeledKeyboardKeys.length, 77);
    await request("set-keyboard-response", {
        address: 0x0ffd,
        value: 0xff,
        known: true,
    });
    await request("set-keyboard-key-state", {
        key: "keypad-divide",
        pressed: true,
    });
    const expandedKeyboard = await request("snapshot", {
        view: {memoryAddress: 0x0ffd, memoryLength: 1},
    });
    assert.deepEqual(expandedKeyboard.memory.bytes, [0x7f]);
    await request("set-keyboard-key-state", {
        key: "keypad-divide",
        pressed: false,
    });
    await request("set-keyboard-response", {
        address: 0x0ffd,
        value: 0,
        known: false,
    });

    const highTimerOutputRom = makeJr8rom(Uint8Array.of(
        0x86, 0x01,
        0x97, 0x08,
        0x86, 0x02,
        0x97, 0x01,
        0xcc, 0xff, 0xfc,
        0xdd, 0x09,
    ));
    const highTimerOutputLoaded = await request("load-jr800", {
        romContainer: highTimerOutputRom,
        configuration: {},
        view: {memoryAddress: 0x8000, memoryLength: 1},
    }, [highTimerOutputRom.buffer]);
    assert.equal(highTimerOutputLoaded.state.port2TimerOutput, "disabled");
    let timerOutputSnapshot;
    for (let instruction = 0; instruction < 4; instruction += 1) {
        timerOutputSnapshot = await request("step", {
            view: {memoryAddress: 0x8000, memoryLength: 1},
        });
    }
    assert.equal(
        timerOutputSnapshot.snapshot.state.port2TimerOutput,
        "unknown",
    );
    for (let instruction = 0; instruction < 2; instruction += 1) {
        timerOutputSnapshot = await request("step", {
            view: {memoryAddress: 0x8000, memoryLength: 1},
        });
    }
    assert.equal(
        timerOutputSnapshot.snapshot.state.port2TimerOutput,
        "high",
    );

    const lowTimerOutputRom = makeJr8rom(Uint8Array.of(
        0x86, 0x00,
        0x97, 0x08,
        0x86, 0x02,
        0x97, 0x01,
        0xcc, 0xff, 0xfc,
        0xdd, 0x09,
    ));
    await request("load-jr800", {
        romContainer: lowTimerOutputRom,
        configuration: {},
        view: {memoryAddress: 0x8000, memoryLength: 1},
    }, [lowTimerOutputRom.buffer]);
    for (let instruction = 0; instruction < 6; instruction += 1) {
        timerOutputSnapshot = await request("step", {
            view: {memoryAddress: 0x8000, memoryLength: 1},
        });
    }
    assert.equal(
        timerOutputSnapshot.snapshot.state.port2TimerOutput,
        "low",
    );

    const calendarTickRom = makeJr8rom(Uint8Array.of(
        0x86, 0x0c,
        0xb7, 0x06, 0x0d,
    ));
    await request("load-jr800", {
        romContainer: calendarTickRom,
        configuration: {
            calendarAddressSource: "a0-a3",
            calendarUpperRead: "zero",
        },
        view: {memoryAddress: 0x0600, memoryLength: 2},
    }, [calendarTickRom.buffer]);
    await request("step", {
        view: {memoryAddress: 0x0600, memoryLength: 2},
    });
    const calendarAlarm = await request("step", {
        view: {memoryAddress: 0x0600, memoryLength: 2},
    });
    assert.equal(calendarAlarm.snapshot.state.calendarAlarmTerminal, "pull-low");
    const calendarSubsecond = await request("advance-calendar-oscillator", {
        ticks: 32_767,
        view: {memoryAddress: 0x0600, memoryLength: 2},
    });
    assert.equal(calendarSubsecond.ticks, 32_767);
    assert.deepEqual(calendarSubsecond.snapshot.memory.bytes, [0, 0]);
    const calendarBoundary = await request("advance-calendar-oscillator", {
        ticks: 1,
        view: {memoryAddress: 0x0600, memoryLength: 2},
    });
    assert.equal(calendarBoundary.ticks, 1);
    assert.deepEqual(calendarBoundary.snapshot.memory.bytes, [1, 0]);
    assert.equal(
        calendarBoundary.snapshot.state.calendarAlarmTerminal,
        "pull-low",
    );
    await assert.rejects(
        request("advance-calendar-oscillator", {
            ticks: 0x1_0000_0000,
            view: {memoryAddress: 0x0600, memoryLength: 2},
        }),
        /Calendar oscillator ticks must be a uint32 value/,
    );

    const calendarAdjustRom = makeJr8rom(Uint8Array.of(
        0x86, 0x00,
        0xb7, 0x06, 0x00,
        0x86, 0x03,
        0xb7, 0x06, 0x01,
    ));
    await request("load-jr800", {
        romContainer: calendarAdjustRom,
        configuration: {
            calendarAddressSource: "a0-a3",
            calendarUpperRead: "zero",
        },
        view: {memoryAddress: 0x0600, memoryLength: 3},
    }, [calendarAdjustRom.buffer]);
    for (let instruction = 0; instruction < 4; instruction += 1) {
        await request("step", {
            view: {memoryAddress: 0x0600, memoryLength: 3},
        });
    }
    const beforeCalendarAdjust = await request("snapshot", {
        view: {memoryAddress: 0x0600, memoryLength: 3},
    });
    assert.deepEqual(beforeCalendarAdjust.memory.bytes, [0, 3, 0]);
    await assert.rejects(
        request("adjust-calendar-seconds", {
            view: {memoryAddress: 0x1000, memoryLength: 1},
        }),
        /read-memory failed: unsupported-access/,
    );
    const preservedCalendarAdjust = await request("snapshot", {
        view: {memoryAddress: 0x0600, memoryLength: 3},
    });
    assert.deepEqual(preservedCalendarAdjust.memory.bytes, [0, 3, 0]);
    const calendarAdjusted = await request("adjust-calendar-seconds", {
        view: {memoryAddress: 0x0600, memoryLength: 3},
    });
    assert.deepEqual(calendarAdjusted.memory.bytes, [0, 0, 1]);
    assert.equal(
        calendarAdjusted.state.cycleCount,
        beforeCalendarAdjust.state.cycleCount,
    );
    assert.equal(
        calendarAdjusted.history.length,
        beforeCalendarAdjust.history.length,
    );
    assert.equal(
        calendarAdjusted.accesses.length,
        beforeCalendarAdjust.accesses.length,
    );

    const calendarRatioRom = makeJr8rom(Uint8Array.of(
        0x86, 0x08,
        0xb7, 0x06, 0x0d,
    ));
    await request("load-jr800", {
        romContainer: calendarRatioRom,
        configuration: {
            calendarAddressSource: "a0-a3",
            calendarUpperRead: "zero",
            calendarCpuCycleRatio: "e030-nominal-1.2288mhz",
        },
        view: {memoryAddress: 0x0600, memoryLength: 2},
    }, [calendarRatioRom.buffer]);
    const ratioSetupLoad = await request("step", {
        view: {memoryAddress: 0x0600, memoryLength: 2},
    });
    const ratioSetupStore = await request("step", {
        view: {memoryAddress: 0x0600, memoryLength: 2},
    });
    assert.equal(ratioSetupLoad.stop.cycles, 2);
    assert.equal(ratioSetupStore.stop.cycles, 4);
    await request("advance-calendar-oscillator", {
        ticks: 32_767,
        view: {memoryAddress: 0x0600, memoryLength: 2},
    });
    for (let instruction = 0; instruction < 31; instruction += 1) {
        const beforeBoundary = await request("step", {
            view: {memoryAddress: 0x0600, memoryLength: 2},
        });
        assert.equal(beforeBoundary.stop.cycles, 1);
    }
    const ratioSubsecond = await request("snapshot", {
        view: {memoryAddress: 0x0600, memoryLength: 2},
    });
    assert.deepEqual(ratioSubsecond.memory.bytes, [0, 0]);
    const ratioBoundary = await request("step", {
        view: {memoryAddress: 0x0600, memoryLength: 2},
    });
    assert.equal(ratioBoundary.stop.cycles, 1);
    assert.deepEqual(ratioBoundary.snapshot.memory.bytes, [1, 0]);

    // Four instructions align each 1,000-instruction Worker slice before LDAA.
    const liveKeyboardRom = makeJr8rom(Uint8Array.of(
        0xb6, 0x0f, 0x7f,
        0xb7, 0x20, 0x00,
        0x01,
        0x20, 0xf7,
    ));
    await request("load-jr800", {
        romContainer: liveKeyboardRom,
        configuration: {
            standardRamInitialValue: 0,
            keyboardWindowValue: 0x7e,
        },
        view: {memoryAddress: 0x2000, memoryLength: 1},
    }, [liveKeyboardRom.buffer]);
    await assert.rejects(
        request("advance-calendar-oscillator", {
            ticks: 1,
            view: {memoryAddress: 0x2000, memoryLength: 1},
        }),
        /advance-calendar-oscillator failed: unsupported-access/,
    );
    await assert.rejects(
        request("adjust-calendar-seconds", {
            view: {memoryAddress: 0x2000, memoryLength: 1},
        }),
        /adjust-calendar-seconds failed: unsupported-access/,
    );
    const liveKeyboardStopPromise = nextEvent("stopped");
    await request("run", {instructionLimit: 10_000_000});
    await assert.rejects(
        request("adjust-calendar-seconds", {
            view: {memoryAddress: 0x2000, memoryLength: 1},
        }),
        /Machine is running/,
    );
    const liveKeyboardUpdate = await request("set-keyboard-response", {
        address: 0x0f7f,
        value: 0xff,
        known: true,
    });
    assert.equal(liveKeyboardUpdate.appliedDuringRun, true);
    assert.ok(Number.isSafeInteger(
        liveKeyboardUpdate.totalInstructionsExecuted,
    ));
    assert.equal(liveKeyboardUpdate.totalInstructionsExecuted % 1_000, 0);
    const liveKeyPress = await request("set-keyboard-key-state", {
        key: "letter-x",
        pressed: true,
    });
    assert.equal(liveKeyPress.appliedDuringRun, true);
    assert.ok(Number.isSafeInteger(
        liveKeyPress.totalInstructionsExecuted,
    ));
    assert.equal(liveKeyPress.totalInstructionsExecuted % 1_000, 0);
    await request("set-memory-watchpoint", {
        address: 0x2000,
        mode: "write",
        enabled: true,
    });
    const liveKeyboardStop = await liveKeyboardStopPromise;
    assert.equal(liveKeyboardStop.stop.reason, "memory-watchpoint");
    assert.equal(liveKeyboardStop.stop.triggerAddress, 0x2000);
    assert.equal(liveKeyboardStop.stop.triggerAccess, "data-write");
    assert.ok(liveKeyboardStop.snapshot.keyboardActivity.readAttempts > 0);
    assert.equal(
        liveKeyboardStop.snapshot.keyboardActivity.distinctAddresses,
        1,
    );
    const liveKeyboardRam = await request("snapshot", {
        view: {memoryAddress: 0x2000, memoryLength: 1},
    });
    assert.deepEqual(liveKeyboardRam.memory.bytes, [0xfe]);
    const stoppedKeyRelease = await request("set-keyboard-key-state", {
        key: "letter-x",
        pressed: false,
    });
    assert.equal(stoppedKeyRelease.appliedDuringRun, false);

    const ramProgram = makeJr8app(
        0x2800,
        0x2800,
        Uint8Array.of(0x86, 0x42, 0x20, 0xfe),
    );
    const loadedRamProgram = await request("load-program", {
        application: ramProgram,
        view: {memoryAddress: 0x2800, memoryLength: 4},
    }, [ramProgram.buffer]);
    assert.equal(loadedRamProgram.state.pc, 0x2800);
    assert.deepEqual(loadedRamProgram.memory.bytes, [0x86, 0x42, 0x20, 0xfe]);
    assert.equal(loadedRamProgram.history.length, 0);
    assert.equal(loadedRamProgram.accesses.length, 0);
    const ramProgramStep = await request("step", {
        view: {memoryAddress: 0x2800, memoryLength: 4},
    });
    assert.equal(ramProgramStep.snapshot.state.pc, 0x2802);
    assert.equal(ramProgramStep.snapshot.state.a, 0x42);

    const beforeRejectedWav = await request("snapshot", {
        view: {memoryAddress: 0x2800, memoryLength: 4},
    });
    const invalidWav = Uint8Array.of(0x52, 0x49, 0x46, 0x46);
    await assert.rejects(
        request("load-native-program-wav", {
            wav: invalidWav,
            view: {memoryAddress: 0x2800, memoryLength: 4},
        }, [invalidWav.buffer]),
        /WAV conversion failed: invalid-wav/,
    );
    assert.deepEqual(
        await request("snapshot", {
            view: {memoryAddress: 0x2800, memoryLength: 4},
        }),
        beforeRejectedWav,
        "rejected WAV conversion changed the active JR-800 machine",
    );

    const nativeProgramWav = makeNativeProgramWav(
        0x2800,
        0x2800,
        Uint8Array.of(0x86, 0x43, 0x20, 0xfe),
    );
    const loadedNativeProgramWav = await request("load-native-program-wav", {
        wav: nativeProgramWav,
        view: {memoryAddress: 0x2800, memoryLength: 4},
    }, [nativeProgramWav.buffer]);
    assert.equal(loadedNativeProgramWav.state.pc, 0x2800);
    assert.deepEqual(
        loadedNativeProgramWav.memory.bytes,
        [0x86, 0x43, 0x20, 0xfe],
    );
    const nativeProgramWavStep = await request("step", {
        view: {memoryAddress: 0x2800, memoryLength: 4},
    });
    assert.equal(nativeProgramWavStep.snapshot.state.pc, 0x2802);
    assert.equal(nativeProgramWavStep.snapshot.state.a, 0x43);

    const beforeRejectedRamProgram = await request("snapshot", {
        view: {memoryAddress: 0x2800, memoryLength: 4},
    });
    const rejectedRamProgram = application.slice();
    await assert.rejects(
        request(
            "load-program",
            {
                application: rejectedRamProgram,
                view: {memoryAddress: 0x2800, memoryLength: 4},
            },
            [rejectedRamProgram.buffer],
        ),
        /load-program failed: segment-out-of-range/,
    );
    assert.deepEqual(
        await request("snapshot", {
            view: {memoryAddress: 0x2800, memoryLength: 4},
        }),
        beforeRejectedRamProgram,
        "rejected RAM program load changed the active JR-800 machine",
    );

    const beforeRejectedRealApplication = await request("snapshot", {
        view: {memoryAddress: 0x8000, memoryLength: 2},
    });
    const realApplication = application.slice();
    await assert.rejects(
        request("load", {application: realApplication}, [realApplication.buffer]),
        /load-application failed: wrong-machine-kind/,
    );
    assert.deepEqual(
        await request("snapshot", {
            view: {memoryAddress: 0x8000, memoryLength: 2},
        }),
        beforeRejectedRealApplication,
        "rejected application load changed the active JR-800 machine",
    );

    const stepOverProgram = new Uint8Array(1_018);
    stepOverProgram.fill(0x01);
    stepOverProgram.set(
        Uint8Array.of(
            0x8e, 0x21, 0xff,
            0x8d, 0x0b,
            0x86, 0x55,
        ),
    );
    stepOverProgram[1_017] = 0x39;
    const stepOverRom = makeJr8rom(stepOverProgram);
    await request("load-jr800", {
        romContainer: stepOverRom,
        configuration: {standardRamInitialValue: 0},
        view: {memoryAddress: 0x8000, memoryLength: 8},
    }, [stepOverRom.buffer]);
    const stackSetup = await request("step", {
        view: {memoryAddress: 0x8000, memoryLength: 8},
    });
    assert.equal(stackSetup.snapshot.state.pc, 0x8003);
    assert.equal(stackSetup.snapshot.state.sp, 0x21ff);
    await request("set-execution-breakpoint", {address: 0x8003, enabled: true});
    await request("set-execution-breakpoint", {address: 0x8005, enabled: true});
    const stepOverPromise = nextEvent("stopped");
    await request("step-over", {
        instructionLimit: 2_000,
        view: {memoryAddress: 0x8000, memoryLength: 8},
    });
    const stepOver = await stepOverPromise;
    assert.equal(stepOver.stop.reason, "address-reached");
    assert.equal(stepOver.stop.triggerAddress, 0x8005);
    assert.equal(stepOver.stop.continuationAddress, null);
    assert.equal(stepOver.stop.totalInstructionsExecuted, 1_003);
    assert.equal(stepOver.snapshot.state.pc, 0x8005);
    assert.equal(stepOver.snapshot.state.sp, 0x21ff);

    const stepOutProgram = new Uint8Array(1_034);
    stepOutProgram.fill(0x01);
    stepOutProgram.set(
        Uint8Array.of(
            0x8e, 0x21, 0xff,
            0xbd, 0x80, 0x10,
        ),
    );
    stepOutProgram.set(Uint8Array.of(0x8d, 0x0e, 0x39), 16);
    stepOutProgram[1_033] = 0x39;

    const boundedStepOutRom = makeJr8rom(stepOutProgram);
    await request("load-jr800", {
        romContainer: boundedStepOutRom,
        configuration: {standardRamInitialValue: 0},
    }, [boundedStepOutRom.buffer]);
    await request("step");
    await request("step");
    await request("set-execution-breakpoint", {address: 0x8010, enabled: true});
    await request("set-execution-breakpoint", {address: 0x8006, enabled: true});
    const boundedStepOutPromise = nextEvent("stopped");
    await request("step-out", {instructionLimit: 1_000});
    const boundedStepOut = await boundedStepOutPromise;
    assert.equal(boundedStepOut.stop.reason, "instruction-limit");
    assert.equal(boundedStepOut.stop.totalInstructionsExecuted, 1_000);
    assert.deepEqual(
        boundedStepOut.stop.stepOutState,
        {continued: true, nestingDepth: 1},
    );

    const continuedStepOutPromise = nextEvent("stopped");
    await request("step-out", {
        instructionLimit: 10,
        stepOutState: boundedStepOut.stop.stepOutState,
    });
    const continuedStepOut = await continuedStepOutPromise;
    assert.equal(continuedStepOut.stop.reason, "step-out-complete");
    assert.equal(continuedStepOut.stop.triggerAddress, 0x8006);
    assert.equal(continuedStepOut.stop.totalInstructionsExecuted, 4);
    assert.equal(continuedStepOut.snapshot.state.pc, 0x8006);
    assert.equal(continuedStepOut.snapshot.state.sp, 0x21ff);

    const automaticStepOutRom = makeJr8rom(stepOutProgram);
    await request("load-jr800", {
        romContainer: automaticStepOutRom,
        configuration: {standardRamInitialValue: 0},
    }, [automaticStepOutRom.buffer]);
    await request("step");
    await request("step");
    await request("set-execution-breakpoint", {address: 0x8010, enabled: true});
    await request("set-execution-breakpoint", {address: 0x8006, enabled: true});
    const automaticStepOutPromise = nextEvent("stopped");
    await request("step-out", {instructionLimit: 2_000});
    const automaticStepOut = await automaticStepOutPromise;
    assert.equal(automaticStepOut.stop.reason, "step-out-complete");
    assert.equal(automaticStepOut.stop.triggerAddress, 0x8006);
    assert.equal(automaticStepOut.stop.totalInstructionsExecuted, 1_004);
    assert.equal(automaticStepOut.stop.stepOutState, undefined);
    assert.equal(automaticStepOut.snapshot.state.pc, 0x8006);
    assert.equal(automaticStepOut.snapshot.state.sp, 0x21ff);

    const readWatchRom = makeJr8rom(Uint8Array.of(0xb6, 0x80, 0x00));
    await request(
        "load-jr800",
        {romContainer: readWatchRom},
        [readWatchRom.buffer],
    );
    await request("set-memory-watchpoint", {
        address: 0x8000,
        mode: "read",
        enabled: true,
    });
    const readWatchPromise = nextEvent("stopped");
    await request("run", {instructionLimit: 4});
    const readWatch = await readWatchPromise;
    assert.equal(readWatch.stop.reason, "memory-watchpoint");
    assert.equal(readWatch.stop.triggerAddress, 0x8000);
    assert.equal(readWatch.stop.triggerAccess, "data-read");
    assert.equal(readWatch.stop.totalInstructionsExecuted, 1);

    const lcdProgram = Uint8Array.of(
        0x86, 0x3e,
        0xb7, 0x0a, 0x01,
        0x86, 0x39,
        0xb7, 0x0a, 0x01,
        0x86, 0x00,
        0xb7, 0x0a, 0x01,
        0x86, 0x01,
        0xb7, 0x0b, 0x01,
        0x86, 0x00,
        0xb7, 0x0a, 0x01,
        0xb6, 0x0b, 0x01,
        0x86, 0x2e,
        0xb7, 0x0a, 0x01,
        0x86, 0x80,
        0xb7, 0x0b, 0x01,
    );
    const lcdRom = makeJr8rom(lcdProgram);
    await request("load-jr800", {
        romContainer: lcdRom,
        configuration: {lcdUnknownDataReadValue: 0},
    }, [lcdRom.buffer]);
    let lcdStep;
    for (let instruction = 0; instruction < 11; ++instruction) {
        lcdStep = await request("step");
    }
    assert.equal(lcdStep.snapshot.lcdPanel.dots[0], 0);
    assert.equal(lcdStep.snapshot.lcdPanel.dots[45], 2);
    assert.equal(lcdStep.snapshot.lcdPanel.dots[192 + 45], 1);
    assert.equal(lcdStep.snapshot.state.lcdSubstitutedDataReadCount, 1);
    for (let instruction = 0; instruction < 4; ++instruction) {
        lcdStep = await request("step");
    }
    assert.equal(lcdStep.snapshot.lcdIndicators["page-1"], 0x80);
    assert.ok(
        Object.entries(lcdStep.snapshot.lcdIndicators).every(
            ([name, value]) => name === "page-1" || value === null,
        ),
    );

    const sleepProgram = Uint8Array.of(
        0x86, 0x00,
        0x97, 0x0b,
        0x86, 0x20,
        0x97, 0x0c,
        0x86, 0x08,
        0x97, 0x08,
        0x1a,
    );
    const boundedSleepRom = makeJr8rom(sleepProgram);
    await request("load-jr800", {
        romContainer: boundedSleepRom,
        view: {memoryAddress: 0x8000, memoryLength: sleepProgram.byteLength},
    }, [boundedSleepRom.buffer]);
    assert.equal(
        (await request("snapshot")).lcdPanel,
        null,
        "Disconnected LCD unexpectedly produced a panel",
    );
    assert.equal(
        (await request("snapshot")).lcdIndicators,
        null,
        "Disconnected LCD unexpectedly produced indicators",
    );
    assert.equal(
        (await request("snapshot")).state.lcdSubstitutedDataReadCount,
        null,
        "Disconnected LCD unexpectedly exposed a substituted-read count",
    );
    const boundedSleepPromise = nextEvent("stopped");
    await request("run", {
        instructionLimit: 8,
        suspendedCycleLimit: 12,
    });
    const boundedSleep = await boundedSleepPromise;
    assert.equal(boundedSleep.stop.reason, "sleeping");
    assert.equal(boundedSleep.stop.totalInstructionsExecuted, 7);
    assert.equal(boundedSleep.stop.totalSuspendedCyclesElapsed, 12);
    assert.equal(boundedSleep.stop.suspendedAdvance.suspended, true);
    assert.equal(boundedSleep.stop.suspendedAdvance.interruptKnown, true);
    assert.equal(boundedSleep.stop.suspendedAdvance.interruptSource, "none");
    assert.equal(boundedSleep.stop.suspendedAdvance.busFault, "none");
    assert.equal(boundedSleep.snapshot.state.executionState, "sleeping");
    assert.equal(boundedSleep.snapshot.state.cycleCount, 31);

    const timerBoundary = await request("advance-suspended-cycles", {
        cycleLimit: 10,
    });
    assert.equal(timerBoundary.advance.suspended, true);
    assert.equal(timerBoundary.advance.cyclesElapsed, 1);
    assert.equal(timerBoundary.advance.interruptKnown, true);
    assert.equal(timerBoundary.advance.busFault, "none");
    assert.equal(
        timerBoundary.advance.interruptSource,
        "timer-output-compare",
    );
    assert.equal(timerBoundary.snapshot.state.cycleCount, 32);
    const resumed = await request("step");
    assert.equal(resumed.stop.reason, "step-complete");
    assert.equal(resumed.stop.stepKind, "sleep-resume");
    assert.equal(resumed.stop.instructionsExecuted, 0);
    assert.equal(resumed.snapshot.state.executionState, "active");
    assert.equal(resumed.snapshot.state.pc, 0x800d);

    const wakingSleepRom = makeJr8rom(sleepProgram);
    await request(
        "load-jr800",
        {romContainer: wakingSleepRom},
        [wakingSleepRom.buffer],
    );
    const wakingSleepPromise = nextEvent("stopped");
    await request("run", {
        instructionLimit: 8,
        suspendedCycleLimit: 32,
    });
    const wakingSleep = await wakingSleepPromise;
    assert.equal(wakingSleep.stop.reason, "instruction-limit");
    assert.equal(wakingSleep.stop.totalInstructionsExecuted, 8);
    assert.equal(wakingSleep.stop.totalSuspendedCyclesElapsed, 13);
    assert.equal(wakingSleep.snapshot.state.executionState, "active");
    assert.equal(wakingSleep.snapshot.state.pc, 0x800e);
    assert.equal(wakingSleep.snapshot.state.cycleCount, 33);

    await assert.rejects(
        request("advance-suspended-cycles", {cycleLimit: 65_537}),
        /Worker cycle limit must be between 1 and 65536/,
    );
    const longSleepRom = makeJr8rom(Uint8Array.of(0x1a));
    await request(
        "load-jr800",
        {romContainer: longSleepRom},
        [longSleepRom.buffer],
    );
    const longSleepPromise = nextEvent("stopped");
    await request("run", {
        instructionLimit: 2,
        suspendedCycleLimit: 65_537,
    });
    const longSleep = await longSleepPromise;
    assert.equal(longSleep.stop.reason, "sleeping");
    assert.equal(longSleep.stop.totalInstructionsExecuted, 1);
    assert.equal(longSleep.stop.totalSuspendedCyclesElapsed, 65_537);
    assert.equal(longSleep.snapshot.state.cycleCount, 65_541);

    const audioRom = makeJr8rom(Uint8Array.of(
        0x86, 0xef,
        0x97, 0x02,
        0x86, 0xff,
        0x97, 0x02,
        0x86, 0xef,
        0x97, 0x02,
        0x1a,
    ));
    await request(
        "load-jr800",
        {romContainer: audioRom},
        [audioRom.buffer],
    );
    assert.deepEqual(
        await request("set-audio-enabled", {enabled: true}),
        {enabled: true},
    );
    const audioPromise = nextEvent("audio-transitions");
    const audioStopPromise = nextEvent("stopped");
    await request("run", {instructionLimit: 20});
    const [audio, audioStop] = await Promise.all([
        audioPromise,
        audioStopPromise,
    ]);
    assert.equal(audio.clockHz, 1_228_800);
    assert.deepEqual(audio.transitions.map(({level}) => level), [true, false]);
    assert.ok(audio.transitions[1].cycle > audio.transitions[0].cycle);
    assert.equal(audioStop.stop.reason, "sleeping");
    const audioResetPromise = nextEvent("audio-reset");
    await request("reset");
    await audioResetPromise;

    const powerOffRom = makeJr8rom(Uint8Array.of(0x20, 0xfe));
    await request(
        "load-jr800",
        {romContainer: powerOffRom},
        [powerOffRom.buffer],
    );
    await request("run", {instructionLimit: 10_000_000});
    assert.deepEqual(await request("power-off"), {poweredOff: true});
    await new Promise((resolvePromise) => setTimeout(resolvePromise, 10));
    assert.equal(
        queuedEvents.some((message) => message.event === "stopped"),
        false,
        "A powered-off run must not emit a later stop event",
    );
    await assert.rejects(
        request("snapshot"),
        /Worker has not been initialized/,
    );

    const reloadedApplication = application.slice();
    const reloadedAfterPowerOff = await request("load", {
        application: reloadedApplication,
        view: {memoryAddress: 0, memoryLength: 2},
    }, [reloadedApplication.buffer]);
    assert.equal(reloadedAfterPowerOff.state.profile, expected.state.profile);

    await request("dispose");
} finally {
    await worker.terminate();
}
