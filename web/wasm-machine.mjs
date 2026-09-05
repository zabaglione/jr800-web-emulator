// SPDX-License-Identifier: MIT

export const Status = Object.freeze({
    ok: 0,
    invalidArgument: 1,
    noApplication: 2,
    invalidApplication: 3,
    unknownProfile: 4,
    unreviewedProfile: 5,
    segmentOutOfRange: 6,
    invalidDebugInfo: 7,
    targetMismatch: 8,
    integrityMismatch: 9,
    detached: 10,
    bufferTooSmall: 11,
    notFound: 12,
    internalError: 13,
    noRom: 14,
    wrongMachineKind: 15,
    invalidLogicalRom: 16,
    backingStoreUnavailable: 17,
    uninitializedRead: 18,
    unsupportedAccess: 19,
    invalidExpression: 20,
    ambiguousSymbol: 21,
    symbolNotAddress: 22,
    invalidJr8rom: 23,
    incompleteJr8rom: 24,
    invalidNativeProgramWav: 25,
    unsupportedBasicRom: 26,
    basicNotReady: 27,
    invalidBasicProgram: 28,
    basicLoadFailed: 29,
    entryPointNotLoaded: 30,
});

export const MAX_LINKED_BINARY_BYTES = 64 * 1024 * 1024;
export const MAX_JR8ROM_BYTES = 52 + 65_535 * 6 + 65_536;
export const JR800_LOGICAL_ROM_BYTES = 32 * 1024;
export const JR800_LCD_PANEL_WIDTH = 192;
export const JR800_LCD_PANEL_HEIGHT = 64;
export const JR800_LCD_PANEL_DOT_COUNT =
    JR800_LCD_PANEL_WIDTH * JR800_LCD_PANEL_HEIGHT;
export const Jr800LcdDotState = Object.freeze({unknown: 0, off: 1, on: 2});
export const MemoryWatchpointMode = Object.freeze({read: 1, write: 2, access: 3});
export const AccessTraceMask = Object.freeze({
    instructionFetch: 0x01,
    dataRead: 0x02,
    dataWrite: 0x04,
    data: 0x06,
    all: 0x07,
});

const STATUS_NAMES = [
    "ok",
    "invalid-argument",
    "no-application",
    "invalid-application",
    "unknown-profile",
    "unreviewed-profile",
    "segment-out-of-range",
    "invalid-debug-info",
    "target-mismatch",
    "integrity-mismatch",
    "detached",
    "buffer-too-small",
    "not-found",
    "internal-error",
    "no-rom",
    "wrong-machine-kind",
    "invalid-logical-rom",
    "backing-store-unavailable",
    "uninitialized-read",
    "unsupported-access",
    "invalid-expression",
    "ambiguous-symbol",
    "symbol-not-address",
    "invalid-jr8rom",
    "incomplete-jr8rom",
    "invalid-native-program-wav",
    "unsupported-basic-rom",
    "basic-not-ready",
    "invalid-basic-program",
    "basic-load-failed",
];

const NATIVE_PROGRAM_WAV_ISSUE_NAMES = [
    "none",
    "invalid-wav",
    "unsupported-wav",
    "no-signal",
    "unexpected-burst-count",
    "synchronization-failed",
    "truncated-block",
    "framing-error",
    "checksum-mismatch",
    "unsupported-header",
    "invalid-length",
    "invalid-program-range",
    "ambiguous-header-byte-order",
    "invalid-basic-program",
    "unexpected-trailing-blocks",
];

const PROFILE_NAMES = ["jr800_unresolved", "mc6801", "hd6301v1"];
const STOP_NAMES = [
    "step-complete",
    "instruction-limit",
    "execution-breakpoint",
    "memory-watchpoint",
    "cpu-fault",
    "detached",
    "sleeping",
    "address-reached",
    "step-out-complete",
    "breakpoint-condition-error",
];
const FAULT_NAMES = [
    "none",
    "unsupported-opcode",
    "unimplemented-operation",
    "bus-access",
    "unknown-state",
    "unknown-interrupt-request",
    "bus-advance",
];
const EXECUTION_STATE_NAMES = [
    "active",
    "sleeping",
    "waiting-for-interrupt",
];
const ACCESS_NAMES = ["instruction-fetch", "data-read", "data-write"];
const BUS_FAULT_NAMES = [
    "none",
    "backing-store-unavailable",
    "uninitialized-read",
    "unsupported-access",
    "read-only-write",
    "device-state-unknown",
    "device-state-unsupported",
];
const STATE_PART_NAMES = [
    "none",
    "program-counter",
    "stack-pointer",
    "index-register",
    "accumulator-a",
    "accumulator-b",
    "condition-code",
];
const STEP_KIND_NAMES = [
    "dormant",
    "instruction",
    "interrupt-entry",
    "sleep-resume",
];
const INTERRUPT_NAMES = [
    "none",
    "timer-input-capture",
    "timer-output-compare",
    "timer-overflow",
    "serial",
];
const EXPRESSION_ERROR_NAMES = [
    "none",
    "unknown-state",
    "memory-access",
    "division-by-zero",
    "invalid-shift",
    "address-out-of-range",
    "symbol-not-found",
    "ambiguous-symbol",
];
const SYMBOL_BINDING_NAMES = ["unknown", "local", "global"];
const SYMBOL_KIND_NAMES = ["unknown", "address", "absolute"];
const CALENDAR_ADDRESS_SOURCES = Object.freeze({
    "a0-a3": 0,
    "a1-a4": 1,
    "a2-a5": 2,
    "a3-a6": 3,
    "a4-a7": 4,
    "a5-a8": 5,
});
const CALENDAR_UPPER_READ_BITS = Object.freeze({zero: 0x00, one: 0xf0});
const CALENDAR_CPU_CYCLE_RATIOS = Object.freeze({
    "e030-nominal-1.2288mhz": 1,
});
const CALENDAR_ALARM_TERMINAL_NAMES = [
    "disconnected",
    "unknown",
    "released",
    "pull-low",
];
const PORT2_TIMER_OUTPUT_NAMES = [
    "unavailable",
    "disabled",
    "unknown",
    "low",
    "high",
];
export const Jr800KeyboardKey = Object.freeze({
    shift: 0,
    control: 1,
    menu: 2,
    return: 3,
    space: 4,
    "main-1": 5,
    "letter-a": 6,
    "letter-x": 7,
    "keypad-insert-rub": 8,
    "keypad-vertical-arrows": 9,
    "keypad-horizontal-arrows": 10,
    "keypad-0": 11,
    "keypad-1": 12,
    "keypad-2": 13,
    "keypad-3": 14,
    "keypad-4": 15,
    "keypad-5": 16,
    "keypad-6": 17,
    "keypad-7": 18,
    break: 19,
    "home-cls": 20,
    "main-0": 21,
    "main-2": 22,
    "main-3": 23,
    "main-4": 24,
    "main-5": 25,
    "main-6": 26,
    "main-7": 27,
    "main-8": 28,
    "main-9": 29,
    "main-caret": 30,
    "letter-b": 31,
    "letter-c": 32,
    "letter-d": 33,
    "letter-e": 34,
    "letter-f": 35,
    "letter-g": 36,
    "letter-h": 37,
    "letter-i": 38,
    "letter-j": 39,
    "letter-k": 40,
    "letter-l": 41,
    "letter-m": 42,
    "letter-n": 43,
    "letter-o": 44,
    "letter-p": 45,
    "letter-q": 46,
    "letter-r": 47,
    "letter-s": 48,
    "letter-t": 49,
    "letter-u": 50,
    "letter-v": 51,
    "letter-w": 52,
    "letter-y": 53,
    "letter-z": 54,
    colon: 55,
    semicolon: 56,
    comma: 57,
    period: 58,
    "pf-1": 59,
    "pf-2": 60,
    "pf-3": 61,
    "pf-4": 62,
    "pf-5": 63,
    "pf-6": 64,
    "pf-7": 65,
    "pf-8": 66,
    "pf-9": 67,
    "pf-10": 68,
    "keypad-8": 69,
    "keypad-9": 70,
    "keypad-multiply": 71,
    "keypad-add": 72,
    "keypad-equal": 73,
    "keypad-subtract": 74,
    "keypad-decimal": 75,
    "keypad-divide": 76,
});

