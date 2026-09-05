// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const {RunPacer, JR800_CPU_CLOCK_HZ, RUN_TURN_MS} = await import(
    pathToFileURL(process.argv[2]),
);
const rate = JR800_CPU_CLOCK_HZ / 1000;
const origin = 9_000_000;
const pacer = new RunPacer(origin, 500);
assert.equal(pacer.cycleBudget(origin, 500), Math.floor(rate * RUN_TURN_MS));
assert.equal(pacer.delay(origin, 500), 0);
assert.equal(pacer.delay(origin + JR800_CPU_CLOCK_HZ, 500), 1000);
assert.equal(pacer.cycleBudget(origin + JR800_CPU_CLOCK_HZ, 500), 0);
assert.equal(pacer.delay(origin + JR800_CPU_CLOCK_HZ, 1500), 0);
assert.equal(pacer.cycleBudget(origin + JR800_CPU_CLOCK_HZ, 1500),
    Math.floor(rate * RUN_TURN_MS));
// A short rendering/timer delay is caught up rather than resetting the clock.
assert.equal(pacer.cycleBudget(origin + JR800_CPU_CLOCK_HZ, 1520),
    Math.floor(rate * (20 + RUN_TURN_MS)));
// A suspended tab starts a new bounded window instead of racing through minutes.
assert.equal(pacer.cycleBudget(origin + JR800_CPU_CLOCK_HZ, 60_000),
    Math.floor(rate * RUN_TURN_MS));
assert.equal(pacer.delay(origin + JR800_CPU_CLOCK_HZ, 60_000), 0);
// Repeated host turns neither lose fractional cycles nor accumulate excess speed.
const continuous = new RunPacer(0, 0);
let cycle = 0;
for (let now = 0; now < 10_000; now += RUN_TURN_MS) {
    cycle += continuous.cycleBudget(cycle, now);
}
assert.equal(cycle, JR800_CPU_CLOCK_HZ * 10);
