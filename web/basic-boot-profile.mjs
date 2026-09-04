// SPDX-License-Identifier: MIT

export const Jr800BasicRunSlice = Object.freeze({
    instructionLimit: 250_000,
    suspendedCycleLimit: 64_000_000,
});

export function jr800BasicBootExperimentConfiguration() {
    return {
        internalRamInitialValue: 0x00,
        standardRamInitialValue: 0x00,
        expansionRamInitialValue: 0x00,
        lcdUnknownDataReadValue: 0x00,
        calendarAddressSource: "a0-a3",
        calendarUpperRead: "zero",
        port1Pins: {value: 0xff, knownMask: 0xff},
        port2Pins: {value: 0x1e, knownMask: 0x1f},
        ramStandbyPowerValid: false,
        keyboardWindowValue: 0xff,
    };
}

export function basicRunCanContinue(stop) {
    return stop?.reason === "instruction-limit";
}
