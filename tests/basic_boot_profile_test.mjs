// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const [modulePath] = process.argv.slice(2);
if (!modulePath) {
    throw new Error(
        "Usage: node basic_boot_profile_test.mjs <basic-boot-profile.mjs>",
    );
}

const {
    Jr800BasicRunSlice,
    basicRunCanContinue,
    browserCalendarDateTime,
    jr800BasicBootExperimentConfiguration,
} = await import(pathToFileURL(modulePath));

assert.deepEqual(Jr800BasicRunSlice, {
    instructionLimit: 20_000,
    realtime: true,
    suspendedCycleLimit: 64_000_000,
});

const expectedConfiguration = {
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
const first = jr800BasicBootExperimentConfiguration();
const second = jr800BasicBootExperimentConfiguration();
assert.deepEqual(first, expectedConfiguration);
assert.deepEqual(second, expectedConfiguration);
assert.notEqual(first, second);
assert.notEqual(first.port1Pins, second.port1Pins);
assert.notEqual(first.port2Pins, second.port2Pins);

assert.equal(basicRunCanContinue({reason: "instruction-limit"}), true);
assert.equal(basicRunCanContinue({reason: "sleeping"}), true);
for (const stop of [
    undefined,
    null,
    {},
    {reason: "pause-requested"},
    {reason: "paused"},
    {reason: "execution-breakpoint"},
    {reason: "memory-watchpoint"},
    {reason: "cpu-fault"},
]) {
    assert.equal(basicRunCanContinue(stop), false);
}

assert.deepEqual(browserCalendarDateTime(new Date(2026, 8, 5, 12, 34, 56)), {
    year: 2026, month: 9, day: 5, hour: 12, minute: 34, second: 56,
});
for (const value of [new Date(NaN), new Date(1999, 11, 31), new Date(2100, 0, 1)]) {
    assert.throws(() => browserCalendarDateTime(value), /2000-2099/);
}