export const Jr800LcdIndicatorNames = Object.freeze([
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

const WORD_BYTES = 4;
const JR800_CONFIGURATION_WORDS = 32;
const STATE_WORDS = 21;
const STOP_WORDS = 24;
const HISTORY_WORDS = 33;
const ACCESS_WORDS = 11;
const ACCESS_FILTER_WORDS = 3;
const SOURCE_WORDS = 5;
const DISASSEMBLY_WORDS = 6;
const SUSPENDED_ADVANCE_WORDS = 5;
const STEP_OUT_STATE_WORDS = 3;
const EXPRESSION_WATCH_WORDS = 6;
const SYMBOL_WATCH_WORDS = 6;
const KEYBOARD_ACTIVITY_WORDS = 4;
const LCD_INDICATOR_RAW_WORDS = 2;
const NATIVE_PROGRAM_WAV_ISSUE_WORDS = 2;
const WASM_ABI_VERSION = 41;

function checkedWatchpointMode(mode) {
    if (typeof mode !== "string"
        || !Object.hasOwn(MemoryWatchpointMode, mode)) {
        throw new TypeError("Watchpoint mode must be read, write, or access");
    }
    return MemoryWatchpointMode[mode];
}

function checkedKeyboardKey(key) {
    if (typeof key !== "string" || !Object.hasOwn(Jr800KeyboardKey, key)) {
        throw new TypeError("Keyboard key is not supported");
    }
    return Jr800KeyboardKey[key];
}

function combine(low, high) {
    const value = (BigInt(high) << 32n) | BigInt(low);
    return value <= BigInt(Number.MAX_SAFE_INTEGER) ? Number(value) : value.toString();
}

function splitSafeInteger(value) {
    const bits = BigInt(value);
    return [
        Number(bits & 0xffff_ffffn),
        Number(bits >> 32n),
    ];
}

function checkedStepOutState(state) {
    if (state === null || typeof state !== "object") {
        throw new TypeError("Step-out state must be an object");
    }
    if (Object.keys(state).some(
        (key) => key !== "continued" && key !== "nestingDepth",
    )) {
        throw new TypeError("Step-out state contains an unknown field");
    }
    const continued = state.continued ?? false;
    const nestingDepth = state.nestingDepth ?? 0;
    if (typeof continued !== "boolean") {
        throw new TypeError("Step-out continued flag must be boolean");
    }
    if (!Number.isSafeInteger(nestingDepth) || nestingDepth < 0) {
        throw new RangeError("Step-out nesting depth must be a non-negative safe integer");
    }
    if (!continued && nestingDepth !== 0) {
        throw new RangeError("A new step-out operation must start at nesting depth zero");
    }
    return {continued, nestingDepth};
}

function asBytes(value) {
    if (value instanceof Uint8Array) {
        return value;
    }
    if (value instanceof ArrayBuffer) {
        return new Uint8Array(value);
    }
    if (ArrayBuffer.isView(value)) {
        return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
    }
    throw new TypeError("Expected binary data");
}

function checkedInputBytes(value) {
    const bytes = asBytes(value);
    if (bytes.byteLength === 0) {
        throw new TypeError("Binary input must not be empty");
    }
    if (bytes.byteLength > MAX_LINKED_BINARY_BYTES) {
        throw new RangeError(
            `Binary input exceeds ${MAX_LINKED_BINARY_BYTES} bytes`,
        );
    }
    return bytes;
}

function checkedJr8rom(value) {
    const bytes = asBytes(value);
    if (bytes.byteLength === 0) {
        throw new TypeError("JR8ROM input must not be empty");
    }
    if (bytes.byteLength > MAX_JR8ROM_BYTES) {
        throw new RangeError(
            `JR8ROM input exceeds ${MAX_JR8ROM_BYTES} bytes`,
        );
    }
    return bytes;
}

function checkedLogicalRom(value) {
    const bytes = checkedInputBytes(value);
    if (bytes.byteLength !== JR800_LOGICAL_ROM_BYTES) {
        throw new RangeError(
            `Logical ROM input must be exactly ${JR800_LOGICAL_ROM_BYTES} bytes`,
        );
    }
    return bytes;
}

function checkedByte(value, label) {
    if (!Number.isInteger(value) || value < 0 || value > 0xff) {
        throw new RangeError(`${label} must be a byte value`);
    }
    return value;
}

function checkedKnownBits(value, allowedMask, label) {
    if (value === undefined) {
        return undefined;
    }
    if (value === null || typeof value !== "object") {
        throw new TypeError(`${label} must contain value and knownMask`);
    }
    const pinValue = checkedByte(value.value, `${label} value`);
    const knownMask = checkedByte(value.knownMask, `${label} known mask`);
    if ((pinValue & ~allowedMask) !== 0 || (knownMask & ~allowedMask) !== 0) {
        throw new RangeError(`${label} exceeds its allowed mask`);
    }
    if ((pinValue & ~knownMask) !== 0) {
        throw new RangeError(`${label} value contains unknown pin bits`);
    }
    const keys = Object.keys(value);
    if (keys.some((key) => key !== "value" && key !== "knownMask")) {
        throw new TypeError(`${label} contains an unknown field`);
    }
    return Object.freeze({value: pinValue, knownMask});
}

export function normalizeJr800Configuration(configuration = {}) {
    if (configuration === null || typeof configuration !== "object") {
        throw new TypeError("JR-800 configuration must be an object");
    }
    const allowedKeys = new Set([
        "resetStackPointer",
        "resetIndexRegister",
        "resetAccumulatorA",
        "resetAccumulatorB",
        "resetConditionCode",
        "internalRamInitialValue",
        "standardRamInitialValue",
        "expansionRamInitialValue",
        "lcdUnknownDataReadValue",
        "calendarAddressSource",
        "calendarUpperRead",
        "calendarCpuCycleRatio",
        "port1Pins",
        "port2Pins",
        "ramStandbyPowerValid",
        "keyboardWindowValue",
        "ignoreUnsupportedIo",
    ]);
    if (Object.keys(configuration).some((key) => !allowedKeys.has(key))) {
        throw new TypeError("JR-800 configuration contains an unknown field");
    }
    if (configuration.ignoreUnsupportedIo !== undefined
        && typeof configuration.ignoreUnsupportedIo !== "boolean") {
        throw new TypeError("Ignore unsupported I/O must be boolean");
    }
    const optionalByte = (key, label) => configuration[key] === undefined
        ? undefined
        : checkedByte(configuration[key], label);
    const optionalWord = (key, label) => configuration[key] === undefined
        ? undefined
        : checkedUint16(configuration[key], label);
    const resetStackPointer = optionalWord(
        "resetStackPointer",
        "Reset stack pointer",
    );
    const resetIndexRegister = optionalWord(
        "resetIndexRegister",
        "Reset index register",
    );
    const resetAccumulatorA = optionalByte(
        "resetAccumulatorA",
        "Reset accumulator A",
    );
    const resetAccumulatorB = optionalByte(
        "resetAccumulatorB",
        "Reset accumulator B",
    );
    const resetConditionCode = configuration.resetConditionCode === undefined
        ? undefined
        : checkedKnownBits(
            configuration.resetConditionCode,
            0x2f,
            "Reset condition code",
        );
    const internalRamInitialValue = optionalByte(
        "internalRamInitialValue",
        "Internal RAM initial value",
    );
    const standardRamInitialValue = optionalByte(
        "standardRamInitialValue",
        "Standard RAM initial value",
    );
    const expansionRamInitialValue = optionalByte(
        "expansionRamInitialValue",
        "Expansion RAM initial value",
    );
    if (expansionRamInitialValue !== undefined
        && standardRamInitialValue === undefined) {
        throw new RangeError("Expansion RAM requires standard RAM");
    }
    const lcdUnknownDataReadValue = optionalByte(
        "lcdUnknownDataReadValue",
        "LCD unknown-data read value",
    );
    const keyboardWindowValue = optionalByte(
        "keyboardWindowValue",
        "Keyboard window value",
    );
    const calendarAddressSource = configuration.calendarAddressSource;
    const calendarUpperRead = configuration.calendarUpperRead;
    const calendarCpuCycleRatio = configuration.calendarCpuCycleRatio;
    if ((calendarAddressSource === undefined)
        !== (calendarUpperRead === undefined)) {
        throw new RangeError(
            "Calendar address source and upper-read mode must be supplied together",
        );
    }
    if (calendarAddressSource !== undefined
        && !Object.hasOwn(CALENDAR_ADDRESS_SOURCES, calendarAddressSource)) {
        throw new RangeError("Unknown calendar address source");
    }
    if (calendarUpperRead !== undefined
        && !Object.hasOwn(CALENDAR_UPPER_READ_BITS, calendarUpperRead)) {
        throw new RangeError("Unknown calendar upper-read mode");
    }
    if (calendarCpuCycleRatio !== undefined
        && calendarAddressSource === undefined) {
        throw new RangeError(
            "Calendar CPU-cycle ratio requires the calendar adapter",
        );
    }
    if (calendarCpuCycleRatio !== undefined
        && !Object.hasOwn(CALENDAR_CPU_CYCLE_RATIOS, calendarCpuCycleRatio)) {
        throw new RangeError("Unknown calendar CPU-cycle ratio");
    }
    if (configuration.ramStandbyPowerValid !== undefined
        && typeof configuration.ramStandbyPowerValid !== "boolean") {
        throw new TypeError("RAM standby power validity must be boolean");
    }
    return Object.freeze({
        resetStackPointer,
        resetIndexRegister,
        resetAccumulatorA,
        resetAccumulatorB,
        resetConditionCode,
        internalRamInitialValue,
        standardRamInitialValue,
        expansionRamInitialValue,
        lcdUnknownDataReadValue,
        calendarAddressSource,
        calendarUpperRead,
        calendarCpuCycleRatio,
        port1Pins: checkedKnownBits(
            configuration.port1Pins,
            0xff,
            "Port 1 pins",
        ),
        port2Pins: checkedKnownBits(
            configuration.port2Pins,
            0x1f,
            "Port 2 pins",
        ),
        ramStandbyPowerValid: configuration.ramStandbyPowerValid,
        keyboardWindowValue,
        ignoreUnsupportedIo: configuration.ignoreUnsupportedIo ?? false,
    });
}

function jr800ConfigurationWords(configuration) {
    const calendarEnabled = configuration.calendarAddressSource !== undefined;
    return Uint32Array.from([
        WASM_ABI_VERSION,
        configuration.resetStackPointer === undefined ? 0 : 1,
        configuration.resetStackPointer ?? 0,
        configuration.resetIndexRegister === undefined ? 0 : 1,
        configuration.resetIndexRegister ?? 0,
        configuration.resetAccumulatorA === undefined ? 0 : 1,
        configuration.resetAccumulatorA ?? 0,
        configuration.resetAccumulatorB === undefined ? 0 : 1,
        configuration.resetAccumulatorB ?? 0,
        configuration.resetConditionCode?.knownMask ?? 0,
        configuration.resetConditionCode?.value ?? 0,
        configuration.internalRamInitialValue === undefined ? 0 : 1,
        configuration.internalRamInitialValue ?? 0,
        configuration.standardRamInitialValue === undefined ? 0 : 1,
        configuration.standardRamInitialValue ?? 0,
        configuration.expansionRamInitialValue === undefined ? 0 : 1,
        configuration.expansionRamInitialValue ?? 0,
        configuration.lcdUnknownDataReadValue === undefined ? 0 : 1,
        configuration.lcdUnknownDataReadValue ?? 0,
        calendarEnabled ? 1 : 0,
        calendarEnabled
            ? CALENDAR_ADDRESS_SOURCES[configuration.calendarAddressSource]
            : 0,
        calendarEnabled
            ? CALENDAR_UPPER_READ_BITS[configuration.calendarUpperRead]
            : 0,
        calendarEnabled && configuration.calendarCpuCycleRatio !== undefined
            ? CALENDAR_CPU_CYCLE_RATIOS[configuration.calendarCpuCycleRatio]
            : 0,
        configuration.port1Pins?.knownMask ?? 0,
        configuration.port1Pins?.value ?? 0,
        configuration.port2Pins?.knownMask ?? 0,
        configuration.port2Pins?.value ?? 0,
        configuration.ramStandbyPowerValid === undefined ? 0 : 1,
        configuration.ramStandbyPowerValid ? 1 : 0,
        configuration.keyboardWindowValue === undefined ? 0 : 1,
        configuration.keyboardWindowValue ?? 0,
        configuration.ignoreUnsupportedIo ? 1 : 0,
    ]);
}

function checkedUint16(value, label) {
    if (!Number.isInteger(value) || value < 0 || value > 0xffff) {
        throw new RangeError(`${label} must be a uint16 value`);
    }
    return value;
}

function checkedUint32(value, label) {
    if (!Number.isInteger(value) || value < 0 || value > 0xffff_ffff) {
        throw new RangeError(`${label} must be a uint32 value`);
    }
    return value;
}

export function normalizeViewOptions(
    {
        memoryAddress = 0,
        memoryLength = 32,
        focusAddress,
        traceFilter,
    } = {},
) {
    const checkedMemoryAddress = checkedUint16(memoryAddress, "Memory address");
    if (!Number.isInteger(memoryLength) || memoryLength < 1
        || memoryLength > 0x1_0000 - checkedMemoryAddress) {
        throw new RangeError("Memory range is out of bounds");
    }
    return {
        memoryAddress: checkedMemoryAddress,
        memoryLength,
        focusAddress: focusAddress === undefined
            ? undefined
            : checkedUint16(focusAddress, "Focus address"),
        traceFilter: normalizeAccessTraceFilter(traceFilter),
    };
}

export function normalizeAccessTraceFilter(
    {
        firstAddress = 0,
        lastAddress = 0xffff,
        kindMask = AccessTraceMask.all,
    } = {},
) {
    const checkedFirst = checkedUint16(firstAddress, "Trace first address");
    const checkedLast = checkedUint16(lastAddress, "Trace last address");
    if (checkedFirst > checkedLast) {
        throw new RangeError("Trace address range is reversed");
    }
    if (!Number.isInteger(kindMask) || kindMask < 1
        || kindMask > AccessTraceMask.all) {
        throw new RangeError("Trace kind mask is invalid");
    }
    return {
        firstAddress: checkedFirst,
        lastAddress: checkedLast,
        kindMask,
    };
}

export class WasmApiError extends Error {
    constructor(operation, status) {
        const name = STATUS_NAMES[status] ?? `status-${status}`;
        super(`${operation} failed: ${name}`);
        this.name = "WasmApiError";
        this.operation = operation;
        this.status = status;
        this.statusName = name;
    }
}

export class NativeProgramWavError extends WasmApiError {
    constructor(status, issueCode, burstIndex) {
        super("load-native-program-wav", status);
        this.name = "NativeProgramWavError";
        this.issueCode = issueCode;
        this.issue = NATIVE_PROGRAM_WAV_ISSUE_NAMES[issueCode]
            ?? `issue-${issueCode}`;
        this.burstIndex = burstIndex;
        this.message = `WAV conversion failed: ${this.issue}`;
    }
}

export class WasmMachine {
    static async create(moduleUrl) {
        const imported = await import(moduleUrl);
        if (typeof imported.default !== "function") {
            throw new Error("WASM module factory is missing");
        }
        const module = await imported.default();
        return new WasmMachine(module);
    }

    static async createJr800(moduleUrl, configuration = {}) {
        const imported = await import(moduleUrl);
        if (typeof imported.default !== "function") {
            throw new Error("WASM module factory is missing");
        }
        const module = await imported.default();
        return new WasmMachine(module, {
            jr800Configuration: configuration,
        });
    }

    constructor(module, {jr800Configuration} = {}) {
        this.module = module;
        this.decoder = new TextDecoder();
        this.encoder = new TextEncoder();
        this.functions = this.#bindFunctions();
        const abiVersion = this.functions.abiVersion();
        if (abiVersion !== WASM_ABI_VERSION) {
            throw new Error(`Unsupported WASM ABI version: ${abiVersion}`);
        }
        const normalizedConfiguration = jr800Configuration === undefined
            ? undefined
            : normalizeJr800Configuration(jr800Configuration);
        this.kind = normalizedConfiguration === undefined ? "synthetic" : "jr800";
        this.jr800Configuration = normalizedConfiguration;
        this.handle = normalizedConfiguration === undefined
            ? this.functions.create()
            : this.#allocate(
                JR800_CONFIGURATION_WORDS * WORD_BYTES,
                (pointer) => {
                    this.module.HEAPU32.set(
                        jr800ConfigurationWords(normalizedConfiguration),
                        pointer / WORD_BYTES,
                    );
                    return this.functions.createJr800(pointer);
                },
            );
        if (!this.handle) {
            throw new Error("Machine creation failed");
        }
    }

    #bindFunctions() {
        const bind = (name, result, arguments_) =>
            this.module.cwrap(name, result, arguments_);
        return {
            abiVersion: bind("jr800_machine_abi_version", "number", []),
            create: bind("jr800_machine_create", "number", []),
            createJr800: bind(
                "jr800_machine_create_jr800",
                "number",
                ["number"],
            ),
            destroy: bind("jr800_machine_destroy", null, ["number"]),
            loadApplication: bind(
                "jr800_machine_load_application",
                "number",
                ["number", "number", "number", "number"],
            ),
            loadDebugInfo: bind(
                "jr800_machine_load_debug_info",
                "number",
                ["number", "number", "number"],
            ),
            loadLogicalRom: bind(
                "jr800_machine_load_logical_rom",
                "number",
                ["number", "number", "number"],
            ),
            loadJr8rom: bind(
                "jr800_machine_load_jr8rom",
                "number",
                ["number", "number", "number"],
            ),
            loadProgram: bind(
                "jr800_machine_load_program",
                "number",
                ["number", "number", "number", "number", "number"],
            ),
            loadNativeProgramWav: bind(
                "jr800_machine_load_native_program_wav",
                "number",
                ["number", "number", "number", "number", "number", "number"],
            ),
            getProgramSaves: bind("jr800_machine_get_program_saves", "number", ["number", "number"]),
            getSavedProgramInfo: bind("jr800_machine_get_saved_program_info", "number", ["number", "number", "number"]),
            exportSavedProgram: bind("jr800_machine_export_saved_program", "number", ["number", "number", "number", "number", "number", "number"]),
            clearProgramSaves: bind("jr800_machine_clear_program_saves", "number", ["number"]),
            reset: bind("jr800_machine_reset", "number", ["number"]),
            getState: bind("jr800_machine_get_state", "number", ["number", "number"]),
            step: bind("jr800_machine_step", "number", ["number", "number"]),
            stepOver: bind(
                "jr800_machine_step_over",
                "number",
                ["number", "number", "number"],
            ),
            stepOut: bind(
                "jr800_machine_step_out",
                "number",
                ["number", "number", "number", "number"],
            ),
            run: bind("jr800_machine_run", "number", ["number", "number", "number"]),
            runTo: bind(
                "jr800_machine_run_to",
                "number",
                ["number", "number", "number", "number"],
            ),
            advanceSuspendedCycles: bind(
                "jr800_machine_advance_suspended_cycles",
                "number",
                ["number", "number", "number"],
            ),
            advanceCalendarOscillatorTicks: bind(
                "jr800_machine_advance_calendar_oscillator_ticks",
                "number",
                ["number", "number"],
            ),
            adjustCalendarSeconds: bind(
                "jr800_machine_adjust_calendar_seconds",
                "number",
                ["number"],
            ),
            setCalendarDateTime: bind(
                "jr800_machine_set_calendar_datetime",
                "number",
                ["number", "number"],
            ),
            setKeyboardBusResponse: bind(
                "jr800_machine_set_keyboard_bus_response",
                "number",
                ["number", "number", "number", "number"],
            ),
            setKeyboardKeyState: bind(
                "jr800_machine_set_keyboard_key_state",
                "number",
                ["number", "number", "number"],
            ),
            getKeyboardActivity: bind(
                "jr800_machine_get_keyboard_activity",
                "number",
                ["number", "number"],
            ),
            clearKeyboardActivity: bind(
                "jr800_machine_clear_keyboard_activity",
                "number",
                ["number"],
            ),
            setExecutionBreakpoint: bind(
                "jr800_machine_set_execution_breakpoint",
                "number",
                ["number", "number", "number"],
            ),
            setConditionalExecutionBreakpoint: bind(
                "jr800_machine_set_conditional_execution_breakpoint",
                "number",
                ["number", "number", "number", "number"],
            ),
            setExpressionWatch: bind(
                "jr800_machine_set_expression_watch",
                "number",
                ["number", "number", "number", "number"],
            ),
            clearExpressionWatch: bind(
                "jr800_machine_clear_expression_watch",
                "number",
                ["number", "number"],
            ),
            evaluateExpressionWatch: bind(
                "jr800_machine_evaluate_expression_watch",
                "number",
                ["number", "number", "number"],
            ),
            setSymbolWatch: bind(
                "jr800_machine_set_symbol_watch",
                "number",
                ["number", "number", "number", "number"],
            ),
            clearSymbolWatch: bind(
                "jr800_machine_clear_symbol_watch",
                "number",
                ["number", "number"],
            ),
            evaluateSymbolWatch: bind(
                "jr800_machine_evaluate_symbol_watch",
                "number",
                ["number", "number", "number"],
            ),
            setMemoryWatchpoint: bind(
                "jr800_machine_set_memory_watchpoint",
                "number",
                ["number", "number", "number", "number"],
            ),
            historyCount: bind("jr800_machine_history_count", "number", ["number"]),
            copyHistory: bind(
                "jr800_machine_copy_history",
                "number",
                ["number", "number", "number"],
            ),
            accessCount: bind(
                "jr800_machine_access_count",
                "number",
                ["number", "number", "number"],
            ),
            copyAccesses: bind(
                "jr800_machine_copy_accesses",
                "number",
                ["number", "number", "number", "number", "number"],
            ),
            clearHistory: bind("jr800_machine_clear_history", null, ["number"]),
            readMemory: bind(
                "jr800_machine_read_memory",
                "number",
                ["number", "number", "number", "number"],
            ),
            copyLcdPanel: bind(
                "jr800_machine_copy_lcd_panel",
                "number",
                ["number", "number", "number"],
            ),
            copyLcdIndicators: bind(
                "jr800_machine_copy_lcd_indicators",
                "number",
                ["number", "number", "number"],
            ),
            sourceAt: bind(
                "jr800_machine_source_at",
                "number",
                ["number", "number", "number"],
            ),
            sourceAddress: bind(
                "jr800_machine_source_address",
                "number",
                ["number", "number", "number", "number", "number"],
            ),
            symbolAddress: bind(
                "jr800_machine_symbol_address",
                "number",
                ["number", "number", "number", "number"],
            ),
            sourcePathSize: bind(
                "jr800_machine_source_path_size",
                "number",
                ["number", "number"],
            ),
            copySourcePath: bind(
                "jr800_machine_copy_source_path",
                "number",
                ["number", "number", "number", "number"],
            ),
            disassemble: bind(
                "jr800_machine_disassemble",
                "number",
                ["number", "number", "number"],
            ),
            disassemblyTextSize: bind(
                "jr800_machine_disassembly_text_size",
                "number",
                ["number", "number"],
            ),
            copyDisassemblyText: bind(
                "jr800_machine_copy_disassembly_text",
                "number",
                ["number", "number", "number", "number"],
            ),
        };
    }

    #requireHandle() {
        if (!this.handle) {
            throw new Error("Machine has been destroyed");
        }
    }

    #check(operation, status) {
        if (status !== Status.ok) {
            throw new WasmApiError(operation, status);
        }
    }

    #allocate(byteLength, callback) {
        const pointer = this.module._malloc(Math.max(byteLength, 1));
        if (!pointer) {
            throw new Error("WASM allocation failed");
        }
        try {
            return callback(pointer);
        } finally {
            this.module._free(pointer);
        }
    }

    #withInput(binary, callback) {
        const bytes = checkedInputBytes(binary);
        return this.#allocate(bytes.byteLength, (pointer) => {
            this.module.HEAPU8.set(bytes, pointer);
            return callback(pointer, bytes.byteLength);
        });
    }

    #readWords(pointer, count) {
        const first = pointer / WORD_BYTES;
        return this.module.HEAPU32.slice(first, first + count);
    }

    #readText(pointer, size) {
        const bytes = this.module.HEAPU8.slice(pointer, pointer + Math.max(size - 1, 0));
        return this.decoder.decode(bytes);
    }

    #copyText(size, operation, callback) {
        if (size === 0) {
            throw new WasmApiError(operation, Status.notFound);
        }
        return this.#allocate(size, (pointer) => {
            this.#check(operation, callback(pointer, size));
            return this.#readText(pointer, size);
        });
    }

    #loadApplication(binary, initialStackPointer) {
        this.#withInput(binary, (pointer, size) => {
            this.#check(
                "load-application",
                this.functions.loadApplication(
                    this.handle,
                    pointer,
                    size,
                    initialStackPointer,
                ),
            );
        });
    }

    #loadDebugInfo(binary) {
        this.#withInput(binary, (pointer, size) => {
            this.#check(
                "load-debug-info",
                this.functions.loadDebugInfo(this.handle, pointer, size),
            );
        });
    }

    #loadJr8rom(binary) {
        this.#withInput(binary, (pointer, size) => {
            this.#check(
                "load-jr8rom",
                this.functions.loadJr8rom(this.handle, pointer, size),
            );
        });
    }

    #loadLogicalRom(binary) {
        this.#withInput(binary, (pointer, size) => {
            this.#check(
                "load-logical-rom",
                this.functions.loadLogicalRom(this.handle, pointer, size),
            );
        });
    }

    #loadHardwareProgram(binary, runAfterLoad, wav) {
        if (typeof runAfterLoad !== "boolean") throw new TypeError("Program run option must be boolean");
        return this.#withInput(binary, (pointer, size) => this.#allocate(36, (scratch) => {
            this.module.HEAPU8.fill(0, scratch, scratch + 36);
            const infoPointer = scratch + 8;
            const status = wav
                ? this.functions.loadNativeProgramWav(this.handle, pointer, size, scratch, Number(runAfterLoad), infoPointer)
                : this.functions.loadProgram(this.handle, pointer, size, Number(runAfterLoad), infoPointer);
            if (status === Status.invalidNativeProgramWav) {
                const issue = this.#readWords(scratch, NATIVE_PROGRAM_WAV_ISSUE_WORDS);
                throw new NativeProgramWavError(status, issue[0], issue[1]);
            }
            this.#check(wav ? "load-native-program-wav" : "load-program", status);
            const info = this.#readWords(infoPointer, 3);
            return {
                kind: ["unknown", "machine-code", "basic-text", "basic-binary"][info[0]],
                byteLength: info[1],
                nameBytes: Array.from(this.module.HEAPU8.slice(infoPointer + 12, infoPointer + 12 + info[2])),
            };
        }));
    }

    load(application, {debugInfo, initialStackPointer = 0x01ff, view} = {}) {
        this.#requireHandle();
        if (this.kind !== "synthetic") {
            throw new WasmApiError(
                "load-application",
                Status.wrongMachineKind,
            );
        }
        const applicationBytes = checkedInputBytes(application);
        const stackPointer = checkedUint16(initialStackPointer, "Initial stack pointer");
        const debugBytes = debugInfo === undefined || debugInfo === null
            ? undefined
            : checkedInputBytes(debugInfo);
        const candidate = new WasmMachine(this.module);
        try {
            candidate.#loadApplication(applicationBytes, stackPointer);
            if (debugBytes !== undefined) {
                candidate.#loadDebugInfo(debugBytes);
            }
            const snapshot = candidate.snapshot(view);
            const previousHandle = this.handle;
            this.handle = candidate.handle;
            candidate.handle = previousHandle;
            return snapshot;
        } finally {
            candidate.destroy();
        }
    }

    loadJr8rom(container, {view} = {}) {
        this.#requireHandle();
        if (this.kind !== "jr800") {
            throw new WasmApiError(
                "load-jr8rom",
                Status.wrongMachineKind,
            );
        }
        const containerBytes = checkedJr8rom(container);
        const normalizedView = normalizeViewOptions(
            view ?? {memoryAddress: 0x8000},
        );
        const candidate = new WasmMachine(this.module, {
            jr800Configuration: this.jr800Configuration,
        });
        try {
            candidate.#loadJr8rom(containerBytes);
            const snapshot = candidate.snapshot(normalizedView);
            const previousHandle = this.handle;
            this.handle = candidate.handle;
            candidate.handle = previousHandle;
            return snapshot;
        } finally {
            candidate.destroy();
        }
    }

    loadLogicalRom(rawRom, {view} = {}) {
        this.#requireHandle();
        if (this.kind !== "jr800") {
            throw new WasmApiError(
                "load-logical-rom",
                Status.wrongMachineKind,
            );
        }
        const romBytes = checkedLogicalRom(rawRom);
        const normalizedView = normalizeViewOptions(
            view ?? {memoryAddress: 0x8000},
        );
        const candidate = new WasmMachine(this.module, {
            jr800Configuration: this.jr800Configuration,
        });
        try {
            candidate.#loadLogicalRom(romBytes);
            const snapshot = candidate.snapshot(normalizedView);
            const previousHandle = this.handle;
            this.handle = candidate.handle;
            candidate.handle = previousHandle;
            return snapshot;
        } finally {
            candidate.destroy();
        }
    }

    loadProgram(application, {view, runAfterLoad = true} = {}) {
        this.#requireHandle();
        if (this.kind !== "jr800") {
            throw new WasmApiError(
                "load-program",
                Status.wrongMachineKind,
            );
        }
        const applicationBytes = checkedInputBytes(application);
        const program = this.#loadHardwareProgram(applicationBytes, runAfterLoad, false);
        return {...this.snapshot(view), program};
    }

    loadNativeProgramWav(wav, {view, runAfterLoad = true} = {}) {
        this.#requireHandle();
        if (this.kind !== "jr800") {
            throw new WasmApiError(
                "load-native-program-wav",
                Status.wrongMachineKind,
            );
        }
        const wavBytes = checkedInputBytes(wav);
        const program = this.#loadHardwareProgram(wavBytes, runAfterLoad, true);
        return {...this.snapshot(view), program};
    }

    reset() {
        this.#requireHandle();
        this.#check("reset", this.functions.reset(this.handle));
    }

    state() {
        this.#requireHandle();
        return this.#allocate(STATE_WORDS * WORD_BYTES, (pointer) => {
            this.#check("get-state", this.functions.getState(this.handle, pointer));
            const words = this.#readWords(pointer, STATE_WORDS);
            return {
                abiVersion: words[0],
                profile: PROFILE_NAMES[words[1]] ?? `profile-${words[1]}`,
                pc: words[2],
                sp: words[3],
                x: words[4],
                a: words[5],
                b: words[6],
                conditionCode: words[7],
                executionState: EXECUTION_STATE_NAMES[words[8]]
                    ?? `execution-state-${words[8]}`,
                cycleCount: combine(words[9], words[10]),
                registerKnownMask: words[11],
                conditionCodeKnownMask: words[12],
                calendarAlarmTerminal:
                    CALENDAR_ALARM_TERMINAL_NAMES[words[13]]
                    ?? `calendar-alarm-terminal-${words[13]}`,
                port2TimerOutput:
                    PORT2_TIMER_OUTPUT_NAMES[words[14]]
                    ?? `port2-timer-output-${words[14]}`,
                lcdSubstitutedDataReadCount: words[15] === 0
                    ? null
                    : combine(words[16], words[17]),
                ignoredIoAccessCount: words[18] === 0
                    ? null
                    : combine(words[19], words[20]),
            };
        });
    }

    #stop(callback, operation) {
        return this.#allocate(STOP_WORDS * WORD_BYTES, (pointer) => {
            this.#check(operation, callback(pointer));
            const words = this.#readWords(pointer, STOP_WORDS);
            return {
                reason: STOP_NAMES[words[0]] ?? `stop-${words[0]}`,
                fault: FAULT_NAMES[words[1]] ?? `fault-${words[1]}`,
                triggerAddress: words[2],
                triggerAccess: words[3] === 0
                    ? null
                    : ACCESS_NAMES[words[4]] ?? `access-${words[4]}`,
                instructionsExecuted: combine(words[5], words[6]),
                pcBefore: words[7],
                pcAfter: words[8],
                bytes: [words[9], words[10], words[11]],
                instructionLength: words[12],
                bytesFetched: words[13],
                cycles: words[14],
                busFault: BUS_FAULT_NAMES[words[15]]
                    ?? `bus-fault-${words[15]}`,
                faultAccess: ACCESS_NAMES[words[16]]
                    ?? `access-${words[16]}`,
                stateFault: STATE_PART_NAMES[words[17]]
                    ?? `state-part-${words[17]}`,
                stepKind: STEP_KIND_NAMES[words[18]]
                    ?? `step-kind-${words[18]}`,
                interruptSource: INTERRUPT_NAMES[words[19]]
                    ?? `interrupt-${words[19]}`,
                continuationAddress: words[20] === 0 ? null : words[21],
                conditionError: EXPRESSION_ERROR_NAMES[words[22]]
                    ?? `expression-error-${words[22]}`,
                conditionFaultAddress: words[23],
            };
        });
    }

    step() {
        this.#requireHandle();
        return this.#stop(
            (pointer) => this.functions.step(this.handle, pointer),
            "step",
        );
    }

    stepOver(instructionLimit) {
        this.#requireHandle();
        if (!Number.isInteger(instructionLimit) || instructionLimit < 1
            || instructionLimit > 0xffff_ffff) {
            throw new RangeError("Instruction limit must be a uint32 value");
        }
        return this.#stop(
            (pointer) => this.functions.stepOver(
                this.handle,
                instructionLimit,
                pointer,
            ),
            "step-over",
        );
    }

    stepOut(
        instructionLimit,
        state = {continued: false, nestingDepth: 0},
    ) {
        this.#requireHandle();
        if (!Number.isInteger(instructionLimit) || instructionLimit < 1
            || instructionLimit > 0xffff_ffff) {
            throw new RangeError("Instruction limit must be a uint32 value");
        }
        const normalizedState = checkedStepOutState(state);
        const [depthLow, depthHigh] = splitSafeInteger(
            normalizedState.nestingDepth,
        );
        return this.#allocate(
            STEP_OUT_STATE_WORDS * WORD_BYTES,
            (statePointer) => {
                this.module.HEAPU32.set(
                    Uint32Array.from([
                        normalizedState.continued ? 1 : 0,
                        depthLow,
                        depthHigh,
                    ]),
                    statePointer / WORD_BYTES,
                );
                const stop = this.#stop(
                    (stopPointer) => this.functions.stepOut(
                        this.handle,
                        instructionLimit,
                        statePointer,
                        stopPointer,
                    ),
                    "step-out",
                );
                const words = this.#readWords(
                    statePointer,
                    STEP_OUT_STATE_WORDS,
                );
                const nestingDepth = combine(words[1], words[2]);
                if (typeof nestingDepth !== "number") {
                    throw new RangeError("Step-out nesting depth exceeds JavaScript's safe integer range");
                }
                return {
                    stop,
                    state: {
                        continued: words[0] !== 0,
                        nestingDepth,
                    },
                };
            },
        );
    }

    run(instructionLimit) {
        this.#requireHandle();
        if (!Number.isInteger(instructionLimit) || instructionLimit < 1
            || instructionLimit > 0xffff_ffff) {
            throw new RangeError("Instruction limit must be a uint32 value");
        }
        return this.#stop(
            (pointer) => this.functions.run(this.handle, instructionLimit, pointer),
            "run",
        );
    }

    runTo(address, instructionLimit) {
        this.#requireHandle();
        const checkedAddress = checkedUint16(address, "Run-to address");
        if (!Number.isInteger(instructionLimit) || instructionLimit < 1
            || instructionLimit > 0xffff_ffff) {
            throw new RangeError("Instruction limit must be a uint32 value");
        }
        return this.#stop(
            (pointer) => this.functions.runTo(
                this.handle,
                checkedAddress,
                instructionLimit,
                pointer,
            ),
            "run-to",
        );
    }

    advanceSuspendedCycles(cycleLimit) {
        this.#requireHandle();
        if (!Number.isInteger(cycleLimit) || cycleLimit < 1
            || cycleLimit > 0xffff_ffff) {
            throw new RangeError("Suspended cycle limit must be a uint32 value");
        }
        return this.#allocate(
            SUSPENDED_ADVANCE_WORDS * WORD_BYTES,
            (pointer) => {
                this.#check(
                    "advance-suspended-cycles",
                    this.functions.advanceSuspendedCycles(
                        this.handle,
                        cycleLimit,
                        pointer,
                    ),
                );
                const words = this.#readWords(
                    pointer,
                    SUSPENDED_ADVANCE_WORDS,
                );
                return {
                    suspended: words[0] !== 0,
                    cyclesElapsed: words[1],
                    interruptKnown: words[2] !== 0,
                    interruptSource: INTERRUPT_NAMES[words[3]]
                        ?? `interrupt-${words[3]}`,
                    busFault: BUS_FAULT_NAMES[words[4]]
                        ?? `bus-fault-${words[4]}`,
                };
            },
        );
    }

    advanceCalendarOscillatorTicks(ticks) {
        this.#requireHandle();
        const checkedTicks = checkedUint32(
            ticks,
            "Calendar oscillator ticks",
        );
        this.#check(
            "advance-calendar-oscillator",
            this.functions.advanceCalendarOscillatorTicks(
                this.handle,
                checkedTicks,
            ),
        );
    }

    setCalendarDateTime(value) {
        this.#requireHandle();
        const keys = ["year", "month", "day", "hour", "minute", "second"];
        if (!value || Object.keys(value).length !== keys.length
            || keys.some((key) => !Number.isInteger(value[key])
                || value[key] < 0 || value[key] > 0xffff_ffff)) {
            throw new TypeError("Calendar datetime requires six unsigned integer fields");
        }
        this.#allocate(keys.length * WORD_BYTES, (pointer) => {
            this.module.HEAPU32.set(
                Uint32Array.from(keys.map((key) => value[key])), pointer / WORD_BYTES,
            );
            this.#check("set-calendar-datetime",
                this.functions.setCalendarDateTime(this.handle, pointer));
        });
    }

    adjustCalendarSeconds() {
        this.#requireHandle();
        this.#check(
            "adjust-calendar-seconds",
            this.functions.adjustCalendarSeconds(this.handle),
        );
    }

    setKeyboardBusResponse(address, value, known = true) {
        this.#requireHandle();
        const checkedAddress = checkedUint16(address, "Keyboard address");
        const checkedValue = checkedByte(value, "Keyboard response");
        if (typeof known !== "boolean") {
            throw new TypeError("Keyboard response knownness must be boolean");
        }
        if (!known && checkedValue !== 0) {
            throw new RangeError("Unknown keyboard response value must be zero");
        }
        this.#check(
            "set-keyboard-bus-response",
            this.functions.setKeyboardBusResponse(
                this.handle,
                checkedAddress,
                checkedValue,
                known ? 1 : 0,
            ),
        );
    }

    setKeyboardKeyState(key, pressed) {
        this.#requireHandle();
        const checkedKey = checkedKeyboardKey(key);
        if (typeof pressed !== "boolean") {
            throw new TypeError("Keyboard pressed state must be boolean");
        }
        this.#check(
            "set-keyboard-key-state",
            this.functions.setKeyboardKeyState(
                this.handle,
                checkedKey,
                pressed ? 1 : 0,
            ),
        );
    }

    keyboardActivity() {
        this.#requireHandle();
        if (this.kind !== "jr800") {
            return null;
        }
        return this.#allocate(
            KEYBOARD_ACTIVITY_WORDS * WORD_BYTES,
            (pointer) => {
                this.#check(
                    "get-keyboard-activity",
                    this.functions.getKeyboardActivity(this.handle, pointer),
                );
                const words = this.#readWords(
                    pointer,
                    KEYBOARD_ACTIVITY_WORDS,
                );
                return {
                    readAttempts: combine(words[0], words[1]),
                    distinctAddresses: combine(words[2], words[3]),
                };
            },
        );
    }

    clearKeyboardActivity() {
        this.#requireHandle();
        this.#check(
            "clear-keyboard-activity",
            this.functions.clearKeyboardActivity(this.handle),
        );
    }

    setExecutionBreakpoint(address, enabled, condition = "") {
        this.#requireHandle();
        const checkedAddress = checkedUint16(address, "Breakpoint address");
        if (typeof enabled !== "boolean") {
            throw new TypeError("Breakpoint enabled state must be boolean");
        }
        if (typeof condition !== "string" || condition.includes("\0")) {
            throw new TypeError("Breakpoint condition must be text without NUL");
        }
        if (enabled && condition.length !== 0) {
            const bytes = this.encoder.encode(condition);
            this.#withInput(bytes, (pointer, size) => this.#check(
                "set-conditional-execution-breakpoint",
                this.functions.setConditionalExecutionBreakpoint(
                    this.handle,
                    checkedAddress,
                    pointer,
                    size,
                ),
            ));
            return;
        }
        this.#check(
            "set-execution-breakpoint",
            this.functions.setExecutionBreakpoint(
                this.handle,
                checkedAddress,
                enabled ? 1 : 0,
            ),
        );
    }

    setExpressionWatch(watchId, expression) {
        this.#requireHandle();
        const checkedId = checkedUint32(watchId, "Expression watch ID");
        if (typeof expression !== "string" || expression.length === 0
            || expression.includes("\0")) {
            throw new TypeError(
                "Expression watch must be nonempty text without NUL",
            );
        }
        const bytes = this.encoder.encode(expression);
        this.#withInput(bytes, (pointer, size) => this.#check(
            "set-expression-watch",
            this.functions.setExpressionWatch(
                this.handle,
                checkedId,
                pointer,
                size,
            ),
        ));
    }

    clearExpressionWatch(watchId) {
        this.#requireHandle();
        this.#check(
            "clear-expression-watch",
            this.functions.clearExpressionWatch(
                this.handle,
                checkedUint32(watchId, "Expression watch ID"),
            ),
        );
    }

    evaluateExpressionWatch(watchId) {
        this.#requireHandle();
        const checkedId = checkedUint32(watchId, "Expression watch ID");
        return this.#allocate(
            EXPRESSION_WATCH_WORDS * WORD_BYTES,
            (pointer) => {
                this.#check(
                    "evaluate-expression-watch",
                    this.functions.evaluateExpressionWatch(
                        this.handle,
                        checkedId,
                        pointer,
                    ),
                );
                const words = this.#readWords(
                    pointer,
                    EXPRESSION_WATCH_WORDS,
                );
                return {
                    value: combine(words[0], words[1]),
                    error: EXPRESSION_ERROR_NAMES[words[2]]
                        ?? `expression-error-${words[2]}`,
                    busFault: BUS_FAULT_NAMES[words[3]]
                        ?? `bus-fault-${words[3]}`,
                    faultAddress: words[4],
                    stateFault: STATE_PART_NAMES[words[5]]
                        ?? `state-part-${words[5]}`,
                };
            },
        );
    }

    setSymbolWatch(watchId, symbolName) {
        this.#requireHandle();
        const checkedId = checkedUint32(watchId, "Symbol watch ID");
        if (typeof symbolName !== "string" || symbolName.length === 0
            || symbolName.includes("\0")) {
            throw new TypeError("Symbol watch name must be nonempty text without NUL");
        }
        const bytes = this.encoder.encode(symbolName);
        this.#withInput(bytes, (pointer, size) => this.#check(
            "set-symbol-watch",
            this.functions.setSymbolWatch(
                this.handle,
                checkedId,
                pointer,
                size,
            ),
        ));
    }

    clearSymbolWatch(watchId) {
        this.#requireHandle();
        this.#check(
            "clear-symbol-watch",
            this.functions.clearSymbolWatch(
                this.handle,
                checkedUint32(watchId, "Symbol watch ID"),
            ),
        );
    }

    evaluateSymbolWatch(watchId) {
        this.#requireHandle();
        const checkedId = checkedUint32(watchId, "Symbol watch ID");
        return this.#allocate(
            SYMBOL_WATCH_WORDS * WORD_BYTES,
            (pointer) => {
                this.#check(
                    "evaluate-symbol-watch",
                    this.functions.evaluateSymbolWatch(
                        this.handle,
                        checkedId,
                        pointer,
                    ),
                );
                const words = this.#readWords(pointer, SYMBOL_WATCH_WORDS);
                return {
                    value: words[0],
                    binding: SYMBOL_BINDING_NAMES[words[1]]
                        ?? `symbol-binding-${words[1]}`,
                    kind: SYMBOL_KIND_NAMES[words[2]]
                        ?? `symbol-kind-${words[2]}`,
                    size: words[3],
                    sourceFileIndex: words[4] === 0 ? null : words[5],
                };
            },
        );
    }

    setMemoryWatchpoint(address, mode, enabled) {
        this.#requireHandle();
        const checkedAddress = checkedUint16(address, "Watchpoint address");
        const checkedMode = checkedWatchpointMode(mode);
        this.#check(
            "set-memory-watchpoint",
            this.functions.setMemoryWatchpoint(
                this.handle,
                checkedAddress,
                checkedMode,
                enabled ? 1 : 0,
            ),
        );
    }

    history() {
        this.#requireHandle();
        const count = this.functions.historyCount(this.handle);
        if (count === 0) {
            return [];
        }
        return this.#allocate(count * HISTORY_WORDS * WORD_BYTES, (pointer) => {
            const copied = this.functions.copyHistory(this.handle, pointer, count);
            if (copied !== count) {
                throw new Error("History copy was incomplete");
            }
            const words = this.#readWords(pointer, count * HISTORY_WORDS);
            const result = [];
            for (let index = 0; index < count; ++index) {
                const at = index * HISTORY_WORDS;
                result.push({
                    sequence: combine(words[at], words[at + 1]),
                    cycleBegin: combine(words[at + 2], words[at + 3]),
                    firstAccessSequence: combine(words[at + 4], words[at + 5]),
                    accessCount: words[at + 6],
                    pcBefore: words[at + 7],
                    pcAfter: words[at + 8],
                    bytes: [words[at + 9], words[at + 10], words[at + 11]],
                    instructionLength: words[at + 12],
                    bytesFetched: words[at + 13],
                    cycles: words[at + 14],
                    fault: FAULT_NAMES[words[at + 15]] ?? `fault-${words[at + 15]}`,
                    busFault: BUS_FAULT_NAMES[words[at + 25]]
                        ?? `bus-fault-${words[at + 25]}`,
                    faultAddress: words[at + 26],
                    faultAccess: ACCESS_NAMES[words[at + 27]]
                        ?? `access-${words[at + 27]}`,
                    stateFault: STATE_PART_NAMES[words[at + 28]]
                        ?? `state-part-${words[at + 28]}`,
                    stepKind: STEP_KIND_NAMES[words[at + 29]]
                        ?? `step-kind-${words[at + 29]}`,
                    interruptSource: INTERRUPT_NAMES[words[at + 30]]
                        ?? `interrupt-${words[at + 30]}`,
                    stateAfter: {
                        pc: words[at + 16],
                        sp: words[at + 17],
                        x: words[at + 18],
                        a: words[at + 19],
                        b: words[at + 20],
                        conditionCode: words[at + 21],
                        executionState:
                            EXECUTION_STATE_NAMES[words[at + 22]]
                                ?? `execution-state-${words[at + 22]}`,
                        cycleCount: combine(words[at + 23], words[at + 24]),
                        registerKnownMask: words[at + 31],
                        conditionCodeKnownMask: words[at + 32],
                    },
                });
            }
            return result;
        });
    }

    accesses(filter = {}) {
        this.#requireHandle();
        const normalized = normalizeAccessTraceFilter(filter);
        return this.#allocate(
            ACCESS_FILTER_WORDS * WORD_BYTES,
            (filterPointer) => {
                this.module.HEAPU32.set(
                    new Uint32Array([
                        normalized.firstAddress,
                        normalized.lastAddress,
                        normalized.kindMask,
                    ]),
                    filterPointer / WORD_BYTES,
                );
                return this.#allocate(WORD_BYTES, (countPointer) => {
                    this.#check(
                        "access-count",
                        this.functions.accessCount(
                            this.handle,
                            filterPointer,
                            countPointer,
                        ),
                    );
                    const count = this.#readWords(countPointer, 1)[0];
                    if (count === 0) {
                        return [];
                    }
                    return this.#allocate(
                        count * ACCESS_WORDS * WORD_BYTES,
                        (recordsPointer) => {
                            this.#check(
                                "copy-accesses",
                                this.functions.copyAccesses(
                                    this.handle,
                                    filterPointer,
                                    recordsPointer,
                                    count,
                                    countPointer,
                                ),
                            );
                            const copied = this.#readWords(countPointer, 1)[0];
                            if (copied !== count) {
                                throw new Error("Access copy was incomplete");
                            }
                            const words = this.#readWords(
                                recordsPointer,
                                count * ACCESS_WORDS,
                            );
                            const result = [];
                            for (let index = 0; index < count; ++index) {
                                const at = index * ACCESS_WORDS;
                                result.push({
                                    sequence: combine(words[at], words[at + 1]),
                                    instructionCycle: combine(
                                        words[at + 2],
                                        words[at + 3],
                                    ),
                                    instructionPc: words[at + 4],
                                    address: words[at + 5],
                                    value: words[at + 6],
                                    valueKnown: words[at + 7] !== 0,
                                    previousValue: words[at + 8],
                                    previousValueKnown: words[at + 9] !== 0,
                                    kind: ACCESS_NAMES[words[at + 10]]
                                        ?? `access-${words[at + 10]}`,
                                });
                            }
                            return result;
                        },
                    );
                });
            },
        );
    }

    clearHistory() {
        this.#requireHandle();
        this.functions.clearHistory(this.handle);
    }

    memory(address, byteLength) {
        this.#requireHandle();
        const checkedAddress = checkedUint16(address, "Memory address");
        if (!Number.isInteger(byteLength) || byteLength < 1
            || byteLength > 0x1_0000 - checkedAddress) {
            throw new RangeError("Memory range is out of bounds");
        }
        return this.#allocate(byteLength, (pointer) => {
            this.#check(
                "read-memory",
                this.functions.readMemory(
                    this.handle,
                    checkedAddress,
                    pointer,
                    byteLength,
                ),
            );
            return this.module.HEAPU8.slice(pointer, pointer + byteLength);
        });
    }

    lcdPanel() {
        this.#requireHandle();
        if (this.kind !== "jr800") {
            return null;
        }
        return this.#allocate(JR800_LCD_PANEL_DOT_COUNT, (pointer) => {
            const status = this.functions.copyLcdPanel(
                this.handle,
                pointer,
                JR800_LCD_PANEL_DOT_COUNT,
            );
            if (status === Status.unsupportedAccess) {
                return null;
            }
            this.#check("copy-lcd-panel", status);
            return {
                width: JR800_LCD_PANEL_WIDTH,
                height: JR800_LCD_PANEL_HEIGHT,
                dots: this.module.HEAPU8.slice(
                    pointer,
                    pointer + JR800_LCD_PANEL_DOT_COUNT,
                ),
            };
        });
    }

    lcdIndicators() {
        this.#requireHandle();
        if (this.kind !== "jr800") {
            return null;
        }
        const byteLength = Jr800LcdIndicatorNames.length
            * LCD_INDICATOR_RAW_WORDS * WORD_BYTES;
        return this.#allocate(byteLength, (pointer) => {
            const status = this.functions.copyLcdIndicators(
                this.handle,
                pointer,
                Jr800LcdIndicatorNames.length,
            );
            if (status === Status.unsupportedAccess) {
                return null;
            }
            this.#check("copy-lcd-indicators", status);
            const words = this.#readWords(
                pointer,
                Jr800LcdIndicatorNames.length * LCD_INDICATOR_RAW_WORDS,
            );
            return Object.fromEntries(
                Jr800LcdIndicatorNames.map((name, index) => [
                    name,
                    words[index * LCD_INDICATOR_RAW_WORDS] === 0
                        ? null
                        : words[index * LCD_INDICATOR_RAW_WORDS + 1],
                ]),
            );
        });
    }

    sourceAt(address) {
        this.#requireHandle();
        const checkedAddress = checkedUint16(address, "Source address");
        return this.#allocate(SOURCE_WORDS * WORD_BYTES, (pointer) => {
            const status = this.functions.sourceAt(
                this.handle,
                checkedAddress,
                pointer,
            );
            if (status === Status.notFound) {
                return null;
            }
            this.#check("source-at", status);
            const words = this.#readWords(pointer, SOURCE_WORDS);
            const pathSize = this.functions.sourcePathSize(this.handle, words[2]);
            const path = this.#copyText(
                pathSize,
                "copy-source-path",
                (textPointer, size) => this.functions.copySourcePath(
                    this.handle,
                    words[2],
                    textPointer,
                    size,
                ),
            );
            return {
                address: words[0],
                length: words[1],
                sourceFileIndex: words[2],
                path,
                line: words[3],
                column: words[4],
            };
        });
    }

    sourceAddress(sourcePath, line) {
        this.#requireHandle();
        if (typeof sourcePath !== "string" || sourcePath.length === 0
            || sourcePath.includes("\0")) {
            throw new TypeError("Source path must be nonempty text without NUL");
        }
        if (!Number.isInteger(line) || line < 1 || line > 0xffff_ffff) {
            throw new RangeError("Source line must be a nonzero uint32 value");
        }
        const pathBytes = this.encoder.encode(sourcePath);
        return this.#withInput(pathBytes, (pathPointer, pathSize) =>
            this.#allocate(WORD_BYTES, (addressPointer) => {
                this.#check(
                    "source-address",
                    this.functions.sourceAddress(
                        this.handle,
                        pathPointer,
                        pathSize,
                        line,
                        addressPointer,
                    ),
                );
                return this.#readWords(addressPointer, 1)[0];
            })
        );
    }

    symbolAddress(symbolName) {
        this.#requireHandle();
        if (typeof symbolName !== "string" || symbolName.length === 0
            || symbolName.includes("\0")) {
            throw new TypeError("Symbol name must be nonempty text without NUL");
        }
        const nameBytes = this.encoder.encode(symbolName);
        return this.#withInput(nameBytes, (namePointer, nameSize) =>
            this.#allocate(WORD_BYTES, (addressPointer) => {
                this.#check(
                    "symbol-address",
                    this.functions.symbolAddress(
                        this.handle,
                        namePointer,
                        nameSize,
                        addressPointer,
                    ),
                );
                return this.#readWords(addressPointer, 1)[0];
            })
        );
    }

    disassemble(address) {
        this.#requireHandle();
        const checkedAddress = checkedUint16(address, "Disassembly address");
        return this.#allocate(DISASSEMBLY_WORDS * WORD_BYTES, (pointer) => {
            this.#check(
                "disassemble",
                this.functions.disassemble(this.handle, checkedAddress, pointer),
            );
            const words = this.#readWords(pointer, DISASSEMBLY_WORDS);
            const textSize = this.functions.disassemblyTextSize(
                this.handle,
                checkedAddress,
            );
            const text = this.#copyText(
                textSize,
                "copy-disassembly-text",
                (textPointer, size) => this.functions.copyDisassemblyText(
                    this.handle,
                    checkedAddress,
                    textPointer,
                    size,
                ),
            );
            return {
                address: words[0],
                bytes: [words[1], words[2], words[3]],
                length: words[4],
                supported: words[5] !== 0,
                text,
            };
        });
    }

    programSaves() {
        this.#requireHandle();
        return this.#allocate(28, (pointer) => {
            this.#check("get-program-saves", this.functions.getProgramSaves(this.handle, pointer));
            const [state, count] = this.#readWords(pointer, 2);
            const files = [];
            for (let index = 0; index < count; ++index) {
                this.#check("get-saved-program-info", this.functions.getSavedProgramInfo(this.handle, index, pointer));
                const info = this.#readWords(pointer, 3);
                files.push({index, kind: ["unknown", "machine-code", "basic-text", "basic-binary"][info[0]],
                    byteLength: info[1], nameBytes: Array.from(this.module.HEAPU8.slice(pointer + 12, pointer + 12 + info[2]))});
            }
            return {state: ["unavailable", "idle", "recording", "failed", "full"][state], files};
        });
    }

    exportSavedProgram(index, format) {
        this.#requireHandle();
        checkedUint32(index, "Saved program index");
        if (!["j8a", "wav"].includes(format)) throw new TypeError("Unsupported save format");
        const kind = format === "j8a" ? 1 : 2;
        return this.#allocate(4, (sizePointer) => {
            this.#check("export-saved-program", this.functions.exportSavedProgram(this.handle, index, kind, 0, 0, sizePointer));
            const size = this.#readWords(sizePointer, 1)[0];
            return this.#allocate(size, (pointer) => {
                this.#check("export-saved-program", this.functions.exportSavedProgram(this.handle, index, kind, pointer, size, sizePointer));
                return this.module.HEAPU8.slice(pointer, pointer + size);
            });
        });
    }

    clearProgramSaves() {
        this.#requireHandle();
        this.#check("clear-program-saves", this.functions.clearProgramSaves(this.handle));
    }

    snapshot(view) {
        const normalizedView = view === undefined && this.kind === "jr800"
            ? {memoryAddress: 0x8000}
            : view;
        const {memoryAddress, memoryLength, focusAddress, traceFilter} =
            normalizeViewOptions(normalizedView);
        const state = this.state();
        const address = focusAddress ?? state.pc;
        return {
            state,
            disassembly: this.disassemble(address),
            source: this.kind === "synthetic" ? this.sourceAt(address) : null,
            history: this.history(),
            accesses: this.accesses(traceFilter),
            memory: {
                address: memoryAddress,
                bytes: Array.from(this.memory(memoryAddress, memoryLength)),
            },
            keyboardActivity: this.keyboardActivity(),
            lcdPanel: this.lcdPanel(),
            lcdIndicators: this.lcdIndicators(),
            programSaves: this.programSaves(),
        };
    }

    destroy() {
        if (this.handle) {
            this.functions.destroy(this.handle);
            this.handle = 0;
        }
    }
}
