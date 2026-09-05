// SPDX-License-Identifier: MIT

export const Jr800BasicRunSlice = Object.freeze({
    instructionLimit: 20_000,
    realtime: true,
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
        calendarCpuCycleRatio: "e030-nominal-1.2288mhz",
        port1Pins: {value: 0xff, knownMask: 0xff},
        port2Pins: {value: 0x1e, knownMask: 0x1f},
        ramStandbyPowerValid: false,
        keyboardWindowValue: 0xff,
    };
}

export function basicRunCanContinue(stop) {
    return stop?.reason === "instruction-limit" || stop?.reason === "sleeping";
}

export function browserCalendarDateTime(now = new Date()) {
    const year = now.getFullYear();
    if (!Number.isInteger(year) || year < 2000 || year > 2099) {
        throw new RangeError("Browser calendar startup supports years 2000-2099");
    }
    return {
        year, month: now.getMonth() + 1, day: now.getDate(),
        hour: now.getHours(), minute: now.getMinutes(), second: now.getSeconds(),
    };
}
